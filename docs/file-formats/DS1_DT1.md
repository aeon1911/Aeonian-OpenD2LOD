# Formatos DS1 e DT1 (Mapas e Tiles)

> **Nota (2026-10-28):** referências a `src/…`, `tests/…` e às camadas
> `platform/`/`formats/`/`engine/`/`game/` abaixo pertencem à fase de
> reimplementação (descontinuada e apagada). Este
> documento segue válido como **referência do formato** para quem faz
> ferramentas de mod. Escopo atual: [`SCOPE.md`](../SCOPE.md).

## Visão Geral
O Diablo II constrói seus mapas de forma procedural ou fixa usando arquivos DS1, que por sua vez referenciam tiles gráficos DT1.

## DS1 (Data Structure 1)
- Representa o layout de uma sala ou área (ex: `Act1/Town/Town1.ds1`).
- Contém a largura e altura da sala, além de uma matriz de células.
- Cada célula aponta para um ID de tile (que será buscado no DT1 correspondente), além de flags de colisão, altura e informações de iluminação.
- Contém também um array de "Objetos" (baús, portas, waypoints) e "Monstros" (spawn points).

## DT1 (Diablo Tile 1)
- É uma biblioteca gráfica de tiles.
- Contém múltiplos blocos gráficos (tiles), com informações de altura, largura, offsets isométricos e dados de colisão.
- Usa paletas de cores próprias e pode conter múltiplas variações do mesmo tile.

## Implementação no Aeonian
A camada `formats/` deve possuir:
- `DS1Parser`: Extrai a matriz de células e os objetos/monstros da sala.
- `DT1Parser`: Carrega os gráficos na memória de vídeo e mapeia as colisões.
- O `engine/` será responsável por renderizar o mapa isométrico usando os dados combinados de DS1 e DT1.

## Fontes de Evidência
- Estruturas mapeadas em `D2Common.dll` (funções de carregamento de salas).
- Formato bem documentado publicamente, usado por ferramentas como o Dr. Tester.
