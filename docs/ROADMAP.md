# Roadmap

Estado em 2026-10-29. Marcos definidos em [`SCOPE.md` §6](SCOPE.md).

## M1 — Ponto de entrada ✅

Feito e validado **no `Game.exe` 1.14d real sob Wine 11 (Linux)**:

- proxy `ijl11.dll` + `aeonian.dll`; instalação e desinstalação sem alterar o
  `Game.exe` (SHA-256 conferido; `ijl11.dll` original restaurado byte a byte);
- 7 hooks de IAT; VFS de mods com índice em memória; `-direct` automático;
- plugins com eventos (`on_loaded`, `on_tick`, `on_window_message`,
  `on_shutdown`) e API de log/símbolos/hook inline/patch;
- falha segura (build desconhecida; `AEONIAN_DISABLE=1`);
- testes: `aeonian/tests/core_test.cpp` (host) e
  `aeonian/tests/integration/run_wine_smoke.sh` (jogo real; 92 checagens com a
  partida da fase E, 78 sem ela).

**Não validado ainda:** Windows nativo; qualquer ambiente tipo Wine-NX/Switch;
instalação oficial da loja; `-txt`. (A troca de arte foi verificada pelo quadro
do próprio jogo, comparando o menu com e sem o mod.)

## M2.1 — Apresentação do quadro e D2GL ✅ (parcial)

Feito e validado no jogo real (Wine 11, 1.14d):

- evento **`on_present`** com tela GDI para os plugins, composta na superfície do
  jogo e **restaurada** depois (nada fica gravado): renderizador **GDI** (quadro de
  8 bits, `StretchBlt`, orientação lida do `CreateDIBSection`) e **D2GL/OpenGL**
  (`SwapBuffers` do `glide3x.dll`, estado GL salvo/restaurado);
- **`request_screenshot`** do quadro do próprio jogo (BMP) e API de mods;
- **integração do D2GL** ([`modding/D2GL.md`](modding/D2GL.md)): instalador
  `--d2gl-dir`, `-3dfx` automático, `-w` removido, `skip_intro`;
- exemplos: `hud-demo` (HUD com F9), `tools/make_art_demo.py` (mod de arte) e
  `map-overlay` (mapa de C1/C2, abaixo).

**Pendências desta etapa:** modo DirectDraw (código existe, **nunca exercitado**: o
Wine travou com o `ddraw.dll` do D2GL e não deu para forçar o DirectDraw
nativo); a **imagem completa sob o D2GL** neste ambiente (ver D2GL.md); medir
desempenho em monitores de alta taxa/resolução; Windows nativo.

## M2.2 — Estado do jogo (em andamento)

Plugins que **leem o jogo**. Cada item = spec de análise → símbolos verificados em
`addresses/1.14d.ini` → API. Prioridade: destrava os requisitos do client (abaixo).

1. **Unidades** (jogador, monstros, objetos/portais): ponteiro do jogador, tabelas
   de unidades por tipo, posição, vida, tipo de monstro. 🟡 spec
   [`game-exe-1.14d-units.md`](specs/game-exe-1.14d-units.md) + símbolos; usado pelo
   `map-overlay` (vida e dono da unidade ainda não).
2. **Nível/Mapa**: nível atual, geometria/saídas, *automap*. 🟡 spec
   [`game-exe-1.14d-levels-automap.md`](specs/game-exe-1.14d-levels-automap.md) +
   símbolos; usado pelo `map-overlay` (Atos II–V e resoluções ≠ 800×600 em aberto).
3. **Personagem**: atributos, pontos livres, habilidades e a rotina de criação.
4. **Carregador de tabelas**: hot-reload e acesso a linhas.

## Requisitos obrigatórios do client (após a suíte)

Pedidos pelo usuário em 2026-10-28, **a implementar depois de a suíte ser
finalizada**. Todos dependem do M2.2.

| # | Funcionalidade | Depende de | Observações |
|---|---|---|---|
| C1 | **Mapa: marcar onde ficam a entrada e a saída** (portas/escadas/waypoints) da área | níveis + objetos/saídas; desenho no automap (M2.1 `on_present`) | 🟡 **feito no Ato I** pelo mod `map-overlay` ([`modding/MAP_OVERLAY.md`](modding/MAP_OVERLAY.md)): warps (exatos ou pela sala), bordas com outros níveis e o waypoint do Ato I, em tela cheia e minimapa, GDI e D2GL |
| C2 | **Mapa: mostrar todos os inimigos** | lista de unidades-monstro do jogador atual | 🟡 **feito** pelo mesmo mod: monstros vivos das salas carregadas, NPCs à parte; campeão/único/superúnico pelos bits da spec (ainda não vistos em jogo); no modo multijogador/online não se aplica |
| C3 | **Planejador de builds**: salvar builds e **aplicá-las a personagens novos** | personagem (atributos/habilidades) + criação de personagem + UI | formato próprio versionado (`builds/*.json`); aplicar = distribuir pontos conforme o plano a cada nível, via API do jogo (nada de editar saves) |
| C4 | **Compatibilidade total com mods — Project Diablo 2 e Median XL** | suporte à base **1.13c** (ver abaixo) | **risco alto: ver a nota** |

**C1/C2 — o que falta** (detalhes e evidência em
[`modding/MAP_OVERLAY.md`](modding/MAP_OVERLAY.md)): validar Atos II–V e
dificuldades acima do Normal; ver em jogo campeões, únicos e superúnicos; nova
análise para o bit de lacaio, o dono da unidade (mercenários/invocações), as
colunas de `objects` (waypoints e objetos de quest sem configuração manual), o
estado de painel aberto e o encaixe do D2GL fora de 800×600/janela. Só o que o
cliente já carregou aparece: o mod não chama `AddRoomData`/`InitLevel`.

> **Nota sobre C4 (decisão arquitetural pendente).** Project Diablo 2 e Median XL
> são mods sobre o **Diablo II 1.13c** (várias DLLs: `D2Client.dll`, `D2Common.dll`,
> `Fog.dll`...; o Median XL Σ ainda traz o seu `D2Sigma.dll`). O Aeonian de hoje
> mira o **`Game.exe` 1.14d monolítico**. Para "compatibilidade total" é preciso
> **uma segunda família de builds** (1.13c e as DLLs/launchers desses mods): os
> hooks de IAT passam a ser instalados nos módulos carregados (Storm/Fog/D2*),
> há um banco de endereços por build **e por mod** (PD2 e Median XL mudam as
> DLLs), e os símbolos de C1–C3 precisam ser levantados **de novo** em cada
> alvo. O seu projeto-irmão do Switch já roda o Median XL Σ 2.14.6 sobre a 1.13c
> com `D2Autorun.dll` — é a melhor referência de como carregá-lo. Isto é, em
> prática, **o maior item do roadmap**; sugere-se começar por C1/C2/C3 na 1.14d e
> portar o levantamento de símbolos para o Median XL depois.

## M3 — UI/HUD por código (inclui o HUD do client)

Mover/redimensionar painéis (inventário, personagem, habilidades, HUD), widescreen
real, novos elementos. Depende de achar as tabelas/constantes de layout e as
rotinas de desenho de painel (spec por painel).

## M4 — Sistemas novos

Novas classes (tabela de classes, telas de seleção, animações), menus e telas.
É o item mais profundo: o jogo assume 7 classes em vários lugares. Avançar
classe a classe, com spec e teste de integração para cada ponto tocado.

## M5 — Portabilidade

- Validar em **Windows nativo**.
- Validar o proxy `ijl11.dll` na camada **AUTORUN/Wine-NX** (Switch). Se o
  carregamento por DLL proxy falhar lá, usar o injetor de import de PE do
  projeto-irmão para carregar `aeonian.dll` (`AeonianBootstrap`).
- Auditar W^X (o código já evita RWX), custo de E/S em SD e a alocação do heap
  do jogo (limite de 2 GB; ver notas do projeto-irmão).
- **Sem camada x86/Windows não há port por este caminho** (ver `SCOPE.md` §5).

## Fora de escopo (decisão registrada)

Reimplementar o motor (tentativa descontinuada e apagada), copiar assets,
multiplayer/Battle.net, anti-cheat, conteúdo novo apresentado como
"Diablo II vanilla".

## Formatos de arquivo

As specs de formato em `docs/specs/` e `docs/file-formats/` (DC6, DCC, DS1,
DT1, COF, PAL, TBL, MPQ, TXT) ficam como referência para as ferramentas do
devkit (visualizador/conversor de sprites para quem troca HUD e sprites,
editor de mapas). O código da tentativa anterior foi apagado: ferramentas
novas são escritas do zero, sem trazer motor nenhum.
