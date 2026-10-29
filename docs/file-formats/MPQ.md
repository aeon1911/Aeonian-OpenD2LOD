# Formato MPQ (MoPaQ)

> **Nota (2026-10-28):** referências a `src/…`, `tests/…` e às camadas
> `platform/`/`formats/`/`engine/`/`game/` abaixo pertencem à fase de
> reimplementação (descontinuada e apagada). Este
> documento segue válido como **referência do formato** para quem faz
> ferramentas de mod. Escopo atual: [`SCOPE.md`](../SCOPE.md).

## Visão Geral

MPQ é o formato de arquivo de arquivo da Blizzard, usado para armazenar todos os assets do jogo (gráficos, sons, dados). É um container com compressão, criptografia e segmentação em blocos[reference:4].

## Estrutura (alto nível)
[Header] → [Hash Table] → [Block Table] → [File Data]


- **Header**: assinatura `MPQ\x1A`, offset para a hash table, offset para a block table.
- **Hash Table**: array de entradas com hash do nome do arquivo, índice do bloco.
- **Block Table**: array de entradas com offset, tamanho comprimido, tamanho descomprimido, flags.
- **File Data**: dados comprimidos (zlib, bzip2, etc.) e possivelmente criptografados.

## Implementação no Aeonian

A camada `platform/` (equivalente a `Storm.dll`) deve fornecer:

- `MPQ_Open(path) -> ArchiveHandle`
- `MPQ_ReadFile(archive, filename) -> byte[]`
- `MPQ_ListFiles(archive) -> string[]`

**Não é necessário** implementar escrita de MPQ para o jogo base, apenas leitura. Modders podem usar ferramentas externas para criar MPQs.

## Evidências de Engenharia Reversa

- A função de abertura de MPQ em `Storm.dll` foi mapeada como `SFileOpenArchive`.
- O algoritmo de hash de nomes de arquivo é conhecido (hash de Jenkin) e documentado publicamente.
