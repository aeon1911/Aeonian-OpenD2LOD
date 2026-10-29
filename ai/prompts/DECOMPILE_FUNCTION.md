# Prompt: Documentar Função Descompilada

Use este prompt no Claude (ou outro assistente) para transformar um descompilado do Ghidra em uma **especificação comportamental**.

---

**Contexto**: Você está na equipe de Análise (Reverse Engineering) do projeto Aeonian-OpenD2LOD. Sua tarefa é documentar o comportamento de uma função a partir de seu descompilado do Ghidra. **Não escreva código de implementação.**

**Entrada**: [cole aqui o descompilado da função]

**Saída esperada**: Um arquivo Markdown com a seguinte estrutura:

```markdown
# Especificação: <NomeDescritivo>

## Assinatura Observada
- Binário: <DLL>
- Endereço: <endereço>
- Parâmetros: <descrição dos parâmetros>
- Retorno: <descrição do retorno>

## Comportamento
<Descrição em linguagem natural do que a função faz, passo a passo. Use pseudocódigo de ALTO NÍVEL se necessário, mas evite sintaxe de C ou assembly.>

## Estruturas de Dados Envolvidas
- <Nome da estrutura>: <campos relevantes e offsets>

## Condições de Borda
- <o que acontece se X for 0, negativo, overflow, etc.>

## Evidências
- <Observações de testes em tempo real, se houver>
- <Referências cruzadas com outras funções já documentadas>

## Notas para Implementação
- <Algoritmos equivalentes conhecidos, se aplicável>

Regra: Não inclua o descompilado bruto no arquivo de especificação. Apenas a descrição comportamental.
text


---

### 13. `ai/prompts/IMPLEMENT_MODULE.md`

```markdown
# Prompt: Implementar Módulo a partir de Especificação

Use este prompt no Cursor/Claude para implementar um módulo a partir de uma especificação existente.

---

**Contexto**: Você está na equipe de Implementação do projeto Aeonian-OpenD2LOD. Você receberá uma especificação em `docs/specs/<modulo>.md`. Sua tarefa é escrever código **novo** que implemente fielmente o comportamento descrito.

**Entrada**: O conteúdo de `docs/specs/<modulo>.md`.

**Instruções**:
1. Leia a especificação inteira antes de escrever qualquer linha.
2. Identifique as estruturas de dados e crie os cabeçalhos/structs primeiro.
3. Implemente a lógica em uma função pura, sem dependências de renderização ou I/O.
4. Crie testes unitários com fixtures de entrada/saída baseados na especificação.
5. Não consulte descompilados ou binários originais. A especificação é a única fonte de verdade.

**Saída esperada**:
- Arquivos de código em `aeonian/<core|host|proxy>/`.
- Arquivos de teste em `aeonian/tests/`.
- Um resumo das decisões de implementação e quaisquer ambiguidades na especificação.
