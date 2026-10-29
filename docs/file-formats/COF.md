# Formato COF (Coefficient of Animation)

> **Nota (2026-10-28):** referências a `src/…`, `tests/…` e às camadas
> `platform/`/`formats/`/`engine/`/`game/` abaixo pertencem à fase de
> reimplementação (descontinuada e apagada). Este
> documento segue válido como **referência do formato** para quem faz
> ferramentas de mod. Escopo atual: [`SCOPE.md`](../SCOPE.md).

## Visão Geral
O arquivo `.cof` (Coefficient) é o "maestro" das animações no Diablo II. Ele não contém gráficos, mas sim a lógica de como os frames de um arquivo `.dcc` (ou `.dc6`) devem ser reproduzidos.

## Estrutura
- **Layers (Camadas)**: Um personagem pode ter várias camadas (corpo, armadura, arma, escudo). O COF define quais camadas compõem a animação.
- **Frames**: Quantidade total de frames na animação.
- **Velocidade**: A duração de cada frame, que interage diretamente com a "IAS" (Increased Attack Speed) ou "FCR" (Faster Cast Rate) do personagem.
- **Triggers (Gatilhos)**: Eventos que ocorrem em frames específicos. Ex: No frame 4 da animação de ataque, a flecha é disparada; no frame 6, o som da espada é tocado.

## Implementação no Aeonian
A camada `game/` (ou uma subcamada de animação no `engine/`) deve interpretar o COF para sincronizar a lógica do servidor com a renderização do cliente.
- O sistema de combate precisa saber em qual frame o dano é aplicado.
- O sistema de áudio precisa tocar o som de impacto no frame exato definido no COF.

## Fontes de Evidência
- Gerenciamento de animação em `D2CMP.dll` e `D2Client.dll`.
- Ferramentas de modding da comunidade (ex: CofEdit).
