# Spec — Carregador de tabelas (`DataTables`) e modo `-txt` (`Game.exe` 1.14d)

> **Tipo:** análise (fatos de interoperabilidade). Não contém código da
> Blizzard nem descompilado. Alimenta `aeonian/addresses/1.14d.ini`, o cache de
> `.bin` do VFS (item **M1** de [`../ROADMAP.md`](../ROADMAP.md)) e o item
> **M2.2.4** (`DataTables` e `CharStatsTxt`). Companheiras:
> [`game-exe-1.14d-units.md`](game-exe-1.14d-units.md) e
> [`game-exe-1.14d-character.md`](game-exe-1.14d-character.md).
>
> **Evidência e método:** `Game.exe` 1.14d (SHA-256
> `631066c1649c4ea9ffe48bf97e24c00bca1f7a6759c21150f1a79982589adaaf`),
> ImageBase `0x400000`, sem ASLR (VA = RVA + `0x400000`). Desmontagem estática
> do `.text` com `objdump` a partir das strings de arquivo-fonte
> (`C:\projects\D2\head\Diablo2\Source\D2Common\DATATBLS\DataTbls.cpp`,
> `.\DATATBLS\SkillTbls.cpp`, `"DATA\\GLOBAL\\EXCEL\\%s.txt"`,
> `"DATA\\GLOBAL\\EXCEL\\%s.bin"`, `"charstats"`). Legenda: ✅ confirmado em
> execução; 🔷 confirmado no binário; ⚠️ hipótese (`TODO(RE)`).

## 1. Globais do carregador de tabelas (`DataTbls.cpp`)

| Global (VA) | Tipo | Valor inicial | Significado | Âncora no INI | Conf. |
|---|---|---|---|---|---|
| `0x744304` | `DataTables*` | `0x96BC30` (`.bss`) | ponteiro global para a estrutura `DataTables` | `D2Common.MonStatsRecordAccess` (+2), `D2Client.GetCharStatsTxtRecord` (+8) | ✅ |
| `0x744308` | `int32_t` | `1` (`.data`) | `bLoadBin`: quando `!= 0`, `CompileTxtFile` lê `DATA\GLOBAL\EXCEL\<nome>.bin` | `D2Common.FreeDataTables` (+18) | 🔷 + ✅ |
| `0x96C8B4` | `int32_t` | `0` (`.bss`) | `bCompileTxt`: ativado (`1`) por `D2Common.EnableTxtCompilation` quando `-txt` está na linha de comando (`0x44D9CC`) | `D2Common.EnableTxtCompilation` (+12) | 🔷 + ✅ |

## 2. Como `D2Common.CompileTxtFile` (`0x6122F0`) funciona e por que `-txt` exige escrita de `.bin`

Todas as tabelas de `data\global\excel\` (`charstats`, `skills`, `skilldesc`,
`monstats`, `objects`, `levels`, `itemstatcost`, `weapons`, `armor`, `misc`…)
são carregadas por `0x619300` (`D2Common.LoadDataTables`) chamando **`0x6122F0`
(`D2Common.CompileTxtFile`)**: 🔷 + ✅

- **Assinatura:** `void* __stdcall D2Common.CompileTxtFile(void* hArchive /*[ebp+8]*/, const char* szName /*[ebp+0xC]*/, BinField* pTbl /*[ebp+0x10]*/, int* pRecordCount /*[ebp+0x14]*/, int nRecordSize /*[ebp+0x18]*/)`
- **Fluxo exato no binário (`0x6122F0`..`0x61258B`):**
  1. Se `[0x96C8B4] != 0` (flag `-txt` ativa):
     - Formata `"DATA\\GLOBAL\\EXCEL\\%s.txt"` e chama `0x612280`
       (`D2Common.ReadTableFile`), que abre o `.txt` via Storm (`SFileOpenFileEx`
       → `CreateFileA` em modo `-direct` com `GENERIC_READ`, `OPEN_EXISTING`).
     - Analisa as linhas e colunas descritas por `BinField* pTbl` e monta os
       registros binários num buffer de heap `ebx` (com cabeçalho de 4 bytes
       contendo `nRecordCount` seguido de `nRecordCount * nRecordSize` bytes).
     - Chama **`0x6121F0` (`D2Common.WriteBinFile`)**, que formata
       `"DATA\\GLOBAL\\EXCEL\\%s"` (`<nome>.bin`) e chama `fopen(..., "wb")` do
       CRT (`0x68239A` → `0x69B2BA` → `KERNEL32!CreateFileA` via IAT `0x6CC19C`
       com `GENERIC_WRITE`, `CREATE_ALWAYS`), gravando `4 + nRecordCount * nRecordSize`
       bytes e fechando o arquivo.
     - **Em `0x612475` (`call 0x40b480`), libera (`Fog.FreeClientMemory`) o
       buffer `ebx` recém-compilado!**
  2. Em seguida (`0x612486`), como `[0x744308]` (`bLoadBin`) vale **`1`**:
     - Formata `"DATA\\GLOBAL\\EXCEL\\%s.bin"` e chama **`0x612280`
       (`D2Common.ReadTableFile`)** para **reler do disco** o `.bin`!
     - Se a abertura de `"DATA\\GLOBAL\\EXCEL\\%s.bin"` no disco falhar (por
       exemplo, porque a escrita em `0x6121F0` falhou por não existir a pasta
       `<jogo>\DATA\GLOBAL\EXCEL\` ou por não ser redirecionada pelo VFS), o
       Storm cai silenciosamente no `<nome>.bin` **original de dentro do MPQ**,
       descartando todas as alterações do `.txt`!

### Consequência para o VFS do Aeonian (item M1)

Para que mods de dados em `mods/<id>/data/global/excel/*.txt` funcionem sem
tocar na pasta do jogo do usuário:
1. Quando qualquer mod ativo contiver arquivos `data\global\excel\*.txt`, o
   Aeonian acrescenta `-direct` e `-txt` automaticamente à linha de comando
   (configurável por `[core] force_txt = auto|on|off`).
2. Quando o jogo abre `DATA\GLOBAL\EXCEL\<nome>.bin` para **escrita**
   (`GENERIC_WRITE` / `CREATE_ALWAYS` em `HookCreateFileA`), o Aeonian
   redireciona a escrita para `<aeonian>\cache\data\global\excel\<nome>.bin` e
   registra o arquivo gerado numa raiz de alta prioridade do VFS (`_txt_cache`).
3. Na leitura imediatamente seguinte em `0x612486`, `HookCreateFileA` resolve
   `DATA\GLOBAL\EXCEL\<nome>.bin` para o `.bin` recém-compilado em
   `<aeonian>\cache\data\global\excel\<nome>.bin`.

| VA | RVA | Convenção | Assinatura | Papel | Conf. |
|---|---|---|---|---|---|
| `0x6125A0` | `0x2125A0` | `__stdcall` | `void D2Common.EnableTxtCompilation(BOOL bDisable)` | define `[0x96C8B4] = (bDisable == 0)`; chamada por `0x44D9CC` com `0` quando `-txt` está presente | 🔷 + ✅ |
| `0x6122F0` | `0x2122F0` | `__stdcall` | `void* D2Common.CompileTxtFile(void* hArchive, const char* szName, BinField* pTbl, int* pRecordCount, int nRecordSize)` | compila `.txt` → grava `.bin` → lê `.bin` | 🔷 + ✅ |
| `0x6121F0` | `0x2121F0` | `__fastcall` | `BOOL D2Common.WriteBinFile(const char* szFileName /*ecx*/, const void* pBuffer /*edx*/, size_t nByteCount /*pilha*/)` | grava `DATA\GLOBAL\EXCEL\<szFileName>` via `fopen("wb")` | 🔷 + ✅ |
| `0x612280` | `0x212280` | `__fastcall` | `void* D2Common.ReadTableFile(void* hArchive /*ecx*/, const char* szPath /*edx*/, size_t* pSize /*pilha*/, const char* szSourceFile /*pilha*/, int nLine /*pilha*/)` | lê um `.txt` ou `.bin` via Storm | 🔷 + ✅ |
| `0x619300` | `0x219300` | `__stdcall` | `BOOL D2Common.LoadDataTables(void* hArchive, int a2, int a3)` | carrega todas as tabelas em `[0x744304]` | 🔷 + ✅ |
| `0x619140` | `0x219140` | `__stdcall` | `void D2Common.FreeDataTables()` | libera as tabelas; âncora de `[0x96C7F4]` (+7) e `[0x744308]` (+18) | 🔷 |

## 3. Tabela `charstats` (`CharStatsTxt`) em `DataTables`

Carregada por `0x613260` (`D2Common.LoadCharStatsTxt`, chamada por
`D2Common.LoadDataTables` em `0x619344`): 🔷 + ✅

- **Ponteiro da tabela:** `[0x96C7F4]` (`DataTables+0xBC4`, âncora
  `D2Common.GetCharStatsTxtBase` em `0x611D10` +1 e
  `D2Client.GetCharStatsTxtRecord` em `0x4833E0` +25).
- **Quantidade de registros:** `[0x96C7F8]` (`DataTables+0xBC8`, `7` no LoD
  vanilla, âncora `D2Client.GetCharStatsTxtRecord` +14).
- **Tamanho do registro:** **`0xC4`** bytes (196 bytes, âncora
  `D2Client.GetCharStatsTxtRecord` +20).
- **Função de consulta segura:** `0x4833E0` `D2Client.GetCharStatsTxtRecord`
  (RVA `0x0833E0`, `__fastcall CharStatsTxt*(int nCharClass /*ecx*/)`): devolve
  `&pCharStatsTxt[nCharClass]` se `0 <= nCharClass < nCharStatsTxtRecordCount`,
  ou `NULL` fora da faixa (**sem asserção**).

### Layout de `CharStatsTxt` (`0xC4` bytes)

Confirmado pelo descritor `BinField` montado em `0x613260`
(`D2Common.LoadCharStatsTxt`): 🔷 + ✅

| Offset | Tam. | Tipo | Coluna em `charstats.txt` | Significado | Conf. |
|---|---|---|---|---|---|
| `+0x00` | 32 | `wchar_t[16]` | — | nome da classe em UTF-16 | 🔷 |
| `+0x20` | 16 | `char[16]` | `class` | nome ASCII da classe (`"Amazon"`, `"Sorceress"`, `"Necromancer"`, `"Paladin"`, `"Barbarian"`, `"Druid"`, `"Expansion"`) | 🔷 + ✅ |
| `+0x30` | 1 | `uint8_t` | `str` | Força inicial (ex.: Bárbaro = `30`, Feiticeira = `10`) | 🔷 + ✅ |
| `+0x31` | 1 | `uint8_t` | `dex` | Destreza inicial | 🔷 + ✅ |
| `+0x32` | 1 | `uint8_t` | `int` | Energia inicial | 🔷 + ✅ |
| `+0x33` | 1 | `uint8_t` | `vit` | Vitalidade inicial | 🔷 + ✅ |
| `+0x34` | 1 | `uint8_t` | `stamina` | Vigor base inicial | 🔷 |
| `+0x35` | 1 | `uint8_t` | `hpadd` | Vida somada à Vitalidade inicial | 🔷 |
| `+0x36` | 1 | `uint8_t` | `PercentStr` | % Força | 🔷 |
| `+0x37` | 1 | `uint8_t` | `PercentInt` | % Energia | 🔷 |
| `+0x38` | 1 | `uint8_t` | `PercentDex` | % Destreza | 🔷 |
| `+0x39` | 1 | `uint8_t` | `PercentVit` | % Vitalidade | 🔷 |
| `+0x3A` | 1 | `uint8_t` | `ManaRegen` | divisor de regeneração de mana | 🔷 |
| `+0x3C` | 4 | `int32_t` | `ToHitFactor` | fator de acerto inicial | 🔷 |
| `+0x40` | 1 | `uint8_t` | `WalkVelocity` | velocidade andando | 🔷 |
| `+0x41` | 1 | `uint8_t` | `RunVelocity` | velocidade correndo | 🔷 |
| `+0x42` | 1 | `uint8_t` | `RunDrain` | consumo de vigor correndo | 🔷 |
| `+0x43` | 1 | `uint8_t` | `LifePerLevel` | vida por nível (em quartos de ponto) | 🔷 |
| `+0x44` | 1 | `uint8_t` | `StaminaPerLevel` | vigor por nível (em quartos) | 🔷 |
| `+0x45` | 1 | `uint8_t` | `ManaPerLevel` | mana por nível (em quartos) | 🔷 |
| `+0x46` | 1 | `uint8_t` | `LifePerVitality` | vida por ponto de Vitalidade (em quartos) | 🔷 |
| `+0x47` | 1 | `uint8_t` | `StaminaPerVitality` | vigor por ponto de Vitalidade (em quartos) | 🔷 |
| `+0x48` | 1 | `uint8_t` | `ManaPerMagic` | mana por ponto de Energia (em quartos) | 🔷 |
| `+0x49` | 1 | `uint8_t` | `BlockFactor` | bônus de bloqueio da classe | 🔷 |
| `+0x4C` | 4 | `char[4]` | `baseWClass` | classe de arma desarmada (`"hth"`) | 🔷 |
| `+0x50` | 1 | `uint8_t` | `StatPerLevel` | pontos de atributo ganhos por nível (`5` no LoD vanilla) | 🔷 + ✅ |
| `+0x52` | 2 | `uint16_t` | `StrAllSkills` | chave de string para `+X para todas as habilidades da classe` | 🔷 |
| `+0x54` | 6 | `uint16_t[3]` | `StrSkillTab1..3` | chaves de string das 3 abas de habilidades | 🔷 |
| `+0x5A` | 2 | `uint16_t` | `StrClassOnly` | chave de string `(Apenas <Classe>)` | 🔷 |
| `+0x5C` | 80 | `StartingItem[10]` | `item1..item10` | 10 itens iniciais (`char code[4]`, `uint8_t loc`, `uint8_t count`, `uint16_t pad`) | 🔷 |
| `+0xAC` | 2 | `int16_t` | `StartSkill` | habilidade inicial | 🔷 |
| `+0xAE` | 20 | `int16_t[10]` | `Skill 1..10` | primeiras 10 habilidades da classe | 🔷 |

## 4. Resumo das tabelas já mapeadas em `DataTables` (`[0x744304] = 0x96BC30`)

| Tabela (`.txt` / `.bin`) | Offset do ponteiro em `DataTables` | Offset da contagem em `DataTables` | Tam. do registro | Spec |
|---|---|---|---|---|
| `monstats` | `+0xA78` (`[0x96C6A8]`) | `+0xA80` (`[0x96C6B0]`) | `0x1A8` (424 B) | [`game-exe-1.14d-units.md` §4](game-exe-1.14d-units.md) |
| `skilldesc` | `+0xB8C` (`[0x96C7BC]`) | `+0xB94` (`[0x96C7C4]`) | `0x120` (288 B) | [`game-exe-1.14d-character.md` §3.3](game-exe-1.14d-character.md) |
| `skills` | `+0xB98` (`[0x96C7C8]`) | `+0xBA0` (`[0x96C7D0]`) | `0x23C` (572 B) | [`game-exe-1.14d-character.md` §3.3](game-exe-1.14d-character.md) |
| `charstats` | `+0xBC4` (`[0x96C7F4]`) | `+0xBC8` (`[0x96C7F8]`) | `0xC4` (196 B) | §3 acima |
| `leveldefs` | `[0x96C890]` (`+0xC60`) | `+0xC5C` (`[0x96C88C]`) | `0x9C` (156 B) | [`game-exe-1.14d-levels-automap.md` §2.7](game-exe-1.14d-levels-automap.md) |
| `objects` | `[0x96D470]` | `[0x96D474]` | `0x1C0` (448 B) | [`game-exe-1.14d-units.md` §5](game-exe-1.14d-units.md) |

## 5. Resumo para o INI

`D2Common.CompileTxtFile`, `D2Common.WriteBinFile`, `D2Common.ReadTableFile`,
`D2Common.EnableTxtCompilation`, `D2Common.LoadDataTables`,
`D2Common.FreeDataTables`, `D2Common.GetCharStatsTxtBase`,
`D2Common.LoadCharStatsTxt`, `D2Client.GetCharStatsTxtRecord`.

