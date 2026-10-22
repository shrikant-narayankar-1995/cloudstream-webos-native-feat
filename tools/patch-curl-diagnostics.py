#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Configure secure legacy entropy and diagnose init; fail if pinned sources change."""
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
    # No retries, network requests, or secrets. Preserve errors for events.log.
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
    # The webOS NDK's old syscall headers omit getrandom. ARM EABI uses syscall
    # 384 (Linux arch/arm/include/uapi/asm/unistd.h); the target kernel is 4.4.
    # The existing code falls back only on ENOSYS, preserving all other failures.
    entropy_text = replace_once(entropy_text, '#include <sys/syscall.h>',
                               '#include <sys/syscall.h>\n#if defined(__arm__) && defined(__ARM_EABI__) && !defined(SYS_getrandom)\n#define SYS_getrandom 384\n#endif')
    # mbedTLS 3.6.7 defaults to /dev/random, absent on the affected TV. Keep a
    # cryptographic device fallback for kernels without the dedicated syscall.
    platform = mbedtls / "include/mbedtls/platform.h"
    platform_text = replace_once(platform.read_text(),
                                 '#define MBEDTLS_PLATFORM_DEV_RANDOM "/dev/random"',
                                 '#define MBEDTLS_PLATFORM_DEV_RANDOM "/dev/urandom"')
    for old, stage in [
        ('    } else if (errno != ENOSYS) {\n        return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;', 'getrandom'),
        ('    if (file == NULL) {\n        return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;', 'entropy device fopen'),
        ('    if (read_len != len) {\n        fclose(file);', 'entropy device fread'),
    ]:
        insert_at = old.index('\n') + 1
        entropy_text = replace_once(entropy_text, old, old[:insert_at] +
                                   f'        csweb_curl_diag_error("{stage}", MBEDTLS_ERR_ENTROPY_SOURCE_FAILED, errno);\n' + old[insert_at:])
    entropy_text = replace_once(entropy_text,
                               '        csweb_curl_diag_error("entropy device fopen",',
                               '        csweb_curl_diag_error(mbedtls_platform_dev_random, MBEDTLS_ERR_ENTROPY_SOURCE_FAILED, errno);\n        csweb_curl_diag_error("entropy device fopen",')
    entropy_text = replace_once(entropy_text,
                               '        csweb_curl_diag_error("entropy device fread",',
                               '        csweb_curl_diag_error(mbedtls_platform_dev_random, MBEDTLS_ERR_ENTROPY_SOURCE_FAILED, errno);\n        csweb_curl_diag_error("entropy device fread",')
    # Validate everything before writing any source.
    easy.write_text(text)
    tls.write_text(tls_text)
    entropy.write_text(entropy_text)
    platform.write_text(platform_text)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: patch-curl-diagnostics.py CURL_SOURCE MBEDTLS_SOURCE")
    patch(pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]))
