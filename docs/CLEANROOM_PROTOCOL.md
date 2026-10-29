# Protocolo cleanroom (sala limpa)

## Por que existe

O Aeonian abre o `Game.exe` original a mods. Para isso é preciso **entender
pontos do binário** (onde estão funções, que bytes as identificam, como se
chamam). O protocolo garante que o que entra no repositório são **fatos de
interoperabilidade escritos por nós**, e não código da Blizzard, e que a
implementação nunca depende de copiar o descompilado.

## As duas fases (nunca na mesma sessão/PR)

- **Análise (engenharia reversa).** Examina o `Game.exe` 1.14d (Ghidra,
  disassembler, depurador, execução sob Wine). **Produz**:
  - `docs/specs/<assunto>.md` — o fato, a evidência (versão + SHA-256 + método),
    a convenção de chamada, o layout de struct, o comportamento observado;
  - entradas em `aeonian/addresses/<build>.ini` — RVA + `bytes` curtos
    (≤ 32) que identificam/verificam a função.
  **Não escreve** código de produção.
- **Implementação.** Recebe **apenas as specs e o banco de endereços** e
  escreve código novo em `aeonian/`. **Nunca** abre o `Game.exe`, o
  descompilado do Ghidra nem artefatos com código da Blizzard. Se falta um
  fato, a resposta é "preciso de uma spec", não "vou olhar o binário".

## O que é permitido nos documentos

| Permitido | Proibido |
|---|---|
| "A função em RVA `0x1234` recebe `(HDC, int, int)` em `__stdcall` e desenha o painel de inventário" | Pseudocódigo/descompilado colado ou traduzido linha a linha |
| Os 5–32 primeiros bytes de uma função **para verificação** (`bytes =`) | Dumps extensos de assembly |
| Layout de struct: campo, offset, tamanho, significado | Cópia do corpo de funções |
| Comportamento observado em execução (logs, traces) | Trechos de código-fonte da Blizzard |

Exemplo de transformação correta: ❌ `mov eax,[ebp+8]; imul eax,3; add eax,0x10`
→ ✅ "o valor mínimo é `base*3 + 16`, com `base` (int32) lido do offset `+8` da
struct de ataque".

## Nada proprietário no repositório

`Game.exe`, DLLs, `.mpq`, `.txt`, `.dc6`, `.dcc`, `.ds1`, `.dt1`, `.wav` e
demais assets originais **nunca** são versionados. Entram apenas código
próprio, specs, dados de endereços e fixtures sintéticos. Mods de exemplo só
com conteúdo próprio.

## Regras para contribuidores (humanos e IA)

1. **Separação de contexto.** Na análise não se escreve código em `aeonian/`;
   na implementação não se abre o binário.
2. **Rastreabilidade.** Toda spec declara versão e SHA-256 da evidência
   (alvo atual: 1.14d, ver [`VERSIONS.md`](VERSIONS.md)) e o método, sem
   incluir código descompilado.
3. **Verificação em runtime.** Todo endereço usado por código é verificado por
   bytes antes de qualquer escrita (o motor já impõe isso).
4. **Endereços de uma build não valem em outra.** Specs de 1.13c são histórico.
5. **Testes com evidência do jogo real** (log/trace), não só compilação.

## Ferramentas permitidas na análise

- Ghidra (GUI ou *headless*), `objdump`, leitores de PE, depuradores
  (x64dbg, `winedbg`), execução sob Wine com `WINEDEBUG=+file`/`+relay`.
- Extração de MPQ para inspecionar **dados** (os arquivos extraídos ficam fora do
  repositório).

## Prior art

[OpenDiablo2](https://github.com/OpenDiablo2/OpenDiablo2) e
[AbyssEngine](https://github.com/AbyssEngine/AbyssEngine) são referência de
formato e comportamento. Não somos fork deles nem copiamos seu código.

**Lembrete:** o objetivo é documentar o **comportamento e a interface**, não o
código. A especificação é a ponte entre os dois mundos.
