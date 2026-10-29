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
"""Instala/desinstala o Aeonian numa instalacao do Diablo II LoD 1.14d.

Instalacao NAO destrutiva: o Game.exe nunca e alterado. O instalador

  1. confere o SHA-256 do Game.exe contra aeonian/addresses/*.ini;
  2. renomeia o ijl11.dll original para ijl11_real.dll e instala o proxy
     (ijl11.dll) no lugar;
  3. copia aeonian.dll, os bancos de enderecos e (se ausente) aeonian.ini
     para <jogo>/aeonian/, e cria a pasta mods/;
  4. opcional (--d2gl-dir): instala o D2GL (wrapper OpenGL GPL-3.0 de
     github.com/bayaraa/d2gl; alto desempenho em monitores modernos) a partir de
     uma pasta que o contenha, guardando como .aeonian-orig o que ja existir.

Tudo o que foi instalado fica registrado em <jogo>/aeonian/install.json; a
desinstalacao restaura o ijl11.dll original byte a byte.

Somente biblioteca padrao do Python 3.
"""

import argparse
import hashlib
import json
import os
import shutil
import sys

SOURCE_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AEONIAN_SRC = os.path.join(SOURCE_ROOT, "aeonian")
PROXY_MARKER = b"ijl11_real."  # presente no proxy, ausente na DLL original
MANIFEST = "install.json"
# Arquivos do D2GL: (nome, obrigatorio, sobrescrever_se_existir). O d2gl.ini e a
# configuracao do usuario: so e copiado se a pasta do jogo ainda nao tiver um.
D2GL_FILES = [("glide3x.dll", False, True), ("ddraw.dll", False, True),
              ("d2gl.mpq", True, True), ("d2gl.ini", False, False)]
D2GL_MARKER = b"d2gl.mpq"


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def is_our_proxy(path):
    with open(path, "rb") as f:
        return PROXY_MARKER in f.read()


def known_builds():
    """{sha256: nome_da_build} lido dos bancos de enderecos do repositorio."""
    builds = {}
    folder = os.path.join(AEONIAN_SRC, "addresses")
    for name in sorted(os.listdir(folder)):
        if not name.endswith(".ini"):
            continue
        build, digest = None, None
        with open(os.path.join(folder, name), encoding="utf-8") as f:
            for line in f:
                line = line.split(";")[0].strip()
                if "=" not in line:
                    continue
                key, value = (s.strip() for s in line.split("=", 1))
                if key.lower() == "name" and build is None:
                    build = value
                elif key.lower() == "sha256":
                    digest = value.lower()
        if build and digest:
            builds[digest] = build
    return builds


def is_d2gl_dll(path):
    with open(path, "rb") as f:
        return D2GL_MARKER in f.read()


def plan_d2gl(d2gl_dir, game_dir):
    """Valida a pasta do D2GL. Devolve [(origem, destino, nome, sobrescreve)] ou
    levanta ValueError com o motivo."""
    plan = []
    wrappers = 0
    for name, required, overwrite in D2GL_FILES:
        src = os.path.join(d2gl_dir, name)
        if not os.path.isfile(src):
            if required:
                raise ValueError(f"{name} nao encontrado em {d2gl_dir}")
            continue
        if name.endswith(".dll"):
            if not is_d2gl_dll(src):
                raise ValueError(f"{src} nao parece ser do D2GL (sem 'd2gl.mpq' dentro)")
            wrappers += 1
        plan.append((src, os.path.join(game_dir, name), name, overwrite))
    if wrappers == 0:
        raise ValueError(f"nenhum glide3x.dll/ddraw.dll do D2GL em {d2gl_dir}")
    return plan


def paths(game_dir):
    return {
        "exe": os.path.join(game_dir, "Game.exe"),
        "ijl": os.path.join(game_dir, "ijl11.dll"),
        "ijl_real": os.path.join(game_dir, "ijl11_real.dll"),
        "dir": os.path.join(game_dir, "aeonian"),
        "manifest": os.path.join(game_dir, "aeonian", MANIFEST),
    }


def fail(message):
    print(f"erro: {message}", file=sys.stderr)
    return 1


def cmd_install(args):
    p = paths(args.game_dir)
    if not os.path.isfile(p["exe"]):
        return fail(f"Game.exe nao encontrado em {args.game_dir}")

    digest = sha256(p["exe"])
    builds = known_builds()
    build = builds.get(digest)
    if build is None and not args.allow_unknown_build:
        return fail(
            f"Game.exe com SHA-256 {digest} nao e uma build conhecida "
            f"({', '.join(sorted(set(builds.values()))) or 'nenhuma'}). "
            "Use --allow-unknown-build para instalar mesmo assim (o motor se "
            "desativa em runtime, salvo allow_unknown_build=1 em aeonian.ini)."
        )

    proxy_src = os.path.join(args.build_dir, "ijl11.dll")
    core_src = os.path.join(args.build_dir, "aeonian.dll")
    for src in (proxy_src, core_src):
        if not os.path.isfile(src):
            return fail(f"{src} nao existe; compile primeiro (ver README)")

    # Situacao do ijl11.dll no jogo.
    have_ijl = os.path.isfile(p["ijl"])
    have_real = os.path.isfile(p["ijl_real"])
    plan = []
    if have_ijl and is_our_proxy(p["ijl"]):
        if not have_real:
            return fail("ijl11.dll e o proxy do Aeonian mas falta ijl11_real.dll "
                        "(reponha o ijl11.dll original do jogo antes)")
        plan.append("atualizar proxy ijl11.dll")
    elif have_ijl:
        if have_real:
            return fail("existem ijl11.dll (original) e ijl11_real.dll; resolva "
                        "manualmente (remova a copia que nao e a original)")
        plan.append("ijl11.dll original -> ijl11_real.dll")
        plan.append("instalar proxy ijl11.dll")
    else:
        return fail("a instalacao nao tem ijl11.dll (biblioteca JPEG da Intel que "
                    "o Game.exe 1.14d importa); sem ela o jogo nem inicia")

    d2gl_plan = []
    if args.d2gl_dir:
        try:
            d2gl_plan = plan_d2gl(os.path.abspath(args.d2gl_dir), args.game_dir)
        except ValueError as e:
            return fail(str(e))
        have_ddraw = any(n == "ddraw.dll" for _s, _d, n, _o in d2gl_plan)
        if have_ddraw:
            print("aviso: ddraw.dll do D2GL: no Wine/Proton defina WINEDLLOVERRIDES=ddraw=n,b "
                  "(e o wrapper DDraw do D2GL travou nos testes sob Wine; prefira o glide3x.dll)")

    print(f"jogo: {args.game_dir}")
    print(f"build: {build or 'DESCONHECIDA'} (sha256 {digest[:16]}...)")
    for step in plan:
        print(f"  - {step}")
    print("  - copiar aeonian.dll, addresses/, aeonian.ini (se ausente), mods/")
    for _src, dst, name, overwrite in d2gl_plan:
        action = "copiar" if overwrite or not os.path.exists(dst) else "manter (ja existe)"
        print(f"  - D2GL: {action} {name}")
    if args.dry_run:
        print("(dry-run: nada foi alterado)")
        return 0

    os.makedirs(p["dir"], exist_ok=True)
    installed = {}

    if not have_real:
        original_digest = sha256(p["ijl"])
        os.replace(p["ijl"], p["ijl_real"])  # preserva a original
    else:
        original_digest = sha256(p["ijl_real"])
    shutil.copy2(proxy_src, p["ijl"])
    installed["ijl11.dll"] = sha256(p["ijl"])

    shutil.copy2(core_src, os.path.join(p["dir"], "aeonian.dll"))
    installed[os.path.join("aeonian", "aeonian.dll")] = sha256(
        os.path.join(p["dir"], "aeonian.dll"))

    dst_addr = os.path.join(p["dir"], "addresses")
    os.makedirs(dst_addr, exist_ok=True)
    src_addr = os.path.join(AEONIAN_SRC, "addresses")
    for name in sorted(os.listdir(src_addr)):
        if name.endswith(".ini"):
            shutil.copy2(os.path.join(src_addr, name), os.path.join(dst_addr, name))
            installed[os.path.join("aeonian", "addresses", name)] = sha256(
                os.path.join(dst_addr, name))

    config = os.path.join(p["dir"], "aeonian.ini")
    if not os.path.exists(config):  # ajustes do usuario nunca sao sobrescritos
        shutil.copy2(os.path.join(AEONIAN_SRC, "config", "aeonian.ini"), config)
    os.makedirs(os.path.join(p["dir"], "mods"), exist_ok=True)

    d2gl_backups = {}
    for src, dst, name, overwrite in d2gl_plan:
        if os.path.exists(dst):
            if not overwrite:
                continue  # configuracao do usuario (d2gl.ini): nunca sobrescreve
            if sha256(dst) == sha256(src):
                installed[name] = sha256(dst)  # ja e esta mesma versao
                continue
            backup = dst + ".aeonian-orig"
            if not os.path.exists(backup):
                os.replace(dst, backup)
                d2gl_backups[name] = os.path.basename(backup)
        shutil.copy2(src, dst)
        if name != "d2gl.ini":  # configuracao do usuario: nunca entra na lista de remocao
            installed[name] = sha256(dst)

    with open(p["manifest"], "w", encoding="utf-8") as f:
        json.dump({
            "build": build,
            "game_exe_sha256": digest,
            "original_ijl11_sha256": original_digest,
            "installed": installed,
            "d2gl_backups": d2gl_backups,
        }, f, indent=2)
    print("instalado. Mods vao em", os.path.join(p["dir"], "mods"))
    return 0


def cmd_uninstall(args):
    p = paths(args.game_dir)
    if not os.path.isfile(p["manifest"]):
        return fail("Aeonian nao parece instalado (sem aeonian/install.json)")
    with open(p["manifest"], encoding="utf-8") as f:
        info = json.load(f)

    if os.path.isfile(p["ijl_real"]):
        if sha256(p["ijl_real"]) != info["original_ijl11_sha256"]:
            return fail("ijl11_real.dll mudou desde a instalacao; nao vou "
                        "sobrescrever o ijl11.dll sem sua confirmacao")
        if os.path.isfile(p["ijl"]):
            if not is_our_proxy(p["ijl"]):
                return fail("ijl11.dll atual nao e o proxy do Aeonian; abortando")
            os.remove(p["ijl"])
        os.replace(p["ijl_real"], p["ijl"])
        print("ijl11.dll original restaurado")
    for rel in info["installed"]:
        if rel == "ijl11.dll":
            continue
        target = os.path.join(args.game_dir, rel)
        if os.path.isfile(target):
            os.remove(target)
    for name, backup in info.get("d2gl_backups", {}).items():  # devolve o que existia antes
        src = os.path.join(args.game_dir, backup)
        if os.path.isfile(src):
            os.replace(src, os.path.join(args.game_dir, name))
            print(f"{name} original restaurado")
    addr = os.path.join(p["dir"], "addresses")
    if os.path.isdir(addr) and not os.listdir(addr):
        os.rmdir(addr)
    os.remove(p["manifest"])
    if args.purge:
        shutil.rmtree(p["dir"], ignore_errors=True)
        print("pasta aeonian/ removida (inclui mods e configuracao)")
    else:
        print("mantidos: aeonian/mods, aeonian/aeonian.ini (use --purge para apagar) e d2gl.ini (sua configuracao)")
    return 0


def cmd_status(args):
    p = paths(args.game_dir)
    if not os.path.isfile(p["manifest"]):
        print("Aeonian: NAO instalado")
        return 0
    with open(p["manifest"], encoding="utf-8") as f:
        info = json.load(f)
    ok = os.path.isfile(p["ijl_real"]) and os.path.isfile(p["ijl"]) and is_our_proxy(p["ijl"])
    print(f"Aeonian: instalado (build {info.get('build')}); proxy {'OK' if ok else 'PROBLEMA'}")
    for rel, digest in info["installed"].items():
        target = os.path.join(args.game_dir, rel)
        state = "ok" if os.path.isfile(target) and sha256(target) == digest else "ALTERADO/AUSENTE"
        print(f"  {rel}: {state}")
    return 0 if ok else 1


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)
    for name, fn in (("install", cmd_install), ("uninstall", cmd_uninstall),
                     ("status", cmd_status)):
        s = sub.add_parser(name)
        s.add_argument("--game-dir", required=True,
                       help="pasta do Diablo II (a que contem Game.exe)")
        s.set_defaults(fn=fn)
        if name == "install":
            s.add_argument("--build-dir", required=True,
                           help="onde estao aeonian.dll e ijl11.dll compilados")
            s.add_argument("--allow-unknown-build", action="store_true")
            s.add_argument("--d2gl-dir",
                           help="pasta com o D2GL (glide3x.dll/ddraw.dll + d2gl.mpq [+ d2gl.ini]) "
                                "a instalar junto; ver docs/modding/D2GL.md")
            s.add_argument("--dry-run", action="store_true")
        if name == "uninstall":
            s.add_argument("--purge", action="store_true",
                           help="apaga tambem aeonian/mods e aeonian.ini")
    args = parser.parse_args(argv)
    args.game_dir = os.path.abspath(args.game_dir)
    return args.fn(args)


if __name__ == "__main__":
    sys.exit(main())
