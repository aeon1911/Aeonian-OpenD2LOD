# Correspondência Entre Versões (Cross-Version Matching)

## O Problema das Versões
O Diablo II LoD teve diversas versões (1.09, 1.10f, 1.13c, 1.14d). O nosso alvo canônico agora é a **1.14d** (última versão oficial). A 1.13c continua como **referência histórica** e base de comparação, porque o ecossistema de modding e as specs já produzidas vieram dela.

## Como Utilizar Outras Versões no Ghidra
Não podemos assumir que o código é idêntico entre as versões. O Ghidra possui a ferramenta **Version Tracking** que deve ser utilizada para mapear funções da 1.13c (legado) para a 1.14d (alvo), e de versões da comunidade (1.10f etc.) apenas como hipótese.

### Estratégias de Matching
1. **String References**: Funções que usam strings únicas (ex: "Unable to load file") geralmente permanecem estáveis.
2. **Call Graphs**: Se sabemos que a `Função A` chama a `Função B` e a `Função C`, e na 1.13c temos uma função com o mesmo padrão de chamadas, há uma alta probabilidade de serem a mesma.
3. **Assinaturas de Biblioteca**: O compilador (MSVC) gera padrões de prólogo e epílogo. Funções da CRT (C Runtime) raramente mudam.
4. **Tabelas de Dados**: Referências à `Skills.txt`, `MonStats.txt` ou às tabelas de `D2Common` são âncoras perfeitas.

## Regra de Ouro Cleanroom
Se uma especificação foi escrita para 1.13c ou 1.10f, **ela deve ser revalidada na 1.14d** antes de ser usada como contrato. Diferenças comportamentais entre versões devem ser documentadas explicitamente na especificação (seção “Delta 1.13c → 1.14d”). Nenhum endereço/hash de 1.13c deve ser tratado como 1.14d sem reanálise.
