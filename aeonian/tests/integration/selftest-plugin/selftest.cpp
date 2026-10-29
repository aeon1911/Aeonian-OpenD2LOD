// Aeonian-OpenD2LOD
// Copyright (C) 2026 Aeonian-OpenD2LOD contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// Plugin de AUTOTESTE (so para tests/integration; nao e um exemplo de mod).
// Roda dentro do Game.exe real e escreve linhas "SELFTEST ..." no log do
// Aeonian, que o script run_wine_smoke.sh confere.

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

#include "aeonian/api.h"

namespace {

const AeonianHost* g_host = nullptr;
unsigned g_window_messages = 0;
unsigned g_presents = 0;

void Say(AeonianLogLevel level, const char* format, ...)
    __attribute__((format(printf, 2, 3)));
void Say(AeonianLogLevel level, const char* format, ...) {
  char buf[256];
  va_list args;
  va_start(args, format);
  std::vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  g_host->log(g_host, level, buf);
}

// Funcao em assembly com bytes conhecidos: `mov eax,[esp+4]` (4 bytes) +
// `inc eax` (1 byte) + `ret`. Os 5 primeiros bytes terminam numa fronteira de
// instrucao, como exige hook_inline.
extern "C" int AeSelfTestTarget(int x);
__asm__(
    ".text\n"
    ".globl _AeSelfTestTarget\n"
    "_AeSelfTestTarget:\n"
    "  .byte 0x8b, 0x44, 0x24, 0x04\n"  // mov eax, [esp+4]
    "  .byte 0x40\n"                    // inc eax
    "  .byte 0xc3\n");                  // ret

using TargetFn = int (*)(int);
TargetFn g_trampoline = nullptr;

int Detour(int x) { return g_trampoline(x) * 10; }

void TestSymbolResolution() {
  uintptr_t ep = g_host->resolve_symbol(g_host, "Game.EntryPoint");
  const uintptr_t expected = g_host->game_base + 0x282985;
  Say(ep == expected ? AEONIAN_LOG_INFO : AEONIAN_LOG_ERROR,
      "SELFTEST resolve_symbol(Game.EntryPoint)=0x%08lx %s",
      static_cast<unsigned long>(ep), ep == expected ? "PASS" : "FAIL");
  uintptr_t missing = g_host->resolve_symbol(g_host, "Nao.Existe");
  Say(missing == 0 ? AEONIAN_LOG_INFO : AEONIAN_LOG_ERROR,
      "SELFTEST resolve_symbol(desconhecido)=0 %s", missing == 0 ? "PASS" : "FAIL");
}

void TestInlineHook() {
  const uint8_t expected[5] = {0x8b, 0x44, 0x24, 0x04, 0x40};
  const uint8_t wrong[5] = {0x90, 0x90, 0x90, 0x90, 0x90};
  void* tramp = nullptr;
  int before = AeSelfTestTarget(4);

  // Bytes errados: o hook DEVE ser recusado sem escrever nada.
  int rc = g_host->hook_inline(g_host, reinterpret_cast<uintptr_t>(&AeSelfTestTarget),
                               wrong, 5, reinterpret_cast<void*>(&Detour), &tramp);
  bool untouched = AeSelfTestTarget(4) == 5;
  Say(rc == AEONIAN_ERR_MISMATCH && untouched ? AEONIAN_LOG_INFO : AEONIAN_LOG_ERROR,
      "SELFTEST hook_inline com bytes errados rc=%d intocado=%d %s", rc,
      untouched ? 1 : 0,
      rc == AEONIAN_ERR_MISMATCH && untouched ? "PASS" : "FAIL");

  rc = g_host->hook_inline(g_host, reinterpret_cast<uintptr_t>(&AeSelfTestTarget),
                           expected, 5, reinterpret_cast<void*>(&Detour), &tramp);
  g_trampoline = reinterpret_cast<TargetFn>(tramp);
  int after = AeSelfTestTarget(4);  // (4+1)*10 se o desvio funcionou
  bool ok = rc == AEONIAN_OK && before == 5 && after == 50;
  Say(ok ? AEONIAN_LOG_INFO : AEONIAN_LOG_ERROR,
      "SELFTEST hook_inline rc=%d antes=%d depois=%d %s", rc, before, after,
      ok ? "PASS" : "FAIL");
}

// Valida a correcao do `call rel32` copiado para o trampolim sobre os bytes
// REAIS do entry point do Game.exe (sem executa-lo): o destino decodificado do
// trampolim deve ser igual ao do original.
void TestRealCodeRelocation() {
  uintptr_t ep = g_host->resolve_symbol(g_host, "Game.EntryPoint");
  if (ep == 0) return;
  const uint8_t* original = reinterpret_cast<const uint8_t*>(ep);
  uint8_t head[5];
  std::memcpy(head, original, 5);  // e8 xx xx xx xx
  int32_t rel;
  std::memcpy(&rel, head + 1, 4);
  uintptr_t original_destination = ep + 5 + static_cast<uintptr_t>(rel);

  void* tramp = nullptr;
  int rc = g_host->hook_inline(g_host, ep, head, 5, reinterpret_cast<void*>(&Detour), &tramp);
  bool ok = false;
  if (rc == AEONIAN_OK && tramp != nullptr) {
    const uint8_t* t = static_cast<const uint8_t*>(tramp);
    int32_t trel;
    std::memcpy(&trel, t + 1, 4);
    uintptr_t tramp_destination = reinterpret_cast<uintptr_t>(t) + 5 + static_cast<uintptr_t>(trel);
    ok = t[0] == 0xE8 && tramp_destination == original_destination;
    // O alvo agora e um jmp para o detour.
    ok = ok && original[0] == 0xE9;
  }
  Say(ok ? AEONIAN_LOG_INFO : AEONIAN_LOG_ERROR,
      "SELFTEST relocacao de call no EntryPoint real (destino 0x%08lx) %s",
      static_cast<unsigned long>(original_destination), ok ? "PASS" : "FAIL");
}

DWORD g_loaded_ms = 0;
bool g_clicked = false;
bool g_menu_shot = false;

// Avanca da tela de titulo ao menu principal (clique postado na janela) e
// grava o quadro do menu, onde ficam os botoes que mods de arte trocam.
void DriveToMenu() {
  const DWORD elapsed = GetTickCount() - g_loaded_ms;
  HWND hwnd = FindWindowA("Diablo II", nullptr);
  if (hwnd == nullptr) return;
  if (!g_clicked && elapsed >= 3000) {
    g_clicked = true;
    PostMessageA(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(400, 300));
    PostMessageA(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(400, 300));
    PostMessageA(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(400, 300));
    PostMessageA(hwnd, WM_KEYDOWN, VK_SPACE, 0);
    PostMessageA(hwnd, WM_KEYUP, VK_SPACE, 0);
    Say(AEONIAN_LOG_INFO, "SELFTEST clique postado para sair da tela de titulo");
  }
  if (!g_menu_shot && elapsed >= 8000) {
    g_menu_shot = true;
    std::string path = std::string(g_host->mod_dir) + "\\shot_menu.bmp";
    int rc = g_host->request_screenshot(g_host, path.c_str());
    Say(rc == AEONIAN_OK ? AEONIAN_LOG_INFO : AEONIAN_LOG_ERROR,
        "SELFTEST request_screenshot(menu) rc=%d", rc);
  }
}

void OnLoaded(void*) {
  g_loaded_ms = GetTickCount();
  Say(AEONIAN_LOG_INFO, "SELFTEST on_loaded recebido");
  // register_events fora de AeonianPluginInit tem de ser recusado.
  AeonianEvents late = {};
  late.struct_size = sizeof(late);
  int rc = g_host->register_events(g_host, &late);
  Say(rc == AEONIAN_ERR_STATE ? AEONIAN_LOG_INFO : AEONIAN_LOG_ERROR,
      "SELFTEST register_events tardio rc=%d %s", rc,
      rc == AEONIAN_ERR_STATE ? "PASS" : "FAIL");
}

constexpr UINT kProbeMessage = WM_APP + 0x2A;
constexpr LRESULT kProbeResult = 0xAE;

void OnTick(void*, uint32_t tick) {
  if (g_loaded_ms != 0 && (tick % 64) == 0) DriveToMenu();
  if (tick == 150) {
    std::string path = std::string(g_host->mod_dir) + "\\shot_overlay.bmp";
    int rc = g_host->request_screenshot(g_host, path.c_str());
    Say(rc == AEONIAN_OK ? AEONIAN_LOG_INFO : AEONIAN_LOG_ERROR,
        "SELFTEST request_screenshot rc=%d (presents ate agora: %u)", rc, g_presents);
  }
  if (tick != 100) return;
  Say(AEONIAN_LOG_INFO, "SELFTEST on_tick alcancou %u PASS", tick);
  // Prova deterministica (sem depender de mouse/teclado): envia uma mensagem
  // propria a janela do jogo; o handler abaixo deve consumi-la e devolver
  // kProbeResult, o que so acontece se o subclasse da janela esta ativo.
  HWND hwnd = FindWindowA("Diablo II", nullptr);
  LRESULT r = hwnd != nullptr ? SendMessageA(hwnd, kProbeMessage, 0x1234, 0x5678) : -1;
  Say(r == kProbeResult ? AEONIAN_LOG_INFO : AEONIAN_LOG_ERROR,
      "SELFTEST on_window_message consumiu a mensagem de prova (hwnd=%p ret=0x%lx) %s",
      static_cast<void*>(hwnd), static_cast<unsigned long>(r),
      r == kProbeResult ? "PASS" : "FAIL");
}

int OnWindowMessage(void*, AeonianWindowMessage* msg) {
  if (msg->message == kProbeMessage && msg->wparam == 0x1234 && msg->lparam == 0x5678) {
    msg->result = kProbeResult;
    return 1;  // consumida: o jogo nunca a ve
  }
  if (++g_window_messages == 1) {
    Say(AEONIAN_LOG_INFO, "SELFTEST on_window_message recebeu a 1a mensagem do jogo (0x%x) PASS",
        msg->message);
  }
  return 0;
}

// Desenha um retangulo vermelho com texto: serve para conferir, no quadro
// gravado pelo host, que a composicao chegou na superficie do jogo.
void OnPresent(void*, AeonianCanvas* canvas) {
  ++g_presents;
  HDC dc = static_cast<HDC>(canvas->hdc);
  HBRUSH brush = CreateSolidBrush(RGB(255, 0, 0));
  RECT box = {10, 10, 210, 60};
  FillRect(dc, &box, brush);
  DeleteObject(brush);
  SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, RGB(255, 255, 255));
  TextOutA(dc, 18, 26, "AEONIAN SELFTEST", 16);
  if (g_presents == 30) {
    Say(AEONIAN_LOG_INFO, "SELFTEST on_present quadro %u (%dx%d) PASS", canvas->frame,
        canvas->width, canvas->height);
  }
}

void OnShutdown(void*) {
  Say(AEONIAN_LOG_INFO, "SELFTEST on_shutdown recebido (mensagens de janela: %u)",
      g_window_messages);
}

}  // namespace

extern "C" AEONIAN_PLUGIN_EXPORT int AeonianPluginInit(const AeonianHost* host) {
  g_host = host;
  Say(AEONIAN_LOG_INFO, "SELFTEST init: api=%u build=%u base=0x%08lx struct_size=%u",
      host->api_version, host->game_build,
      static_cast<unsigned long>(host->game_base), host->struct_size);
  TestSymbolResolution();
  TestInlineHook();
  TestRealCodeRelocation();

  AeonianEvents events = {};
  events.struct_size = sizeof(events);
  events.on_loaded = OnLoaded;
  events.on_tick = OnTick;
  events.on_window_message = OnWindowMessage;
  events.on_shutdown = OnShutdown;
  events.on_present = OnPresent;
  return host->register_events(host, &events);
}
