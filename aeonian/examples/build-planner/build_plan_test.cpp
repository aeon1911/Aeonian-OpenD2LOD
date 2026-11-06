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

// Testes no host da logica portavel do mod build-planner (C3 e M2.2).

#include "build_plan.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "core/address_db.h"

namespace {

int g_failures = 0;

#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "%s:%d: CHECK falhou: %s\n", __FILE__, __LINE__, \
                   #cond);                                                \
      ++g_failures;                                                       \
    }                                                                     \
  } while (0)

#define CHECK_EQ(a, b)                                                    \
  do {                                                                    \
    auto va_ = (a);                                                       \
    auto vb_ = (b);                                                       \
    if (!(va_ == vb_)) {                                                  \
      std::fprintf(stderr, "%s:%d: CHECK_EQ falhou: %s != %s (%lld != %lld)\n", \
                   __FILE__, __LINE__, #a, #b, static_cast<long long>(va_),  \
                   static_cast<long long>(vb_));                          \
      ++g_failures;                                                       \
    }                                                                     \
  } while (0)

using namespace build_planner;

class FakeMemory final : public MemoryView {
 public:
  bool Read(uint32_t address, void* out, uint32_t size) const override {
    for (const Segment& s : segments_) {
      if (address >= s.base && address - s.base + size <= s.bytes.size()) {
        std::memcpy(out, s.bytes.data() + (address - s.base), size);
        return true;
      }
    }
    return false;
  }
  uint32_t Alloc(uint32_t size) {
    const uint32_t base = next_;
    next_ += (size + 15u) / 16u * 16u + 16u;
    segments_.push_back(Segment{base, std::vector<uint8_t>(size, 0)});
    return base;
  }
  void Map(uint32_t base, const std::vector<uint8_t>& bytes) {
    segments_.push_back(Segment{base, bytes});
  }
  void Put32(uint32_t address, uint32_t value) { Put(address, &value, 4); }
  void PutI(uint32_t address, int32_t value) { Put(address, &value, 4); }
  void Put16(uint32_t address, uint16_t value) { Put(address, &value, 2); }
  void PutI16(uint32_t address, int16_t value) { Put(address, &value, 2); }
  void Put8(uint32_t address, uint8_t value) { Put(address, &value, 1); }
  void PutStr(uint32_t address, const char* str, uint32_t max_len) {
    const uint32_t n = static_cast<uint32_t>(std::strlen(str));
    Put(address, str, n < max_len ? n + 1 : max_len);
  }

 private:
  struct Segment {
    uint32_t base;
    std::vector<uint8_t> bytes;
  };
  void Put(uint32_t address, const void* value, uint32_t size) {
    for (Segment& s : segments_) {
      if (address >= s.base && address - s.base + size <= s.bytes.size()) {
        std::memcpy(s.bytes.data() + (address - s.base), value, size);
        return;
      }
    }
    std::fprintf(stderr, "escrita fora da memoria sintetica: 0x%08x\n", address);
    ++g_failures;
  }
  std::vector<Segment> segments_;
  uint32_t next_ = 0x02000000;
};

constexpr uint32_t kImageBase = 0x400000;

std::string ReadText(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  std::stringstream buf;
  buf << f.rdbuf();
  return buf.str();
}

uint32_t ResolveFromDb(void* context, const char* name) {
  const auto* db = static_cast<const aeonian::AddressDb*>(context);
  const aeonian::Symbol* s = db->Find(name);
  return s == nullptr ? 0 : kImageBase + s->rva;
}

void TestAnchorsAgainstRealIni() {
  std::string error;
  auto db = aeonian::AddressDb::Parse(
      ReadText(std::string(AEONIAN_SOURCE_DIR) + "/addresses/1.14d.ini"), &error);
  CHECK(db.has_value());
  if (!db) return;

  FakeMemory mem;
  for (const std::string& name : RequiredSymbols()) {
    const aeonian::Symbol* s = db->Find(name);
    CHECK(s != nullptr);
    if (s == nullptr) continue;
    CHECK(!s->expect.empty());
    mem.Map(kImageBase + s->rva, s->expect);
  }

  CharacterAddresses a;
  ResolveReport report;
  ResolveAddresses(mem, ResolveFromDb, &*db, &a, &report);
  CHECK_EQ(report.resolved, report.total);
  CHECK_EQ(report.total, 17u);
  CHECK(report.missing.empty());
  CHECK(report.notes.empty());

  // Globais ancorados na spec.
  CHECK_EQ(a.player_unit, 0x7A6A70u);
  CHECK_EQ(a.current_act, 0x7A0634u);
  CHECK_EQ(a.game_type, 0x7A0610u);
  CHECK_EQ(a.data_tables, 0x744304u);

  // Funcoes verificadas na spec (game-exe-1.14d-character.md).
  CHECK_EQ(a.get_unit_base_stat, 0x6253B0u);
  CHECK_EQ(a.get_unit_stat, 0x625480u);
  CHECK_EQ(a.get_first_skill, 0x643910u);
  CHECK_EQ(a.get_next_skill, 0x6438F0u);
  CHECK_EQ(a.get_skill_by_id, 0x6439B0u);
  CHECK_EQ(a.get_skill_id, 0x643CE0u);
  CHECK_EQ(a.get_skill_level, 0x6442A0u);
  CHECK_EQ(a.get_base_skill_level_by_id, 0x6447B0u);
  CHECK_EQ(a.get_skill_level_by_id, 0x644AA0u);
  CHECK_EQ(a.check_skill_prereqs, 0x6447D0u);
  CHECK_EQ(a.check_skill_stat_reqs, 0x644920u);
  CHECK_EQ(a.get_skill_max_level, 0x4AA8B0u);
  CHECK_EQ(a.send_packet_byte_word, 0x4785B0u);
}

void TestJsonParseAndFormat() {
  const char* sample = R"({
    "version": 1,
    "name": "Sorceress Hydra/Orb",
    "class": "sorceress",
    "auto_prereqs": true,
    "attributes": {
      "strength": 156,
      "dexterity": 35,
      "vitality": 300,
      "energy": 50,
      "overflow": "vitality"
    },
    "skills": [
      { "id": 36, "level": 1 },
      { "id": 62, "level": 20 },
      { "id": 64, "level": 20 }
    ]
  })";

  BuildPlan plan;
  std::string err;
  CHECK(ParseBuildJson(sample, &plan, &err));
  CHECK(err.empty());
  CHECK_EQ(plan.version, 1);
  CHECK(plan.name == "Sorceress Hydra/Orb");
  CHECK_EQ(plan.char_class, kClassSorceress);
  CHECK(plan.auto_prereqs);
  CHECK_EQ(plan.target_strength, 156);
  CHECK_EQ(plan.target_dexterity, 35);
  CHECK_EQ(plan.target_vitality, 300);
  CHECK_EQ(plan.target_energy, 50);
  CHECK_EQ(plan.overflow_stat, kStatVitality);
  CHECK_EQ(plan.skills.size(), 3u);
  if (plan.skills.size() == 3) {
    CHECK_EQ(plan.skills[1].skill_id, 62);
    CHECK_EQ(plan.skills[1].target_level, 20);
  }

  // Round-trip pelo FormatBuildJson.
  const std::string formatted = FormatBuildJson(plan);
  BuildPlan reparsed;
  CHECK(ParseBuildJson(formatted, &reparsed, &err));
  CHECK(reparsed.name == plan.name);
  CHECK_EQ(reparsed.char_class, plan.char_class);
  CHECK_EQ(reparsed.target_strength, plan.target_strength);
  CHECK_EQ(reparsed.skills.size(), plan.skills.size());

  // Erros esperados em entradas invalidas.
  BuildPlan bad;
  CHECK(!ParseBuildJson("{}", &bad, &err));
  CHECK(!ParseBuildJson("{\"version\": 2, \"name\": \"x\", \"class\": \"sorceress\"}", &bad, &err));
  CHECK(!ParseBuildJson("{\"version\": 1, \"name\": \"x\", \"class\": \"paladino_errado\"}", &bad, &err));
}

void TestCharacterStateAndPlanner() {
  FakeMemory mem;
  CharacterAddresses addr;
  const uint32_t globals = mem.Alloc(0x40);
  addr.player_unit = globals + 0;
  addr.current_act = globals + 4;
  addr.game_type = globals + 8;
  addr.data_tables = globals + 12;

  const uint32_t act = mem.Alloc(0x50);
  mem.Put32(addr.current_act, act);
  mem.Put32(addr.game_type, 0);

  // Tabela SkillsTxt sintetica com 70 entradas (stride 0x23C) e CharStatsTxt (7 entradas, stride 0xC4).
  const uint32_t tables = mem.Alloc(0xD00);
  const uint32_t skills_txt = mem.Alloc(70 * 0x23C);
  const uint32_t charstats_txt = mem.Alloc(7 * 0xC4);
  mem.Put32(addr.data_tables, tables);
  mem.Put32(tables + 0xB98, skills_txt);
  mem.Put32(tables + 0xBA0, 70);
  mem.Put32(tables + 0xBC4, charstats_txt);
  mem.Put32(tables + 0xBC8, 7);
  const uint32_t sorc_cs = charstats_txt + kClassSorceress * 0xC4;
  mem.Put8(sorc_cs + 0x30, 10);
  mem.Put8(sorc_cs + 0x31, 25);
  mem.Put8(sorc_cs + 0x32, 35);
  mem.Put8(sorc_cs + 0x33, 10);
  mem.PutI(sorc_cs + 0x50, 5);
  CharStatsDef cs_def;
  CHECK(ReadCharStatsDef(mem, addr, kClassSorceress, &cs_def));
  CHECK_EQ(cs_def.base_str, 10);
  CHECK_EQ(cs_def.base_int, 35);
  CHECK_EQ(cs_def.stat_per_level, 5);

  auto DefineSkill = [&](uint16_t id, int8_t cls, uint16_t req_lvl, int16_t req1 = -1,
                         int16_t req2 = -1, uint16_t req_str = 0) {
    const uint32_t r = skills_txt + id * 0x23C;
    mem.Put16(r + 0x000, id);
    mem.Put8(r + 0x00C, static_cast<uint8_t>(cls));
    mem.Put16(r + 0x12C, 20);
    mem.Put16(r + 0x174, req_lvl);
    mem.Put16(r + 0x176, req_str);
    mem.PutI16(r + 0x17E, req1);
    mem.PutI16(r + 0x180, req2);
    mem.PutI16(r + 0x182, -1);
    return r;
  };

  // Arvore de fogo da Feiticeira (classe 1):
  // 36 Fire Bolt (req_lvl 1) -> 47 Fire Ball (req_lvl 12, req 36) ->
  // 56 Enchant (req_lvl 18, req 47 e 37 Warmth) -> 62 Hydra (req_lvl 30, req 56).
  const uint32_t txt_36 = DefineSkill(36, kClassSorceress, 1);
  DefineSkill(37, kClassSorceress, 1);
  DefineSkill(47, kClassSorceress, 12, 36);
  DefineSkill(56, kClassSorceress, 18, 47, 37);
  DefineSkill(62, kClassSorceress, 30, 56);
  // Habilidade de outra classe (Barbaro = 4).
  DefineSkill(68, kClassBarbarian, 1);

  // Jogador Feiticeira em nivel 30.
  const uint32_t player = mem.Alloc(0x100);
  const uint32_t pdata = mem.Alloc(0x40);
  const uint32_t stat_list = mem.Alloc(0x60);
  const uint32_t base_pairs = mem.Alloc(16 * 8);
  const uint32_t full_pairs = mem.Alloc(16 * 8);
  const uint32_t skill_list = mem.Alloc(0x20);
  mem.Put32(addr.player_unit, player);
  mem.Put32(player + 0x00, 0);  // kTypePlayer
  mem.PutI(player + 0x04, kClassSorceress);
  mem.Put32(player + 0x14, pdata);
  mem.Put32(player + 0x5C, stat_list);
  mem.Put32(player + 0xA8, skill_list);
  mem.PutStr(pdata, "AeonSorc", 16);

  auto SetStat = [&](uint32_t pairs, uint32_t idx, uint16_t stat_id, int32_t val) {
    mem.Put32(pairs + idx * 8, static_cast<uint32_t>(stat_id) << 16);
    mem.PutI(pairs + idx * 8 + 4, val);
  };
  SetStat(base_pairs, 0, kStatStrength, 40);
  SetStat(base_pairs, 1, kStatEnergy, 35);
  SetStat(base_pairs, 2, kStatDexterity, 25);
  SetStat(base_pairs, 3, kStatVitality, 80);
  SetStat(base_pairs, 4, kStatPts, 10);
  SetStat(base_pairs, 5, kStatNewSkills, 5);
  SetStat(base_pairs, 6, kStatLevel, 30);

  SetStat(full_pairs, 0, kStatStrength, 65);  // +25 de equipamento
  SetStat(full_pairs, 1, kStatEnergy, 50);
  SetStat(full_pairs, 2, kStatDexterity, 25);
  SetStat(full_pairs, 3, kStatVitality, 100);
  SetStat(full_pairs, 4, kStatHitPoints, 320 << 8);
  SetStat(full_pairs, 5, kStatMaxHp, 350 << 8);
  SetStat(full_pairs, 6, kStatMana, 200 << 8);
  SetStat(full_pairs, 7, kStatMaxMana, 210 << 8);
  SetStat(full_pairs, 8, kStatLevel, 30);

  mem.Put32(stat_list + 0x10, 0x80000000u);
  mem.Put32(stat_list + 0x24, base_pairs);
  mem.Put16(stat_list + 0x28, 7);
  mem.Put32(stat_list + 0x48, full_pairs);
  mem.Put16(stat_list + 0x4C, 9);

  // Duas habilidades na lista (SkillList+0x04): 36 (nivel base 1, owner_guid = -1) e
  // uma de carga de item (owner_guid = 42, ignorada).
  const uint32_t sk1 = mem.Alloc(0x40);
  const uint32_t sk_charge = mem.Alloc(0x40);
  mem.Put32(skill_list + 0x04, sk1);
  mem.Put32(sk1 + 0x00, txt_36);
  mem.Put32(sk1 + 0x04, sk_charge);
  mem.PutI(sk1 + 0x28, 1);
  mem.PutI(sk1 + 0x34, -1);
  mem.Put32(sk_charge + 0x00, skills_txt + 47 * 0x23C);
  mem.Put32(sk_charge + 0x04, 0);
  mem.PutI(sk_charge + 0x28, 5);
  mem.PutI(sk_charge + 0x34, 42);  // carga de item: nao conta como base_level

  CharacterState state;
  CHECK(ReadCharacterState(mem, addr, &state));
  CHECK(state.in_game);
  CHECK_EQ(state.char_class, kClassSorceress);
  CHECK(std::strcmp(state.name, "AeonSorc") == 0);
  CHECK_EQ(state.base_strength, 40);
  CHECK_EQ(state.strength, 65);
  CHECK_EQ(state.base_vitality, 80);
  CHECK_EQ(state.vitality, 100);
  CHECK_EQ(state.stat_pts, 10);
  CHECK_EQ(state.new_skills, 5);
  CHECK_EQ(state.level, 30);
  CHECK_EQ(state.hp, 320);
  CHECK_EQ(state.max_hp, 350);
  CHECK_EQ(state.mana, 200);
  CHECK_EQ(state.max_mana, 210);
  CHECK_EQ(state.BaseSkillLevel(36), 1);
  CHECK_EQ(state.BaseSkillLevel(47), 0);

  // Plano que pede Forca 45, Destreza 30, Vitalidade 90 e Hydra (62) nivel 20.
  BuildPlan plan;
  plan.char_class = kClassSorceress;
  plan.target_strength = 45;
  plan.target_dexterity = 30;
  plan.target_vitality = 90;
  plan.overflow_stat = kStatVitality;
  plan.auto_prereqs = true;
  plan.skills.push_back(SkillStep{62, 20});

  // Atributos: base_strength (40) < 45, mesmo com strength total = 65!
  CHECK_EQ(NextAttributeToSpend(state, plan), kStatStrength);
  state.base_strength = 45;
  CHECK_EQ(NextAttributeToSpend(state, plan), kStatDexterity);
  state.base_dexterity = 30;
  CHECK_EQ(NextAttributeToSpend(state, plan), kStatVitality);
  state.base_vitality = 90;
  CHECK_EQ(NextAttributeToSpend(state, plan), kStatVitality);  // overflow

  // Habilidades: para chegar em 62 (Hydra), falta 56 (Enchant), que pede 47 (Fire Ball) e 37 (Warmth).
  // Como 36 ja esta em nivel 1, o primeiro pre-requisito elegivel de 56 e 47!
  CHECK(!CanSpendSkillPoint(mem, addr, state, 62));
  CHECK(CanSpendSkillPoint(mem, addr, state, 47));
  CHECK(!CanSpendSkillPoint(mem, addr, state, 68));  // outra classe
  CHECK_EQ(NextSkillToSpend(mem, addr, state, plan), 47);

  // Aprende 47 -> falta o segundo pre-requisito de 56 (37 Warmth).
  state.skills.push_back(LearnedSkill{47, 1});
  CHECK_EQ(NextSkillToSpend(mem, addr, state, plan), 37);

  // Aprende 37 -> agora 56 (Enchant) pode receber ponto.
  state.skills.push_back(LearnedSkill{37, 1});
  CHECK_EQ(NextSkillToSpend(mem, addr, state, plan), 56);

  // Aprende 56 -> agora 62 (Hydra, req_level 30) pode receber o 1o ponto em nivel 30!
  state.skills.push_back(LearnedSkill{56, 1});
  CHECK_EQ(NextSkillToSpend(mem, addr, state, plan), 62);

  // Depois do 1o ponto em 62 (base_level = 1), o 2o ponto exige nivel 30 + 1 = 31.
  state.skills.push_back(LearnedSkill{62, 1});
  CHECK(!CanSpendSkillPoint(mem, addr, state, 62));
  CHECK_EQ(NextSkillToSpend(mem, addr, state, plan), -1);
  state.level = 31;
  CHECK(CanSpendSkillPoint(mem, addr, state, 62));
  CHECK_EQ(NextSkillToSpend(mem, addr, state, plan), 62);

  // Captura de build a partir do estado atual.
  const BuildPlan captured = CaptureBuildFromState(state, "Snapshot");
  CHECK(captured.name == "Snapshot");
  CHECK_EQ(captured.char_class, kClassSorceress);
  CHECK_EQ(captured.target_strength, 45);
  CHECK_EQ(captured.skills.size(), 5u);
}

}  // namespace

int main() {
  TestAnchorsAgainstRealIni();
  TestJsonParseAndFormat();
  TestCharacterStateAndPlanner();
  if (g_failures == 0) {
    std::printf("build_plan_test: tudo passou\n");
    return 0;
  }
  std::printf("build_plan_test: %d falha(s)\n", g_failures);
  return 1;
}
