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

// Plugin CONDUTOR de teste (so para tests/integration; nao e um exemplo de
// mod). Executa um roteiro (mapdrive.txt na pasta do mod) que leva o jogo do
// titulo a uma partida e grava quadros com request_screenshot, para validar o
// mod map-overlay no jogo real. Os cliques e teclas vao so para a janela do
// jogo (PostMessage); o cursor so e movido dentro dela (que roda num
// gamescope invisivel nos testes). Escreve linhas "MAPDRIVE ..." no log.
//
// Com AEONIAN_MAPDRIVE_HOOK_DRAW=1 (e prioridade maior que a do map-overlay)
// ele tambem desvia D2Client.DrawAutomap ao carregar, so contando as chamadas:
// e o caso de um mod encontrar a funcao ja desviada por outro.
//
// Comandos do roteiro (um por linha; ';' comeca comentario):
//   wait <ms>                  espera
//   click <x> <y>              clique esquerdo em coordenadas do quadro 800x600
//   rclick <x> <y>             clique direito (a habilidade do botao direito)
//   move <x> <y>               so move o cursor
//   closepanels                Esc ate nao restar painel lateral
//   key <tecla> [vezes]        tab, esc, enter, space, up, down, left, right, f1..f12, letra
//   type <texto>               WM_CHAR para cada caractere
//   shot <nome>                pede <pasta do mod>\<nome>.bmp (gravado no proximo
//                              present: espere antes de mudar o que aparece nele)
//   waitgame <ms>              espera o jogador e o ato do cliente (partida)
//   automap <0|1> <ms>         espera o automap visivel no modo dado
//   ensuremode <0|1>           troca o tamanho do automap pelo menu (Esc ->
//                              Options -> Automap Options -> Automap Size) se
//                              o modo atual for outro (a opcao persiste)
//   walk <nivel> <ms>          anda ate entrar no nivel (pela borda/saida que leva a ele)
//   walkin <subtiles> <ms>     anda para longe da borda por onde entrou
//   walkexit <nivel> <ms>      anda ate ver a posicao exata da saida para o nivel
//   walkmode <left|right>      botao dos passos de walk fora da cidade (right:
//                              Teletransporte; na cidade o jogo nao o permite)
//   walkpoint <dx> <dy> <ms>   anda ate (jogador inicial + dx, dy) em subtiles
//   waitlevel <nivel> <ms>     espera o jogador estar no nivel
//   waitui <indice> <ms>       espera a entrada de estado de UI ligar
//   clickmark <tipo> <ui|-1> <ms>  anda ate a marca mais perto e clica nela ate a
//                              entrada de UI ligar (-1: ate o nivel mudar). Tipos:
//                              npc, wp, blue, red, exit
//   hover <tipo>               poe o cursor sobre a marca mais perto (o jogo mostra
//                              o rotulo no proximo quadro). Tipos acima e monster,
//                              minion, champion, unique, super, ally, other
//   goto <tipo> <ms>           anda ate a marca mais perto ficar na tela
//   sweep <tipo|none> <ms>     percorre o retangulo do nivel ate ver uma marca do tipo
//   report <etiqueta>          linha com todas as contagens da varredura
//   log <texto>

#include <windows.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <string>
#include <vector>

#include "aeonian/api.h"
#include "game_memory.h"
#include "map_logic.h"

// Desvio de contagem: incrementa e segue pelo trampolim, sem tocar em pilha nem
// registradores (a 1a instrucao da original e um cmp, que refaz as flags).
extern "C" {
volatile LONG g_ae_drive_draws = 0;
void* g_ae_drive_trampoline = nullptr;
void AeMapDriveDrawDetour();
}
__asm__(
    ".text\n"
    ".globl _AeMapDriveDrawDetour\n"
    "_AeMapDriveDrawDetour:\n"
    "  lock incl _g_ae_drive_draws\n"
    "  jmp *_g_ae_drive_trampoline\n");

namespace {

const AeonianHost* g_host = nullptr;
map_overlay::GameMemory g_mem;
map_overlay::GameAddresses g_addr;
std::vector<std::string> g_script;
size_t g_pc = 0;
DWORD g_wait_until = 0;
DWORD g_deadline = 0;
bool g_started = false;
bool g_failed = false;

// Estado do comando walk/walkin.
DWORD g_walk_next = 0;
DWORD g_walk_moved_ms = 0;
int32_t g_walk_last_x = -1;
int32_t g_walk_last_y = -1;
int g_walk_detour = 0;
DWORD g_walk_detour_until = 0;
int32_t g_entry_x = 0;
int32_t g_entry_y = 0;
bool g_walk_right = false;
int32_t g_click_level = 0;

// Estado do comando sweep: pontos do retangulo do nivel ainda nao visitados.
struct SweepPoint {
  int32_t x;
  int32_t y;
};
std::vector<SweepPoint> g_sweep;
bool g_sweep_active = false;
DWORD g_sweep_since = 0;

void Say(AeonianLogLevel level, const char* format, ...) __attribute__((format(printf, 2, 3)));
void Say(AeonianLogLevel level, const char* format, ...) {
  char buf[512];
  va_list args;
  va_start(args, format);
  std::vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  g_host->log(g_host, level, buf);
}

HWND GameWindow() { return FindWindowA("Diablo II", nullptr); }

// Coordenadas do quadro 800x600 -> cliente da janela (no D2GL a janela pode
// ser maior; o quadro e encaixado mantendo a proporcao).
POINT ToWindow(HWND hwnd, int x, int y) {
  RECT rc;
  POINT p = {x, y};
  if (!GetClientRect(hwnd, &rc) || rc.right <= 0 || rc.bottom <= 0) return p;
  const map_overlay::CanvasMap map =
      map_overlay::FitFrame(800, 600, static_cast<int32_t>(rc.right),
                            static_cast<int32_t>(rc.bottom), true);
  p.x = static_cast<LONG>(std::lround(x * map.scale_x + map.offset_x));
  p.y = static_cast<LONG>(std::lround(y * map.scale_y + map.offset_y));
  return p;
}

void MoveCursor(HWND hwnd, int x, int y) {
  POINT p = {x, y};
  ClientToScreen(hwnd, &p);
  SetCursorPos(p.x, p.y);
  PostMessageA(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(x, y));
}

void Click(int x, int y) {
  HWND hwnd = GameWindow();
  if (hwnd == nullptr) return;
  const POINT w = ToWindow(hwnd, x, y);
  MoveCursor(hwnd, static_cast<int>(w.x), static_cast<int>(w.y));
  PostMessageA(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(w.x, w.y));
  PostMessageA(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(w.x, w.y));
}

void RightClick(int x, int y) {
  HWND hwnd = GameWindow();
  if (hwnd == nullptr) return;
  const POINT w = ToWindow(hwnd, x, y);
  MoveCursor(hwnd, static_cast<int>(w.x), static_cast<int>(w.y));
  PostMessageA(hwnd, WM_RBUTTONDOWN, MK_RBUTTON, MAKELPARAM(w.x, w.y));
  PostMessageA(hwnd, WM_RBUTTONUP, 0, MAKELPARAM(w.x, w.y));
}

UINT KeyCode(const std::string& name) {
  struct {
    const char* name;
    UINT vk;
  } const keys[] = {{"tab", VK_TAB},     {"esc", VK_ESCAPE}, {"enter", VK_RETURN},
                    {"space", VK_SPACE}, {"up", VK_UP},      {"down", VK_DOWN},
                    {"left", VK_LEFT},   {"right", VK_RIGHT}};
  for (const auto& k : keys) {
    if (name == k.name) return k.vk;
  }
  if (name.size() >= 2 && name[0] == 'f') {
    const int n = std::atoi(name.c_str() + 1);
    if (n >= 1 && n <= 12) return static_cast<UINT>(VK_F1 + n - 1);
  }
  if (name.size() == 1 && name[0] >= 'a' && name[0] <= 'z') {
    return static_cast<UINT>(name[0] - 'a' + 'A');
  }
  return 0;
}

void Key(UINT vk) {
  HWND hwnd = GameWindow();
  if (hwnd == nullptr || vk == 0) return;
  const UINT scan = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
  PostMessageA(hwnd, WM_KEYDOWN, vk, static_cast<LPARAM>(1 | (scan << 16)));
  PostMessageA(hwnd, WM_KEYUP, vk, static_cast<LPARAM>(1 | (scan << 16) | (3u << 30)));
}

bool LoadScript() {
  const std::string path = std::string(g_host->mod_dir) + "\\mapdrive.txt";
  FILE* f = std::fopen(path.c_str(), "rb");
  if (f == nullptr) return false;
  char line[512];
  while (std::fgets(line, sizeof(line), f) != nullptr) {
    std::string s(line);
    const size_t semi = s.find(';');
    if (semi != std::string::npos) s.erase(semi);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
    size_t start = s.find_first_not_of(" \t");
    if (start == std::string::npos) continue;
    g_script.push_back(s.substr(start));
  }
  std::fclose(f);
  return true;
}

uint32_t Resolve(void*, const char* name) {
  return static_cast<uint32_t>(g_host->resolve_symbol(g_host, name));
}

bool InGame() {
  uint32_t player = 0;
  uint32_t act = 0;
  if (!map_overlay::ReadU32(g_mem, g_addr.player_unit, &player) || player == 0) return false;
  return g_addr.current_act == 0 ||
         (map_overlay::ReadU32(g_mem, g_addr.current_act, &act) && act != 0);
}

map_overlay::Scan ScanNow() {
  map_overlay::ScanOptions options;
  map_overlay::Scan scan;
  map_overlay::ScanGame(g_mem, g_addr, options, &scan);
  return scan;
}

// Clica na tela na direcao (dx, dy) em pixels de cliente a partir do jogador,
// girada `turn` graus (para contornar obstaculos).
void ClickToward(double dx, double dy, double turn, bool right) {
  const double len = std::sqrt(dx * dx + dy * dy);
  if (len < 1.0) return;
  const double a = std::atan2(dy, dx) + turn * 3.14159265358979 / 180.0;
  const double r = len < 180.0 ? len : 180.0;
  const int x = 400 + static_cast<int>(std::lround(std::cos(a) * r));
  const int y = 285 + static_cast<int>(std::lround(std::sin(a) * r));
  const int cx = x < 20 ? 20 : x > 780 ? 780 : x;
  const int cy = y < 40 ? 40 : y > 520 ? 520 : y;
  if (right) {
    RightClick(cx, cy);
  } else {
    Click(cx, cy);
  }
}

// Um passo do walk. Parado por mais de 1,2 s: anda perpendicular ao alvo
// (ao longo do obstaculo) por um tempo que cresce a cada tentativa,
// alternando os lados; depois volta a mirar o alvo.
void WalkStep(const map_overlay::Scan& scan, double dx, double dy, DWORD now) {
  if (std::abs(scan.player_x - g_walk_last_x) + std::abs(scan.player_y - g_walk_last_y) > 1) {
    g_walk_last_x = scan.player_x;
    g_walk_last_y = scan.player_y;
    g_walk_moved_ms = now;
  }
  if (now < g_walk_next) return;
  g_walk_next = now + 300;
  if (now - g_walk_moved_ms > 1200) {
    ++g_walk_detour;
    const DWORD length = 2500u + 1500u * static_cast<DWORD>((g_walk_detour - 1) / 2);
    g_walk_detour_until = now + length;
    g_walk_moved_ms = now;
    Say(AEONIAN_LOG_INFO, "MAPDRIVE desvio %d (%s, %lu ms) em (%d,%d)", g_walk_detour,
        g_walk_detour % 2 == 1 ? "esquerda" : "direita", static_cast<unsigned long>(length),
        scan.player_x, scan.player_y);
  }
  double turn = 0.0;
  if (now < g_walk_detour_until) turn = g_walk_detour % 2 == 1 ? -90.0 : 90.0;
  ClickToward(dx, dy, turn, g_walk_right && !map_overlay::IsTownLevel(scan.level_id));
}

// Alvo do walk: o ponto de saida (borda ou warp) para `level` mais perto do
// jogador, em pixels de cliente.
bool WalkTarget(const map_overlay::Scan& scan, int32_t level, double* tx, double* ty) {
  const map_overlay::Point me = map_overlay::SubtileToClient(scan.player_x, scan.player_y);
  bool found = false;
  double best = 0.0;
  for (const map_overlay::Mark& m : scan.marks) {
    if (m.dest_level != level) continue;
    double x = m.x[0];
    double y = m.y[0];
    if (m.kind == map_overlay::MarkKind::kPassage ||
        m.kind == map_overlay::MarkKind::kPassageApprox) {
      // Ponto do segmento mais perto do jogador.
      const double sx = m.x[1] - m.x[0];
      const double sy = m.y[1] - m.y[0];
      const double l2 = sx * sx + sy * sy;
      double t = l2 > 0 ? ((me.x - m.x[0]) * sx + (me.y - m.y[0]) * sy) / l2 : 0.0;
      t = t < 0.1 ? 0.1 : t > 0.9 ? 0.9 : t;
      x = m.x[0] + t * sx;
      y = m.y[0] + t * sy;
    } else if (m.kind == map_overlay::MarkKind::kExitRoom) {
      x = (m.x[0] + m.x[1] + m.x[2] + m.x[3]) / 4.0;
      y = (m.y[0] + m.y[1] + m.y[2] + m.y[3]) / 4.0;
    } else if (m.kind != map_overlay::MarkKind::kExit) {
      continue;
    }
    const double d = (x - me.x) * (x - me.x) + (y - me.y) * (y - me.y);
    if (!found || d < best) {
      found = true;
      best = d;
      *tx = x - me.x;
      *ty = y - me.y;
    }
  }
  return found;
}

bool MarkKindFromName(const std::string& name, map_overlay::MarkKind* kind) {
  using map_overlay::MarkKind;
  struct {
    const char* name;
    MarkKind kind;
  } const kinds[] = {{"npc", MarkKind::kNpc},          {"wp", MarkKind::kWaypoint},
                     {"blue", MarkKind::kPortalBlue},  {"red", MarkKind::kPortalRed},
                     {"exit", MarkKind::kExit},         {"other", MarkKind::kPortalOther},
                     {"monster", MarkKind::kMonster},  {"minion", MarkKind::kMinion},
                     {"champion", MarkKind::kChampion}, {"unique", MarkKind::kUnique},
                     {"super", MarkKind::kSuperUnique}, {"ally", MarkKind::kAlly}};
  for (const auto& k : kinds) {
    if (name == k.name) {
      *kind = k.kind;
      return true;
    }
  }
  return false;
}

// A marca do tipo mais perto do jogador, em pixels de cliente relativos a ele.
const map_overlay::Mark* NearestMark(const map_overlay::Scan& scan, map_overlay::MarkKind kind,
                                     double* dx, double* dy) {
  const map_overlay::Point me = map_overlay::SubtileToClient(scan.player_x, scan.player_y);
  const map_overlay::Mark* best = nullptr;
  double best_d = 0.0;
  for (const map_overlay::Mark& m : scan.marks) {
    if (m.kind != kind) continue;
    const double x = m.x[0] - me.x;
    const double y = m.y[0] - me.y;
    const double d = x * x + y * y;
    if (best == nullptr || d < best_d) {
      best = &m;
      best_d = d;
      *dx = x;
      *dy = y;
    }
  }
  return best;
}

bool OnScreen(double dx, double dy) { return std::abs(dx) < 370.0 && std::abs(dy) < 240.0; }

bool UiOn(int index) {
  uint32_t value = 0;
  return g_addr.ui_state != 0 && index >= 0 && index < 0x26 &&
         map_overlay::ReadU32(g_mem, g_addr.ui_state + static_cast<uint32_t>(index) * 4, &value) &&
         value != 0;
}

void Report(const char* tag) {
  const map_overlay::Scan scan = ScanNow();
  const map_overlay::ScanStats& s = scan.stats;
  map_overlay::AutomapView view;
  map_overlay::ReadAutomapView(g_mem, g_addr, &view);
  Say(AEONIAN_LOG_INFO,
      "MAPDRIVE report %s: ato=%d nivel=%d salas=%u/%u monstros=%u lacaios=%u campeoes=%u "
      "unicos=%u superunicos=%u aliados=%u/%u npcs=%u wp=%u wp_sala=%u azul=%u vermelho=%u "
      "outros=%u objetos=%u saidas=%u salas_warp=%u bordas=%u aprox=%u lado=%d quadro=%dx%d",
      tag, scan.act_index + 1, scan.level_id, s.rooms_loaded, s.rooms, s.monsters, s.minions,
      s.champions, s.uniques, s.superuniques, s.own_allies, s.allies, s.npcs, s.waypoints,
      s.waypoint_rooms, s.portals_blue, s.portals_red, s.portals_other, s.objects, s.exits,
      s.exit_rooms, s.passages, s.passages_approx, view.panel_side, view.frame_width,
      view.frame_height);
  for (const map_overlay::Mark& m : scan.marks) {
    using map_overlay::MarkKind;
    // Monstro hostil numa cidade e raro: a classe ajuda a conferir o filtro.
    const bool town_monster = map_overlay::IsTownLevel(scan.level_id) &&
                              (m.kind == MarkKind::kMonster || m.kind == MarkKind::kMinion);
    const char* what = town_monster                       ? "monstro na cidade"
                       : m.kind == MarkKind::kChampion    ? "campeao"
                       : m.kind == MarkKind::kUnique      ? "unico"
                       : m.kind == MarkKind::kSuperUnique ? "superunico"
                       : m.kind == MarkKind::kAlly        ? "aliado"
                       : m.kind == MarkKind::kWaypoint    ? "waypoint"
                       : m.kind == MarkKind::kPortalBlue  ? "portal azul"
                       : m.kind == MarkKind::kPortalRed   ? "portal vermelho"
                       : m.kind == MarkKind::kPortalOther ? "portal outro"
                       : m.kind == MarkKind::kExit        ? "saida exata"
                       : m.kind == MarkKind::kExitRoom    ? "sala de warp"
                                                          : nullptr;
    if (what != nullptr) {
      Say(AEONIAN_LOG_INFO, "MAPDRIVE   %s classe=%d destino=%d lvlwarp=%d", what, m.unit_class,
          m.dest_level, m.warp_class);
    }
  }
}

void Next() {
  ++g_pc;
  g_deadline = 0;
  g_walk_next = 0;
  g_walk_detour_until = 0;
  g_walk_detour = 0;
  g_walk_last_x = -1;
  g_sweep_active = false;
}

// Executa o comando atual; devolve false enquanto ele ainda nao terminou.
bool Step(DWORD now) {
  const std::string& line = g_script[g_pc];
  char cmd[32] = {};
  char arg[400] = {};
  std::sscanf(line.c_str(), "%31s %399[^\n]", cmd, arg);
  const std::string c(cmd);
  if (c == "wait") {
    if (g_deadline == 0) g_deadline = now + static_cast<DWORD>(std::atol(arg));
    return now >= g_deadline;
  }
  if (c == "click") {
    int x = 0;
    int y = 0;
    std::sscanf(arg, "%d %d", &x, &y);
    Click(x, y);
    return true;
  }
  if (c == "rclick") {
    int x = 0;
    int y = 0;
    std::sscanf(arg, "%d %d", &x, &y);
    RightClick(x, y);
    return true;
  }
  if (c == "move") {
    int x = 0;
    int y = 0;
    std::sscanf(arg, "%d %d", &x, &y);
    HWND hwnd = GameWindow();
    if (hwnd != nullptr) {
      const POINT w = ToWindow(hwnd, x, y);
      MoveCursor(hwnd, static_cast<int>(w.x), static_cast<int>(w.y));
    }
    return true;
  }
  if (c == "closepanels") {
    if (g_deadline == 0) g_deadline = now + 3000;
    map_overlay::AutomapView view;
    if (!map_overlay::ReadAutomapView(g_mem, g_addr, &view) || view.panel_side == 0 ||
        now >= g_deadline) {
      return true;
    }
    if (now >= g_walk_next) {
      g_walk_next = now + 500;
      Key(VK_ESCAPE);
    }
    return false;
  }
  if (c == "walkmode") {
    g_walk_right = std::strcmp(arg, "right") == 0;
    return true;
  }
  if (c == "report") {
    Report(arg);
    return true;
  }
  if (c == "hover") {
    map_overlay::MarkKind kind = map_overlay::MarkKind::kMonster;
    const map_overlay::Scan scan = ScanNow();
    double dx = 0.0;
    double dy = 0.0;
    HWND hwnd = GameWindow();
    const map_overlay::Mark* mark =
        MarkKindFromName(arg, &kind) ? NearestMark(scan, kind, &dx, &dy) : nullptr;
    if (mark == nullptr || hwnd == nullptr || !OnScreen(dx, dy)) {
      Say(AEONIAN_LOG_INFO, "MAPDRIVE hover %s: nenhuma marca na tela", arg);
      return true;
    }
    // A posicao e a dos pes; o corpo do monstro fica acima dela.
    const int lift = kind == map_overlay::MarkKind::kPortalRed ||
                             kind == map_overlay::MarkKind::kPortalBlue ||
                             kind == map_overlay::MarkKind::kPortalOther
                         ? 0
                         : 24;
    const POINT w =
        ToWindow(hwnd, 400 + static_cast<int>(dx), 285 + static_cast<int>(dy) - lift);
    MoveCursor(hwnd, static_cast<int>(w.x), static_cast<int>(w.y));
    Say(AEONIAN_LOG_INFO, "MAPDRIVE hover %s: classe %d em (%+.0f,%+.0f)", arg, mark->unit_class,
        dx, dy);
    return true;
  }
  if (c == "key") {
    char name[32] = {};
    int times = 1;
    std::sscanf(arg, "%31s %d", name, &times);
    for (int i = 0; i < times; ++i) Key(KeyCode(name));
    return true;
  }
  if (c == "type") {
    HWND hwnd = GameWindow();
    for (const char* p = arg; hwnd != nullptr && *p != '\0'; ++p) {
      PostMessageA(hwnd, WM_CHAR, static_cast<WPARAM>(static_cast<unsigned char>(*p)), 1);
    }
    return true;
  }
  if (c == "shot") {
    const std::string path = std::string(g_host->mod_dir) + "\\" + arg + ".bmp";
    const int rc = g_host->request_screenshot(g_host, path.c_str());
    Say(AEONIAN_LOG_INFO, "MAPDRIVE shot %s rc=%d", arg, rc);
    return true;
  }
  if (c == "log") {
    Say(AEONIAN_LOG_INFO, "MAPDRIVE %s", arg);
    return true;
  }
  const long ms = c == "waitgame" ? std::atol(arg) : 0;
  if (c == "waitgame") {
    if (g_deadline == 0) g_deadline = now + static_cast<DWORD>(ms);
    if (InGame()) {
      Say(AEONIAN_LOG_INFO, "MAPDRIVE em partida");
      return true;
    }
    if (now >= g_deadline) {
      Say(AEONIAN_LOG_ERROR, "MAPDRIVE FAIL a partida nao comecou");
      g_failed = true;
    }
    return now >= g_deadline;
  }
  if (c == "ensuremode") {
    const int mode = std::atoi(arg);
    map_overlay::AutomapView view;
    if (map_overlay::ReadAutomapView(g_mem, g_addr, &view) && view.mode != mode) {
      // Posicoes dos itens no quadro 800x600 (lidas dos quadros gravados).
      static const char* const kToggle[] = {
          "key esc",       "wait 800",  "click 400 210", "wait 900", "click 400 260",
          "wait 900",      "click 400 170", "wait 900",  "key esc",  "wait 1200"};
      g_script.insert(g_script.begin() + static_cast<std::ptrdiff_t>(g_pc) + 1,
                      std::begin(kToggle), std::end(kToggle));
      Say(AEONIAN_LOG_INFO, "MAPDRIVE trocando o automap do modo %d para %d", view.mode, mode);
    }
    return true;
  }
  if (c == "automap") {
    int mode = 0;
    long timeout = 0;
    std::sscanf(arg, "%d %ld", &mode, &timeout);
    if (g_deadline == 0) g_deadline = now + static_cast<DWORD>(timeout);
    map_overlay::AutomapView view;
    if (map_overlay::ReadAutomapView(g_mem, g_addr, &view) && view.visible && view.mode == mode) {
      Say(AEONIAN_LOG_INFO, "MAPDRIVE automap visivel no modo %d", mode);
      return true;
    }
    if (now >= g_deadline) {
      Say(AEONIAN_LOG_ERROR,
          "MAPDRIVE FAIL automap nao ficou visivel no modo %d (visivel=%d modo=%d)",
          mode, view.visible ? 1 : 0, view.mode);
      g_failed = true;
      return true;
    }
    return false;
  }
  if (c == "waitlevel" || c == "waitui") {
    int value = 0;
    long timeout = 0;
    std::sscanf(arg, "%d %ld", &value, &timeout);
    if (g_deadline == 0) g_deadline = now + static_cast<DWORD>(timeout);
    const bool done = c == "waitui" ? UiOn(value) : ScanNow().level_id == value;
    if (done) {
      Say(AEONIAN_LOG_INFO, "MAPDRIVE %s %d alcancado", c.c_str(), value);
      return true;
    }
    if (now >= g_deadline) {
      Say(AEONIAN_LOG_WARN, "MAPDRIVE %s %d: prazo esgotado", c.c_str(), value);
      return true;
    }
    return false;
  }
  if (c == "clickmark") {
    char name[32] = {};
    int ui = -1;
    long timeout = 0;
    std::sscanf(arg, "%31s %d %ld", name, &ui, &timeout);
    if (g_deadline == 0) {
      g_deadline = now + static_cast<DWORD>(timeout);
      g_walk_moved_ms = now;
      g_click_level = ScanNow().level_id;
    }
    const map_overlay::Scan scan = ScanNow();
    const bool left_level =
        scan.in_game && scan.level_id != 0 && scan.level_id != g_click_level;
    if (ui >= 0 ? UiOn(ui) : left_level) {
      Say(AEONIAN_LOG_INFO, "MAPDRIVE clickmark %s concluido no nivel %d", name, scan.level_id);
      return true;
    }
    map_overlay::MarkKind kind = map_overlay::MarkKind::kNpc;
    double dx = 0.0;
    double dy = 0.0;
    if (MarkKindFromName(name, &kind) && NearestMark(scan, kind, &dx, &dy) != nullptr) {
      // Com Teletransporte, chega perto antes de clicar: um clique longe pode cair
      // num monstro ou num painel que abriu.
      const bool close_enough = g_walk_right && !map_overlay::IsTownLevel(scan.level_id)
                                    ? std::hypot(dx, dy) < 120.0
                                    : OnScreen(dx, dy);
      if (close_enough) {
        if (now >= g_walk_next) {
          g_walk_next = now + 500;
          Click(400 + static_cast<int>(dx), 285 + static_cast<int>(dy));
        }
      } else {
        WalkStep(scan, dx, dy, now);
      }
    } else if (kind == map_overlay::MarkKind::kWaypoint) {
      // Sala do waypoint ainda nao carregada: anda ate o centro dela.
      const map_overlay::Point me = map_overlay::SubtileToClient(scan.player_x, scan.player_y);
      for (const map_overlay::Mark& m : scan.marks) {
        if (m.kind != map_overlay::MarkKind::kWaypointRoom) continue;
        WalkStep(scan, (m.x[0] + m.x[1] + m.x[2] + m.x[3]) / 4.0 - me.x,
                 (m.y[0] + m.y[1] + m.y[2] + m.y[3]) / 4.0 - me.y, now);
        break;
      }
    }
    if (now >= g_deadline) {
      Say(AEONIAN_LOG_WARN, "MAPDRIVE clickmark %s: prazo esgotado no nivel %d em (%d,%d)", name,
          scan.level_id, scan.player_x, scan.player_y);
      return true;
    }
    return false;
  }
  if (c == "goto") {
    char name[32] = {};
    long timeout = 0;
    std::sscanf(arg, "%31s %ld", name, &timeout);
    if (g_deadline == 0) {
      g_deadline = now + static_cast<DWORD>(timeout);
      g_walk_moved_ms = now;
    }
    const map_overlay::Scan scan = ScanNow();
    map_overlay::MarkKind kind = map_overlay::MarkKind::kNpc;
    double dx = 0.0;
    double dy = 0.0;
    const map_overlay::Mark* mark =
        MarkKindFromName(name, &kind) ? NearestMark(scan, kind, &dx, &dy) : nullptr;
    if (mark == nullptr || now >= g_deadline) {
      Say(AEONIAN_LOG_INFO, "MAPDRIVE goto %s: sem marca ou prazo esgotado", name);
      return true;
    }
    if (std::abs(dx) < 200.0 && std::abs(dy) < 130.0) {
      Say(AEONIAN_LOG_INFO, "MAPDRIVE goto %s: classe %d na tela", name, mark->unit_class);
      return true;
    }
    WalkStep(scan, dx, dy, now);
    return false;
  }
  if (c == "sweep") {
    char name[32] = {};
    long timeout = 0;
    std::sscanf(arg, "%31s %ld", name, &timeout);
    const map_overlay::Scan scan = ScanNow();
    if (!g_sweep_active) {
      g_sweep_active = true;
      g_deadline = now + static_cast<DWORD>(timeout);
      g_walk_moved_ms = now;
      g_sweep_since = now;
      g_sweep.clear();
      constexpr int32_t kStep = 60;  // subtiles: as salas em volta carregam
      const map_overlay::TileRect& r = scan.level_rect;
      for (int32_t y = r.y * 5 + kStep / 2; y < (r.y + r.h) * 5; y += kStep) {
        for (int32_t x = r.x * 5 + kStep / 2; x < (r.x + r.w) * 5; x += kStep) {
          g_sweep.push_back(SweepPoint{x, y});
        }
      }
    }
    map_overlay::MarkKind kind = map_overlay::MarkKind::kNpc;
    double dx = 0.0;
    double dy = 0.0;
    if (MarkKindFromName(name, &kind) && NearestMark(scan, kind, &dx, &dy) != nullptr) {
      Say(AEONIAN_LOG_INFO, "MAPDRIVE sweep %s: achou no nivel %d", name, scan.level_id);
      return true;
    }
    // Vai ao ponto mais perto; chegou ou passou 6 s nele, risca e segue.
    size_t best = g_sweep.size();
    int64_t best_d = 0;
    for (size_t i = 0; i < g_sweep.size(); ++i) {
      const int64_t ex = g_sweep[i].x - scan.player_x;
      const int64_t ey = g_sweep[i].y - scan.player_y;
      const int64_t d = ex * ex + ey * ey;
      if (best == g_sweep.size() || d < best_d) {
        best = i;
        best_d = d;
      }
    }
    if (best == g_sweep.size() || now >= g_deadline) {
      Say(AEONIAN_LOG_INFO, "MAPDRIVE sweep %s: terminou sem achar no nivel %d", name,
          scan.level_id);
      return true;
    }
    if (best_d < 20 * 20 || now - g_sweep_since > 6000) {
      g_sweep.erase(g_sweep.begin() + static_cast<std::ptrdiff_t>(best));
      g_sweep_since = now;
      return false;
    }
    const map_overlay::Point me = map_overlay::SubtileToClient(scan.player_x, scan.player_y);
    const map_overlay::Point to = map_overlay::SubtileToClient(g_sweep[best].x, g_sweep[best].y);
    WalkStep(scan, to.x - me.x, to.y - me.y, now);
    return false;
  }
  if (c == "walkpoint") {
    long px = 0;
    long py = 0;
    long timeout = 0;
    std::sscanf(arg, "%ld %ld %ld", &px, &py, &timeout);
    const map_overlay::Scan scan = ScanNow();
    if (g_deadline == 0) {
      g_deadline = now + static_cast<DWORD>(timeout);
      g_walk_moved_ms = now;
      g_entry_x = scan.player_x;
      g_entry_y = scan.player_y;
    }
    const int32_t tx = g_entry_x + static_cast<int32_t>(px);
    const int32_t ty = g_entry_y + static_cast<int32_t>(py);
    if (std::abs(scan.player_x - tx) + std::abs(scan.player_y - ty) < 8) return true;
    const map_overlay::Point me = map_overlay::SubtileToClient(scan.player_x, scan.player_y);
    const map_overlay::Point to = map_overlay::SubtileToClient(tx, ty);
    WalkStep(scan, to.x - me.x, to.y - me.y, now);
    if (now >= g_deadline) {
      Say(AEONIAN_LOG_WARN, "MAPDRIVE walkpoint: prazo esgotado em (%d,%d)", scan.player_x,
          scan.player_y);
      return true;
    }
    return false;
  }
  if (c == "walk" || c == "walkin" || c == "walkexit") {
    long value = 0;
    long timeout = 0;
    std::sscanf(arg, "%ld %ld", &value, &timeout);
    if (g_deadline == 0) {
      g_deadline = now + static_cast<DWORD>(timeout);
      g_walk_moved_ms = now;
    }
    const map_overlay::Scan scan = ScanNow();
    if (!scan.in_game) return now >= g_deadline;
    if (c == "walkexit") {
      for (const map_overlay::Mark& m : scan.marks) {
        if (m.kind != map_overlay::MarkKind::kExit || m.dest_level != value) continue;
        const map_overlay::Point me = map_overlay::SubtileToClient(scan.player_x, scan.player_y);
        const double d = std::hypot(m.x[0] - me.x, m.y[0] - me.y);
        if (d < 300.0) {
          Say(AEONIAN_LOG_INFO, "MAPDRIVE saida exata para o nivel %ld a %.0f px (lvlwarp %d)",
              value, d, m.warp_class);
          return true;
        }
      }
      double dx = 0.0;
      double dy = 0.0;
      if (WalkTarget(scan, static_cast<int32_t>(value), &dx, &dy)) WalkStep(scan, dx, dy, now);
    } else if (c == "walk") {
      if (scan.level_id == value) {
        g_entry_x = scan.player_x;
        g_entry_y = scan.player_y;
        Say(AEONIAN_LOG_INFO, "MAPDRIVE chegou ao nivel %d em (%d,%d)", scan.level_id,
            scan.player_x, scan.player_y);
        return true;
      }
      double dx = 0.0;
      double dy = 0.0;
      if (!WalkTarget(scan, static_cast<int32_t>(value), &dx, &dy)) {
        if (now >= g_deadline) Say(AEONIAN_LOG_ERROR, "MAPDRIVE FAIL sem saida para %ld", value);
        return now >= g_deadline;
      }
      WalkStep(scan, dx, dy, now);
    } else {
      // walkin: afasta-se do ponto de entrada (para dentro do nivel).
      const int32_t distance =
          std::abs(scan.player_x - g_entry_x) + std::abs(scan.player_y - g_entry_y);
      if (distance >= value) {
        Say(AEONIAN_LOG_INFO, "MAPDRIVE andou %d subtiles para dentro do nivel %d", distance,
            scan.level_id);
        return true;
      }
      // Direcao: do ponto de entrada para longe da borda com o nivel anterior.
      double dx = 0.0;
      double dy = 0.0;
      double bx = 0.0;
      double by = 0.0;
      bool have = false;
      for (const map_overlay::Mark& m : scan.marks) {
        if (m.kind == map_overlay::MarkKind::kPassage && m.primary && !have) {
          bx = (m.x[0] + m.x[1]) / 2.0;
          by = (m.y[0] + m.y[1]) / 2.0;
          have = true;
        }
      }
      const map_overlay::Point me = map_overlay::SubtileToClient(scan.player_x, scan.player_y);
      if (have) {
        dx = me.x - bx;
        dy = me.y - by;
      } else {
        dx = 1.0;
      }
      WalkStep(scan, dx, dy, now);
    }
    if (now >= g_deadline) {
      Say(AEONIAN_LOG_WARN, "MAPDRIVE %s: prazo esgotado no nivel %d em (%d,%d)", c.c_str(),
          scan.level_id, scan.player_x, scan.player_y);
      return true;
    }
    return false;
  }
  Say(AEONIAN_LOG_ERROR, "MAPDRIVE FAIL comando desconhecido: %s", line.c_str());
  g_failed = true;
  return true;
}

void OnTick(void*, uint32_t) {
  if (!g_started || g_pc >= g_script.size()) return;
  const DWORD now = GetTickCount();
  if (now < g_wait_until) return;
  g_wait_until = now + 30;  // ~30 ms entre passos
  g_mem.Reset();
  if (Step(now)) {
    Say(AEONIAN_LOG_DEBUG, "MAPDRIVE ok: %s", g_script[g_pc].c_str());
    Next();
    if (g_pc >= g_script.size()) {
      if (g_ae_drive_trampoline != nullptr) {
        Say(AEONIAN_LOG_INFO, "MAPDRIVE desvio proprio contou %ld chamadas de DrawAutomap",
            static_cast<long>(g_ae_drive_draws));
      }
      Say(g_failed ? AEONIAN_LOG_ERROR : AEONIAN_LOG_INFO, "MAPDRIVE roteiro concluido %s",
          g_failed ? "COM FALHAS" : "sem falhas");
    }
  }
}

void OnLoaded(void*) { g_started = true; }

void HookDrawAutomap(const map_overlay::MemoryView& original) {
  const uintptr_t target = g_host->resolve_symbol(g_host, "D2Client.DrawAutomap");
  uint8_t expected[7];  // a 1a instrucao, sem operando relativo
  void* trampoline = nullptr;
  const int rc = target != 0 && original.Read(static_cast<uint32_t>(target), expected, 7)
                     ? g_host->hook_inline(g_host, target, expected, 7,
                                           reinterpret_cast<void*>(&AeMapDriveDrawDetour),
                                           &trampoline)
                     : AEONIAN_ERR_MISMATCH;
  g_ae_drive_trampoline = trampoline;
  Say(rc == AEONIAN_OK ? AEONIAN_LOG_INFO : AEONIAN_LOG_ERROR,
      "MAPDRIVE desvio proprio em DrawAutomap rc=%d", rc);
}

}  // namespace

extern "C" AEONIAN_PLUGIN_EXPORT int AeonianPluginInit(const AeonianHost* host) {
  g_host = host;
  if (!LoadScript()) {
    Say(AEONIAN_LOG_ERROR, "MAPDRIVE sem mapdrive.txt");
    return 1;
  }
  map_overlay::ResolveReport report;
  const map_overlay::OriginalMemory original(host, g_mem);
  map_overlay::ResolveAddresses(original, Resolve, nullptr, &g_addr, &report);
  Say(AEONIAN_LOG_INFO, "MAPDRIVE roteiro com %u linhas; simbolos %u/%u; ato 0x%08x",
      static_cast<unsigned>(g_script.size()), report.resolved, report.total, g_addr.current_act);
  const char* hook = std::getenv("AEONIAN_MAPDRIVE_HOOK_DRAW");
  if (hook != nullptr && hook[0] == '1') HookDrawAutomap(original);
  AeonianEvents events = {};
  events.struct_size = sizeof(events);
  events.on_loaded = OnLoaded;
  events.on_tick = OnTick;
  return host->register_events(host, &events);
}
