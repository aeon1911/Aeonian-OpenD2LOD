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
"""Empacota o Aeonian (DLLs compiladas + instalador + docs + exemplos) em um zip.

O zip tem o MESMO layout do repositorio (so o necessario), entao
`python3 tools/ae_install.py install --game-dir X --build-dir build/win32`
funciona de dentro dele. NAO inclui nada da Blizzard nem o D2GL.

Uso: package.py --build-dir build/win32 --out dist/aeonian-<versao>-win32.zip
"""

import argparse
import os
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

DLLS = ["aeonian.dll", "ijl11.dll", "hello.dll", "hud.dll"]  # sem o selftest (so teste)
FILES = [
    "README.md", "LICENSE", "NOTICE.md",
    "tools/ae_install.py", "tools/ae_mirror.sh", "tools/make_art_demo.py", "tools/bmp2png.py",
    "aeonian/include/aeonian/api.h",
    "aeonian/addresses/1.14d.ini", "aeonian/config/aeonian.ini",
    "aeonian/examples/hello-plugin/hello.cpp", "aeonian/examples/hello-plugin/mod.ini",
    "aeonian/examples/hud-demo/hud.cpp", "aeonian/examples/hud-demo/mod.ini",
    "docs/FINAL_TEST.md", "docs/HANDOFF.md", "docs/ROADMAP.md", "docs/SCOPE.md",
    "docs/modding/MODS.md", "docs/modding/PLUGIN_API.md", "docs/modding/D2GL.md",
]


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--build-dir", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--version", default="0.1.0")
    a = p.parse_args()
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    top = f"aeonian-{a.version}"
    with zipfile.ZipFile(a.out, "w", zipfile.ZIP_DEFLATED) as z:
        for dll in DLLS:
            src = os.path.join(a.build_dir, dll)
            if not os.path.isfile(src):
                raise SystemExit(f"erro: {src} nao existe (compile antes)")
            z.write(src, f"{top}/build/win32/{dll}")
        for rel in FILES:
            src = os.path.join(ROOT, rel)
            if not os.path.isfile(src):
                raise SystemExit(f"erro: {src} nao existe")
            info = zipfile.ZipInfo.from_file(src, f"{top}/{rel}")
            info.compress_type = zipfile.ZIP_DEFLATED
            with open(src, "rb") as f:
                z.writestr(info, f.read())
    print(f"{a.out}: {os.path.getsize(a.out) // 1024} KiB")


if __name__ == "__main__":
    main()
