#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Tiny livesim: serves a packaged static DASH presentation (SegmentTimeline, $Number$) as itself under
/vod/ and as a looping live stream under /live/ (type=dynamic, SegmentTemplate@duration), shifting each
segment's tfdt and sidx time by whole loops, as DASH-IF livesim2 does.

  livesim.py <vod dir> [port] [--window S] [--throttle-kbps N]

/time answers the server clock (UTCTiming http-iso). Test content only.
"""
import datetime as dt
import http.server
import os
import re
import socketserver
import struct
import sys
import time

root = os.path.abspath(sys.argv[1])
port = int(sys.argv[2]) if len(sys.argv) > 2 and not sys.argv[2].startswith("--") else 8099
args = sys.argv[1:]
window = int(args[args.index("--window") + 1]) if "--window" in args else 1800
throttle = int(args[args.index("--throttle-kbps") + 1]) if "--throttle-kbps" in args else 0

SEG = 2  # seconds per segment
mpd_vod = open(os.path.join(root, "manifest.mpd")).read()
loop_s = float(re.search(r'mediaPresentationDuration="PT([0-9.]+)S"', mpd_vod).group(1))
nseg = int(round(loop_s / SEG))
timescales = {}  # media template dir -> timescale
for m in re.finditer(r'<SegmentTemplate timescale="(\d+)" initialization="([^/]+)/init.mp4"', mpd_vod):
    timescales[m.group(2)] = int(m.group(1))
start = time.time()
ast = start - window  # the window is full from the start


def iso(t):
    return dt.datetime.fromtimestamp(t, dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%S.%f")[:-3] + "Z"


def live_mpd(host):
    s = mpd_vod
    s = s.replace('type="static"', 'type="dynamic"')
    s = re.sub(r' mediaPresentationDuration="[^"]*"',
               f' availabilityStartTime="{iso(ast)}" publishTime="{iso(time.time())}" minimumUpdatePeriod="PT10S"'
               f' timeShiftBufferDepth="PT{window}S" maxSegmentDuration="PT{SEG}S"', s)
    s = s.replace('<Period id="0">', '<Period id="0" start="PT0S">')

    def tmpl(m):
        ts = int(m.group(1))
        return (f'<SegmentTemplate timescale="{ts}" duration="{ts * SEG}" initialization="{m.group(2)}"'
                f' media="{m.group(3)}" startNumber="1"/>')
    s = re.sub(r'<SegmentTemplate timescale="(\d+)" initialization="([^"]+)" media="([^"]+)" startNumber="1">'
               r'\s*<SegmentTimeline>.*?</SegmentTimeline>\s*</SegmentTemplate>', tmpl, s, flags=re.S)
    s = s.replace("</Period>", "</Period>\n  <UTCTiming schemeIdUri=\"urn:mpeg:dash:utc:http-iso:2014\" "
                  f"value=\"http://{host}/time\"/>")
    return s


def shift(data, add):
    """Add `add` (timescale units) to every tfdt and sidx earliest_presentation_time."""
    b = bytearray(data)

    def walk(i, end):
        while i + 8 <= end:
            size, typ = struct.unpack(">I4s", b[i:i + 8])
            if size < 8:
                return
            if typ in (b"moof", b"traf"):
                walk(i + 8, i + size)
            elif typ == b"tfdt":
                if b[i + 8] == 1:
                    v = struct.unpack(">Q", b[i + 12:i + 20])[0]
                    b[i + 12:i + 20] = struct.pack(">Q", v + add)
                else:
                    v = struct.unpack(">I", b[i + 12:i + 16])[0]
                    if v + add >= 1 << 32:
                        raise ValueError("tfdt v0 overflow")
                    b[i + 12:i + 16] = struct.pack(">I", v + add)
            elif typ == b"sidx":
                ver = b[i + 8]
                if ver == 1:
                    v = struct.unpack(">Q", b[i + 20:i + 28])[0]
                    b[i + 20:i + 28] = struct.pack(">Q", v + add)
                else:
                    v = struct.unpack(">I", b[i + 20:i + 24])[0]
                    if v + add >= 1 << 32:
                        raise ValueError("sidx v0 overflow")
                    b[i + 20:i + 24] = struct.pack(">I", v + add)
            i += size
    walk(0, len(b))
    return bytes(b)


class H(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **k):
        super().__init__(*a, directory=root, **k)

    def log_message(self, fmt, *a):
        sys.stderr.write("%.1f %s\n" % (time.time() - start, fmt % a))

    def send(self, body, ctype):
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        if throttle:
            step = max(1, throttle * 125 // 10)  # bytes per 100 ms
            for i in range(0, len(body), step):
                self.wfile.write(body[i:i + step])
                time.sleep(0.1)
        else:
            self.wfile.write(body)

    def do_GET(self):
        path = self.path.split("?")[0]
        if path == "/time":
            return self.send(iso(time.time()).encode(), "text/plain")
        if path.startswith("/vod/"):
            self.path = path[4:]
            return super().do_GET()
        if path == "/live/manifest.mpd":
            return self.send(live_mpd(self.headers.get("Host", "localhost")).encode(), "application/dash+xml")
        m = re.fullmatch(r"/live/([^/]+)/init.mp4", path)
        if m and m.group(1) in timescales:
            return self.send(open(os.path.join(root, m.group(1), "init.mp4"), "rb").read(), "video/mp4")
        m = re.fullmatch(r"/live/([^/]+)/(\d+).m4s", path)
        if m and m.group(1) in timescales:
            n = int(m.group(2))
            avail = ast + n * SEG  # segment n (1-based) is complete at the end of its 2 s
            if n < 1 or time.time() < avail or time.time() > avail + window + SEG:
                return self.send_error(404)
            loops, k = divmod(n - 1, nseg)
            data = open(os.path.join(root, m.group(1), f"{k + 1}.m4s"), "rb").read()
            return self.send(shift(data, int(loops * loop_s * timescales[m.group(1)])), "video/mp4")
        return self.send_error(404)


class S(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    allow_reuse_address = True


print(f"serving {root} on :{port}: /vod/manifest.mpd, /live/manifest.mpd (window {window} s, loop {loop_s} s)",
      flush=True)
S(("0.0.0.0", port), H).serve_forever()
