#!/usr/bin/env bash
# Aeonian-OpenD2LOD - GPLv3
#
# Teste de integracao: roda o Game.exe 1.14d REAL (no Wine) com o Aeonian
# instalado e confere o log e os quadros gravados pelo motor. Nao altera a
# instalacao do usuario: trabalha numa copia-espelho (symlinks para os MPQs)
# fora do repositorio. Nunca captura a area de trabalho: so o log do motor e os
# BMP que o proprio motor grava a partir da superficie/framebuffer do jogo.
#
# Variaveis:
#   AEONIAN_D2_DIR         (obrigatoria) pasta do Diablo II 1.14d (com Game.exe)
#   AEONIAN_BUILD_DIR      DLLs compiladas (padrao: build/win32)
#   AEONIAN_WORK_DIR       pasta de trabalho (padrao: mktemp)
#   AEONIAN_WINEPREFIX     prefixo Wine de TESTE (padrao: <work>/prefix)
#   AEONIAN_EXTRA_DLL_DIR  de onde copiar binkw32/SmackW32/ijl11 se faltarem
#   AEONIAN_MPQ_EXTRACT    ae_mpq_extract; sem ele, a fase de arte/VFS e pulada
#   AEONIAN_D2GL_DIR       pasta com o D2GL (glide3x.dll + d2gl.mpq [+ d2gl.ini]);
#                          sem ela, a fase D2GL e pulada
#   AEONIAN_RUN_SECONDS    duracao de cada execucao (padrao: 16)
#   WINE                   executavel do wine (padrao: wine)
#
# Requer um display (o jogo abre uma janela). Saida 0 = tudo passou,
# 1 = falhou algo, 77 = pre-requisito ausente (teste pulado).

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
HERE="$ROOT/aeonian/tests/integration"
D2="${AEONIAN_D2_DIR:-}"
BUILD="${AEONIAN_BUILD_DIR:-$ROOT/build/win32}"
WINE="${WINE:-wine}"
SECS="${AEONIAN_RUN_SECONDS:-16}"
WORK="${AEONIAN_WORK_DIR:-$(mktemp -d)}"
PREFIX="${AEONIAN_WINEPREFIX:-$WORK/prefix}"
D2GL="${AEONIAN_D2GL_DIR:-}"

skip() { echo "SKIP: $*"; exit 77; }
[[ -n "$D2" && -f "$D2/Game.exe" ]] || skip "defina AEONIAN_D2_DIR (pasta com Game.exe 1.14d)"
command -v "$WINE" >/dev/null || skip "wine nao encontrado"
[[ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]] || skip "sem display"
for f in aeonian.dll ijl11.dll selftest.dll hud.dll; do
  [[ -f "$BUILD/$f" ]] || skip "$BUILD/$f ausente; compile antes (cmake --build)"
done

GAME="$WORK/game"
MODS="$GAME/aeonian/mods"
FAILS=0
pass() { echo "  PASS  $*"; }
fail() { echo "  FAIL  $*"; FAILS=$((FAILS + 1)); }
log_has() { grep -qE -- "$1" "$GAME/aeonian/aeonian.log" 2>/dev/null; }
expect() { if log_has "$2"; then pass "$1"; else fail "$1  (/$2/ nao esta no log)"; fi; }
forbid() { if log_has "$2"; then fail "$1  (/$2/ apareceu no log)"; else pass "$1"; fi; }
check() { # check "descricao" comando...   (o comando decide pelo codigo de saida)
  local what="$1"; shift
  local out; out="$("$@" 2>&1)"
  if [[ $? -eq 0 ]]; then pass "$what ($out)"; else fail "$what ($out)"; fi
}

reg_render() { # valor do registro Render do D2 (1 = GDI; !=1 = modos aceleraveis)
  WINEPREFIX="$PREFIX" WINEDEBUG=-all "$WINE" reg add \
    'HKCU\Software\Blizzard Entertainment\Diablo II\VideoConfig' /v Render /t REG_DWORD /d "$1" /f >/dev/null 2>&1
}

run_game() { # run_game <segundos> [VAR=valor...]
  local secs="$1"; shift
  ( cd "$GAME" && env WINEPREFIX="$PREFIX" WINEDEBUG=-all \
      WINEDLLOVERRIDES="winemenubuilder.exe=d" "$@" \
      timeout "$secs" "$WINE" Game.exe >"$WORK/wine.out" 2>&1 )
  local rc=$?
  # 124 = o `timeout` encerrou um jogo que seguia vivo (esperado).
  [[ $rc -eq 124 ]] || fail "o jogo terminou sozinho antes de ${secs}s (rc=$rc); ver $WORK/wine.out"
}

write_config() { printf '%b' "$1" >"$GAME/aeonian/aeonian.ini"; }
clear_shots() { rm -f "$MODS/selftest/"*.bmp; }

echo "== preparando prefixo Wine"
WINEPREFIX="$PREFIX" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" timeout 120 "$WINE" wineboot -u >/dev/null 2>&1 || true

echo "== preparando espelho em $GAME"
rm -rf "$GAME"; mkdir -p "$GAME"
for f in "$D2"/*; do
  case "$(basename "$f")" in
    Game.exe|ijl11.dll|binkw32.dll|SmackW32.dll|smackw32.dll) cp --reflink=auto "$f" "$GAME/" ;;
    data|aeonian|glide3x.dll|ddraw.dll|d2gl.*) ;;  # nao herdar
    *) ln -s "$f" "$GAME/" ;;
  esac
done
for f in binkw32.dll SmackW32.dll ijl11.dll; do
  [[ -f "$GAME/$f" ]] && continue
  [[ -n "${AEONIAN_EXTRA_DLL_DIR:-}" && -f "$AEONIAN_EXTRA_DLL_DIR/$f" ]] \
    && cp "$AEONIAN_EXTRA_DLL_DIR/$f" "$GAME/" || skip "falta $f (defina AEONIAN_EXTRA_DLL_DIR)"
done
ORIG_IJL="$(sha256sum "$GAME/ijl11.dll" | cut -d' ' -f1)"

echo "== instalando o Aeonian"
python3 -I "$ROOT/tools/ae_install.py" install --game-dir "$GAME" --build-dir "$BUILD" >/dev/null \
  && pass "instalador" || { fail "instalador"; exit 1; }
[[ "$(sha256sum "$GAME/ijl11_real.dll" | cut -d' ' -f1)" == "$ORIG_IJL" ]] \
  && pass "ijl11.dll original preservado byte a byte" || fail "ijl11_real.dll difere do original"
cmp -s "$GAME/Game.exe" "$D2/Game.exe" && pass "Game.exe intocado" || fail "Game.exe foi alterado"

mkdir -p "$MODS/selftest" "$MODS/hud-demo"
cp "$BUILD/selftest.dll" "$MODS/selftest/"
printf '[mod]\nid = selftest\nplugin = selftest.dll\nbuilds = 1.14d\n' >"$MODS/selftest/mod.ini"
cp "$BUILD/hud.dll" "$MODS/hud-demo/"
cp "$ROOT/aeonian/examples/hud-demo/mod.ini" "$MODS/hud-demo/mod.ini"

# ---------------------------------------------------------------- fase A (GDI)
echo "== fase A: renderizador original (GDI), plugins, overlay e quadro (${SECS}s)"
reg_render 1
write_config '[core]\nlog_level = debug\nextra_args = -w -ns\n[game]\nskip_intro = true\n'
clear_shots
run_game "$SECS" AEONIAN_HUD_OPEN=1
expect "build reconhecida"                      'build reconhecida: 1\.14d'
expect "13/13 importacoes interceptadas"        '13/13 importacoes interceptadas'
expect "2 plugins carregados"                   'inicializado: 2 mod\(s\), 2 plugin\(s\)'
expect "resolve_symbol verifica bytes no binario real" 'resolve_symbol\(Game\.EntryPoint\)=0x00682985 PASS'
expect "hook inline recusa bytes errados"       'hook_inline com bytes errados .* PASS'
expect "hook inline desvia e preserva a original" 'hook_inline rc=0 antes=5 depois=50 PASS'
expect "relocacao de call no codigo real"      'relocacao de call no EntryPoint real .* PASS'
expect "janela principal interceptada"          "janela 'Diablo II'"
expect "on_loaded"                              'SELFTEST on_loaded recebido'
expect "register_events tardio recusado"       'register_events tardio rc=-2 PASS'
expect "on_tick"                                'on_tick alcancou 100 PASS'
expect "on_window_message recebe mensagens do jogo" 'on_window_message recebeu a 1a mensagem do jogo .* PASS'
expect "on_window_message consome mensagem (prova determinista)" 'consumiu a mensagem de prova .* PASS'
expect "backend GDI reconhecido (quadro 800x600, 8 bpp)" 'GDI: quadro 800x600 8 bpp via StretchBlt'
expect "orientacao do DIB vem do CreateDIBSection" 'orientacao de cima para baixo \(origem: CreateDIBSection\)'
expect "on_present dos plugins"                 'SELFTEST on_present quadro 30 \(800x600\) PASS'
expect "screenshot do quadro gravado"           'screenshot gravado: .*shot_overlay\.bmp \(800x600, 8 bpp\)'
forbid "sem excecoes de 1a chance"              'excecao de 1a chance'
forbid "sem falhas de selftest"                 'SELFTEST .* FAIL'
check "quadro do titulo tem 800x600"            python3 -I "$HERE/bmp_check.py" size "$MODS/selftest/shot_overlay.bmp" 800 600
check "overlay (retangulo vermelho) no canto superior esquerdo" python3 -I "$HERE/bmp_check.py" red "$MODS/selftest/shot_overlay.bmp" 100 35
check "quadro do jogo de pe (nao invertido)"    python3 -I "$HERE/bmp_check.py" upright "$MODS/selftest/shot_overlay.bmp"
check "menu principal gravado (800x600)"        python3 -I "$HERE/bmp_check.py" size "$MODS/selftest/shot_menu.bmp" 800 600
cp "$MODS/selftest/shot_menu.bmp" "$WORK/menu_base.bmp" 2>/dev/null

# ------------------------------------------------- fase A2 (mod de arte via VFS)
if [[ -n "${AEONIAN_MPQ_EXTRACT:-}" && -x "${AEONIAN_MPQ_EXTRACT}" ]]; then
  echo "== fase A2: mod de arte (botoes recoloridos) por VFS"
  python3 -I "$ROOT/tools/make_art_demo.py" --self-test >/dev/null && pass "transformador de DC6 (autoteste)" || fail "transformador de DC6"
  if python3 -I "$ROOT/tools/make_art_demo.py" --d2dir "$D2" --extractor "$AEONIAN_MPQ_EXTRACT" \
        --out "$MODS/art-demo" >/dev/null 2>&1; then
    clear_shots
    run_game "$SECS" AEONIAN_HUD_OPEN=1
    expect "-direct injetado automaticamente"   'linha de comando: .*-direct'
    expect "botao do menu redirecionado ao mod" "vfs: data\\\\GLOBAL\\\\ui\\\\FrontEnd\\\\WideButtonBlank\.dc6 -> mod 'art-demo'"
    forbid "overrides abriram sem erro"         'vfs: falha ao abrir'
    forbid "sem excecoes de 1a chance (arte)"   'excecao de 1a chance'
    check "menu com mod de arte gravado"        python3 -I "$HERE/bmp_check.py" size "$MODS/selftest/shot_menu.bmp" 800 600
    check "botoes do menu mudaram de cor (diff vs. sem o mod)" \
          python3 -I "$HERE/bmp_check.py" diff "$WORK/menu_base.bmp" "$MODS/selftest/shot_menu.bmp" 260 285 540 430 600
  else
    fail "geracao do mod de arte"
  fi
  rm -rf "$MODS/art-demo"
else
  echo "  SKIP  fase de arte/VFS (defina AEONIAN_MPQ_EXTRACT)"
fi

# --------------------------------------------------------- fase B (D2GL, Glide)
if [[ -n "$D2GL" && -f "$D2GL/glide3x.dll" ]]; then
  echo "== fase B: D2GL (OpenGL) via glide3x.dll: instalacao, -3dfx, overlay GL, intro"
  # Um glide3x.dll PREEXISTENTE (qualquer um) deve ser guardado e depois restaurado.
  printf 'GLIDE-ORIGINAL-DO-USUARIO' >"$GAME/glide3x.dll"
  GLIDE_ORIG="$(sha256sum "$GAME/glide3x.dll" | cut -d' ' -f1)"
  # d2gl.ini do usuario (janela contida de teste); o instalador NAO pode sobrescreve-lo.
  if [[ -f "$D2GL/d2gl.ini" ]]; then cp "$D2GL/d2gl.ini" "$GAME/d2gl.ini"; else printf '[Screen]\n' >"$GAME/d2gl.ini"; fi
  sed -i 's/^fullscreen=.*/fullscreen=false/; s/^window_width=.*/window_width=1280/; s/^window_height=.*/window_height=720/; s/^centered_window=.*/centered_window=false/; s/^fxaa=.*/fxaa=false/; s/^bloom=.*/bloom=false/; s/^sharpen=.*/sharpen=false/; s/^use_compute_shader=.*/use_compute_shader=false/' "$GAME/d2gl.ini"
  INI_SUM="$(sha256sum "$GAME/d2gl.ini" | cut -d' ' -f1)"
  python3 -I "$ROOT/tools/ae_install.py" install --game-dir "$GAME" --build-dir "$BUILD" --d2gl-dir "$D2GL" >/dev/null \
    && pass "instalador com --d2gl-dir" || fail "instalador com --d2gl-dir"
  [[ "$(sha256sum "$GAME/glide3x.dll" | cut -d' ' -f1)" == "$(sha256sum "$D2GL/glide3x.dll" | cut -d' ' -f1)" ]] \
    && pass "glide3x.dll do D2GL instalado" || fail "glide3x.dll do D2GL nao instalado"
  [[ "$(sha256sum "$GAME/glide3x.dll.aeonian-orig" 2>/dev/null | cut -d' ' -f1)" == "$GLIDE_ORIG" ]] \
    && pass "glide3x.dll preexistente guardado como .aeonian-orig" || fail "glide3x.dll preexistente nao foi guardado"
  [[ "$(sha256sum "$GAME/d2gl.ini" | cut -d' ' -f1)" == "$INI_SUM" ]] \
    && pass "d2gl.ini do usuario nao foi sobrescrito" || fail "d2gl.ini do usuario foi sobrescrito"
  [[ -e "$GAME/d2gl.mpq" ]] && pass "d2gl.mpq instalado" || fail "d2gl.mpq ausente"

  reg_render 3   # D2GL so apresenta bem com Render != 1 (GDI); ver docs/modding/D2GL.md
  write_config '[core]\nlog_level = debug\nextra_args = -w -ns\n[video]\nbackend = auto\n[game]\nskip_intro = true\n'
  clear_shots
  run_game "$((SECS + 6))" AEONIAN_HUD_OPEN=1
  expect "backend D2GL via glide3x.dll"         'video: backend=auto \(D2GL/OpenGL via glide3x\.dll \(-3dfx\)\)'
  expect "-3dfx acrescentado"                   'linha de comando: .*-3dfx'
  forbid "-w removido (incompativel com o D2GL)" 'linha de comando: .* -w( |$)'
  expect "D2GL detectado ao carregar glide3x.dll" 'D2GL detectado \(glide3x\.dll\)'
  expect "overlay OpenGL pronto (shaders compilaram)" 'OpenGL: overlay pronto'
  expect "canvas dos plugins sobre o framebuffer GL" 'OpenGL: janela [0-9]+x[0-9]+, canvas dos plugins [0-9]+x[0-9]+'
  expect "on_present chamado no modo D2GL"      'SELFTEST on_present quadro 30 \([0-9]+x[0-9]+\) PASS'
  expect "screenshot do framebuffer GL gravado" 'screenshot gravado: .*shot_menu\.bmp \([0-9]+x[0-9]+, OpenGL\)'
  forbid "sem excecoes de 1a chance (D2GL)"     'excecao de 1a chance'
  forbid "sem erros de shader/OpenGL"            'OpenGL: (shader|link)'
  check "framebuffer GL gravado (>= 640x480)"   python3 -I "$HERE/bmp_check.py" minsize "$MODS/selftest/shot_menu.bmp" 640 480
  check "overlay composto no framebuffer do D2GL" python3 -I "$HERE/bmp_check.py" red "$MODS/selftest/shot_menu.bmp" 100 35

  # A abertura da Blizzard so toca com Render=1 + -3dfx (o que o usuario relatou:
  # video pequeno no canto da janela grande). Com skip_intro ela deve ser pulada.
  echo "== fase B2: D2GL com Render=1 (a combinacao que toca a abertura): skip_intro"
  reg_render 1
  run_game 12
  expect "videos de abertura pulados (BinkOpen por handle)" 'video Bink: .* -> PULADO'
  forbid "nenhum video de abertura chegou a tocar nos primeiros 30 s" 'video Bink: [^>]*flags=0x[0-9a-f]+$'
  write_config '[core]\nlog_level = debug\nextra_args = -w -ns\n[video]\nbackend = auto\n[game]\nskip_intro = false\n'
  run_game 12
  expect "com skip_intro=false a abertura NAO e pulada" 'video Bink: .*flags=0x[0-9a-f]+$'
  forbid "com skip_intro=false nada e pulado"           'video Bink: .* -> PULADO'
  reg_render 1
else
  echo "  SKIP  fase D2GL (defina AEONIAN_D2GL_DIR com glide3x.dll + d2gl.mpq)"
fi

# ------------------------------------------------------- fase C (desativado)
echo "== fase C: AEONIAN_DISABLE=1 (jogo vanilla, motor inerte)"
reg_render 1
touch -d '2001-01-01' "$GAME/aeonian/aeonian.log"
run_game 10 AEONIAN_DISABLE=1
if [[ "$(stat -c %Y "$GAME/aeonian/aeonian.log")" -lt 1000000000 ]]; then pass "motor inerte; log nao foi tocado"; else fail "o motor escreveu no log mesmo desativado"; fi

# ------------------------------------------------ fase D (build desconhecida)
echo "== fase D: build desconhecida (modo seguro)"
sed -i 's/^timestamp *=.*/timestamp     = 0x11111111/' "$GAME/aeonian/addresses/1.14d.ini"
write_config '[core]\nextra_args = -w -ns\n'
run_game 10
expect "recusa a build desconhecida"            'build NAO reconhecida: Aeonian desativado'
forbid "nenhum hook instalado em build desconhecida" 'importacoes interceptadas'
forbid "plugins nao carregam em build desconhecida"  'plugin selftest\.dll carregado'

# ------------------------------------------------------------ desinstalacao
echo "== desinstalando"
[[ -f "$GAME/d2gl.ini" ]] && INI_BEFORE_UNINSTALL="$(sha256sum "$GAME/d2gl.ini" | cut -d' ' -f1)"   # o D2GL reescreve o proprio ini ao rodar
python3 -I "$ROOT/tools/ae_install.py" uninstall --game-dir "$GAME" >/dev/null \
  && pass "desinstalador" || fail "desinstalador"
[[ "$(sha256sum "$GAME/ijl11.dll" | cut -d' ' -f1)" == "$ORIG_IJL" ]] \
  && pass "ijl11.dll original restaurado byte a byte" || fail "ijl11.dll nao restaurado"
[[ ! -e "$GAME/ijl11_real.dll" ]] && pass "ijl11_real.dll removido" || fail "ijl11_real.dll sobrou"
if [[ -n "${GLIDE_ORIG:-}" ]]; then
  [[ "$(sha256sum "$GAME/glide3x.dll" 2>/dev/null | cut -d' ' -f1)" == "$GLIDE_ORIG" ]] \
    && pass "glide3x.dll preexistente restaurado byte a byte" || fail "glide3x.dll preexistente nao restaurado"
  [[ ! -e "$GAME/glide3x.dll.aeonian-orig" ]] && pass "backup .aeonian-orig removido" || fail "backup .aeonian-orig sobrou"
  [[ ! -e "$GAME/d2gl.mpq" ]] && pass "d2gl.mpq removido" || fail "d2gl.mpq sobrou"
  [[ -e "$GAME/d2gl.ini" ]] && pass "d2gl.ini (configuracao do usuario) nao foi apagado" || fail "d2gl.ini foi apagado"
  [[ "$(sha256sum "$GAME/d2gl.ini" | cut -d' ' -f1)" == "${INI_BEFORE_UNINSTALL:-x}" ]] \
    && pass "d2gl.ini do usuario preservado pelo desinstalador" || fail "o desinstalador alterou o d2gl.ini"
fi

echo
if [[ $FAILS -eq 0 ]]; then echo "RESULTADO: tudo passou"; else echo "RESULTADO: $FAILS falha(s). Log: $GAME/aeonian/aeonian.log"; fi
[[ -n "${AEONIAN_WORK_DIR:-}" ]] || echo "(pasta de trabalho: $WORK)"
exit $(( FAILS == 0 ? 0 : 1 ))
