# Aeonian-OpenD2LOD

**Um ponto de entrada de mods, open source (GPLv3), para o Diablo II: Lord of
Destruction 1.14d _original_.**

O Aeonian **não reimplementa** o jogo. Você continua rodando o `Game.exe`
da Blizzard que **você** comprou; o Aeonian é um conjunto de DLLs que roda
dentro desse processo e abre o jogo a quem quiser modificá-lo: **arte e
dados** (UI, HUD, fontes, paletas, tabelas, strings, sons), **plugins nativos**
com eventos do jogo, e **hooks no código** por endereço verificado — sem
alterar o `Game.exe` no disco.

> Escopo, limites e o que **não** é o projeto: [`docs/SCOPE.md`](docs/SCOPE.md).

## O que já funciona (validado no `Game.exe` 1.14d real, Wine 11/Linux)

- **Instalação não destrutiva** (`tools/ae_install.py`): confere o SHA-256 do
  `Game.exe`, usa um proxy do `ijl11.dll` como ponto de entrada e restaura o
  original byte a byte ao desinstalar.
- **Mods de arquivos:** `mods/<id>/data/...` substitui qualquer arquivo de
  `data\` (prioridades entre mods, `-direct` automático, índice em memória).
- **Plugins nativos** (API C estável, [`docs/modding/PLUGIN_API.md`](docs/modding/PLUGIN_API.md)):
  eventos `on_loaded`, `on_tick`, `on_window_message` (pode consumir entrada),
  `on_shutdown`; log; hooks inline e patch de bytes **só com bytes conferidos**.
- **Desenho sobre o jogo** (`on_present`, API de HUD com GDI) nos renderizadores
  original (GDI) e **D2GL/OpenGL**, mais `request_screenshot` do quadro do jogo.
- **D2GL integrado** ([`docs/modding/D2GL.md`](docs/modding/D2GL.md)): instalação
  (`--d2gl-dir`), `-3dfx` automático, `-w` removido, vídeos de abertura pulados.
- **Falha segura:** build desconhecida ⇒ motor inerte; `AEONIAN_DISABLE=1`
  roda o jogo vanilla.

**Ainda não:** leitura do estado do jogo (unidades, mapa, personagem: M2.2),
os **requisitos do client** (mapa com entradas/saídas e inimigos, planejador de
builds, compatibilidade com Project Diablo 2/Median XL), novas classes (M4) e
testes em Windows nativo/Switch (M5). Veja o
[`ROADMAP`](docs/ROADMAP.md) e a lista do que **não** foi validado em
[`docs/HANDOFF.md`](docs/HANDOFF.md).

## Como usar

Pré-requisitos: **você** tem o Diablo II + LoD **1.14d** instalado (a pasta com
`Game.exe`), `python3`, `cmake` e um compilador mingw-w64 i686
(`g++-mingw-w64-i686`, ou um [llvm-mingw](https://github.com/mstorsjo/llvm-mingw)).

```bash
# 1) compilar as DLLs que rodam dentro do jogo
cmake -S aeonian -B build/win32 -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/toolchain-i686-mingw.cmake
cmake --build build/win32
# (com llvm-mingw: export AEONIAN_MINGW_ROOT=/caminho/do/llvm-mingw antes)

# 2) instalar na SUA instalação (reversível)
python3 tools/ae_install.py install --game-dir "/caminho/Diablo II" --build-dir build/win32

# 3) criar um mod: <jogo>/aeonian/mods/meu-mod/{mod.ini, data/...}   (docs/modding/MODS.md)
# 4) jogar normalmente. O log fica em <jogo>/aeonian/aeonian.log

python3 tools/ae_install.py uninstall --game-dir "/caminho/Diablo II"   # volta ao original
```

Guias: [`docs/modding/MODS.md`](docs/modding/MODS.md) (mods e configuração) ·
[`docs/modding/PLUGIN_API.md`](docs/modding/PLUGIN_API.md) (plugins) ·
exemplo em [`aeonian/examples/hello-plugin/`](aeonian/examples/hello-plugin/).

## Desenvolvimento e testes

```bash
cmake -S . -B build/host -G Ninja && cmake --build build/host && ctest --test-dir build/host   # núcleo (host)
AEONIAN_D2_DIR=... bash aeonian/tests/integration/run_wine_smoke.sh                            # jogo real (Wine)
```

Arquitetura: [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md). Passo a passo de
build/ambiente: [`docs/HANDOFF.md`](docs/HANDOFF.md).

## Modelo de distribuição

O repositório contém **apenas código próprio, specs e ferramentas**. `Game.exe`,
DLLs, MPQs e quaisquer assets da Blizzard **nunca** são versionados nem
redistribuídos; cada pessoa precisa da própria cópia legítima do jogo e instala
o Aeonian nela. Veja [`NOTICE.md`](NOTICE.md) e a nota legal em
[`docs/SCOPE.md`](docs/SCOPE.md) (não é aconselhamento jurídico).

## Engenharia reversa (cleanroom)

Para abrir pontos de extensão no `Game.exe` o projeto usa engenharia reversa
com um protocolo de duas fases (análise → specs/endereços; implementação só a
partir das specs). Leia [`docs/CLEANROOM_PROTOCOL.md`](docs/CLEANROOM_PROTOCOL.md)
e [`CONTRIBUTING.md`](CONTRIBUTING.md) **antes de contribuir**.

## Histórico

Uma primeira tentativa reimplementava o motor do zero (itens, combate, HUD
próprios). Foi **descontinuada** por não ser fiel ao jogo real e **apagada**
do repositório; não será retomada.

## Licença

GNU General Public License v3.0 (GPLv3). Nenhum código proprietário da
Blizzard é usado.
