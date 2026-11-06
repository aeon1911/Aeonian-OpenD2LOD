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

// Plugin de exemplo de HUD: desenha sobre o jogo usando o evento on_present
// e controla/inspeciona estados de UI do Game.exe (M3).
//
//  * um distintivo discreto no canto superior direito (sempre visivel);
//  * F9 abre/fecha um painel com os mods, FPS, resolucao, estado dos paineis
//    e deslocamentos de tela;
//  * F6 alterna o inventario (UI 0x01);
//  * F7 alterna a folha de personagem (UI 0x02);
//  * F8 alterna a arvore de habilidades (UI 0x04).
//
// Variavel de ambiente AEONIAN_HUD_OPEN=1 abre o painel ja na partida.

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "aeonian/api.h"

namespace {

const AeonianHost* g_host = nullptr;
bool g_panel_open = false;
HFONT g_font = nullptr;
HFONT g_font_bold = nullptr;

// Tipos de funcoes e globais do Game.exe 1.14d
using GetUiStateFn = int (__fastcall*)(int ui_id);
using SetUiStateFn = int (__fastcall*)(int ui_id, int action, int a3);
using GetAutomapPanelSideFn = int (*)();

GetUiStateFn g_get_ui_state = nullptr;
SetUiStateFn g_set_ui_state = nullptr;
GetAutomapPanelSideFn g_get_automap_side = nullptr;

const int32_t* g_screen_offset_left_x = nullptr;
const int32_t* g_screen_offset_top_y = nullptr;

// FPS medido pelo numero de quadros apresentados por segundo.
DWORD g_window_start = 0;
unsigned g_window_frames = 0;
unsigned g_fps = 0;

const COLORREF kGold = RGB(199, 179, 119);
const COLORREF kText = RGB(230, 230, 230);
const COLORREF kDim = RGB(150, 150, 150);
const COLORREF kGreen = RGB(100, 220, 100);
const COLORREF kPanel = RGB(20, 16, 12);

HFONT MakeFont(int height, int weight) {
  return CreateFontA(height, 0, 0, 0, weight, FALSE, FALSE, FALSE, ANSI_CHARSET,
                     OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
                     DEFAULT_PITCH | FF_SWISS, "Arial");
}

void Text(HDC dc, HFONT font, COLORREF color, int x, int y, const char* text) {
  SelectObject(dc, font);
  SetTextColor(dc, color);
  TextOutA(dc, x, y, text, static_cast<int>(lstrlenA(text)));
}

void DrawBox(HDC dc, const RECT& r, COLORREF fill, COLORREF border) {
  HBRUSH brush = CreateSolidBrush(fill);
  FillRect(dc, &r, brush);
  DeleteObject(brush);
  HBRUSH edge = CreateSolidBrush(border);
  FrameRect(dc, &r, edge);
  DeleteObject(edge);
}

void UpdateFps() {
  const DWORD now = GetTickCount();
  if (g_window_start == 0) g_window_start = now;
  ++g_window_frames;
  if (now - g_window_start >= 1000) {
    g_fps = g_window_frames * 1000 / (now - g_window_start);
    g_window_frames = 0;
    g_window_start = now;
  }
}

void ResolveUiSymbols(const AeonianHost* host) {
  const uintptr_t get_ui_va = host->resolve_symbol(host, "D2Client.GetUiState");
  if (get_ui_va != 0) {
    g_get_ui_state = reinterpret_cast<GetUiStateFn>(get_ui_va);
  }
  const uintptr_t set_ui_va = host->resolve_symbol(host, "D2Client.SetUiState");
  if (set_ui_va != 0) {
    g_set_ui_state = reinterpret_cast<SetUiStateFn>(set_ui_va);
  }
  const uintptr_t side_va = host->resolve_symbol(host, "D2Client.GetAutomapPanelSide");
  if (side_va != 0) {
    g_get_automap_side = reinterpret_cast<GetAutomapPanelSideFn>(side_va);
  }

  const uintptr_t offsets_anchor = host->resolve_symbol(host, "D2Client.UiScreenOffsetsAnchor");
  if (offsets_anchor != 0) {
    // bytes: c7 05 [58 28 7a 00] 50 00 00 00 c7 05 [5c 28 7a 00] c4 ff ff ff
    uint8_t buf[24];
    if (host->read_original(host, offsets_anchor, buf, sizeof(buf)) == AEONIAN_OK) {
      uint32_t addr_x = 0;
      uint32_t addr_y = 0;
      std::memcpy(&addr_x, buf + 2, sizeof(addr_x));
      std::memcpy(&addr_y, buf + 12, sizeof(addr_y));
      if (addr_x != 0) g_screen_offset_left_x = reinterpret_cast<const int32_t*>(addr_x);
      if (addr_y != 0) g_screen_offset_top_y = reinterpret_cast<const int32_t*>(addr_y);
    }
  }
}

void OnPresent(void*, AeonianCanvas* canvas) {
  HDC dc = static_cast<HDC>(canvas->hdc);
  UpdateFps();
  SetBkMode(dc, TRANSPARENT);
  canvas->opacity = 235;

  // Distintivo no canto superior direito
  char badge[64];
  std::snprintf(badge, sizeof(badge), "AEONIAN  F9");
  RECT b = {canvas->width - 100, 4, canvas->width - 4, 22};
  DrawBox(dc, b, kPanel, kGold);
  Text(dc, g_font_bold, kGold, b.left + 8, b.top + 2, badge);

  if (!g_panel_open) return;

  const uint32_t mods = g_host->mod_count(g_host);
  const int row = 16;
  const int width = 360;
  const int height = 140 + static_cast<int>(mods) * row;
  RECT p = {canvas->width - width - 4, 28, canvas->width - 4, 28 + height};
  DrawBox(dc, p, kPanel, kGold);
  int y = p.top + 6;
  const int x = p.left + 10;
  Text(dc, g_font_bold, kGold, x, y, "AEONIAN - mods carregados");
  y += row + 4;
  char line[160];
  for (uint32_t i = 0; i < mods; ++i) {
    AeonianModInfo info = {};
    info.struct_size = sizeof(info);
    if (g_host->mod_info(g_host, i, &info) != AEONIAN_OK) continue;
    std::snprintf(line, sizeof(line), "%s  v%s  (prio %d)  %u arq.  %u plugin(s)", info.id,
                  info.version, info.priority, info.data_files, info.plugins);
    Text(dc, g_font, kText, x, y, line);
    y += row;
  }
  if (mods == 0) {
    Text(dc, g_font, kDim, x, y, "(nenhum mod)");
    y += row;
  }
  y += 4;
  std::snprintf(line, sizeof(line), "%u FPS   %dx%d   quadro %u", g_fps, canvas->width,
                canvas->height, canvas->frame);
  Text(dc, g_font, kDim, x, y, line);
  y += row + 2;

  // Estado da UI (M3)
  Text(dc, g_font_bold, kGold, x, y, "Paineis de UI (F6=Inv, F7=Char, F8=Skills):");
  y += row;

  int inv_state = g_get_ui_state ? g_get_ui_state(0x01) : -1;
  int char_state = g_get_ui_state ? g_get_ui_state(0x02) : -1;
  int skill_state = g_get_ui_state ? g_get_ui_state(0x04) : -1;
  int quest_state = g_get_ui_state ? g_get_ui_state(0x0F) : -1;
  int stash_state = g_get_ui_state ? g_get_ui_state(0x19) : -1;

  std::snprintf(line, sizeof(line), "Inv:%s  Char:%s  Tree:%s  Quest:%s  Stash:%s",
                inv_state > 0 ? "ON" : "off",
                char_state > 0 ? "ON" : "off",
                skill_state > 0 ? "ON" : "off",
                quest_state > 0 ? "ON" : "off",
                stash_state > 0 ? "ON" : "off");
  Text(dc, g_font, (inv_state > 0 || char_state > 0 || skill_state > 0) ? kGreen : kText,
       x, y, line);
  y += row;

  const int off_x = g_screen_offset_left_x ? *g_screen_offset_left_x : 0;
  const int off_y = g_screen_offset_top_y ? *g_screen_offset_top_y : 0;
  const int side = g_get_automap_side ? g_get_automap_side() : 0;

  std::snprintf(line, sizeof(line), "Deslocamento de tela: X=%d  Y=%d  (Lado=%d)",
                off_x, off_y, side);
  Text(dc, g_font, kDim, x, y, line);
}

int OnWindowMessage(void*, AeonianWindowMessage* msg) {
  if (msg->message == WM_KEYDOWN && (msg->lparam & (1 << 30)) == 0) {
    if (msg->wparam == VK_F9) {
      g_panel_open = !g_panel_open;
      g_host->log(g_host, AEONIAN_LOG_INFO, g_panel_open ? "painel aberto" : "painel fechado");
    } else if (msg->wparam == VK_F6 && g_set_ui_state) {
      // Alterna inventario (UI 0x01, action 2 = toggle)
      g_set_ui_state(0x01, 2, 0);
      g_host->log(g_host, AEONIAN_LOG_INFO, "UI toggle inventario (F6)");
    } else if (msg->wparam == VK_F7 && g_set_ui_state) {
      // Alterna folha de personagem (UI 0x02)
      g_set_ui_state(0x02, 2, 0);
      g_host->log(g_host, AEONIAN_LOG_INFO, "UI toggle personagem (F7)");
    } else if (msg->wparam == VK_F8 && g_set_ui_state) {
      // Alterna arvore de habilidades (UI 0x04)
      g_set_ui_state(0x04, 2, 0);
      g_host->log(g_host, AEONIAN_LOG_INFO, "UI toggle arvore habilidades (F8)");
    }
  }
  return 0;
}

}  // namespace

extern "C" AEONIAN_PLUGIN_EXPORT int AeonianPluginInit(const AeonianHost* host) {
  g_host = host;
  const char* open = std::getenv("AEONIAN_HUD_OPEN");
  g_panel_open = open != nullptr && open[0] == '1';
  g_font = MakeFont(14, FW_NORMAL);
  g_font_bold = MakeFont(14, FW_BOLD);

  ResolveUiSymbols(host);

  AeonianEvents events = {};
  events.struct_size = sizeof(events);
  events.on_present = OnPresent;
  events.on_window_message = OnWindowMessage;
  return host->register_events(host, &events);
}
