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

// Mod de mapa (C1/C2 do docs/ROADMAP.md): com o automap do jogo aberto (tela
// cheia ou minimapa), desenha por cima dele
//
//  * os monstros vivos do nivel atual, com destaque para campeao, unico e
//    superunico;
//  * os objetivos que a spec permite identificar (NPCs e os objetos da
//    configuracao, como o waypoint);
//  * as saidas do nivel: warps (cavernas, escadas) e bordas abertas com
//    outros niveis.
//
// So LE o estado do jogo. O unico codigo do jogo tocado e um gancho opcional
// no inicio de DrawAutomap que apenas conta as chamadas (para desenhar so nos
// quadros em que o jogo desenhou o automap). A leitura acontece no on_tick
// (thread principal); o on_present so desenha a ultima lista publicada, porque
// no D2GL ele roda na thread de renderizacao.

#include <windows.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "aeonian/api.h"
#include "core/ini.h"
#include "game_memory.h"
#include "map_logic.h"

// Gancho de DrawAutomap: incrementa o contador e segue para a original pelo
// trampolim, sem tocar em pilha nem registradores (a 1a instrucao da original
// e um cmp, que refaz as flags).
extern "C" {
volatile LONG g_ae_map_automap_draws = 0;
void* g_ae_map_draw_trampoline = nullptr;
void AeMapOverlayDrawAutomapDetour();
}
__asm__(
    ".text\n"
    ".globl _AeMapOverlayDrawAutomapDetour\n"
    "_AeMapOverlayDrawAutomapDetour:\n"
    "  lock incl _g_ae_map_automap_draws\n"
    "  jmp *_g_ae_map_draw_trampoline\n");

namespace {

using map_overlay::AutomapView;
using map_overlay::Mark;
using map_overlay::MarkKind;
using map_overlay::ScreenMark;

// Comprimento da 1a instrucao de DrawAutomap (`cmp dword [abs32], 0`, sem
// operando relativo), ponto de desvio recomendado pela spec de niveis sec. 5.3.
constexpr uint32_t kDrawHookLength = 7;
// Sem chamada de DrawAutomap ha mais que isto, o automap nao esta na tela.
constexpr DWORD kDrawnWindowMs = 300;
// Intervalo minimo entre dois resumos no log.
constexpr DWORD kSummaryEveryMs = 5000;

const AeonianHost* g_host = nullptr;

struct Config {
  bool enabled = true;
  UINT toggle_vk = VK_F11;
  bool toggle_ctrl = false;
  bool toggle_shift = false;
  bool labels = true;
  bool status = true;
  bool draw_hook = true;
  DWORD refresh_ms = 100;
  int32_t game_width = 800;
  int32_t game_height = 600;
  bool keep_aspect = true;
  // Do d2gl.ini da pasta do jogo (ver FitFrameD2gl); 0 = desconhecido.
  bool use_d2gl_ini = true;
  bool d2gl_ini = false;
  bool d2gl_fullscreen = false;
  int32_t d2gl_window_w = 0;
  int32_t d2gl_window_h = 0;
  bool d2gl_stretch_h = false;
  bool d2gl_stretch_v = false;
  map_overlay::ScanOptions scan;
  std::vector<std::wstring> object_names;     // por indice de scan.objects_to_mark
  std::map<int32_t, std::wstring> level_names;  // id de levels.txt -> nome
};

Config g_cfg;
map_overlay::GameAddresses g_addr;
uint32_t g_features = 0;
bool g_hooked = false;

void Logf(AeonianLogLevel level, const char* format, ...)
    __attribute__((format(printf, 2, 3)));
void Logf(AeonianLogLevel level, const char* format, ...) {
  char buf[512];
  va_list args;
  va_start(args, format);
  std::vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  g_host->log(g_host, level, buf);
}

map_overlay::GameMemory g_mem;  // so a thread principal usa

// --- Estado compartilhado com on_present -------------------------------------

struct Counts {
  uint32_t monsters = 0;
  uint32_t special = 0;  // campeoes + unicos + superunicos
  uint32_t objectives = 0;
  uint32_t exits = 0;
};

struct Shared {
  bool active = false;  // em partida, automap visivel e desenhado pelo jogo
  AutomapView view;     // estado do automap no ultimo tick
  int32_t level_id = 0;
  std::vector<Mark> marks;
  Counts counts;
};

SRWLOCK g_lock = SRWLOCK_INIT;
Shared g_shared;  // protegido por g_lock

// So a thread principal (on_tick/on_window_message e o on_present do GDI).
DWORD g_main_thread = 0;
bool g_enabled = true;
bool g_was_active = false;
LONG g_tick_draws = 0;
DWORD g_last_draw_ms = 0;
DWORD g_last_scan_ms = 0;
DWORD g_last_summary_ms = 0;
int32_t g_summary_level = -1;
map_overlay::ScanStats g_summary_stats;
bool g_logged_view = false;
bool g_in_game_logged = false;
map_overlay::Scan g_scan;
uint32_t g_scans = 0;
double g_scan_ms_total = 0.0;
double g_scan_ms_max = 0.0;
LONG g_present_draws = 0;  // so o on_present da thread principal

// --- Configuracao ------------------------------------------------------------

std::wstring Widen(const std::string& utf8) {
  if (utf8.empty()) return std::wstring();
  const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                                    nullptr, 0);
  if (n <= 0) return std::wstring();
  std::wstring out(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), n);
  return out;
}

bool ReadFileText(const std::string& path, std::string* out) {
  HANDLE f = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
  if (f == INVALID_HANDLE_VALUE) return false;
  char buf[4096];
  DWORD got = 0;
  out->clear();
  while (ReadFile(f, buf, sizeof(buf), &got, nullptr) && got > 0 && out->size() < (1u << 20)) {
    out->append(buf, got);
  }
  CloseHandle(f);
  return true;
}

// "f11", "ctrl+f9", "shift+k"...
bool ParseKey(const std::string& text, Config* cfg) {
  std::string rest = aeonian::ToLowerAscii(aeonian::Trim(text));
  bool ctrl = false;
  bool shift = false;
  for (;;) {
    if (rest.rfind("ctrl+", 0) == 0) {
      ctrl = true;
      rest.erase(0, 5);
    } else if (rest.rfind("shift+", 0) == 0) {
      shift = true;
      rest.erase(0, 6);
    } else {
      break;
    }
  }
  UINT vk = 0;
  if (rest.size() >= 2 && rest[0] == 'f') {
    const auto n = aeonian::ParseInt(rest.substr(1));
    if (n && *n >= 1 && *n <= 24) vk = static_cast<UINT>(VK_F1 + *n - 1);
  } else if (rest.size() == 1 && ((rest[0] >= 'a' && rest[0] <= 'z') ||
                                  (rest[0] >= '0' && rest[0] <= '9'))) {
    vk = static_cast<UINT>(rest[0] >= 'a' ? rest[0] - 'a' + 'A' : rest[0]);
  }
  if (vk == 0) return false;
  cfg->toggle_vk = vk;
  cfg->toggle_ctrl = ctrl;
  cfg->toggle_shift = shift;
  return true;
}

void LoadConfig() {
  Config cfg;
  cfg.scan.objects_to_mark.push_back(map_overlay::ObjectLabel{119, "WP"});
  const std::string path = std::string(g_host->mod_dir) + "\\map-overlay.ini";
  std::string text;
  if (!ReadFileText(path, &text)) {
    Logf(AEONIAN_LOG_INFO, "sem map-overlay.ini; usando a configuracao padrao");
  } else {
    const aeonian::Ini ini = aeonian::Ini::Parse(text);
    cfg.enabled = ini.GetBool("overlay", "enabled", true);
    const std::string key = ini.GetOr("overlay", "toggle_key", "f11");
    if (!ParseKey(key, &cfg)) {
      Logf(AEONIAN_LOG_WARN, "toggle_key '%s' invalida; usando f11", key.c_str());
    }
    cfg.scan.monsters = ini.GetBool("overlay", "monsters", true);
    cfg.scan.npcs = ini.GetBool("overlay", "npcs", true);
    cfg.scan.objects = ini.GetBool("overlay", "objects", true);
    cfg.scan.exits = ini.GetBool("overlay", "exits", true);
    cfg.scan.only_current_level = ini.GetBool("overlay", "only_current_level", true);
    cfg.labels = ini.GetBool("overlay", "labels", true);
    cfg.status = ini.GetBool("overlay", "status", true);
    cfg.draw_hook = ini.GetBool("overlay", "draw_hook", true);
    const int64_t refresh = ini.GetInt("overlay", "refresh_ms", 100);
    cfg.refresh_ms = static_cast<DWORD>(refresh < 20 ? 20 : refresh > 2000 ? 2000 : refresh);
    const int64_t gw = ini.GetInt("d2gl", "game_width", 800);
    const int64_t gh = ini.GetInt("d2gl", "game_height", 600);
    if (gw >= 320 && gw <= 8192 && gh >= 200 && gh <= 8192) {
      cfg.game_width = static_cast<int32_t>(gw);
      cfg.game_height = static_cast<int32_t>(gh);
    }
    cfg.keep_aspect = ini.GetBool("d2gl", "keep_aspect", true);
    cfg.use_d2gl_ini = ini.GetBool("d2gl", "use_d2gl_ini", true);
    if (ini.HasSection("objects")) {
      cfg.scan.objects_to_mark.clear();
      for (const auto& [k, v] : ini.Section("objects")) {
        const auto id = aeonian::ParseInt(k);
        if (!id || *id < 0 || *id > 0xFFFF) continue;
        cfg.scan.objects_to_mark.push_back(
            map_overlay::ObjectLabel{static_cast<uint32_t>(*id), v});
      }
    }
    for (const auto& [k, v] : ini.Section("level_names")) {
      const auto id = aeonian::ParseInt(k);
      if (id && *id > 0 && *id < 1024) cfg.level_names[static_cast<int32_t>(*id)] = Widen(v);
    }
  }
  for (const auto& o : cfg.scan.objects_to_mark) cfg.object_names.push_back(Widen(o.label));
  // Geometria do D2GL: o tamanho de janela que ele usa para encaixar o quadro
  // vem do d2gl.ini (TODO(RE): nao ha spec do D2GL; modelo observado no
  // 1.3.3, ver FitFrameD2gl).
  std::string d2gl_text;
  if (cfg.use_d2gl_ini &&
      ReadFileText(std::string(g_host->game_dir) + "\\d2gl.ini", &d2gl_text)) {
    const aeonian::Ini d2gl = aeonian::Ini::Parse(d2gl_text);
    const int64_t w = d2gl.GetInt("screen", "window_width", 0);
    const int64_t h = d2gl.GetInt("screen", "window_height", 0);
    cfg.d2gl_fullscreen = d2gl.GetBool("screen", "fullscreen", false);
    cfg.d2gl_stretch_h = d2gl.GetBool("graphic", "stretched_horizontal", false);
    cfg.d2gl_stretch_v = d2gl.GetBool("graphic", "stretched_vertical", false);
    if (w > 0 && w <= 16384 && h > 0 && h <= 16384) {
      cfg.d2gl_ini = true;
      cfg.d2gl_window_w = static_cast<int32_t>(w);
      cfg.d2gl_window_h = static_cast<int32_t>(h);
    }
  }
  g_cfg = std::move(cfg);
  if (g_cfg.d2gl_ini) {
    Logf(AEONIAN_LOG_INFO, "d2gl.ini: janela %dx%d, tela cheia %d, esticado h=%d v=%d",
         g_cfg.d2gl_window_w, g_cfg.d2gl_window_h, g_cfg.d2gl_fullscreen ? 1 : 0,
         g_cfg.d2gl_stretch_h ? 1 : 0, g_cfg.d2gl_stretch_v ? 1 : 0);
  }
}

// --- Simbolos e gancho -------------------------------------------------------

uint32_t ResolveForLogic(void*, const char* name) {
  return static_cast<uint32_t>(g_host->resolve_symbol(g_host, name));
}

void ResolveSymbols() {
  map_overlay::ResolveReport report;
  const map_overlay::OriginalMemory original(g_host, g_mem);
  map_overlay::ResolveAddresses(original, ResolveForLogic, nullptr, &g_addr, &report);
  g_features = map_overlay::AvailableFeatures(g_addr);
  for (const std::string& name : report.missing) {
    Logf(AEONIAN_LOG_WARN, "simbolo %s nao resolvido (ausente ou bytes nao conferem)",
         name.c_str());
  }
  for (const std::string& note : report.notes) Logf(AEONIAN_LOG_WARN, "%s", note.c_str());
  Logf(report.missing.empty() ? AEONIAN_LOG_INFO : AEONIAN_LOG_WARN,
       "simbolos: %u/%u resolvidos", report.resolved, report.total);
  Logf(AEONIAN_LOG_DEBUG, "ancoras: jogador 0x%08x, ato 0x%08x, unidades 0x%08x, recorte 0x%08x",
       g_addr.player_unit, g_addr.current_act, g_addr.server_units, g_addr.automap_clip);
  struct {
    uint32_t bit;
    const char* what;
  } const features[] = {
      {map_overlay::kFeatureAutomap, "marcas no automap (tudo)"},
      {map_overlay::kFeatureMonsters, "monstros e NPCs"},
      {map_overlay::kFeatureObjectUnits, "objetos/saidas pela tabela de unidades"},
      {map_overlay::kFeatureLevelLinks, "destino das saidas e salas de warp (Vis/Warp)"},
      {map_overlay::kFeatureClip, "recorte do automap"},
  };
  for (const auto& f : features) {
    if ((g_features & f.bit) == 0) Logf(AEONIAN_LOG_WARN, "recurso desligado: %s", f.what);
  }
  if ((g_features & map_overlay::kFeatureMonsters) == 0) {
    g_cfg.scan.monsters = false;
    g_cfg.scan.npcs = false;
  }
}

void InstallDrawHook() {
  if (!g_cfg.draw_hook) return;
  const uintptr_t target = g_host->resolve_symbol(g_host, "D2Client.DrawAutomap");
  if (target == 0) return;  // ja registrado como nao resolvido
  // Os bytes ORIGINAIS (outro mod pode ja ter desviado a funcao; o motor
  // encadeia os hooks) ja foram conferidos contra o INI por resolve_symbol; a
  // 1a instrucao precisa ser o cmp [abs32], imm8 (83 3D) que a spec descreve.
  uint8_t expected[kDrawHookLength];
  const map_overlay::OriginalMemory original(g_host, g_mem);
  if (!original.Read(static_cast<uint32_t>(target), expected, sizeof(expected)) ||
      expected[0] != 0x83 || expected[1] != 0x3D) {
    Logf(AEONIAN_LOG_WARN, "gancho de DrawAutomap: 1a instrucao inesperada; sem gancho");
    return;
  }
  void* trampoline = nullptr;
  const int rc = g_host->hook_inline(g_host, target, expected, kDrawHookLength,
                                     reinterpret_cast<void*>(&AeMapOverlayDrawAutomapDetour),
                                     &trampoline);
  if (rc != AEONIAN_OK || trampoline == nullptr) {
    Logf(AEONIAN_LOG_WARN, "gancho de DrawAutomap recusado (rc=%d); uso so a flag de visivel",
         rc);
    return;
  }
  g_ae_map_draw_trampoline = trampoline;
  g_hooked = true;
  Logf(AEONIAN_LOG_INFO, "gancho de contagem em DrawAutomap (0x%08lx) instalado",
       static_cast<unsigned long>(target));
}

// --- Leitura (thread principal) ----------------------------------------------

Counts CountOf(const map_overlay::ScanStats& s) {
  Counts c;
  c.monsters = s.monsters;
  c.special = s.champions + s.uniques + s.superuniques;
  c.objectives = s.objects + s.npcs + s.superuniques;
  c.exits = s.exits + s.exit_rooms + s.passage_levels;
  return c;
}

bool SameCounts(const map_overlay::ScanStats& a, const map_overlay::ScanStats& b) {
  return a.rooms_loaded == b.rooms_loaded && a.monsters == b.monsters &&
         a.champions == b.champions && a.uniques == b.uniques &&
         a.superuniques == b.superuniques && a.npcs == b.npcs && a.objects == b.objects &&
         a.exits == b.exits && a.exit_rooms == b.exit_rooms && a.passages == b.passages;
}

// Resumo no log: ao trocar de nivel e, depois, quando as contagens mudam
// (no maximo um a cada kSummaryEveryMs).
void LogSummary(const AutomapView& view, DWORD now) {
  const map_overlay::ScanStats& s = g_scan.stats;
  if (g_scan.level_id == g_summary_level &&
      (SameCounts(s, g_summary_stats) || now - g_last_summary_ms < kSummaryEveryMs)) {
    return;
  }
  g_summary_level = g_scan.level_id;
  g_summary_stats = s;
  g_last_summary_ms = now;
  Logf(AEONIAN_LOG_INFO,
       "nivel %d: %u salas (%u carregadas), %u predefinidas, %u monstros (%u campeoes, %u "
       "unicos, %u superunicos), %u NPCs, %u objetos, %u saidas exatas, %u salas de warp, %u "
       "trechos de borda para %u nivel(is); Vis/Warp %s; jogador (%d,%d); varredura media "
       "%.3f ms, maxima %.3f ms",
       g_scan.level_id, s.rooms, s.rooms_loaded, s.presets, s.monsters, s.champions, s.uniques,
       s.superuniques, s.npcs, s.objects, s.exits, s.exit_rooms, s.passages, s.passage_levels,
       g_scan.links_ok ? "lidos" : "indisponiveis", g_scan.player_x, g_scan.player_y,
       g_scans != 0 ? g_scan_ms_total / g_scans : 0.0, g_scan_ms_max);
  for (const Mark& m : g_scan.marks) {
    if (m.kind == MarkKind::kExit || m.kind == MarkKind::kExitRoom ||
        (m.kind == MarkKind::kPassage && m.primary)) {
      const map_overlay::Point p = map_overlay::ClientToAutomap(m.x[0], m.y[0], view);
      Logf(AEONIAN_LOG_INFO, "  saida %s -> nivel %d (lvlwarp %d), automap (%d,%d)",
           m.kind == MarkKind::kExit       ? "exata"
           : m.kind == MarkKind::kExitRoom ? "na sala"
                                           : "pela borda",
           m.dest_level, m.warp_class, p.x, p.y);
    }
  }
}

void Publish(bool active, const AutomapView& view, bool scanned) {
  AcquireSRWLockExclusive(&g_lock);
  g_shared.active = active;
  g_shared.view = view;
  if (scanned) {
    g_shared.level_id = g_scan.level_id;
    g_shared.marks = g_scan.marks;
    g_shared.counts = CountOf(g_scan.stats);
  }
  ReleaseSRWLockExclusive(&g_lock);
}

void OnTick(void*, uint32_t) {
  if (g_main_thread == 0) g_main_thread = GetCurrentThreadId();
  if ((g_features & map_overlay::kFeatureAutomap) == 0) return;
  const DWORD now = GetTickCount();
  g_mem.Reset();
  AutomapView view;
  const bool view_ok = map_overlay::ReadAutomapView(g_mem, g_addr, &view);
  if (g_hooked) {
    const LONG draws = InterlockedCompareExchange(&g_ae_map_automap_draws, 0, 0);
    if (draws != g_tick_draws) {
      g_tick_draws = draws;
      g_last_draw_ms = now;
    }
  }
  const bool drawn = !g_hooked || now - g_last_draw_ms <= kDrawnWindowMs;
  const bool want = g_enabled && view_ok && view.visible && drawn;
  bool scanned = false;
  if (want && (!g_was_active || now - g_last_scan_ms >= g_cfg.refresh_ms)) {
    LARGE_INTEGER freq;
    LARGE_INTEGER t0;
    LARGE_INTEGER t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);
    map_overlay::ScanGame(g_mem, g_addr, g_cfg.scan, &g_scan);
    QueryPerformanceCounter(&t1);
    const double ms = 1000.0 * static_cast<double>(t1.QuadPart - t0.QuadPart) /
                      static_cast<double>(freq.QuadPart);
    ++g_scans;
    g_scan_ms_total += ms;
    if (ms > g_scan_ms_max) g_scan_ms_max = ms;
    g_last_scan_ms = now;
    scanned = true;
    if (g_scan.in_game && !g_in_game_logged) {
      g_in_game_logged = true;
      Logf(AEONIAN_LOG_INFO, "em partida, automap aberto");
    }
    if (g_scan.in_game && !g_logged_view) {
      g_logged_view = true;
      Logf(AEONIAN_LOG_INFO, "automap: modo %d, divisor %d, deslocamento (%d,%d), recorte %s"
           " (%d,%d,%d,%d), gancho %s", view.mode, view.divisor, view.offset_x, view.offset_y,
           view.has_clip ? "lido" : "ausente", view.clip_left, view.clip_top, view.clip_right,
           view.clip_bottom, g_hooked ? "ativo" : "inativo");
    }
    if (g_scan.in_game) LogSummary(view, now);
  }
  const bool active = want && g_scan.in_game;
  if (g_was_active && !active && !g_scan.in_game) g_in_game_logged = false;
  if (view_ok && view.mode != g_shared.view.mode) g_logged_view = false;  // modo trocado
  g_was_active = active;
  Publish(active, view, scanned);
}

// --- Desenho -----------------------------------------------------------------

const COLORREF kMonsterColor = RGB(235, 45, 35);
const COLORREF kChampionColor = RGB(80, 150, 255);
const COLORREF kUniqueColor = RGB(235, 195, 60);
const COLORREF kSuperUniqueColor = RGB(255, 125, 20);
const COLORREF kNpcColor = RGB(110, 235, 110);
const COLORREF kObjectColor = RGB(70, 225, 245);
const COLORREF kExitColor = RGB(255, 225, 0);
const COLORREF kOutline = RGB(8, 8, 8);
const COLORREF kStatusColor = RGB(230, 220, 190);

struct Painter {
  HDC dc;
  map_overlay::CanvasMap map;
  double unit;  // escala dos tamanhos (1 no GDI)

  int X(int32_t x) const { return static_cast<int>(std::lround(x * map.scale_x + map.offset_x)); }
  int Y(int32_t y) const { return static_cast<int>(std::lround(y * map.scale_y + map.offset_y)); }
  int S(int v) const {
    const long s = std::lround(v * unit);
    return s < 1 ? 1 : static_cast<int>(s);
  }

  void Box(int x, int y, int half, COLORREF color) const {
    HBRUSH outline = CreateSolidBrush(kOutline);
    HBRUSH fill = CreateSolidBrush(color);
    RECT o = {x - half - 1, y - half - 1, x + half + 2, y + half + 2};
    RECT r = {x - half, y - half, x + half + 1, y + half + 1};
    FillRect(dc, &o, outline);
    FillRect(dc, &r, fill);
    DeleteObject(outline);
    DeleteObject(fill);
  }
  void Diamond(int x, int y, int half, COLORREF color) const {
    HPEN pen = CreatePen(PS_SOLID, 1, kOutline);
    HBRUSH fill = CreateSolidBrush(color);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, fill);
    POINT pts[4] = {{x, y - half}, {x + half, y}, {x, y + half}, {x - half, y}};
    Polygon(dc, pts, 4);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(pen);
    DeleteObject(fill);
  }
  void Ring(int x, int y, int radius, COLORREF color) const {
    HPEN pen = CreatePen(PS_SOLID, S(2), color);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Ellipse(dc, x - radius, y - radius, x + radius + 1, y + radius + 1);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(pen);
  }
  void Frame(int x, int y, int half, COLORREF color) const {
    HPEN pen = CreatePen(PS_SOLID, S(2), color);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, x - half, y - half, x + half + 1, y + half + 1);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(pen);
  }
  void Line(int x1, int y1, int x2, int y2, int width, COLORREF color) const {
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HGDIOBJ old = SelectObject(dc, pen);
    MoveToEx(dc, x1, y1, nullptr);
    LineTo(dc, x2, y2);
    SelectObject(dc, old);
    DeleteObject(pen);
  }
  void Room(const POINT (&pts)[4], COLORREF color) const {
    HPEN pen = CreatePen(PS_SOLID, S(2), color);
    HBRUSH hatch = CreateHatchBrush(HS_BDIAGONAL, color);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, hatch);
    Polygon(dc, pts, 4);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(pen);
    DeleteObject(hatch);
  }
  // Texto com contorno escuro (legivel sobre o automap).
  void Text(int x, int y, const std::wstring& text, COLORREF color) const {
    const int n = static_cast<int>(text.size());
    SetTextColor(dc, kOutline);
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        if (dx != 0 || dy != 0) TextOutW(dc, x + dx, y + dy, text.c_str(), n);
      }
    }
    SetTextColor(dc, color);
    TextOutW(dc, x, y, text.c_str(), n);
  }
};

std::wstring LevelName(int32_t id) {
  const auto it = g_cfg.level_names.find(id);
  if (it != g_cfg.level_names.end() && !it->second.empty()) return it->second;
  if (id <= 0) return L"?";
  wchar_t buf[32];
  std::swprintf(buf, 32, L"n\u00edvel %d", id);
  return buf;
}

HFONT g_font = nullptr;
int g_font_height = 0;

HFONT FontFor(int height) {
  if (g_font != nullptr && g_font_height == height) return g_font;
  if (g_font != nullptr) DeleteObject(g_font);
  // NONANTIALIASED_QUALITY: sem bordas misturadas com a cor-chave do canvas.
  g_font = CreateFontW(height, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
                       DEFAULT_PITCH | FF_SWISS, L"Arial");
  g_font_height = height;
  return g_font;
}

void DrawShape(const Painter& p, const ScreenMark& m) {
  const int x = p.X(m.x[0]);
  const int y = p.Y(m.y[0]);
  switch (m.kind) {
    case MarkKind::kMonster:
      p.Box(x, y, p.S(2), kMonsterColor);
      break;
    case MarkKind::kChampion:
      p.Box(x, y, p.S(3), kChampionColor);
      break;
    case MarkKind::kUnique:
      p.Diamond(x, y, p.S(5), kUniqueColor);
      break;
    case MarkKind::kSuperUnique:
      p.Diamond(x, y, p.S(7), kSuperUniqueColor);
      p.Ring(x, y, p.S(9), kSuperUniqueColor);
      break;
    case MarkKind::kNpc:
      p.Ring(x, y, p.S(6), kNpcColor);
      break;
    case MarkKind::kObject:
      p.Frame(x, y, p.S(6), kObjectColor);
      break;
    case MarkKind::kExit:
      p.Box(x, y, p.S(4), kExitColor);
      break;
    case MarkKind::kExitRoom: {
      POINT pts[4];
      for (int i = 0; i < 4; ++i) {
        pts[i].x = p.X(m.x[i]);
        pts[i].y = p.Y(m.y[i]);
      }
      p.Room(pts, kExitColor);
      break;
    }
    case MarkKind::kPassage: {
      const int x2 = p.X(m.x[1]);
      const int y2 = p.Y(m.y[1]);
      p.Line(x, y, x2, y2, p.S(4), kOutline);
      p.Line(x, y, x2, y2, p.S(2), kExitColor);
      break;
    }
  }
}

// Rotulo de uma marca (saidas e objetos); false se ela nao tem rotulo. O
// ponto (x, y) e o canto superior esquerdo do texto.
bool LabelOf(const Painter& p, const ScreenMark& m, std::wstring* text, int* x, int* y,
             COLORREF* color) {
  const std::wstring arrow = L"\u00bb ";
  *color = kExitColor;
  switch (m.kind) {
    case MarkKind::kObject:
      if (m.label < 0 || static_cast<size_t>(m.label) >= g_cfg.object_names.size()) return false;
      *text = g_cfg.object_names[static_cast<size_t>(m.label)];
      *color = kObjectColor;
      *x = p.X(m.x[0]) + p.S(9);
      *y = p.Y(m.y[0]) - p.S(7);
      return true;
    case MarkKind::kExit:
      *text = arrow + LevelName(m.dest_level);
      *x = p.X(m.x[0]) + p.S(8);
      *y = p.Y(m.y[0]) - p.S(7);
      return true;
    case MarkKind::kExitRoom:
      *text = arrow + LevelName(m.dest_level) + L" (?)";
      *x = (p.X(m.x[0]) + p.X(m.x[1]) + p.X(m.x[2]) + p.X(m.x[3])) / 4 - p.S(10);
      *y = (p.Y(m.y[0]) + p.Y(m.y[1]) + p.Y(m.y[2]) + p.Y(m.y[3])) / 4 - p.S(7);
      return true;
    case MarkKind::kPassage:
      if (!m.primary) return false;
      *text = arrow + LevelName(m.dest_level);
      *x = (p.X(m.x[0]) + p.X(m.x[1])) / 2 + p.S(6);
      *y = (p.Y(m.y[0]) + p.Y(m.y[1])) / 2 - p.S(7);
      return true;
    default:
      return false;
  }
}

// Rotulos fora do recorte (um nome nao pode ser cortado na borda do
// minimapa): so de marcas cujo ponto esta no recorte, empurrados para dentro
// dele quando passariam da borda direita.
void DrawLabels(const Painter& p, const std::vector<ScreenMark>& screen, const AutomapView& view) {
  const int left = view.has_clip ? p.X(view.clip_left) : 0;
  const int right = view.has_clip ? p.X(view.clip_right) : 1 << 20;
  const int top = view.has_clip ? p.Y(view.clip_top) : 0;
  const int bottom = view.has_clip ? p.Y(view.clip_bottom) : 1 << 20;
  for (const ScreenMark& m : screen) {
    std::wstring text;
    int x = 0;
    int y = 0;
    COLORREF color = kExitColor;
    if (!LabelOf(p, m, &text, &x, &y, &color)) continue;
    const int anchor_y = y + p.S(7);
    if (x < left || anchor_y < top || anchor_y > bottom) continue;
    SIZE size = {0, 0};
    GetTextExtentPoint32W(p.dc, text.c_str(), static_cast<int>(text.size()), &size);
    if (x > right) continue;
    if (x + size.cx > right) {
      x = right - static_cast<int>(size.cx);
      if (x < left) x = left;
    }
    p.Text(x, y, text, color);
  }
}

void DrawStatus(const Painter& p, int canvas_w, const Counts& c, int32_t level_id) {
  wchar_t buf[160];
  std::swprintf(buf, 160,
                L"Aeonian mapa \u00b7 n\u00edvel %d \u00b7 %u monstros (%u especiais) \u00b7 %u "
                L"objetivos \u00b7 %u sa\u00eddas",
                level_id, c.monsters, c.special, c.objectives, c.exits);
  SetTextAlign(p.dc, TA_CENTER | TA_TOP);
  p.Text(canvas_w / 2, p.S(2), buf, kStatusColor);
  SetTextAlign(p.dc, TA_LEFT | TA_TOP);
}

void OnPresent(void*, AeonianCanvas* canvas) {
  // Lista e estado reaproveitados entre quadros (on_present nunca roda em
  // duas threads ao mesmo tempo).
  static std::vector<Mark> marks;
  static std::vector<ScreenMark> screen;
  AutomapView view;
  Counts counts;
  int32_t level_id = 0;
  AcquireSRWLockShared(&g_lock);
  const bool active = g_shared.active;
  if (active) {
    view = g_shared.view;
    marks = g_shared.marks;
    counts = g_shared.counts;
    level_id = g_shared.level_id;
  }
  ReleaseSRWLockShared(&g_lock);
  if (!active) return;

  const bool on_main = GetCurrentThreadId() == g_main_thread;
  map_overlay::CanvasMap map;
  if (on_main) {
    // GDI/DirectDraw: o quadro deste on_present e o que o jogo acabou de
    // desenhar, na mesma thread. Desenha so se o automap entrou nele e com
    // o deslocamento deste quadro (o do tick pode estar um quadro atrasado).
    if (g_hooked) {
      const LONG draws = InterlockedCompareExchange(&g_ae_map_automap_draws, 0, 0);
      const bool drawn = draws != g_present_draws;
      g_present_draws = draws;
      if (!drawn) return;
    }
    g_mem.Reset();
    AutomapView fresh;
    if (map_overlay::ReadAutomapView(g_mem, g_addr, &fresh)) view = fresh;
    if (!view.visible) return;
  } else {
    // D2GL: o canvas tem o tamanho da area cliente e o quadro do jogo e
    // encaixado nela pelo D2GL. TODO(RE): a resolucao do quadro (so 800x600
    // observada, spec de niveis sec. 5.2) vem de [d2gl] na configuracao; o
    // encaixe, do d2gl.ini (modelo observado, ver FitFrameD2gl).
    if (g_cfg.d2gl_ini) {
      const bool full = g_cfg.d2gl_fullscreen;
      map = map_overlay::FitFrameD2gl(g_cfg.game_width, g_cfg.game_height, canvas->width,
                                      canvas->height, full ? canvas->width : g_cfg.d2gl_window_w,
                                      full ? canvas->height : g_cfg.d2gl_window_h,
                                      g_cfg.d2gl_stretch_h, g_cfg.d2gl_stretch_v);
    } else {
      map = map_overlay::FitFrame(g_cfg.game_width, g_cfg.game_height, canvas->width,
                                  canvas->height, g_cfg.keep_aspect);
    }
  }
  map_overlay::ProjectMarks(marks, view, &screen);

  Painter p;
  p.dc = static_cast<HDC>(canvas->hdc);
  p.map = map;
  p.unit = map.scale_x < map.scale_y ? map.scale_x : map.scale_y;
  if (p.unit < 1.0) p.unit = 1.0;
  canvas->opacity = 255;
  SetBkMode(p.dc, TRANSPARENT);
  HGDIOBJ old_font = SelectObject(p.dc, FontFor(p.S(12)));
  const int saved = SaveDC(p.dc);
  if (view.has_clip) {
    IntersectClipRect(p.dc, p.X(view.clip_left), p.Y(view.clip_top), p.X(view.clip_right) + 1,
                      p.Y(view.clip_bottom) + 1);
  }
  // Ordem: bordas e salas embaixo, depois saidas, objetivos e monstros.
  for (const ScreenMark& m : screen) {
    if (m.kind == MarkKind::kPassage || m.kind == MarkKind::kExitRoom) DrawShape(p, m);
  }
  for (const ScreenMark& m : screen) {
    if (m.kind == MarkKind::kExit || m.kind == MarkKind::kObject || m.kind == MarkKind::kNpc) {
      DrawShape(p, m);
    }
  }
  for (const ScreenMark& m : screen) {
    if (m.kind == MarkKind::kMonster) DrawShape(p, m);
  }
  for (const ScreenMark& m : screen) {
    if (m.kind == MarkKind::kChampion || m.kind == MarkKind::kUnique ||
        m.kind == MarkKind::kSuperUnique) {
      DrawShape(p, m);
    }
  }
  RestoreDC(p.dc, saved);
  if (g_cfg.labels) DrawLabels(p, screen, view);
  // Evidencia no log (a cada 5 s, ou ao trocar de modo): o que foi desenhado.
  static DWORD last_log_ms = 0;
  static int32_t last_log_mode = -1;
  const DWORD now = GetTickCount();
  if (view.mode != last_log_mode || now - last_log_ms >= 5000) {
    last_log_ms = now;
    last_log_mode = view.mode;
    Logf(AEONIAN_LOG_DEBUG, "quadro %u: %u marcas desenhadas (modo %d, canvas %dx%d, %s)",
         canvas->frame, static_cast<unsigned>(screen.size()), view.mode, canvas->width,
         canvas->height, on_main ? "thread principal" : "thread do D2GL");
  }
  if (g_cfg.status) DrawStatus(p, canvas->width, counts, level_id);
  SelectObject(p.dc, old_font);
}

int OnWindowMessage(void*, AeonianWindowMessage* msg) {
  if (msg->message != WM_KEYDOWN || msg->wparam != g_cfg.toggle_vk) return 0;
  if ((msg->lparam & (1 << 30)) != 0) return 0;  // repeticao
  const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
  const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
  if (ctrl != g_cfg.toggle_ctrl || shift != g_cfg.toggle_shift) return 0;
  g_enabled = !g_enabled;
  Logf(AEONIAN_LOG_INFO, "marcas %s", g_enabled ? "ligadas" : "desligadas");
  msg->result = 0;
  return 1;  // a tecla e do mod: o jogo nao a ve
}

void OnShutdown(void*) {
  Logf(AEONIAN_LOG_INFO,
       "%u varreduras, media %.3f ms, maxima %.3f ms; DrawAutomap chamado %ld vez(es)",
       g_scans, g_scans != 0 ? g_scan_ms_total / g_scans : 0.0, g_scan_ms_max,
       static_cast<long>(g_ae_map_automap_draws));
}

}  // namespace

extern "C" AEONIAN_PLUGIN_EXPORT int AeonianPluginInit(const AeonianHost* host) {
  g_host = host;
  if (host->game_build != AEONIAN_BUILD_D2LOD_1_14D) {
    Logf(AEONIAN_LOG_WARN, "build %u nao suportada; mod inativo", host->game_build);
    return 1;
  }
  LoadConfig();
  g_enabled = g_cfg.enabled;
  ResolveSymbols();
  if ((g_features & map_overlay::kFeatureAutomap) == 0) {
    Logf(AEONIAN_LOG_WARN, "sem os simbolos basicos do automap; mod inativo");
    return 1;
  }
  InstallDrawHook();
  AeonianEvents events = {};
  events.struct_size = sizeof(events);
  events.on_tick = OnTick;
  events.on_present = OnPresent;
  events.on_window_message = OnWindowMessage;
  events.on_shutdown = OnShutdown;
  return host->register_events(host, &events);
}
