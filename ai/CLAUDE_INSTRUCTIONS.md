# Instruções para IAs — sessões de trabalho

Complementa o `AGENTS.md` (raiz), que é a autoridade máxima. Leia-o primeiro,
depois `docs/SCOPE.md` e `docs/HANDOFF.md`. Este arquivo cobre o **fluxo
operacional por sessão**.

> **O projeto abre o `Game.exe` 1.14d real a mods. NÃO reimplementa o jogo.**
> A fase de reimplementação foi descontinuada e apagada. Não a retome.

## 1. Modo de sessão

Declare o papel no início:

- **`[ANÁLISE]`** — engenharia reversa do `Game.exe`. Saída: apenas
  `docs/specs/*.md`, `docs/evidence/*.md` e entradas em
  `aeonian/addresses/*.ini`. Proibido tocar em `aeonian/*.cpp|h`.
- **`[IMPLEMENTAÇÃO]`** — código em `aeonian/` + testes em `aeonian/tests/`, a
  partir de specs existentes. Proibido abrir o `Game.exe`/descompilados.
- **`[REVISÃO]`** — confere specs ↔ código ↔ testes.

Não misture papéis na mesma sessão (Regra de Ouro do `AGENTS.md`).

## 2. Ferramentas

- Análise: Ghidra (GUI/headless), `objdump`, scripts de leitura de PE em Python
  (`struct`), execução sob Wine (`WINEDEBUG=+file`). Registre sempre o SHA-256
  do binário e o método na evidência.
- Build/teste: ver `docs/HANDOFF.md` (toolchain mingw-w64 i686, CMake, suíte de
  integração no jogo real).
- **Não capture a tela inteira** do usuário; use log/trace do motor.

## 3. Dados do jogo original

Fora do repositório, na máquina do usuário (caminhos em `docs/HANDOFF.md`).
Extração de MPQ: `ae_mpq_extract <mpq> list|extract <dir> [prefixo]` (build com
`-DAEONIAN_BUILD_TOOLS=ON`). **Nunca** commite o que extrair.

## 4. Ordem de trabalho

Siga `docs/ROADMAP.md` (M2: apresentação do quadro → estado do jogo → tabelas).
Atualize `docs/HANDOFF.md` ao fim de cada sessão substancial, incluindo o que
**não** foi validado.

## 5. Padrões não negociáveis

- Evidência de tudo (hash, endereço, ferramenta, método).
- Spec antes de código. Sem spec, não existe hook.
- Todo endereço entra no `addresses/*.ini` **com `bytes`** para verificação.
- "Funcionou" = evidência do jogo real (log/trace), nunca só "compilou".
- Cabeçalho GPLv3 (ano atual) em todo arquivo novo.
