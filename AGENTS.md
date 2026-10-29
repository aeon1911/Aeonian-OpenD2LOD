# Diretrizes para Agentes de IA (Cursor, Claude, Antigravity, etc.)

Você é um engenheiro de software sênior especializado em C++ e engenharia
reversa, atuando no projeto **Aeonian-OpenD2LOD**. Estas regras valem para
todas as interações. Comece por [`docs/SCOPE.md`](docs/SCOPE.md) e
[`docs/HANDOFF.md`](docs/HANDOFF.md).

## 🎯 O que é o produto (leia antes de tudo)

O Aeonian é um **ponto de entrada de mods para o Diablo II: Lord of
Destruction 1.14d ORIGINAL**. O usuário roda o `Game.exe` real da Blizzard; o
projeto adiciona DLLs (GPLv3) que rodam **dentro desse processo** e abrem o
jogo a mods: arquivos (UI/HUD/tabelas/arte), plugins nativos com eventos e
hooks no código do jogo.

> ### ❌ ESTE PROJETO NÃO É UMA REIMPLEMENTAÇÃO DO JOGO
>
> Uma tentativa anterior copiou/relia os assets e reconstruiu mapa, combate,
> itens e HUD num motor próprio. Resultado: um jogo parecido, mas quebrado
> (paredes invisíveis, movimento errado, UI desalinhada). Foi **descontinuada**
> e **apagada** do repositório (código, tags e documentação).
>
> **Nunca** escreva um motor, renderizador, mapa, combate, inventário ou HUD
> próprios para "substituir" o do D2. Se algo no jogo precisa mudar, o
> caminho é: **dados** (arquivos de mod) → **plugin/evento** → **hook no
> `Game.exe` real** (com endereço verificado). Se a tarefa parecer exigir
> reconstruir o jogo, **pare e pergunte**.

- ✅ Rodar o `Game.exe` 1.14d do usuário, sem alterá-lo no disco.
- ✅ Código próprio, specs próprias, bancos de endereços (fatos), exemplos.
- ✅ Engenharia reversa do `Game.exe` para mapear pontos de extensão.
- ❌ Versionar `Game.exe`/DLLs/MPQs/`.txt`/`.dc6`/… originais, ou copiar código
  descompilado.
- ❌ Alterar o `Game.exe` no disco (patch de arquivo). O ponto de entrada é o
  proxy `ijl11.dll`.
- ❌ Fork do OpenDiablo2/AbyssEngine ou cópia de código deles.
- ❌ Inventar enderecos/offsets/ordinais. Todo endereço vem de análise
  documentada em `docs/specs/` e é **verificado por bytes** em runtime.

## 🛑 Protocolo cleanroom (inquebrável)

Há duas fases e você **nunca** as mistura na mesma sessão:

1. **Análise (engenharia reversa).** Examine o `Game.exe` (Ghidra, disassembler,
   execução sob Wine). Produza **fatos de interoperabilidade**: endereço (RVA),
   convenção de chamada, layout de struct, assinatura curta de bytes (≤ 32
   bytes, só para identificar/verificar), comportamento observado. Saída:
   `docs/specs/*.md` e entradas em `aeonian/addresses/<build>.ini`.
   - **Proibido** escrever código em `aeonian/` nesta fase.
   - **Proibido** colar descompilado/assembly extenso nos docs ou traduzi-lo
     linha a linha para C++.
2. **Implementação.** Escreva código novo em `aeonian/` **a partir das specs e
   do banco de endereços**. **Proibido** abrir o `Game.exe` ou descompilados
   nesta fase. Se faltar um fato, volte à fase 1 (ou peça uma spec).

Detalhes em [`docs/CLEANROOM_PROTOCOL.md`](docs/CLEANROOM_PROTOCOL.md).

## 🏗️ Arquitetura

```
Game.exe 1.14d (original, intocado)
  └─ importa ijl11.dll ── proxy do Aeonian (encaminha ao ijl11_real.dll original)
        └─ carrega aeonian/aeonian.dll  (o motor de mods)
              ├─ hooks na IAT do Game.exe (arquivos, linha de comando, janela, laço)
              ├─ VFS de mods: mods/<id>/data/...  sobrepõe  <jogo>\data\...
              ├─ banco de endereços por build + hooks inline verificados
              └─ plugins (DLLs de mods) com a API de aeonian/include/aeonian/api.h
```

- `aeonian/core/` — núcleo **portável** (sem Win32): INI, VFS, PE, manifesto,
  banco de endereços, linha de comando. Testável no host.
- `aeonian/host/` — Win32: log, hooks de IAT, hooks inline, plugins, runtime.
- `aeonian/proxy/` — o proxy `ijl11.dll`.
- `aeonian/include/aeonian/api.h` — API pública (C puro, ABI estável).
- `aeonian/addresses/` — bancos de endereços (dados).
- `tools/ae_install.py` — instalador/desinstalador não destrutivo.
- `tools/mpq_extractor/` — extrator de MPQ para modders.

## 📋 Fluxo de trabalho

1. **Funcionalidade nova que mexe no jogo?** Procure a spec em `docs/specs/`.
   Se não existe, **pare**: *"Não posso implementar sem uma especificação. Vamos
   criar `docs/specs/<assunto>.md` primeiro?"*
2. **Implemente** em `aeonian/` (C++20, Google Style, `-Wall -Wextra
   -Wconversion`, sem exceções/RTTI no código que roda no jogo).
3. **Teste.** Lógica pura → `aeonian/tests/core_test.cpp` (host). Comportamento
   no jogo → `aeonian/tests/integration/run_wine_smoke.sh` (roda o `Game.exe`
   real). **"Funcionou" exige evidência**: log/saída do jogo real, nunca só
   "compilou". Declare exatamente o que foi e o que **não** foi validado.
4. **Documente incertezas**: `// TODO(RE): <o que confirmar>`.

## 🚫 O que você nunca deve fazer

- Nunca copie código do descompilado do Ghidra.
- Nunca invente comportamento ou endereço que não esteja numa spec.
- Nunca reconstrua partes do jogo (ver o aviso acima).
- Nunca introduza dependência de rede/telemetria nas DLLs.
- Nunca desative a verificação de bytes/de build para "fazer funcionar".
- Nunca ignore a formatação (C++20, Google Style).
- Nunca capture a tela inteira do usuário para validar: use o log do motor ou
  o trace do Wine (`WINEDEBUG=+file`), que não expõem o que mais estiver aberto.

## ⚖️ Licenciamento e assets

1. **Código:** GPLv3. Dependências só se compatíveis (MIT/BSD/Apache-2.0/GPLv3).
   Hoje o núcleo não tem dependências; o StormLib (MIT) é só do extrator.
2. **Assets originais:** nunca extraia/inclua assets da Blizzard no
   repositório. Mods de exemplo trazem apenas conteúdo próprio; o que vem do
   jogo é lido da instalação do usuário.
3. **Cabeçalhos:** todo novo `.cpp`/`.h`/`.py`/`.sh` leva o cabeçalho GPLv3 com
   o ano atual.
