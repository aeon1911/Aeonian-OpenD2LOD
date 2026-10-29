# Prompt: Documentar Estrutura de Dados (Struct)

Use este prompt para instruir o Claude a traduzir o formato de uma estrutura de dados observada no Ghidra para uma especificação Markdown e, posteriormente, para um código C++ limpo.

---

**Contexto**: Você está atuando na documentação de estruturas de dados do projeto Aeonian-OpenD2LOD. O objetivo é documentar estruturas observadas (como `Unit`, `ItemData`, `PlayerData`) nos binários originais.

**Entrada**: [Cole aqui o layout da struct do Ghidra, contendo offsets, tamanhos e nomes de variáveis, ou a descrição da memória]

**Instruções**:
1. Traduza o layout de memória para uma especificação compreensível.
2. Identifique ponteiros, tipos primitivos (int, short, byte, float) e arrays.
3. Se houver offsets desconhecidos (padding), documente-os explicitamente como reservados (`padding`).
4. Gere a estrutura C++ equivalente para a equipe de implementação.

**Saída Esperada**:

```markdown
# Estrutura: <NomeDaEstrutura>

## Origem
- DLL: <D2Common.dll / D2Game.dll>
- Endereço de referência: <endereço no Ghidra>

## Layout de Memória
| Offset | Tamanho | Tipo | Nome | Descrição |
|--------|---------|------|------|-----------|
| 0x00   | 4       | ptr  | pNext | Ponteiro para a próxima unidade |
| 0x04   | 4       | int  | UnitId | ID único da unidade |
| ...    | ...     | ...  | ...   | ... |

## Implementação C++ Proposta
```cpp
struct Unit {
    void* pNext;      // 0x00
    int UnitId;       // 0x04
    // ...
};
