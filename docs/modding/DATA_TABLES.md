# Tabelas de dados e modding

> Versão anterior deste documento descrevia um motor próprio que leria e
> compilaria as tabelas. Essa rota foi **descontinuada** (ver
> [`../SCOPE.md`](../SCOPE.md)): quem lê as tabelas é o **próprio jogo**.

## Como funciona hoje

- As tabelas do D2 (`skills`, `charstats`, `monstats`, `itemtypes`…) vivem em
  `data\global\excel\` (nos MPQs do jogo). O **Aeonian não as interpreta**.
- Para alterá-las, coloque o arquivo no mesmo caminho dentro do seu mod:
  `mods\<id>\data\global\excel\<tabela>`. O VFS entrega ao jogo a sua versão
  (ver [`MODS.md`](MODS.md)).
- A flag `-txt` existe na 1.14d. Pela prática da comunidade ela faz o jogo
  ler `.txt` em vez de `.bin`. **Este projeto ainda não validou esse
  comportamento**; o que está validado é o redirecionamento do arquivo.
- Colunas novas/desconhecidas são tratadas pelo parser **do jogo**; o Aeonian
  não as expõe a plugins.

## Não implementado (precisa de engenharia reversa)

| Ideia | Dependência |
|---|---|
| Recarregar tabelas sem reiniciar (hot-reload) | achar e chamar o carregador de tabelas do jogo (spec + símbolo) |
| Ler/modificar linhas de tabela pela API de plugin | acesso às estruturas em memória das tabelas |
| Colunas novas visíveis ao jogo | extensão do parser interno (hook) |

Entram no [`ROADMAP.md`](../ROADMAP.md) (M2/M3).
