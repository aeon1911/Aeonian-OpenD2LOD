# Arquitetura

> Contrato de produto em [`SCOPE.md`](SCOPE.md). Resumo: o Aeonian roda **dentro
> do `Game.exe` 1.14d original**; não é uma reimplementação.

## Visão geral

```
 ┌────────────────────────────────────────────────────────────────────────┐
 │ Game.exe 1.14d  (original do usuário, NÃO modificado no disco)         │
 │   importa ijl11.dll (por ordinal) ──┐                                  │
 └─────────────────────────────────────┼──────────────────────────────────┘
                                       ▼
   <jogo>\ijl11.dll   proxy do Aeonian (aeonian/proxy/)
        │  encaminha as 6 exportações ──►  <jogo>\ijl11_real.dll (original)
        │  DllMain: carrega aeonian\aeonian.dll e chama AeonianBootstrap()
        ▼
   <jogo>\aeonian\aeonian.dll   motor de mods (aeonian/host/ + aeonian/core/)
        │
        ├─ Bootstrap (sob o loader lock — leve):
        │     log · aeonian.ini · identifica a build · hooks de IAT
        │
        ├─ Inicialização tardia (1ª chamada interceptada, fora do loader lock):
        │     descobre mods · indexa data\ dos mods · carrega plugins
        │
        └─ Em execução:
              GetCommandLineA   → acrescenta -direct / flags dos mods
              CreateFileA +
              GetFileAttributesA→ VFS: <jogo>\data\X  ⇒  mods\<id>\data\X
              PeekMessageA      → on_tick (+ on_loaded no 1º)
              CreateWindowExA   → subclasse da janela → on_window_message
              StretchBlt /      → on_present (GDI): compõe o overlay dos plugins
              DirectDrawCreate    no quadro de 8 bits e RESTAURA os pixels depois
              LoadLibraryA      → ao carregar o glide3x.dll do D2GL, intercepta o
                                  SwapBuffers dele: on_present em OpenGL
              BinkOpen/SmackOpen→ pula os vídeos de abertura (skip_intro)
              ExitProcess       → on_shutdown
              hook_inline /     → desvio/patch por endereço, só com bytes
              patch_bytes         conferidos (banco de endereços por build)
```

## Camadas e dependências

| Diretório | Depende de | Conteúdo |
|---|---|---|
| `aeonian/core/` | só a biblioteca padrão | INI, VFS (resolução de caminhos), leitura de PE, manifesto de mod, banco de endereços, montagem da linha de comando. **Sem Win32** → testável no host |
| `aeonian/host/` | `core/` + Win32 | log, hooks de IAT, hooks inline/patch, índice de arquivos, plugins, `Runtime`, `AeonianBootstrap` |
| `aeonian/proxy/` | Win32 | `ijl11.dll`: encaminhamento + carga do `aeonian.dll` |
| `aeonian/include/aeonian/api.h` | — | API pública dos plugins (C puro, ABI estável) |
| `aeonian/addresses/` | — | bancos de endereços por build (dados) |
| `tools/` | — | `ae_install.py` (instalador), `mpq_extractor/` |

O código que roda no jogo é C++20 **sem exceções e sem RTTI**, com a runtime
estática (só depende de `KERNEL32`, `USER32` e `msvcrt`).

## Decisões de projeto e por quê

**Proxy do `ijl11.dll` em vez de patch no `Game.exe`.** O jogo já importa essa
DLL; basta uma `ijl11.dll` na pasta do jogo que encaminhe as exportações.
Nenhum byte do `Game.exe` muda, a desinstalação é trivial e restaura o
original byte a byte. Alternativas descartadas: patch de PE em disco (altera o
binário da Blizzard) e launcher com injeção por `CreateRemoteThread` (frágil
em camadas de compatibilidade). O `aeonian.dll` exporta `AeonianBootstrap` e
pode ser carregado por **qualquer** injetor (por exemplo o import extra de PE
usado no projeto-irmão do Switch).

**Hooks na IAT, não inline, para o essencial.** Os pontos de interceptação
(arquivos, linha de comando, laço de mensagens, janela) são importações do
sistema que o executável já usa. Isso **não depende de nenhum endereço
interno** da build e portanto sobrevive a mudanças no `Game.exe`; só o banco
de endereços (camada 3) é específico de build.

**VFS por índice em memória.** O jogo, em modo `-direct`, tenta ler
`data\...` solto antes dos MPQs. O Aeonian monta, na inicialização, um índice
dos arquivos de cada mod; cada abertura vira uma busca em memória (nenhuma
chamada ao SO por mod). Importa em mídia lenta (cartão SD). Só leituras de
arquivos existentes (`OPEN_EXISTING`, sem acesso de escrita) são
redirecionadas — saves e logs nunca. Consequência: arquivo **novo** num mod só
vale após reiniciar o jogo.

**Banco de endereços orientado a dados + verificação por bytes.** Cada símbolo
traz RVA e os bytes esperados. O runtime só o entrega se a memória confere;
`hook_inline`/`patch_bytes` conferem antes de escrever. Build desconhecida →
motor inerte (ou `allow_unknown_build=1` para só os hooks de IAT).

**Sem páginas RWX.** Trampolins são escritos como RW e promovidos a RX; o patch
de código usa `VirtualProtect` temporário. Isso importa para ambientes com
W^X (Horizon/Switch).

**Falha sempre para o lado do jogo vanilla.** Proxy sem `aeonian.dll`, build
desconhecida, `AEONIAN_DISABLE=1`, mod inválido: o jogo roda como o original.

## Fluxo de inicialização (detalhe)

1. O loader do Windows resolve as importações do `Game.exe` e carrega
   `ijl11.dll` (proxy). O `DllMain` do proxy carrega `aeonian\aeonian.dll` e
   chama `AeonianBootstrap()` — ainda sob o *loader lock*, então só se faz
   trabalho leve.
2. `Bootstrap`: abre `aeonian\aeonian.log`, lê `aeonian.ini`, lê o cabeçalho PE
   do `Game.exe`, procura em `aeonian\addresses\*.ini` o banco que confere e
   instala 7 hooks de IAT.
3. A primeira chamada interceptada (normalmente `GetCommandLineA`, no CRT)
   dispara a inicialização tardia: varre `aeonian\mods\*\mod.ini`, ordena por
   prioridade, indexa os `data\`, monta o VFS e carrega os plugins
   (`AeonianPluginInit`).
4. `GetCommandLineA` devolve a linha com `-direct` (se algum mod tem `data\`)
   e as flags de `extra_args`/mods.

## Vídeo: três renderizadores, um evento (`on_present`)

O jogo tem renderizadores de software (GDI, DirectDraw) e o **D2GL** (OpenGL,
alto desempenho). O Aeonian entrega aos plugins sempre a **mesma coisa** — uma
tela GDI com cor-chave transparente — e adapta a composição a cada um:

| Backend | Como intercepta | Composição |
|---|---|---|
| GDI | `GDI32!StretchBlt` (origem = DIB de 8 bits; orientação registrada via `CreateDIBSection`) | escreve no DIB (cores → paleta, cache RGB555), chama o `StretchBlt` e **restaura** os pixels |
| DirectDraw | `DDRAW!DirectDrawCreate` → tabelas COM (`CreateSurface`, `Blt`, `Flip`) | `Lock` → escreve → `Blt` → `Lock` → restaura. **Sem teste** |
| D2GL | `LoadLibraryA("glide3x.dll")` → importação `GDI32!SwapBuffers` do módulo | textura OpenGL + quadrilátero, **salvando e restaurando o estado GL**; recria os recursos se o contexto mudar; canvas ≤ `overlay_max_width`, redesenhado a `overlay_hz` |

Nenhum endereço interno do jogo é usado. O `request_screenshot` lê o buffer do
**próprio jogo** em cada backend (DIB, superfície ou framebuffer GL).

## Portabilidade

Ver [`SCOPE.md` §5](SCOPE.md). Em resumo: x86/PE → Windows e Wine (validado em
Wine 11/Linux); Switch pela camada AUTORUN/Wine-NX do projeto-irmão (não
testado); sem camada x86, **não** por este caminho.
