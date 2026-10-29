# Especificação: formato DCC (sprites comprimidos por células do Diablo II)

> **Nota (2026-10-28):** referências a `src/…`, `tests/…` e às camadas
> `platform/`/`formats/`/`engine/`/`game/` abaixo pertencem à fase de
> reimplementação (descontinuada e apagada). Este
> documento segue válido como **referência do formato** para quem faz
> ferramentas de mod. Escopo atual: [`SCOPE.md`](../SCOPE.md).

> **Versão da evidência: 1.14d.** Estrutura validada nos arquivos `.dcc` de
> `d2char.mpq` e `d2data.mpq` (1.14d) e documentada na referência comunitária
> Paul Siramy / OpenDiablo2 (`docs/file-formats/dc6-dcc-sprites.md`).
> Nenhum asset proprietário é incluído no repositório.

## 1. Visão geral

O formato **DCC** (*Direction Cell Compression*) armazena animações
multidirecionais de unidades (personagens, monstros, mísseis e sombras) com
forte compressão espacial e temporal baseada em células de 4×4 pixels e
múltiplos sub-fluxos de bits (*bitstreams* LSB-first).

Cada arquivo DCC contém:
1. Cabeçalho do arquivo (**15 bytes**) + tabela de offsets por direção
   (`num_directions × 4` bytes).
2. Um bloco comprimido independente por direção (`0 .. num_directions - 1`),
   que decodifica `frames_per_direction` quadros indexados de 8 bits (0 =
   transparente, 1..255 = índice na paleta do ato).

## 2. Cabeçalho do Arquivo (little-endian)

| Offset | Tipo | Nome | Descrição |
|---|---|---|---|
| `0x00` | `uint8` | `signature` | Sempre `0x74` (`116`). |
| `0x01` | `uint8` | `version` | Sempre `6`. |
| `0x02` | `uint8` | `num_directions` | Número de direções (`1..32`). |
| `0x03` | `int32` | `frames_per_direction` | Quadros por direção (`> 0`). |
| `0x07` | `int32` | `tag` | Sempre `1`. |
| `0x0B` | `uint32` | `final_dc6_size` | Tamanho estimado descomprimido. |
| `0x0F` | `uint32[num_directions]` | `dir_offsets` | Offset absoluto em bytes de cada direção no arquivo. |

## 3. Leitura de Bits (LSB-first)

Todos os fluxos de bits de uma direção são lidos **LSB-first** (bit 0 = bit
menos significativo do primeiro byte, bit 7 = bit mais significativo do
primeiro byte):
- `ReadUnsigned(bits)`: concatena `bits` bits (`b = 0 .. bits-1`) como
  `value |= (bit << b)`.
- `ReadSigned(bits)`: lê `ReadUnsigned(bits)` e estende o sinal a partir do bit
  `bits - 1` (se `bits == 0`, retorna `0`).

### Tabela de Largura de Bits (`kCrazyBitTable`)

Os campos de cabeçalho de quadro codificam a largura em bits usando um índice
de 4 bits (`0..15`) mapeado por:

```text
Índice:  0  1  2  3  4  5   6   7   8   9  10  11  12  13  14  15
Bits:    0  1  2  4  6  8  10  12  14  16  20  24  26  28  30  32
```

## 4. Cabeçalho de Direção e Quadros

A partir de `dir_offsets[d]`, abre-se um bitstream principal:
1. `out_size_coded`: `ReadUnsigned(32)`
2. `compression_flags`: `ReadUnsigned(2)`
   - bit 0 (`0x1`): `has_raw_pixel_encoding` (`encoding_type` e `raw_pixel_codes` presentes)
   - bit 1 (`0x2`): `has_equal_cells` (`equal_cells` presente)
3. Sete índices de 4 bits na `kCrazyBitTable`:
   - `variable0_bits`
   - `width_bits`
   - `height_bits`
   - `x_offset_bits`
   - `y_offset_bits`
   - `optional_data_bits`
   - `coded_bytes_bits`
4. Para cada quadro `f` em `[0, frames_per_direction)`:
   - `variable0 = ReadUnsigned(variable0_bits)`
   - `width = ReadUnsigned(width_bits)`
   - `height = ReadUnsigned(height_bits)`
   - `x_offset = ReadSigned(x_offset_bits)`
   - `y_offset = ReadSigned(y_offset_bits)`
   - `optional_bytes = ReadUnsigned(optional_data_bits)`
   - `coded_bytes = ReadUnsigned(coded_bytes_bits)`
   - `frame_bottom_up = ReadUnsigned(1)` (1 = bottom-up, 0 = top-down)
   - Caixa do quadro (`box`):
     - `left = x_offset`
     - `top = frame_bottom_up ? y_offset : (y_offset - height + 1)`
     - `right = left + width`
     - `bottom = top + height`
   - Caixa da direção (`dir_box`): união das caixas de todos os quadros com
     `width > 0 && height > 0`.
5. Dados opcionais: se a soma de `optional_bytes` de todos os quadros for
   `> 0`, o leitor alinha para o próximo limite de byte (`(bit_pos + 7) & ~7`)
   e avança `sum(optional_bytes) * 8` bits.
6. Tamanhos dos sub-fluxos de bits:
   - `equal_cells_bits = (compression_flags & 2) ? ReadUnsigned(20) : 0`
   - `pixel_mask_bits = ReadUnsigned(20)`
   - `encoding_type_bits = (compression_flags & 1) ? ReadUnsigned(20) : 0`
   - `raw_pixel_bits = (compression_flags & 1) ? ReadUnsigned(20) : 0`
7. Mapa de paleta (`palette_entries`): 256 bits lidos em ordem (`0..255`). Se
   o bit `i` for `1`, o valor `i` é adicionado sequencialmente à tabela
   compacta `palette_lookup[]`.
8. Os cinco sub-bitstreams começam imediatamente após `palette_entries`:
   - `equal_cells_stream`: no bit atual, tamanho `equal_cells_bits`
   - `pixel_mask_stream`: após `equal_cells_stream`, tamanho `pixel_mask_bits`
   - `encoding_type_stream`: após `pixel_mask_stream`, tamanho `encoding_type_bits`
   - `raw_pixel_stream`: após `encoding_type_stream`, tamanho `raw_pixel_bits`
   - `pixel_code_and_displacers_stream`: após `raw_pixel_stream`, até o final da direção.

## 5. Grade de Células (Direction Grid e Frame Grid)

### 5.1 Grade da Direção
- `dir_width = dir_box.right - dir_box.left`
- `dir_height = dir_box.bottom - dir_box.top`
- Número de células horizontais:
  `h_cells = (dir_width <= 1) ? 1 : (1 + (dir_width - 2) / 4)`
- Número de células verticais:
  `v_cells = (dir_height <= 1) ? 1 : (1 + (dir_height - 2) / 4)`
- Larguras das colunas (`cell_widths[x]`):
  - Se `h_cells == 1`: `cell_widths[0] = dir_width`
  - Caso contrário: `cell_widths[0] = 4`, `cell_widths[1..h_cells-2] = 4`,
    `cell_widths[h_cells - 1] = dir_width - 4 * (h_cells - 1)`.
- Alturas das linhas (`cell_heights[y]`): análogo com `dir_height` e `v_cells`.

### 5.2 Grade de Cada Quadro
Para um quadro `f` com `width > 0 && height > 0`:
- `w1 = 4 - ((frame.box.left - dir_box.left) % 4)`
- `h1 = 4 - ((frame.box.top - dir_box.top) % 4)`
- Se `frame.width - w1 <= 1`, `frame_h_cells = 1`; senão
  `frame_h_cells = 2 + (frame.width - w1 - 2) / 4`.
- Se `frame.height - h1 <= 1`, `frame_v_cells = 1`; senão
  `frame_v_cells = 2 + (frame.height - h1 - 2) / 4`.
- Larguras das células do quadro:
  - Se `frame_h_cells == 1`: `[frame.width]`
  - Senão: `cell_w[0] = w1`, intermediárias `= 4`, última `= frame.width - w1 - 4 * (frame_h_cells - 2)`.
- Coordenadas de cada célula `(cx, cy)` do quadro e índice na grade da direção:
  - `cell_x0 = (frame.box.left - dir_box.left) / 4 + cx`
  - `cell_y0 = (frame.box.top - dir_box.top) / 4 + cy`
  - `buffer_index = cell_y0 * h_cells + cell_x0`.

## 6. Estágio 1 (`FillPixelBuffer`) e Estágio 2 (`GenerateFrames`)

### 6.1 Estágio 1: Preenchimento do Buffer de Células (`pixel_buffer`)
Percorre cada quadro `f` e cada célula do quadro `(cy, cx)` mapeada para a
célula de direção `buffer_cell = cell_y0 * h_cells + cell_x0`:
- Se a célula de direção já foi visitada num quadro anterior:
  - Se `equal_cells_bits > 0` e `equal_cells_stream.ReadUnsigned(1) == 1` e
    as dimensões `(cell_w, cell_h)` e offsets `(x_off, y_off)` coincidem com
    a última visita daquela célula de direção: copia a entrada do `pixel_buffer`
    anterior (`[0..3] = 0`, mas no Estágio 2 reutiliza o bitmap da célula!).
    Na representação canônica de `pixel_buffer`, quando `equal_cell == 1`,
    nenhum `pixel_mask` é lido e `pb_entry.val = {0, 0, 0, 0}` naquela célula
    (ou o Estágio 2 salta diretamente copiando/mantendo a célula).
  - Caso contrário, lê `pixel_mask = pixel_mask_stream.ReadUnsigned(4)`.
- Se é a primeira visita da célula de direção: `pixel_mask = 0x0F`.
- Se `pixel_mask != 0`:
  - `encoding_type = (encoding_type_bits > 0) ? encoding_type_stream.ReadUnsigned(1) : 0`.
  - `num_pixels = kPixelMaskLookup[pixel_mask]` (popcount de 4 bits).
  - Decodifica `num_pixels` índices em `read_pixel[0..3]`:
    - Se `encoding_type == 1`: lê códigos de 8 bits de `raw_pixel_stream`
      enquanto diferentes do anterior (parando cedo se repetir o valor
      imediatamente anterior).
    - Se `encoding_type == 0`: acumula deslocadores de 4 bits de
      `pixel_code_and_displacers_stream` (`displacer = ReadUnsigned(4)`,
      enquanto `displacer == 15` soma `15` e lê outro de 4 bits; se `displacer == 0`
      termina cedo os pixels restantes).
  - Combina `read_pixel` com os 4 valores da entrada anterior da mesma célula
    de direção segundo os bits de `pixel_mask`: para cada slot `i` em `0..3`,
    se `(pixel_mask & (1 << i)) != 0`, grava o próximo pixel decodificado de
    `read_pixel` em ordem reversa (`curr_idx = decoded_pixels - 1` até `0`);
    se `(pixel_mask & (1 << i)) == 0` e há `prev_pb`, preserva o mesmo slot
    `curr_pb.val[i] = prev_pb->val[i]` (validado bit a bit nas 16 direções de
    `amTRlittn1ht.dcc`).

### 6.2 Estágio 2: Geração dos Bitmaps dos Quadros
- Converte todos os valores de `pixel_buffer` através de `palette_lookup`.
- Mantém um canvas da direção (`dir_width × dir_height`) e o último estado de
  cada célula da direção.
- Para cada quadro `f` e cada célula `(cy, cx)`:
  - Se `(pb.val[0] == pb.val[1]) && (pb.val[1] == pb.val[2]) && (pb.val[2] == pb.val[3])`:
    - Se `pb.val[0] == 0` (e a célula veio de `equal_cell` quando as dimensões
      e posição coincidem, ou cor uniforme `0`): quando a célula tem mesma
      largura/altura/offset da última visita e `pb.val == {0,0,0,0}` sem
      mudança, mantém os pixels existentes na região da célula; se a dimensão
      mudou, limpa a região com `0`. Se `pb.val[0] != 0`, preenche a célula com
      `pb.val[0]`.
  - Caso contrário, para cada pixel `(py, px)` de `0..cell_h-1` e `0..cell_w-1`,
    lê índice de 1 bit (se `pb.val[1] == pb.val[2]`) ou 2 bits de
    `pixel_code_and_displacers_stream` e escreve `pb.val[code]` no canvas da
    direção.
- Ao final das células do quadro `f`, extrai o sub-retângulo
  `[frame.box.left - dir_box.left, frame.box.top - dir_box.top, width, height]`
  para `DccFrame::indexed_pixels`.

## 7. Contrato do Adaptador (`src/formats/dcc.h`)

```cpp
struct DccFrame {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::int32_t x_offset = 0;
  std::int32_t y_offset = 0;
  bool bottom_up = false;
  std::vector<std::uint8_t> indexed_pixels;
};
```

- `Dcc::FromBytes(data, size)` valida cabeçalho e decodifica todas as direções
  e quadros.
- `Dcc::Frame(direction, frame)` retorna o `DccFrame` correspondente.
