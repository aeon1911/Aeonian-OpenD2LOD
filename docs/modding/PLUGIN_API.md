# API de plugins (v1)

Um plugin é uma DLL **x86 de 32 bits** (PE) que exporta `AeonianPluginInit` e é
listada em `plugin =` do `mod.ini`. A API é **C puro** com ABI estável:
[`aeonian/include/aeonian/api.h`](../../aeonian/include/aeonian/api.h). Pode ser
escrita em C, C++, ou qualquer linguagem que gere uma DLL i686 cdecl.

> ⚠️ Um plugin é código nativo dentro do processo do jogo, **sem sandbox**.
> Instale apenas plugins em que você confia.

## O mínimo

```cpp
#include "aeonian/api.h"

static const AeonianHost* g_host;
static void OnLoaded(void*) { g_host->log(g_host, AEONIAN_LOG_INFO, "oi!"); }

extern "C" AEONIAN_PLUGIN_EXPORT int AeonianPluginInit(const AeonianHost* host) {
  g_host = host;
  AeonianEvents ev = {};
  ev.struct_size = sizeof(ev);
  ev.on_loaded = OnLoaded;
  return host->register_events(host, &ev);   // 0 = manter carregado
}
```

Exemplo completo: `aeonian/examples/hello-plugin/` (compile com a toolchain
i686 — ver `README.md`).

## `AeonianHost` (o que você recebe)

| Campo | Significado |
|---|---|
| `struct_size`, `api_version` | tamanho/versão do host (`AEONIAN_API_VERSION` = 1); campos só são **acrescentados** no fim |
| `game_build` | `AEONIAN_BUILD_D2LOD_1_14D` (1) ou `AEONIAN_BUILD_UNKNOWN` (0) |
| `game_base` | endereço base do `Game.exe` (`0x400000`) |
| `game_dir`, `mod_dir`, `mod_id` | pastas/ids (strings válidas durante todo o processo) |
| `log(host, nível, msg)` | escreve em `aeonian.log` com o prefixo `[mod_id]` |
| `register_events(host, ev)` | registra callbacks; **só** dentro de `AeonianPluginInit` (depois: `AEONIAN_ERR_STATE`) |
| `resolve_symbol(host, "Game.EntryPoint")` | endereço absoluto de um símbolo do banco de endereços, ou `0` se desconhecido **ou** se os bytes esperados não conferem |
| `hook_inline(host, alvo, esperado, n, detour, &trampolim)` | desvia a função em `alvo`; ver abaixo |
| `patch_bytes(host, alvo, esperado|NULL, novos, n)` | sobrescreve `n` bytes (só se `esperado` conferir) |
| `request_screenshot(host, "x.bmp")` | grava o **próximo quadro** (já com os overlays) como BMP de 24 bits; lê só a superfície/framebuffer do **jogo**, nunca a área de trabalho |
| `mod_count(host)`, `mod_info(host, i, &info)` | lista os mods carregados (id, nome, versão, autor, prioridade, nº de arquivos em `data\`, nº de plugins) |

Retorno das funções `int`: `AEONIAN_OK` (0) ou `AEONIAN_ERR_*` (negativos).
`AeonianPluginInit` retornando ≠ 0 descarrega o plugin.

## Eventos (`AeonianEvents`)

Todos chegam na **thread principal do jogo** (qualquer campo pode ser `NULL`):

| Callback | Quando |
|---|---|
| `on_loaded(user)` | o jogo alcançou o laço de mensagens; 1 vez |
| `on_tick(user, n)` | a cada passagem do laço de mensagens (`n` ≥ 1). **Não é um quadro de render** |
| `on_window_message(user, msg)` | mensagem da janela principal, **antes** do jogo. Retorne ≠ 0 para consumi-la (preencha `msg->result`) |
| `on_shutdown(user)` | `ExitProcess` foi chamado |
| `on_present(user, canvas)` | um quadro vai ser apresentado: **desenhe sobre o jogo** (ver abaixo). **Thread:** a principal nos modos GDI/DirectDraw; a de **renderização do D2GL** no modo D2GL |

Ordem de carga/despacho: mods por `priority` (maior primeiro), depois `id`.

## Desenhar sobre o jogo (`on_present`)

`canvas->hdc` é um contexto **GDI** de uma imagem de 32 bits do tamanho do quadro
(800×600 no GDI; a janela, limitada por `overlay_max_width`, no D2GL), inicialmente
preenchida com a **cor-chave** `AEONIAN_CANVAS_KEY` (magenta = transparente).
Desenhe com GDI (`TextOut`, `FillRect`, ...); depois do callback o host compõe na
tela do jogo tudo o que **não** for a cor-chave — convertendo para a paleta no
modo de 8 bits do renderizador original — e **restaura os pixels originais**, então
nada fica gravado no jogo. `canvas->opacity` (0–255) mistura com o jogo.

```cpp
void OnPresent(void*, AeonianCanvas* c) {
  HDC dc = (HDC)c->hdc;
  RECT r = { c->width - 120, 4, c->width - 4, 22 };
  HBRUSH b = CreateSolidBrush(RGB(20, 16, 12)); FillRect(dc, &r, b); DeleteObject(b);
  SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(199, 179, 119));
  TextOutA(dc, r.left + 8, r.top + 2, "meu HUD", 7);
}
```

Dicas: não use a cor-chave nos seus desenhos; use fontes **sem suavização**
(`NONANTIALIASED_QUALITY`) para não deixar bordas magenta; calcule posições a
partir de `canvas->width/height` (não assuma 800×600). Veja
`aeonian/examples/hud-demo/` (distintivo, painel de mods com **F9**, FPS) e
`aeonian/examples/map-overlay/` (lê o estado do jogo no `on_tick` e desenha no
automap: [`MAP_OVERLAY.md`](MAP_OVERLAY.md)).

## Hooks inline

```cpp
uintptr_t alvo = host->resolve_symbol(host, "Game.AlgumaFuncao");   // verificado por bytes
const uint8_t esperado[5] = { /* os 5+ bytes de início da função, conforme a spec */ };
void* original = nullptr;
int rc = host->hook_inline(host, alvo, esperado, 5, (void*)&MeuDetour, &original);
```

Regras (violá-las trava o jogo):

- `n` entre 5 e 32 e **termina numa fronteira de instrução** do alvo.
- Os bytes copiados para o trampolim precisam ser **independentes de posição**,
  exceto que o **primeiro** byte pode ser um `E8`/`E9` relativo (é ajustado).
- A convenção de chamada do detour deve ser a **mesma da função alvo**.
- Instale durante `AeonianPluginInit` (o jogo ainda é de uma thread só).
- Se os bytes em `alvo` ≠ `esperado`, retorna `AEONIAN_ERR_MISMATCH` e **não
  escreve nada**: trate como "esta build não é suportada".

Símbolos disponíveis hoje: 45 em [`addresses/1.14d.ini`](../../aeonian/addresses/1.14d.ini)
— `Game.EntryPoint` (âncora de teste) e os de estado do jogo (unidades, níveis,
saídas, automap, primitivas de desenho), cada um documentado em
[`../specs/game-exe-1.14d-units.md`](../specs/game-exe-1.14d-units.md) e
[`../specs/game-exe-1.14d-levels-automap.md`](../specs/game-exe-1.14d-levels-automap.md).
Vários são **âncoras de dados**: a função não é para chamar; o endereço de um global
está dentro dos bytes verificados, no deslocamento que a descrição do símbolo dá
(exemplo completo: [`MAP_OVERLAY.md`](MAP_OVERLAY.md)). O banco cresce por
contribuição de dados + spec — ver [`../ROADMAP.md`](../ROADMAP.md) e
[`../specs/game-exe-1.14d-hooking-surface.md`](../specs/game-exe-1.14d-hooking-surface.md).

> **Símbolo desviado por outro mod.** `resolve_symbol` confere os bytes **atuais**
> do alvo. Depois que um plugin desvia uma função (`hook_inline`), os primeiros
> bytes dela mudam e quem resolver o mesmo símbolo **depois** recebe 0 (e o log
> diz "bytes nao conferem"). Resolva os símbolos em `AeonianPluginInit`; se o seu
> mod depende de um símbolo que outro mod desvia, dê a ele `priority` maior (carrega
> antes). Exemplo: o `map-overlay` desvia `D2Client.DrawAutomap`.

## Compatibilidade

A API v1 é **estável**: novas funções entram no fim de `AeonianHost` /
`AeonianEvents`; verifique `host->struct_size` antes de usar um campo recente.
Mudança incompatível = `AEONIAN_API_VERSION` 2 e carregamento lado a lado.
