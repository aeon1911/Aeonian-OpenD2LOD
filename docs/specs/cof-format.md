# Especificação: formato COF (Components Order File do Diablo II)

> **Nota (2026-10-28):** referências a `src/…`, `tests/…` e às camadas
> `platform/`/`formats/`/`engine/`/`game/` abaixo pertencem à fase de
> reimplementação (descontinuada e apagada). Este
> documento segue válido como **referência do formato** para quem faz
> ferramentas de mod. Escopo atual: [`SCOPE.md`](../SCOPE.md).

> **Versão da evidência: 1.14d.** Estrutura validada nos arquivos
> `data\global\chars\<token>\cof\<token><modo><arma>.cof` de `d2data.mpq`
> e `d2char.mpq` (1.14d) e consistente com a documentação comunitária de
> formato binário (`docs/file-formats/dc6-dcc-sprites.md`).
> Nenhum asset proprietário é incluído no repositório.

## 1. Visão geral

O formato **COF** (*Components Order File*) descreve como compor as camadas de
equipamento/corpo (DCC) de uma unidade animada (personagem, monstro ou objeto)
para um determinado `(token, animation_mode, weapon_class)` (ex.:
`AMTNHTH.COF` = Amazona, modo `TN` Town Neutral, classe de arma `HTH`
Hand-to-Hand).

Um arquivo COF contém:
1. Cabeçalho fixo de **28 bytes**.
2. Tabela de camadas (`num_layers × 9` bytes).
3. Tabela de gatilhos de evento por quadro (`frames_per_direction × 1` byte).
4. Tabela tridimensional de prioridade de desenho por direção, quadro e camada
   (`num_directions × frames_per_direction × num_layers` bytes).

Tamanho total exato esperado:
`28 + 9 * num_layers + frames_per_direction + num_directions * frames_per_direction * num_layers`.

## 2. Cabeçalho (28 bytes, little-endian)

| Offset | Tipo | Nome | Descrição |
|---|---|---|---|
| `0x00` | `uint8` | `num_layers` | Número de camadas (1..16). |
| `0x01` | `uint8` | `frames_per_direction` | Quadros por direção (> 0). |
| `0x02` | `uint8` | `num_directions` | Direções da animação (tipicamente 1, 4, 8, 16 ou 32). |
| `0x03` | `uint8` | `version` | Versão do cabeçalho (tipicamente `0x14` = 20). |
| `0x04` | `uint32` | `unknown` | Campo de flags/body (4 bytes). |
| `0x08` | `int32` | `x_min` | Caixa delimitadora X mínimo. |
| `0x0C` | `int32` | `x_max` | Caixa delimitadora X máximo. |
| `0x10` | `int32` | `y_min` | Caixa delimitadora Y mínimo. |
| `0x14` | `int32` | `y_max` | Caixa delimitadora Y máximo. |
| `0x18` | `int32` | `anim_rate` | Velocidade base da animação (256 = 100% a 25 fps). |

## 3. Registros de Camada (`num_layers × 9` bytes)

Logo após o cabeçalho (`offset 0x1C`), há `num_layers` estruturas de 9 bytes:

| Offset | Tipo | Nome | Descrição |
|---|---|---|---|
| `+0x00` | `uint8` | `type` | Índice do componente composto (`CompositeType`, 0..15). |
| `+0x01` | `uint8` | `shadow` | `1` se a camada projeta sombra, `0` caso contrário. |
| `+0x02` | `uint8` | `selectable` | `1` se a camada é selecionável/ativa. |
| `+0x03` | `uint8` | `transparent` | `1` se usa transparência especial (`override_trans_lvl`). |
| `+0x04` | `uint8` | `draw_effect` | Efeito de blending (`DrawEffect`, 0..7). |
| `+0x05` | `char[4]` | `weapon_class` | Código ASCIIZ da classe de arma da camada (ex.: `"hth\0"`). |

### Mapeamento `CompositeType` (`0..15`)

| Índice | Código | Significado |
|---|---|---|
| `0` | `HD` | Head (Cabeça) |
| `1` | `TR` | Torso (Tronco) |
| `2` | `LG` | Legs (Pernas) |
| `3` | `RA` | Right Arm (Braço direito) |
| `4` | `LA` | Left Arm (Braço esquerdo) |
| `5` | `RH` | Right Hand (Mão direita / arma principal) |
| `6` | `LH` | Left Hand (Mão esquerda / arma secundária) |
| `7` | `SH` | Shield (Escudo) |
| `8..15` | `S1..S8` | Special 1..8 |

## 4. Gatilhos de Quadro (`frames_per_direction` bytes)

Para cada quadro `f` em `[0, frames_per_direction)`:
- `0`: sem evento
- `1`: ataque / impacto corpo a corpo
- `2`: disparo de míssil
- `3`: efeito sonoro
- `4`: gatilho de skill

## 5. Tabela de Prioridade (`num_directions × frames_per_direction × num_layers` bytes)

Armazenada em ordem `[direction][frame][layer_order]`:
- Para uma direção `d` (`0 <= d < num_directions`) e quadro `f`
  (`0 <= f < frames_per_direction`), os `num_layers` bytes subsequentes
  indicam a ordem de pintura **de trás para frente** (valores de
  `CompositeType` `0..15`).

## 6. Mapeamento de Direções (COF vs DCC em 8 e 16 direções)

Enquanto os arquivos DCC armazenam as direções intercalando cantos diagonais
(`0..3` = `SW, NW, NE, SE`), cardeais (`4..7` = `S, W, N, E`) e intermediárias
(`8..15`), a tabela `priority` do COF armazena as direções em **sentido horário
contínuo** a partir de `SW` (`0`):
- Para `num_directions == 8`: `kDccToCof8 = {0, 2, 4, 6, 7, 1, 3, 5}`.
- Para `num_directions == 16`: `kDccToCof16 = {0, 4, 8, 12, 14, 2, 6, 10, 1, 3, 5, 7, 9, 11, 13, 15}` (ex.: direção DCC `4` = Sul mapeia para direção COF `14`).

## 7. Contrato do Adaptador (`src/formats/cof.h`)

```cpp
enum class CofLayerType : std::uint8_t {
  kHead = 0,
  kTorso = 1,
  kLegs = 2,
  kRightArm = 3,
  kLeftArm = 4,
  kRightHand = 5,
  kLeftHand = 6,
  kShield = 7,
  kSpecial1 = 8,
  kSpecial2 = 9,
  kSpecial3 = 10,
  kSpecial4 = 11,
  kSpecial5 = 12,
  kSpecial6 = 13,
  kSpecial7 = 14,
  kSpecial8 = 15,
};
```

- `Cof::FromBytes(data, size)` valida ponteiro, limites (`num_layers > 0`,
  `frames_per_direction > 0`, `num_directions > 0`) e se `size` comporta
  cabeçalho + camadas + gatilhos + prioridades.
- `CofLayerCode(CofLayerType)` retorna a sigla de 2 letras (`"HD"`, `"TR"`, …).
- `PriorityOrder(direction, frame)` retorna a sequência de `CofLayerType`
  para pintar do fundo para a frente.
