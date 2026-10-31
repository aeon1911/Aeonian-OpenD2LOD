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
> **VA** (VA = RVA + `0x400000`, sem ASLR). A segunda análise (2026-10-30) está
> descrita na spec companheira; a matriz do que foi visto em cada ato e dificuldade
> está em §9.

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
| `+0x14` | número do ato: 0 a 4 (igual a `Unit+0x18` do jogador) | ✅ Atos I–V |
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
| `+0x1C`, `+0x20` | X, Y em tiles; **já valem com `+0x10 == 0`** | ✅ |
| `+0x24`, `+0x28` | largura, altura em tiles; idem | ✅ |
| `+0x1AC` | próximo `Level` do ato | ✅ |
| `+0x1B4` | `DrlgAct*` | ✅ |
| `+0x1C0` | tipo de nível (de `LevelDefs+0x34`) | 🔷 |
| `+0x1C4` | semente | 🔷 |
| `+0x1D0` | id (linha de `levels.txt`) | ✅ |
| `+0x1E0` | `int[9]`: X, em subtiles, do centro de cada sala com waypoint ou warp real | ✅ |
| `+0x204` | `int[9]`: Y, idem | ✅ |
| `+0x228` | quantos pontos valem | ✅ |

**Pontos de interesse do nível.** `0x6423D0` `D2Common.BuildLevelPoints` roda ao gerar
o nível e percorre as salas: entram as que têm os bits `0x30000` (§2.4) ou um warp
com `Warp[n] ≠ −1`. 🔷 Exemplo ao vivo: nível 44 → (5740, 5420), a sala do waypoint,
e (5540, 5300), a do warp. O nível 43, gerado e nunca visitado, já trazia dois
pontos. ✅ A lista não diz qual ponto é o quê: cruze com as flags das salas.

Na primeira leitura dentro do Ato I, o ato já tinha os níveis 1, 2, 3, 4,
5, 6, 7, 8–16, 17, 26, 27, 39 **alocados**, mas só 1, 2 e 3 com salas
(`+0x10 ≠ 0`). Ao carregar a sala da entrada do Covil, o nível 8 ganhou 27
salas. ✅

### 2.4 `Room2` (sala do DRLG, 0xEC bytes 🔷)

| Offset | Tam. | Campo | Conf. |
|---|---|---|---|
| `+0x08` | 4 | lista de `Room2*` vizinhas (pode incluir salas de **outro** nível). **Só existe depois que a sala foi ativada** (abaixo) | ✅ |
| `+0x0E` | 2 | contador de carga explícita (1 nas salas pedidas pelo servidor, 0 nas vizinhas) | ✅ observado |
| `+0x20` | 4 | dados do tipo (preset/labirinto) | 🔷 |
| `+0x24` | 4 | próxima `Room2` do nível | ✅ |
| `+0x28` | 4 | flags (abaixo) | ✅ |
| `+0x2C` | 4 | número de vizinhas; 0 antes da ativação | ✅ |
| `+0x30` | 4 | `Room1*`; **≠ 0 ⇔ sala carregada** | ✅ |
| `+0x34`, `+0x38` | 4 | X, Y em tiles | ✅ |
| `+0x3C`, `+0x40` | 4 | largura, altura em tiles | ✅ (8×8 na cidade) |
| `+0x44` | 1 | estágio de atividade: 1 a 3 nas salas ativas ou na fila de descarga; 4 em sala inativa, visitada ou não | ✅ valor ⚠️ |
| `+0x48` | 4 | tipo da sala (2 = preset) | 🔷 + ✅ valor |
| `+0x4C` | 4 | lista de `RoomTile` (saídas por warp, §4.2) | ✅ |
| `+0x58` | 4 | `Level*` | ✅ |
| `+0x5C` | 4 | lista de `PresetUnit` | ✅ |

**Flags `Room2+0x28`:**

| Bit | Significado | Conf. |
|---|---|---|
| `0x10 << n` (`0xFF0`) | vínculo com `Vis[n]`; é warp só com `Warp[n] ≠ −1` (§4.2) | ✅ |
| `0x1000`…`0x8000` | em poucas salas de cada nível externo | ⚠️ `TODO(RE)` |
| **`0x10000`, `0x20000`** | **a sala tem waypoint** (máscara `0x30000`). Vale **sem carregar a sala**, desde a geração do nível | ✅ uma sala por nível com waypoint, nos Atos I–V, e o waypoint estava nela sempre que a sala carregou. 🔷 `0x66BAA0`, `0x66BB60` |
| `0x40000`, `0x80000` | quase todas as salas | ⚠️ `TODO(RE)` |
| `0x100000` | sala ligada: vizinhas prontas (`0x61B757`) | 🔷 + ✅ valor |
| `0x800000` | sala de preenchimento nas beiradas (`0x66BB43` devolve nível 0 para ela). Não distingue passagem | 🔷 + ✅ valor |
| `0x1000000` | arquivos de tiles da sala processados (`0x61B324` → `0x66F240`) | 🔷 |
| `0x2000000` | predefinidas da sala processadas (`0x61B338` → `0x667890`) | 🔷 |

`0x1000000` e `0x2000000` mostram só até onde a preparação do jogo já chegou. Não
marcam borda nem passagem. A primeira leitura pareceu indicar isso; o código mostra
o contrário.

**Quando as vizinhas existem.** A lista `+0x08` é montada em `0x61B730`
`D2Common.EnsureRoomActive` (`0x66C370`, só se `+0x2C` for 0), na ativação. 🔷 Ao
vivo ✅: na Planície Fria, as salas carregadas tinham vizinhas e as de `r1 = 0`
nunca visitadas tinham `+0x2C = 0`. Depois de descarregada, a sala **mantém** a
lista: no nível 44 havia 61 salas descarregadas com vizinhas.

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
- **As vizinhanças externas só existem na exceção.** No Ato I, `LevelDefs` dá o nível
  2 com `Vis` = 0, 0, 0, 8, 8, 8, 8, 0; a exceção dá 3, 1, 0, 8, 8, 8, 8, 0, com
  `Warp[0..1]` = −1. Quem ler só `LevelDefs` não vê as bordas. Listas de exceção
  vistas nos Atos I–V. ✅
- `Vis[n] ≠ 0` com `Warp[n] = −1` é passagem aberta (§4.1); com `Warp[n] ≠ −1`, warp
  (§4.2). ✅

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

### 3.1 Carregar salas por fora: o que foi medido

Teste de análise, só na cópia-espelho: um plugin descartável (scratchpad) chamou as
funções na **thread principal**, dentro de `on_tick`, em uma partida de um jogador,
dificuldade Normal, Ato I. Resultados ✅:

| Caso | Resultado |
|---|---|
| `AddRoomData(act, nível, x, y, sala do jogador)` numa sala distante do nível atual | 5,2 ms; `Room1` criada, `+0x0E` = 1; a sala do warp ganhou `RoomTile` e `PresetUnit` tipo 5 |
| Posição obtida assim | **exata**: (5113, 5066), igual à unidade tile que apareceu depois ao chegar lá. O warp funcionou normalmente |
| Nível ainda não gerado (`Level+0x10 == 0`) | gerou o nível sozinho: 11,7 ms, 95 salas, 9 carregadas em volta do ponto. Posições dos dois warps iguais às de outra partida com a mesma semente |
| Todas as 95 salas de um nível gerado, em seguida | 4,9 ms no total, 0,76 ms no pior caso |
| `RemoveRoomData` | 0,018 ms; `+0x0E` volta a 0 e a sala vai para o estágio 3. A `Room1` não some na hora: seguia lá após 3 s em dois testes; no teste com o nível inteiro, nenhuma restava após 4 s. `PresetUnit` e `RoomTile` **continuam** |
| Unidades | nenhuma nova nas tabelas: o servidor não fica sabendo |
| Automap do jogo | nenhuma célula nova visível com 6 salas carregadas (comparação visual de quadros) |
| Depois disso | andar e entrar no warp seguiram normais, e salvar e sair também. Nenhuma queda nesses testes, que duraram poucos minutos |

Portanto dá para saber a posição exata de um warp sem chegar perto: chame
`AddRoomData` e depois `RemoveRoomData` na sala do warp, uma vez por sala, e leia as
`PresetUnit`, que ficam. Salas com waypoint já são conhecidas pela flag (§2.4).

`0x667890` `D2Common.InitRoomPresets` sozinha **não** resolve: numa sala já
preparada e sem `Room1`, a chamada não criou a predefinida do warp. Quem cria é a
ativação (`0x61B730`). ✅

`TODO(RE)`, o que **não** foi testado:
- multijogador: lá o servidor é remoto, e esse caminho não foi exercitado;
- Atos II–V e dificuldades acima do Normal, com estas chamadas;
- nível de **outro ato** como argumento (não tente);
- efeito no arquivo `.ma0`/`.map` do personagem e sessões longas;
- carregar enquanto o servidor pede a mesma sala: o contador `+0x0E` acumula, mas a
  corrida não foi provocada de propósito;
- `GetLevel` e `InitLevel` chamadas diretamente. `AddRoomData` já as cobre.

`AddRoomData` **asserta com `Act*` nulo**. Chame só dentro de partida, com o jogador
carregado, e no evento da thread principal.

## 4. Saídas do nível (C1)

O jogo identifica três tipos de saída:

### 4.1 Passagem aberta entre níveis externos

Uma `Room2` cujas vizinhas (`+0x08`, `+0x2C`) incluem salas de **outro**
`Level` (`Room2+0x58` diferente) é borda de saída. A cidade do Ato I liga-se à
Charneca assim, sem warp: as salas da borda sul (tiles y = 1112) têm vizinhas do
nível 2 em y = 1120. ✅

**Correção da primeira análise:** a vizinhança **não** existe desde a geração. Ela
só aparece nas salas já ativadas (§2.4). A primeira análise viu a cidade, onde tudo
fica ativo, e generalizou. ✅

**Por que a borda da Charneca com os Campos Frios (nível 3) não apareceu.** As salas
da Charneca junto ao nível 3 ainda não tinham sido ativadas, então as listas estavam
vazias. O nível vizinho existia. Ao entrar pelo waypoint do nível 3, perto dessa
borda, cinco salas de cada lado passaram a listar as do outro nível. ✅ O mecanismo
está confirmado; a caminhada original pela Charneca não foi repetida passo a passo.

**Como saber da borda antes de chegar perto:** os retângulos dos níveis (`Level+0x1C`
a `+0x28`) já valem antes da geração, e os vizinhos são os `Vis[n] ≠ 0` com
`Warp[n] = −1`. Dois retângulos que se tocam dão o trecho da fronteira:

| Níveis | Retângulo A | Retângulo B | Trecho comum |
|---|---|---|---|
| 2 e 3 | (888, 1064, 96, 56) | (984, 1080, 80, 80) | x = 984, y de 1080 a 1120 |
| 3 e 4 | (984, 1080, 80, 80) | (1000, 1000, 80, 80) | y = 1080, x de 1000 a 1064 |

O trecho é **onde a passagem está**, não a passagem. Na borda 3–4, o trecho tem 8
salas de largura; as salas do nível 4 ligadas ao nível 3 na hora da leitura eram as
de x = 1008 e 1016. ✅ `TODO(RE)`: o gerador do mapa externo guarda a posição exata
em alguma estrutura; não foi localizada. Para ela hoje: ativar a sala (§3.1) ou
esperar o jogador chegar. Um mod deve marcar esse caso como aproximado.

A lista de vizinhas também inclui a **sala de destino de um warp** já ligado: no nível
44, a sala (1104, 1056) lista salas do nível 65, com `Warp[2]` = 50. ✅ Não confunda
com borda: só é borda se o `n` daquele nível tiver `Warp[n] = −1`.

### 4.2 Warps (cavernas, escadas, portais de nível)

1. `Vis[n]`/`Warp[n]` do nível (§2.7) dizem que o warp *n* leva ao nível
   `Vis[n]` usando o registro `Warp[n]` de `lvlwarp`. ✅
2. A `Room2` que contém o warp *n* tem o bit `0x10 << n` em `+0x28`. ✅ Na
   Charneca, **uma** sala tinha `0x200` (n = 5 → `Vis[5]` = 8, Covil do Mal) e era
   a sala da entrada. **Regra:** só considere *n* com `Vis[n] ≠ 0` **e**
   `Warp[n] ≠ −1`, usando os valores da exceção (§2.7). Com `Warp[n] = −1` o bit
   marca vínculo com nível aberto e aparece em muitas salas (180 de 192 no nível
   76, 288 de 288 no 107). ✅ Isso dá a sala da saída **sem carregar nada**; a posição
   exata exige a sala carregada ou ativada (§3.1).

   **Labirintos e demais níveis.** Com essa regra, houve **no máximo uma sala por
   *n***, em todos os níveis conferidos dos Atos I–V (§9). Em labirinto (tipo 1) o bit
   é preciso: no nível 57, 2 de 24 salas têm algum bit, e são as duas dos warps; no
   113, 3 de 44; no 101, 2 de 15. ✅ O mod **não precisa** descartar candidatas nem
   tratar labirinto à parte: várias salas com o mesmo *n* real não ocorreram.

   Um nível pode ter vários *n* para o mesmo destino, e só um é usado: no nível 3,
   `Vis[3..6]` = 9, com salas só em `n` = 6. No nível 101 há quatro warps possíveis
   e duas salas. ✅ Não conte saídas pelos `Vis`; conte pelas salas.
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
   (cinco cópias vistas para a entrada do Covil). ✅ As predefinidas **ficam** depois
   que a sala descarrega. ✅ A tabela de unidades também traz o tile do **outro lado**
   do warp: no nível 76 apareceu a classe 52 em (7629, 5011), fora do retângulo do
   nível; no 106, a classe 70 em (7900, 6350). Filtre pelo nível da sala da unidade. ✅
5. Registro de `lvlwarp`: 0x30 bytes; os valores lidos (id 2: −60, −60, 85, 100,
   5, 0, −1, 1, 1, 2, `'b'`) coincidem com a ordem pública das colunas de
   `lvlwarp.txt` (Id, SelectX, SelectY, SelectDX, SelectDY, ExitWalkX,
   ExitWalkY, OffsetX, OffsetY, LitVersion, Tiles, Direction). ✅ id; ⚠️ nomes
   dos demais.

### 4.3 Waypoints e objetivos de quest

São `PresetUnit` tipo 2 (objetos) ou tipo 1 (NPCs/superúnicos) em salas
carregadas, e as unidades correspondentes. O jogo não marca "objetivo" na
estrutura. Waypoints são reconhecidos por `SubClass & 0x40` no registro de `objects`
(`game-exe-1.14d-units.md` §5.1), e a **sala** do waypoint pelos bits `0x30000`, sem
carregar nada (§2.4). ✅

O próprio jogo faz isso em `0x66AD80` `D2Common.FindLevelWaypoint`: acha a sala pelos
bits, ativa-a e procura a `PresetUnit` tipo 2 com o bit `0x40`. 🔷 Ela tem convenção
própria e ativa a sala; prefira a flag ou §3.1. Portais por objeto:
`game-exe-1.14d-units.md` §5.3.

## 5. Automap

### 5.1 Estado

| Global (VA) | Significado | Âncora no INI | Conf. |
|---|---|---|---|
| `0x7A27E8` | automap **visível** (≠ 0). Tab alterna; vai a 0 com o menu do Esc aberto e volta ao fechar | `D2Client.DrawAutomapCallSite` (+2) | ✅ |
| `0x7A5150` | modo: **0 = tela cheia**, **1 = minimapa** (opção "Automap Size") | `D2Client.GetAutomapMode` (+1) | ✅ |
| `0x711254` | divisor da escala: **10** (modo 0) ou **20** (modo 1) | `D2Client.SetAutomapMode` (+20) | ✅ |
| `0x7A5198`, `0x7A519C` | deslocamento X, Y (int32), recalculado a cada quadro do automap | `D2Client.AutomapUnitToScreen` (+2, +13) | ✅ |
| `0x7A51C8`…`0x7A51D4` | retângulo de recorte: esquerda, topo, direita, base | `D2Client.AutomapClipCheck` (+2, +14, +26) | ✅ valores |
| `0x7A5210` | lado dos painéis abertos (§5.4) | `D2Client.GetAutomapPanelSide` (+1) | ✅ |
| `0x7A5214` | desvio do automap em tela cheia: −largura/4, 0 ou +largura/4 | `D2Client.SetAutomapPanelSide` (+18) | ✅ |
| `0x7A27C0` | array de estado de UI (§5.4) | `D2Client.GetUiState` (+3) | ✅ |
| `0x71146C`, `0x711470` | largura e altura do quadro: **640×480 ou 800×600** (§5.5) | `D2Client.ScreenSizeAnchor` (+6, +12) | ✅ |

- O modo vem da opção do jogo (registro `AutoMapMode`), aplicada por
  `0x45A720` `D2Client.SetAutomapMode` (`ecx` = modo), que também grava o
  divisor. 🔷 Valores > 2 viram 0. O menu só alterna 0 ↔ 1. ✅
- Recorte observado em 800×600: tela cheia (−16, −16, 800, 600); minimapa (−8, 96,
  266, 296); minimapa com painel à esquerda (525, 78, 799, 278). Em 640×480, tela
  cheia (−16, −16, 640, 480). ✅
- No modo 1 o minimapa fica no **canto superior esquerdo** do quadro 800×600. Com um
  painel à esquerda, ele vai para o **canto superior direito**, e o recorte e os
  deslocamentos mudam junto. ✅ Leia sempre os globais; não fixe posição.

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
- A cruz do jogo (`0x45A7F0`) ainda soma **x − 1, y + 5** no modo 1. ✅ (as marcas
  calculadas com o ajuste caíram sobre as cruzes.)
- **Vale em todos os casos testados** ✅, com marcas calculadas sobre as cruzes do jogo
  em quadros gravados: cidades dos Atos I a V; Normal, Pesadelo (Atos I e II) e Inferno
  (Atos III e V); 640×480 e 800×600; painel à direita e à esquerda; minimapa com painel
  à direita e à esquerda. Os deslocamentos já incorporam o painel: **não some
  `0x7A5214` de novo**. Com o lado 3 o automap não é desenhado e os deslocamentos ficam
  velhos.

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
teste `[0x7A27E8]`, recorte pelo retângulo `0x7A51C8…` e pela §5.4; sob D2GL ele roda
em outra thread (copie os pontos no `on_tick`); a escala do D2GL não foi examinada.

### 5.4 Painéis abertos

**`[0x7A5210]` (lado):** ✅ observado em tela cheia e no minimapa.

| Valor | Situação | Automap em tela cheia | `[0x7A5214]` (800×600) |
|---|---|---|---|
| 0 | sem painel lateral | centrado | 0 |
| 1 | painel à **direita** (inventário, árvore de habilidades) | desviado para a esquerda; marcas em x < 400 | −200 |
| 2 | painel à **esquerda** (personagem, missões, waypoint, mercenário) | desviado para a direita; marcas em x ≥ 400 | +200 |
| 3 | os dois lados (personagem + inventário; baú) | **não é desenhado** (`0x456F93` testa) | 0 |

O recorte `0x7A51C8…` **não** exclui o painel em tela cheia: continua (−16, −16, 800,
600). Para não desenhar sobre o painel, o mod precisa cortar por conta própria:
lado 1 → x < largura/2; lado 2 → x ≥ largura/2; lado 3 → nada. ✅ No minimapa o painel
direito não muda nada e o esquerdo muda o canto (§5.1).

`0x45AE90` `D2Client.GetAutomapPanelSide` devolve o lado, sem argumentos. 🔷
`0x45AEA0` `D2Client.SetAutomapPanelSide` o grava; não chame. 🔷

**`[0x7A27C0 + i*4]` (o que está aberto):** 0x26 entradas. Leitura direta, ou
`0x453A90` `D2Client.GetUiState` (`__fastcall`, `ecx` = i, sem conferir) ou `0x4538D0`
(asserta com i ≥ 0x26). 🔷 Índices vistos ligando cada coisa ✅:

| i | Painel | Lado |
|---|---|---|
| 1 | inventário | 1 |
| 2 | personagem | 2 |
| 3 | lista de habilidades do botão | 0 |
| 4 | árvore de habilidades | 1 |
| 8 | **diálogo de NPC** | **0** |
| 9 | menu do Esc (zera `[0x7A27E8]` enquanto aberto) | 0 |
| 10 | automap (`0x7A27E8` é esta entrada) | — |
| 15 | missões | 2 |
| 19 | mercenário | 2 |
| 20 | waypoint | 2 |
| 25 | baú | 3 |
| 33 | ajuda (H) | 0 |

- O diálogo de NPC **não muda o lado**, e o automap continua ligado: teste também
  `i = 8`. Fora dele, o lado basta para os painéis laterais. ✅
- A entrada 19 ficou em 1 depois que o painel do mercenário fechou com Esc. ✅ Portanto
  não confie em uma entrada isolada: o lado é a fonte confiável para painel lateral.
- Entradas 0, 21 e 35 ficaram ligadas sem nenhum painel. 38 em diante não são flags
  (valores de outra natureza além do limite de 0x26). ⚠️
- `TODO(RE)`: cubo, troca com NPC, jogadores, chat. Os demais índices não foram
  abertos.

### 5.5 Tamanho do quadro

- `0x44BA20` `D2Client.SetGameResolution` (`ecx` = modo): modo 0 → 640×480; modo 2 →
  800×600. Grava `0x71146C`, `0x711470` e recalcula o lado. 🔷 São os únicos tamanhos
  que este caminho grava.
- Visto ao vivo ✅: menu principal 640×480 nos globais, mesmo com a janela do menu em
  800×600; partida 800×600; na opção "Resolution" do jogo, área cliente, globais e
  quadro passam a 640×480 e o automap segue a fórmula.
- **Regra:** com o renderizador GDI, o quadro do `on_present` tem o tamanho de
  `[0x71146C]`×`[0x711470]` numa partida. Leia os globais, não fixe 800×600. ✅
- `TODO(RE)`: o encaixe do D2GL segue sendo o modelo observado em
  [`modding/MAP_OVERLAY.md`](../modding/MAP_OVERLAY.md). Nesta análise o D2GL não rodou.

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
`D2Gfx.DrawRectangle`, `D2Win.DrawText`, `D2Win.SetFont`, `D2Win.GetTextWidth`. Da
segunda análise: `D2Client.GetAutomapPanelSide`, `D2Client.SetAutomapPanelSide`,
`D2Client.GetUiState`, `D2Client.GetUiStateChecked`, `D2Client.SetGameResolution`,
`D2Client.ScreenSizeAnchor`, `D2Common.Room2WaypointFlags`,
`D2Common.Room2HasWaypoint`, `D2Common.BuildLevelPoints`, `D2Common.InitRoomPresets`,
`D2Common.EnsureRoomActive`, `D2Common.FindLevelWaypoint`.

## 8. Em aberto (`TODO(RE)`)

1. Carregar salas por plugin: o que falta está em §3.1.
2. `RoomTile+0x08/+0x0C/+0x14`, `Act+0x10`, `PresetUnit+0x00/+0x1C`.
3. Colunas do registro de `lvlwarp` além do id (só a ordem bate com o público).
4. Encaixe do D2GL. O automap em 640×480 e o desvio por painel estão resolvidos (§5.2,
   §5.4, §5.5).
5. Cores/modos das primitivas por renderizador; nenhuma primitiva foi executada.
6. Flags `Room2+0x28` de `0x1000` a `0x8000`, `0x40000`, `0x80000`; `Room2+0x44`.
7. Posição exata da passagem entre níveis externos sem ativar sala (§4.1).
8. Índices de UI não abertos (§5.4).
9. Multijogador não examinado.

## 9. O que foi visto em cada ato e dificuldade

Execução real, invisível e muda (2026-10-30), personagem de teste, mesma semente de
mapa. "Saídas" = salas com warp real, pela regra de §4.2, e cada `n` real teve no
máximo uma sala. A fórmula do automap foi conferida nas cidades.

| Nível | Tipo | Ato | Dificuldade | Salas / com algum bit | `n` reais com sala | Bordas vistas | Waypoint |
|---|---|---|---|---|---|---|---|
| 3 | externo | I | Normal, Inferno | 97 / 62 | 6 | 2, 4 | classe 119 |
| 4 | externo | I | Normal | — | 4 (posição exata por §3.1) | — | sala 0x10000 |
| 5 | externo | I | Pesadelo | 88 / 64 | 4 | — | classe 119 |
| 6 | externo | I | Pesadelo | 95 / 60 | 2, 5 | 5, 7, 11, 20 | classe 119 |
| 42 | externo | II | Pesadelo | 95 / 76 | 0 | 43 | classe 156 |
| 44 | externo | II | Normal, Pesadelo | 96 / 82 | 2 | 43, 45, 65 (warp) | classe 156 |
| 46 | externo | II | Normal | 97 / 7 | 1 a 7 | — | classe 402 |
| 57 | labirinto | II | Normal | 24 / 2 | 0, 4 | — | classe 156 |
| 74 | labirinto | II | Normal | 61 / 0 | nenhum | — | classe 402 |
| 76 | externo | III | Normal | 192 / 180 | 0, 1 | 77, 84 (warp) | classe 237 |
| 101 | labirinto | III | Normal | 15 / 2 | 0, 3 | 100, 102 (warps) | classe 324 |
| 106 | externo | IV | Normal | 74 / 74 | 1 | 105, 107 (warp) | classe 238 |
| 107 | labirinto | IV | Pesadelo | 288 / 288 | 0 | — | classe 238 |
| 111 | externo | V | Normal | 160 / 133 | nenhum | 110 | classe 496 |
| 113 | labirinto | V | Normal | 44 / 3 | 0, 1, 2 | — | classe 511 |

- Layouts de `Unit`, `Room1`, `Room2`, `Level`, `PresetUnit`, `RoomTile`, `LevelDefs` e
  exceções: leituras coerentes em todos os níveis da tabela. ✅
- Dificuldade: o nível 3 teve o mesmo retângulo, as mesmas 97 salas e os mesmos warps
  em Normal e Inferno, com a mesma semente; Pesadelo e Inferno não mudaram offsets. ✅
- Cidades com a fórmula conferida: Ato I (Normal, Pesadelo), II (Pesadelo), III
  (Inferno), IV (Normal), V (Normal, Inferno). ✅
- **Cobertura limitada em Inferno:** o personagem de teste morreu na varredura do nível
  3, e as rodadas no Ato III e no nível 112 pararam com erro da ferramenta de teste.
  Inferno ficou com cidades, nível 3 e o início do nível 112. ⚠️
- **Uma queda sem explicação:** o processo do jogo terminou uma vez durante a condução
  na cidade do Ato I, com o personagem de teste em nível 99 e sem nenhum mod carregado.
  Não se repetiu nos testes em nível 90 e a causa não foi investigada. ⚠️
- As tumbas do Ato II (nível 46) usam as exceções como os demais, e a regra de §4.2 deu
  uma sala por `n`. ✅ Saber qual é a tumba verdadeira não foi examinado. `TODO(RE)`.
