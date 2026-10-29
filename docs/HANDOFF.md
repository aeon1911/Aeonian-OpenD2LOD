# HANDOFF — estado do projeto para continuidade

> **Primeiro documento a ler ao retomar.** Atualizado em 2026-10-29.
> A fase de reimplementação foi descontinuada e **apagada** (código, tags e
> documentação). Não a retome.

## O que o projeto é agora

Ponto de entrada de mods para o **Game.exe 1.14d original** (não é
reimplementação). Leia [`SCOPE.md`](SCOPE.md), depois
[`ARCHITECTURE.md`](ARCHITECTURE.md).

## Onde está o trabalho (git)

- Repositório: `/mnt/STEAM/Projetos/Aeonian-OpenD2LOD` (host atual, CachyOS); remoto `origin` =
  `https://github.com/aeon1911/Aeonian-OpenD2LOD.git`.
- O trabalho do Aeonian está na branch **`claude/diablo2-lod-open-source-a87cb5`**
  (worktree em `.claude/worktrees/diablo2-lod-open-source-a87cb5`):
  `f54a1b9` (pivot para ponto de entrada de mods) → `3089e7c` (`on_present`, D2GL,
  `skip_intro`, pacote de teste) → o commit de handoff que vem depois.
- A `main` **local** foi posta no mesmo commit desta branch (`e6e12f3`) em
  2026-10-29, por ordem do dono. A `origin/main` no GitHub ainda aponta para a
  reimplementação (`9686591`) até um push.
- As tags `v0.1.0-demo` … `v1.0.1-vendors` (código da reimplementação) foram
  apagadas localmente.

## Estado

| Área | Estado | Evidência |
|---|---|---|
| Proxy `ijl11.dll` + `aeonian.dll` | ✅ | suíte de integração no jogo real |
| Instalador/desinstalador não destrutivo | ✅ | idem (restaura `ijl11.dll` byte a byte; `Game.exe` intocado) |
| VFS de mods (arquivos de `data\`) | ✅ | log + trace de arquivos do Wine |
| Plugins + eventos + hook inline | ✅ | selftest dentro do `Game.exe` |
| Falha segura (build desconhecida, `AEONIAN_DISABLE`) | ✅ | fases B/C da suíte |
| Símbolos de código do jogo | 🟡 só `Game.EntryPoint` | `addresses/1.14d.ini` |
| `on_present` (overlay) GDI + D2GL/OpenGL, `request_screenshot`, mods API | ✅ | suíte: quadro do jogo gravado e comparado |
| Integração do D2GL (instalador, `-3dfx`, `-w`, skip_intro) | ✅ mecânica e imagem completa com o **release oficial** v1.3.3 (Wine 11.20, RX 9070 XT, 2026-10-29); o instalador avisa se o D2GL não for oficial | [`modding/D2GL.md`](modding/D2GL.md) |
| Mod de arte visível (botões recoloridos via VFS) | ✅ | diff de 11 mil pixels no menu |
| Modo DirectDraw (`on_present`) | ⬜ código sem teste | o Wine não o ativou |
| Estado do jogo (unidades/mapa/personagem) e requisitos do client | ⬜ | M2.2 e "Requisitos do client" no [`ROADMAP.md`](ROADMAP.md) |
| Windows nativo, Switch/Wine-NX | ⬜ **não testado** | — |
| Medição de FPS em monitores de alta taxa/4K; D2GL dentro de uma partida | ⬜ **não medido / não visto** (ver D2GL.md) | — |
| Instalação **oficial** da loja (1.14d completa) | ⬜ **não testado**: a suíte rodou na instalação reconstruída (MPQs 1.14b + `Game.exe` 1.14d) | — |

## Como construir e testar

Toolchain: a **única validada** é um `llvm-mingw` (release `20261020`, variante
`msvcrt`, extraído fora do repo) apontado por `AEONIAN_MINGW_ROOT`. O
`g++-mingw-w64-i686` do sistema deve servir, mas **não foi testado**.

```bash
# núcleo portável + testes unitários (host)
cmake -S . -B build/host -G Ninja && cmake --build build/host && ctest --test-dir build/host

# DLLs que rodam no jogo (i686)
export AEONIAN_MINGW_ROOT=/caminho/do/llvm-mingw      # ou deixe vazio e use o mingw do PATH
cmake -S aeonian -B build/win32 -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/toolchain-i686-mingw.cmake
cmake --build build/win32

# integração: roda o Game.exe 1.14d REAL no Wine numa cópia-espelho (não toca na sua instalação)
AEONIAN_D2_DIR=/caminho/d2-114d \
AEONIAN_EXTRA_DLL_DIR=/caminho/com/binkw32+SmackW32+ijl11 \   # só se a instalação não os tiver
AEONIAN_MPQ_EXTRACT=/caminho/ae_mpq_extract \                 # opcional: habilita a fase de arte/VFS
AEONIAN_D2GL_DIR=/pasta/com/glide3x.dll+d2gl.mpq \            # opcional: habilita a fase D2GL
bash aeonian/tests/integration/run_wine_smoke.sh
```

A suíte exige display e leva ~3 min. Com **`AEONIAN_HIDDEN=1`** ela roda dentro
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
  `AEONIAN_MINGW_ROOT`), `ghidra/` (12.2) + `jdk/` (Temurin 21), `gh/` (GitHub CLI)
  e `d2gl-1.3.3/` (o **D2GL oficial**, use-o em `AEONIAN_D2GL_DIR`). O D2GL da
  pasta 1.13c/D2SE **não** é o oficial e deixa o jogo preto no Wine.
- Neste host: `cmake -S aeonian -B build/win32 ...` compila sem avisos e a suíte
  dá **66/66** com o D2GL oficial (2026-10-29).

## Notas de ambiente (Wine) descobertas

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
   de releases do `mstorsjo/llvm-mingw` e exporte `AEONIAN_MINGW_ROOT`. O dono prefere
   baixar por conta própria; **peça antes de baixar qualquer coisa**.
3. **Mudou o ponto de montagem do disco `STEAM`?** O worktree guarda caminhos
   absolutos. No checkout principal, rode
   `git worktree repair <novo-caminho>/.claude/worktrees/diablo2-lod-open-source-a87cb5`.
4. **Arquivos do jogo** (nunca versionados), em `/mnt/STEAM/Projetos/` no host
   atual (antes `/media/aeon/STEAM/Projetos/`): `d2-114d` (1.14d reconstruída; o
   Aeonian **nunca** foi instalado nela) e `d2-114b`. A cópia de tabelas
   `d2data-extracted` foi **apagada** em 2026-10-29 por ordem do dono. D2GL v1.3.3 em
   `/media/aeon/STEAM/Diablo II LoD 1.13c -D2SE`. Projeto-irmão do Switch:
   `DIABLO 2 LOD SWITCH PORT` e `medianxl-switch`, na mesma pasta.
5. **Recompile e rode a suíte** (seção "Como construir e testar"). Referência:
   66 PASS / 0 FAIL no commit `3089e7c`, Wine 11 em Linux.
6. `build/` e `dist/` não são versionados. Regenere-os: CMake e
   `python3 tools/package.py`.
7. O histórico da conversa e as memórias do Claude ficam em
   `~/.claude/projects/-media-aeon-STEAM-Projetos-Aeonian-OpenD2LOD*`. Se a `/home`
   for apagada, o essencial está na seção seguinte.

## Preferências e regras do dono (para qualquer agente)

- Responder em **português**.
- Engenharia reversa do `Game.exe` **autorizada** pelo dono, sempre dentro do
  protocolo cleanroom (análise → specs e endereços; implementação só a partir
  das specs, em sessões separadas).
- **Pedir antes de baixar** qualquer coisa. **Não instalar o Aeonian** na
  instalação real (`d2-114d`) sem pedir; testar na cópia-espelho da suíte.
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

0. **Decisões do dono ainda abertas**: enviar (push) a branch ao `origin`; mesclar
   na `main`; estratégia do C4 (base 1.13c do PD2 e do Median XL).
1. **Teste final do usuário** (roteiro em [`FINAL_TEST.md`](FINAL_TEST.md)): conferir a
   imagem sob o D2GL no hardware dele e medir o desempenho.
2. **M2.2 — estado do jogo** (engenharia reversa: unidades, nível/mapa, personagem).
3. **Requisitos do client** (mapa com entrada/saída e inimigos; planejador de builds;
   compatibilidade com Project Diablo 2 / Median XL, que é base 1.13c) — ver o
   [`ROADMAP.md`](ROADMAP.md).
4. Validar em Windows nativo e planejar o teste em Wine-NX.
