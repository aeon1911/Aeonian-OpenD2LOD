# Especificação: formato DS1 (Diablo II) — mapa pré-definido

> **Nota (2026-10-28):** referências a `src/…`, `tests/…` e às camadas
> `platform/`/`formats/`/`engine/`/`game/` abaixo pertencem à fase de
> reimplementação (descontinuada e apagada). Este
> documento segue válido como **referência do formato** para quem faz
> ferramentas de mod. Escopo atual: [`SCOPE.md`](../SCOPE.md).

> **Versão da evidência: 1.14b/1.14d (dados).** O alvo do projeto é **1.14d**.
> Estrutura confirmada nos arquivos de `d2data.mpq`/overlay 1.14d e confrontada
> com a documentação pública do formato (OpenDiablo2 `d2ds1`). A implementação
> é própria; nenhum asset é versionado.
>
> **Escopo:** versões **16, 17 e 18** (usadas pelas cidades, caves e outdoors de
> Act 1–5, como `townE1.ds1` v18 e `caveNSEW2.ds1` v16).
> Versões 3–15 têm variações de header; o adaptador rejeita as não
> suportadas por enquanto.

## 1. Cabeçalho

Ordem little-endian:

| Campo | Tipo | Observação |
|---|---|---|
| version | int32 | 16, 17 ou 18 (ex.: `caveNSEW2.ds1` v16, `townE1.ds1` v18) |
| width_minus_1 | int32 | largura real = valor + 1 |
| height_minus_1 | int32 | altura real = valor + 1 |
| act | int32 | 0-based (v ≥ 8) |
| substitution_type | int32 | 0/1/2 (v ≥ 10) |
| file_count | int32 | v ≥ 3 |
| file_count × string | ASCIIZ | caminhos de `.tg1`/`.dt1` |

Para v16 não há `unknown1` (só v9–v13). Em seguida:

| Campo | Tipo |
|---|---|
| num_walls | int32 |
| num_floors | int32 |

O `file` do header (ex.: `data\global\tiles\ACT1\Caves\cave.tg1`) aponta para
o tile group; na prática o adaptador troca `.tg1` por `.dt1` e carrega esse
DT1.

## 2. Camadas (v16/v17)

Cada camada é `width * height` dwords, na ordem:

1. para cada wall: camada de **wall** e camada de **orientation**;
2. camada de **floor**;
3. camada de **shadow** (sempre 1);
4. camada de **substitution** se `substitution_type` ∈ {1,2}.

Cada célula é lida em ordem `y` (0..height-1) e `x` (0..width-1).

### Decodificação de um dword

Bits comuns a wall/floor/shadow:

```text
prop1     = bits 0..7    (8 bits)
sequence  = bits 8..13   (6 bits)
unknown1  = bits 14..19  (6 bits)
style     = bits 20..25  (6 bits)
unknown2  = bits 26..30  (5 bits)
hidden    = bit 31
```

- **Floor:** `style` (main index), `sequence` (sub index), `prop1`.
- **Wall:** os mesmos campos do dword de wall; o dword de orientation fornece
  `type = orientation & 0xFF` (0–18) e os bits altos são `zero`.
- **Shadow:** `style`, `sequence`; desenhado com o tipo 13 do DT1.

O índice de variação (`random_index`) escolhe uma das opções de DT1 com o
mesmo `(style, sequence, type)`. Para o adaptador inicial, usa a opção 0.

## 3. Objetos e NPCs

Depois das camadas vêm `num_objects`, cada objeto com 5 int32
(`type, id, x, y, flags`), e depois NPCs/paths (v > 14). O adaptador inicial
ignora objetos/NPCs; eles entram nas próximas entregas.

## 4. Renderização e Caminhabilidade

- Projeção isométrica: `screen_x = (x - y) * 80`, `screen_y = (x + y) * 40`.
- A imagem do tile DT1 é desenhada com deslocamento `-80` em X e `YAdjust`
  em Y:
  - Floors (`type == 0`) e Shadows (`type == 13`): `YAdjust = 0`.
  - Roofs (`type == 15`): `YAdjust = -roof_height`.
  - Walls (`type != 0 && type != 13 && type != 15`): `YAdjust = min_y + 80`,
    onde `min_y = min(0, min(block.y))`.
- Célula ativa: uma célula de floor, shadow ou wall só é ativa se
  `prop1 != 0 && !hidden`. Note que `(style == 0, sequence == 0)` é um índice
  de tile válido (p.ex. 52 tiles em `cave.dt1` têm `style == 0`) e **não**
  significa célula vazia!
- Tipos especiais `type == 10` (`TileSpecialTile1`) e `type == 11`
  (`TileSpecialTile2`) são marcadores de lógica/editor (magenta no DT1) e
  **não** são desenhados na cena.
- Ordem: floors primeiro, depois shadows (`type == 13`), depois walls (por
  camada), com oclusão por `x + y`.
- Piso caminhável: célula com pelo menos um floor ativo cujo tile exista no
  conjunto DT1 carregado (rejeitando índices de void inexistentes como
  `style == 30` nas bordas das cavernas) e sem wall bloqueante ativa.

## 5. Vetores de teste

O teste `tests/ds1_test.cpp` constrói um DS1 v16 mínimo `2x2` com
`num_walls=0`, `num_floors=1`, uma camada de floor e uma de shadow, e verifica
largura/altura, `style`, `sequence` e `prop1` das células.

## Ver também

- `docs/specs/dt1-format.md`
- `docs/versions/1.14d.md`
