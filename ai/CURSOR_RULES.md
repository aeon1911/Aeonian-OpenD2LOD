# Regras para Cursor (`.cursor/rules/`)

Cole cada bloco como uma regra do Cursor. Elas impõem o escopo e o protocolo
cleanroom do projeto (autoridade máxima: `AGENTS.md`).

## Regra 0: Escopo — abrir o jogo real, não reconstruí-lo

```yaml
description: "O Aeonian abre o Game.exe 1.14d original a mods. Nunca reimplemente o jogo."
alwaysApply: true
```

- Nunca escreva motor, renderizador, mapa, combate, inventário ou HUD próprios
  para substituir os do Diablo II. O caminho é: dados de mod → plugin/evento →
  hook verificado no `Game.exe` real.
- Se a tarefa parecer exigir reconstruir o jogo, pare e pergunte.

## Regra 1: Separação de contexto

```yaml
description: "Nunca misture análise de binário com implementação de código."
globs: ["aeonian/**/*", "docs/specs/**/*"]
alwaysApply: true
```

- Em `aeonian/` (implementação), não abra o `Game.exe` nem descompilados.
- Em `docs/specs/` (análise), não escreva código de produção em `aeonian/`.

## Regra 2: Especificação antes de código

```yaml
description: "Antes de criar um hook, a spec e o símbolo verificado devem existir."
globs: ["aeonian/host/**/*", "aeonian/addresses/**/*"]
alwaysApply: true
```

- Verifique `docs/specs/` e `aeonian/addresses/*.ini`. Sem spec, peça-a primeiro.
- Todo endereço entra no banco **com `bytes`** para verificação em runtime.

## Regra 3: Nenhum código proprietário

```yaml
description: "Proibido inserir código descompilado ou arquivos da Blizzard."
alwaysApply: true
```

- Recuse tradução linha a linha de descompilado. Reformule como especificação
  de comportamento/interface.
- Nunca versione `Game.exe`, DLLs, MPQs ou assets originais.

## Regra 4: Testes com evidência do jogo real

```yaml
description: "Toda mudança no motor vem com teste; 'funciona' exige evidência do jogo real."
globs: ["aeonian/**/*"]
alwaysApply: true
```

- Lógica pura → `aeonian/tests/core_test.cpp`.
- Comportamento no jogo → `aeonian/tests/integration/run_wine_smoke.sh`.
- Declare o que **não** foi validado.

## Exemplo de reformulação cleanroom

Se o usuário disser *"a função `FUN_6fadecd0` faz `eax = [ecx+8] * 3 + 16`"*:

> "Entendido. A especificação é: 'o valor mínimo é `base * 3 + 16`, com `base`
> (int32) no offset +8 da struct de entrada'. Vou registrá-la em
> `docs/specs/…` (análise) e só depois implementar em `aeonian/` com um teste."
