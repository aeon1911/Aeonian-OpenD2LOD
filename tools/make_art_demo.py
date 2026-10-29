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
"""Gera o mod de arte de demonstracao "art-demo" a partir do SEU jogo.

Extrai do seu d2data.mpq/d2exp.mpq os DC6 dos botoes do menu e a paleta do
menu, recolore os botoes (troca vermelho<->azul) e grava um mod pronto:

    <saida>/mod.ini
    <saida>/data/global/ui/FrontEnd/*ButtonBlank*.dc6   (recoloridos)

O resultado e uma obra derivada de arquivos da Blizzard: fica so na sua
maquina. NAO versione nem redistribua a pasta gerada.

Uso:
  make_art_demo.py --d2dir <pasta do jogo> --extractor <ae_mpq_extract> --out <mod>
  make_art_demo.py --self-test

So biblioteca padrao do Python 3 (o extrator e o do projeto: tools/mpq_extractor).
"""

import argparse
import os
import shutil
import struct
import subprocess
import sys
import tempfile

# (mpq, prefixo dentro do MPQ) dos botoes que o menu usa.
BUTTON_PREFIXES = [
    "data\\global\\ui\\FrontEnd\\WideButtonBlank",
    "data\\global\\ui\\FrontEnd\\NarrowButtonBlank",
    "data\\global\\ui\\FrontEnd\\MediumButtonBlank",
    "data\\global\\ui\\FrontEnd\\3WideButtonBlank",
    "data\\global\\ui\\FrontEnd\\OkCancelButtonBlank",
    "data\\global\\ui\\FrontEnd\\CancelButtonBlank",
    "data\\global\\ui\\CharSelect\\ShortButtonBlank",
    "data\\global\\ui\\CharSelect\\TallButtonBlank",
]
PALETTE_PREFIX = "data\\global\\palette\\sky\\pal.dat"
MPQS = ["d2data.mpq", "d2exp.mpq"]

MOD_INI = """\
; GERADO LOCALMENTE por tools/make_art_demo.py a partir do seu jogo.
; Obra derivada de arquivos da Blizzard: nao versione nem redistribua.
[mod]
id       = art-demo
name     = Demo de arte (botoes azuis)
version  = 0.1.0
author   = Aeonian-OpenD2LOD
priority = 5
builds   = 1.14d
"""


def read_palette(path):
    """pal.dat: 256 entradas B,G,R (ver docs/specs/palette-format.md)."""
    data = open(path, "rb").read()
    if len(data) != 768:
        raise ValueError("pal.dat deve ter 768 bytes, tem %d" % len(data))
    return [(data[3 * i + 2], data[3 * i + 1], data[3 * i]) for i in range(256)]


def swap_red_blue_table(palette):
    """Para cada indice, o indice cuja cor e a mais proxima da original com
    vermelho e azul trocados. O indice 0 (preto) fica fixo."""
    table = list(range(256))
    cache = {}
    for i, (r, g, b) in enumerate(palette):
        if i == 0:
            continue
        want = (b, g, r)
        if want not in cache:
            best, best_d = i, 1 << 30
            for j, (pr, pg, pb) in enumerate(palette):
                d = (pr - want[0]) ** 2 + (pg - want[1]) ** 2 + (pb - want[2]) ** 2
                if d < best_d:
                    best, best_d = j, d
            cache[want] = best
        table[i] = cache[want]
    return table


def remap_dc6(data, table):
    """Aplica `table` aos pixels literais do RLE de um DC6.

    Estrutura (docs/specs/dc6-format.md): cabecalho de 24 bytes
    (versao, flags, codificacao, terminador, direcoes, quadros por direcao),
    ponteiros de frame (4 bytes cada), e por frame: 32 bytes de cabecalho e
    `length` bytes de RLE (+3 de terminador). RLE: 0x80 fim de linha;
    0x81-0xFF pula (b & 0x7F) pixels transparentes; 0x01-0x7F copia b pixels.
    So os pixels literais mudam; o tamanho e a estrutura ficam intactos.
    """
    out = bytearray(data)
    version, _flags, _enc, _term, directions, per_dir = struct.unpack_from("<6I", data, 0)
    if version != 6:
        raise ValueError("DC6 com versao %d (esperado 6)" % version)
    count = directions * per_dir
    pointers = struct.unpack_from("<%dI" % count, data, 24)
    for ptr in pointers:
        _flip, _w, _h, _ox, _oy, _unk, _next, length = struct.unpack_from("<IIIiiIII", data, ptr)
        i, end = ptr + 32, ptr + 32 + length
        if end > len(data):
            raise ValueError("frame fora do arquivo")
        while i < end:
            b = data[i]
            if b == 0x80 or b & 0x80:
                i += 1
            else:
                for k in range(b):
                    out[i + 1 + k] = table[data[i + 1 + k]]
                i += 1 + b
    return bytes(out)


def list_mpq(extractor, mpq):
    """Nomes dentro do MPQ (o extrator diferencia maiusculas no prefixo)."""
    out = subprocess.run([extractor, mpq, "list"], check=True, stdout=subprocess.PIPE,
                         stderr=subprocess.DEVNULL, text=True, errors="replace").stdout
    return [line.strip() for line in out.splitlines() if line.strip()]


def extract(extractor, mpq, outdir, names, prefix):
    """Extrai as entradas cujo nome comeca com `prefix`, sem diferenciar caixa."""
    wanted = prefix.lower()
    for name in names:
        if name.lower().startswith(wanted):
            subprocess.run([extractor, mpq, "extract", outdir, name], check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def find_files(root, suffix=".dc6"):
    for base, _dirs, files in os.walk(root):
        for name in files:
            if name.lower().endswith(suffix):
                yield os.path.join(base, name)


def generate(d2dir, extractor, out):
    work = tempfile.mkdtemp(prefix="ae_art_")  # fora do repositorio
    try:
        for mpq in MPQS:
            path = os.path.join(d2dir, mpq)
            if not os.path.isfile(path):
                continue
            names = list_mpq(extractor, path)
            for prefix in BUTTON_PREFIXES + [PALETTE_PREFIX]:
                extract(extractor, path, work, names, prefix)
        pal_files = [p for p in find_files(work, ".dat") if p.lower().endswith("pal.dat")]
        if not pal_files:
            raise SystemExit("erro: nao consegui extrair a paleta do menu de %s" % d2dir)
        table = swap_red_blue_table(read_palette(pal_files[0]))
        written = 0
        for src in find_files(work):
            rel = os.path.relpath(src, work)
            dst = os.path.join(out, rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            with open(src, "rb") as f:
                data = f.read()
            with open(dst, "wb") as f:
                f.write(remap_dc6(data, table))
            written += 1
        if written == 0:
            raise SystemExit("erro: nenhum botao extraido (o extrator rodou? %s)" % extractor)
        os.makedirs(out, exist_ok=True)
        with open(os.path.join(out, "mod.ini"), "w", encoding="utf-8") as f:
            f.write(MOD_INI)
        print("art-demo gerado em %s (%d arquivo(s) DC6 recolorido(s))" % (out, written))
    finally:
        shutil.rmtree(work, ignore_errors=True)


def self_test():
    # Paleta sintetica: 0 preto, 1 vermelho, 2 azul, 3 verde.
    pal = [(0, 0, 0)] * 256
    pal[1], pal[2], pal[3] = (200, 0, 0), (0, 0, 200), (0, 200, 0)
    table = swap_red_blue_table(pal)
    assert table[0] == 0 and table[1] == 2 and table[2] == 1 and table[3] == 3, table[:4]

    # DC6 sintetico: 1 direcao, 1 frame 4x2: linha de cima [1,2,skip2], baixo [3,3,3,3].
    rle = bytes([2, 1, 2, 0x82, 0x80, 4, 3, 3, 3, 3, 0x80])
    frame = struct.pack("<IIIiiIII", 0, 4, 2, 0, 0, 0, 0, len(rle)) + rle + b"\x00\x00\x00"
    header = struct.pack("<6I", 6, 1, 0, 0, 1, 1) + struct.pack("<I", 28)
    dc6 = header + frame
    out = remap_dc6(dc6, table)
    assert len(out) == len(dc6)
    body = out[28 + 32: 28 + 32 + len(rle)]
    assert body == bytes([2, 2, 1, 0x82, 0x80, 4, 3, 3, 3, 3, 0x80]), body
    assert out[:28 + 32] == dc6[:28 + 32]            # cabecalhos intactos
    assert out[28 + 32 + len(rle):] == dc6[28 + 32 + len(rle):]  # terminador intacto
    print("make_art_demo self-test: OK")


def main(argv):
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    p.add_argument("--self-test", action="store_true")
    p.add_argument("--d2dir")
    p.add_argument("--extractor")
    p.add_argument("--out")
    a = p.parse_args(argv)
    if a.self_test:
        self_test()
        return 0
    if not (a.d2dir and a.extractor and a.out):
        p.error("--d2dir, --extractor e --out sao obrigatorios")
    generate(os.path.abspath(a.d2dir), os.path.abspath(a.extractor), os.path.abspath(a.out))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
