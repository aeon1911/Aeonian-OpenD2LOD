# Spec — Superfície de interceptação do `Game.exe` 1.14d

> **Tipo:** análise (fatos de interoperabilidade). Não contém código da
> Blizzard nem descompilado. Alimenta `aeonian/host/hooks.cpp`,
> `aeonian/addresses/1.14d.ini` e a camada de VFS.
>
> **Evidência:** `Game.exe` 1.14d (PE32, 3.618.792 bytes), SHA-256
> `631066c1649c4ea9ffe48bf97e24c00bca1f7a6759c21150f1a79982589adaaf`
> (inventário em [`../versions/1.14d.md`](../versions/1.14d.md)).
> Método: leitura do cabeçalho PE/tabela de importação/seções com `struct`
> do Python; busca de strings; execução sob Wine 11 com o Aeonian registrando
> as chamadas; trace de arquivos do próprio Wine (`WINEDEBUG=+file`) como
> prova independente. **Nenhuma função interna do jogo foi decompilada**
> para produzir este documento.

## 1. Identidade da build

| Campo | Valor |
|---|---|
| Machine | `0x014C` (i386), PE32 |
| TimeDateStamp | `0x574DDFBC` |
| SizeOfImage | `0x005BA000` |
| ImageBase | `0x00400000`; `DllCharacteristics = 0` (**sem ASLR**) → carrega em endereço fixo (RVA + `0x400000`) |
| AddressOfEntryPoint (RVA) | `0x00282985` |
| Bytes no entry point | `E8 1B 9F 00 00 E9 16 FE FF FF` (um `call rel32` + um `jmp rel32`: o stub do CRT) |
| Seções | `.text` RVA `0x1000` (`0x2CA5A1`), `.rdata` `0x2CC000`, `.data` `0x305000`, `.rsrc` `0x595000`, `.reloc` `0x597000` |

O banco `aeonian/addresses/1.14d.ini` identifica a build por
`timestamp + size_of_image + entry_rva` (e o instalador por SHA-256).

## 2. Por que o jogo é "abrível" por importação

O `Game.exe` 1.14d é **monolítico** (Storm/Fog/D2*.dll linkados estaticamente),
mas ainda importa do sistema tudo o que o Aeonian precisa para interceptar
**sem conhecer nenhum endereço interno**. As entradas da IAT (VA do slot = RVA + `0x400000`):

| Importação | Slot (VA) | Uso no Aeonian |
|---|---|---|
| `KERNEL32!GetCommandLineA` | `0x6CC3AC` | acrescenta flags (ex.: `-direct`) à linha de comando |
| `KERNEL32!CreateFileA` | `0x6CC19C` | VFS: redireciona leituras de `data\...` para o mod |
| `KERNEL32!GetFileAttributesA` | `0x6CC1B0` | VFS: existência de arquivos do mod |
| `KERNEL32!ExitProcess` | `0x6CC2E0` | evento `on_shutdown` |
| `USER32!PeekMessageA` | `0x6CC480` | cadência do laço principal → `on_tick` |
| `USER32!RegisterClassA` | — | descobre a(s) classe(s) de janela do jogo |
| `USER32!CreateWindowExA` | — | subclasse da janela principal → `on_window_message` |
| `USER32!GetMessageA`, `DefWindowProcA`, `DispatchMessageA`, `TranslateMessage` | — | (importadas; não interceptadas) |
| `DDRAW!DirectDrawCreate`, `DSOUND!#1/#2` | `0x6CC07C` | **candidatas** a hooks de vídeo/áudio (ainda não usadas) |
| `IJL11!#2,#3,#5` (`ijlInit/ijlFree/ijlWrite`) | — | **ponto de entrada**: o proxy `ijl11.dll` |

Também importadas (não usadas ainda): `CreateFileW`, `FindFirstFileA/W`,
`GetModuleFileNameA`, `GetCurrentDirectoryA`, `timeGetTime`, `Sleep`,
`QueryPerformanceCounter`, `LoadLibraryA`, `GetProcAddress`.
`GetFileAttributesExA` **não** é importada.

### 2.1 `ijl11.dll` como ponto de entrada

O jogo importa `ijl11.dll` (Intel JPEG Library; usada nos screenshots) **por
ordinal** (2, 3, 5). A DLL original exporta seis nomes: `ijlGetLibVersion`(1),
`ijlInit`(2), `ijlFree`(3), `ijlRead`(4), `ijlWrite`(5), `ijlErrorStr`(6). O
proxy do Aeonian exporta os mesmos nomes/ordinais como *forwarders* para
`ijl11_real.<nome>`, então o jogo (e os screenshots) seguem iguais. O
carregador do Windows (e o Wine) mapeia a DLL da pasta do aplicativo antes da
do sistema, e a `ijl11.dll` não é uma DLL embutida do Wine.

## 3. Modo de arquivos soltos (`-direct`)

A linha de comando é interpretada por uma tabela de flags no `.data`
(região `0x706000`–`0x706700`). As strings ali, em ordem, incluem
`NOSOUND`, `NOPREDLOAD`/`npl`, `DIRECT`/`direct`, `LOWEND`/`lem`,
`QuEsTs`/`questall`, `COMINT`/`comint`, `SKIPTOBNET`/`skiptobnet`,
`OPENC`/`openc`, `NOCOMPRESS`/`nocompress`, `TXT`/`txt`, `BUILD`/`build`,
`nosound` e `SOUNDBKG`/`sndbkg`, intercaladas com rótulos em maiúsculas
(`DEBUG`, `FILEIO`, `NETWORK`) que **aparentam** ser categorias (leitura nossa,
não confirmada). Os nomes das flags ficam **sem o `-`** (por isso `-direct`
não aparece literal no binário). Que `-direct` e `-skiptobnet` são aceitas na
1.14d foi confirmado por execução (§3), não só pela presença das strings.

**Observado (Wine 11, Aeonian injetando `-direct`):**

- O jogo passa a pedir arquivos soltos de `data\`; o caminho chega a
  `CreateFileA`/`GetFileAttributesA` **relativo** e com caixa mista, por
  exemplo `.\DATA\GLOBAL\ui\FrontEnd\WideButtonBlank.dc6`.
- **Sem pasta `data\` no disco, o jogo cai para os MPQs**: arquivos ausentes
  em `data\` não são erro; o jogo segue rodando por dezenas de segundos e
  carregando os assets do front-end/seleção de personagem a partir dos MPQs.
  (A tela em si **não foi inspecionada visualmente**; a evidência é o log, o
  trace de arquivos e a ausência de saída/queda do processo.)
- Quando o Aeonian devolve um arquivo de mod para um desses pedidos, o
  sistema operacional abre **o arquivo do mod** (confirmado pelo trace de
  arquivos do Wine) e o jogo continua normalmente.
- Os pedidos vêm de mais de uma thread (ex.: thread principal e uma thread de
  carga).
- O jogo repete a consulta/abertura do mesmo arquivo várias vezes.

### 3.1 Pedidos observados no início (amostra, 85 caminhos únicos)

Ordem aproximada, relativa a `data\`: `local\use` (sondagem de existência),
`local\Font\Latin\Font16/…/FontExocet10.{dc6,tbl}`, `GLOBAL\UI\Font\...`,
`GLOBAL\UI\CURSOR\*.dc6`, `Global\Items\Palette\*.dat`,
`GLOBAL\palette\act1\pal.{dat,pl2}`, `local\font\LATIN\default.map`,
`local\lng\ENG\{string,patchstring,expansionstring}.tbl`,
`GLOBAL\UI\FrontEnd\*.dc6` (telas, botões, logo), `GLOBAL\ui\CharSelect\*.dc6`,
`GLOBAL\palette\sky\pal.{dat,pl2}`, `Global\Realms.bin`,
`global\sfx\cursor\{wavindx,curindx}.wav`, `global\chars\am\cof\*.cof`.
Ou seja: **toda a arte de UI e as tabelas passam pelo caminho `data\`** e
são substituíveis por mod.

## 4. Janela e laço de mensagens

- O jogo registra **uma** classe de janela via `RegisterClassA` com o nome
  `Diablo II` e cria a janela principal (`Diablo II`) com `CreateWindowExA`.
  A janela é ANSI (`CallWindowProcA` é o encadeamento correto).
- O laço usa `PeekMessageA`. No início da execução foram observadas ≥100
  chamadas em ~0,2 s após o laço ser alcançado.
- Subclassar a janela com `SetWindowLongA(GWL_WNDPROC)` logo após o
  `CreateWindowExA` retornar funciona: mensagens enviadas por `SendMessageA`
  passam pelo subclasse e podem ser consumidas.
- O volume de mensagens de janela varia entre execuções: numa, o handler
  recebeu ≥50 mensagens em 0,3 s; noutra, menos de 50 em 25 s. Hipótese (não
  confirmada): depende de eventos de mouse sobre a janela. **Não use volume
  de mensagens como critério de teste**; o teste de integração envia uma
  mensagem própria e confere que ela é consumida.

## 4b. Renderização e vídeo (evidência de 2026-10-28)

- **Renderizador GDI** (o que roda com `Render=1` no registro, valor que o Wine
  grava na primeira execução): o jogo importa `CreateDIBSection`,
  `SetDIBColorTable`, `StretchBlt`, `SelectPalette`, `RealizePalette`,
  `CreatePalette`, `GdiFlush`. O quadro é um **DIB de 800×600, 8 bits, de cima
  para baixo** (orientação confirmada pelo `biHeight` negativo passado ao
  `CreateDIBSection`) e é enviado à janela por `StretchBlt` (origem 0,0 800×600 →
  destino 800×600). **`GetObject(HBITMAP)` do Wine devolve a altura normalizada**,
  então a orientação não pode ser lida dali (o primeiro teste saiu invertido).
- `DirectDrawCreate` **não** é chamado nesse modo. Forçar DirectDraw via `Render`
  ou `-w`/sem `-w` não funcionou no Wine (continuou GDI); o modo DirectDraw ficou
  **sem validação**.
- **Vídeos (Bink):** `binkw32!_BinkOpen@8` é chamado com `flags=0x00800000`
  (`BINKFILEHANDLE`: o 1º argumento é um **HANDLE de arquivo**, não um nome);
  `GetFinalPathNameByHandle` devolve o caminho de um MPQ (`d2music.mpq` nos
  testes), logo o **nome do vídeo não é recuperável** nessa chamada. Foram vistos
  **dois** `BinkOpen` nos primeiros ~4 s (logos de abertura). Retornar `NULL`
  faz o jogo **seguir sem o vídeo** e chegar ao título.
- **`-3dfx` + D2GL:** o jogo carrega `glide3x.dll` por `LoadLibraryA` (importada);
  o `glide3x.dll` do D2GL v1.3.3 importa `GDI32!SwapBuffers` e
  `OPENGL32!wglGetProcAddress/wglGetCurrentDC` e chama `SetProcessDPIAware`. O
  D2GL apresenta o quadro **numa thread própria** (diferente da principal).
  `Render=1` + `-3dfx` dá uma janela de tamanho estranho e a abertura toca
  pequena; `Render` ∈ {2,3,4,5} respeitam o tamanho do `d2gl.ini`. O wrapper
  `ddraw.dll` do D2GL **travou** na inicialização sob Wine 11 (também sem o
  Aeonian).

## 5. Garantias que o motor depende

1. As entradas de IAT acima existem na 1.14d (verificado; o motor registra
   cada importação ausente e segue sem ela).
2. O entry point tem os bytes da §1 (símbolo `Game.EntryPoint`, verificado por
   bytes em runtime e por teste de integração).
3. Hooks inline recusam bytes diferentes do esperado (testado com bytes
   errados).
4. `GDI32!StretchBlt`, `GDI32!CreateDIBSection`, `KERNEL32!LoadLibraryA`,
   `binkw32!_BinkOpen@8` e `smackw32!_SmackOpen@12` existem na IAT (verificado;
   o motor registra cada importação ausente e segue sem ela).

## 6. Questões em aberto / próximos alvos de análise

| Alvo | Para quê | Como achar (ideia) |
|---|---|---|
| Função de apresentação do quadro (flip/blit) | `on_present`: desenhar HUD próprio, medir FPS | xrefs de `DirectDrawCreate`; vtable do `IDirectDrawSurface` |
| Constantes/tabelas de layout de painéis (inventário, stats, skills) | mover/estender HUD por plugin | strings de arquivos DC6 de painel + xrefs |
| Ponteiro global do jogador / estado do jogo | ler HP/mana/posição em plugins | strings `D2Client` (`__FILE__`) + xrefs |
| Tabela de classes (`charstats`) e telas de seleção | novas classes | `charselect*.dc6` + xrefs |
| Loader de `string.tbl` | strings novas/UI localizada | strings + xrefs |
| Semântica de `data\local\use` | entender o que a sondagem decide | trace + xrefs |
| `FindFirstFileA/W` | entender se enumeram `data\` | log em hook de diagnóstico |

Cada alvo concluído vira uma spec própria + entradas em
`aeonian/addresses/1.14d.ini` (com `bytes` para verificação).
