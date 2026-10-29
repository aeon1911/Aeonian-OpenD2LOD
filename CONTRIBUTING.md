# Diretrizes de Contribuição

## Antes de tudo: escopo e cleanroom

Leia [`docs/SCOPE.md`](docs/SCOPE.md) e o
[`CLEANROOM_PROTOCOL.md`](docs/CLEANROOM_PROTOCOL.md). O Aeonian **abre o jogo
real**; **não** o reimplementa. O repositório distribui apenas código
próprio, specs e dados de endereços — nunca arquivos da Blizzard. Não são
aceitos: reconstrução de partes do jogo, forks de OpenDiablo2/AbyssEngine,
código descompilado, patches no `Game.exe` em disco. Contribuições que violem
a separação análise/implementação serão rejeitadas.

## Tipos de contribuição

1. **Mods e exemplos** (`aeonian/examples/`): só conteúdo próprio, sem assets da
   Blizzard.
2. **Análise (engenharia reversa)**: documente em `docs/specs/<assunto>.md`
   (fato, evidência, versão/hash, método) e, se houver endereço, acrescente o
   símbolo a `aeonian/addresses/1.14d.ini` **com `bytes`** para verificação.
   Não escreva código de produção nesta PR.
3. **Implementação**: a partir de uma spec existente. Não abra o `Game.exe` nem
   descompilados nesta PR. Se faltar um fato, peça uma spec.
4. **Novas builds do jogo**: um `addresses/<build>.ini` novo (identificação por
   `timestamp` + `size_of_image` + `entry_rva`, `sha256`) e a spec de evidência.

Indique no título da PR se é **análise** ou **implementação**.

## Padrões de código

- **C++20**, Google C++ Style (adaptado). `snake_case` para funções e
  variáveis, `PascalCase` para tipos. Código que roda no jogo: sem exceções e
  sem RTTI; só as bibliotecas padrão + Win32.
- Compile sem warnings (`-Wall -Wextra -Wpedantic -Wconversion -Wshadow`).
- Todo arquivo novo leva o cabeçalho GPLv3 com o ano atual.
- A API pública (`aeonian/include/aeonian/api.h`) é **ABI estável**: só se
  acrescenta no fim das structs.

## Testes

- Lógica pura → `aeonian/tests/core_test.cpp` (roda no host, com
  `-fsanitize=address,undefined` recomendado).
- Comportamento dentro do jogo → `aeonian/tests/integration/run_wine_smoke.sh`.
- **"Funciona" exige evidência do jogo real** (log/trace), não só "compilou".
  Descreva na PR o que foi e o que **não** foi validado.
- Testes não podem depender de input humano nem de captura de tela.

## Ambiente

Passo a passo de build, toolchain e execução dos testes:
[`docs/HANDOFF.md`](docs/HANDOFF.md).
