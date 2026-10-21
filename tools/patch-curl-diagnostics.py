#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Instrument only the legacy curl/mbedTLS build; fail if pinned source changes."""
import pathlib
import re
import sys


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise SystemExit(f"curl diagnostics: expected one occurrence of {old!r}")
    return text.replace(old, new, 1)


def patch(curl, mbedtls):
    easy = curl / "lib/easy.c"
    text = easy.read_text()
    # No retries, network requests, secrets, or changes to initialization results.
    support = '''
#include <errno.h>
#include <stdio.h>
#include <string.h>
static char csweb_init_errors[1024];
static int csweb_init_recording;
void csweb_curl_diag_error(const char *stage, long code, int saved_errno);
void csweb_curl_diag_error(const char *stage, long code, int saved_errno)
{
  int caller_errno = errno;
  if(!csweb_init_recording)
    return;
  size_t used = strlen(csweb_init_errors);
  if(used < sizeof(csweb_init_errors) - 1)
    snprintf(csweb_init_errors + used, sizeof(csweb_init_errors) - used,
             "%s%s: code=%ld errno=%d", used ? "; " : "", stage, code,
             saved_errno);
  errno = caller_errno;
}
CURL_EXTERN const char *curl_csweb_legacy_diagnostic(void);
const char *curl_csweb_legacy_diagnostic(void)
{
  return csweb_init_errors[0] ? csweb_init_errors :
    "no initialization failure recorded";
}
'''
    text = replace_once(text, "static CURLcode global_init(long flags, bool memoryfuncs)",
                        support + "\nstatic CURLcode global_init(long flags, bool memoryfuncs)")
    # Record each failed global initializer, including ones outside TLS.
    start = text.index("static CURLcode global_init(long flags, bool memoryfuncs)")
    end = text.index("\nCURLcode curl_global_init(long flags)", start)
    section = text[start:end]
    section = replace_once(section, "  if(memoryfuncs) {",
                           "  csweb_init_recording = 1;\n\n  if(memoryfuncs) {")
    section = replace_once(section, "  return CURLE_OK;\n\nfail:",
                           "  csweb_init_recording = 0;\n  return CURLE_OK;\n\nfail:\n  csweb_init_recording = 0;")
    section, count = re.subn(
        r'(    DEBUGF\(curl_mfprintf\(stderr, "Error: ([^"\\]+)(?:\\n)"\)\);)',
        lambda m: f'    csweb_curl_diag_error("{m[2]}", CURLE_FAILED_INIT, errno);\n' + m[1],
        section)
    if count != 10:
        raise SystemExit(f"curl diagnostics: expected 10 global failure sites, got {count}")
    text = text[:start] + section + text[end:]

    tls = curl / "lib/vtls/mbedtls.c"
    tls_text = tls.read_text()
    tls_text = replace_once(tls_text, "static int mbedtls_init(void)",
                           "#include <errno.h>\nextern void csweb_curl_diag_error(const char *, long, int);\n\nstatic int mbedtls_init(void)")
    tls_text = replace_once(tls_text, "  if(status != PSA_SUCCESS)\n    return 0;",
                           '  if(status != PSA_SUCCESS) {\n    csweb_curl_diag_error("psa_crypto_init", (long)status, errno);\n    return 0;\n  }')
    tls_text = replace_once(tls_text,
                           "  if(ret)\n    /* mbedtls_ctr_drbg_seed returned error */\n    return 0;",
                           '  if(ret) {\n    csweb_curl_diag_error("mbedtls_ctr_drbg_seed", ret, errno);\n    return 0;\n  }')

    entropy = mbedtls / "library/entropy_poll.c"
    entropy_text = entropy.read_text()
    entropy_text = replace_once(entropy_text, '#include <string.h>',
                               '#include <string.h>\n#include <errno.h>\nextern void csweb_curl_diag_error(const char *, long, int);')
    for old, stage in [
        ('    } else if (errno != ENOSYS) {\n        return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;', 'getrandom'),
        ('    if (file == NULL) {\n        return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;', 'urandom fopen'),
        ('    if (read_len != len) {\n        fclose(file);', 'urandom fread'),
    ]:
        insert_at = old.index('\n') + 1
        entropy_text = replace_once(entropy_text, old, old[:insert_at] +
                                   f'        csweb_curl_diag_error("{stage}", MBEDTLS_ERR_ENTROPY_SOURCE_FAILED, errno);\n' + old[insert_at:])
    # Validate everything before writing any source.
    easy.write_text(text)
    tls.write_text(tls_text)
    entropy.write_text(entropy_text)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: patch-curl-diagnostics.py CURL_SOURCE MBEDTLS_SOURCE")
    patch(pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]))
