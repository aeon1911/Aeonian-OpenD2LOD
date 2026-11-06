# Spec — Estado do personagem: atributos, habilidades e pontos (`Game.exe` 1.14d)

> **Tipo:** análise (fatos de interoperabilidade). Não contém código da
> Blizzard nem descompilado. Alimenta `aeonian/addresses/1.14d.ini` e os itens
> **M2.2** e **C3** (planejador de builds) de [`../ROADMAP.md`](../ROADMAP.md).
> Companheiras: [`game-exe-1.14d-units.md`](game-exe-1.14d-units.md) (estado do
> cliente, unidades) e
> [`game-exe-1.14d-levels-automap.md`](game-exe-1.14d-levels-automap.md)
> (níveis, automap).
>
> **Evidência e método:** `Game.exe` 1.14d (SHA-256
> `631066c1649c4ea9ffe48bf97e24c00bca1f7a6759c21150f1a79982589adaaf`),
> ImageBase `0x400000`, sem ASLR (VA = RVA + `0x400000`). Desmontagem estática
> do `.text` com `objdump` a partir das strings de arquivo-fonte
> (`.\DATATBLS\SkillTbls.cpp`, `C:\projects\D2\head\Diablo2\Source\D2Common\SKILLS\Skills.cpp`,
> `.\UI\CharScrn.cpp`, `.\UI\spellsel.cpp`, `D2Game/Player/PlrMsg.cpp`,
> `D2Game/Player/PlrMod.cpp`) e das funções exportadas de `D2Common` / `D2Client`.
> Legenda: ✅ confirmado em execução; 🔷 confirmado no binário; ⚠️ hipótese
> (`TODO(RE)`).

## 1. Identificação do personagem

Na unidade do jogador local (`Unit*` em `[0x7A6A70]`, tipo `0` em `Unit+0x00`):

| Offset | Tam. | Campo | Conf. |
|---|---|---|---|
| `Unit+0x04` | 4 | classe do personagem (`0` Amazona, `1` Feiticeira, `2` Necromante, `3` Paladino, `4` Bárbaro, `5` Druida, `6` Assassina) | ✅ |
| `Unit+0x0C` | 4 | id da unidade | ✅ |
| `Unit+0x14` | 4 | `PlayerData*`; `PlayerData+0x00` é `char[16]` com o nome do personagem (ASCII terminado em `\0`) | 🔷 |
| `Unit+0x5C` | 4 | `StatListEx*` (atributos e pontos livres, §2) | ✅ |
| `Unit+0xA8` | 4 | `SkillList*` (habilidades aprendidas e ativas, §3) | 🔷 |

## 2. Atributos e pontos livres (`StatListEx`)

### 2.1 Funções do jogo (`D2Common`)

Todas verificam `Unit* == NULL` e `Unit+0x5C == NULL` no prólogo e devolvem `0`
sem asserção. 🔷

| VA | Convenção | Assinatura | Comportamento | Conf. |
|---|---|---|---|---|
| `0x6253B0` | `__stdcall` | `int D2Common.GetUnitBaseStat(Unit* pUnit, int nStatId, uint16_t nLayer)` | lê o valor **base** do atributo (sem bônus de itens/auras) no vetor `StatListEx+0x24` | 🔷 |
| `0x625480` | `__stdcall` | `int D2Common.GetUnitStat(Unit* pUnit, int nStatId, uint16_t nLayer)` | lê o valor **efetivo/total** do atributo (com bônus); para `StatListEx` (`flag 0x80000000` em `+0x10`), consulta `StatListEx+0x48` | 🔷 |
| `0x625560` | `__stdcall` | `int D2Common.GetUnitStatBonus(Unit* pUnit, int nStatId, uint16_t nLayer)` | diferença / bônus acumulado sobre a base | 🔷 |
| `0x6272B0` | `__stdcall` | `int D2Common.AddUnitStat(Unit* pUnit, int nStatId, int nAddValue, uint16_t nLayer)` | soma `nAddValue` (pode ser negativo) ao stat base no servidor; não chame do cliente para distribuir pontos (use §4) | 🔷 |

Para todos os atributos abaixo, `nLayer` vale **`0`**.

### 2.2 IDs de atributos (`ItemStatCost.txt`)

Confirmados nos chamadores da tela de personagem (`0x4A78C0`), da árvore de
habilidades (`0x4ABC30`), de `CheckSkillStatReqs` (`0x644920`) e de
`SpendStatPoint` (`0x570D60`): 🔷

| `nStatId` | Nome | Unidade / escala | Uso |
|---|---|---|---|
| `0` | `strength` (Força) | inteiro | atributo distribuível (pacote `0x3A` com id `0`); requisito de habilidades (`SkillsTxt+0x176`) |
| `1` | `energy` (Energia) | inteiro | atributo distribuível (pacote `0x3A` com id `1`); requisito (`SkillsTxt+0x17A`) |
| `2` | `dexterity` (Destreza) | inteiro | atributo distribuível (pacote `0x3A` com id `2`); requisito (`SkillsTxt+0x178`) |
| `3` | `vitality` (Vitalidade) | inteiro | atributo distribuível (pacote `0x3A` com id `3`); requisito (`SkillsTxt+0x17C`) |
| `4` | `statpts` | inteiro | **pontos de atributo livres** para distribuir (`0x4A78F7`, `0x570D66`) |
| `5` | `newskills` | inteiro | **pontos de habilidade livres** para distribuir (`0x4ABCA0`, `0x570089`) |
| `6` | `hitpoints` | ponto fixo 24.8 (`valor >> 8`) | vida atual |
| `7` | `maxhp` | ponto fixo 24.8 (`valor >> 8`) | vida máxima |
| `8` | `mana` | ponto fixo 24.8 (`valor >> 8`) | mana atual |
| `9` | `maxmana` | ponto fixo 24.8 (`valor >> 8`) | mana máxima |
| `10` | `stamina` | ponto fixo 24.8 (`valor >> 8`) | vigor atual |
| `11` | `maxstamina` | ponto fixo 24.8 (`valor >> 8`) | vigor máximo |
| `12` | `level` | inteiro | **nível do personagem** (`0x6447D0`, `0x644920`) |
| `13` | `experience` | `uint32_t` | experiência total |
| `14` | `gold` | inteiro | ouro no inventário |
| `15` | `goldbank` | inteiro | ouro no baú |

### 2.3 Layout de `StatListEx` para leitura direta (sem chamar função)

`Unit+0x5C` aponta para `StatListEx`: 🔷

| Offset | Tam. | Campo | Conf. |
|---|---|---|---|
| `+0x10` | 4 | flags; o bit `0x80000000` identifica `StatListEx` (presente em jogadores e monstros) | 🔷 |
| `+0x24` | 4 | `StatPair*` dos **stats base** | 🔷 |
| `+0x28` | 2 | `uint16_t` quantidade de pares no vetor base (`+0x24`) | 🔷 |
| `+0x3C` | 4 | primeira sub-lista de estados (usada para o alinhamento em `game-exe-1.14d-units.md` §7.2) | ✅ |
| `+0x48` | 4 | `StatPair*` dos **stats totais/modificados** (apenas quando `flags & 0x80000000`) | 🔷 |
| `+0x4C` | 2 | `uint16_t` quantidade de pares no vetor total (`+0x48`) | 🔷 |

Cada entrada de `StatPair` tem **8 bytes**, ordenada em ordem crescente da chave
de 32 bits em `+0x00`:
- `+0x00` (`uint32_t`): `(uint32_t(nStatId) << 16) | uint32_t(nLayer)` (para `nLayer == 0`, `stat_key >> 16 == nStatId` e `(stat_key & 0xFFFF) == 0`).
- `+0x04` (`int32_t`): valor do stat.

O jogo faz busca binária nesse vetor (`0x624530`), mas como o vetor tem poucas
dezenas de entradas, uma varredura linear com limite (`<= 1024`) ou a chamada de
`D2Common.GetUnitBaseStat` / `D2Common.GetUnitStat` dão o mesmo resultado.
**Nota:** nos stats `0..3` (Força, Energia, Destreza, Vitalidade), o jogo verifica
requisitos de habilidades contra o valor **efetivo** (`GetUnitStat`, em
`0x644920`), enquanto os pontos livres (`4` e `5`) são lidos de `GetUnitStat` /
`GetUnitBaseStat` indistintamente. 🔷

## 3. Habilidades e árvore de habilidades

### 3.1 `SkillList` (`Unit+0xA8`) e `Skill`

| Estrutura / Offset | Tam. | Campo | Conf. |
|---|---|---|---|
| `SkillList+0x00` | 4 | pool de memória | 🔷 |
| `SkillList+0x04` | 4 | `Skill*` primeira habilidade da lista encadeada | 🔷 |
| `SkillList+0x08` | 4 | `Skill*` habilidade do botão esquerdo | 🔷 |
| `SkillList+0x0C` | 4 | `Skill*` habilidade do botão direito | 🔷 |
| `SkillList+0x10` | 4 | `Skill*` habilidade em uso | 🔷 |
| `Skill+0x00` | 4 | `SkillsTxt*` ponteiro para o registro da habilidade em `DataTables` (§3.3) | 🔷 |
| `Skill+0x04` | 4 | `Skill*` próxima habilidade na lista | 🔷 |
| `Skill+0x28` | 4 | `int32_t` **nível base** da habilidade (pontos investidos) | 🔷 |
| `Skill+0x34` | 4 | `int32_t` `nOwnerGUID` (`-1` / `0xFFFFFFFF` para habilidades próprias do personagem; `!= -1` para cargas de item) | 🔷 |
| `Skill+0x38` | 4 | `int32_t` quantidade de cargas | 🔷 |
| `Skill+0x3C` | 4 | `int32_t` flag de habilidade por carga | 🔷 |

**Importante:** um personagem recém-criado só tem nós `Skill` na lista para as
habilidades já aprendidas (ou iniciais, como Ataque `0`). Uma habilidade da
classe com **0 pontos** pode ainda **não** ter nó em `SkillList` até receber o
primeiro ponto (`0x570080` chama `0x644550` para criar o nó quando o 1º ponto é
gasto). 🔷

### 3.2 Funções de habilidades (`D2Common` e `D2Client`)

| VA | Convenção | Assinatura | Comportamento | Conf. |
|---|---|---|---|---|
| `0x643910` | `__stdcall` | `Skill* D2Common.GetFirstSkillFromSkillList(SkillList* pList)` | devolve `pList ? pList->pFirstSkill : NULL` (sem asserção) | 🔷 |
| `0x6438F0` | `__stdcall` | `Skill* D2Common.GetNextSkill(Skill* pSkill)` | devolve `pSkill ? pSkill->pNextSkill : NULL` (sem asserção) | 🔷 |
| `0x6439B0` | `__fastcall` | `Skill* D2Common.GetSkillById(Unit* pUnit /*ecx*/, int nSkillId /*edx*/, int nOwnerGUID /*pilha: -1*/)` | procura na `SkillList` da unidade o nó com `wSkillId == nSkillId` e `nOwnerGUID` dado (passe `-1`); devolve `NULL` se não aprendida (sem asserção com `pUnit != NULL`) | 🔷 |
| `0x643CE0` | `__stdcall` | `int D2Common.GetSkillId(Skill* pSkill, const char* szFile, int nLine)` | devolve `int16_t` `pSkill->pSkillsTxt->wSkillId`, ou `-1` se `pSkill == NULL` | 🔷 |
| `0x6442A0` | `__stdcall` | `int D2Common.GetSkillLevel(Unit* pUnit, Skill* pSkill, int bBonus)` | devolve o nível base (`bBonus == 0`) ou efetivo com bônus de itens/auras (`bBonus != 0`); `0` se `pUnit` ou `pSkill` for `NULL` | 🔷 |
| `0x6447B0` | `__fastcall` | `int D2Common.GetBaseSkillLevelById(Unit* pUnit /*ecx*/, int nSkillId /*edx*/)` | atalho seguro: busca `GetSkillById(pUnit, nSkillId, -1)` e devolve `GetSkillLevel(pUnit, pSkill, 0)` (`0` se ainda não aprendida) | 🔷 |
| `0x644AA0` | `__stdcall` | `int D2Common.GetSkillLevelById(Unit* pUnit, int nSkillId)` | devolve o nível **efetivo** (com bônus) da habilidade `nSkillId`, ou `0` | 🔷 |
| `0x6447D0` | `__stdcall` | `int D2Common.CheckSkillPrereqs(Unit* pUnit, int nSkillId)` | devolve `!= 0` se o personagem tem nível suficiente para o **próximo** ponto (`level >= reqlevel + base_level_atual`) e nível base `> 0` em todas as habilidades pré-requisito (`reqskill1..3`) | 🔷 |
| `0x644920` | `__stdcall` | `int D2Common.CheckSkillStatReqs(Unit* pUnit, int nSkillId)` | confere `CheckSkillPrereqs` e também os requisitos de atributos (`reqstr`, `reqdex`, `reqint`, `reqvit`) | 🔷 |
| `0x646140` | `__stdcall` | `int D2Common.GetClassSkillCount(int nCharClass)` | quantidade de habilidades da classe `0..6` (30 no LoD vanilla) | 🔷 |
| `0x6460F0` | `__stdcall` | `int D2Common.GetClassSkillId(int nCharClass, int nIndex)` | devolve o `nSkillId` da `nIndex`-ésima habilidade (`0..count-1`) da classe `nCharClass`, ou `-1` | 🔷 |
| `0x4AA8B0` | `__fastcall` | `int D2Client.GetSkillMaxLevel(int nSkillId /*ecx*/)` | devolve `SkillsTxt.wMaxLvl` (`+0x12C`) se `> 0`, ou `20` por padrão | 🔷 |

### 3.3 Tabelas `skills` (`SkillsTxt`) e `skilldesc` (`SkillDescTxt`) em `DataTables`

A partir de `DataTables*` em `[0x744304]` (âncora `D2Common.MonStatsRecordAccess` +2): 🔷

| Campo em `DataTables` | Offset | Tipo | Significado | Conf. |
|---|---|---|---|---|
| `pSkillDescTxt` | `+0xB8C` | `SkillDescTxt*` | base dos registros de `skilldesc.txt` (tamanho do registro: **`0x120`** = 288 bytes) | 🔷 |
| `nSkillDescTxtRecordCount` | `+0xB94` | `int32_t` | quantidade de registros em `pSkillDescTxt` | 🔷 |
| `pSkillsTxt` | `+0xB98` | `SkillsTxt*` | base dos registros de `skills.txt` (tamanho do registro: **`0x23C`** = 572 bytes) | 🔷 |
| `nSkillsTxtRecordCount` | `+0xBA0` | `int32_t` | quantidade de registros em `pSkillsTxt` | 🔷 |

Campos de `SkillsTxt` (registro de `0x23C` bytes em `[[0x744304]+0xB98] + skill_id * 0x23C`): 🔷

| Offset | Tam. | Tipo | Campo | Uso no código |
|---|---|---|---|---|
| `+0x000` | 2 | `int16_t` | `wSkillId` | `0x643CE0` (`GetSkillId`) |
| `+0x00C` | 1 | `int8_t` | `nCharClass` | classe dona (`0..6`, ou `-1` se geral) |
| `+0x12C` | 2 | `int16_t` | `wMaxLvl` | nível base máximo (se `<= 0`, vale `20`; `0x4AA8B0`) |
| `+0x174` | 2 | `int16_t` | `wReqLevel` | nível mínimo do personagem para o 1º ponto (`0x6447D0`) |
| `+0x176` | 2 | `int16_t` | `wReqStr` | Força mínima (`0x644920`) |
| `+0x178` | 2 | `int16_t` | `wReqDex` | Destreza mínima (`0x644920`) |
| `+0x17A` | 2 | `int16_t` | `wReqInt` | Energia mínima (`0x644920`) |
| `+0x17C` | 2 | `int16_t` | `wReqVit` | Vitalidade mínima (`0x644920`) |
| `+0x17E` | 2 | `int16_t` | `wReqSkill1` | 1º pré-requisito (`< 0` = nenhum; `0x6447D0`) |
| `+0x180` | 2 | `int16_t` | `wReqSkill2` | 2º pré-requisito (`< 0` = nenhum; `0x6447D0`) |
| `+0x182` | 2 | `int16_t` | `wReqSkill3` | 3º pré-requisito (`< 0` = nenhum; `0x6447D0`) |
| `+0x194` | 2 | `int16_t` | `wSkillDesc` | índice em `pSkillDescTxt` (`<= 0` = sem descrição) |

Campos de `SkillDescTxt` (registro de `0x120` bytes em `[[0x744304]+0xB8C] + skill_desc * 0x120`): 🔷

| Offset | Tam. | Tipo | Campo | Uso |
|---|---|---|---|---|
| `+0x002` | 1 | `uint8_t` | `nSkillPage` | aba na árvore de habilidades (`1..3`, ou `0` fora da árvore) |
| `+0x003` | 1 | `uint8_t` | `nSkillRow` | linha na aba (`1..6`) |
| `+0x004` | 1 | `uint8_t` | `nSkillColumn` | coluna na aba (`1..3`) |
| `+0x008` | 2 | `uint16_t` | `wStrName` | índice do nome da habilidade na tabela de strings |

Faixas de `nSkillId` das 7 classes no LoD vanilla (30 habilidades por classe, na
ordem de `skills.txt`):
- `0` Amazona: `6..35`
- `1` Feiticeira: `36..65`
- `2` Necromante: `66..95`
- `3` Paladino: `96..125`
- `4` Bárbaro: `126..155`
- `5` Druida: `221..250`
- `6` Assassina: `251..280`

Um mod que leia `DataTables` (ou use `GetClassSkillCount` / `GetClassSkillId`)
funciona tanto no LoD vanilla quanto em mods que alterem `skills.txt`.

## 4. Como o jogo gasta pontos de atributo e habilidade

### 4.1 No cliente: `D2Client.SendPacket_ByteWord` (`0x4785B0`)

Quando o jogador clica nos botões `+` da tela de personagem (`0x4A78C0`) ou numa
habilidade da árvore (`0x4ABC30`), o cliente **não** altera `StatListEx` nem
`SkillList` diretamente: ele envia um pacote de 3 bytes ao servidor (local em um
jogador, remoto em multijogador) por **`0x4785B0`**: 🔷

- **Endereço:** `0x4785B0` (RVA `0x0785B0`, bytes `55 8b ec 51 57 8d 45 fc 50 bf 03 00 00 00 88 4d fc 66 89 55 fd`).
- **Assinatura:** `void __fastcall D2Client.SendPacket_ByteWord(uint8_t bPacketId /*cl*/, uint16_t wParam /*dx*/)`
- **O que faz:** monta `{ bPacketId, (uint8_t)(wParam & 0xFF), (uint8_t)(wParam >> 8) }` na pilha e chama `0x478350` (`D2Client.SendPacketToServer`).

| Ação | `bPacketId` (`cl`) | `wParam` (`dx`) | Chamador na UI do cliente | Tratador no servidor (`D2Game`) | Conf. |
|---|---|---|---|---|---|
| **Gastar ponto(s) de atributo** | `0x3A` | `(uint16_t)((stat_id & 0xFF) \| (((qtd - 1) & 0xFF) << 8))` | `0x4A7A1D` (`CharScrn.cpp`) | `0x54BD10` → chama `0x570D60` (`D2Game.SpendStatPoint`) `qtd` vezes | 🔷 |
| **Gastar 1 ponto de habilidade** | `0x3B` | `(uint16_t)skill_id` | `0x4ABEC9` (`spellsel.cpp`) | `0x54BD90` → valida e chama `0x570080` (`D2Game.SpendSkillPoint`) | 🔷 |

Detalhes importantes verificados no binário: 🔷
1. **Lote de atributos no pacote `0x3A`:** o byte baixo de `wParam` é o atributo
   (`0` Força, `1` Energia, `2` Destreza, `3` Vitalidade) e o byte alto é
   `qtd - 1`. Com clique simples a UI passa `qtd = 1` (`wParam = stat_id`); com
   Shift+clique a UI limita a `32` por clique (`0x4A7A0A`), e o servidor aceita
   até `qtd = 100` (`byte_alto <= 0x63` em `0x54BD21`). Para gastar 1 ponto por
   vez, basta `wParam = (uint16_t)stat_id`.
2. **Filtro de pacotes idênticos em `0x478350` (`SendPacketToServer`):** antes de
   enviar um pacote com `bPacketId >= 0x05`, `0x478350` compara o tamanho e os
   bytes com o último pacote enviado (buffer em `0x7BB3B8`) e, se forem
   **idênticos** e `GetTickCount() - [0x7BB5B8] < 200` (`0xC8` ms), **descarta**
   o pacote sem enviar (`0x478379`..`0x47839C`).
   - Consequência para mods (como o **C3 Build Planner**): ao distribuir pontos
     chamando `D2Client.SendPacket_ByteWord` no `on_tick`, espaçar envios por
     **`>= 210 ms`** (ou enviar atributos em lote via `qtd - 1` no pacote `0x3A`)
     garante que nenhum pacote seja descartado pelo filtro de 200 ms e que o
     servidor local já tenha processado e sincronizado o ponto anterior antes da
     próxima decisão.
3. **O que o servidor faz ao receber:**
   - `0x570D60` (`D2Game.SpendStatPoint`, `__fastcall(Unit* pPlayer, int nStatId)`):
     confere `GetUnitBaseStat(pPlayer, 4, 0) > 0` e `nStatId <= 3`, subtrai 1 de
     `statpts` (`4`), soma 1 ao atributo `nStatId`, recalcula vida/mana/vigor
     máximos a partir de `CharStatsTxt` e envia o pacote S→C de sincronização ao
     cliente.
   - `0x54BD90` + `0x570080` (`D2Game.SpendSkillPoint`, `__fastcall(Unit* pPlayer, int nSkillId)`):
     confere `GetUnitBaseStat(pPlayer, 5, 0) > 0`, que a habilidade pertence à
     classe do jogador (`0x549490`), que o nível base atual é `< GetSkillMaxLevel`
     (`0x4AA8B0`) e que `CheckSkillPrereqs` (`0x6447D0`) passa; subtrai 1 de
     `newskills` (`5`), incrementa o nível base da habilidade (criando o nó em
     `SkillList` se for o 1º ponto), atualiza passivas e sincroniza com o cliente.

## 5. Resumo para o INI

`D2Common.GetUnitBaseStat`, `D2Common.GetUnitStat`, `D2Common.GetUnitStatBonus`,
`D2Common.AddUnitStat`, `D2Common.GetFirstSkillFromSkillList`,
`D2Common.GetNextSkill`, `D2Common.GetSkillById`, `D2Common.GetSkillId`,
`D2Common.GetSkillLevel`, `D2Common.GetBaseSkillLevelById`,
`D2Common.GetSkillLevelById`, `D2Common.CheckSkillPrereqs`,
`D2Common.CheckSkillStatReqs`, `D2Common.GetClassSkillCount`,
`D2Common.GetClassSkillId`, `D2Client.GetSkillMaxLevel`,
`D2Client.SendPacket_ByteWord`, `D2Game.SpendStatPoint`,
`D2Game.SpendSkillPoint`.

