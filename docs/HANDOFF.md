# HANDOFF — estado do projeto para continuidade

> **Primeiro documento a ler ao retomar.** Atualizado em 2026-10-31.
> A fase de reimplementação foi descontinuada e **apagada** (código, tags e
> documentação). Não a retome.

## Comece aqui

**O projeto:** um devkit que roda **dentro** do `Game.exe` 1.14d original, para quem quiser
mudar HUD, classes, habilidades, sprites e mapas. É o que o Median XL fez com engenharia
reversa, só que aberto e para o jogo vanilla. Não é um jogo novo.

**Situação em 2026-10-31:** o trabalho está na `main`, publicado até `dc50244`. Nada ficou pela
metade e nenhuma sessão está rodando. O primeiro mod de teste (`map-overlay`, C1/C2) está
pronto nos Atos I a V, com as ressalvas de [`modding/MAP_OVERLAY.md`](modding/MAP_OVERLAY.md).

Confira antes de confiar neste texto:

```bash
git status -sb && git branch -a && git log --oneline -5
```

### Regras que já custaram caro ao dono

1. **Não reescreva o jogo nem copie assets.** Duas IAs fizeram isso e o dono perdeu tempo e
   dinheiro. Se a tarefa parecer pedir isso, pare e pergunte.
2. **Uma fase por sessão.** Declare `[ANÁLISE]` ou `[IMPLEMENTAÇÃO]` no início
   ([`CLEANROOM_PROTOCOL.md`](CLEANROOM_PROTOCOL.md)). Na análise não há código em `aeonian/`,
   exceto o banco de endereços. Na implementação não se abre o `Game.exe`, nem as transcrições
   ou evidências da análise. Se faltar um fato, deixe `TODO(RE)` e peça uma nova análise, em
   **outra** sessão.
3. **O jogo roda invisível e mudo.** Nenhuma janela no monitor e nenhum som: o dono usa a mesma
   máquina. Veja "Rodar o jogo fora da suíte", abaixo.
4. **Commit e push só quando o dono pedir.** Ele trabalha em lotes grandes, com testes no fim,
   e responde "Commitar" quando aprova. Responda em português.
5. **Separe o que foi validado no jogo do que só compilou**, sem arredondar para "funciona".

### O que fazer agora

Se o dono não disser o que quer, pergunte; esta é a ordem recomendada.

| # | Tarefa | Fase | Comece por | Pronto quando |
|---|---|---|---|---|
| 1 | **Personagem** (M2.2): atributos, pontos livres, habilidades e criação | Análise | criar `docs/specs/game-exe-1.14d-character.md` | spec e símbolos com bytes conferidos, confirmados em execução |
| 2 | **Planejador de builds** (C3) | Implementação, depois da 1 | linha C3 do [`ROADMAP.md`](ROADMAP.md) | aplica um plano a um personagem novo, no jogo real, sem editar saves |
| 3 | **Sobras do mapa**, a começar pelo estado das missões no cliente | Análise, depois implementação | "`TODO(RE)`" em [`modding/MAP_OVERLAY.md`](modding/MAP_OVERLAY.md) | o mod diz o que está pendente, não só o que foi usado |
| 4 | **HUD e classes** (M3, M4): o objetivo maior do devkit | Análise por painel ou sistema | [`ROADMAP.md`](ROADMAP.md) | spec e teste de integração para cada ponto tocado |

**Dependem do dono, não comece sozinho:** a estratégia do C4 (Project Diablo 2 e Median XL, base
1.13c, é o maior risco do roadmap) e o teste em hardware dele ([`FINAL_TEST.md`](FINAL_TEST.md)).

### Como uma rodada costuma andar

1. Sessão de análise: escreve as specs e o INI, sem commit.
2. O dono aprova e o commit entra (`docs(specs): ...`).
3. **Sessão nova** de implementação, só com as specs e o INI. Testes no fim do lote.
4. Atualize `MAP_OVERLAY.md` (ou o documento do mod), `ROADMAP.md` e este arquivo. O dono
   aprova e o commit entra (`feat(...): ...`).

Quem coordena sessões e já escreveu código em `aeonian/` não examina o binário: delega a análise.

### Onde está cada coisa

| Procurando | Veja |
|---|---|
| Escopo, arquitetura, protocolo | [`SCOPE.md`](SCOPE.md), [`ARCHITECTURE.md`](ARCHITECTURE.md), [`CLEANROOM_PROTOCOL.md`](CLEANROOM_PROTOCOL.md) |
| Fatos do `Game.exe` | `docs/specs/game-exe-1.14d-*.md` e `aeonian/addresses/1.14d.ini` |
| API de plugins | [`modding/PLUGIN_API.md`](modding/PLUGIN_API.md), `aeonian/include/aeonian/api.h` |
| Mod de exemplo mais completo | `aeonian/examples/map-overlay/` |
| Plugin que dirige o jogo nos testes | `aeonian/tests/integration/mapdrive-plugin/` (comandos no cabeçalho de `mapdrive.cpp`) |
| Modelos antigos de prompt | `ai/`: se divergirem, valem `AGENTS.md` e este arquivo |

As evidências das rodadas ficaram em `/tmp` (tmpfs) e somem ao reiniciar. O que vale fica na
matriz de validação do `MAP_OVERLAY.md`.

## O que o projeto é agora

Ponto de entrada de mods para o **Game.exe 1.14d original** (não é
reimplementação). Leia [`SCOPE.md`](SCOPE.md), depois
[`ARCHITECTURE.md`](ARCHITECTURE.md).

## Onde está o trabalho (git)

- Repositório: `/mnt/STEAM/Projetos/Aeonian-OpenD2LOD` (host atual, CachyOS); remoto `origin` =
  `https://github.com/aeon1911/Aeonian-OpenD2LOD.git`.
- Todo o trabalho está na **`main`**, a única branch, local e no GitHub.
- O histórico foi **reescrito em 2026-10-30**, por ordem do dono, para apagar a
  reimplementação: o código, as tags e as branches dela saíram. O commit-raiz
  reúne a base do ponto de entrada; hashes anteriores a essa data não valem mais.

## Estado

| Área | Estado | Evidência |
|---|---|---|
| Proxy `ijl11.dll` + `aeonian.dll` | ✅ | suíte de integração no jogo real |
| Instalador/desinstalador não destrutivo | ✅ | idem (restaura `ijl11.dll` byte a byte; `Game.exe` intocado) |
| VFS de mods (arquivos de `data\`) | ✅ | log + trace de arquivos do Wine |
| Plugins + eventos + hook inline | ✅ | selftest dentro do `Game.exe` |
| Falha segura (build desconhecida, `AEONIAN_DISABLE`) | ✅ | fases B/C da suíte |
| Símbolos de código do jogo | ✅ 79 (entrada + estado do jogo: unidades, níveis, saídas, automap, desenho, painéis), com specs; 19 deles usados e verificados em jogo pelo `map-overlay` | `addresses/1.14d.ini`; suíte, fases A3 e E |
| `on_present` (overlay) GDI + D2GL/OpenGL, `request_screenshot`, mods API | ✅ | suíte: quadro do jogo gravado e comparado |
| Integração do D2GL (instalador, `-3dfx`, `-w`, skip_intro) | ✅ mecânica e imagem completa com o **release oficial** v1.3.3 (Wine 11.20, RX 9070 XT, 2026-10-29); o instalador avisa se o D2GL não for oficial | [`modding/D2GL.md`](modding/D2GL.md) |
| Mod de arte visível (botões recoloridos via VFS) | ✅ | diff de 11 mil pixels no menu |
| Modo DirectDraw (`on_present`) | ⬜ código sem teste | o Wine não o ativou |
| Mod de mapa C1/C2 (`map-overlay`: monstros, aliados, amigos, waypoints, portais, objetivos de quest e saídas sobre o automap) | 🟡 Atos I–V, um jogador; GDI em Normal, Pesadelo e Inferno; D2GL em Normal (janela 800×600 e 1280×720); painéis; 640×480 em GDI. Objetivos pelo `OperateFn` (7 dos 24 valores vistos em jogo com o mod, Anya não alcançada); filtro de alinhamento; passagem exata pela colisão e pela regra do Ato I, aproximada e rotulada no resto; `reveal_exits` experimental e desligado; D2GL em tela cheia **não visto** | [`modding/MAP_OVERLAY.md`](modding/MAP_OVERLAY.md); suíte, fase E; quadros gravados |
| Estado do jogo: personagem; requisitos C3/C4 do client | ⬜ | M2.2 e "Requisitos do client" no [`ROADMAP.md`](ROADMAP.md) |
| Windows nativo, Switch/Wine-NX | ⬜ **não testado** | — |
| Medição de FPS em monitores de alta taxa/4K; outros monitores e GPUs | ⬜ **não medido** (a imagem em partida foi vista com o D2GL oficial; ver D2GL.md) | — |
| Instalação **oficial** da loja (1.14d completa) | ⬜ **não testado**: a suíte rodou na instalação reconstruída (MPQs 1.14b + `Game.exe` 1.14d) | — |

## Como construir e testar

Toolchain: a **única validada** é um `llvm-mingw` (release `20261020`, variante
`msvcrt`, extraído fora do repo) apontado por `AEONIAN_MINGW_ROOT`. O
`g++-mingw-w64-i686` do sistema deve servir, mas **não foi testado**.

Comandos prontos para **esta máquina**, a partir da raiz do repositório, em `bash`:

```bash
# núcleo portável + testes unitários (host): core e map_overlay. AEONIAN_BUILD_TOOLS gera o
# ae_mpq_extract usado pela suíte; o build/host desta máquina já o tem (na primeira vez, baixa o StormLib)
cmake -S . -B build/host -G Ninja -DAEONIAN_BUILD_TOOLS=ON && cmake --build build/host && ctest --test-dir build/host

# DLLs que rodam no jogo (i686): 7 DLLs em build/win32
export AEONIAN_MINGW_ROOT=/mnt/STEAM/Projetos/devkit-tools/llvm-mingw
cmake -S aeonian -B build/win32 -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/toolchain-i686-mingw.cmake
cmake --build build/win32

# integração: roda o Game.exe 1.14d REAL no Wine numa cópia-espelho, invisível e mudo.
# A pasta de trabalho fica no disco: em /tmp a suíte já encheu o tmpfs. Apague-a ao terminar.
AEONIAN_HIDDEN=1 \
AEONIAN_WORK_DIR=/mnt/STEAM/Projetos/aeonian-tmp-suite \
AEONIAN_D2_DIR=/mnt/STEAM/Projetos/d2-114d \
AEONIAN_EXTRA_DLL_DIR="/mnt/STEAM/Diablo II LoD 1.13c -D2SE" \
AEONIAN_MPQ_EXTRACT=$PWD/build/host/tools/mpq_extractor/ae_mpq_extract \
AEONIAN_D2GL_DIR=/mnt/STEAM/Projetos/devkit-tools/d2gl-1.3.3 \
AEONIAN_D2_LNG="/mnt/STEAM/Diablo II LoD 1.13c -D2SE/D2.LNG" \
bash aeonian/tests/integration/run_wine_smoke.sh
```

O que cada variável faz: `AEONIAN_EXTRA_DLL_DIR` traz `binkw32`, `SmackW32` e `ijl11`, se a instalação
não os tiver; `AEONIAN_MPQ_EXTRACT` habilita a fase de arte/VFS; `AEONIAN_D2GL_DIR` habilita a fase
D2GL; `AEONIAN_D2_LNG` habilita a fase E (partida real) e é copiado só para o espelho.

Em outra máquina, troque os caminhos. O `llvm-mingw` é a única toolchain validada.

### Rodar o jogo fora da suíte

```bash
gamescope --backend headless -W 1280 -H 720 -- env -u WAYLAND_DISPLAY \
  WINEPREFIX=/mnt/STEAM/Projetos/aeonian-tmp-<algo>/prefix \
  WINEDLLOVERRIDES="winemenubuilder.exe=d;winepulse.drv,winealsa.drv,wineoss.drv=d" \
  wine Game.exe
```

- O prefixo de teste precisa de `HKCU\Software\Wine\Drivers\Audio=""`. Nunca defina
  `WINEDLLOVERRIDES` sem manter os drivers de áudio desligados: foi assim que o som vazou.
- Rode num espelho da instalação, nunca em `d2-114d`, e encerre com `wineserver -k`.
- Confirme o silêncio com `pactl list sink-inputs` durante a execução: zero fluxos do jogo.
- Dentro do gamescope não há gerenciador de janelas: ache a janela com `XQueryTree`.

A suíte exige display e leva ~3 min (~6 min com as fases E e E2). Com **`AEONIAN_HIDDEN=1`** ela roda dentro
de um `gamescope --backend headless`: **nenhuma janela aparece no monitor** e o
**som fica desligado** (drivers de áudio do Wine desativados). Use sempre esse
modo quando o dono estiver usando a máquina. Ela usa **um prefixo Wine de teste**
(nunca o `~/.wine`) e só lê os quadros que o próprio motor grava (BMP do buffer
do jogo): **não captura a tela**.

## Ambiente desta máquina (para reproduzir)

- Instalação 1.14d reconstruída: `/mnt/STEAM/Projetos/d2-114d` (sem
  `binkw32.dll`, `SmackW32.dll`, `ijl11.dll`: vieram da pasta da 1.13c
  `/mnt/STEAM/Diablo II LoD 1.13c -D2SE`, só para teste). Até 2026-10-28 o disco
  montava em `/media/aeon/STEAM`.
- Essa instalação é **reconstruída** (MPQs 1.14b + `Game.exe` 1.14d), não uma
  instalação oficial da loja. Sua pasta `data\` tem 1226 arquivos do overlay
  1.14d: 1016 com **cabeçalho BNUpdate de 24 bytes** e 210 **deltas binários
  não aplicados** (verificado em 2026-10-28). **Nunca** use essa `data\` com
  `-direct` (o jogo leria arquivos inválidos); a cópia-espelho do teste não a
  herda. Numa instalação oficial a pasta `data\` normalmente não existe.
- Wine 11 (`/usr/bin/wine`); a suíte cria o próprio prefixo de teste (nunca o `~/.wine`).
- Docker aqui não é utilizável sem root; por isso o llvm-mingw.
- **Ferramentas (host CachyOS, desde 2026-10-29)** em `/mnt/STEAM/Projetos/devkit-tools/`,
  baixadas dos releases oficiais com SHA-256 conferido: `llvm-mingw/` (20261020,
  `AEONIAN_MINGW_ROOT`), `ghidra/` (12.2) + `jdk/` (Temurin 21), `gh/` (GitHub CLI,
  já autenticado; o binário está em `gh/bin/`)
  e `d2gl-1.3.3/` (o **D2GL oficial**, use-o em `AEONIAN_D2GL_DIR`). O D2GL da
  pasta 1.13c/D2SE **não** é o oficial e deixa o jogo preto no Wine.
- **O Ghidra 12.2 precisa de Java 25 e o `jdk/` é 21**: ele não abre com o que está lá. As
  análises até aqui usaram `objdump`, scripts e execução no jogo. Para usar o Ghidra, peça
  autorização e baixe você mesmo um JDK 25 do release oficial: o dono prefere não fazer isso.
- Neste host: `cmake -S aeonian -B build/win32 ...` compila sem avisos e a suíte
  dá **126/126** com o D2GL oficial, `AEONIAN_MPQ_EXTRACT`, `AEONIAN_D2_LNG` e
  `AEONIAN_HIDDEN=1`, em 2026-10-31 (sem as fases E e E2, que precisam do `D2.LNG`,
  ela não entra em partida). A suíte ocupa alguns GB na pasta de trabalho: em `/tmp` (tmpfs) ela já
  encheu o disco e terminou com `exit=77`. A instalação
  reconstruída não tem `D2.LNG`; o da pasta 1.13c
  (`/mnt/STEAM/Diablo II LoD 1.13c -D2SE/D2.LNG`) serve para a cópia-espelho.

## Notas de ambiente (Wine) descobertas

- **Partida no espelho:** sem `D2.LNG` o jogo não entra em partida. O condutor de
  teste (`tests/integration/mapdrive-plugin/`) cria o personagem por cliques
  postados na janela; a opção *Automap Size* **persiste** no prefixo entre execuções
  (por isso o comando `ensuremode`). Fora da cidade do Ato I o mapa muda a cada
  partida; andar até a Charneca é heurístico (roteiro `charneca.txt`, manual).
- **Validação do `map-overlay` com o Teletransporte:** depois de uma varredura longa, voltar ao
  waypoint costuma falhar. Use um roteiro curto por alvo. Clique esquerdo sobre monstro ataca em vez de
  andar: para testar uma passagem a pé, limpe o caminho antes com Hidra. Os comandos novos do condutor
  (`align`, `rulecheck`, `nearpass`, `crossoff`, `walkabs`) estão no cabeçalho de `mapdrive.cpp`.
- **Outros atos no espelho:** `mapdrive-plugin/test_save.py` leva o personagem de
  teste a um ato e dificuldade, com todos os waypoints, Teletransporte, Hidra e
  mercenário (só dentro de `--test-root`, nunca nos saves de verdade). O condutor viaja
  pelo painel de waypoint: abas em x = 115, 175, 235, 295 e 355, y = 77; linhas em
  x = 215, y = 138 + 35·linha. Sob o D2GL, o clique num NPC não abriu o diálogo e, com a
  janela de 1280×720, o clique no waypoint também não; a causa não foi investigada. Com a
  janela de 800×600 a viagem por waypoint funcionou.
- **Ganchos e `resolve_symbol`:** depois que um mod desvia uma função, os bytes
  dela mudam. Até 2026-10-29 quem resolvia aquele símbolo depois recebia 0 (visto
  com `D2Client.DrawAutomap`). Agora o motor guarda os bytes originais de tudo o
  que `hook_inline`/`patch_bytes` escrevem (`core/patch_registry.h`) e
  `resolve_symbol` confere o banco contra eles; `read_original` (API) devolve
  esses bytes para âncoras de dados. Segundo hook no mesmo endereço **encadeia**;
  outra sobreposição dá `AEONIAN_ERR_CONFLICT` (ver
  [`modding/PLUGIN_API.md`](modding/PLUGIN_API.md)). Provas: `core_test`
  (registro) e o selftest na suíte (reresolver `Game.EntryPoint` depois do hook;
  `DrawAutomap` resolvido pelo selftest com o `map-overlay` carregado). O
  `map-overlay` e o `mapdrive` leem as âncoras e o `esperado` com `read_original`
  (`OriginalMemory`, em `examples/map-overlay/game_memory.h`); a suíte cobre outro
  mod desviar `DrawAutomap` antes deles (fases A3 e E2).

- O D2 grava `Render` em `HKCU\Software\Blizzard Entertainment\Diablo II\VideoConfig`
  na 1ª execução. `Render=1` = GDI. O D2GL só se comporta bem com `Render` ≠ 1
  (ver D2GL.md); a suíte troca o valor por fase.
- O wrapper `ddraw.dll` do D2GL **trava** no Wine 11 (também sem o Aeonian); só o
  Glide foi usado. DDraw exigiria `WINEDLLOVERRIDES=ddraw=n,b`.
- Runtime VC++ nativo (`msvcp140`...) **não** foi necessário para o D2GL carregar; copiá-lo
  não mudou a imagem.
- Para inspecionar a janela do jogo **por fora**, capture só o id dela (XGetImage no id
  de `xwininfo`): o XWayland não enxerga clientes Wayland, então nada mais vaza.

## Lições (não repetir)

1. **Nunca reconstruir o jogo** (ver `AGENTS.md`). O erro da fase anterior foi
   esse.
2. **Prove com evidência independente.** A prova do VFS foi o *trace de arquivos
   do próprio Wine* (`WINEDEBUG=+file`) mostrando o SO abrindo o arquivo do
   mod, não só o log do Aeonian.
3. **Testes não podem depender de input humano.** O primeiro teste de
   `on_window_message` contava mensagens (dependia do mouse) e falhou; foi
   trocado por uma mensagem própria enviada à janela.
4. **Não capturar a tela inteira** para "ver" o jogo: expõe o que mais estiver
   aberto. Use log/trace; para inspeção visual peça ao usuário ou capture só a
   janela do jogo.
5. **Declare o que NÃO foi validado** (ver tabela acima).
6. **Controle antes de culpar o código.** O quadro preto no D2GL parecia bug do
   overlay; rodar o D2GL **sem** o Aeonian mostrou o mesmo. E uma "falha" de teste
   (`on_window_message`) era o teste dependendo de input humano.
7. **Não confie em `GetObject` para a orientação de um DIB**: registre o que o jogo
   passou ao `CreateDIBSection`.

## Retomando em outra máquina ou distro

1. **Pacotes** (nomes de Debian/Ubuntu; versões usadas: Wine 11.0, CMake 3.28,
   Ninja 1.11, g++ 13, Python 3.12): `wine`, `cmake`, `ninja-build`, `g++`, `python3`
   e, opcionalmente, `x11-utils` (`xwininfo`, só para inspecionar a janela do jogo).
2. **Toolchain i686**: obtenha o `llvm-mingw` (variante `msvcrt`, alvo i686) na página
   de releases do `mstorsjo/llvm-mingw` e exporte `AEONIAN_MINGW_ROOT`. O dono quer que a
   IA baixe as ferramentas ("abra você o navegador e baixe você"), mas **peça antes de
   baixar qualquer coisa**.
3. **Mudou o ponto de montagem do disco `STEAM`?** Atualize os caminhos dos comandos
   deste arquivo. Não há worktrees: só o checkout principal, na `main`.
4. **Arquivos do jogo** (nunca versionados), em `/mnt/STEAM/Projetos/` no host
   atual (antes `/media/aeon/STEAM/Projetos/`): `d2-114d` (1.14d reconstruída; o
   Aeonian **nunca** foi instalado nela) e `d2-114b`. A cópia de tabelas
   `d2data-extracted` foi **apagada** em 2026-10-29 por ordem do dono. D2GL oficial
   v1.3.3 em `devkit-tools/d2gl-1.3.3`. Projeto-irmão do Switch:
   `DIABLO 2 LOD SWITCH PORT` e `medianxl-switch`, na mesma pasta.
5. **Recompile e rode a suíte** (seção "Como construir e testar"). Referência:
   126 PASS / 0 FAIL com o `map-overlay` e as fases E e E2 (2026-10-31), Wine 11
   em Linux.
6. `build/` e `dist/` não são versionados. Regenere-os: CMake e
   `python3 tools/package.py`.
7. O histórico da conversa e as memórias do Claude ficam em
   `~/.claude/projects/-mnt-STEAM-Projetos-Aeonian-OpenD2LOD*`. Outras IAs não os veem
   e a `/home` pode ser apagada: o essencial está em "Comece aqui" e na seção seguinte.
8. **Backup do histórico antigo:** `/mnt/STEAM/Projetos/aeonian-historico-antes-da-reescrita.bundle`
   guarda o que foi reescrito em 2026-10-30, incluindo a reimplementação. Não restaure nada dele. Apagá-lo é
   decisão do dono.

## Preferências e regras do dono (para qualquer agente)

- Responder em **português**.
- Engenharia reversa do `Game.exe` **autorizada** pelo dono, sempre dentro do
  protocolo cleanroom (análise → specs e endereços; implementação só a partir
  das specs, em sessões separadas).
- **Pedir antes de baixar** qualquer coisa; as ferramentas de `devkit-tools/` já foram
  baixadas com a autorização dele. **Não instalar o Aeonian** na
  instalação real (`d2-114d`) sem pedir; testar na cópia-espelho da suíte.
- **O jogo roda invisível e mudo**, sempre: ver "Rodar o jogo fora da suíte".
- O dono entra no GitHub pelo Google e não tem senha: o `gh` da pasta `devkit-tools/` já está
  autenticado (`aeon1911`). Nunca peça nem digite credenciais.
- **Nunca capturar a tela inteira** (já expôs conversas privadas uma vez). Use o log
  do motor, o trace do Wine ou o BMP que o próprio motor grava.
- **Separar sempre "validado no jogo real" de "não validado"**. O dono já foi
  prejudicado por agentes que declararam "100% validado" sem cobrir o cenário real.
- Commit e push **só quando o dono pedir**. Não afirmar o que a Blizzard permite:
  a documentação diz que não é aconselhamento jurídico.
- Requisitos obrigatórios do client (D2GL com alto desempenho; mapa, planejador de
  builds, compatibilidade com Project Diablo 2 e Median XL): ver o
  [`ROADMAP.md`](ROADMAP.md). **Sinalize o risco do C4 antes de prometer.**

## Próximos passos (ordem recomendada)

0. **Decisão do dono ainda aberta**: estratégia do C4 (base 1.13c do PD2 e do
   Median XL).
1. **Teste final do usuário** (roteiro em [`FINAL_TEST.md`](FINAL_TEST.md)): conferir a
   imagem sob o D2GL no hardware dele e medir o desempenho.
2. **M2.2 — estado do jogo**: unidades e nível/mapa têm spec e símbolos; falta o
   personagem (atributos, habilidades, criação) para o C3. Nova análise pedida pelo
   `map-overlay` (lista em [`modding/MAP_OVERLAY.md`](modding/MAP_OVERLAY.md)): estado das
   missões no cliente, classe 534, passagem exata fora do Ato I, modo de jogo, encaixe do D2GL.
3. **Requisitos do client**: C1/C2 feitos nos Atos I–V (`map-overlay`); planejador de
   builds (C3); compatibilidade com Project Diablo 2 / Median XL (C4, base 1.13c) —
   ver o [`ROADMAP.md`](ROADMAP.md).
4. Validar em Windows nativo e planejar o teste em Wine-NX.
