# Formatos DCC e DC6 (Gráficos)

> **Nota (2026-10-28):** referências a `src/…`, `tests/…` e às camadas
> `platform/`/`formats/`/`engine/`/`game/` abaixo pertencem à fase de
> reimplementação (descontinuada e apagada). Este
> documento segue válido como **referência do formato** para quem faz
> ferramentas de mod. Escopo atual: [`SCOPE.md`](../SCOPE.md).

## DC6

Formato de sprite simples, usado para inventário, painéis de UI e algumas animações estáticas[reference:6].

- Cabeçalho com contagem de frames, offsets.
- Cada frame: largura, altura, offset X/Y, dados de paleta (indexados).
- Compressão RLE opcional.

## DCC

Formato de animação complexo, usado para personagens, monstros e projéteis[reference:7].

- Contém múltiplas direções (até 8 ou 16) e múltiplos frames por direção.
- Compressão mais sofisticada que DC6.
- Frequentemente acompanhado por um arquivo `.cof` que define a composição da animação.

## Implementação

A camada `formats/` deve fornecer decodificadores para ambos os formatos, produzindo buffers de imagem (RGBA) que o `engine/` possa renderizar.

## Evidências

As funções de decodificação em `D2CMP.dll` foram mapeadas; os algoritmos de compressão são conhecidos e serão especificados em detalhe.
