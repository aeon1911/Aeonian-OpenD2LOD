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
"""Verificacoes sobre os BMP de 24 bits gravados pelo Aeonian (testes).

  bmp_check.py size   ARQ LARGURA ALTURA        -> exit 0 se as dimensoes batem
  bmp_check.py minsize ARQ LARGURA ALTURA       -> exit 0 se >= LARGURAxALTURA
  bmp_check.py red    ARQ X Y                   -> exit 0 se o pixel e vermelho (overlay)
  bmp_check.py diff   A B X0 Y0 X1 Y1 MIN       -> exit 0 se >= MIN pixels diferem na regiao
  bmp_check.py same   A B X0 Y0 X1 Y1 MAX       -> exit 0 se <= MAX pixels diferem na regiao
  bmp_check.py upright ARQ                      -> exit 0 se a tela de titulo esta de pe

Somente biblioteca padrao. O sinal de saida e o resultado; a saida de texto
so documenta o numero medido.
"""

import struct
import sys


def read(path):
    data = open(path, "rb").read()
    if data[:2] != b"BM":
        raise ValueError("nao e BMP")
    offset = struct.unpack_from("<I", data, 10)[0]
    w, h, _planes, bpp, comp = struct.unpack_from("<iiHHI", data, 18)
    if bpp != 24 or comp != 0:
        raise ValueError("so BMP 24 bits sem compressao")
    top_down = h < 0
    h = abs(h)
    stride = (w * 3 + 3) & ~3

    def pixel(x, y):  # y a partir do topo
        row = y if top_down else h - 1 - y
        i = offset + row * stride + x * 3
        return data[i + 2], data[i + 1], data[i]  # r, g, b

    return w, h, pixel


def main(a):
    cmd = a[1]
    if cmd in ("size", "minsize"):
        w, h, _ = read(a[2])
        want = (int(a[3]), int(a[4]))
        ok = (w, h) == want if cmd == "size" else (w >= want[0] and h >= want[1])
        print(f"{w}x{h}")
        return 0 if ok else 1
    if cmd == "red":
        _w, _h, px = read(a[2])
        r, g, b = px(int(a[3]), int(a[4]))
        print(f"rgb=({r},{g},{b})")
        return 0 if r >= 150 and g <= 110 and b <= 110 else 1
    if cmd in ("diff", "same"):
        wa, ha, pa = read(a[2])
        wb, hb, pb = read(a[3])
        x0, y0, x1, y1, need = (int(v) for v in a[4:9])
        if (wa, ha) != (wb, hb):
            print("tamanhos diferentes")
            return 1
        n = sum(1 for y in range(y0, min(y1, ha)) for x in range(x0, min(x1, wa))
                if pa(x, y) != pb(x, y))
        print(f"{n} pixels diferentes")
        return 0 if (n >= need if cmd == "diff" else n <= need) else 1
    if cmd == "upright":
        # Titulo do D2: o logotipo de fogo (laranja) fica na metade de cima e o
        # copyright (claro, discreto) embaixo. Conta "fogo" em duas faixas.
        w, h, px = read(a[2])

        def fire(y0, y1):
            n = 0
            for y in range(int(h * y0), int(h * y1), 2):
                for x in range(0, w, 3):
                    r, g, b = px(x, y)
                    if r > 190 and 90 < g < 200 and b < 90:
                        n += 1
            return n

        top, bottom = fire(0.05, 0.40), fire(0.60, 0.95)
        print(f"fogo: topo={top} base={bottom}")
        return 0 if top > 3 * max(bottom, 1) and top > 200 else 1
    raise SystemExit("comando desconhecido")


if __name__ == "__main__":
    sys.exit(main(sys.argv))
