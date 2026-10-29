/* Aeonian-OpenD2LOD
 * Copyright (C) 2026 Aeonian-OpenD2LOD contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/* aeonian/include/aeonian/api.h
 *
 * API publica (C puro, ABI estavel) dos plugins do Aeonian. Um plugin e uma
 * DLL de 32 bits que exporta `AeonianPluginInit` e vive na pasta de um mod:
 *
 *   <jogo>/aeonian/mods/<id>/mod.ini      (manifesto; ver docs/modding/MODS.md)
 *   <jogo>/aeonian/mods/<id>/<plugin>.dll
 *   <jogo>/aeonian/mods/<id>/data/...     (arquivos que substituem os do jogo)
 *
 * Regras de compatibilidade:
 *  - Os campos de AeonianHost/AeonianEvents so sao ACRESCENTADOS no fim, nunca
 *    reordenados. `struct_size` diz ao plugin quais campos existem.
 *  - Todas as funcoes usam a convencao cdecl.
 *  - Os eventos chegam na thread principal do jogo, salvo indicacao contraria.
 */

#ifndef AEONIAN_API_H_
#define AEONIAN_API_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AEONIAN_API_VERSION 1u

/* Identificadores de build do jogo (definidos pelos arquivos em
 * os arquivos .ini de aeonian/addresses; 0 = build nao reconhecida). */
#define AEONIAN_BUILD_UNKNOWN 0u
#define AEONIAN_BUILD_D2LOD_1_14D 1u

/* Codigos de retorno das funcoes que devolvem int. */
#define AEONIAN_OK 0
#define AEONIAN_ERR_ARGUMENT (-1)
#define AEONIAN_ERR_STATE (-2)      /* chamada fora do momento permitido */
#define AEONIAN_ERR_MISMATCH (-3)   /* bytes esperados nao conferem */
#define AEONIAN_ERR_MEMORY (-4)     /* falha de protecao/alocacao */
#define AEONIAN_ERR_UNSUPPORTED (-5)

typedef enum AeonianLogLevel {
  AEONIAN_LOG_DEBUG = 0,
  AEONIAN_LOG_INFO = 1,
  AEONIAN_LOG_WARN = 2,
  AEONIAN_LOG_ERROR = 3
} AeonianLogLevel;

/* Mensagem de janela vista pela janela principal do jogo. */
typedef struct AeonianWindowMessage {
  void* hwnd;
  uint32_t message;
  uintptr_t wparam;
  intptr_t lparam;
  intptr_t result; /* valor devolvido ao sistema quando on_window_message
                      retorna diferente de zero */
} AeonianWindowMessage;

/* Callbacks de eventos. Qualquer campo pode ser NULL. */
/* Tela de desenho entregue a on_present. `hdc` e um contexto GDI de uma
 * imagem de 32 bits do tamanho do quadro do jogo, inicialmente preenchida com
 * a cor-chave (transparente). Desenhe com GDI normalmente; depois do callback
 * o host compoe na tela do jogo tudo que NAO for a cor-chave (convertendo
 * para a paleta quando o jogo esta em 8 bits) e restaura os pixels
 * originais, entao nada que voce desenha fica gravado no jogo.
 * Dicas: nao use a cor-chave nos seus desenhos; fontes sem suavizacao
 * (NONANTIALIASED_QUALITY) evitam bordas magenta. */
#define AEONIAN_CANVAS_KEY 0x00FF00FFu /* COLORREF 0x00BBGGRR: magenta */

typedef struct AeonianCanvas {
  uint32_t struct_size;
  void* hdc;
  int32_t width;
  int32_t height;
  uint32_t opacity; /* 0..255 (padrao 255): mistura com o jogo */
  uint32_t frame;   /* contador de quadros apresentados, comecando em 1 */
} AeonianCanvas;

/* Informacoes de um mod carregado (ver mod_count/mod_info). */
typedef struct AeonianModInfo {
  uint32_t struct_size;
  const char* id;
  const char* name;
  const char* version;
  const char* author;
  int32_t priority;
  uint32_t data_files; /* arquivos em data\ */
  uint32_t plugins;    /* plugins declarados */
} AeonianModInfo;

typedef struct AeonianEvents {
  uint32_t struct_size; /* sizeof(AeonianEvents) */
  void* user;           /* devolvido como primeiro argumento */

  /* O jogo chegou ao seu laco de mensagens: tudo foi inicializado e a janela
   * principal existe. Disparado uma unica vez. */
  void (*on_loaded)(void* user);

  /* Uma passagem do laco de mensagens do jogo (a mesma cadencia do laco
   * principal; NAO e um quadro de renderizacao). `tick` comeca em 1. */
  void (*on_tick)(void* user, uint32_t tick);

  /* Mensagem da janela principal, antes do jogo. Retorne diferente de zero
   * para consumi-la (preencha msg->result); zero deixa o jogo processar. */
  int (*on_window_message)(void* user, AeonianWindowMessage* msg);

  /* O jogo esta encerrando (ExitProcess). */
  void (*on_shutdown)(void* user);

  /* Um quadro esta prestes a ser apresentado. Desenhe em `canvas`. Funciona
   * nos renderizadores GDI e DirectDraw do jogo e no D2GL (OpenGL, via glide).
   * THREAD: nos modos GDI/DirectDraw e a thread principal; no D2GL e a thread
   * de RENDERIZACAO do D2GL (diferente da principal): proteja estado
   * compartilhado com os demais eventos. O tamanho do canvas depende do modo
   * (800x600 no GDI; a janela, limitada por overlay_max_width, no D2GL). */
  void (*on_present)(void* user, AeonianCanvas* canvas);
} AeonianEvents;

typedef struct AeonianHost AeonianHost;

struct AeonianHost {
  uint32_t struct_size; /* sizeof(AeonianHost) conforme o Aeonian que carregou */
  uint32_t api_version; /* AEONIAN_API_VERSION do host */

  uint32_t game_build;  /* AEONIAN_BUILD_* */
  uintptr_t game_base;  /* endereco base do Game.exe na memoria */
  const char* game_dir; /* pasta do jogo, sem barra final */
  const char* mod_id;   /* id deste mod */
  const char* mod_dir;  /* pasta deste mod, sem barra final */

  void (*log)(const AeonianHost* host, AeonianLogLevel level,
              const char* message);

  /* Registra callbacks. So pode ser chamada de dentro de AeonianPluginInit. */
  int (*register_events)(const AeonianHost* host, const AeonianEvents* events);

  /* Endereco absoluto de um simbolo do banco de enderecos da build atual, ou
   * 0 se desconhecido OU se os bytes esperados nao conferem. */
  uintptr_t (*resolve_symbol)(const AeonianHost* host, const char* name);

  /* Desvia a funcao em `target` para `detour`. `expected`/`length` (5..32)
   * sao os bytes que DEVEM estar em `target` (verificados antes de qualquer
   * escrita). Esses bytes sao copiados para um trampolim devolvido em
   * `*trampoline` (chame-o para executar a funcao original). O primeiro
   * byte pode ser um E8/E9 relativo (e ajustado); os demais precisam ser
   * independentes de posicao. Faca isto durante AeonianPluginInit. */
  int (*hook_inline)(const AeonianHost* host, uintptr_t target,
                     const uint8_t* expected, uint32_t length, void* detour,
                     void** trampoline);

  /* Sobrescreve `length` bytes em `target`, so se `expected` (opcional, pode
   * ser NULL) conferir. */
  int (*patch_bytes)(const AeonianHost* host, uintptr_t target,
                     const uint8_t* expected, const uint8_t* replacement,
                     uint32_t length);

  /* Pede que o PROXIMO quadro apresentado (ja com os overlays) seja gravado
   * em `path` como BMP de 24 bits. So le a superficie do proprio jogo (nunca
   * a area de trabalho). Retorna AEONIAN_OK se o pedido foi aceito. */
  int (*request_screenshot)(const AeonianHost* host, const char* path_bmp);

  /* Mods carregados, na ordem de carga (maior prioridade primeiro). */
  uint32_t (*mod_count)(const AeonianHost* host);
  int (*mod_info)(const AeonianHost* host, uint32_t index, AeonianModInfo* out);
};

/* Assinatura e nome do ponto de entrada exportado por cada plugin. Retorne 0
 * para manter o plugin carregado; qualquer outro valor o descarrega. */
typedef int (*AeonianPluginInitFn)(const AeonianHost* host);
#define AEONIAN_PLUGIN_INIT_SYMBOL "AeonianPluginInit"

#if defined(_WIN32)
#define AEONIAN_PLUGIN_EXPORT __declspec(dllexport)
#else
#define AEONIAN_PLUGIN_EXPORT
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AEONIAN_API_H_ */
