#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# ABR test content: 120 s of testsrc2 at 30 fps (white flash + 1 kHz beep in the first 80 ms of every
# second, for A/V sync), three H.264 renditions each stamped with its own label in its own colour
# (540p blue, 720p orange, 1080p green) and a frame counter, AAC audio; packaged as ClearKey (cenc)
# DASH with 2 s segments by Shaka Packager.
#   make-abr-test.sh <out dir>     (README.md here: serving it and playing it on the TV)
# Needs ffmpeg with libx264 and drawtext; fetches Shaka Packager (pinned, SHA-256 checked) into the cache.
set -euo pipefail
out=${1:?usage: make-abr-test.sh <out dir>}
cache=${XDG_CACHE_HOME:-$HOME/.cache}/csweb-abr-test
packager=$cache/packager-v3.9.3
if [[ ! -x $packager ]]; then
    mkdir -p "$cache"
    curl -sSLo "$packager.part" https://github.com/shaka-project/shaka-packager/releases/download/v3.9.3/packager-linux-x64
    echo "7a3cf35ad146fd7810b4ededab363c8a3e6121d1b2c8391f53863126186f9ee6  $packager.part" | sha256sum -c --quiet
    chmod +x "$packager.part" && mv "$packager.part" "$packager"
fi
mkdir -p "$out"; out=$(cd "$out" && pwd)
src=$out/src; vod=$out/vod
rm -rf "$src" "$vod"; mkdir -p "$src" "$vod"
font=/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf
mono=/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf
V="testsrc2=size=1920x1080:rate=30:duration=120,drawbox=x=0:y=0:w=iw:h=ih:color=white:t=fill:enable='lt(mod(t\,1)\,0.08)'"
A="sine=frequency=1000:sample_rate=48000:duration=120,volume=volume=0:enable='gte(mod(t\,1)\,0.08)'"
label() {  # <name> <text> <colour>
    echo "drawtext=fontfile=$font:text='$2':fontsize=h/11:fontcolor=white:box=1:boxcolor=$3@0.9:boxborderw=h/40:x=(w-tw)/2:y=h/8,drawtext=fontfile=$mono:text='frame %{frame_num}   %{pts\:hms}':fontsize=h/14:fontcolor=white:box=1:boxcolor=black@0.8:boxborderw=h/80:x=(w-tw)/2:y=h-h/5"
}
X="-c:v libx264 -preset veryfast -pix_fmt yuv420p -profile:v high -g 60 -keyint_min 60 -sc_threshold 0"
ffmpeg -hide_banner -loglevel error -y -f lavfi -i "$V" -f lavfi -i "$A" -filter_complex \
  "[0:v]split=3[a][b][c];\
[a]scale=960:540,$(label v540 '540p  960x540  1.5 Mb/s' 0x1565C0)[v540];\
[b]scale=1280:720,$(label v720 '720p  1280x720  4 Mb/s' 0xE65100)[v720];\
[c]$(label v1080 '1080p  1920x1080  8 Mb/s' 0x2E7D32)[v1080]" \
  -map "[v540]" $X -b:v 1500k -maxrate 1500k -bufsize 3000k -an "$src/v540.mp4" \
  -map "[v720]" $X -b:v 4000k -maxrate 4000k -bufsize 8000k -an "$src/v720.mp4" \
  -map "[v1080]" $X -b:v 8000k -maxrate 8000k -bufsize 16000k -an "$src/v1080.mp4" \
  -map 1:a -c:a aac -b:a 128k -vn "$src/a.mp4"
cd "$vod"
"$packager" \
  in="$src/v540.mp4",stream=video,init_segment='v540/init.mp4',segment_template='v540/$Number$.m4s' \
  in="$src/v720.mp4",stream=video,init_segment='v720/init.mp4',segment_template='v720/$Number$.m4s' \
  in="$src/v1080.mp4",stream=video,init_segment='v1080/init.mp4',segment_template='v1080/$Number$.m4s' \
  in="$src/a.mp4",stream=audio,init_segment='a/init.mp4',segment_template='a/$Number$.m4s' \
  --enable_raw_key_encryption --keys label=:key_id=0123456789abcdef0123456789abcdef:key=fedcba9876543210fedcba9876543210 \
  --protection_scheme cenc --protection_systems CommonSystem --clear_lead 0 \
  --segment_duration 2 --generate_static_live_mpd --mpd_output manifest.mpd 2>&1 | tail -1
