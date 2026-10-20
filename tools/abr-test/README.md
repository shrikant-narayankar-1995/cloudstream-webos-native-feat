# Live DASH bitrate-switch test stream

Legitimate test content for checking live DASH, ClearKey and bitrate switching on the TV. Each quality
is stamped with its own label (540p blue, 720p orange, 1080p green) and a frame counter, so a switch
and any skipped or frozen frame can be seen on screen. The picture flashes white and beeps at the
start of every second, to judge audio/video sync.

```
tools/abr-test/make-abr-test.sh /tmp/abr                         # ~30 s; fetches Shaka Packager v3.9.3
python3 -I tools/abr-test/livesim.py /tmp/abr/vod 8099           # serves /vod/ and /live/ on the LAN
```

- `/vod/manifest.mpd`: the static presentation (120 s).
- `/live/manifest.mpd`: the same as a live event that loops.
  - The manifest is `type="dynamic"`, with a `SegmentTemplate@duration`, 2 s segments, `minimumUpdatePeriod` 10 s and a 30-minute window.
  - Segment times move on by whole loops, as DASH-IF livesim2 does.
  - `/time` answers the server clock (`UTCTiming`).
- ClearKey (cenc): key ID `0123456789abcdef0123456789abcdef`, key `fedcba9876543210fedcba9876543210`.
- `--throttle-kbps N` limits each response.

**On the TV**, with the kit's player example (`cmake --build --preset webos-release --target wnk-player-install` in the kit):

```
tools/launch.sh org.wnk.examples.player '{"uri":"http://<this machine>:8099/live/manifest.mpd",
  "keys":"0123456789abcdef0123456789abcdef:fedcba9876543210fedcba9876543210",
  "codecs":"h264","capKbps":"30:5000,60:0,90:2000,120:0"}'
```

**What to expect:** it starts at 720p and moves up to 1080p. The bandwidth caps then force 1080p→720p
at about 30 s, back to 1080p at 60 s, 540p at 90 s, and 1080p at 120 s. Each switch takes effect a few
seconds after the cap, once the buffered segments have played.

**The log** (`tools/logs.sh org.wnk.examples.player`) should show:
- `video A -> B at segment N`, with the same N as the `switching ... at segment N` line before it;
- Starfish `sink:` lines at about 150 frames displayed and 0 dropped every 5 s.

Found with this stream on an LG C1 and fixed in Native App Kit PR #9 (live DASH stalls; switches
landing in the past).
