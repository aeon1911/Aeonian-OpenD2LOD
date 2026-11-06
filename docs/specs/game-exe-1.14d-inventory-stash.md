# Spec — Sistema de Inventário, Baú (Stash) e Compatibilidade PlugY (`Game.exe` 1.14d)

> **Tipo:** análise (fatos de interoperabilidade). Não contém código da
> Blizzard nem descompilado. Alimenta `aeonian/addresses/1.14d.ini` e o
> subsistema de baú infinito/compartilhado e respec (PlugY nativo).
> Companheiras: [`game-exe-1.14d-units.md`](game-exe-1.14d-units.md) (unidades),
> [`game-exe-1.14d-character.md`](game-exe-1.14d-character.md) (personagem/stats)
> e [`game-exe-1.14d-ui-hud.md`](game-exe-1.14d-ui-hud.md) (estados de UI e painéis).
>
> **Evidência e método:** `Game.exe` 1.14d (SHA-256
> `631066c1649c4ea9ffe48bf97e24c00bca1f7a6759c21150f1a79982589adaaf`),
> ImageBase `0x400000`, sem ASLR (VA = RVA + `0x400000`). Desmontagem estática
> do `.text` com `objdump` em referências cruzadas aos painéis de banco/baú
> (`%s\ui\panel\bank`, `%s\ui\panel\TradeStash`), à assinatura mágica de
> inventário `0x01020304`, às rotinas de nó de item (`pItemData+0x5C`) e ao
> decodificador de bitstream de itens (`0x4D4A` / "JM").
> Legenda: ✅ confirmado em execução; 🔷 confirmado no binário; ⚠️ hipótese
> (`TODO(RE)`).

---

## 1. O subsistema de Inventário (`Inventory`)

No `Game.exe` 1.14d, a unidade do jogador (`UnitAny*` em `[0x7A6A70]`, tipo `0`)
armazena o ponteiro para sua estrutura de inventário no deslocamento `+0x60`:

```
UnitAny (Jogador)
  └─ +0x60: Inventory* pInventory
```

### 1.1 Estrutura de `Inventory`

A estrutura de inventário possui a assinatura mágica fixa `0x01020304` em `+0x00`,
validada por todas as rotinas de manipulação antes de qualquer operação: 🔷

| Offset | Tam. | Tipo | Campo / Significado | Conf. |
|---|---|---|---|---|
| `+0x00` | 4 | `uint32_t` | `dwSignature`: valor fixo `0x01020304` | 🔷 |
| `+0x04` | 4 | `void*` | `pMemPool`: alocador de memória do contêiner | 🔷 |
| `+0x08` | 4 | `UnitAny*` | `pOwner`: unidade proprietária do inventário | 🔷 |
| `+0x0C` | 4 | `UnitAny*` | `pFirstItem`: primeiro item da lista encadeada | 🔷 |
| `+0x10` | 4 | `UnitAny*` | `pLastItem`: último item da lista encadeada | 🔷 |
| `+0x14` | 4 | `void*` | `pGrids`: ponteiro para as matrizes de grade | 🔷 |
| `+0x18` | 4 | `uint32_t` | `nGrids`: quantidade de matrizes ativas | 🔷 |
| `+0x1C` | 4 | `uint32_t` | `_unknown1`: flags internas de sincronização | 🔷 |
| `+0x20` | 4 | `UnitAny*` | `pCursorItem`: item atualmente seguro pelo cursor do mouse | 🔷 |
| `+0x24` | 4 | `uint32_t` | `_unknown2` | 🔷 |
| `+0x28` | 4 | `uint32_t` | `nItemsCount`: contagem total de itens no inventário | 🔷 |

---

## 2. Unidades de Item e Nó de Contêiner (`ItemNode`)

### 2.1 Unidade do Item (`UnitAny` Tipo 4)

Itens no jogo são instâncias de `UnitAny` com `Unit+0x00 == 4` (tipo `ITEM`):
- `Unit+0x04`: ID numérico da linha na tabela `Items.txt`;
- `Unit+0x0C`: ID único da unidade do item no mundo/servidor;
- `Unit+0x14`: Ponteiro `ItemData*` contendo os dados estendidos do item.

### 2.2 Layout do Nó de Item (`pItemData + 0x5C`)

No deslocamento `+0x5C` de `ItemData`, o jogo mantém o nó de ligação (`ItemNode`)
utilizado pela lista duplamente encadeada do inventário e posicionamento em grade: 🔷

| Offset | Tam. | Tipo | Campo / Significado | Conf. |
|---|---|---|---|---|
| `+0x00` | 4 | `Inventory*` | Ponteiro para o `Inventory` onde o item está inserido | 🔷 |
| `+0x04` | 4 | `UnitAny*` | `pPrevItem`: item anterior na lista encadeada | 🔷 |
| `+0x08` | 4 | `UnitAny*` | `pNextItem`: próximo item na lista encadeada | 🔷 |
| `+0x0C` | 1 | `uint8_t` | `nPage`: página/contêiner de armazenamento (§2.3) | 🔷 |
| `+0x0D` | 1 | `uint8_t` | `nNodePage`: página lógica secundária | 🔷 |
| `+0x10` | 4 | `uint32_t` | `nItemX`: coordenada horizontal (coluna na grade) | 🔷 |
| `+0x14` | 4 | `uint32_t` | `nItemY`: coordenada vertical (linha na grade) | 🔷 |

### 2.3 Identificadores de Página de Armazenamento (`nPage`)

Identificados nos despachantes de renderização e checagem de colisão:

| Código | Nome | Descrição | Dimensão padrão (LoD) |
|---|---|---|---|
| `0x00` | `PAGE_EQUIPPED` | Itens vestidos no corpo do personagem | Slots dedicados (Elmo, Armadura, etc.) |
| `0x01` | `PAGE_INVENTORY` | Mochila do personagem (`UI_INVENTORY`, `0x01`) | 10 colunas × 4 linhas (40 slots) |
| `0x02` | `PAGE_BELT` | Cinto de poções / mercenário | 4 colunas × 1 a 4 linhas |
| `0x03` | `PAGE_CUBE` | Cubo Horádrico (`UI_CUBE`, `0x1A`) | 3 colunas × 4 linhas (12 slots) |
| `0x04` | `PAGE_STASH` | Baú da cidade (`UI_STASH`, `0x19`) | 10 colunas × 10 linhas (100 slots LoD) / 6×8 clássico |

---

## 3. Ciclo de Vida e Interface do Baú (`UI_STASH`)

### 3.1 Painel do Baú no Cliente

O baú corresponde ao estado de UI `0x19` no vetor `[0x7A27C0]`.
O cliente mantém ponteiros globais para os sprites DC6 dos painéis:
- `[0x7BCC2C]`: Sprite do baú clássico (`%s\ui\panel\bank`);
- `[0x7BCC3C]`: Sprite do baú de expansão LoD (`%s\ui\panel\TradeStash`).

### 3.2 Rotinas de Abertura, Fechamento e Desenho

1. **`0x489E50` (`D2Client.InitStashUi`):**  
   Aloca e inicializa os recursos gráficos do baú. Verifica expansão via `0x408F20`
   e carrega `TradeStash`.
2. **`0x489EE0` (`D2Client.CloseStashUi`):**  
   Fecha o baú, chama `D2Client.SetUiState(0x19, 1, 0)` em `0x455F20` e emite o
   som de fechamento via `0x478600` (`cl = 0x4F`, `edx = 0x12`).
3. **`0x48F18F` (`D2Client.DrawStashPanel`):**  
   Desenha os blocos do painel do baú com `0x4F6480`, calcula o ouro depositado
   com `0x623460` (`D2Common.GetUnitBankGold`), define a página ativa em
   `[0x7BCC04] = 0x04` e despacha para `0x483FF0` para desenhar os itens do baú.
4. **`0x483FF0` (`D2Client.DrawContainerItems`):**  
   Recebe `UnitAny* pPlayer`, lê `pPlayer+0x60`, obtém a grade em `0x621050`,
   percorre os itens via `GetFirstInventoryItem` (`0x63B2C0`) e `GetNextInventoryItem`
   (`0x63DFD0`), desenhando cada item filtrado pela página `[0x7BCC04]`.

### 3.3 Caixas Delimitadoras dos Botões Nativos do Baú

Coordenadas calculadas relativas a `ScreenOffsetLeftX` (`[0x7A2858]`),
`ScreenOffsetTopY` (`[0x7A285C]`) e `ScreenHeight` (`[0x711470]`):
- **Botão Fechar (X)** em `0x489FB0`:
  - `X`: `[0x7A2858] + 0x113` a `+ 0x13B` (275 a 315 px);
  - `Y`: `ScreenHeight + [0x7A285C] - 0x64` a `- 0x3C`.
- **Botão Retirar Ouro** em `0x48A000`:
  - `X`: `[0x7A2858] + 0x90` a `+ 0xB8` (144 a 184 px);
  - `Y`: `ScreenHeight + [0x7A285C] - 0xDF` a `- 0xB7`.

---

## 4. Funções do Motor de Inventário (`D2Common`)

| VA | RVA | Convenção | Assinatura | Comportamento |
|---|---|---|---|---|
| `0x63B2C0` | `0x23B2C0` | `__stdcall` | `UnitAny* D2Common.GetFirstInventoryItem(Inventory* pInv)` | Valida `dwSignature == 0x01020304` e retorna `pInv->pFirstItem` (`+0x0C`). |
| `0x63B2E0` | `0x23B2E0` | `__stdcall` | `UnitAny* D2Common.GetLastInventoryItem(Inventory* pInv)` | Valida `dwSignature` e retorna `pInv->pLastItem` (`+0x10`). |
| `0x63DFD0` | `0x23DFD0` | `__stdcall` | `UnitAny* D2Common.GetNextInventoryItem(UnitAny* pItem)` | Valida `pItem->dwType == 4` e retorna o próximo item encadeado. |
| `0x63A7E0` | `0x23A7E0` | `__fastcall` | `ItemNode* D2Common.GetItemNode(UnitAny* pItem)` | Recebe `pItem` em `eax`. Retorna `pItem->pItemData + 0x5C`. |
| `0x63AAF0` | `0x23AAF0` | `__fastcall` | `int D2Common.RemoveItemFromInventory(UnitAny* pItem)` | Recebe `pItem` em `ecx`. Remove o item da lista encadeada e decrementa `pInv->nItemsCount`. |
| `0x63A810` | `0x23A810` | `__fastcall` | `void D2Common.UnlinkItemNode(ItemNode** ppHead, UnitAny* pItem)` | Desvincula o nó duplamente encadeado do item (`esi = &pInv->pFirstItem`, `edx = pItem`). |
| `0x63A8A0` | `0x23A8A0` | `__stdcall` | `int D2Common.CanItemFitInGrid(InventoryGrid* pGrid, int x, int y, int w, int h)` | Testa se o retângulo de células da grade está livre (`== 0`). |
| `0x63A910` | `0x23A910` | `__stdcall` | `void D2Common.SetItemInGrid(InventoryGrid* pGrid, int x, int y, int w, int h, UnitAny* pItem)` | Preenche as células da grade com o ID da unidade do item. |
| `0x63AAAD` | `0x23AAAD` | `__fastcall` | `void D2Common.ClearItemFromGrid(InventoryGrid* pGrid, int x, int y, int w, int h)` | Zera as células ocupadas pelo item na grade. |

---

## 5. Especificação de Interoperabilidade PlugY

O PlugY original (v10–v14) operava injetando hooks em assembly no D2 1.13c.
No Aeonian 1.14d, a compatibilidade total com os baús existentes dos jogadores
é garantida implementando a especificação binária exata dos arquivos `.sss` e `.d2x`.

### 5.1 Formato de Arquivo de Baú Compartilhado (`.sss`)

Armazenado no diretório de saves como `_LOD_SharedStashSave.sss`:

```
[Cabeçalho do Arquivo] (10 bytes)
  0x00: char magic[4]       = "SSS\0" (0x00535353)
  0x04: char version[2]     = "01" (0x3130) ou "02" (0x3230)
  0x06: uint32_t sharedGold = Ouro armazenado no baú compartilhado

[Metadados de Páginas] (4 bytes)
  0x0A: uint32_t pagesCount = Quantidade total de páginas no baú

[Bloco por Página] (Repetido pagesCount vezes)
  0x00: char pageHeader[2]  = "ST" (0x5453)
  0x02: uint32_t pageFlags  = Flags (0x01: compartilhada, 0x00: pessoal)
  0x06: char pageName[N]    = String terminada em \0 (nome da página, até 32 bytes)
  0x??: char itemHeader[2]  = "JM" (0x4D4A) - Bloco padrão de itens Diablo II
  0x??: uint16_t itemCount  = Quantidade de itens na página
  0x??: Bitstream de itens serializados (formato oficial Diablo II)
```

### 5.2 Formato de Arquivo de Baú Pessoal (`.d2x`)

Armazenado no diretório de saves como `<NomePersonagem>.d2x`.  
Idêntico ao formato `.sss`, com o cabeçalho mágico `magic[4] = "CSTM"` (`0x4D545343`).

### 5.3 Serialização e Deserialização de Itens ("JM")

O Diablo II codifica itens gravados com o prefixo `"JM"` (`0x4D4A`):
- **Leitura / Deserialização:** função em `0x533350` (`D2Game.LoadItems`), que
  valida o cabeçalho `"JM"`, lê a contagem de itens de 16 bits e decodifica o
  stream de bits de cada item via `0x532F30`.
- **Gravação / Serialização:** função em `0x5317FF` (`D2Game.SaveItems`), que
  grava `"JM"`, calcula e escreve a contagem e empacota os bits de cada item.

---

## 6. Arquitetura de Implementação do Plugin Aeonian

O plugin nativo (`aeonian-stash`) opera em 4 frentes limpas:

### 6.1 Renderização dos Botões de Navegação (Overlay)
No evento `on_present`:
- Quando `D2Client.GetUiState(0x19) == 1` (baú aberto):
  - Desenha os botões de navegação:
    - `|<<` (Primeira Página), `<` (Página Anterior);
    - `[ Página X / Y ]` (Indicador de página com nome customizado);
    - `>` (Próxima Página), `>>|` (Última Página);
    - `[ Compartilhado / Pessoal ]` (Alternador de modo).
  - Posicionamento calculado dinamicamente sobre a moldura superior do baú
    usando `[0x7A2858]` e `[0x7A285C]`.

### 6.2 Interceptação de Cliques e Atalhos
No evento `on_window_message`:
- Mensagens `WM_LBUTTONDOWN`:
  - Se as coordenadas do mouse caírem na área dos botões de navegação, consome
    o clique e executa a troca de página.
- Atalhos de teclado customizáveis:
  - `Seta Esquerda` / `Seta Direita` ou `PageUp` / `PageDown` para virar páginas
    quando o baú estiver aberto.

### 6.3 Mecanismo de Troca de Página (Swap em Memória)
Ao trocar de página (ex.: da página 1 para a página 2):
1. **Descarregar itens da página atual:**
   - Itera a lista `pInventory->pFirstItem`;
   - Para cada item com `nPage == 0x04`:
     - Limpa da grade via `ClearItemFromGrid` (`0x63AAAD`);
     - Desvincula da lista encadeada via `RemoveItemFromInventory` (`0x63AAF0`);
     - Armazena a referência no vetor de páginas em memória do plugin.
2. **Carregar itens da nova página:**
   - Para cada item da página de destino:
     - Define `nPage = 0x04`;
     - Insere na grade via `SetItemInGrid` (`0x63A910`);
     - Vincula na lista encadeada de `pInventory`.
3. **Atualização Visual:**
   - Força a invalidação gráfica do painel do baú para repintura imediata.

### 6.4 Respec de Atributos e Habilidades
- **Atributos:**
  - Quando a tela de personagem (`UI_CHAR_SCREEN`, `0x02`) estiver aberta e o
    jogador segurar `Alt` (ou clicar no botão `-` customizado):
    - Subtrai 1 ponto do atributo base via `D2Common.AddUnitStat(pPlayer, nStatId, -1, 0)` em `0x6272B0`;
    - Incrementa 1 ponto livre via `D2Common.AddUnitStat(pPlayer, 4, +1, 0)`.
- **Habilidades:**
  - Na árvore de habilidades (`UI_SKILL_TREE`, `0x04`):
    - Subtrai o nível base da habilidade em `SkillList`;
    - Incrementa o stat `5` (`newskills`) com os pontos devolvidos.

