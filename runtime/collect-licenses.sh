#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Collect the licence texts of the runtime's Alpine packages from their own sources: for each origin in
# runtime/licenses/packages.tsv (runtime/tools/notices.py packages), the recipe at its aports commit,
# its sources fetched and checked against the recipe's SHA-512 by abuild, and the licence-like files
# near the top of each source archive copied to runtime/licenses/texts/<origin>/.
#   docker run --rm -v <project>:/p alpine:edge sh /p/runtime/collect-licenses.sh
# With -v <dir>:/sources it also keeps, for publishing next to the package, each origin's recipe
# (aports-<origin>-<commit>.tar.gz, patches included) and its verified source archives.
# Nothing from the sources is built or run.
set -eu
apk add -q abuild curl tar xz bzip2 unzip >/dev/null 2>&1
L=${CSWEB_LICENSES_DIR:-/p/runtime/licenses}  # legacy runtime: -e CSWEB_LICENSES_DIR=/p/runtime/licenses-armv7
mkdir -p "$L/texts"
export SRCDEST=/tmp/distfiles
mkdir -p "$SRCDEST"
NAMES='^(LICEN[CS]E|COPYING|COPYRIGHT|NOTICE|AUTHORS\.LICENSE|COPYING\.[A-Z0-9.]+|LICEN[CS]E[._-].+|NOTICE[._-].+|MODULE_LICENSE_.+)$'

grep -v '^#' "$L/packages.tsv" | while IFS="$(printf '\t')" read -r origin commit _rest; do
    w=/tmp/w/$origin
    rm -rf "$w" && mkdir -p "$w" && cd "$w"
    dir=
    for repo in main community testing; do
        if curl -sfL "https://gitlab.alpinelinux.org/alpine/aports/-/archive/$commit/a.tar.gz?path=$repo/$origin" | tar xz 2>/dev/null &&
            [ -f aports-*/"$repo/$origin/APKBUILD" ]; then
            dir=$(echo "$w"/aports-*/"$repo/$origin")
            break
        fi
        rm -rf aports-*
    done
    if [ -z "$dir" ]; then echo "$origin: recipe not found at $commit" >&2; exit 1; fi
    if [ -d /sources ]; then
        (cd "$dir/.." && tar czf "/sources/aports-$origin-$commit.tar.gz" "$origin")
    fi
    cd "$dir"
    abuild -F fetch >/dev/null 2>&1 || { echo "$origin: fetch failed" >&2; exit 1; }
    abuild -F verify >/dev/null 2>&1 || { echo "$origin: SHA-512 check failed" >&2; exit 1; }
    out="$L/texts/$origin"
    rm -rf "$out" && mkdir -p "$out"
    # shellcheck disable=SC2046
    # a source "name::url" is saved as name, a plain URL as its last path element
    for f in $(sh -c '. ./APKBUILD; for s in $source; do case "$s" in *::*) echo "${s%%::*}" ;; *) echo "${s##*/}" ;; esac; done'); do
        a="$SRCDEST/$f"
        case "$f" in *.tar.*|*.tgz|*.tar) ;; *) continue ;; esac
        [ -f "$a" ] || continue
        tar tf "$a" 2>/dev/null | awk -F/ -v re="$NAMES" '
            { n = $NF; d = NF - 1 } n ~ re && d <= 3 && $0 !~ /(^|\/)(test|tests|testdata)\// { print }' |
            while read -r m; do
                dest="$out/$(echo "$m" | cut -d/ -f2- | tr / _)"
                tar xOf "$a" "$m" > "$dest"
            done
    done
    if [ "$origin" = ca-certificates ]; then
        # The shipped data is Mozilla's CA list (MPL-2.0); the source archive carries no licence text.
        curl -sfL -o "$out/MPL-2.0.txt" https://www.mozilla.org/media/MPL/2.0/index.txt
        echo "3f3d9e0024b1921b067d6f7f88deb4a60cbe7a78e76c64e3f1d7fc3b779b9d04  $out/MPL-2.0.txt" | sha256sum -c - >/dev/null
    fi
    echo "$origin: $(ls "$out" | wc -l) file(s)"
done
chown -R "$(stat -c %u:%g /p)" "$L"
if [ -d /sources ]; then
    cp "$SRCDEST"/* /sources/
    chown -R "$(stat -c %u:%g /p)" /sources
fi
