# Especificação: formato DT1 (Diablo II) — tileset

> **Nota (2026-10-28):** referências a `src/…`, `tests/…` e às camadas
> `platform/`/`formats/`/`engine/`/`game/` abaixo pertencem à fase de
> reimplementação (descontinuada e apagada). Este
> documento segue válido como **referência do formato** para quem faz
> ferramentas de mod. Escopo atual: [`SCOPE.md`](../SCOPE.md).

> **Versão da evidência: 1.14b/1.14d (dados).** Alvo do projeto: **1.14d**.
> Estrutura confrontada com a documentação pública (OpenDiablo2 `d2dt1`,
> Phrozen Keep). Implementação própria; nenhum asset versionado.

## 1. Cabeçalho

| Campo | Tipo | Observação |
|---|---|---|
| major_version | int32 | 7 |
| minor_version | int32 | 6 |
| unknown | byte[260] | ignorado |
| num_tiles | int32 | |
| body_position | int32 | offset da tabela de headers de tile |

## 2. Header de tile (96 bytes)

| Campo | Tipo |
|---|---|
| direction | int32 |
| roof_height | int16 |
| material_flags | uint16 |
| height | int32 (pode ser negativo; usar abs) |
| width | int32 |
| unknown1 | 4 bytes |
| type | int32 (orientação 0–18) |
| style | int32 (main index) |
| sequence | int32 (sub index) |
| rarity_frame_index | int32 |
| unknown2 | 4 bytes |
| sub_tile_flags | 25 bytes |
| unknown3 | 7 bytes |
| block_header_pointer | int32 (offset absoluto) |
| block_header_size | int32 |
| num_blocks | int32 |
| unknown4 | 12 bytes |

## 3. Header de bloco (20 bytes)

| Campo | Tipo |
|---|---|
| x | int16 |
| y | int16 |
| unknown1 | 2 bytes |
| grid_x | uint8 |
| grid_y | uint8 |
| format | int16 (0 = RLE, 1 = isométrico) |
| length | int32 |
| unknown2 | 2 bytes |
| file_offset | int32 |

Os dados do bloco começam em `block_header_pointer + file_offset`.

## 4. Decodificação da imagem do tile

- Para os blocos do tile, calcula-se:
  - `min_y = min(0, min(block.y))`
  - `max_y = max(0, max(block.y + 32))`
- Dimensões do canvas decodificado:
  `width` × `max(abs(height), max_y - min_y)`.
- `tile_y_offset = -min_y`; a imagem cobre `[min_y, max_y]` dos blocos sem
  truncar a base (`y >= 0`) das paredes altas.
- Para cada bloco, escrever os pixels do bloco na posição
  `(block.x, block.y + tile_y_offset)` da imagem.
- Deslocamento vertical na tela (`y_adjust`):
  - `type == 0` (floor) ou `type == 13` (shadow): `0`
  - `type == 15` (roof): `-roof_height`
  - demais tipos (walls/colunas): `min_y + 80`

### Formato RLE (`format == 0`)

Pares `(skip, count)` seguidos de `count` bytes de índice:

```text
skip = data[i]; count = data[i+1]; i += 2
if skip == 0 and count == 0: x = 0; y++  // nova scanline
else: x += skip; copia count bytes; x += count
```

`count` bytes são índices de paleta; 0 é transparente para o tile.

### Formato isométrico (`format == 1`)

15 scanlines com larguras `4, 8, 12, 16, 20, 24, 28, 32, 28, 24, 20, 16,
12, 8, 4` e início `x = 14, 12, 10, ..., 0, ..., 14`. Os bytes são copiados
nessa ordem (256 bytes no total).

## 5. Chave de seleção

O mapa DS1 referencia tiles por `(style, sequence, type)`. O adaptador
indexa todos os tiles do DT1 por essa tripla; se houver mais de um, usa o
primeiro (variação 0). `type` 0 = floor; 1–18 = paredes/especiais.

## 6. Renderização

- Desenhar a imagem decodificada com a paleta do ato (`ACT1/pal.dat`).
- Índice 0 = transparente.
- O tile é posicionado no mapa em `(x - y) * 80 - 80`,
  `(x + y) * 40 + YAdjust`.
- Paredes são desenhadas depois dos floors; oclusão por ordem de `x + y`.

## 7. Vetores de teste

`tests/dt1_test.cpp` constrói um DT1 mínimo com:
- 1 tile `width=32`, `height=32`, type 0, style 1, sequence 0;
- 1 bloco RLE em (0,0), com `(skip,count)` e pixels;
- 1 bloco isométrico sintético.
Verifica dimensões, chave e pixels decodificados.

## Ver também

- `docs/specs/ds1-format.md`
- `docs/versions/1.14d.md`
