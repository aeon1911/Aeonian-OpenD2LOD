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

// Plugin de exemplo de HUD: desenha sobre o jogo usando o evento on_present.
//
//  * um distintivo discreto no canto superior direito (sempre visivel);
//  * F9 abre/fecha um painel com os mods carregados, o FPS e a resolucao.
//
// Mostra o essencial de um mod de interface: desenhar com GDI numa tela do
// host, ouvir o teclado por on_window_message e consultar a API de mods.
// Variavel de ambiente AEONIAN_HUD_OPEN=1 abre o painel ja na partida (util
// em testes automatizados).

#include <windows.h>

#include <cstdio>
#include <cstdlib>

#include "aeonian/api.h"

namespace {

const AeonianHost* g_host = nullptr;
bool g_panel_open = false;
HFONT g_font = nullptr;
HFONT g_font_bold = nullptr;

// FPS medido pelo numero de quadros apresentados por segundo.
DWORD g_window_start = 0;
unsigned g_window_frames = 0;
unsigned g_fps = 0;

const COLORREF kGold = RGB(199, 179, 119);
const COLORREF kText = RGB(230, 230, 230);
const COLORREF kDim = RGB(150, 150, 150);
const COLORREF kPanel = RGB(20, 16, 12);

HFONT MakeFont(int height, int weight) {
  // NONANTIALIASED_QUALITY: sem bordas misturadas com a cor-chave do canvas.
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

void OnPresent(void*, AeonianCanvas* canvas) {
  HDC dc = static_cast<HDC>(canvas->hdc);
  UpdateFps();
  SetBkMode(dc, TRANSPARENT);
  canvas->opacity = 235;  // levemente translucido sobre o jogo

  // Distintivo no canto superior direito.
  char badge[64];
  std::snprintf(badge, sizeof(badge), "AEONIAN  F9");
  RECT b = {canvas->width - 100, 4, canvas->width - 4, 22};
  DrawBox(dc, b, kPanel, kGold);
  Text(dc, g_font_bold, kGold, b.left + 8, b.top + 2, badge);

  if (!g_panel_open) return;

  const uint32_t mods = g_host->mod_count(g_host);
  const int row = 16;
  const int width = 330;
  const int height = 70 + static_cast<int>(mods) * row;
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
  y += 6;
  std::snprintf(line, sizeof(line), "%u FPS   %dx%d   quadro %u", g_fps, canvas->width,
                canvas->height, canvas->frame);
  Text(dc, g_font, kDim, x, y, line);
}

int OnWindowMessage(void*, AeonianWindowMessage* msg) {
  if (msg->message == WM_KEYDOWN && msg->wparam == VK_F9 && (msg->lparam & (1 << 30)) == 0) {
    g_panel_open = !g_panel_open;
    g_host->log(g_host, AEONIAN_LOG_INFO, g_panel_open ? "painel aberto" : "painel fechado");
  }
  return 0;  // nunca consome: o jogo continua vendo todas as teclas
}

}  // namespace

extern "C" AEONIAN_PLUGIN_EXPORT int AeonianPluginInit(const AeonianHost* host) {
  g_host = host;
  const char* open = std::getenv("AEONIAN_HUD_OPEN");
  g_panel_open = open != nullptr && open[0] == '1';
  g_font = MakeFont(14, FW_NORMAL);
  g_font_bold = MakeFont(14, FW_BOLD);
  AeonianEvents events = {};
  events.struct_size = sizeof(events);
  events.on_present = OnPresent;
  events.on_window_message = OnWindowMessage;
  return host->register_events(host, &events);
}
