# Spec — Sistema de UI e HUD por Código (`Game.exe` 1.14d)

> **Tipo:** análise (fatos de interoperabilidade). Não contém código da
> Blizzard nem descompilado. Alimenta `aeonian/addresses/1.14d.ini` e o item
> **M3** (UI e HUD por código) de [`../ROADMAP.md`](../ROADMAP.md).
> Companheiras: [`game-exe-1.14d-character.md`](game-exe-1.14d-character.md) e
> [`game-exe-1.14d-levels-automap.md`](game-exe-1.14d-levels-automap.md).
>
> **Evidência e método:** `Game.exe` 1.14d (SHA-256
> `631066c1649c4ea9ffe48bf97e24c00bca1f7a6759c21150f1a79982589adaaf`),
> ImageBase `0x400000`, sem ASLR (VA = RVA + `0x400000`). Desmontagem estática
> do `.text` com `objdump` em referências cruzadas ao vetor `[0x7A27C0]`,
> aos globais de deslocamento de tela `[0x7A2858]` / `[0x7A285C]` e às rotinas
> de renderização de painéis.
> Legenda: ✅ confirmado em execução; 🔷 confirmado no binário; ⚠️ hipótese
> (`TODO(RE)`).

## 1. O vetor de estados de interface (`UiState`)

O cliente mantém um vetor de 38 inteiros (`0x26` entradas de 4 bytes) em
`[0x7A27C0]`. Cada índice representa uma tela ou painel modal/não-modal do
jogo. Valor `0` = fechado; valor `1` = aberto. 🔷

| Índice | Nome | Tecla padrão | Posição / Lado | Descrição |
|---|---|---|---|---|
| `0x01` | Inventário | `I` / `B` | Direita | Painel de itens e equipamentos do jogador |
| `0x02` | Personagem | `C` / `A` | Esquerda | Folha de atributos, resistências e pontos |
| `0x03` | Seleção rápida de skills | Botões skill | Flutuante | Menu de seleção de habilidade direita/esquerda |
| `0x04` | Árvore de habilidades | `T` / `S` | Direita | Árvore de habilidades com 3 abas da classe |
| `0x05` | Chat | `Enter` | Inferior esquerdo | Linha de entrada de texto e mensagens |
| `0x06` | Troca (Trade) | — | Esquerda | Painel de negociação com outro jogador |
| `0x07` | Confirmação de troca | — | Centro | Diálogo de aceite de troca |
| `0x08` | Menu de NPC | — | Esquerda | Opções de conversa, comércio, aposta |
| `0x09` | Menu Esc | `Esc` | Centro | Opções de jogo, retornar, salvar e sair |
| `0x0A` | Automap | `Tab` | Tela toda | Minimapa ou mapa sobreposto |
| `0x0B` | Opções de configuração | — | Centro | Configurações de vídeo, áudio e teclado |
| `0x0C` | Diálogo com NPC | — | Esquerda | Caixa de fala e som do NPC |
| `0x0F` | Missões (Quest Log) | `Q` | Esquerda | Livro de missões dos Atos |
| `0x11` | Seleção de Waypoint | — | Esquerda | Lista de waypoints ativos no Ato |
| `0x12` | Minipainel | — | Inferior centro | Botões pequenos do HUD |
| `0x13` | Mercenário | `O` | Esquerda | Equipamentos e atributos do mercenário |
| `0x14` | Atalhos de feitiços | — | Centro | Overlay de atalhos rápidos |
| `0x15` | Menu de Grupo / PvP | `P` | Esquerda | Relações de grupo e declaração de hostilidade |
| `0x17` | Cinto expandido (Belt) | `~` | Inferior centro | Linhas extras do cinto de poções |
| `0x18` | Ajuda (Help) | `H` | Centro | Telas de ajuda e tutorial |
| `0x19` | Baú (Stash) | — | Esquerda | Baú da cidade |
| `0x1A` | Cubo Horádrico | — | Esquerda | Inventário interno do Cubo |

## 2. Manipulação de estados de UI

### 2.1 Funções do jogo (`D2Client`)

| VA | RVA | Convenção | Assinatura | Comportamento |
|---|---|---|---|---|
| `0x453A90` | `0x53A90` | `__fastcall` | `int D2Client.GetUiState(int nUiId)` | Lê `[0x7A27C0 + nUiId*4]`. Não valida faixa. |
| `0x4538D0` | `0x538D0` | `__fastcall` | `int D2Client.GetUiStateChecked(int nUiId)` | Asserta se `nUiId >= 0x26`. Prefira `GetUiState`. |
| `0x455F20` | `0x55F20` | `__fastcall` | `int D2Client.SetUiState(int nUiId, int nAction, int a3)` | Altera o estado do painel: `nAction = 0` (abrir), `nAction = 1` (fechar), `nAction = 2` (alternar/toggle). Valida conflitos entre painéis e chama hooks de abertura. |

### 2.2 Conflitos entre painéis (`0x711130`)

Em `0x453910`, o jogo consulta a matriz bidimensional `[0x711130 + nExistingUiId*4][nTargetUiId*4]`:
- Painéis esquerdos (Personagem `0x02`, Missões `0x0F`, Baú `0x19`, etc.) fecham automaticamente o painel esquerdo anterior.
- Painéis direitos (Inventário `0x01`, Árvore de Skills `0x04`) fecham o painel direito anterior.
- Diálogos de menu e Esc fecham todos os painéis abertos.

## 3. Posicionamento de painéis e resolução

### 3.1 Globais de deslocamento (`ScreenOffsets`)

O cliente calcula as coordenadas de todos os painéis e globos de interface a
partir da resolução corrente (`[0x71146C]` largura, `[0x711470]` altura) e de
dois deslocamentos base calculados em `0x456EE0`:

| Endereço | Nome | 640×480 | 800×600 | Descrição |
|---|---|---|---|---|
| `[0x7A2858]` | `ScreenOffsetLeftX` | `0` | `80` (`0x50`) | Deslocamento horizontal dos painéis em relação à borda |
| `[0x7A285C]` | `ScreenOffsetTopY` | `0` | `-60` (`-0x3C`) | Deslocamento vertical da base dos painéis |
| `[0x7BC96C]` | `PanelSideLeftBusy` | `0` ou `1` | `0` ou `1` | Flag que indica se algum painel esquerdo está cobrindo a tela |
| `[0x7BC968]` | `PanelSideRightBusy` | `0` ou `1` | `0` ou `1` | Flag que indica se algum painel direito está cobrindo a tela |
| `[0x7A5210]` | `AutomapPanelSide` | `0..3` | `0..3` | Modo de desvio do automap (`0` livre, `1` painel à dir., `2` esq., `3` ambos) |

### 3.2 Fórmulas de posicionamento

Para um painel esquerdo (ex.: Personagem `0x02`):
- `X = [0x7A2858]` (em 800×600 fica em X = 80; em 640×480 em X = 0).
- `Y = ScreenHeight + [0x7A285C] - 224`.

Para um painel direito (ex.: Inventário `0x01`):
- `X = ScreenWidth - [0x7A2858] - PanelWidth` (largura do painel = 320 px).
- `Y = ScreenHeight + [0x7A285C] - 224`.

Para os globos de Vida e Mana:
- **Globo de Vida (esquerda):**
  - Se `AutomapPanelSide == 2` (painel aberto à esquerda): `X = (ScreenWidth / 2) + 40`.
  - Senão: `X = 40`.
- **Globo de Mana (direita):**
  - Se `AutomapPanelSide == 1` (painel aberto à direita): `X = ScreenWidth - (ScreenWidth / 2) - 73`.
  - Senão: `X = ScreenWidth - 73`.

## 4. Despachante e rotinas de desenho de interface

No laço principal do cliente (`0x44EFA0`), durante a fase de apresentação do
quadro da partida, o despachante de UI é invocado em `0x456EE0`:

| VA | RVA | Símbolo | Função |
|---|---|---|---|
| `0x456EE0` | `0x56EE0` | `D2Client.UiDrawDispatcher` | Ponto de entrada do desenho da interface inteira. Calcula offsets e desenha componentes. |
| `0x456F02` | `0x56F02` | `D2Client.UiScreenOffsetsAnchor` | Âncora da atribuição de `[0x7A2858]` (`0x50`) e `[0x7A285C]` (`-0x3C`). |
| `0x48EDF0` | `0x8EDF0` | `D2Client.DrawInventory` | Desenha o painel de inventário e os itens equipados. |
| `0x4A7D00` | `0xA7D00` | `D2Client.DrawCharacterSheet` | Desenha a folha de personagem (atributos, nível, experiência). |
| `0x495180` | `0x95180` | `D2Client.DrawSkillTree` | Desenha a árvore de habilidades e ícones de skill. |
| `0x4A6B30` | `0xA6B30` | `D2Client.DrawHealthOrb` | Desenha o globo de vida e o valor numérico. |
| `0x4A6E60` | `0xA6E60` | `D2Client.DrawManaOrb` | Desenha o globo de mana e o valor numérico. |
| `0x494320` | `0x94320` | `D2Client.DrawExpBar` | Desenha a barra inferior central de experiência. |
| `0x494EB0` | `0x94EB0` | `D2Client.DrawBelt` | Desenha as colunas de poções do cinto no HUD. |
| `0x47EA60` | `0x7EA60` | `D2Client.PanelSideLeftCheck` | Avalia se o lado esquerdo possui painel ativo. |
| `0x47EB50` | `0x7EB50` | `D2Client.PanelSideRightCheck` | Avalia se o lado direito possui painel ativo. |

