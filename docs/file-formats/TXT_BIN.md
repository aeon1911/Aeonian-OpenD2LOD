# Formato TXT / BIN (Tabelas de Dados)

> **Nota (2026-10-28):** referências a `src/…`, `tests/…` e às camadas
> `platform/`/`formats/`/`engine/`/`game/` abaixo pertencem à fase de
> reimplementação (descontinuada e apagada). Este
> documento segue válido como **referência do formato** para quem faz
> ferramentas de mod. Escopo atual: [`SCOPE.md`](../SCOPE.md).

## Visão Geral

O Diablo II usa arquivos `.txt` (TSV – tab-separated values) para definir quase todos os dados estáticos do jogo: itens, monstros, skills, níveis, etc. Durante a execução, esses arquivos são compilados para `.bin` para carregamento rápido[reference:5].

## Estrutura TXT

- Primeira linha: nomes das colunas.
- Linhas subsequentes: dados separados por tabulação.
- Comentários começam com `//`.

## Estrutura BIN

- Binário com um cabeçalho de contagem de registros e, em seguida, os registros em formato binário compacto.
- O layout de cada registro é derivado do TXT correspondente.

## Implementação no Aeonian

A camada `formats/` deve:

1. Parsear `.txt` para uma estrutura de tabela genérica (linhas e colunas nomeadas).
2. Carregar `.bin` diretamente em structs tipadas para performance.
3. Fornecer uma API para consultar dados por ID ou nome.

## Tabelas Prioritárias

| Arquivo | Conteúdo |
|---------|----------|
| `ItemTypes.txt` | Tipos de item |
| `Weapons.txt` / `Armor.txt` | Itens equipáveis |
| `Skills.txt` | Habilidades |
| `MonStats.txt` | Estatísticas de monstros |
| `Levels.txt` | Definição de níveis |
| `Experience.txt` | Tabela de XP |

A especificação completa de cada tabela será preenchida conforme a engenharia reversa avança.
