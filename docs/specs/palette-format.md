# Especificação: formato PAL (paleta do Diablo II)

> **Nota (2026-10-28):** referências a `src/…`, `tests/…` e às camadas
> `platform/`/`formats/`/`engine/`/`game/` abaixo pertencem à fase de
> reimplementação (descontinuada e apagada). Este
> documento segue válido como **referência do formato** para quem faz
> ferramentas de mod. Escopo atual: [`SCOPE.md`](../SCOPE.md).

> **Versão da evidência: 1.13c (histórica).** O alvo do projeto é **1.14d**.
> Esta spec precisa ser revalidada contra os binários 1.14d antes de ser
> tratada como contrato. Ver [`docs/VERSIONS.md`](../VERSIONS.md).

> **Fase de Análise (Cleanroom).** Estrutura observada nos arquivos
> `data\global\palette\<conjunto>\pal.dat` da instalação 1.13c do usuário.
> Nenhum asset proprietário é incluído no repositório: o adaptador apenas
> lê o arquivo da instalação em tempo de execução.

## 1. Estrutura

- Tamanho: exatamente **768 bytes**.
- 256 entradas de 3 bytes na ordem **B, G, R** (0–255):
  - `b = data[3 * i + 0]`
  - `g = data[3 * i + 1]`
  - `r = data[3 * i + 2]`
- Sem cabeçalho, sem transparência e sem tabela de índices especiais: o
  índice `i` (0–255) corresponde aos bytes `[3i, 3i+1, 3i+2]` em ordem BGR.

Exemplo observado (`Menu0/pal.dat` e `ACT1/pal.dat`): `00 00 00` (preto),
entradas alaranjadas/avermelhadas na fogueira de `TitleScreen.DC6` e tons
terrosos/marrons nas rochas de `cave.dt1` quando interpretados como `B, G, R`.

## 2. `Pal.PL2`

Os arquivos `Pal.PL2` (≈443 KB) são outra representação (paleta + dados
auxiliares) e **não** são usados nesta entrega. `pal.dat` é suficiente para
o renderizador e é o formato documentado aqui. `TODO(RE)`: mapear o PL2 se
alguma funcionalidade do engine exigir.

## 3. Contrato do adaptador

`src/formats/palette.{h,cpp}` expõe:

```cpp
struct Rgb { std::uint8_t r, g, b; };
class Palette {
  static std::optional<Palette> FromBytes(const std::uint8_t* data,
                                          std::size_t size);
  const Rgb& Get(std::uint8_t index) const;
};
```

- `FromBytes` rejeita tamanho diferente de 768.
- `Get(0)` é cor válida (preto na paleta de menu).
- Sem alocação dinâmica por pixel; a paleta é copiada uma vez.

## 4. Vetores de teste

`tests/palette_test.cpp`:

- bytes `0,1,2, 3,4,5, …` (em ordem `B, G, R`) → `Get(0) = {.r=2, .g=1, .b=0}`, `Get(1) = {.r=5, .g=4, .b=3}`;
- 767/769 bytes → `std::nullopt`;
- `Get(255)` lê os últimos três bytes.

## Ver também

- `docs/specs/dc6-format.md`
- `docs/HANDOFF.md`
