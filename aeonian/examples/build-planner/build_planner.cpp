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

// Plugin nativo do planejador de builds (item C3 do ROADMAP e M2.2 Personagem).
//
//  * Resolve os simbolos de personagem de aeonian/addresses/1.14d.ini;
//  * Le atributos base/totais, vida/mana, nivel, pontos livres e habilidades;
//  * Confere os valores lidos da memoria contra D2Common.GetUnitBaseStat,
//    D2Common.GetUnitStat e D2Common.GetBaseSkillLevelById no jogo real;
//  * Carrega planos versionados (builds/*.json) e salva a build do personagem
//    atual ao apertar F8;
//  * Quando auto_apply = 1 (somente em um jogador), distribui pontos livres
//    pelo proprio pacote da UI do cliente (D2Client.SendPacket_ByteWord,
//    0x3A para atributo e 0x3B para habilidade), respeitando o intervalo de
//    200 ms do filtro de pacotes do cliente (game-exe-1.14d-character.md sec. 4).

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "aeonian/api.h"
#include "build_plan.h"
#include "core/ini.h"

namespace {

using namespace build_planner;

// Leitura protegida da memoria do processo (mesmo padrao do map-overlay).
class ProcessMemory final : public MemoryView {
 public:
  bool Read(uint32_t address, void* out, uint32_t size) const override {
    if (address < 0x10000u || size == 0 || address + size < address) return false;
    uintptr_t cursor = address;
    const uintptr_t end = cursor + size;
    while (cursor < end) {
      MEMORY_BASIC_INFORMATION mbi = {};
      if (VirtualQuery(reinterpret_cast<const void*>(cursor), &mbi, sizeof(mbi)) == 0) {
        return false;
      }
      const DWORD prot = mbi.Protect & 0xFFu;
      const bool readable = mbi.State == MEM_COMMIT && (mbi.Protect & PAGE_GUARD) == 0 &&
                            (prot == PAGE_READONLY || prot == PAGE_READWRITE ||
                             prot == PAGE_WRITECOPY || prot == PAGE_EXECUTE_READ ||
                             prot == PAGE_EXECUTE_READWRITE || prot == PAGE_EXECUTE_WRITECOPY);
      if (!readable) return false;
      const uintptr_t region_end =
          reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
      if (region_end <= cursor) return false;
      cursor = region_end;
    }
    std::memcpy(out, reinterpret_cast<const void*>(static_cast<uintptr_t>(address)), size);
    return true;
  }
};

class OriginalMemory final : public MemoryView {
 public:
  OriginalMemory(const AeonianHost* host, const MemoryView* fallback)
      : host_(host), fallback_(fallback) {}
  bool Read(uint32_t address, void* out, uint32_t size) const override {
    if (host_ != nullptr && host_->struct_size >= sizeof(AeonianHost) &&
        host_->read_original != nullptr &&
        host_->read_original(host_, address, out, size) == AEONIAN_OK) {
      return true;
    }
    return fallback_->Read(address, out, size);
  }

 private:
  const AeonianHost* host_;
  const MemoryView* fallback_;
};

// Assinaturas das funcoes do jogo (game-exe-1.14d-character.md sec. 2, 3 e 4).
using GetUnitStatFn = int32_t(__stdcall*)(uint32_t unit, int32_t stat_id, uint16_t layer);
using GetBaseSkillLevelByIdFn = int32_t(__fastcall*)(uint32_t unit, int32_t skill_id);
using CheckSkillPrereqsFn = int32_t(__stdcall*)(uint32_t unit, int32_t skill_id);
using SendPacketByteWordFn = void(__fastcall*)(uint8_t packet_id, uint16_t param);

const AeonianHost* g_host = nullptr;
ProcessMemory g_mem;
CharacterAddresses g_addr;
BuildPlan g_plan;
bool g_plan_loaded = false;
std::string g_plan_file = "builds\\sorceress-orb-hydra.json";

bool g_enabled = true;
bool g_auto_apply = false;
UINT g_toggle_vk = VK_F10;
UINT g_capture_vk = VK_F8;
DWORD g_refresh_ms = 100;
DWORD g_step_delay_ms = 250;

DWORD g_last_scan_tick = 0;
DWORD g_last_spend_tick = 0;
uint32_t g_verified_unit = 0;
uint32_t g_spent_stats = 0;
uint32_t g_spent_skills = 0;
bool g_logged_completion = false;

SRWLOCK g_lock = SRWLOCK_INIT;
CharacterState g_published_state;
int32_t g_published_next_attr = -1;
int32_t g_published_next_skill = -1;

HFONT g_font = nullptr;
HFONT g_font_bold = nullptr;

void Logf(AeonianLogLevel level, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
void Logf(AeonianLogLevel level, const char* fmt, ...) {
  if (g_host == nullptr) return;
  char buf[512];
  va_list args;
  va_start(args, fmt);
  std::vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  g_host->log(g_host, level, buf);
}

uint32_t ResolveCallback(void*, const char* name) {
  return static_cast<uint32_t>(g_host->resolve_symbol(g_host, name));
}

std::string ReadFileText(const std::string& path) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (f == nullptr) return {};
  std::string out;
  char buf[1024];
  size_t n = 0;
  while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
    out.append(buf, n);
  }
  std::fclose(f);
  return out;
}

bool WriteFileText(const std::string& path, const std::string& content) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (f == nullptr) return false;
  const size_t n = std::fwrite(content.data(), 1, content.size(), f);
  std::fclose(f);
  return n == content.size();
}

UINT ParseKey(const std::string& name, UINT fallback) {
  if (name.size() >= 2 && (name[0] == 'f' || name[0] == 'F')) {
    const int n = std::atoi(name.c_str() + 1);
    if (n >= 1 && n <= 24) return static_cast<UINT>(VK_F1 + n - 1);
  }
  return fallback;
}

void LoadConfig() {
  const std::string cfg_path = std::string(g_host->mod_dir) + "\\build-planner.ini";
  const std::string text = ReadFileText(cfg_path);
  if (!text.empty()) {
    const aeonian::Ini ini = aeonian::Ini::Parse(text);
    g_enabled = ini.GetBool("planner", "enabled", g_enabled);
    g_auto_apply = ini.GetBool("planner", "auto_apply", g_auto_apply);
    g_toggle_vk = ParseKey(ini.GetOr("planner", "toggle_key", "f10"), VK_F10);
    g_capture_vk = ParseKey(ini.GetOr("planner", "capture_key", "f8"), VK_F8);
    g_plan_file = ini.GetOr("planner", "plan_file", g_plan_file);
    const int64_t delay = ini.GetInt("planner", "step_delay_ms", 250);
    g_step_delay_ms = static_cast<DWORD>(delay < 220 ? 220 : (delay > 5000 ? 5000 : delay));
  }
  if (const char* env_auto = std::getenv("AEONIAN_BUILD_AUTO_APPLY")) {
    g_auto_apply = env_auto[0] == '1';
  }
  if (const char* env_plan = std::getenv("AEONIAN_BUILD_PLAN")) {
    if (env_plan[0] != '\0') g_plan_file = env_plan;
  }
  const std::string full_plan = std::string(g_host->mod_dir) + "\\" + g_plan_file;
  const std::string json_text = ReadFileText(full_plan);
  if (!json_text.empty()) {
    std::string err;
    if (ParseBuildJson(json_text, &g_plan, &err)) {
      g_plan_loaded = true;
      Logf(AEONIAN_LOG_INFO, "build-planner: plano carregado '%s' (classe=%s, %zu passos)",
           g_plan.name.c_str(), ClassName(g_plan.char_class), g_plan.skills.size());
    } else {
      Logf(AEONIAN_LOG_WARN, "build-planner: erro em %s: %s", full_plan.c_str(), err.c_str());
    }
  } else {
    Logf(AEONIAN_LOG_WARN, "build-planner: plano nao encontrado em %s", full_plan.c_str());
  }
}

// Confere na partida que a leitura direta de StatListEx e SkillList produz
// exatamente os mesmos valores que as funcoes de D2Common do jogo real.
void VerifyAgainstD2Common(const CharacterState& state) {
  if (g_addr.get_unit_base_stat == 0 || g_addr.get_unit_stat == 0 ||
      g_addr.get_base_skill_level_by_id == 0) {
    return;
  }
  const auto get_base = reinterpret_cast<GetUnitStatFn>(
      static_cast<uintptr_t>(g_addr.get_unit_base_stat));
  const auto get_total = reinterpret_cast<GetUnitStatFn>(
      static_cast<uintptr_t>(g_addr.get_unit_stat));
  const auto get_skill_base = reinterpret_cast<GetBaseSkillLevelByIdFn>(
      static_cast<uintptr_t>(g_addr.get_base_skill_level_by_id));

  uint32_t diffs = 0;
  const int32_t base_ids[] = {kStatStrength, kStatEnergy, kStatDexterity, kStatVitality,
                              kStatPts,      kStatNewSkills, kStatLevel};
  for (int32_t id : base_ids) {
    const int32_t fn_val = get_base(state.unit, id, 0);
    if (fn_val != state.BaseStat(id)) ++diffs;
  }
  const int32_t total_ids[] = {kStatStrength, kStatEnergy, kStatDexterity, kStatVitality};
  for (int32_t id : total_ids) {
    const int32_t fn_val = get_total(state.unit, id, 0);
    if (fn_val != state.TotalStat(id)) ++diffs;
  }
  for (const LearnedSkill& sk : state.skills) {
    const int32_t fn_lvl = get_skill_base(state.unit, sk.skill_id);
    if (fn_lvl != sk.base_level) ++diffs;
  }

  uint32_t prereq_diffs = 0;
  if (g_addr.check_skill_prereqs != 0 && state.new_skills > 0) {
    const auto check_prereqs = reinterpret_cast<CheckSkillPrereqsFn>(
        static_cast<uintptr_t>(g_addr.check_skill_prereqs));
    for (const SkillStep& step : g_plan.skills) {
      if (step.skill_id < 0) continue;
      SkillDef def;
      if (!ReadSkillDef(g_mem, g_addr, step.skill_id, &def)) continue;
      if (def.char_class != state.char_class) continue;
      const bool fn_ok = check_prereqs(state.unit, step.skill_id) != 0;
      // CheckSkillPrereqs testa nivel e pre-requisitos (sem atributos/max_level).
      bool mem_prereqs_ok =
          state.level >= static_cast<int32_t>(def.req_level) + state.BaseSkillLevel(step.skill_id);
      for (int i = 0; i < 3; ++i) {
        if (def.req_skill[i] >= 0 && state.BaseSkillLevel(def.req_skill[i]) < 1) {
          mem_prereqs_ok = false;
        }
      }
      if (fn_ok != mem_prereqs_ok) ++prereq_diffs;
    }
  }

  CharStatsDef cs_def;
  const bool has_cs = ReadCharStatsDef(g_mem, g_addr, state.char_class, &cs_def);
  const int32_t cs_str = has_cs ? static_cast<int32_t>(cs_def.base_str) : -1;
  const int32_t stat_per_lvl = has_cs ? cs_def.stat_per_level : -1;

  Logf(AEONIAN_LOG_INFO,
       "build-planner: personagem '%s' classe=%s(%d) nv=%d HP=%d/%d Mana=%d/%d "
       "STR=%d(%d) DEX=%d(%d) VIT=%d(%d) ENE=%d(%d) StatPts=%d NewSkills=%d "
       "skills_aprendidas=%zu cs_str=%d stat_per_lvl=%d (conferencia D2Common: stat_diffs=%u prereq_diffs=%u)",
       state.name, ClassName(state.char_class), state.char_class, state.level, state.hp,
       state.max_hp, state.mana, state.max_mana, state.base_strength, state.strength,
       state.base_dexterity, state.dexterity, state.base_vitality, state.vitality,
       state.base_energy, state.energy, state.stat_pts, state.new_skills, state.skills.size(),
       cs_str, stat_per_lvl, diffs, prereq_diffs);
}

void SaveCurrentCharacterBuild(const CharacterState& state) {
  if (!state.in_game) return;
  const std::string builds_dir = std::string(g_host->mod_dir) + "\\builds";
  CreateDirectoryA(builds_dir.c_str(), nullptr);
  const std::string safe_name = state.name[0] != '\0' ? state.name : "captured";
  const BuildPlan captured = CaptureBuildFromState(state, safe_name);
  const std::string path = builds_dir + "\\" + safe_name + ".json";
  if (WriteFileText(path, FormatBuildJson(captured))) {
    Logf(AEONIAN_LOG_INFO, "build-planner: build capturada em %s (%zu habilidades)",
         path.c_str(), captured.skills.size());
  } else {
    Logf(AEONIAN_LOG_WARN, "build-planner: falha ao gravar %s", path.c_str());
  }
}

void OnLoaded(void*) {
  OriginalMemory orig(g_host, &g_mem);
  ResolveReport report;
  ResolveAddresses(orig, ResolveCallback, nullptr, &g_addr, &report);
  Logf(AEONIAN_LOG_INFO, "build-planner: %u/%u simbolos resolvidos", report.resolved,
       report.total);
  for (const std::string& m : report.missing) {
    Logf(AEONIAN_LOG_WARN, "build-planner: simbolo ausente: %s", m.c_str());
  }
  for (const std::string& n : report.notes) {
    Logf(AEONIAN_LOG_WARN, "build-planner: %s", n.c_str());
  }
  LoadConfig();
  g_font = CreateFontA(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
                       DEFAULT_PITCH | FF_SWISS, "Arial");
  g_font_bold = CreateFontA(13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
                            DEFAULT_PITCH | FF_SWISS, "Arial");
}

void OnTick(void*, uint32_t) {
  const DWORD now = GetTickCount();
  if (now - g_last_scan_tick < g_refresh_ms) return;
  g_last_scan_tick = now;

  CharacterState state;
  if (!ReadCharacterState(g_mem, g_addr, &state)) {
    g_verified_unit = 0;
    AcquireSRWLockExclusive(&g_lock);
    g_published_state = CharacterState{};
    g_published_next_attr = -1;
    g_published_next_skill = -1;
    ReleaseSRWLockExclusive(&g_lock);
    return;
  }

  if (state.unit != g_verified_unit) {
    g_verified_unit = state.unit;
    g_spent_stats = 0;
    g_spent_skills = 0;
    g_logged_completion = false;
    VerifyAgainstD2Common(state);
    if (const char* cap = std::getenv("AEONIAN_BUILD_CAPTURE_ON_ENTER")) {
      if (cap[0] == '1') SaveCurrentCharacterBuild(state);
    }
  }

  int32_t next_attr = -1;
  int32_t next_skill = -1;
  if (g_plan_loaded) {
    next_attr = NextAttributeToSpend(state, g_plan);
    next_skill = NextSkillToSpend(g_mem, g_addr, state, g_plan);
  }

  if (!g_logged_completion && (g_spent_stats > 0 || g_spent_skills > 0) &&
      state.stat_pts == 0 && state.new_skills == 0) {
    g_logged_completion = true;
    VerifyAgainstD2Common(state);
    SaveCurrentCharacterBuild(state);
    Logf(AEONIAN_LOG_INFO,
         "build-planner: todos os pontos livres aplicados na partida "
         "(gasto_attr=%u gasto_skill=%u STR=%d DEX=%d VIT=%d ENE=%d)",
         g_spent_stats, g_spent_skills, state.base_strength, state.base_dexterity,
         state.base_vitality, state.base_energy);
  }

  if (g_auto_apply && g_plan_loaded && g_addr.send_packet_byte_word != 0 &&
      state.game_type_known && state.game_type <= 1 &&
      now - g_last_spend_tick >= g_step_delay_ms) {
    const auto send_pkt = reinterpret_cast<SendPacketByteWordFn>(
        static_cast<uintptr_t>(g_addr.send_packet_byte_word));
    if (next_attr >= 0) {
      send_pkt(0x3Au, static_cast<uint16_t>(next_attr));
      g_last_spend_tick = now;
      ++g_spent_stats;
      Logf(AEONIAN_LOG_INFO, "build-planner: pacote 0x3A enviado (+1 %s, total gasto=%u)",
           StatName(next_attr), g_spent_stats);
    } else if (next_skill >= 0) {
      send_pkt(0x3Bu, static_cast<uint16_t>(next_skill));
      g_last_spend_tick = now;
      ++g_spent_skills;
      Logf(AEONIAN_LOG_INFO, "build-planner: pacote 0x3B enviado (+1 skill %d, total gasto=%u)",
           next_skill, g_spent_skills);
    }
  }

  AcquireSRWLockExclusive(&g_lock);
  g_published_state = state;
  g_published_next_attr = next_attr;
  g_published_next_skill = next_skill;
  ReleaseSRWLockExclusive(&g_lock);
}

int OnWindowMessage(void*, AeonianWindowMessage* msg) {
  if (msg->message == WM_KEYDOWN && (msg->lparam & (1 << 30)) == 0) {
    if (msg->wparam == g_toggle_vk) {
      g_enabled = !g_enabled;
      Logf(AEONIAN_LOG_INFO, "build-planner: painel %s", g_enabled ? "ligado" : "desligado");
      return 1;
    }
    if (msg->wparam == g_capture_vk) {
      CharacterState copy;
      AcquireSRWLockShared(&g_lock);
      copy = g_published_state;
      ReleaseSRWLockShared(&g_lock);
      SaveCurrentCharacterBuild(copy);
      return 1;
    }
  }
  return 0;
}

void DrawBox(HDC dc, const RECT& r, COLORREF fill, COLORREF border) {
  HBRUSH b = CreateSolidBrush(fill);
  FillRect(dc, &r, b);
  DeleteObject(b);
  HBRUSH e = CreateSolidBrush(border);
  FrameRect(dc, &r, e);
  DeleteObject(e);
}

void DrawTextLine(HDC dc, HFONT font, COLORREF color, int x, int y, const char* text) {
  SelectObject(dc, font);
  SetTextColor(dc, color);
  TextOutA(dc, x, y, text, static_cast<int>(std::strlen(text)));
}

void OnPresent(void*, AeonianCanvas* canvas) {
  if (!g_enabled) return;
  CharacterState state;
  int32_t next_attr = -1;
  int32_t next_skill = -1;
  AcquireSRWLockShared(&g_lock);
  state = g_published_state;
  next_attr = g_published_next_attr;
  next_skill = g_published_next_skill;
  ReleaseSRWLockShared(&g_lock);
  if (!state.in_game) return;

  HDC dc = static_cast<HDC>(canvas->hdc);
  SetBkMode(dc, TRANSPARENT);
  canvas->opacity = 230;

  RECT box = {6, 6, 390, 62};
  DrawBox(dc, box, RGB(18, 16, 12), RGB(199, 179, 119));

  char line1[160];
  std::snprintf(line1, sizeof(line1), "%s (%s nv %d)  HP %d/%d  Mana %d/%d",
                state.name[0] ? state.name : "Jogador", ClassName(state.char_class), state.level,
                state.hp, state.max_hp, state.mana, state.max_mana);
  DrawTextLine(dc, g_font_bold, RGB(199, 179, 119), 12, 10, line1);

  char line2[160];
  std::snprintf(line2, sizeof(line2),
                "STR %d  DEX %d  VIT %d  ENE %d  | Livres: %d attr, %d sk", state.strength,
                state.dexterity, state.vitality, state.energy, state.stat_pts, state.new_skills);
  DrawTextLine(dc, g_font, RGB(230, 230, 230), 12, 26, line2);

  char line3[160];
  std::snprintf(line3, sizeof(line3), "Build: %s | Prox: attr=%s, skill=%d%s",
                g_plan_loaded ? g_plan.name.c_str() : "(nenhuma)", StatName(next_attr),
                next_skill, g_auto_apply ? " [AUTO]" : "");
  DrawTextLine(dc, g_font, RGB(170, 210, 170), 12, 42, line3);
}

void OnShutdown(void*) {
  if (g_font != nullptr) DeleteObject(g_font);
  if (g_font_bold != nullptr) DeleteObject(g_font_bold);
  g_font = nullptr;
  g_font_bold = nullptr;
}

}  // namespace

extern "C" AEONIAN_PLUGIN_EXPORT int AeonianPluginInit(const AeonianHost* host) {
  g_host = host;
  AeonianEvents events = {};
  events.struct_size = sizeof(events);
  events.on_loaded = OnLoaded;
  events.on_tick = OnTick;
  events.on_window_message = OnWindowMessage;
  events.on_present = OnPresent;
  events.on_shutdown = OnShutdown;
  return host->register_events(host, &events);
}
