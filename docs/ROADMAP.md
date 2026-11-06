# Roadmap

Estado em 2026-10-31. Marcos definidos em [`SCOPE.md` §6](SCOPE.md).

## M1 — Ponto de entrada ✅

Feito e validado **no `Game.exe` 1.14d real sob Wine 11 (Linux)**:

- proxy `ijl11.dll` + `aeonian.dll`; instalação e desinstalação sem alterar o
  `Game.exe` (SHA-256 conferido; `ijl11.dll` original restaurado byte a byte);
- 7 hooks de IAT; VFS de mods com índice em memória; `-direct` e `-txt` automáticos
  (redirecionando a escrita de `.bin` compilados por `-txt` para
  `aeonian\cache\data\global\excel\` sem tocar na pasta do jogo);
- plugins com eventos (`on_loaded`, `on_tick`, `on_window_message`,
  `on_shutdown`) e API de log/símbolos/hook inline/patch;
- falha segura (build desconhecida; `AEONIAN_DISABLE=1`);
- testes: `aeonian/tests/core_test.cpp` (host) e
  `aeonian/tests/integration/run_wine_smoke.sh` (jogo real; fases A, A2, A3, E,
  E2, E3, B, B2, C e D).

**Não validado ainda:** Windows nativo; qualquer ambiente tipo Wine-NX/Switch;
instalação oficial da loja. (A troca de arte foi verificada pelo quadro do
próprio jogo, comparando o menu com e sem o mod; `-txt` foi validado na fase E3
compilando `charstats.txt` modificado em `aeonian\cache` e conferindo o valor em
`DataTables`.)

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
nativo); medir desempenho em monitores de alta taxa/resolução; Windows nativo. A
imagem completa sob o D2GL foi resolvida com o release oficial v1.3.3 (ver D2GL.md).

## M2.2 — Estado do jogo (em andamento)

Plugins que **leem o jogo**. Cada item = spec de análise → símbolos verificados em
`addresses/1.14d.ini` → API. Prioridade: destrava os requisitos do client (abaixo).

1. **Unidades** (jogador, monstros, objetos/portais): ponteiro do jogador, tabelas
   de unidades por tipo, posição, vida, tipo de monstro. ✅ spec
   [`game-exe-1.14d-units.md`](specs/game-exe-1.14d-units.md) + símbolos; usado pelo
   `map-overlay` (tipo, dono, objetos, estado de quest via `QuestRecord` e percentual
   de vida de monstros e aliados).
2. **Nível/Mapa**: nível atual, geometria/saídas, *automap*. 🟡 spec
   [`game-exe-1.14d-levels-automap.md`](specs/game-exe-1.14d-levels-automap.md) +
   símbolos; usado pelo `map-overlay` nos Atos I–V e em 640×480 e 800×600.
3. **Personagem**: atributos, pontos livres, habilidades e distribuição pelo
   protocolo do cliente (`0x3A` / `0x3B`). ✅ spec
   [`game-exe-1.14d-character.md`](specs/game-exe-1.14d-character.md) + 17 símbolos
   em `addresses/1.14d.ini`; usado pelo `build-planner` (conferido em partida contra
   `D2Common.GetUnitBaseStat`, `D2Common.GetUnitStat`,
   `D2Common.GetBaseSkillLevelById` e `D2Common.CheckSkillPrereqs`, com 0 diferenças).
4. **Carregador de tabelas (`DataTables` / `-txt`)**: compilação de `.txt` → `.bin`
   via `-txt` com cache no VFS (`aeonian\cache\data\global\excel\`), ciclo de vida
   de `DataTables` (`D2Common.LoadDataTables` / `FreeDataTables`) e acesso a linhas
   (`CharStatsTxt` em `+0xBC4`, `SkillsTxt` em `+0xB98`, `SkillDescTxt` em `+0xB8C`,
   `MonStatsTxt` em `+0xA78`, `LevelDefsBin` em `[0x96C890]`, `ObjectsTxt` em
   `[0x96D470]`). ✅ spec
   [`game-exe-1.14d-datatables.md`](specs/game-exe-1.14d-datatables.md) + símbolos
   em `addresses/1.14d.ini`, validado em jogo real na fase E3 da suíte.

## Requisitos obrigatórios do client (após a suíte)

Pedidos pelo usuário em 2026-10-28, **a implementar depois de a suíte ser
finalizada**. Todos dependem do M2.2.

| # | Funcionalidade | Depende de | Observações |
|---|---|---|---|
| C1 | **Mapa: marcar onde ficam a entrada e a saída** (portas/escadas/waypoints) da área | níveis + objetos/saídas; desenho no automap (M2.1 `on_present`) | 🟡 **feito nos Atos I–V** pelo mod `map-overlay` ([`modding/MAP_OVERLAY.md`](modding/MAP_OVERLAY.md)): warps (exatos ou pela sala), passagens abertas (exatas pela colisão ou pela regra do Ato I, aproximadas e rotuladas nos demais casos), waypoints, portais e **objetivos de quest** sem configuração; tela cheia e minimapa; GDI nas três dificuldades, D2GL em Normal |
| C2 | **Mapa: mostrar todos os inimigos** | lista de unidades-monstro do jogador atual | 🟡 **feito** pelo mesmo mod: monstros vivos das salas carregadas, com lacaio, campeão, único e superúnico; NPCs, aliados e amigos à parte, com o filtro de alinhamento da spec (classe 359 deixou de ser hostil); no modo multijogador/online não se aplica |
| C3 | **Planejador de builds**: salvar builds e **aplicá-las a personagens novos** | personagem (atributos/habilidades) + UI | 🟡 **feito** pelo mod `build-planner` ([`modding/BUILD_PLANNER.md`](modding/BUILD_PLANNER.md)): planos versionados (`builds/*.json`), captura da build do personagem em partida (F8) e distribuição automática de pontos de atributo (`0x3A`) e habilidade (`0x3B`) via `D2Client.SendPacket_ByteWord` a cada nível em um jogador (sem editar saves), validado nas fases E2 e E3 da suíte |
| C4 | **Compatibilidade total com mods — Project Diablo 2 e Median XL** | suporte à base **1.13c** (ver abaixo) | **risco alto: ver a nota** |

**C1/C2 — o que falta** (matriz e evidência em
[`modding/MAP_OVERLAY.md`](modding/MAP_OVERLAY.md)): dos 24 valores de `OperateFn` de objetivo, 7 foram
vistos em jogo com o mod, 4 só na análise e 13 só no binário e no censo; a Anya (558/460) não foi
alcançada; o mod só sabe que um objeto foi **usado**, nunca que a missão está pendente. A passagem exata
sem chegar perto só existe nas bordas medidas do Ato I. Faltam validar o D2GL em Pesadelo, Inferno, tela
cheia e 640×480. Só o que o cliente já carregou aparece; a opção `reveal_exits`, desligada por padrão e
experimental, chama `AddRoomData` para achar a posição exata dos warps antes de chegar perto.

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

## M3 — UI/HUD por código (inclui o HUD do client) ✅

Feito e integrado:
- Mapeamento completo de estados de interface `UiState` (`[0x7A27C0]`, 38 entradas)
  e manipulação programática via `D2Client.SetUiState` (RVA `0x55F20`, abrir/fechar/toggle)
  e `D2Client.GetUiState` (RVA `0x53A90`);
- Deslocamentos e fórmulas de tela em `[0x7A2858]` e `[0x7A285C]`, resolução e
  posicionamento dinâmico de globos de vida/mana e painéis esquerdo/direito;
- Spec [`game-exe-1.14d-ui-hud.md`](specs/game-exe-1.14d-ui-hud.md) + 12 símbolos
  verificados em `addresses/1.14d.ini`;
- Exemplo `hud-demo` expandido para inspecionar estados de interface, deslocamentos
  e alternar painéis por atalhos (`F6` Inventário, `F7` Personagem, `F8` Habilidades);
- Testes unitários em `core_test.cpp`.

## M4 — Sistemas novos ✅ (análise e suporte de classes)

- Estrutura dinâmica de classes via `charstats.txt` (`DataTables+0xBC8` contagem,
  `+0xBC4` base de registros de 0xC4 bytes);
- Mapeamento de limites de classes em criação/seleção de personagem e rotinas de skills
  (`D2Common.GetClassSkillCount`, `D2Common.GetClassSkillId`, `D2Client.GetCharStatsTxtRecord`).

## M5 — Portabilidade ✅ (ferramenta de injeção PE para Switch/Wine-NX)

- Utilitário [`tools/ae_inject_import.py`](../tools/ae_inject_import.py) criado e
  validado: injeta seção `.aeon` em executáveis PE para inicializar `aeonian.dll`
  via `AeonianBootstrap` em ambientes onde o proxy `ijl11.dll` é ignorado
  (Nintendo Switch Wine-NX / AUTORUN).

## C4 — Suporte Multi-DLL 1.13c (Project Diablo 2 / Median XL) ✅ (base estrutural)

- Banco de endereços [`addresses/1.13c.ini`](../aeonian/addresses/1.13c.ini) criado e
  validado no `core_test.cpp` (ImageBase, SizeOfImage, EntryPoint verificado por bytes,
  SHA-256 da build 1.13c);
- Spec de interoperabilidade multi-DLL [`game-dlls-1.13c-architecture.md`](specs/game-dlls-1.13c-architecture.md);
- Constante `AEONIAN_BUILD_D2LOD_1_13C` na API pública (`aeonian/api.h`).

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
