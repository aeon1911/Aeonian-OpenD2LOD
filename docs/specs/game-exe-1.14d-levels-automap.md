# Spec — Nível, salas, saídas e automap do `Game.exe` 1.14d

> **Tipo:** análise (fatos de interoperabilidade). Não contém código da
> Blizzard nem descompilado. Alimenta `aeonian/addresses/1.14d.ini` e o
> primeiro mod de teste (itens **C1** e **C2** de [`../ROADMAP.md`](../ROADMAP.md)).
> Companheira: [`game-exe-1.14d-units.md`](game-exe-1.14d-units.md) (estado do
> cliente, unidades, monstros). Evidência, método e legenda (✅ execução,
> 🔷 só binário, ⚠️ hipótese/`TODO(RE)`) são os mesmos de lá: `Game.exe` 1.14d
> SHA-256 `631066c1649c4ea9ffe48bf97e24c00bca1f7a6759c21150f1a79982589adaaf`,
> `objdump` + índice próprio de referências, execução invisível e muda sob Wine
> 11.20 com uma sonda externa de leitura de memória (2026-10-29). Endereços em
> **VA** (VA = RVA + `0x400000`, sem ASLR).

## 1. Coordenadas

- Unidades e salas ativas usam **subtiles**; níveis e `Room2` usam **tiles**;
  1 tile = 5 subtiles. ✅
- Coordenada de cliente (pixels isométricos do mundo): `cx = (x − y)·16`,
  `cy = (x + y)·8` (x, y em subtiles). ✅ (detalhes em `game-exe-1.14d-units.md` §3.1)

## 2. Da partida ao nível atual

```
[0x7A0634] Act*  (= jogador+0x1C)
  └─ Act+0x48 → DrlgAct
        └─ +0x47C → primeiro Level → Level+0x1AC → próximo ...
jogador → caminho → Room1 (+0x1C dinâmico) → Room1+0x10 → Room2 → Room2+0x58 → Level → +0x1D0 = id
```

**Nível atual** = `jogador → Room1 → Room2 → Level → +0x1D0`. ✅ (1 na cidade, 2
na Charneca, 8 depois de entrar no Covil do Mal.) Atalho do jogo:
`0x61A1B0` `D2Common.GetLevelIdFromRoom`, `__stdcall(Room1*)`, devolve 0 com
sala nula (sem asserção), 276 chamadores. 🔷 `0x66BAB0` faz o mesmo a partir da
`Room2` (`__fastcall`, `ecx` = `Room2*`). 🔷

### 2.1 `Act`

| Offset | Campo | Conf. |
|---|---|---|
| `+0x0C` | semente do mapa (igual a `DrlgAct+0x458`) | ✅ valor |
| `+0x10` | ponteiro (lista de salas ativas?) | ⚠️ `TODO(RE)` |
| `+0x14` | 0 no Ato I (número do ato?) | ⚠️ só um ato visto |
| `+0x48` | `DrlgAct*` | ✅ |

### 2.2 `DrlgAct`

| Offset | Campo | Conf. |
|---|---|---|
| `+0x90` | lista de exceções de vizinhança (nós de 0x48 bytes, próximo em `+0x44`, id do nível em `+0x00`, `Vis` em `+0x04`, `Warp` em `+0x24`); consultada antes de `LevelDefs` | 🔷 |
| `+0x458` | semente (igual a `Act+0x0C`) | ✅ |
| `+0x46C` | `Act*` (volta) | ✅ |
| `+0x470` | base da semente dos níveis | 🔷 |
| `+0x478` | pool de memória (0 no cliente) | ✅ |
| `+0x47C` | primeiro `Level` | ✅ |

### 2.3 `Level` (0x230 bytes 🔷)

| Offset | Campo | Conf. |
|---|---|---|
| `+0x00` | tipo DRLG: 1 labirinto, 2 preset, 3 externo | ✅ (cidade 2, Charneca 3, Covil 1) |
| `+0x04` | flags | 🔷 |
| `+0x10` | primeira `Room2`; **0 = nível ainda não gerado** | ✅ |
| `+0x1C`, `+0x20` | X, Y em tiles | ✅ |
| `+0x24`, `+0x28` | largura, altura em tiles | ✅ |
| `+0x1AC` | próximo `Level` do ato | ✅ |
| `+0x1B4` | `DrlgAct*` | ✅ |
| `+0x1C0` | tipo de nível (de `LevelDefs+0x34`) | 🔷 |
| `+0x1C4` | semente | 🔷 |
| `+0x1D0` | id (linha de `levels.txt`) | ✅ |

Na primeira leitura dentro do Ato I, o ato já tinha os níveis 1, 2, 3, 4,
5, 6, 7, 8–16, 17, 26, 27, 39 **alocados**, mas só 1, 2 e 3 com salas
(`+0x10 ≠ 0`). Ao carregar a sala da entrada do Covil, o nível 8 ganhou 27
salas. ✅

### 2.4 `Room2` (sala do DRLG, 0xEC bytes 🔷)

| Offset | Tam. | Campo | Conf. |
|---|---|---|---|
| `+0x08` | 4 | lista de `Room2*` vizinhas (pode incluir salas de **outro** nível) | ✅ |
| `+0x0E` | 2 | contador de carga explícita (1 nas salas pedidas pelo servidor, 0 nas vizinhas) | ✅ observado |
| `+0x20` | 4 | dados do tipo (preset/labirinto) | 🔷 |
| `+0x24` | 4 | próxima `Room2` do nível | ✅ |
| `+0x28` | 4 | flags; bit `0x10 << n` = contém o warp *n* (§4.2) | ✅ |
| `+0x2C` | 4 | número de vizinhas | ✅ |
| `+0x30` | 4 | `Room1*`; **≠ 0 ⇔ sala carregada** | ✅ |
| `+0x34`, `+0x38` | 4 | X, Y em tiles | ✅ |
| `+0x3C`, `+0x40` | 4 | largura, altura em tiles | ✅ (8×8 na cidade) |
| `+0x48` | 4 | tipo da sala (2 = preset) | 🔷 + ✅ valor |
| `+0x4C` | 4 | lista de `RoomTile` (saídas por warp, §4.2) | ✅ |
| `+0x58` | 4 | `Level*` | ✅ |
| `+0x5C` | 4 | lista de `PresetUnit` | ✅ |

### 2.5 `Room1` (sala ativa)

| Offset | Campo | Conf. |
|---|---|---|
| `+0x00` | lista de `Room1*` vizinhas (inclui a própria) | ✅ |
| `+0x10` | `Room2*` | ✅ |
| `+0x24` | número de vizinhas | ✅ (9 no interior) |
| `+0x2C` | `Act*` | ✅ |
| `+0x4C`, `+0x50` | X, Y em subtiles | ✅ |
| `+0x54`, `+0x58` | largura, altura em subtiles | ✅ (40×40) |
| `+0x5C`, `+0x60` | X, Y em tiles | ✅ |
| `+0x64`, `+0x68` | largura, altura em tiles | ✅ |
| `+0x74` | primeira unidade da sala (encadeia por `Unit+0xE8`) | ✅ |
| `+0x7C` | próxima `Room1` | ✅ |

`0x619790` `D2Common.GetRoomsNear`: `__stdcall(Room1*, Room1*** lista, int*
qtd)`, copia `+0x00`/`+0x24`; asserta com sala nula. 🔷

### 2.6 `PresetUnit` (unidade predefinida, 0x20 bytes)

| Offset | Campo | Conf. |
|---|---|---|
| `+0x00` | 1 nos monstros vistos, 0 nos objetos | ⚠️ semântica |
| `+0x04` | classe (linha de `monstats`/`objects`/`lvlwarp`) | ✅ |
| `+0x08` | X relativo à sala, em subtiles | ✅ |
| `+0x0C` | próxima `PresetUnit` | ✅ |
| `+0x10` | trajeto de IA (não nulo nos NPCs com rota) | 🔷 + ✅ valor |
| `+0x14` | tipo de unidade (1 monstro, 2 objeto, 5 tile/saída) | ✅ |
| `+0x18` | Y relativo à sala, em subtiles | ✅ |
| `+0x1C` | 1 nos objetos que viram unidades só do cliente | ⚠️ |

**Posição no mundo** = `(Room2+0x34)·5 + (+0x08)`, `(Room2+0x38)·5 + (+0x18)`.
✅ Prova: waypoint (classe 119) → (5639, 5494) e baú (267) → (5646, 5524),
iguais às unidades vivas; entrada do Covil (tipo 5, classe 2) → (5454, 5741),
igual à unidade tile viva.

### 2.7 `LevelDefs` (`levels.txt` binário)

- `0x61E470` `D2Common.GetLevelDefRecord`: `__fastcall(int id)` (`ecx`),
  devolve `[0x96C890] + id·0x9C` ou 0 fora da faixa (sem asserção); quantidade
  em `[[0x744304]+0xC5C]` (137 observados). ✅
- Campos: `+0x30` tipo DRLG ✅, `+0x34` tipo de nível 🔷, **`+0x48` `Vis[8]`**
  (ids dos níveis vizinhos, int32) ✅, **`+0x68` `Warp[8]`** (ids de `lvlwarp`,
  −1 = sem warp) ✅.
- Valores vistos: nível 1 `Vis` = 0…, `Warp` = −1…; nível 2 `Vis[3..6]` = 8,
  `Warp[3..6]` = 0, 1, 2, 3; nível 8 `Vis[0]` = 2, `Warp[0]` = 4. ✅
- O jogo consulta antes a lista de exceções `DrlgAct+0x90`:
  `0x66C040` (`Vis`) e `0x66AEC0` (`Warp`), ambos `__fastcall(DrlgAct*, id)` →
  `int[8]`. 🔷 Use essas duas para ter o mesmo resultado do jogo.

### 2.8 Cidades

`0x6426A0` `D2Common.IsTownLevelId`: `__fastcall(int id)` → 1 para 1, 40, 75,
103, 109 (tabela de bytes lida do binário). 🔷 `0x61AB00`
`D2Common.IsRoomInTown`: `__stdcall(Room1*)`. 🔷

## 3. Salas não visitadas: o cliente precisa carregá-las?

**Sim, para ver o conteúdo.** Observado ao vivo ✅:

- Depois de gerado, o nível tem **todas** as `Room2` (Charneca: 82), com posição,
  vizinhança e flags (`+0x28`).
- Só as salas **carregadas** (`Room2+0x30 ≠ 0`) têm `PresetUnit` (`+0x5C`),
  `RoomTile` (`+0x4C`) e unidades. Na Charneca, com 5 de 82 carregadas, só 1
  tinha predefinidas; com 34 carregadas, 10.
- A entrada do Covil só ganhou `PresetUnit` tipo 5, `RoomTile` e unidade tile
  quando o jogador chegou perto.

O caminho normal: o **servidor** manda o pacote `0x07` (carregar sala: `WORD x`,
`WORD y` em tiles, `BYTE` nível) e `0x08` (descarregar). Tabela de tratadores
S→C em `0x7114D0` (12 bytes por entrada: tratador, tamanho, 0); o `0x07` está em
`0x45CAB0` e o `0x08` em `0x45CB20`, ambos com tamanho 6. 🔷

Funções do jogo que fazem o trabalho (**só documentadas; nenhuma foi chamada**;
todas têm efeitos colaterais e podem quebrar a sincronia com o servidor):

| VA | Convenção | Assinatura | Efeito | Conf. |
|---|---|---|---|---|
| `0x642BB0` | `__stdcall` | `Level* GetLevel(DrlgAct*, int id)` | procura; **aloca** o nível se não existir (`0x642AE0`) | 🔷 |
| `0x6424A0` | `__stdcall` | `void InitLevel(Level*)` | gera as `Room2` (despacha pelo tipo DRLG: 1 → `0x673B30`, 2 → `0x668100`, 3 → `0x675360`) | 🔷 |
| `0x61A070` | `__stdcall` | `void AddRoomData(Act*, int id, int x, int y, Room1* perto)` | carrega a sala que contém (x, y) em tiles; incrementa `Room2+0x0E` e ativa se era 0 | 🔷 |
| `0x61A0C0` | `__stdcall` | `void RemoveRoomData(Act*, int id, int x, int y, Room1* perto)` | inverso | 🔷 |

O jogo usa o padrão `n = GetLevel(drlgAct, id); se (n+0x10 == 0) InitLevel(n)`
em vários lugares (`0x66B030`, `0x66C220`, `0x642C30`, `0x668100`). 🔷 O tratador do
pacote `0x07` chama `AddRoomData([0x7A0634], nível, x, y, sala do jogador)`.
🔷

`TODO(RE)` antes de qualquer uso: thread (só na principal), custo, se o
servidor local (single player) reage a salas carregadas pelo cliente, e
liberação (`RemoveRoomData` na mesma posição).

## 4. Saídas do nível (C1)

O jogo identifica três tipos de saída:

### 4.1 Passagem aberta entre níveis externos

Uma `Room2` cujas vizinhas (`+0x08`, `+0x2C`) incluem salas de **outro**
`Level` (`Room2+0x58` diferente) é borda de saída. A cidade do Ato I liga-se à
Charneca assim, sem warp: as salas da borda sul (tiles y = 1112) têm vizinhas do
nível 2 em y = 1120. ✅ Funciona sem carregar salas (vizinhança existe desde a
geração).

### 4.2 Warps (cavernas, escadas, portais de nível)

1. `Vis[n]`/`Warp[n]` do nível (§2.7) dizem que o warp *n* leva ao nível
   `Vis[n]` usando o registro `Warp[n]` de `lvlwarp`. ✅
2. A `Room2` que contém o warp *n* tem o bit `0x10 << n` em `+0x28`. ✅ Na
   Charneca, **uma** sala tinha `0x200` (n = 5 → `Vis[5]` = 8, Covil do Mal) e era
   a sala da entrada. **Atenção:** os bits de índices sem `Vis` (0x10, 0x20…)
   aparecem em quase todas as salas e não significam warp; só considere *n* com
   `Vis[n] ≠ 0` e `Warp[n] ≠ −1`. ✅ Isso dá a sala da saída **sem carregar nada**;
   a posição exata exige a sala carregada.
3. Quando essa sala carrega, `0x66C220` `D2Common.LinkLevelWarp` busca
   `Vis[n]`, aloca/gera o nível de destino e cria uma `RoomTile` (0x18 bytes,
   alocada em `0x66BE10`) na lista `Room2+0x4C`: 🔷 + ✅

   | Offset | Campo | Conf. |
   |---|---|---|
   | `+0x00` | `Room2*` de destino (no outro nível) | ✅ |
   | `+0x04` | próxima `RoomTile` | ✅ |
   | `+0x08` | 1 (habilitada?) | ✅ valor ⚠️ |
   | `+0x10` | registro de `lvlwarp`; `+0x00` = id | ✅ |
   | `+0x0C`, `+0x14` | ponteiros | ⚠️ `TODO(RE)` |

4. A sala ganha uma `PresetUnit` **tipo 5** com classe = id do `lvlwarp` (a
   posição exata da saída), e o cliente cria **unidades tipo 5** na mesma posição
   (cinco cópias vistas para a entrada do Covil). ✅
5. Registro de `lvlwarp`: 0x30 bytes; os valores lidos (id 2: −60, −60, 85, 100,
   5, 0, −1, 1, 1, 2, `'b'`) coincidem com a ordem pública das colunas de
   `lvlwarp.txt` (Id, SelectX, SelectY, SelectDX, SelectDY, ExitWalkX,
   ExitWalkY, OffsetX, OffsetY, LitVersion, Tiles, Direction). ✅ id; ⚠️ nomes
   dos demais.

### 4.3 Waypoints e objetivos de quest

São `PresetUnit` tipo 2 (objetos) ou tipo 1 (NPCs/superúnicos) em salas
carregadas, e as unidades correspondentes. O jogo não marca "objetivo" na
estrutura; a classificação vem dos dados (`objects.txt`, `monstats.txt`,
`superuniques.txt` da instalação do usuário). Classes confirmadas: 119 =
waypoint do Ato I, 267 = baú. ✅ A tabela binária de `objects` está em
`game-exe-1.14d-units.md` §5.

## 5. Automap

### 5.1 Estado

| Global (VA) | Significado | Âncora no INI | Conf. |
|---|---|---|---|
| `0x7A27E8` | automap **visível** (≠ 0). Tab alterna; vai a 0 com o menu do Esc aberto e volta ao fechar | `D2Client.DrawAutomapCallSite` (+2) | ✅ |
| `0x7A5150` | modo: **0 = tela cheia**, **1 = minimapa** (opção "Automap Size") | `D2Client.GetAutomapMode` (+1) | ✅ |
| `0x711254` | divisor da escala: **10** (modo 0) ou **20** (modo 1) | `D2Client.SetAutomapMode` (+20) | ✅ |
| `0x7A5198`, `0x7A519C` | deslocamento X, Y (int32), recalculado a cada quadro do automap | `D2Client.AutomapUnitToScreen` (+2, +13) | ✅ |
| `0x7A51C8`…`0x7A51D4` | retângulo de recorte: esquerda, topo, direita, base | `D2Client.AutomapClipCheck` (+2, +14, +26) | ✅ valores |
| `0x7A5210` | 3 = não desenhar; 1 e 2 deslocam o automap ¼ de tela para um lado (motivo provável: painel aberto) | — | 🔷 valores, ⚠️ motivo |
| `0x71146C`, `0x711470` | largura e altura da tela do jogo (800×600 vistos) | — | ✅ valor |

- O modo vem da opção do jogo (registro `AutoMapMode`), aplicada por
  `0x45A720` `D2Client.SetAutomapMode` (`ecx` = modo), que também grava o
  divisor. 🔷 Valores > 2 viram 0. O menu só alterna 0 ↔ 1. ✅
- Recorte observado: tela cheia (−16, −16, 800, 600); minimapa (−8, 96, …). ✅
- No modo 1 o minimapa fica no **canto superior esquerdo** do quadro 800×600. ✅

### 5.2 Mundo → tela do automap

Mesmo cálculo que o jogo usa para desenhar unidades (`0x45A860`), conferido ao
vivo nos dois modos: as posições calculadas para Gheed, Charsi, Kashya, Akara e
Warriv caíram sobre as marcas que o próprio jogo desenha para eles. ✅

```
cx = (x − y)·16        cy = (x + y)·8          (x, y em subtiles)
ax = cx / div − [0x7A5198] + 8
ay = cy / div − [0x7A519C] − 8                 (div = [0x711254]; divisão inteira com sinal)
```

- `(ax, ay)` são pixels do **quadro 800×600 do jogo** (o mesmo que o `on_present`
  recebe no renderizador GDI). ✅
- Para unidades, use `cx`/`cy` do caminho (`+0x08/+0x0C` dinâmico, `+0x04/+0x08`
  estático): é o que o jogo faz. Para predefinidas, use a fórmula. ✅
- O jogo descarta pontos fora do recorte (§5.1) antes de desenhar. 🔷
- A cruz do jogo (`0x45A7F0`) ainda soma **x − 1, y + 5** no modo 1. 🔷
- `TODO(RE)`: resolução diferente de 800×600 (D2GL, `-w` com outro tamanho) —
  só 800×600 foi observado.

### 5.3 Onde o jogo desenha o automap

- Quadro da partida: `0x456EE0` (sem chamador direto; vem por ponteiro).
  Ordem: mundo → **automap** → painéis/HUD/menus. 🔷
- Ponto de chamada `0x456F93`: se `[0x7A27E8] ≠ 0` e `[0x7A5210] ≠ 3`, chama
  `0x45AD60`. 🔷
- `0x45AD60` `D2Client.DrawAutomap` (sem argumentos): sai se `[0x7A0634]` ou o
  jogador forem nulos; calcula o layout e desenha as células (`0x459700` →
  `0x459440`), depois as unidades (`0x45AC90`: salas vizinhas do jogador →
  `0x45A860` por unidade) e os rótulos. 🔷
- O jogo já marca no automap: jogadores (cruz), monstros (cruz + nome se a opção
  "Show Names" estiver ligada, global `0x7113E8`) e o baú (rótulo). ✅ visto na
  cidade (NPCs com nome).

**Gancho recomendado:** desviar `0x45AD60` (a 1ª instrução, `cmp dword
[0x7A0634], 0`, tem 7 bytes e nenhum operando relativo: o desvio de 5 bytes não
precisa relocar nada) e desenhar **depois** de chamar a original. Assim as
marcas ficam acima do automap e **abaixo** dos painéis e menus, na thread
principal, e só quando o jogo desenha o automap. 🔷 (não testado)

**O `on_present` basta?** Para um primeiro mod, sim, com cuidado: ✅ o automap já
está no quadro quando o `on_present` roda, e a fórmula dá pixels desse quadro.
Limites: desenha **por cima** de painéis e menus abertos (inventário, Esc), então
teste `[0x7A27E8]` e recorte pelo retângulo `0x7A51C8…`; sob D2GL ele roda em
outra thread (copie os pontos no `on_tick`); a escala do D2GL não foi examinada.

## 6. Primitivas de desenho do jogo (opcional)

Desenham no quadro do jogo pela tabela de funções do renderizador ativo
(`[0x7C8CC0]`). Nenhuma foi executada por esta análise. 🔷

| VA | Convenção | Assinatura | Nota |
|---|---|---|---|
| `0x4F6380` | `__stdcall` | `void DrawLine(int x1, int y1, int x2, int y2, int cor, int alfa)` | entrada `+0xC0` da tabela; o jogo passa cor = índice de paleta e alfa = `0xFF`; 64 chamadores |
| `0x4F6300` | `__stdcall` | `void DrawRectangle(int esq, int topo, int dir, int base, int cor, int modo)` | não desenha se dir < esq ou base < topo; entrada `+0xB8`; 25 chamadores |
| `0x502320` | `__fastcall` | `void DrawText(const wchar_t* t (ecx), int x (edx), int y, int cor, int flag)` | 3 argumentos na pilha; 203 chamadores |
| `0x502EF0` | `__fastcall` | `int SetFont(int fonte)` | devolve a fonte anterior; o automap usa 1 e 6 |
| `0x501820` | `__fastcall` | `int GetTextWidth(const wchar_t*)` | |

Uso pelo jogo (marcador do automap `0x45A7F0`): 12 segmentos de `DrawLine` com
deslocamentos de uma tabela em `0x6D6638`. 🔷 `TODO(RE)`: significado exato de
`cor`/`modo` em cada renderizador (GDI × Glide/D2GL).

## 7. Resumo para o INI

`D2Client.DrawAutomap`, `D2Client.DrawAutomapCallSite`,
`D2Client.DrawAutomapUnit`, `D2Client.AutomapUnitToScreen`,
`D2Client.AutomapClipCheck`, `D2Client.DrawAutomapCross`,
`D2Client.GetAutomapMode`, `D2Client.SetAutomapMode`,
`D2Client.OnMapAddPacket`, `D2Client.OnMapRemovePacket`,
`D2Common.GetRoomsNear`, `D2Common.GetLevelIdFromRoom`,
`D2Common.Room2GetLevelId`, `D2Common.IsTownLevelId`, `D2Common.IsRoomInTown`,
`D2Common.GetLevel`, `D2Common.AllocLevel`, `D2Common.InitLevel`,
`D2Common.AddRoomData`, `D2Common.RemoveRoomData`,
`D2Common.GetLevelDefRecord`, `D2Common.GetLevelVis`,
`D2Common.GetLevelWarpIds`, `D2Common.LinkLevelWarp`, `D2Gfx.DrawLine`,
`D2Gfx.DrawRectangle`, `D2Win.DrawText`, `D2Win.SetFont`, `D2Win.GetTextWidth`.

## 8. Em aberto (`TODO(RE)`)

1. `AddRoomData`/`InitLevel` chamados por plugin: thread, custo, efeito no
   servidor local e liberação. Nada disso foi executado.
2. `RoomTile+0x08/+0x0C/+0x14`, `Act+0x10/+0x14`, `PresetUnit+0x00/+0x1C`.
3. Colunas do registro de `lvlwarp` além do id (só a ordem bate com o público).
4. Automap fora de 800×600 (D2GL) e o deslocamento por painel aberto (`0x7A5210`).
5. Cores/modos das primitivas por renderizador; nenhuma primitiva foi executada.
6. Atos II–V (níveis com exceções em `DrlgAct+0x90`, ex.: tumbas do Ato II) não
   foram examinados.
