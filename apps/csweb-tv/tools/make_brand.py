#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Draws the app's brand images from CloudStream's launcher vector (GPL-3.0-or-later,
external/cloudstream/app/src/main/res/drawable-v24/ic_launcher_foreground.xml): the cloud's three
paths with their linear gradients, flattened and filled at 8x supersampling.

  tools/make_brand.py <ic_launcher_foreground.xml> <bold .ttf> <out dir>

Writes:
  logo.png        the cloud alone, transparent, 512 px wide (splash, onboarding)
  icon.png        80x80 launcher icon: the cloud on CloudStream's black, labelled "webOS"
  largeIcon.png   130x130, the same
The label marks the native webOS build. It is the word in the app's own font, not LG's logo.
"""
from __future__ import annotations

import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFont

ANDROID = "{http://schemas.android.com/apk/res/android}"
SS = 8  # supersampling


def parse_path(d: str) -> list[list[tuple[float, float]]]:
    """M/L/C/Z path data (absolute) -> closed polygons, cubic curves flattened."""
    tokens = re.findall(r"[MLCZmlcz]|-?\d*\.?\d+(?:e-?\d+)?", d)
    polys, cur, i, cmd = [], [], 0, None
    pos = (0.0, 0.0)

    def num():
        nonlocal i
        v = float(tokens[i])
        i += 1
        return v

    while i < len(tokens):
        if re.fullmatch(r"[A-Za-z]", tokens[i]):
            cmd = tokens[i]
            i += 1
        if cmd in "Zz":
            if cur:
                polys.append(cur)
            cur = []
            cmd = None
            continue
        if cmd == "M":
            if cur:
                polys.append(cur)
            pos = (num(), num())
            cur = [pos]
            cmd = "L"
        elif cmd == "L":
            pos = (num(), num())
            cur.append(pos)
        elif cmd == "C":
            p0 = pos
            p1, p2, p3 = (num(), num()), (num(), num()), (num(), num())
            for k in range(1, 25):
                t = k / 24
                a, b, c, e = (1 - t) ** 3, 3 * (1 - t) ** 2 * t, 3 * (1 - t) * t ** 2, t ** 3
                cur.append((a * p0[0] + b * p1[0] + c * p2[0] + e * p3[0],
                            a * p0[1] + b * p1[1] + c * p2[1] + e * p3[1]))
            pos = p3
        else:
            raise ValueError(f"unsupported path command {cmd}")
    if cur:
        polys.append(cur)
    return polys


def hex_rgb(s: str) -> tuple[int, int, int]:
    s = s.lstrip("#")[-6:]
    return int(s[0:2], 16), int(s[2:4], 16), int(s[4:6], 16)


def load_paths(xml: Path):
    """[(polygons, (x0, colour0), (x1, colour1))] in the group's own coordinates."""
    root = ET.parse(xml).getroot()
    out = []
    for p in root.iter("path"):
        grad = p.find(".//gradient")
        items = grad.findall("item")
        x0, x1 = float(grad.get(ANDROID + "startX")), float(grad.get(ANDROID + "endX"))
        c0, c1 = hex_rgb(items[0].get(ANDROID + "color")), hex_rgb(items[-1].get(ANDROID + "color"))
        out.append((parse_path(p.get(ANDROID + "pathData")), (x0, c0), (x1, c1)))
    return out


def render_cloud(paths, width: int) -> Image.Image:
    """The cloud, cropped to its bounds, `width` px wide, transparent background."""
    xs = [x for polys, _, _ in paths for poly in polys for x, _ in poly]
    ys = [y for polys, _, _ in paths for poly in polys for _, y in poly]
    x_lo, x_hi, y_lo, y_hi = min(xs), max(xs), min(ys), max(ys)
    scale = width / (x_hi - x_lo)
    w, h = width * SS, round((y_hi - y_lo) * scale) * SS
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    for polys, (gx0, c0), (gx1, c1) in paths:
        mask = Image.new("L", (w, h), 0)
        draw = ImageDraw.Draw(mask)
        for poly in polys:
            draw.polygon([((x - x_lo) * scale * SS, (y - y_lo) * scale * SS) for x, y in poly], fill=255)
        # Android's linear gradient along x, clamped outside [start, end].
        ramp = Image.new("RGB", (w, 1))
        for px in range(w):
            gx = x_lo + px / (scale * SS)
            t = 0.0 if gx1 == gx0 else min(1.0, max(0.0, (gx - gx0) / (gx1 - gx0)))
            ramp.putpixel((px, 0), tuple(round(a + (b - a) * t) for a, b in zip(c0, c1)))
        fill = ramp.resize((w, h), Image.NEAREST).convert("RGBA")
        fill.putalpha(mask)
        img = Image.alpha_composite(img, fill)
    return img.resize((width, h // SS), Image.LANCZOS)


def rounded(size: int, radius: float, colour) -> Image.Image:
    big = Image.new("RGBA", (size * SS, size * SS), (0, 0, 0, 0))
    ImageDraw.Draw(big).rounded_rectangle((0, 0, size * SS - 1, size * SS - 1), radius * SS, fill=colour)
    return big.resize((size, size), Image.LANCZOS)


def webos_label(width: int, height: int, font: Path) -> Image.Image:
    """The word "webOS" in white on a blue pill, written in the app's font (not LG's logo)."""
    w, h = width * SS, height * SS
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((0, 0, w - 1, h - 1), h / 2, fill=(37, 76, 219, 255))
    size = round(h * 0.72)
    face = ImageFont.truetype(str(font), size)
    while d.textlength("webOS", font=face) > w - h * 0.9 and size > 4:
        size -= 1
        face = ImageFont.truetype(str(font), size)
    d.text((w / 2, h / 2), "webOS", font=face, fill=(255, 255, 255, 255), anchor="mm")
    return img.resize((width, height), Image.LANCZOS)


def icon(paths, size: int, font: Path) -> Image.Image:
    img = rounded(size, size * 0.22, (0, 0, 0, 255))
    cloud = render_cloud(paths, round(size * 0.74))
    img.alpha_composite(cloud, ((size - cloud.width) // 2, round(size * 0.40 - cloud.height / 2)))
    label = webos_label(round(size * 0.66), round(size * 0.22), font)
    img.alpha_composite(label, ((size - label.width) // 2, round(size * 0.70)))
    # Keep the rounded corners transparent after compositing.
    img.putalpha(ImageChops.multiply(img.getchannel("A"), rounded(size, size * 0.22, (0, 0, 0, 255)).getchannel("A")))
    return img


def main(argv: list[str]) -> int:
    if len(argv) != 4:
        print(__doc__, file=sys.stderr)
        return 2
    paths = load_paths(Path(argv[1]))
    font = Path(argv[2])
    out = Path(argv[3])
    out.mkdir(parents=True, exist_ok=True)
    render_cloud(paths, 512).save(out / "logo.png", optimize=True)
    icon(paths, 80, font).save(out / "icon.png", optimize=True)
    icon(paths, 130, font).save(out / "largeIcon.png", optimize=True)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
