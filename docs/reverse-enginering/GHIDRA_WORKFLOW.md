# Fluxo de Trabalho com Ghidra

## Configuração Inicial

1. Importe os binários do Diablo II **1.14d** (`Game.exe`, `D2Game.dll`, `D2Client.dll`, `D2Common.dll`, etc.) para um projeto Ghidra. Use a 1.13c apenas como projeto de comparação/Version Tracking.
2. Aplique os scripts de assinatura de bibliotecas padrão (Visual Studio 2008 / x86).
3. Execute a análise automática (`Auto Analyze`).

## Ordem de Documentação (baseada em dependências)

A documentação segue a hierarquia de dependências dos binários para maximizar a reutilização de conhecimento[reference:1]:

**Fase 0 – Fundação**
1. `Storm.dll` (MPQ, memória, criptografia) – muito estável entre versões.
2. `Fog.dll` (gerenciamento de memória, debug).

**Fase 1 – Serviços Centrais**
3. `D2Lang.dll` (localização, tabelas de string).
4. `D2CMP.dll` (formatos gráficos DC6, DCC, DT1).

**Fase 2 – Fundação do Jogo**
5. `D2Common.dll` (estruturas de Units, Items, Skills, Stats) – **crítico**, documentar primeiro na versão canônica (1.14d).

**Fase 3 – Subsistemas**
6. `D2Sound.dll`, `D2Win.dll`, `D2Gfx.dll`, `D2Net.dll`.

**Fase 4 – Lógica de Alto Nível**
7. `D2Multi.dll`, `D2Game.dll`, `D2Client.dll`.

**Fase 5 – Pontos de Entrada**
8. `D2Launch.dll`, `Game.exe`.

## Convenções de Nomenclatura

- Funções desconhecidas: manter `FUN_xxxxxxxx` até que a especificação seja escrita.
- Após a especificação, renomear no Ghidra para `RE_<NomeDescritivo>` (ex: `RE_CalculateMinDamage`). Isso ajuda a rastrear o que já foi documentado.
- Estruturas: criar `struct` no Ghidra com os campos identificados e anotar os offsets na especificação.

## Extração de Especificações

Para cada função documentada, preencha o template em `ai/prompts/DECOMPILE_FUNCTION.md`. O resultado deve ser um arquivo `.md` em `docs/specs/` (não versionado junto com o código, mas usado como entrada para a implementação).
