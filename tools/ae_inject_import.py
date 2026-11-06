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
"""Injeta uma secao de import PE (.aeon) num executavel PE de 32 bits (Game.exe)
para carregar aeonian.dll e chamar AeonianBootstrap diretamente pelo loader do Windows.

Util para ambientes onde o proxy ijl11.dll nao e carregado (por exemplo, Switch sob
Wine-NX / AUTORUN onde DLLs do sistema ou caminhos sao redirecionados).

Cria backup .orig antes de modificar. Somente biblioteca padrao do Python 3.
"""

import argparse
import os
import shutil
import struct
import sys

def align_up(x: int, a: int) -> int:
    return (x + a - 1) & ~(a - 1)

def inject_import(exe_path: str, dll_name: str = "aeonian.dll", proc_name: str = "AeonianBootstrap") -> bool:
    if not os.path.exists(exe_path):
        print(f"Erro: arquivo nao encontrado: {exe_path}", file=sys.stderr)
        return False

    backup_path = exe_path + ".orig"
    if not os.path.exists(backup_path):
        shutil.copy2(exe_path, backup_path)
        print(f"Backup criado: {backup_path}")

    with open(backup_path, "rb") as f:
        data = bytearray(f.read())

    # DOS Header
    if len(data) < 0x40 or data[:2] != b"MZ":
        print("Erro: cabecalho DOS invalido", file=sys.stderr)
        return False
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]

    # NT Signature
    if data[e_lfanew:e_lfanew+4] != b"PE\x00\x00":
        print("Erro: assinatura PE invalida", file=sys.stderr)
        return False

    # File Header
    num_sec = struct.unpack_from("<H", data, e_lfanew + 6)[0]
    opt_hdr_sz = struct.unpack_from("<H", data, e_lfanew + 20)[0]
    opt_hdr_off = e_lfanew + 24
    sec_tbl_off = opt_hdr_off + opt_hdr_sz

    # Optional Header (PE32)
    magic = struct.unpack_from("<H", data, opt_hdr_off)[0]
    if magic != 0x10B:
        print(f"Erro: esperado PE32 (0x10B), encontrado 0x{magic:X}", file=sys.stderr)
        return False

    sec_align = struct.unpack_from("<I", data, opt_hdr_off + 32)[0]
    file_align = struct.unpack_from("<I", data, opt_hdr_off + 36)[0]
    orig_import_rva, orig_import_sz = struct.unpack_from("<II", data, opt_hdr_off + 96 + 8)

    sections = []
    max_vaddr_end = 0
    max_roff_end = 0
    for i in range(num_sec):
        s_off = sec_tbl_off + i * 40
        name = data[s_off:s_off+8].rstrip(b"\x00").decode(errors="ignore")
        vsize, vaddr, rsize, roff = struct.unpack_from("<IIII", data, s_off + 8)
        if name == ".aeon":
            print("Executavel ja contem a secao .aeon! Ignorando reinjecao.")
            return True
        sections.append({"name": name, "vsize": vsize, "vaddr": vaddr, "rsize": rsize, "roff": roff})
        max_vaddr_end = max(max_vaddr_end, vaddr + align_up(vsize, sec_align))
        max_roff_end = max(max_roff_end, roff + align_up(rsize, file_align))

    new_vaddr = max_vaddr_end
    new_roff = max_roff_end

    # Acha o offset no arquivo da tabela de imports original
    orig_import_off = None
    for s in sections:
        if s["vaddr"] <= orig_import_rva < s["vaddr"] + s["vsize"]:
            orig_import_off = s["roff"] + (orig_import_rva - s["vaddr"])
            break
    if orig_import_off is None:
        print("Erro: RVA da tabela de import original nao encontrada nas secoes", file=sys.stderr)
        return False

    # Le descritores originais ate o terminador nulo (20 bytes zero)
    orig_desc = bytearray()
    curr_off = orig_import_off
    while True:
        desc = data[curr_off:curr_off+20]
        oft, ts, fc, name_rva, ft = struct.unpack_from("<IIIII", desc, 0)
        if not (oft or ts or fc or name_rva or ft):
            break
        orig_desc.extend(desc)
        curr_off += 20

    n_orig = len(orig_desc) // 20
    print(f"Lidos {n_orig} descritores de import originais da RVA 0x{orig_import_rva:X}")

    # Layout da nova secao .aeon
    desc_table_sz = (n_orig + 2) * 20
    dll_name_off = desc_table_sz
    dll_bytes = dll_name.encode("ascii") + b"\x00"
    if len(dll_bytes) % 2 != 0:
        dll_bytes += b"\x00"

    proc_name_off = dll_name_off + len(dll_bytes)
    # Hint (2 bytes) + Nome da proc + terminador
    proc_bytes = struct.pack("<H", 0) + proc_name.encode("ascii") + b"\x00"
    if len(proc_bytes) % 4 != 0:
        proc_bytes += b"\x00" * (4 - (len(proc_bytes) % 4))

    ilt_off = proc_name_off + len(proc_bytes)
    # ILT (Import Lookup Table): ponteiro para proc + terminador nulo
    proc_rva = new_vaddr + proc_name_off
    ilt_bytes = struct.pack("<II", proc_rva, 0)

    iat_off = ilt_off + len(ilt_bytes)
    # IAT (Import Address Table) inicializada identica a ILT
    iat_bytes = ilt_bytes

    sec_data_sz = iat_off + len(iat_bytes)
    sec_raw_sz = align_up(sec_data_sz, file_align)
    sec_virt_sz = align_up(sec_data_sz, sec_align)

    # Constroi os dados da nova secao
    sec_data = bytearray(sec_raw_sz)
    # Copia descritores originais
    sec_data[0:len(orig_desc)] = orig_desc

    # Novo descritor para aeonian.dll
    new_desc = struct.pack("<IIIII",
        new_vaddr + ilt_off,        # OriginalFirstThunk (ILT)
        0,                          # TimeDateStamp
        0,                          # ForwarderChain
        new_vaddr + dll_name_off,   # Name RVA
        new_vaddr + iat_off         # FirstThunk (IAT)
    )
    sec_data[len(orig_desc):len(orig_desc)+20] = new_desc
    # Terminador nulo (20 bytes zero ja estao zerados pelo bytearray)

    sec_data[dll_name_off:dll_name_off+len(dll_bytes)] = dll_bytes
    sec_data[proc_name_off:proc_name_off+len(proc_bytes)] = proc_bytes
    sec_data[ilt_off:ilt_off+len(ilt_bytes)] = ilt_bytes
    sec_data[iat_off:iat_off+len(iat_bytes)] = iat_bytes

    # Anexa a nova secao ao final do arquivo
    data.extend(sec_data)

    # Escreve o novo cabecalho de secao
    new_sec_off = sec_tbl_off + num_sec * 40
    new_sec_header = struct.pack("<8sIIIIIIHHI",
        b".aeon\x00\x00\x00",
        sec_virt_sz,
        new_vaddr,
        sec_raw_sz,
        new_roff,
        0, 0, 0, 0,
        0xC0000040  # IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE | IMAGE_SCN_CNT_INITIALIZED_DATA
    )
    data[new_sec_off:new_sec_off+40] = new_sec_header

    # Atualiza File Header (num_sec + 1)
    struct.pack_into("<H", data, e_lfanew + 6, num_sec + 1)

    # Atualiza Optional Header: SizeOfImage e Import Table Directory
    new_image_sz = new_vaddr + sec_virt_sz
    struct.pack_into("<I", data, opt_hdr_off + 56, new_image_sz)
    struct.pack_into("<II", data, opt_hdr_off + 96 + 8, new_vaddr, desc_table_sz)

    with open(exe_path, "wb") as f:
        f.write(data)

    print(f"Secao .aeon injetada com sucesso em {exe_path}!")
    print(f"  Import Table RVA: 0x{new_vaddr:X} (tamanho {desc_table_sz} bytes)")
    print(f"  DLL: {dll_name}, Funcao: {proc_name}")
    print(f"  SizeOfImage atualizado para 0x{new_image_sz:X}")
    return True

def main():
    parser = argparse.ArgumentParser(description="Injeta secao de import do Aeonian em Game.exe")
    parser.add_argument("exe", help="Caminho para o executavel PE (Game.exe)")
    parser.add_argument("--dll", default="aeonian.dll", help="Nome da DLL a carregar (padrao: aeonian.dll)")
    parser.add_argument("--proc", default="AeonianBootstrap", help="Nome da funcao exportada (padrao: AeonianBootstrap)")
    args = parser.parse_args()

    if not inject_import(args.exe, args.dll, args.proc):
        sys.exit(1)

if __name__ == "__main__":
    main()

