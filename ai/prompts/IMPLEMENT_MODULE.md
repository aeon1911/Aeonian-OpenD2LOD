# Prompt: Implementar Módulo a partir de Especificação

Use este prompt no Cursor/Claude para implementar um módulo a partir de uma especificação existente na pasta `docs/specs/`.

---

**Contexto**: Você está na equipe de Implementação do projeto Aeonian-OpenD2LOD. Você receberá uma especificação técnica. Sua tarefa é escrever código **novo** (Cleanroom) que implemente fielmente o comportamento descrito.

**Instruções**:
1. Leia a especificação inteira antes de escrever qualquer linha.
2. Identifique as estruturas de dados e crie os cabeçalhos/structs primeiro, respeitando a arquitetura (`aeonian/core` portável, sem Win32 -> `aeonian/host` Win32).
3. Implemente a lógica em uma função pura, sem dependências de renderização ou I/O (especialmente em `aeonian/core`).
4. Crie testes unitários em `aeonian/tests/` com fixtures de entrada/saída baseados na especificação.
5. **CRÍTICO**: Não consulte descompilados ou binários originais. A especificação é a única fonte de verdade. Se a especificação estiver incompleta, pare e pergunte.

**Saída esperada**:
- Arquivos de código em `aeonian/<core|host|proxy>/`.
- Arquivos de teste em `aeonian/tests/`.
- Um resumo das decisões de implementação e quaisquer ambiguidades na especificação.
