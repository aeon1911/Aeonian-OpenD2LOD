# Especificação: Tabelas de Dados `.txt` (`data/global/excel/*.txt`)

> **Nota (2026-10-28):** referências a `src/…`, `tests/…` e às camadas
> `platform/`/`formats/`/`engine/`/`game/` abaixo pertencem à fase de
> reimplementação (descontinuada e apagada). Este
> documento segue válido como **referência do formato** para quem faz
> ferramentas de mod. Escopo atual: [`SCOPE.md`](../SCOPE.md).

> **Versão da evidência:** Diablo II: Lord of Destruction **1.14d**
> (`d2data.mpq`, `d2exp.mpq`, `patch_d2.mpq` e diretório `data/global/excel/`).
> Validado sobre `charstats.txt`, `levels.txt`, `lvltypes.txt` e `lvlprest.txt`.

## 1. Visão Geral e Papel no Sistema de Modding

O Diablo II: Lord of Destruction armazena suas tabelas de regras de jogo em
arquivos texto separados por tabulação (`TSV`) dentro de `data/global/excel/`.
Quando lidos pelo `AssetStore` (que prioriza o diretório `data/` local sobre
`patch_d2.mpq` > `d2exp.mpq` > `d2data.mpq`), qualquer modder pode editar ou
adicionar arquivos `.txt` em `data/global/excel/` sem precisar recompilar o
motor ou reempacotar MPQs.

## 2. Estrutura do Formato

1. **Quebra de linha:** `CRLF` (`\r\n`) ou `LF` (`\n`).
2. **Separador de colunas:** tabulação ASCII (`\t`, `0x09`).
3. **Linha de cabeçalho (linha 0):** nomes das colunas. A busca por coluna no
   motor deve ser **case-insensitive** (ex.: `"class"`, `"str"`, `"WalkVelocity"`,
   `"#walk"`).
4. **Linhas separadoras (`Expansion`) e vazias:**
   - Tabelas que combinam dados Classic e Expansion (`charstats.txt`,
     `levels.txt`, `weapons.txt`, etc.) contêm uma linha intermediária cuja
     primeira coluna é `"Expansion"` (sem dados de entidade). O parser deve
     ignorar linhas vazias e linhas cuja primeira coluna seja `"Expansion"`
     (case-insensitive).

## 3. Colunas Relevantes Validadas em 1.14d

### 3.1 `data/global/excel/charstats.txt`

| Coluna | Tipo | Exemplo (`Amazon`) | Exemplo (`Sorceress`) | Exemplo (`Druid`) |
|---|---|---|---|---|
| `class` | string | `Amazon` | `Sorceress` | `Druid` |
| `str` | int | `20` | `10` | `15` |
| `dex` | int | `25` | `25` | `20` |
| `int` | int | `15` | `35` | `20` |
| `vit` | int | `20` | `10` | `25` |
| `stamina` | int | `84` | `74` | `84` |
| `hpadd` | int | `30` | `30` | `30` |
| `WalkVelocity` | int | `6` | `6` | `6` |
| `RunVelocity` | int | `9` | `9` | `9` |
| `RunDrain` | int | `20` | `20` | `20` |
| `LifePerLevel` | int (quartos) | `8` (+2/lvl) | `4` (+1/lvl) | `6` (+1.5/lvl) |
| `ManaPerLevel` | int (quartos) | `6` (+1.5/lvl) | `8` (+2/lvl) | `8` (+2/lvl) |
| `LifePerVitality` | int (quartos) | `12` (+3/vit) | `8` (+2/vit) | `8` (+2/vit) |
| `ManaPerMagic` | int (quartos) | `6` (+1.5/int) | `8` (+2/int) | `8` (+2/int) |
| `baseWClass` | string | `hth` | `hth` | `hth` |

### 3.2 `data/global/excel/lvltypes.txt`

| Coluna | Tipo | Descrição |
|---|---|---|
| `Name` | string | Nome do grupo de tiles (ex.: `Act 1 - Town`, `Act 1 - Cave`) |
| `Id` | int | Identificador numérico do tipo de nível (`1` = Town, `3` = Cave) |
| `File 1` .. `File 32` | string | Caminhos relativos a `data/global/tiles/` ou `"0"` se vazio |
