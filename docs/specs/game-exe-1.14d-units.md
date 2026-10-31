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

**Filtro "inimigo" para o C2** (todos os campos ✅):
`unit+0x00 == 1` **e** modo ∉ {0, 12} **e** `(monstats+0x0C & 0x100) == 0`
**e** `(monstats+0x0C & 0x8000) != 0` **e** a unidade **não** está na lista de aliados (§7.1). Conferido nos registros
carregados das classes vistas nos Atos I–V: as de cidade, inclusive as de ambiente
(567, 568, 569, com `0x02040401`), são recusadas pelos bits de `+0x0C`; todos os
monstros hostis vistos passam. Mercenários (271) e hidras (351–353) **passam** pelos
bits, então a lista de aliados é obrigatória. ✅ Eles têm o bit `0x00040000`, ausente
nos hostis amostrados, mas presente também nas classes 567–569: não use só esse bit.

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
| 43, 46, 66, 70, 71, 73 | outras passagens por objeto (classes 100, 342, 547, 563, 564, 566, 569) | 🔷 valor; não usadas ao vivo ⚠️ |

**Objetos de quest não têm coluna própria.** ⚠️ O jogo não os marca: cada um tem uma
`OperateFn` exclusiva ou quase. Os valores 9 (pedras de Caim), 12, 21, 24, 25, 28,
31, 33, 42, 49, de 52 a 56 e de 62 a 65 caem em objetos de nome ligado a missões na
tabela da instalação de análise (cada um com 1 a 5 classes). Isso é pista lida da
tabela, não execução. Para um mod: comece por `OperateFn` nesses valores, ou por
`AutoMap ≠ 0`, e ofereça override em `[objects]`. `TODO(RE)`: confirmar um a um
em jogo, o que exige avançar as missões.

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
| 298 | portal do Santuário Arcano | `SubClass & 0x10` | `ObjectData+0x04` = **0** ⚠️ | ✅ visto no nível 74; destino não vem deste campo |
| 342 | portal (`Parm0` = 18) | `SubClass & 0x10` | ⚠️ | 🔷 valor, não visto |

- Na cidade, o portal azul aparece na tabela do servidor, tipo 2, modo 2. ✅
- `TODO(RE)`: destino das classes 298 e 342. `TODO(RE)`: outros portais vermelhos
  (Tristram, vacas, Nihlathak) não foram abertos, mas não há outra classe de
  `SubClass & 0x04` além de 59 e 60. 🔷

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
`D2Game.WaypointOperate`, `D2Game.PortalOperate`. Offsets de estrutura ficam só
nesta spec.

## 7. Dono da unidade: mercenários e invocações

### 7.1 Lista de aliados do cliente

**`[0x7BB5BC]`** é a cabeça de uma lista encadeada com um nó para cada mercenário ou
invocação conhecida pelo cliente. Âncora: `D2Client.FindPetEntry`, endereço nos
bytes `+1`. ✅

| Offset | Tam. | Campo | Conf. |
|---|---|---|---|
| `+0x00` | 4 | classe (linha de `monstats`) | ✅ |
| `+0x04` | 4 | tipo do aliado: 7 mercenário; 19 hidra | ✅ |
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

`TODO(RE)`: outros tipos de aliado (golem, esqueleto, valquíria, lobos) e o que muda
com outro jogador na partida. Só mercenário e hidra foram vistos; a lista não
depende da classe, então a regra deve valer, mas não foi testada.

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

Portanto **alinhamento 2 não identifica aliado do jogador**: também vale para NPCs.
Use-o como reforço; para dono, use a lista de §7.1. O filtro de §4 continua
necessário para os NPCs.

## 8. Em aberto (`TODO(RE)`)

1. Bits de `+0xC4`/`+0xC8` além dos da tabela (§3.3) e `MonsterData+0x2C`.
2. Bits `0x80`/`0x100` do WORD `MonsterData+0x16`. Nenhum teste direto de `0x10`
   foi achado no binário: vale o rótulo do jogo.
3. Colunas de `monstats` além de `+0x00`/`+0x0C`, com o alinhamento das linhas (§4).
4. Objetos de quest: confirmar em jogo (§5.1). Classes 298 e 342: destino (§5.3).
5. Outros aliados e multijogador (§7.1). Multijogador (TCP/IP) segue não examinado.
6. Tudo foi visto com um jogador. As classes de monstro de §4 são uma amostra: o
   filtro não foi conferido linha a linha na tabela.
