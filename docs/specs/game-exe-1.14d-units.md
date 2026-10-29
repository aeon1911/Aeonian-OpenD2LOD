# Spec — Estado do cliente e unidades do `Game.exe` 1.14d

> **Tipo:** análise (fatos de interoperabilidade). Não contém código da
> Blizzard nem descompilado. Alimenta `aeonian/addresses/1.14d.ini` e o
> primeiro mod de teste (itens **C1** e **C2** de [`../ROADMAP.md`](../ROADMAP.md)).
> Companheira: [`game-exe-1.14d-levels-automap.md`](game-exe-1.14d-levels-automap.md)
> (nível, salas, saídas, automap, desenho).
>
> **Evidência:** `Game.exe` 1.14d (PE32, 3.618.792 bytes), SHA-256
> `631066c1649c4ea9ffe48bf97e24c00bca1f7a6759c21150f1a79982589adaaf`
> (conferido antes da análise). ImageBase `0x400000` e **sem ASLR**: VA = RVA +
> `0x400000`. Neste documento os endereços vêm como **VA**; o INI usa RVA.
>
> **Método (2026-10-29).**
> 1. *Estático:* desmontagem do `.text` com `objdump` e um índice próprio de
>    referências cruzadas (constantes absolutas e alvos de `call`), sem
>    descompilador. O Ghidra 12.2 do devkit **não rodou**: exige Java 25 e o
>    JDK do devkit é o 21. As âncoras foram strings de arquivo-fonte
>    (`.\UI\automap.cpp`, `.\DRLG\Preset.cpp`, `.\DRLG\DrlgRoom.cpp`…) e
>    funções pequenas com muitos chamadores.
> 2. *Execução:* o jogo real numa **cópia-espelho** (symlinks para os MPQs,
>    prefixo Wine de teste), dentro de `gamescope --backend headless`, com os
>    drivers de áudio do Wine desligados e `-ns`. Uma sonda externa (ferramenta
>    de análise, fora do repositório) lia memória com `ReadProcessMemory` e
>    mandava cliques/teclas à janela por `PostMessage`. Personagem de teste
>    (Bárbaro, normal) criado no prefixo de teste; ninguém tocou na instalação
>    nem nos saves do usuário. Wine 11.20, CachyOS.
> 3. *Validação visual:* só quadros da **própria janela do jogo** dentro do
>    gamescope headless (nunca a tela do usuário), para conferir a fórmula do
>    automap e navegar nos menus.
>
> **Legenda de confiança:** ✅ confirmado **em execução** (valor lido ao vivo);
> 🔷 confirmado **só no binário** (código lido, não executado/observado);
> ⚠️ hipótese, marcada `TODO(RE)`.

## 0. Avisos para quem implementa

- Várias funções do jogo têm **asserção**: com ponteiro nulo ou índice fora da
  faixa elas chamam o tratador de erro fatal e **o jogo fecha**. Onde isso vale,
  a tabela diz "asserta". Leia memória direto quando possível.
- Todas as estruturas abaixo pertencem à thread principal do jogo. Leia-as a
  partir de um evento da thread principal (`on_tick`) ou de um gancho no código
  do jogo, nunca de uma thread própria. O `on_present` sob D2GL roda numa
  **thread do D2GL** ([`game-exe-1.14d-hooking-surface.md` §4b](game-exe-1.14d-hooking-surface.md)):
  copie os dados no `on_tick` e só desenhe no `on_present`.
- Os globais de dados não têm bytes fixos. O INI registra **funções-âncora** cujos
  bytes (verificados) contêm o endereço do global; a coluna "âncora" diz em que
  deslocamento dos bytes o endereço aparece (little-endian).
- `Game.exe` 1.14d **nega `OpenProcess`** a outros processos (erro 5 sob Wine,
  mesmo com `SeDebugPrivilege`). Irrelevante para plugins (rodam dentro do
  processo); ferramentas externas precisam lançar o jogo e guardar o handle da
  criação. ✅

## 1. Estado do cliente

| Global (VA) | Tipo | Significado | Âncora no INI | Conf. |
|---|---|---|---|---|
| `0x7A6A70` | `Unit*` | jogador local; `NULL` fora da partida | `D2Client.GetPlayerUnit` (+1) | ✅ |
| `0x7A0634` | `Act*` | ato carregado no cliente; `NULL` fora da partida | `D2Client.DrawAutomap` (+2), `D2Client.ExitGameCleanup` (+1, +17) | ✅ |

- **Partida em andamento** ⇔ `[0x7A0634] != 0` **e** `[0x7A6A70] != 0`. Observado:
  ambos 0 no menu principal, ambos preenchidos na cidade, e ambos de volta a 0
  depois de "Save and Exit Game". ✅
- O `Act*` de `[0x7A0634]` é o mesmo de `jogador+0x1C`. ✅
- `0x463DD0` (`D2Client.GetPlayerUnit`): devolve `[0x7A6A70]` em `eax`, sem
  argumentos (qualquer convenção serve), 438 chamadores. Há uma cópia idêntica
  em `0x463DE0`. 🔷
- `0x463D90` grava o global (recebe a unidade em `ecx`) e `0x465870` o zera
  quando a unidade do jogador é liberada. 🔷 Não chame.
- `0x44C89F` (meio da rotina de saída da partida): libera o ato e zera
  `[0x7A0634]`. 🔷 Só âncora.
- Quando o jogador morre, a unidade continua em `0x7A6A70` (modo 17 =
  morto) até o renascimento na cidade. ✅

## 2. Tabelas de unidades

O cliente mantém **duas** tabelas de espalhamento, cada uma com 6 tipos × 128
baldes de `Unit*` (0x200 bytes por tipo, 0xC00 no total):

| Tabela (VA) | Conteúdo | Busca | Conf. |
|---|---|---|---|
| `0x7A5E70` | unidades vindas do servidor (jogador, monstros, NPCs, objetos, itens, tiles de saída) | `0x463990` `D2Client.FindServerSideUnit` | ✅ |
| `0x7A5270` | unidades **só do cliente** (na cidade: monstros classe 149 e objetos 40/41) | `0x4639B0` `D2Client.FindClientSideUnit` | ✅ |

- Balde: `tabela + tipo*0x200 + (id & 0x7F)*4`. Encadeamento pelo campo
  `+0xE4` da unidade. ✅
- A inserção (`0x463A50`) escolhe a tabela pelo bit `0x00200000` de `+0xC4`:
  ligado → `0x7A5270`. As unidades vistas em `0x7A5270` tinham `+0xC4 =
  0x600009`/`0x700020`. 🔷 + ✅
- `FindServerSideUnit(id, tipo)` / `FindClientSideUnit(id, tipo)`: `__fastcall`
  (`ecx` = id, `edx` = tipo), devolve `Unit*` ou 0. Se achar o id com outro tipo,
  **asserta**. 🔷
- **Para o mod (C2):** percorra `0x7A5E70`, tipo 1, os 128 baldes e as cadeias
  `+0xE4`. Na Charneca Sangrenta a tabela trazia só os monstros das salas
  carregadas; os NPCs da cidade **saem** dela quando as salas da cidade
  descarregam. ✅ Isso é o que o cliente sabe: monstros de salas não carregadas
  não existem no cliente.
- O próprio jogo, ao desenhar unidades no automap, **não** usa as tabelas: percorre
  as salas vizinhas da sala do jogador (`Room1+0x74`, `Unit+0xE8`). 🔷 (ver a outra spec)

Contagem observada (cidade do Ato I, 0x7A5E70): 1 jogador, 10 monstros (NPCs),
24 objetos, 8 itens; 0x7A5270: 9 monstros classe 149 e 221 objetos classes 40/41.
Na Charneca, apareceram unidades tipo 5 (tiles de saída). ✅

### 2.1 Tipos de unidade

`0` jogador, `1` monstro/NPC, `2` objeto, `3` míssil, `4` item, `5` tile (saída).
✅ para 0, 1, 2, 4 e 5 (vistos ao vivo); 🔷 para 3 (os testes de tipo do código
agrupam 0/1/3 como "caminho dinâmico" e 2/4/5 como "estático").

## 3. Layout da unidade

| Offset | Tam. | Campo | Conf. |
|---|---|---|---|
| `+0x00` | 4 | tipo (§2.1) | ✅ |
| `+0x04` | 4 | classe: linha de `monstats` (tipo 1), `objects` (2), `lvlwarp` (5), classe de personagem (0) | ✅ (Bárbaro = 4; waypoint = 119; baú = 267) |
| `+0x0C` | 4 | id da unidade (chave das tabelas) | ✅ |
| `+0x10` | 4 | modo (§3.2) | ✅ |
| `+0x14` | 4 | dados por tipo; tipo 1 → `MonsterData*` (§4) | ✅ |
| `+0x18` | 4 | índice do ato (0 = Ato I) | ✅ (só Ato I visto) |
| `+0x1C` | 4 | `Act*` | ✅ jogador/objetos/tiles; ⚠️ **0 nos monstros** observados |
| `+0x2C` | 4 | caminho: dinâmico (tipos 0/1/3) ou estático (2/4/5) | ✅ |
| `+0xC4` | 4 | flags (§3.3) | ✅ valores |
| `+0xC8` | 4 | flags 2 (valores vistos `0x80`, `0x02000080`) | ⚠️ sem semântica |
| `+0xE4` | 4 | próxima unidade no **balde** da tabela | ✅ |
| `+0xE8` | 4 | próxima unidade na **sala** (`Room1+0x74`) | ✅ |

### 3.1 Posição

**Caminho dinâmico** (jogador, monstro, míssil):

| Offset | Tam. | Campo | Conf. |
|---|---|---|---|
| `+0x02` | 2 | X em subtiles (parte inteira) | ✅ |
| `+0x06` | 2 | Y em subtiles | ✅ |
| `+0x08` | 4 | X de cliente (pixels isométricos) | ✅ |
| `+0x0C` | 4 | Y de cliente | ✅ |
| `+0x1C` | 4 | `Room1*` | ✅ |

`+0x00` e `+0x04` são DWORDs de precisão (16.16); os WORDs `+0x02`/`+0x06` são a
parte inteira. ✅ (`0x6488C0`/`0x648900` leem os WORDs). 🔷

**Caminho estático** (objeto, item, tile):

| Offset | Tam. | Campo | Conf. |
|---|---|---|---|
| `+0x00` | 4 | `Room1*` (0 para itens no inventário) | ✅ |
| `+0x04` | 4 | X de cliente | ✅ |
| `+0x08` | 4 | Y de cliente | ✅ |
| `+0x0C` | 4 | X em subtiles (no inventário: coluna da grade) | ✅ |
| `+0x10` | 4 | Y em subtiles (no inventário: linha) | ✅ |

- Coordenada de cliente: `cx = (x − y)·16`, `cy = (x + y)·8`, com `x`,`y` em
  subtiles de precisão. Com as partes inteiras, o jogo dá `cy` 8 a mais (centro
  do subtile). ✅ (jogador em (5653, 5523): `cx` = 2080, `cy` = 89416.)
- 1 tile = 5 subtiles. ✅

Funções do jogo (todas 🔷; leitura direta dos campos é equivalente e não
asserta):

| VA | Convenção | Assinatura | Nota |
|---|---|---|---|
| `0x45ADF0` | `__fastcall` | `int GetUnitX(Unit* ecx)` | 0 se o caminho for nulo |
| `0x45AE20` | `__fastcall` | `int GetUnitY(Unit* ecx)` | idem |
| `0x620870` | `__stdcall` | `void GetUnitCoords(Unit*, POINT* out)` | asserta com `Unit*` nulo |
| `0x620BB0` | `__stdcall` | `Room1* GetUnitRoom(Unit*)` | asserta; 774 chamadores |
| `0x620650` / `0x6206B0` | `__stdcall` | `int GetUnitClientX/Y(Unit*)` | assertam |
| `0x6488C0` / `0x648900` | `__stdcall` | `int PathGetX/Y(Path*)` | assertam |

### 3.2 Modos

- **Monstro:** sequência vista ao matar um monstro de classe 63 (`SI`): `1` → `2` →
  `5` → **`0`** (morrendo) → **`12`** (cadáver). O cadáver continua na tabela. ✅
- **Jogador:** `5` (parado na cidade), `1` (parado fora), `2` (andando), `3`
  (correndo), `17` (morto). ✅
- **Vivo (monstro):** modo ∉ {0, 12}. ✅ Os demais códigos seguem o padrão dos
  modos de animação do D2; só os listados foram observados.

### 3.3 Flags `+0xC4` (observado)

| Bit | Observação | Conf. |
|---|---|---|
| `0x00000002` | ligado em monstros vivos; **desligado** ao morrer (`0x0F` → `0x0D`) | ✅ |
| `0x00010000`, `0x00000020` | ligados no cadáver (`0x3001002D`) | ✅ valor |
| `0x00200000` | unidade só do cliente (tabela `0x7A5270`) | 🔷 + ✅ |

Semântica dos outros bits: `TODO(RE)`.

## 4. Monstros: `MonsterData` e `monstats`

`Unit+0x14` (tipo 1) → `MonsterData`:

| Offset | Tam. | Campo | Conf. |
|---|---|---|---|
| `+0x00` | 4 | ponteiro para o registro de `monstats` da classe | ✅ (igual a `base + classe*0x1A8`) |
| `+0x14` | 2 | WORD lido pelo código de nome do monstro | 🔷 sem semântica |
| `+0x16` | 1 | **flags de tipo** (abaixo) | 🔷 bits; ✅ valor 0 em monstros normais |
| `+0x1C` | 10 | bytes de modificadores (lidos como lista terminada em 0) | 🔷 ⚠️ |
| `+0x2C` | 4 | ponteiro (não nulo nos vistos) | ⚠️ `TODO(RE)` |

Flags de `+0x16`, testadas pelo cliente depois de `cmp [unit],1` e `unit+0x14`
(`0x4AC8D9`, `0x4ACC98`, `0x5A397A`, `0x5A428E`, `0x5A0C14`):

| Bit | Tipo | Conf. |
|---|---|---|
| `0x02` | superúnico | 🔷 |
| `0x04` | campeão | 🔷 |
| `0x08` | único | 🔷 |
| `0x10` | lacaio (minion) | ⚠️ `TODO(RE)`: nenhum teste direto achado; a pista é a numeração pública da 1.10 (projeto D2MOO). Confirmar com um bando (ex.: Corpsefire, no Covil do Mal). |
| nenhum | normal | ✅ (todos os monstros vistos tinham 0) |

**Tabela `monstats`** (DataTables):

- `[0x744304]` = ponteiro para os DataTables (`0x96BC30` observado, dentro do
  `.data`). ✅
- `DataTables+0xA78` = base dos registros; `DataTables+0xA80` = quantidade (734
  observados); registro de **0x1A8** bytes. ✅ Âncora:
  `D2Common.MonStatsRecordAccess` (padrão inline com `0x744304`, `0xA80`,
  `0x1A8`, `0xA78`). 🔷
- Registro `+0x00` (WORD) = id da linha ✅; `+0x0C` (DWORD) = flags:

| Classe (código) | `+0x0C` | |
|---|---|---|
| 5 (`ZM`) | `0x02418803` | monstro hostil |
| 63 (`SI`) | `0x12018001` | monstro hostil (morto ao vivo) |
| 147 Gheed, 148 Akara, 154 Charsi, 155 Warriv | `0x03010703` | NPC |
| 150 Kashya (`RC`) | `0x02010703` | NPC |
| 152 (`RG`), sem nome no automap | `0x02018502` | NPC |
| 149 (`CK`), 179 (`CW`) | `0x02010402`, `0x02010403` | ambiente da cidade (não NPC) |

  Leitura dos bits a partir do observado ✅: `0x100` ligado em **todos** os NPCs
  da cidade e em mais nada; `0x400` em tudo que fica na cidade; `0x8000` nos
  dois monstros hostis, ausente nos NPCs e nas classes 149/179. Os nomes (NPC, inTown, killable)
  são a hipótese coerente com a ordem pública das colunas de `monstats.txt`. ⚠️

**Filtro "inimigo" proposto para o C2** (todos os campos ✅):
`unit+0x00 == 1` **e** modo ∉ {0, 12} **e** `(monstats+0x0C & 0x100) == 0`
**e** `(monstats+0x0C & 0x8000) != 0`. Mercenários e invocações do jogador não
foram examinados (`TODO(RE)`: dono/"pet").

## 5. Objetos: tabela `objects`

- `0x640E90` `D2Common.GetObjectsTxtRecord`: `__stdcall(int classe)` →
  registro; **asserta** fora da faixa. 99 chamadores. 🔷
- Base `[0x96D470]`, quantidade `[0x96D474]`, registro de **0x1C0** bytes. 🔷
  (âncora `D2Common.ObjectsTxtBase`). Para classificar objetos (waypoint, baú,
  objeto de quest) use as colunas de `objects.txt` da instalação do usuário; os
  offsets das colunas no registro binário são `TODO(RE)`.
- Classes vistas ao vivo no Ato I: 119 waypoint e 267 baú (rotulados pelo
  próprio jogo: o waypoint pela posição, o baú pelo rótulo "Stash" do automap);
  40/41/42/65 só na tabela do cliente; 35, 36, 37, 39, 78, 385 sem
  classificação. ✅

## 6. Resumo para o INI

Símbolos registrados (todos com `bytes` conferidos no binário):
`D2Client.GetPlayerUnit`, `D2Client.FindServerSideUnit`,
`D2Client.FindClientSideUnit`, `D2Client.GetUnitX`, `D2Client.GetUnitY`,
`D2Client.ExitGameCleanup`, `D2Common.GetUnitCoords`, `D2Common.GetUnitRoom`,
`D2Common.GetUnitClientX`, `D2Common.GetUnitClientY`, `D2Common.PathGetX`,
`D2Common.PathGetY`, `D2Common.MonStatsRecordAccess`,
`D2Common.GetObjectsTxtRecord`, `D2Common.ObjectsTxtBase`. Offsets de estrutura
ficam só nesta spec.

## 7. Em aberto (`TODO(RE)`)

1. Bit de lacaio (`0x10`) em `MonsterData+0x16`: confirmar ao vivo.
2. `Unit+0x1C` nulo nos monstros: usar `[0x7A0634]` ou `jogador+0x1C` como ato.
3. Semântica de `+0xC4`/`+0xC8` além dos bits da tabela.
4. Offsets das colunas do registro de `objects` (0x1C0) e de `monstats` além de
   `+0x00`/`+0x0C`.
5. Mercenários/invocações: como distingui-los (dono) dos monstros hostis.
6. Tudo foi observado no Ato I, dificuldade normal, um jogador. Atos II–V e
   multijogador (TCP/IP) não foram examinados.
