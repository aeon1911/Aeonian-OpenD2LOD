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
> **Segunda análise (2026-10-30).** Mesmo binário (SHA-256 conferido), mesmas
> ferramentas, de novo numa cópia-espelho invisível e muda (nenhum fluxo de áudio
> do jogo em `pactl list sink-inputs`). Para ver os Atos II–V e as três
> dificuldades, o save do personagem de teste foi editado **só no prefixo de
> teste**: nível, atributos, Teletransporte, Hidra, waypoints, mercenário. O mapa
> usou a mesma semente em todas as partidas. As colunas de tabelas foram
> identificadas comparando os registros lidos da memória com os `.txt` extraídos
> de `d2exp.mpq` para o scratchpad. Nada disso está no repositório. Rótulos citados
> como "rótulo do jogo" são o texto que o próprio jogo mostra ao pôr o cursor na
> unidade, lido em quadros da janela do jogo.
>
> **Terceira análise (2026-10-31).** Mesmo binário, de novo numa cópia-espelho invisível e
> muda, com personagem de teste. Tabelas lidas da memória do processo, sem extração para
> arquivo. Um plugin descartável (fora do repositório) percorreu as `PresetUnit` de todos os
> níveis dos cinco atos, em Normal (o "censo"); outra sonda visitou e operou objetos, e leu
> os monstros vivos. Sementes de mapa: `0x6C838D4A` e, para conferir o que depende do mapa,
> `0x5A5A1234`. Os módulos de quest do binário foram situados pelas strings de arquivo-fonte
> `D2Game/Quests/aNqM.cpp`. Rótulos são os que o jogo mostra, lidos em quadros da janela do
> jogo.
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
| `+0x18` | 4 | índice do ato (0 = Ato I) | ✅ (valores 0 a 4, Atos I–V) |
| `+0x1C` | 4 | `Act*` | ✅ jogador/objetos/tiles; **0 nos monstros**: para o ato, use `[0x7A0634]` ou `jogador+0x1C` ✅ |
| `+0x2C` | 4 | caminho: dinâmico (tipos 0/1/3) ou estático (2/4/5) | ✅ |
| `+0x5C` | 4 | lista de stats da unidade (§7.2) | ✅ |
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
| `0x10000000`, `0x20000000` | variam com o tempo na mesma unidade viva (`0x0F`, `0x1000000F`, `0x3000000F`) | ✅ valor |
| `0x00000200` | ligado no mercenário (`0x3000020F`); as hidras tinham `0x10000009` | ✅ valor |

`TODO(RE)`: semântica dos demais bits de `+0xC4` e de `+0xC8`. O mod não precisa
deles: **não use `+0xC4` para saber o dono**; use §7.

## 4. Monstros: `MonsterData` e `monstats`

`Unit+0x14` (tipo 1) → `MonsterData`:

| Offset | Tam. | Campo | Conf. |
|---|---|---|---|
| `+0x00` | 4 | ponteiro para o registro de `monstats` da classe | ✅ (igual a `base + classe*0x1A8`) |
| `+0x14` | 2 | semente do nome: ≠ 0 em único, superúnico e campeão; 0 em lacaios e normais | ✅ valor |
| `+0x16` | 2 | **flags de tipo** (abaixo). O jogo as acessa como WORD; os bits de tipo estão no byte baixo | ✅ |
| `+0x1C` | 10 | modificadores (bytes, lista terminada em 0). Campeões vistos: 16 ou 38. Lacaios repetem os do chefe, exceto o 22 dos superúnicos | ✅ valor |
| `+0x26` | 2 | linha de `superuniques`; só vale com o bit `0x02` | ✅ (0 para Bishibosh, 17 para Dark Elder, conferidos pelo rótulo do jogo ou pela classe) |
| `+0x2C` | 4 | ponteiro (não nulo nos vistos) | ⚠️ `TODO(RE)` |

Flags de `+0x16`, confirmadas ao vivo com o rótulo que o próprio jogo mostra na
unidade:

| Byte | Tipo | Exemplo observado | Conf. |
|---|---|---|---|
| `0x00` | normal | a maioria | ✅ |
| `0x08` | único | classe 131, rótulo "Rot Fist the Cold", "Undead Fire Enchanted". Atos I–V; Normal, Pesadelo, e Inferno no Ato V | ✅ |
| `0x10` | **lacaio** | classe 19, rótulo "Fallen", "Demon Minion"; sempre em volta de um chefe `0x08` ou `0x0A`. Atos I–V | ✅ |
| `0x0A` | **superúnico** (`0x02` + `0x08`) | classe 9, `+0x26` = 17, rótulo "Dark Elder". Atos I, II e V, Normal | ✅ |
| `0x0C` | **campeão** (`0x04` + `0x08`) | classe 85, modificador 16, rótulo "Champion Night Tiger", em grupo de 4. Atos I e II; Normal e Pesadelo | ✅ |

**Atenção:** superúnicos e campeões **também têm o bit `0x08`**. Classifique nesta
ordem: `0x02` → superúnico; `0x04` → campeão; `0x08` → único; `0x10` → lacaio;
senão normal. Testar `0x08` primeiro marcaria os três como único. Um chefe nunca
foi visto com `0x10`, nem um lacaio com `0x02`, `0x04` ou `0x08`. ✅ observado

Referências no binário 🔷: `0x4AC7E0` `D2Client.MonsterHasTypeFlag`
(`__fastcall`, `ecx` = `Unit*`, `edx` = máscara, sem asserção); `0x4AC7C0`
`D2Client.GetSuperUniqueIndex`. O cliente recebe as flags pelo pacote de criação do
monstro. Testes com máscara fixa estão em `0x4AC8D9` e `0x5A03B2` (`0x02`),
`0x4ACC98` e `0x5A0C14` (`0x04`) e `0x5A397A` (`0x08`). **Nenhum teste direto de
`0x10` foi achado** nas leituras de `+0x16`; o significado vem do rótulo do jogo.

Bits `0x80` e `0x100` do WORD têm setters próprios (`0x4ADAA4`, `0x4AC854`), mas não
apareceram nos monstros vistos. `TODO(RE)`.

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
  Uma comparação com `monstats.txt` deu acerto parcial para `0x400` (InTown),
  `0x8000` (Killable) e `0x200` (Interact), mas o arquivo tem 569 linhas úteis e a
  tabela 734, então o alinhamento não é confiável. `TODO(RE)`: colunas de
  `monstats` além de `+0x00`/`+0x0C`.

**Filtro "inimigo" para o C2 (corrigido em 2026-10-31):**

```
inimigo ⇔ unit+0x00 == 1  e  modo ∉ {0, 12}
          e (monstats+0x0C & 0x100) == 0  e  (monstats+0x0C & 0x8000) != 0
          e GetUnitAlignment(unit) != 2                 (§7.2)
```

**Consequência:** a lista de aliados (§7.1) deixa de ser necessária para saber se um monstro é
hostil. Continua necessária para saber o **dono** e para marcar o aliado como aliado.

O filtro anterior, só com os bits de `+0x0C`, estava errado. Os dois falsos positivos vistos são
aliados sem nó na lista:

| Classe | Código | `+0x0C` | Onde | Alinhamento | Lista de aliados | Conf. |
|---|---|---|---|---|---|---|
| **359** | `IW` | `0x02018408` | cidade do Ato III (3 predefinidas no nível 75) | 2 | ausente | ✅ Normal, Pesadelo, Inferno |
| **522** | `0A` | `0x0205840B` | nível 112, em volta do jogador | 2 | ausente | ✅ Normal |

**O que distingue a classe 359:** o estado 105 com o stat 172 = 2, igual aos NPCs. O servidor o
define com `0x5543B0` `D2Game.SetUnitAlignment`. 🔷 Nos registros de `monstats` não há coluna
que dê esse valor: fora os campos de identificação, nenhum byte separou amigos de hostis nas 37
classes comparadas. ✅

**Conferência, 123 classes vistas vivas** (Normal, Atos I–V; a cidade do Ato III também em
Pesadelo e Inferno). Por classe, numa leitura ao vivo: ✅

| Passa nos bits? | Alinhamento | Classes | Situação |
|---|---|---|---|
| sim | 0 (sem estado 105 ou sem lista de estados) | 95 | hostis; o filtro as mantém |
| sim | 2 | 2 (359, 522) | os falsos positivos acima; o filtro as recusa |
| não | 2 | 18 | NPCs |
| não | 0 | 8 | ambiente (como 517 e 567–569) |

- Nas invocações e mercenários vistos (§7.1), os bits passam e o alinhamento é 2. ✅
- **O estado já está lá na primeira leitura.** Em 12 unidades da cidade do Ato III, lidas de
  cerca de 100 em 100 ms enquanto o jogador se aproximava, nenhuma apareceu sem ele. ✅
- **Por varredura da tabela:** 626 das 734 classes passam nos bits. Das 43 com `0x400`, as 13 vistas
  vivas são amigas (359, 522, mercenário e invocações); nenhuma hostil apareceu com esse bit. Se
  `GetUnitAlignment` não estiver disponível, recusar `0x400` deu o mesmo resultado nas classes
  vistas. ⚠️ As outras 30 nunca foram vistas vivas.
- Entre as classes com predefinida nas cinco cidades, só a 359 passa nos bits. ✅
- Uma `PresetUnit` tipo 1 pode trazer classe **≥ 734**, fora da tabela de `monstats` (64 valores
  distintos no censo, de 734 a 836). ✅ Nunca a use como índice sem conferir o limite.
- `TODO(RE)`: alinhamento 1. O servidor o grava em alguns caminhos (🔷), mas ele nunca foi visto
  vivo. O filtro acima trata 1 como hostil. Também não foram testados monstro convertido e outro
  jogador na partida.

O próprio jogo, ao desenhar o automap (`0x459BC0` `D2Client.AutomapClassifyUnit`), escolhe a cor
de monstro pelo bit `0x200` de `+0x0C` (fora as classes 537–539) e pela relação de aliado
(§7.1). 🔷

## 5. Objetos: tabela `objects`

- `0x640E90` `D2Common.GetObjectsTxtRecord`: `__stdcall(int classe)` →
  registro; **asserta** fora da faixa. 99 chamadores. 🔷
- Base `[0x96D470]`, quantidade `[0x96D474]` (573 observados), registro de
  **0x1C0** bytes. ✅ (âncoras `D2Common.GetObjectsTxtRecord` e
  `D2Common.ObjectsTxtBase`). A tabela é a do jogo do usuário, carregada na memória:
  **leia-a em tempo de execução**; o mod não precisa de lista de classes.

### 5.1 Layout do registro

Método: 256.704 bytes lidos da memória, comparados coluna a coluna com o
`objects.txt` extraído para o scratchpad. "Acerto" é quantas das 571 linhas
comparadas bateram. As poucas diferenças vêm de linhas que divergem entre o texto e
a tabela carregada. As colunas em negrito também foram conferidas no código ou ao
vivo.

| Offset | Tipo | Coluna | Acerto | Conf. |
|---|---|---|---|---|
| `+0x000` | `char[64]` | nome interno | leitura | ✅ |
| `+0x040` | `wchar_t[64]` | descrição | leitura | ✅ |
| `+0x0C0` | `char[3]` | token | leitura | ✅ |
| `+0x0C3` | byte | SpawnMax | 569 | ✅ |
| `+0x0C4` | byte[8] | Selectable0–7 | 569 | ✅ |
| `+0x0CC` | int32 | TrapProb | 569 | ✅ |
| `+0x0D0`, `+0x0D4` | int32 | SizeX, SizeY | 570 | ✅ |
| `+0x13A` | byte | IsDoor | 571 | ✅ |
| **`+0x167`** | byte | **SubClass** (abaixo) | 569 | ✅ + 🔷 |
| `+0x168` | int32 | NameOffset | 569 | ✅ |
| `+0x170` | byte | Act (máscara) | 569 | ✅ |
| `+0x171` | byte | Lockable | 569 | ✅ |
| `+0x178` | int32[8] | Parm0–7 | 568–571 | ✅ |
| `+0x1B1` | byte | InitFn | 571 | ✅ |
| `+0x1B2` | byte | PopulateFn | 569 | ✅ |
| **`+0x1B3`** | byte | **OperateFn** (abaixo) | 566 | ✅ + 🔷 |
| **`+0x1B4`** | byte | **ClientFn** | 571 | ✅ + 🔷 (`0x4BDEEC`) |
| `+0x1B8` | int32 | OpenWarp | 571 | ✅ |
| **`+0x1BC`** | int32 | **AutoMap**: célula do automap; ≠ 0 faz o próprio jogo desenhar o objeto | 571 | ✅ + 🔷 (`0x458E40`) |

**`SubClass` (`+0x167`), a classificação pronta do jogo:**

| Bit | Grupo | Conf. |
|---|---|---|
| `0x01` | santuário | 🔷 (`0x621B00` `D2Common.IsObjectShrine`) + ✅ valor |
| `0x02` | obelisco | ✅ valor |
| `0x04` | **portal de jogador ou portal vermelho** (só as classes 59 e 60) | 🔷 (`0x620A4C`) + ✅ |
| `0x08` | baú, urna, cesto e afins | ✅ valor |
| `0x10` | portal de nível (classes 298 e 342) | ✅ valor |
| `0x20` | poço | ✅ valor |
| `0x40` | **waypoint** (as 16 classes de waypoint, de todos os atos) | 🔷 (`0x66ADCE`) + ✅ |
| `0x80` | passagem secreta, cama, armadilha | ⚠️ grupo heterogêneo |

**`OperateFn` (`+0x1B3`):** índice na tabela de funções `0x732D18`, com 0x65
entradas. O servidor despacha por ela em `0x584504` (âncora
`D2Game.ObjectOperateDispatch`). 🔷 Valores confirmados:

| Valor | Uso | Conf. |
|---|---|---|
| 23 | waypoint (`0x584E30`, procura o waypoint do nível) | ✅ valor nas 16 classes; ✅ waypoints abertos em todos os atos |
| 15 | portal de jogador e vermelho (classes 59, 60; `0x584870`) | ✅ |
| 34 | portal do Santuário Arcano (classe 298) | ✅ valor |
| 46 | portal da classe 342 (§5.3) | ✅ |
| 43, 66, 70, 71, 73 | outras passagens por objeto (classes 100, 547, 563, 564, 566, 569) | 🔷 valor; não usadas ao vivo ⚠️ |

**Objetos de quest não têm coluna própria, mas `OperateFn` os identifica:** ver §5.4.

**Para o mod (C2):**
- waypoint ⇔ `registro+0x167 & 0x40`, com o registro lido por `Unit+0x14` →
  `ObjectData+0x00` (§5.2) ou pela classe; vale também para `PresetUnit` tipo 2
  com a classe em `+0x04`. Classes abaixo de `[0x96D474]` apenas: o jogo testa o
  limite antes de consultar. ✅
- As classes de waypoint vistas ao vivo nos Atos I–V (119, 156, 237, 238, 324, 402,
  496, 511) têm esse bit e `OperateFn` 23. ✅
- As classes vistas no Ato I sem classificação (35, 36, 37, 39, 78, 385) trazem a
  própria `SubClass` e `OperateFn` no registro; não foram classificadas uma a uma.

### 5.2 `ObjectData` (`Unit+0x14`, tipo 2)

| Offset | Tam. | Campo | Conf. |
|---|---|---|---|
| `+0x00` | 4 | ponteiro para o registro de `objects` (igual a `base + classe*0x1C0`) | ✅ |
| `+0x04` | 1 | **nível de destino**, em portais (linha de `levels.txt`) | ✅ |
| `+0x05` | 1 | flags do portal (3 no lado da cidade, 0 nos demais vistos) | ✅ valor ⚠️ |
| `+0x28` | 16 | nome do dono, ASCII, em portais de jogador | ✅ |

### 5.3 Portais que são objetos

| Classe | Objeto | Como reconhecer | Destino | Conf. |
|---|---|---|---|---|
| **59** | portal da cidade (azul) | `SubClass & 0x04`, classe 59; dono em `ObjectData+0x28` | `ObjectData+0x04` | ✅ aberto com pergaminho: na Planície Fria o destino é 1, no lado da cidade é 3 |
| **60** | portal vermelho | `SubClass & 0x04`, classe 60; sem dono | `ObjectData+0x04` | ✅ em Frigid Highlands: destino 125, rótulo do jogo "Abaddon" |
| 298 | portal do Santuário Arcano | `SubClass & 0x10` | pelo **nível atual**: 74 → 54, 54 → 74. `ObjectData+0x04` = 0 | ✅ usado nos dois sentidos |
| 342 | portal do nível 102 | `SubClass & 0x10` | pelo **nível atual**: 102 → 103. `ObjectData+0x04` = 0 | ✅ com a missão 22 concluída no save; sem ela o clique não faz nada, e o objeto aparece em modo 2 nos dois casos |

- Na cidade, o portal azul aparece na tabela do servidor, tipo 2, modo 2. ✅
- A classe 298 existe só nos níveis 54 e 74, e a 342 só no 102. ✅ censo. Os destinos estão fixos
  nos tratadores `D2Game.ArcanePortalOperate` e `D2Game.DurancePortalOperate`. 🔷 + ✅
- A classe 100 (`OperateFn` 43, `D2Game.ObjectWarpOperate`) leva ao nível 73; a 377, que divide o
  tratador, usa `ObjectData+0x04`. 🔷 Nenhuma das duas tem predefinida no censo nem foi vista viva.
- `TODO(RE)`: outros portais vermelhos
  (Tristram, vacas, Nihlathak) não foram abertos, mas não há outra classe de
  `SubClass & 0x04` além de 59 e 60. 🔷

### 5.4 Objetivos de quest

**Não há coluna, mas há um critério único e bom: `OperateFn` (`+0x1B3`) nos valores abaixo.** Ele
vem de onde o tratador está no binário. Os tratadores da tabela `0x732D18` que ficam no código
dos módulos `D2Game/Quests/aNqM.cpp` (referências às strings entre `0x586EBB` e `0x5BDAD5`) são
exatamente os 35 valores desta seção; todos os demais estão até `0x5869F0`, no código de
`.\OBJECTS\ObjMode.cpp`. 🔷 Um mod lê o `OperateFn` do registro; **não precisa de lista de
classes**.

**Objetivo: use este conjunto.** `OperateFn` ∈ {6, 9, 10, 12, 21, 24, 25, 28, 31, 39, 40, 41, 42,
49, 52, 53, 54, 55, 56, 57, 58, 59, 65, 67}

Níveis do censo, semente `0x6C838D4A`. Cada classe teve **uma** predefinida, em **um** nível (as 5
pedras e os 5 selos são classes distintas). "Clique sem efeito": o objeto não mudou de modo, porque
falta item ou passo da missão no personagem de teste.

| `OperateFn` | Classes | Módulo vizinho | Nível | Em jogo |
|---|---|---|---|---|
| 6 | 8 | a1q5 | 4 | ✅ modo 0 → 2 ao operar |
| 9 | 17–21 (pedras) | a1q4 | 4 | ✅ clique sem efeito |
| 10 | 26 | a1q4 | 38 | 🔷 + censo |
| 12 | 30 | a1q4 | 5 | ✅ rótulo "Tree of Inifuss"; modo 0 → 1, item classe 524 no chão |
| 21 | 108 | a1q3 | 28 | ✅ clique sem efeito |
| 24 | 149 | a2q3 | 61 | 🔷 + censo |
| 25 | 152 | a2q6 | **a tumba verdadeira** (abaixo) | ✅ clique sem efeito |
| 39, 40, 41 | 354, 355, 356 | a2q1–a2q3 | 60, 49, 64 | 🔷 + censo |
| 42 | 357 | a2q4 | 74 | ✅ clique sem efeito |
| 28 | 193 | a3q1 | 94 | 🔷 + censo |
| 31 | 252 | a3q3 | 78 | ✅ modo 0 → 2; visto no quadro |
| 53 | 404 | a3q5 | 83 | ✅ clique sem efeito |
| 57, 58, 59 | 405, 407, 406 | a3q2–a3q3 | 93, 85, 91 | 🔷 + censo |
| 49 | 376 | a4q3 | 107 | ✅ clique sem efeito |
| 52, 54, 55, 56 | 393 e 395, 392, 394, 396 (selos) | a4q2–a4q3 | 108 | 🔷 + censo; o teste não chegou ao nível |
| 65 | 546 | a5q5 | 120 | ✅ modo 0 → 2 |
| 67 | **558** | a5q3 | 114 | ✅ unidade, clique sem efeito; **sem predefinida** (abaixo) |

Visto em jogo, nos cinco atos: 11 valores dos 24, em 15 classes. Os outros 13 têm só o tratador e a
predefinida.

**Código de quest, mas não é objetivo (falsos positivos do critério cru).** Não os inclua; são
passagens e peças de roteiro:

| `OperateFn` | Classes | O que é | Conf. |
|---|---|---|---|
| 44, 45 | 366, 367 | escada e alavanca do esgoto, nível 92 | 🔷 + censo |
| 62, 63, 64 | 474, 475, 476 | estátuas, nível 120; passam ao modo 4 quando o altar 546 é operado | ✅ |
| 66, 71 | 547, 564 | passagens do nível 120 | 🔷 + censo |
| 70 | 563, 569 | passagens; 563 no nível 131 | 🔷 + censo |
| 73 | 566 | passagem | 🔷 valor |
| 69, 72 | 561, 565 | não selecionável; fim do jogo | 🔷 valor |

**Falsos negativos: ligados a missão e fora do conjunto.**

| Classe | `OperateFn` | Situação | Conf. |
|---|---|---|---|
| **460** | 0 | a **predefinida** que marca o lugar da 558, no nível 114 | ✅ as duas na mesma posição |
| **473** | 0 | 3 predefinidas no nível 111, gaiolas; `InitFn` em código de quest; `AutoMap` 310 | 🔷 + censo |
| 100, 298, 342 | 43, 34, 46 | passagens que dependem de missão; o tratador fica em `ObjMode.cpp` | §5.3 |
| 268 | 33 | nível 38; `SubClass` 0x08; não é objetivo de missão | 🔷 + censo |
| 251, 378, 379, 382 | 0 | nível 75, peças de roteiro; `InitFn` em código de quest | 🔷 + censo |

- **Por objeto no chão não dá para saber tudo:** missões que pedem matar um monstro (Radament,
  Duriel e afins) ou limpar um nível não têm objeto. O mod já mostra superúnicos (§4).
- **`InitFn` (`+0x1B1`, tabela `0x731BC0`) não serve de critério:** dá positivo em dezenas de classes
  sem `OperateFn`, inclusive caixas de cena, e não pega 26, 108, 149, 193, 357, 405–407.
- **`AutoMap ≠ 0` também não serve.** Medido no censo: 63 classes e 194 predefinidas com ele,
  incluindo santuário, poço, waypoint, baú do jogador, alçapão, escada e portal. E faltam 26, 149,
  152 e 376. ✅

**A tumba verdadeira do Ato II** é o nível que tem a predefinida da classe 152. ✅ Na semente
`0x6C838D4A` é o 71; na `0x5A5A1234` é o 69; o nível 73 tem a classe 153 nas duas. Saber isso de
longe exige carregar as salas (spec de níveis §3.1).

**Mostrar de longe.** Fora a 558, todas as classes do conjunto têm `PresetUnit` tipo 2, então valem
as duas fontes do mod: a predefinida (sala carregada) e a unidade. Para a 558, leia a unidade e use a
460 como predefinida. ✅

**Missão já concluída.** O modo da unidade ajuda, sem ser garantia. Com as palavras de missão do save
marcadas como concluídas (`0x1001` em cada missão, formato público do `.d2s`, só no prefixo de teste),
sem nenhum clique: ✅

| Classe | Modo | |
|---|---|---|
| 8 | 3 | |
| 17 | 2 | |
| 30 | 1 | |
| 546, 474 | 0 | ⚠️ altar e estátua não mudaram. Rodada seguinte à marcação, sem conferir que as palavras seguiam no save |

Portanto: modo ≠ 0 indica objeto já usado; modo 0 **não** prova missão pendente.

**Estado da missão.** `0x65C310` `D2Common.GetQuestState` é `__stdcall(QuestRecord*, int missão, int
estado)` e lê o bit `missão*16 + estado`. 🔷 Asserta com registro nulo. Os tratadores passam números
fixos, e eles são os índices de palavra do `.d2s`: 4 (`OperateFn` 9, 10), 11 (24), 17 (28), 19 (31),
21 (53), 22 (46), 26 (73), 27 (49), 39 (69), 40 (72). 🔷 A 22 foi confirmada: marcar a palavra 22 no
save liberou a classe 342. ✅

**No cliente esse registro não ajuda.** `PlayerData+0x10`, `+0x14` e `+0x18` do jogador local apontam
para `{buffer, 0x300}`, um por dificuldade, mas o buffer lido estava **zerado** com a palavra 22
marcada no save. ✅ `TODO(RE)`: de onde o painel de missões do cliente tira o estado.

## 6. Resumo para o INI

Símbolos registrados (todos com `bytes` conferidos no binário):
`D2Client.GetPlayerUnit`, `D2Client.FindServerSideUnit`,
`D2Client.FindClientSideUnit`, `D2Client.GetUnitX`, `D2Client.GetUnitY`,
`D2Client.ExitGameCleanup`, `D2Common.GetUnitCoords`, `D2Common.GetUnitRoom`,
`D2Common.GetUnitClientX`, `D2Common.GetUnitClientY`, `D2Common.PathGetX`,
`D2Common.PathGetY`, `D2Common.MonStatsRecordAccess`,
`D2Common.GetObjectsTxtRecord`, `D2Common.ObjectsTxtBase`. Da segunda análise:
`D2Client.MonsterHasTypeFlag`, `D2Client.GetSuperUniqueIndex`,
`D2Client.FindPetEntry`, `D2Client.AddPetEntry`, `D2Client.RemovePetEntry`,
`D2Client.OnPetPacket`, `D2Client.OnMercPacket`, `D2Client.GetObjectClientFn`,
`D2Common.GetUnitAlignment`, `D2Common.IsObjectShrine`,
`D2Common.ObjectSubClassWaypointAnchor`, `D2Game.ObjectOperateDispatch`,
`D2Game.WaypointOperate`, `D2Game.PortalOperate`. Da terceira análise:
`D2Common.GetQuestState`, `D2Game.SetUnitAlignment`, `D2Game.ObjectInitDispatch`,
`D2Game.ArcanePortalOperate`, `D2Game.DurancePortalOperate`, `D2Game.ObjectWarpOperate`,
`D2Client.AutomapClassifyUnit`, `D2Client.GetPetRelation`. Offsets de estrutura ficam só nesta
spec.

## 7. Dono da unidade: mercenários e invocações

### 7.1 Lista de aliados do cliente

**`[0x7BB5BC]`** é a cabeça de uma lista encadeada com um nó para cada mercenário ou
invocação conhecida pelo cliente. Âncora: `D2Client.FindPetEntry`, endereço nos
bytes `+1`. ✅

| Offset | Tam. | Campo | Conf. |
|---|---|---|---|
| `+0x00` | 4 | classe (linha de `monstats`) | ✅ |
| `+0x04` | 4 | tipo do aliado (tabela abaixo) | ✅ |
| `+0x08` | 4 | **id da unidade aliada** (igual a `Unit+0x0C`) | ✅ |
| `+0x0C` | 4 | **id da unidade dona** (no jogador local, o `Unit+0x0C` dele) | ✅ |
| `+0x10` | 4 | 1 no mercenário, 0 nas hidras | ✅ valor ⚠️ |
| `+0x1C` | 4 | 100 ao criar; provável vida em % | 🔷 ⚠️ |
| `+0x24`, `+0x28` | 4 | só no mercenário: semente e id do nome | ✅ valor |
| `+0x30` | 4 | próximo nó, 0 no fim | ✅ |

Nó de 0x34 bytes, alocado em `0x478B4C`. 🔷

- **Para o mod:** monstro aliado ⇔ `Unit+0x0C` aparece em algum nó `+0x08`. Dono é o
  jogador local ⇔ nó `+0x0C` igual ao `Unit+0x0C` do jogador. Percorra a lista com
  limite. `0x478A90` `D2Client.FindPetEntry` (`__fastcall`, `ecx` = id, devolve o nó
  ou 0, sem asserção) faz a busca. 🔷
- **Compare só com unidades tipo 1.** Os ids são por tipo: o mercenário visto tinha id
  1, igual ao do jogador. ✅
- Visto ao vivo, em Inferno, no Ato I: mercenário de classe 271 (tipo 7), dono =
  jogador; três hidras de classes 351, 352 e 353 (tipo 19), ids 0x13–0x15, dono =
  jogador. ✅

Como a lista é mantida (🔷):

| Pacote S→C | Tamanho | Tratador | Efeito |
|---|---|---|---|
| `0x7A` | 13 | `0x45E860` | `BYTE +1` ação (0 retira, senão inclui), `BYTE +2` tipo, `WORD +3` classe, `DWORD +5` dono, `DWORD +9` aliado |
| `0x81` | 20 | `0x45E890` | mercenário: tipo 7 |

`0x478B10` inclui ou atualiza; `0x478C90` retira; `0x478A50` limpa a lista. O tipo 7
substitui o nó anterior de mercenário do mesmo dono (`0x478AB0`).

**Aliados vistos ao vivo**, todos com dono = jogador e alinhamento 2. ✅ A classe do personagem de
teste foi trocada no save, só no prefixo de teste, em Normal, no nível 3.

| Tipo | Classe | Invocação |
|---|---|---|
| 2 | 357 | Valquíria |
| 3 | 289 | Golem de Barro |
| 7 | 271 | mercenário do Ato I |
| 10 | 419 | Corvo |
| 11 | 420 | Lobo Espiritual |
| 12 | 421 | Lobo Atroz |
| 13 | 424 | Sábio de Carvalho |
| 14 | 425 | Trepadeira Venenosa |
| 15 | 428 | Urso Pardo |
| 16 | 417, 418 | Guerreiro e Mestre das Sombras |
| 19 | 351–353 | Hidra |

Lançar Erguer Esqueleto e Reviver sem cadáver por perto não criou unidade: esqueleto e revivido
**não foram vistos**. `TODO(RE)`, com o que muda com outro jogador na partida.

`0x478D90` `D2Client.GetPetRelation` (`__fastcall`, `ecx` = id, sem asserção): 0 = fora da lista, ou
de dono sem grupo em comum com o jogador local; 1 = do jogador local; 2 = de alguém do mesmo grupo;
3 = tipo de aliado sem o bit consultado na tabela de tipos. 🔷

### 7.2 Alinhamento

`0x6259B0` `D2Common.GetUnitAlignment`: `__stdcall(Unit*)`, sem asserção. 🔷 Devolve 0
com unidade nula ou sem stats; nos tipos 0 e 1, o stat 172 (`0xAC`) da sub-lista do
estado 105 (`0x69`), ou 0 sem esse estado; nos demais tipos, 2.

Lido ao vivo, percorrendo a memória: lista em `Unit+0x5C`; sub-listas a partir de
`lista+0x3C`, próximo em `+0x2C`; estado em `+0x14`; pares (`stat << 16`, valor) em
`+0x24`, com a contagem no WORD `+0x28`. ✅

| Unidade | Resultado | Conf. |
|---|---|---|
| mercenário (271), hidras (351–353) | 2 | ✅ |
| NPCs 511 e 513, Ato V | 2 | ✅ |
| monstros hostis | sem o estado: 0 | ✅ |
| classes 567, 568, 569, na cidade do Ato V | sem o estado: 0 | ✅ |
| classes 359 e 522, sem nó na lista de aliados | 2 | ✅ |
| todas as invocações da tabela de §7.1 | 2 | ✅ |

Portanto **alinhamento 2 recusa o que não é hostil, mas não diz quem é aliado do jogador**: também
vale para NPCs e para as classes 359 e 522. Para dono, use a lista de §7.1. O filtro de §4 usa os dois
sinais.

## 8. Em aberto (`TODO(RE)`)

1. Bits de `+0xC4`/`+0xC8` além dos da tabela (§3.3) e `MonsterData+0x2C`.
2. Bits `0x80`/`0x100` do WORD `MonsterData+0x16`. Nenhum teste direto de `0x10`
   foi achado no binário: vale o rótulo do jogo.
3. Colunas de `monstats` além de `+0x00`/`+0x0C`, com o alinhamento das linhas (§4).
4. Objetos de quest: 13 dos 24 valores de `OperateFn` só têm tratador e predefinida; os selos não foram
   alcançados em jogo (§5.4). De onde o cliente tira o estado das missões (§5.4). Classes 100 e 377,
   que não têm predefinida (§5.3).
5. Esqueleto, revivido e multijogador (§7.1). Multijogador (TCP/IP) segue não examinado.
6. Tudo foi visto com um jogador. Filtro de §4: 97 das 626 classes que passam nos bits foram vistas
   vivas; alinhamento 1 e monstro convertido não foram vistos.
