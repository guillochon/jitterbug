#!/usr/bin/env python3
"""Generate Jitterbug's Finder icon family and startup-parade frames.

Classic Mac OS extension style: a puzzle piece with a pixel-art bug dancing
on it. Everything is drawn from the ASCII art below, then quantised to the
standard Mac 8-bit and 4-bit system palettes, so the art is the single source
of truth. Outputs:

  mac/resources/JitterbugIcons.r   raw 'ICN#' 'icl8' 'icl4' 'ics#' 'ics8'
                                   'ics4' data; no Rez headers needed
  mac/resources/icon/preview/      enlarged PNGs and an animated GIF (host
                                   only, needs Pillow)

Resource IDs: 128 is the Finder icon family (dance frame A). 'ICN#'/'icl8'/
'icl4' 129-130 are frames A/B for animating the icon in the startup parade.

Usage: python3 mac/resources/icon/make_icons.py
"""

import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REZ_OUT = os.path.join(HERE, "..", "JitterbugIcons.r")
PREVIEW_DIR = os.path.join(HERE, "preview")

# Colours by art character. Values are chosen from the Mac 8-bit palette so
# quantisation is exact on 256-colour screens.
COLORS = {
    "k": (0x00, 0x00, 0x00),  # outline
    "P": (0x99, 0x99, 0xCC),  # puzzle body
    "H": (0xFF, 0xFF, 0xFF),  # puzzle highlight (top/left bevel)
    "h": (0xCC, 0xCC, 0xFF),  # small-icon highlight (white is too loud at 16px)
    "S": (0x66, 0x66, 0x99),  # puzzle shadow (bottom/right bevel)
    # Figeater beetle (Cotinis mutabilis), seen from above.
    "G": (0x33, 0xCC, 0x33),  # glossy pronotum
    "l": (0x99, 0xFF, 0x99),  # pronotum shine
    "g": (0x33, 0x99, 0x33),  # velvety elytra
    "d": (0x00, 0x66, 0x00),  # elytral suture and scutellum
    "y": (0xCC, 0x99, 0x33),  # tan elytral margin
    "m": (0x33, 0x33, 0x66),  # motion lines
}

# 32x32 dance frame A, a figeater beetle from behind: left foreleg raised,
# left hind leg kicked out. Frame B is the mirror image.
BUG_LARGE = [
    ".k...k.....k.....",
    "..k...k...k......",
    "..k....kkk.......",
    "...k..kdGdk......",
    "....kkGlGGGk..k..",
    ".....kGlGGGGkk...",
    "....kdGGGGGdk....",
    "...kyggdddggyk...",
    ".kkkyGggdggGykkk.",
    "k..kyGggdggGyk..k",
    "...kygggdgggyk...",
    "k..kygggdgggyk...",
    ".kkkkyggdggykk...",
    ".....kygdgyk.k...",
    "......kkkkk...k..",
]

# 16x16 is too small to shade procedurally, so it is drawn whole.
ICON_SMALL = [
    "......kkkk......",
    ".....khhhPk.....",
    ".....khPPSk.....",
    ".kkkkkkPSkkkkk..",
    ".khhhhhhhhhhPk..",
    ".khPkPkkkPkPPkkk",
    ".khPPkGlGkPPPkhk",
    ".khPkkgdgkkPPPSk",
    ".khPPkgdgkPPPSSk",
    ".khPkkgdgkkPPkkk",
    ".khPPkydykPPPk..",
    ".khPPPkkkPPPSk..",
    ".khPPPPPPPPPSk..",
    ".khPPPPPPPPSSk..",
    ".kPSSSSSSSSSSk..",
    ".kkkkkkkkkkkkk..",
]

# Motion lines beside the raised foreleg and kicking hind leg: (x, y, length)
# in 32x32 space. Frame B mirrors them about the body's centre column.
MOTION_LARGE = [(4, 12, 2), (4, 14, 2), (4, 23, 2), (4, 25, 2)]
MIRROR_X = 28


def puzzle_mask(size):
    """Puzzle piece: tabs on top and right, a socket in the bottom."""
    s = size / 32.0
    x0, y0, x1, y1 = 2 * s, 7 * s, 25 * s, 30 * s   # body, inclusive-ish
    top = (14.0 * s, 4.0 * s, 3.8 * s)
    right = (27.5 * s, 18.5 * s, 3.8 * s)
    sock = (14.0 * s, 32.0 * s, 3.8 * s)

    def inside(px, py):
        cx, cy = px + 0.5, py + 0.5
        def circ(c):
            return (cx - c[0]) ** 2 + (cy - c[1]) ** 2 <= c[2] ** 2
        body = x0 <= cx <= x1 + 1 and y0 <= cy <= y1 + 1
        if circ(sock):
            return False
        return body or circ(top) or circ(right)

    return [[inside(x, y) for x in range(size)] for y in range(size)]


def shade_piece(mask):
    size = len(mask)
    def m(x, y):
        return 0 <= x < size and 0 <= y < size and mask[y][x]
    img = [[None] * size for _ in range(size)]
    for y in range(size):
        for x in range(size):
            if not mask[y][x]:
                continue
            if not (m(x - 1, y) and m(x + 1, y) and m(x, y - 1) and m(x, y + 1)):
                img[y][x] = "k"
            elif not (m(x - 2, y) and m(x, y - 2)):
                img[y][x] = "H"
            elif not (m(x + 2, y) and m(x, y + 2)):
                img[y][x] = "S"
            else:
                img[y][x] = "P"
    return img


def stamp(img, art, ox, oy):
    for dy, row in enumerate(art):
        for dx, ch in enumerate(row):
            if ch != ".":
                img[oy + dy][ox + dx] = ch


def mirror(art):
    return [row[::-1] for row in art]


def draw_large(frame):
    img = shade_piece(puzzle_mask(32))
    bug = BUG_LARGE if frame == 0 else mirror(BUG_LARGE)
    for x, y, n in MOTION_LARGE:
        if frame:
            x = MIRROR_X - x - (n - 1)
        for i in range(n):
            img[y][x + i] = "m"
    stamp(img, bug, (MIRROR_X - 16) // 2, 10)
    return img


def draw_small():
    return [[None if ch == "." else ch for ch in row] for row in ICON_SMALL]


# ---------------------------------------------------------------- palettes

def mac_clut8():
    steps = [0xFF, 0xCC, 0x99, 0x66, 0x33, 0x00]
    pal = [(r, g, b) for r in steps for g in steps for b in steps][:215]
    ramp = [0xEE, 0xDD, 0xBB, 0xAA, 0x88, 0x77, 0x55, 0x44, 0x22, 0x11]
    pal += [(v, 0, 0) for v in ramp]
    pal += [(0, v, 0) for v in ramp]
    pal += [(0, 0, v) for v in ramp]
    pal += [(v, v, v) for v in ramp]
    pal.append((0, 0, 0))
    return pal


CLUT4 = [
    (0xFF, 0xFF, 0xFF), (0xFC, 0xF3, 0x05), (0xFF, 0x64, 0x03), (0xDD, 0x08, 0x06),
    (0xF2, 0x08, 0x84), (0x46, 0x00, 0xA5), (0x00, 0x00, 0xD4), (0x02, 0xAB, 0xEA),
    (0x1F, 0xB7, 0x14), (0x00, 0x64, 0x11), (0x56, 0x2C, 0x05), (0x90, 0x71, 0x3A),
    (0xC0, 0xC0, 0xC0), (0x80, 0x80, 0x80), (0x40, 0x40, 0x40), (0x00, 0x00, 0x00),
]

# Hand-picked 4-bit mapping: nearest-colour sends the lavender body to grey,
# which is right, but keeps the bug green and the cheeks pink.
MAP4 = {"k": 15, "P": 12, "H": 0, "h": 0, "S": 13, "G": 8, "l": 8, "g": 8,
        "d": 9, "y": 11, "m": 14}


def nearest(pal, rgb):
    return min(range(len(pal)),
               key=lambda i: sum((a - b) ** 2 for a, b in zip(pal[i], rgb)))


def luminance(rgb):
    r, g, b = rgb
    return (0.299 * r + 0.587 * g + 0.114 * b) / 255.0


# ---------------------------------------------------------------- encoders

def pack_bits(rows):
    out = bytearray()
    for row in rows:
        for i in range(0, len(row), 8):
            byte = 0
            for bit, on in enumerate(row[i:i + 8]):
                if on:
                    byte |= 0x80 >> bit
            out.append(byte)
    return bytes(out)


def one_bit(img):
    """B&W rendering: only outline, bug ink and motion lines are black."""
    return [[c is not None and luminance(COLORS[c]) < 0.3 for c in row]
            for row in img]


def mask_of(img):
    return [[c is not None for c in row] for row in img]


def eight_bit(img, clut):
    return bytes(0 if c is None else nearest(clut, COLORS[c])
                 for row in img for c in row)


def four_bit(img):
    out = bytearray()
    for row in img:
        vals = [0 if c is None else MAP4[c] for c in row]
        for i in range(0, len(vals), 2):
            out.append((vals[i] << 4) | vals[i + 1])
    return bytes(out)


def rez_data(rtype, rid, name, data):
    lines = ["data '%s' (%d, \"%s\") {" % (rtype, rid, name)]
    for i in range(0, len(data), 16):
        lines.append('\t$"%s"' % data[i:i + 16].hex().upper())
    lines.append("};\n")
    return "\n".join(lines)


def write_rez(large, small):
    clut = mac_clut8()
    a, b = large
    parts = [
        "/* Generated by mac/resources/icon/make_icons.py. Do not edit. */\n",
        rez_data("ICN#", 128, "Jitterbug", pack_bits(one_bit(a)) + pack_bits(mask_of(a))),
        rez_data("icl8", 128, "Jitterbug", eight_bit(a, clut)),
        rez_data("icl4", 128, "Jitterbug", four_bit(a)),
        rez_data("ics#", 128, "Jitterbug", pack_bits(one_bit(small)) + pack_bits(mask_of(small))),
        rez_data("ics8", 128, "Jitterbug", eight_bit(small, clut)),
        rez_data("ics4", 128, "Jitterbug", four_bit(small)),
        "/* Startup-parade dance frames; PlotIconID picks the depth. */\n",
    ]
    for rid, name, img in ((129, "Dance A", a), (130, "Dance B", b)):
        parts += [
            rez_data("ICN#", rid, name, pack_bits(one_bit(img)) + pack_bits(mask_of(img))),
            rez_data("icl8", rid, name, eight_bit(img, clut)),
            rez_data("icl4", rid, name, four_bit(img)),
        ]
    with open(REZ_OUT, "w", newline="\n") as f:
        f.write("\n".join(parts))


def write_previews(large, small):
    try:
        from PIL import Image
    except ImportError:
        print("Pillow not installed; skipping previews", file=sys.stderr)
        return
    os.makedirs(PREVIEW_DIR, exist_ok=True)
    clut = mac_clut8()

    def to_image(img, kind="8"):
        n = len(img)
        im = Image.new("RGBA", (n, n), (0, 0, 0, 0))
        bw = one_bit(img)
        for y, row in enumerate(img):
            for x, c in enumerate(row):
                if c is None:
                    continue
                if kind == "1":
                    rgb = (0, 0, 0) if bw[y][x] else (255, 255, 255)
                elif kind == "4":
                    rgb = CLUT4[MAP4[c]]
                else:
                    rgb = clut[nearest(clut, COLORS[c])]
                im.putpixel((x, y), rgb + (255,))
        return im

    def big(im, scale):
        return im.resize((im.width * scale, im.height * scale), Image.NEAREST)

    def on_desktop(im):
        bg = Image.new("RGBA", im.size, (0x66, 0x66, 0x99, 255))
        bg.alpha_composite(im)
        return bg.convert("RGB")

    big(to_image(large[0]), 8).save(os.path.join(PREVIEW_DIR, "icl8.png"))
    big(to_image(large[0], "4"), 8).save(os.path.join(PREVIEW_DIR, "icl4.png"))
    big(to_image(large[0], "1"), 8).save(os.path.join(PREVIEW_DIR, "icn.png"))
    big(to_image(small), 8).save(os.path.join(PREVIEW_DIR, "ics8.png"))

    frames = [on_desktop(big(to_image(f), 8)) for f in large]
    frames[0].save(os.path.join(PREVIEW_DIR, "dance.gif"), save_all=True,
                   append_images=frames[1:], duration=300, loop=0)

    # Contact sheet: every variant at 1x and 4x on a Platinum-grey desktop.
    sheet = Image.new("RGB", (32 * 4 * 4 + 32 * 5, 32 * 4 + 64), (0xDD, 0xDD, 0xDD))
    x = 16
    for im in (to_image(large[0]), to_image(large[1]),
               to_image(large[0], "4"), to_image(large[0], "1")):
        sheet.paste(big(im, 4), (x, 16), big(im, 4))
        x += 32 * 4 + 32
    x = 16
    for im in (to_image(large[0]), to_image(small), to_image(large[0], "1")):
        sheet.paste(im, (x, 32 * 4 + 24), im)
        x += 48
    sheet.save(os.path.join(PREVIEW_DIR, "sheet.png"))


def main():
    large = [draw_large(0), draw_large(1)]
    small = draw_small()
    write_rez(large, small)
    write_previews(large, small)
    print("wrote", os.path.normpath(REZ_OUT))


if __name__ == "__main__":
    main()
