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

// Plugin nativo de Baú Infinito/Compartilhado e Respec (Compatibilidade PlugY)
// no Game.exe 1.14d.
//
// Especificação: docs/specs/game-exe-1.14d-inventory-stash.md

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

#include "aeonian/api.h"
#include "core/ini.h"
#include "stash_storage.h"

namespace {

using namespace plugy_stash;

const AeonianHost* g_host = nullptr;
StashStorage g_storage;
std::string g_save_dir = "save";
std::string g_current_char_name;

// Ponteiros de funções resolvidos do Game.exe 1.14d
using GetPlayerUnitFn = void* (*)();
using GetUiStateFn = int (__fastcall*)(int ui_id);
using AddUnitStatFn = int (__stdcall*)(void* unit, int stat_id, int value, uint16_t layer);
using GetUnitBaseStatFn = int (__stdcall*)(void* unit, int stat_id, uint16_t layer);
using GetUnitStatFn = int (__stdcall*)(void* unit, int stat_id, uint16_t layer);

GetPlayerUnitFn g_get_player_unit = nullptr;
GetUiStateFn g_get_ui_state = nullptr;
AddUnitStatFn g_add_unit_stat = nullptr;
GetUnitBaseStatFn g_get_unit_base_stat = nullptr;
GetUnitStatFn g_get_unit_stat = nullptr;

const int32_t* g_screen_offset_left_x = nullptr;
const int32_t* g_screen_offset_top_y = nullptr;

// Recursos visuais GDI
HFONT g_font = nullptr;
HFONT g_font_bold = nullptr;

const COLORREF kGold = RGB(199, 179, 119);
const COLORREF kDarkGold = RGB(120, 100, 60);
const COLORREF kDim = RGB(160, 160, 160);
const COLORREF kGreen = RGB(100, 220, 100);
const COLORREF kRed = RGB(220, 100, 100);
const COLORREF kPanel = RGB(20, 16, 12);
const COLORREF kBtnFill = RGB(40, 34, 28);

// Caixas delimitadoras interativas da UI do Baú
struct Button {
  RECT rect;
  int id;
  const char* label;
};

enum ButtonId {
  kBtnFirstPage = 1,
  kBtnPrevPage,
  kBtnNextPage,
  kBtnLastPage,
  kBtnToggleMode,
  kBtnNewPage,
  kBtnRespecStr,
  kBtnRespecEne,
  kBtnRespecDex,
  kBtnRespecVit,
  kBtnRespecSkills,
  kBtnToggleStats,
};

Button g_btn_first = {};
Button g_btn_prev = {};
Button g_btn_next = {};
Button g_btn_last = {};
Button g_btn_mode = {};
Button g_btn_new = {};

Button g_btn_respec_str = {};
Button g_btn_respec_ene = {};
Button g_btn_respec_dex = {};
Button g_btn_respec_vit = {};
Button g_btn_respec_skills = {};
Button g_btn_toggle_stats = {};
bool g_show_extended_stats = false;

HFONT MakeFont(int height, int weight) {
  return CreateFontA(height, 0, 0, 0, weight, FALSE, FALSE, FALSE, ANSI_CHARSET,
                     OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
                     DEFAULT_PITCH | FF_SWISS, "Arial");
}

void DrawBox(HDC dc, const RECT& r, COLORREF fill, COLORREF border) {
  HBRUSH brush = CreateSolidBrush(fill);
  FillRect(dc, &r, brush);
  DeleteObject(brush);
  HBRUSH edge = CreateSolidBrush(border);
  FrameRect(dc, &r, edge);
  DeleteObject(edge);
}

void DrawTextCentered(HDC dc, HFONT font, COLORREF color, const RECT& r, const char* text) {
  SelectObject(dc, font);
  SetTextColor(dc, color);
  DrawTextA(dc, text, static_cast<int>(lstrlenA(text)), const_cast<RECT*>(&r),
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void TextOutAt(HDC dc, HFONT font, COLORREF color, int x, int y, const char* text) {
  SelectObject(dc, font);
  SetTextColor(dc, color);
  TextOutA(dc, x, y, text, static_cast<int>(lstrlenA(text)));
}

bool PtInButton(const Button& btn, int x, int y) {
  POINT pt = {x, y};
  return PtInRect(&btn.rect, pt) != FALSE;
}

void ResolveSymbols(const AeonianHost* host) {
  const uintptr_t player_va = host->resolve_symbol(host, "D2Client.GetPlayerUnit");
  if (player_va != 0) g_get_player_unit = reinterpret_cast<GetPlayerUnitFn>(player_va);

  const uintptr_t get_ui_va = host->resolve_symbol(host, "D2Client.GetUiState");
  if (get_ui_va != 0) g_get_ui_state = reinterpret_cast<GetUiStateFn>(get_ui_va);

  const uintptr_t add_stat_va = host->resolve_symbol(host, "D2Common.AddUnitStat");
  if (add_stat_va != 0) g_add_unit_stat = reinterpret_cast<AddUnitStatFn>(add_stat_va);

  const uintptr_t get_stat_va = host->resolve_symbol(host, "D2Common.GetUnitBaseStat");
  if (get_stat_va != 0) g_get_unit_base_stat = reinterpret_cast<GetUnitBaseStatFn>(get_stat_va);

  const uintptr_t get_total_stat_va = host->resolve_symbol(host, "D2Common.GetUnitStat");
  if (get_total_stat_va != 0) g_get_unit_stat = reinterpret_cast<GetUnitStatFn>(get_total_stat_va);

  const uintptr_t offsets_anchor = host->resolve_symbol(host, "D2Client.UiScreenOffsetsAnchor");
  if (offsets_anchor != 0) {
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

void LoadStashes() {
  const std::string shared_path = g_save_dir + "/_LOD_SharedStashSave.sss";
  if (g_storage.LoadShared(shared_path)) {
    if (g_host) g_host->log(g_host, AEONIAN_LOG_INFO, "[plugy-stash] Bau compartilhado carregado.");
  }

  if (!g_current_char_name.empty()) {
    const std::string personal_path = g_save_dir + "/" + g_current_char_name + ".d2x";
    if (g_storage.LoadPersonal(personal_path)) {
      if (g_host) g_host->log(g_host, AEONIAN_LOG_INFO, "[plugy-stash] Bau pessoal carregado.");
    }
  }
}

void SaveStashes() {
  const std::string shared_path = g_save_dir + "/_LOD_SharedStashSave.sss";
  g_storage.SaveShared(shared_path);

  if (!g_current_char_name.empty()) {
    const std::string personal_path = g_save_dir + "/" + g_current_char_name + ".d2x";
    g_storage.SavePersonal(personal_path);
  }
}

// ---------------------------------------------------------------------------
// Eventos do Plugin
// ---------------------------------------------------------------------------

void OnLoaded(void*) {
  g_font = MakeFont(14, FW_NORMAL);
  g_font_bold = MakeFont(14, FW_BOLD);

  // Carrega configuracao
  if (g_host && g_host->mod_dir) {
    const std::string ini_path = std::string(g_host->mod_dir) + "/plugy-stash.ini";
    std::ifstream in(ini_path);
    if (in.is_open()) {
      std::string content((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());
      aeonian::Ini ini = aeonian::Ini::Parse(content);
      g_save_dir = ini.GetOr("stash", "save_path", "save");
    }
  }

  LoadStashes();
}

void OnShutdown(void*) {
  SaveStashes();
  if (g_font) DeleteObject(g_font);
  if (g_font_bold) DeleteObject(g_font_bold);
}

void OnTick(void*, uint32_t) {
  if (g_get_player_unit == nullptr) return;
  void* player = g_get_player_unit();

  if (player != nullptr) {
    // Lê nome do personagem a partir de PlayerData em Unit+0x14
    const uint8_t* p = reinterpret_cast<const uint8_t*>(player);
    uint32_t player_data_addr = 0;
    std::memcpy(&player_data_addr, p + 0x14, sizeof(player_data_addr));
    if (player_data_addr != 0) {
      const char* name = reinterpret_cast<const char*>(player_data_addr);
      if (name[0] != '\0' && g_current_char_name != name) {
        g_current_char_name = name;
        LoadStashes();
      }
    }
  } else if (!g_current_char_name.empty()) {
    // Saiu da partida: salva stashes
    SaveStashes();
    g_current_char_name.clear();
  }
}

void OnPresent(void*, AeonianCanvas* canvas) {
  if (canvas == nullptr || canvas->hdc == nullptr || g_get_ui_state == nullptr) return;
  HDC dc = static_cast<HDC>(canvas->hdc);
  SetBkMode(dc, TRANSPARENT);

  const int off_x = g_screen_offset_left_x ? *g_screen_offset_left_x : 0;
  const int off_y = g_screen_offset_top_y ? *g_screen_offset_top_y : 0;
  const int h = canvas->height;

  // 1. Painel do Baú (UI 0x19)
  if (g_get_ui_state(0x19) == 1) {
    canvas->opacity = 240;

    // Barra de navegação superior do Baú
    const int bx = off_x + 8;
    const int by = h + off_y - 280;
    const int bw = 304;
    const int bh = 28;

    RECT bar_rect = {bx, by, bx + bw, by + bh};
    DrawBox(dc, bar_rect, kPanel, kGold);

    // Botões de navegação
    int cx = bx + 4;
    const int btn_w = 24;
    const int btn_h = 20;
    const int btn_y = by + 4;

    g_btn_first.rect = {cx, btn_y, cx + btn_w, btn_y + btn_h};
    DrawBox(dc, g_btn_first.rect, kBtnFill, kDarkGold);
    DrawTextCentered(dc, g_font_bold, kGold, g_btn_first.rect, "|<");
    cx += btn_w + 3;

    g_btn_prev.rect = {cx, btn_y, cx + btn_w, btn_y + btn_h};
    DrawBox(dc, g_btn_prev.rect, kBtnFill, kDarkGold);
    DrawTextCentered(dc, g_font_bold, kGold, g_btn_prev.rect, "<");
    cx += btn_w + 3;

    // Indicador de página e modo
    const int info_w = 120;
    RECT info_rect = {cx, btn_y, cx + info_w, btn_y + btn_h};
    DrawBox(dc, info_rect, RGB(15, 12, 10), kDarkGold);

    char page_str[64];
    std::snprintf(page_str, sizeof(page_str), "%s %zu/%zu",
                  g_storage.IsSharedMode() ? "Comp." : "Pess.",
                  g_storage.CurrentPageIndex() + 1, g_storage.PageCount());
    DrawTextCentered(dc, g_font_bold, kGreen, info_rect, page_str);
    cx += info_w + 3;

    g_btn_next.rect = {cx, btn_y, cx + btn_w, btn_y + btn_h};
    DrawBox(dc, g_btn_next.rect, kBtnFill, kDarkGold);
    DrawTextCentered(dc, g_font_bold, kGold, g_btn_next.rect, ">");
    cx += btn_w + 3;

    g_btn_last.rect = {cx, btn_y, cx + btn_w, btn_y + btn_h};
    DrawBox(dc, g_btn_last.rect, kBtnFill, kDarkGold);
    DrawTextCentered(dc, g_font_bold, kGold, g_btn_last.rect, ">|");
    cx += btn_w + 3;

    // Alternar Compartilhado / Pessoal
    const int mode_w = 40;
    g_btn_mode.rect = {cx, btn_y, cx + mode_w, btn_y + btn_h};
    DrawBox(dc, g_btn_mode.rect, kBtnFill, kDarkGold);
    DrawTextCentered(dc, g_font_bold, g_storage.IsSharedMode() ? kGold : kDim,
                     g_btn_mode.rect, g_storage.IsSharedMode() ? "SHR" : "PER");
    cx += mode_w + 3;

    // Botão Adicionar Página
    const int new_w = 20;
    g_btn_new.rect = {cx, btn_y, cx + new_w, btn_y + btn_h};
    DrawBox(dc, g_btn_new.rect, kBtnFill, kGreen);
    DrawTextCentered(dc, g_font_bold, kGreen, g_btn_new.rect, "+");
  }

  // 2. Painel de Personagem (UI 0x02) - Controles de Respec Livre
  if (g_get_ui_state(0x02) == 1) {
    canvas->opacity = 240;

    // Posições dos botões de respec [-] alinhados com Strength, Energy, Dexterity, Vitality
    const int rx = off_x + 138;
    const int btn_sz = 14;

    // Força (Stat 0)
    const int y_str = h + off_y - 148;
    g_btn_respec_str.rect = {rx, y_str, rx + btn_sz, y_str + btn_sz};
    DrawBox(dc, g_btn_respec_str.rect, kBtnFill, kRed);
    DrawTextCentered(dc, g_font_bold, kRed, g_btn_respec_str.rect, "-");

    // Energia (Stat 1)
    const int y_ene = h + off_y - 116;
    g_btn_respec_ene.rect = {rx, y_ene, rx + btn_sz, y_ene + btn_sz};
    DrawBox(dc, g_btn_respec_ene.rect, kBtnFill, kRed);
    DrawTextCentered(dc, g_font_bold, kRed, g_btn_respec_ene.rect, "-");

    // Destreza (Stat 2)
    const int y_dex = h + off_y - 84;
    g_btn_respec_dex.rect = {rx, y_dex, rx + btn_sz, y_dex + btn_sz};
    DrawBox(dc, g_btn_respec_dex.rect, kBtnFill, kRed);
    DrawTextCentered(dc, g_font_bold, kRed, g_btn_respec_dex.rect, "-");

    // Vitalidade (Stat 3)
    const int y_vit = h + off_y - 52;
    g_btn_respec_vit.rect = {rx, y_vit, rx + btn_sz, y_vit + btn_sz};
    DrawBox(dc, g_btn_respec_vit.rect, kBtnFill, kRed);
    DrawTextCentered(dc, g_font_bold, kRed, g_btn_respec_vit.rect, "-");

    // Botão Reset Skills
    g_btn_respec_skills.rect = {off_x + 16, h + off_y - 36, off_x + 105, h + off_y - 18};
    DrawBox(dc, g_btn_respec_skills.rect, kBtnFill, kGold);
    DrawTextCentered(dc, g_font_bold, kGold, g_btn_respec_skills.rect, "Reset Skills");

    // Botão Stats Avançados [>]
    g_btn_toggle_stats.rect = {off_x + 115, h + off_y - 36, off_x + 215, h + off_y - 18};
    DrawBox(dc, g_btn_toggle_stats.rect, kBtnFill, g_show_extended_stats ? kGreen : kDarkGold);
    DrawTextCentered(dc, g_font_bold, g_show_extended_stats ? kGreen : kGold,
                     g_btn_toggle_stats.rect, g_show_extended_stats ? "[<] Ocultar" : "[>] Stats Extra");

    // 3. Painel de Estatísticas Avançadas (quando aberto)
    if (g_show_extended_stats && g_get_player_unit && g_get_unit_stat) {
      void* player = g_get_player_unit();
      if (player != nullptr) {
        const int px = off_x + 325;
        const int py = h + off_y - 280;
        const int pw = 195;
        const int ph = 245;

        RECT pnl_rect = {px, py, px + pw, py + ph};
        DrawBox(dc, pnl_rect, kPanel, kGold);

        int ty = py + 8;
        TextOutAt(dc, g_font_bold, kGold, px + 12, ty, "ESTATÍSTICAS AVANÇADAS");
        ty += 22;

        char buf[64];
        auto draw_stat = [&](const char* label, int stat_id, const char* suffix, COLORREF color) {
          int val = g_get_unit_stat(player, stat_id, 0);
          std::snprintf(buf, sizeof(buf), "%s: %+d%s", label, val, suffix);
          TextOutAt(dc, g_font, color, px + 14, ty, buf);
          ty += 16;
        };

        draw_stat("Magic Find", 80, "%", kGold);
        draw_stat("Extra Gold", 79, "%", kGold);
        draw_stat("Faster Cast (FCR)", 105, "%", kGreen);
        draw_stat("Faster Hit Rec (FHR)", 99, "%", kGreen);
        draw_stat("Attack Speed (IAS)", 93, "%", kGreen);
        draw_stat("Faster Run (FRW)", 96, "%", kGreen);
        draw_stat("Crushing Blow", 136, "%", kRed);
        draw_stat("Deadly Strike", 141, "%", kRed);
        draw_stat("Open Wounds", 135, "%", kRed);
        draw_stat("Damage Reduced", 36, "%", RGB(150, 200, 255));

        int fr = g_get_unit_stat(player, 39, 0);
        int cr = g_get_unit_stat(player, 43, 0);
        int lr = g_get_unit_stat(player, 41, 0);
        int pr = g_get_unit_stat(player, 45, 0);
        std::snprintf(buf, sizeof(buf), "Res: F:%d C:%d L:%d P:%d", fr, cr, lr, pr);
        TextOutAt(dc, g_font, RGB(255, 180, 100), px + 14, ty, buf);
      }
    }
  }
}

int OnWindowMessage(void*, AeonianWindowMessage* msg) {
  if (msg == nullptr) return 0;

  // 1. Cliques com botão esquerdo do mouse
  if (msg->message == WM_LBUTTONDOWN) {
    const int x = static_cast<int>(LOWORD(msg->lparam));
    const int y = static_cast<int>(HIWORD(msg->lparam));

    // Interceptação dos controles do Baú
    if (g_get_ui_state && g_get_ui_state(0x19) == 1) {
      if (PtInButton(g_btn_first, x, y)) {
        g_storage.FirstPage();
        msg->result = 0;
        return 1;
      }
      if (PtInButton(g_btn_prev, x, y)) {
        g_storage.PrevPage();
        msg->result = 0;
        return 1;
      }
      if (PtInButton(g_btn_next, x, y)) {
        g_storage.NextPage();
        msg->result = 0;
        return 1;
      }
      if (PtInButton(g_btn_last, x, y)) {
        g_storage.LastPage();
        msg->result = 0;
        return 1;
      }
      if (PtInButton(g_btn_mode, x, y)) {
        g_storage.SetSharedMode(!g_storage.IsSharedMode());
        msg->result = 0;
        return 1;
      }
      if (PtInButton(g_btn_new, x, y)) {
        g_storage.AddPage("");
        msg->result = 0;
        return 1;
      }
    }

    // Interceptação dos controles de Respec na tela de personagem
    if (g_get_ui_state && g_get_ui_state(0x02) == 1 && g_add_unit_stat && g_get_player_unit) {
      void* player = g_get_player_unit();
      if (player != nullptr) {
        auto DoRespecStat = [&](int stat_id) {
          int base_val = g_get_unit_base_stat ? g_get_unit_base_stat(player, stat_id, 0) : 0;
          if (base_val > 10) {  // Mantém atributos mínimos básicos
            g_add_unit_stat(player, stat_id, -1, 0);
            g_add_unit_stat(player, 4, 1, 0);  // Incrementa statpts (pontos livres)
          }
        };

        if (PtInButton(g_btn_respec_str, x, y)) {
          DoRespecStat(0);
          msg->result = 0;
          return 1;
        }
        if (PtInButton(g_btn_respec_ene, x, y)) {
          DoRespecStat(1);
          msg->result = 0;
          return 1;
        }
        if (PtInButton(g_btn_respec_dex, x, y)) {
          DoRespecStat(2);
          msg->result = 0;
          return 1;
        }
        if (PtInButton(g_btn_respec_vit, x, y)) {
          DoRespecStat(3);
          msg->result = 0;
          return 1;
        }

        if (PtInButton(g_btn_respec_skills, x, y)) {
          uint8_t* p = reinterpret_cast<uint8_t*>(player);
          uint32_t skill_list_addr = 0;
          std::memcpy(&skill_list_addr, p + 0xA8, sizeof(skill_list_addr));
          if (skill_list_addr != 0) {
            uint8_t* slist = reinterpret_cast<uint8_t*>(skill_list_addr);
            uint32_t curr_skill = 0;
            std::memcpy(&curr_skill, slist + 0x04, sizeof(curr_skill));

            int refunded = 0;
            int guard = 0;
            while (curr_skill != 0 && guard++ < 200) {
              uint8_t* s = reinterpret_cast<uint8_t*>(curr_skill);
              int32_t owner_guid = 0;
              std::memcpy(&owner_guid, s + 0x34, sizeof(owner_guid));
              int32_t base_lvl = 0;
              std::memcpy(&base_lvl, s + 0x28, sizeof(base_lvl));

              if (owner_guid == -1 && base_lvl > 0) {
                refunded += base_lvl;
                int32_t zero = 0;
                std::memcpy(s + 0x28, &zero, sizeof(zero));
              }

              uint32_t next_skill = 0;
              std::memcpy(&next_skill, s + 0x04, sizeof(next_skill));
              curr_skill = next_skill;
            }

            if (refunded > 0) {
              g_add_unit_stat(player, 5 /* newskills */, refunded, 0);
              if (g_host) {
                char b[128];
                std::snprintf(b, sizeof(b),
                              "[plugy-stash] Respec de habilidades: %d ponto(s) recuperados!",
                              refunded);
                g_host->log(g_host, AEONIAN_LOG_INFO, b);
              }
            }
          }
          msg->result = 0;
          return 1;
        }

        if (PtInButton(g_btn_toggle_stats, x, y)) {
          g_show_extended_stats = !g_show_extended_stats;
          msg->result = 0;
          return 1;
        }
      }
    }
  }

  // 2. Atalhos de Teclado
  if (msg->message == WM_KEYDOWN) {
    if (msg->wparam == VK_F10) {
      g_show_extended_stats = !g_show_extended_stats;
      msg->result = 0;
      return 1;
    }
    if (g_get_ui_state && g_get_ui_state(0x19) == 1) {
      if (msg->wparam == VK_PRIOR || msg->wparam == VK_LEFT) {  // PageUp / Esquerda
        g_storage.PrevPage();
        msg->result = 0;
        return 1;
      }
      if (msg->wparam == VK_NEXT || msg->wparam == VK_RIGHT) {  // PageDown / Direita
        g_storage.NextPage();
        msg->result = 0;
        return 1;
      }
    }
  }

  return 0;
}

}  // namespace

// Exportação obrigatória do Plugin Aeonian
extern "C" __declspec(dllexport) int AeonianPluginInit(const AeonianHost* host) {
  if (host == nullptr || host->struct_size < sizeof(AeonianHost)) {
    return AEONIAN_ERR_ARGUMENT;
  }
  if (host->game_build != AEONIAN_BUILD_D2LOD_1_14D &&
      host->game_build != AEONIAN_BUILD_D2LOD_1_13C) {
    return AEONIAN_ERR_UNSUPPORTED;
  }

  g_host = host;
  ResolveSymbols(host);

  AeonianEvents events = {};
  events.struct_size = sizeof(events);
  events.on_loaded = OnLoaded;
  events.on_shutdown = OnShutdown;
  events.on_tick = OnTick;
  events.on_present = OnPresent;
  events.on_window_message = OnWindowMessage;

  return host->register_events(host, &events);
}
