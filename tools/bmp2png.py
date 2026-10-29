#!/usr/bin/env python3
# Aeonian-OpenD2LOD
# Copyright (C) 2026 Aeonian-OpenD2LOD contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
"""Converte o BMP de 24 bits gravado pelo Aeonian (request_screenshot) em PNG.

Uso: bmp2png.py entrada.bmp saida.png
     bmp2png.py --stats entrada.bmp         (dimensoes e contagem de cores)
Somente biblioteca padrao.
"""

import struct
import sys
import zlib


def read_bmp(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:2] != b"BM":
        raise ValueError("nao e um BMP")
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height, planes, bpp, comp = struct.unpack_from("<iiHHI", data, 18)
    if bpp != 24 or comp != 0:
        raise ValueError("so BMP de 24 bits sem compressao (bpp=%d comp=%d)" % (bpp, comp))
    top_down = height < 0
    height = abs(height)
    stride = (width * 3 + 3) & ~3
    rows = []
    for y in range(height):
        src_row = y if top_down else height - 1 - y
        raw = data[offset + src_row * stride: offset + src_row * stride + width * 3]
        rows.append(bytes(raw[i + 2:i + 3] + raw[i + 1:i + 2] + raw[i:i + 1]
                          for i in range(0, len(raw), 3)) if False else _bgr_to_rgb(raw))
    return width, height, rows


def _bgr_to_rgb(raw):
    out = bytearray(len(raw))
    out[0::3] = raw[2::3]
    out[1::3] = raw[1::3]
    out[2::3] = raw[0::3]
    return bytes(out)


def write_png(path, width, height, rows):
    def chunk(tag, payload):
        body = tag + payload
        return struct.pack(">I", len(payload)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    raw = b"".join(b"\x00" + row for row in rows)
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 6))
           + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)


def main(argv):
    if len(argv) == 3 and argv[1] == "--stats":
        w, h, rows = read_bmp(argv[2])
        colors = set()
        for row in rows:
            colors.update(row[i:i + 3] for i in range(0, len(row), 3))
        print(f"{w}x{h}, {len(colors)} cores distintas")
        return 0
    if len(argv) != 3:
        print(__doc__)
        return 2
    w, h, rows = read_bmp(argv[1])
    write_png(argv[2], w, h, rows)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
