# Especificação: formato DC6 (Diablo II) — decodificador de sprites

> **Nota (2026-10-28):** referências a `src/…`, `tests/…` e às camadas
> `platform/`/`formats/`/`engine/`/`game/` abaixo pertencem à fase de
> reimplementação (descontinuada e apagada). Este
> documento segue válido como **referência do formato** para quem faz
> ferramentas de mod. Escopo atual: [`SCOPE.md`](../SCOPE.md).

> **Versão da evidência: 1.13c (histórica).** O alvo do projeto é **1.14d**.
> Esta spec precisa ser revalidada contra os binários 1.14d antes de ser
> tratada como contrato. Ver [`docs/VERSIONS.md`](../VERSIONS.md).

> **Fase de Análise (Cleanroom).** Estrutura observada nos arquivos DC6 do
> usuário (ex.: `data\global\ui\FrontEnd\TitleScreen.DC6`, 1.13c) e
> confrontada com documentação pública de formato ([OpenDiablo2
> d2dc6](https://pkg.go.dev/github.com/OpenDiablo2/D2Shared/d2data/d2dc6)).
> A documentação foi usada apenas como hipótese; a implementação foi escrita
> a partir desta especificação e validada por testes sintéticos e pela
> renderização do arquivo real. Nenhum asset proprietário é incluído no
> repositório.

## 1. Cabeçalho (24 bytes, little-endian)

| Offset | Campo | Tipo | Observação |
|---|---|---|---|
| 0 | version | int32 | 6 na 1.13c |
| 4 | flags | uint32 | 1 |
| 8 | encoding | uint32 | 0 |
| 12 | termination | byte[4] | sentinela (`0xCDCDCDCD` ou `0xEEEEEEEE`) |
| 16 | directions | uint32 | 1 na maioria dos arquivos de UI |
| 20 | frames_per_direction | uint32 | total de frames = `directions * frames_per_direction` |

Em seguida vem uma tabela de `total_frames` ponteiros `uint32`, cada um
relativo ao início do arquivo, apontando para o cabeçalho do frame.

## 2. Cabeçalho de frame (32 bytes)

| Offset | Campo | Tipo |
|---|---|---|
| 0 | flipped | uint32 |
| 4 | width | uint32 |
| 8 | height | uint32 |
| 12 | offset_x | int32 |
| 16 | offset_y | int32 |
| 20 | unknown | uint32 |
| 24 | next_block | uint32 |
| 28 | length | uint32 |

Depois do cabeçalho vêm `length` bytes de dados RLE e 3 bytes de terminador.
`offset_x`/`offset_y` são o deslocamento do frame em relação à âncora do
sprite (usado para compor animações); para imagens de tela cheia são 0.

## 3. RLE por scanline

O frame é decodificado em um buffer `width * height` de índices de paleta.
Os dados são lidos byte a byte:

| Byte | Significado |
|---|---|
| `0x80` | fim da scanline; a próxima linha começa em `x = 0` |
| `0x81`–`0xFF` | pula `b & 0x7F` pixels transparentes (`x += b & 0x7F`) |
| `0x01`–`0x7F` | copia os próximos `b` bytes como índices opacos |

Detalhes fiéis observados:

- As linhas são armazenadas de **baixo para cima**: o primeiro `y` usado é
  `height - 1` e decrementa a cada `0x80`. O último `0x80` (em `y == 0`)
  encerra o frame.
- O frame não é necessariamente retangular em bytes: o número de bytes por
  linha varia; o separador é sempre `0x80`.
- Pixels pulados ficam transparentes (alfa 0 no renderizador); o índice de
  paleta 0 **não** é tratado como transparente por si só.
- `flipped` não foi observado como 1 nos arquivos usados; o adaptador expõe
  o campo e não o aplica (TODO(RE) para sprites espelhados).

## 4. Paleta

O DC6 não contém paleta. O chamador fornece uma paleta de 256 cores RGB
(768 bytes, ordem R,G,B), documentada em `docs/specs/palette-format.md`.
O índice 0 é uma cor válida (preto na paleta de menu).

## 5. Validação e erros

O decodificador do adaptador:

- rejeita arquivo truncado, versão diferente de 6, ponteiros fora do
  arquivo, `width * height == 0` ou estouro de tamanho;
- para de decodificar se o RLE acabar antes do frame completo, mantendo os
  pixels restantes transparentes;
- devolve `bool`/`std::optional` em vez de abortar.

## 6. Vetores de teste sintéticos

O teste `tests/dc6_test.cpp` constrói um DC6 em memória com 1 direção,
1 frame `4x2`. Os dados RLE (armazenados de baixo para cima) são:

```text
linha inferior (y=1):  04 11 22 33 44 80
linha superior (y=0):  81 02 55 66 81 80
```

Resultado esperado (`width=4`, `height=2`):

| (x,y) | índice | alfa |
|---|---|---|
| (0,1) | `0x11` | 255 |
| (1,1) | `0x22` | 255 |
| (2,1) | `0x33` | 255 |
| (3,1) | `0x44` | 255 |
| (0,0) | — | 0 (transparente) |
| (1,0) | `0x55` | 255 |
| (2,0) | `0x66` | 255 |
| (3,0) | — | 0 (transparente) |

O teste completo está em `tests/dc6_test.cpp` e cobre também versão
inválida, ponteiro fora do arquivo, dimensão zero e arquivo truncado.

## 7. Evidências

- `TitleScreen.DC6`: versão 6, 1 direção, 12 frames; frames de `256x256`,
  `32x256`, `256x88`, `32x88`; `length` confere com os separadores `0x80`.
- A renderização do frame 5 com a paleta `Menu0/pal.dat` reproduz a cena
  noturna de Tristram (fogueira e personagens), confirmando a ordem RLE e a
  ordem de linhas.
- O campo `length` de cada frame inclui os dados RLE, não o cabeçalho;
  `next_block` = início do próximo frame.

## Ver também

- `docs/specs/palette-format.md`
- `docs/specs/ds1-format.md` (não implementado nesta entrega)
- `docs/HANDOFF.md`
