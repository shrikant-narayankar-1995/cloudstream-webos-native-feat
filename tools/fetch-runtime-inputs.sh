#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Fetches the plugin runtime's pinned inputs (runtime/inputs.lock) into a directory and checks every
# SHA-256. Each file is taken from the project's mirror release first (CloudStream's pre-release and
# Alpine edge replace or drop old builds), then from its upstream URL. Files already present with the
# right hash are kept.
#   tools/fetch-runtime-inputs.sh [<dir>]     default: ~/.cache/csweb-runtime-inputs
# Mirror: $RUNTIME_INPUTS_MIRROR (default: the runtime-inputs-N release named in runtime/inputs.lock),
# where a file's asset name is its path with '/' replaced by '_'.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOCK="$ROOT/runtime/inputs.lock"
DEST="${1:-$HOME/.cache/csweb-runtime-inputs}"
MIRROR="${RUNTIME_INPUTS_MIRROR:-$(sed -n 's/^# mirror: //p' "$LOCK")}"

verified() { [[ -f "$1" ]] && [[ "$(sha256sum "$1" | cut -d' ' -f1)" == "$2" ]]; }

fetched=0 kept=0
while read -r sha path url; do
    [[ -z "$sha" || "$sha" == \#* ]] && continue
    out="$DEST/$path"
    if verified "$out" "$sha"; then kept=$((kept + 1)); continue; fi
    mkdir -p "$(dirname "$out")"
    ok=0
    for src in ${MIRROR:+"$MIRROR/${path//\//_}"} "$url"; do
        if curl -fsSL --retry 3 -o "$out.part" "$src" && verified "$out.part" "$sha"; then
            mv "$out.part" "$out"
            ok=1
            break
        fi
    done
    rm -f "$out.part"
    if (( ! ok )); then
        echo "fetch-runtime-inputs: no copy of $path with SHA-256 $sha" >&2
        exit 1
    fi
    fetched=$((fetched + 1))
done < "$LOCK"
echo "fetch-runtime-inputs: $DEST: $fetched fetched, $kept already present, all verified"
