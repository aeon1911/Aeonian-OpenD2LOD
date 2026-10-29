# Especificação: Fontes Bitmap do Diablo II (`.tbl` + `.dc6`)

> **Nota (2026-10-28):** referências a `src/…`, `tests/…` e às camadas
> `platform/`/`formats/`/`engine/`/`game/` abaixo pertencem à fase de
> reimplementação (descontinuada e apagada). Este
> documento segue válido como **referência do formato** para quem faz
> ferramentas de mod. Escopo atual: [`SCOPE.md`](../SCOPE.md).

> **Versão da evidência:** Diablo II: Lord of Destruction **1.14d**
> (`d2data.mpq` / `d2exp.mpq`, diretório virtual `data/local/font/latin/`).
> Validado sobre `font16.{dc6,tbl}`, `fontexocet10.{dc6,tbl}`,
> `fontexocet8.{dc6,tbl}`, `fontformal12.{dc6,tbl}`, `fontformal11.{dc6,tbl}` e
> `font6.{dc6,tbl}`.

## 1. Visão Geral

Cada fonte de texto do Diablo II é composta por um par de arquivos em
`data/local/font/<locale>/<nome>.{dc6,tbl}`:
1. `<nome>.dc6`: sprite DC6 com `1` direção e `256` quadros (um quadro por
   código de caractere de 8 bits `0..255`).
2. `<nome>.tbl`: tabela binária de métricas de glifos (`3596` bytes para fontes
   de 256 glifos), que define o avanço horizontal proporcional (`width`), a
   altura de linha (`height`) e o índice de quadro no `.dc6` (`frame_index`).

Sem o arquivo `.tbl`, os quadros do `.dc6` possuem caixa fixa (ex.: `14×16` em
`font16.dc6` ou `35×30` em `fontformal12.dc6`), o que produziria espaçamento
monoespaçado incorreto.

## 2. Layout Binário do Arquivo `.tbl` de Fonte

Todos os inteiros de múltiplos bytes são little-endian.

### 2.1 Cabeçalho (`12` bytes)

| Offset | Tamanho | Tipo | Descrição |
|---|---|---|---|
| `0x00` | 5 | `char[5]` | Assinatura mágica `"Woo!\x01"` (`57 6F 6F 21 01`) |
| `0x05` | 5 | `uint8[5]` | Constante de cabeçalho (`00 00 00 00 01`) |
| `0x0A` | 1 | `uint8` | `line_height` — altura nominal da linha em pixels (ex.: `10` em `font16`, `20` em `fontformal12`) |
| `0x0B` | 1 | `uint8` | `cap_width` — largura máxima/avanço padrão |

### 2.2 Registros de Glifos (`N × 14` bytes, a partir de `0x0C`)

Para fontes latinas padrão, `N = 256` (`12 + 256 × 14 = 3596` bytes). Cada
registro de 14 bytes possui:

| Offset | Tamanho | Tipo | Descrição |
|---|---|---|---|
| `+0x00` | 2 | `uint16` | `code` — código do caractere (ex.: `0x0041` para `'A'`) |
| `+0x02` | 1 | `uint8` | `zero0` (`0x00`) |
| `+0x03` | 1 | `uint8` | `width` — avanço horizontal proporcional do glifo em pixels |
| `+0x04` | 1 | `uint8` | `height` — altura métrica do glifo em pixels |
| `+0x05` | 1 | `uint8` | `defined` (`0x01`) |
| `+0x06` | 2 | `uint16` | `zero1` (`0x0000`) |
| `+0x08` | 2 | `uint16` | `frame_index` — índice do quadro correspondente no `.dc6` |
| `+0x0A` | 4 | `uint32` | Reservado (`0x00000000`) |

## 3. Evidência Empírica (1.14d)

- `font16.tbl` (`3596` bytes, `line_height = 10`, `dc6 = 14×16`):
  - `' '` (`32`): `width = 8`, `height = 10`, `frame_index = 32`
  - `'1'` (`49`): `width = 5`, `height = 10`, `frame_index = 49`
  - `'A'` (`65`): `width = 12`, `height = 10`, `frame_index = 65`
  - `'I'` (`73`): `width = 5`, `height = 10`, `frame_index = 73`
- `fontexocet10.tbl` (`3596` bytes, `line_height = 10`, `dc6 = 17×18`):
  - `' '` (`32`): `width = 9`, `'A'` (`65`): `width = 13`, `'I'` (`73`): `width = 6`
- `fontformal12.tbl` (`3596` bytes, `line_height = 20`, `dc6 = 35×30`):
  - `' '` (`32`): `width = 6`, `'A'` (`65`): `width = 12`, `'I'` (`73`): `width = 6`

## 4. Modos de Pintura de Glifos

1. **Modo Paleta Nativa (`kPaletteDirect`)**:
   - Usado em fontes entalhadas em pedra (`fontexocet10`, `fontexocet8`) sobre
     botões (`WideButtonBlank.dc6`, `MediumButtonBlank.dc6`), onde o próprio
     quadro DC6 contém o centro escuro entalhado (índice `0x0F`) e o bisel
     iluminado nas bordas (índice `0x20`).
2. **Modo Luminância Colorizada (`kLuminanceTint`)**:
   - Usado em fontes de interface e avisos (`font16`, `fontformal12`,
     `fontformal11`, `font6`), onde a luminância da paleta base é modulada pela
     cor de texto solicitada (Branco, Ouro D2 `#C7B377`, Azul `#6969FF`,
     Vermelho `#FF4D4D`, Verde `#00FF00`, Amarelo `#FFFF64`, Cinza `#909090`).
