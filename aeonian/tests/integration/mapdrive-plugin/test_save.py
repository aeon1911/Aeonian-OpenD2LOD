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
"""Prepara o personagem de TESTE do condutor mapdrive (so para tests/integration).

Leva um .d2s recem-criado pelo proprio jogo, dentro de um prefixo Wine de teste, ate
o ato e a dificuldade pedidos, para validar o mod map-overlay fora do Ato I sem jogar
a campanha. Usa so o formato publico do .d2s (versao 96). Recusa qualquer arquivo
fora de --test-root: nunca aponte para os saves de verdade.

  test_save.py --test-root DIR --save ARQ.d2s --act 2 --difficulty 0 [--sorceress] [--merc]
"""

import argparse
import os
import struct
import sys

OFF_SIZE = 0x08
OFF_CHECKSUM = 0x0C
OFF_PROGRESSION = 0x25
OFF_CLASS = 0x28
OFF_LEVEL = 0x2B
OFF_HOTKEYS = 0x38
OFF_LEFT_SKILL = 0x78
OFF_RIGHT_SKILL = 0x7C
OFF_DIFFICULTY = 0xA8
OFF_MERC_SEED = 0xB3
OFF_MERC_EXP = 0xBB
OFF_QUESTS = 0x14F
OFF_WAYPOINTS = 0x279
OFF_STATS = 0x2FD

CLASS_SORCERESS = 1
FIRST_SORCERESS_SKILL = 36
SKILL_TELEPORT = 54
SKILL_HYDRA = 62
NO_SKILL = 0xFFFF

# id -> bits, na ordem do formato.
STAT_BITS = {0: 10, 1: 10, 2: 10, 3: 10, 4: 10, 5: 8, 6: 21, 7: 21, 8: 21, 9: 21, 10: 21,
             11: 21, 12: 7, 13: 32, 14: 25, 15: 25}


def pack_stats(stats):
    bits = 0
    count = 0

    def put(value, width):
        nonlocal bits, count
        bits |= (value & ((1 << width) - 1)) << count
        count += width

    for stat, value in sorted(stats.items()):
        put(stat, 9)
        put(value, STAT_BITS[stat])
    put(0x1FF, 9)
    return bits.to_bytes((count + 7) // 8, "little")


def checksum(data):
    data = bytearray(data)
    data[OFF_CHECKSUM:OFF_CHECKSUM + 4] = b"\0\0\0\0"
    total = 0
    for byte in data:
        total = ((total << 1) | (total >> 31)) & 0xFFFFFFFF
        total = (total + byte) & 0xFFFFFFFF
    return total


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--test-root", required=True)
    parser.add_argument("--save", required=True)
    parser.add_argument("--act", type=int, default=1, help="1 a 5")
    parser.add_argument("--difficulty", type=int, default=0, help="0 Normal, 1 Pesadelo, 2 Inferno")
    parser.add_argument("--level", type=int, default=90)
    parser.add_argument("--sorceress", action="store_true",
                        help="troca a classe e da Teletransporte (F1) e Hidra (F2)")
    parser.add_argument("--merc", action="store_true", help="contrata um mercenario")
    args = parser.parse_args()

    root = os.path.realpath(args.test_root)
    path = os.path.realpath(args.save)
    if not path.startswith(root + os.sep):
        sys.exit("recusado: o save nao esta dentro de --test-root")

    data = bytearray(open(path, "rb").read())
    if data[:4] != b"\x55\xaa\x55\xaa" or struct.unpack_from("<I", data, 4)[0] != 96:
        sys.exit("nao e um .d2s versao 96")
    if data[OFF_STATS:OFF_STATS + 2] != b"gf":
        sys.exit("secao de atributos nao encontrada")

    data[OFF_PROGRESSION] = 15  # todas as dificuldades liberadas
    data[OFF_LEVEL] = args.level
    for d in range(3):
        data[OFF_DIFFICULTY + d] = 0
    data[OFF_DIFFICULTY + args.difficulty] = 0x80 | (args.act - 1)
    # Todos os waypoints, nas tres dificuldades (blocos de 24 bytes depois do cabecalho).
    for d in range(3):
        start = OFF_WAYPOINTS + 8 + d * 24 + 2
        data[start:start + 5] = b"\xff\xff\xff\xff\x7f"

    # Atos ja visitados nas tres dificuldades: o painel de waypoint so mostra as
    # abas deles (palavras de 16 bits depois do cabecalho de 10 bytes de "Woo!").
    for d in range(3):
        for word in (7, 8, 15, 16, 23, 24, 28, 29, 30):
            struct.pack_into("<H", data, OFF_QUESTS + 10 + d * 96 + word * 2, 1)

    skills = OFF_STATS + 2 + data[OFF_STATS + 2:].index(b"if")
    stats = pack_stats({0: 200, 1: 1000, 2: 200, 3: 1000, 6: 8000 << 8, 7: 8000 << 8,
                        8: 8000 << 8, 9: 8000 << 8, 10: 2000 << 8, 11: 2000 << 8,
                        12: args.level, 13: 1764543065})
    data[OFF_STATS + 2:skills] = stats
    skills = OFF_STATS + 2 + len(stats)

    if args.sorceress:
        data[OFF_CLASS] = CLASS_SORCERESS
        data[skills + 2 + SKILL_TELEPORT - FIRST_SORCERESS_SKILL] = 20
        data[skills + 2 + SKILL_HYDRA - FIRST_SORCERESS_SKILL] = 20
        for key in range(16):
            struct.pack_into("<I", data, OFF_HOTKEYS + key * 4, NO_SKILL)
        struct.pack_into("<I", data, OFF_HOTKEYS, SKILL_TELEPORT)
        struct.pack_into("<I", data, OFF_HOTKEYS + 4, SKILL_HYDRA)
        struct.pack_into("<I", data, OFF_LEFT_SKILL, 0)
        struct.pack_into("<I", data, OFF_RIGHT_SKILL, SKILL_TELEPORT)

    if args.merc and struct.unpack_from("<I", data, OFF_MERC_SEED)[0] == 0:
        struct.pack_into("<I", data, OFF_MERC_SEED, 0x1234ABCD)
        struct.pack_into("<I", data, OFF_MERC_EXP, 100000)
        # Com mercenario, a secao "jf" traz a lista de itens dele.
        tail = bytes(data).rindex(b"jfkf")
        data[tail:tail + 4] = b"jfJM\0\0kf"

    struct.pack_into("<I", data, OFF_SIZE, len(data))
    struct.pack_into("<I", data, OFF_CHECKSUM, checksum(data))
    with open(path, "wb") as f:
        f.write(data)
    print(f"{os.path.basename(path)}: ato {args.act}, dificuldade {args.difficulty}, "
          f"nivel {args.level}, {len(data)} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
