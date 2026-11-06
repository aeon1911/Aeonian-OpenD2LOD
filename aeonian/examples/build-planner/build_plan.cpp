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

#include "build_plan.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace build_planner {

namespace {

// Offsets de Unit e PlayerData (game-exe-1.14d-character.md sec. 1).
constexpr uint32_t kUnitType = 0x00;
constexpr uint32_t kUnitClass = 0x04;
constexpr uint32_t kUnitData = 0x14;
constexpr uint32_t kUnitStats = 0x5C;
constexpr uint32_t kUnitSkills = 0xA8;
constexpr uint32_t kTypePlayer = 0;

// Offsets de StatListEx (game-exe-1.14d-character.md sec. 2.1).
constexpr uint32_t kStatListFlags = 0x10;
constexpr uint32_t kStatListBasePairs = 0x24;
constexpr uint32_t kStatListBaseCount = 0x28;  // WORD
constexpr uint32_t kStatListFullPairs = 0x48;
constexpr uint32_t kStatListFullCount = 0x4C;  // WORD
constexpr uint32_t kStatListFlagEx = 0x80000000u;
constexpr uint32_t kStatPairSize = 8;
constexpr uint32_t kMaxStatPairs = 1024;

// Offsets de SkillList e Skill (game-exe-1.14d-character.md sec. 3.1).
constexpr uint32_t kSkillListFirst = 0x04;
constexpr uint32_t kSkillTxtPtr = 0x00;
constexpr uint32_t kSkillNext = 0x04;
constexpr uint32_t kSkillLevel = 0x28;
constexpr uint32_t kSkillOwnerGuid = 0x34;
constexpr uint32_t kSkillBlock = 0x38;
constexpr uint32_t kMaxSkills = 256;

// Offsets de DataTables, CharStatsTxt e SkillsTxt (game-exe-1.14d-datatables.md sec. 3.3,
// game-exe-1.14d-character.md sec. 3.3).
constexpr uint32_t kDataTablesCharStatsBase = 0xBC4;
constexpr uint32_t kDataTablesCharStatsCount = 0xBC8;
constexpr uint32_t kCharStatsTxtRecordSize = 0xC4;
constexpr uint32_t kCharStatsTxtBaseStr = 0x30;
constexpr uint32_t kCharStatsTxtBaseDex = 0x31;
constexpr uint32_t kCharStatsTxtBaseInt = 0x32;
constexpr uint32_t kCharStatsTxtBaseVit = 0x33;
constexpr uint32_t kCharStatsTxtStatPerLvl = 0x50;
constexpr uint32_t kMaxCharStatsTxtCount = 32;

constexpr uint32_t kDataTablesSkillsBase = 0xB98;
constexpr uint32_t kDataTablesSkillsCount = 0xBA0;
constexpr uint32_t kSkillsTxtRecordSize = 0x23C;
constexpr uint32_t kSkillsTxtId = 0x000;        // WORD
constexpr uint32_t kSkillsTxtClass = 0x00C;     // int8_t
constexpr uint32_t kSkillsTxtMaxLvl = 0x12C;    // int16_t
constexpr uint32_t kSkillsTxtReqLvl = 0x174;    // int16_t
constexpr uint32_t kSkillsTxtReqStr = 0x176;    // int16_t
constexpr uint32_t kSkillsTxtReqDex = 0x178;    // int16_t
constexpr uint32_t kSkillsTxtReqInt = 0x17A;    // int16_t
constexpr uint32_t kSkillsTxtReqVit = 0x17C;    // int16_t
constexpr uint32_t kSkillsTxtReqSkill = 0x17E;  // int16_t[3]
constexpr uint32_t kMaxSkillsTxtCount = 2048;

struct AnchorSpec {
  const char* symbol;
  uint32_t offset;
  uint32_t CharacterAddresses::*field;
};

constexpr AnchorSpec kAnchors[] = {
    {"D2Client.GetPlayerUnit", 1, &CharacterAddresses::player_unit},
    {"D2Client.DrawAutomap", 2, &CharacterAddresses::current_act},
    {"D2Client.GetGameType", 1, &CharacterAddresses::game_type},
    {"D2Common.MonStatsRecordAccess", 2, &CharacterAddresses::data_tables},
};

struct DirectSymbol {
  const char* symbol;
  uint32_t CharacterAddresses::*field;
};

constexpr DirectSymbol kDirectSymbols[] = {
    {"D2Common.GetUnitBaseStat", &CharacterAddresses::get_unit_base_stat},
    {"D2Common.GetUnitStat", &CharacterAddresses::get_unit_stat},
    {"D2Common.GetFirstSkillFromSkillList", &CharacterAddresses::get_first_skill},
    {"D2Common.GetNextSkill", &CharacterAddresses::get_next_skill},
    {"D2Common.GetSkillById", &CharacterAddresses::get_skill_by_id},
    {"D2Common.GetSkillId", &CharacterAddresses::get_skill_id},
    {"D2Common.GetSkillLevel", &CharacterAddresses::get_skill_level},
    {"D2Common.GetBaseSkillLevelById", &CharacterAddresses::get_base_skill_level_by_id},
    {"D2Common.GetSkillLevelById", &CharacterAddresses::get_skill_level_by_id},
    {"D2Common.CheckSkillPrereqs", &CharacterAddresses::check_skill_prereqs},
    {"D2Common.CheckSkillStatReqs", &CharacterAddresses::check_skill_stat_reqs},
    {"D2Client.GetSkillMaxLevel", &CharacterAddresses::get_skill_max_level},
    {"D2Client.SendPacket_ByteWord", &CharacterAddresses::send_packet_byte_word},
};

void ReadStatArray(const MemoryView& mem, uint32_t pairs_addr, uint16_t count,
                   int32_t out_stats[16]) {
  if (pairs_addr == 0 || count == 0) return;
  const uint32_t limit = std::min<uint32_t>(count, kMaxStatPairs);
  for (uint32_t i = 0; i < limit; ++i) {
    uint32_t key = 0;
    int32_t value = 0;
    const uint32_t entry = pairs_addr + i * kStatPairSize;
    if (!ReadU32(mem, entry, &key) || !ReadI32(mem, entry + 4, &value)) break;
    const uint16_t param = static_cast<uint16_t>(key & 0xFFFFu);
    const uint16_t stat_id = static_cast<uint16_t>(key >> 16);
    if (param == 0 && stat_id < 16) {
      out_stats[stat_id] = value;
    }
  }
}

void ReadSkills(const MemoryView& mem, uint32_t unit, std::vector<LearnedSkill>* out) {
  out->clear();
  uint32_t skill_list = 0;
  if (!ReadU32(mem, unit + kUnitSkills, &skill_list) || skill_list == 0) return;
  uint32_t node = 0;
  if (!ReadU32(mem, skill_list + kSkillListFirst, &node)) return;
  for (uint32_t i = 0; node != 0 && i < kMaxSkills; ++i) {
    uint8_t block[kSkillBlock];
    if (!mem.Read(node, block, sizeof(block))) break;
    uint32_t txt_ptr = 0;
    uint32_t next_ptr = 0;
    int32_t level = 0;
    int32_t owner_guid = 0;
    std::memcpy(&txt_ptr, block + kSkillTxtPtr, 4);
    std::memcpy(&next_ptr, block + kSkillNext, 4);
    std::memcpy(&level, block + kSkillLevel, 4);
    std::memcpy(&owner_guid, block + kSkillOwnerGuid, 4);
    uint16_t skill_id = 0xFFFFu;
    if (txt_ptr != 0 && ReadU16(mem, txt_ptr + kSkillsTxtId, &skill_id) &&
        skill_id != 0xFFFFu && owner_guid == -1 && level > 0) {
      out->push_back(LearnedSkill{static_cast<int32_t>(skill_id), level});
    }
    node = next_ptr;
  }
}

// --- Leitor enxuto de JSON para builds/*.json --------------------------------

struct JsonCursor {
  const std::string& text;
  size_t pos = 0;

  void SkipWs() {
    while (pos < text.size() &&
           (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\r' || text[pos] == '\n')) {
      ++pos;
    }
  }

  bool Consume(char expected) {
    SkipWs();
    if (pos < text.size() && text[pos] == expected) {
      ++pos;
      return true;
    }
    return false;
  }

  bool Peek(char expected) {
    SkipWs();
    return pos < text.size() && text[pos] == expected;
  }

  bool ParseString(std::string* out) {
    SkipWs();
    if (pos >= text.size() || text[pos] != '"') return false;
    ++pos;
    out->clear();
    while (pos < text.size()) {
      const char c = text[pos++];
      if (c == '"') return true;
      if (c == '\\') {
        if (pos >= text.size()) return false;
        const char esc = text[pos++];
        switch (esc) {
          case '"':
          case '\\':
          case '/':
            out->push_back(esc);
            break;
          case 'n':
            out->push_back('\n');
            break;
          case 'r':
            out->push_back('\r');
            break;
          case 't':
            out->push_back('\t');
            break;
          default:
            return false;
        }
      } else {
        out->push_back(c);
      }
    }
    return false;
  }

  bool ParseInt(int32_t* out) {
    SkipWs();
    if (pos >= text.size()) return false;
    size_t start = pos;
    if (text[pos] == '-') ++pos;
    if (pos >= text.size() || !std::isdigit(static_cast<unsigned char>(text[pos]))) {
      pos = start;
      return false;
    }
    while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) {
      ++pos;
    }
    const long val = std::strtol(text.c_str() + start, nullptr, 10);
    *out = static_cast<int32_t>(val);
    return true;
  }

  bool ParseBool(bool* out) {
    SkipWs();
    if (text.compare(pos, 4, "true") == 0) {
      pos += 4;
      *out = true;
      return true;
    }
    if (text.compare(pos, 5, "false") == 0) {
      pos += 5;
      *out = false;
      return true;
    }
    return false;
  }
};

int32_t ParseStatIdValue(JsonCursor* cur, bool* ok) {
  *ok = false;
  cur->SkipWs();
  if (cur->Peek('"')) {
    std::string s;
    if (!cur->ParseString(&s)) return -1;
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    *ok = true;
    if (s == "strength" || s == "str") return kStatStrength;
    if (s == "energy" || s == "ene" || s == "int") return kStatEnergy;
    if (s == "dexterity" || s == "dex") return kStatDexterity;
    if (s == "vitality" || s == "vit") return kStatVitality;
    if (s == "none" || s == "") return -1;
    *ok = false;
    return -1;
  }
  int32_t id = -1;
  if (cur->ParseInt(&id) && id >= -1 && id <= 3) {
    *ok = true;
    return id;
  }
  return -1;
}

bool ParseAttributesObject(JsonCursor* cur, BuildPlan* plan, std::string* error) {
  if (!cur->Consume('{')) {
    *error = "attributes deve ser um objeto JSON";
    return false;
  }
  if (cur->Consume('}')) return true;
  for (;;) {
    std::string key;
    if (!cur->ParseString(&key) || !cur->Consume(':')) {
      *error = "chave invalida em attributes";
      return false;
    }
    if (key == "strength" || key == "str") {
      if (!cur->ParseInt(&plan->target_strength) || plan->target_strength < 0) {
        *error = "valor invalido para strength";
        return false;
      }
    } else if (key == "dexterity" || key == "dex") {
      if (!cur->ParseInt(&plan->target_dexterity) || plan->target_dexterity < 0) {
        *error = "valor invalido para dexterity";
        return false;
      }
    } else if (key == "vitality" || key == "vit") {
      if (!cur->ParseInt(&plan->target_vitality) || plan->target_vitality < 0) {
        *error = "valor invalido para vitality";
        return false;
      }
    } else if (key == "energy" || key == "ene") {
      if (!cur->ParseInt(&plan->target_energy) || plan->target_energy < 0) {
        *error = "valor invalido para energy";
        return false;
      }
    } else if (key == "overflow") {
      bool ok = false;
      plan->overflow_stat = ParseStatIdValue(cur, &ok);
      if (!ok) {
        *error = "valor invalido para overflow em attributes";
        return false;
      }
    } else {
      *error = "campo desconhecido em attributes: " + key;
      return false;
    }
    if (cur->Consume('}')) return true;
    if (!cur->Consume(',')) {
      *error = "esperado ',' ou '}' em attributes";
      return false;
    }
  }
}

bool ParseSkillStepObject(JsonCursor* cur, SkillStep* step, std::string* error) {
  *step = SkillStep{};
  if (!cur->Consume('{')) {
    *error = "item de skills deve ser um objeto JSON";
    return false;
  }
  if (cur->Consume('}')) {
    *error = "item de skills vazio";
    return false;
  }
  for (;;) {
    std::string key;
    if (!cur->ParseString(&key) || !cur->Consume(':')) {
      *error = "chave invalida em item de skills";
      return false;
    }
    if (key == "id" || key == "skill_id") {
      if (!cur->ParseInt(&step->skill_id) || step->skill_id < 0) {
        *error = "id de habilidade invalido";
        return false;
      }
    } else if (key == "level" || key == "target_level") {
      if (!cur->ParseInt(&step->target_level) || step->target_level <= 0 ||
          step->target_level > 99) {
        *error = "nivel alvo de habilidade invalido";
        return false;
      }
    } else {
      *error = "campo desconhecido em item de skills: " + key;
      return false;
    }
    if (cur->Consume('}')) break;
    if (!cur->Consume(',')) {
      *error = "esperado ',' ou '}' em item de skills";
      return false;
    }
  }
  if (step->skill_id < 0) {
    *error = "item de skills sem campo 'id'";
    return false;
  }
  return true;
}

bool ParseSkillsArray(JsonCursor* cur, std::vector<SkillStep>* skills, std::string* error) {
  skills->clear();
  if (!cur->Consume('[')) {
    *error = "skills deve ser uma lista JSON";
    return false;
  }
  if (cur->Consume(']')) return true;
  for (;;) {
    SkillStep step;
    if (!ParseSkillStepObject(cur, &step, error)) return false;
    skills->push_back(step);
    if (cur->Consume(']')) return true;
    if (!cur->Consume(',')) {
      *error = "esperado ',' ou ']' em skills";
      return false;
    }
  }
}

std::string EscapeJsonString(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if (c == '"' || c == '\\') out.push_back('\\');
    out.push_back(c);
  }
  return out;
}

// Procura recursivamente um pre-requisito ainda nao aprendido (base_level == 0)
// que ja possa receber ponto agora. Limita a profundidade a 6 para evitar ciclos.
int32_t FindSpendablePrerequisite(const MemoryView& mem, const CharacterAddresses& addr,
                                  const CharacterState& state, int32_t skill_id, int depth) {
  if (depth <= 0 || skill_id < 0) return -1;
  SkillDef def;
  if (!ReadSkillDef(mem, addr, skill_id, &def)) return -1;
  for (int i = 0; i < 3; ++i) {
    const int32_t req = def.req_skill[i];
    if (req < 0) continue;
    if (state.BaseSkillLevel(req) > 0) continue;
    if (CanSpendSkillPoint(mem, addr, state, req)) return req;
    const int32_t deeper = FindSpendablePrerequisite(mem, addr, state, req, depth - 1);
    if (deeper >= 0) return deeper;
  }
  return -1;
}

}  // namespace

bool ReadU32(const MemoryView& mem, uint32_t address, uint32_t* out) {
  return address != 0 && mem.Read(address, out, sizeof(*out));
}

bool ReadI32(const MemoryView& mem, uint32_t address, int32_t* out) {
  return address != 0 && mem.Read(address, out, sizeof(*out));
}

bool ReadU16(const MemoryView& mem, uint32_t address, uint16_t* out) {
  return address != 0 && mem.Read(address, out, sizeof(*out));
}

const char* ClassName(int32_t char_class) {
  switch (char_class) {
    case kClassAmazon:
      return "amazon";
    case kClassSorceress:
      return "sorceress";
    case kClassNecromancer:
      return "necromancer";
    case kClassPaladin:
      return "paladin";
    case kClassBarbarian:
      return "barbarian";
    case kClassDruid:
      return "druid";
    case kClassAssassin:
      return "assassin";
    default:
      return "unknown";
  }
}

int32_t ParseClassName(const std::string& text) {
  std::string lower = text;
  for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (lower == "amazon" || lower == "ama" || lower == "0") return kClassAmazon;
  if (lower == "sorceress" || lower == "sor" || lower == "1") return kClassSorceress;
  if (lower == "necromancer" || lower == "nec" || lower == "2") return kClassNecromancer;
  if (lower == "paladin" || lower == "pal" || lower == "3") return kClassPaladin;
  if (lower == "barbarian" || lower == "bar" || lower == "4") return kClassBarbarian;
  if (lower == "druid" || lower == "dru" || lower == "5") return kClassDruid;
  if (lower == "assassin" || lower == "ass" || lower == "6") return kClassAssassin;
  return -1;
}

const char* StatName(int32_t stat_id) {
  switch (stat_id) {
    case kStatStrength:
      return "strength";
    case kStatEnergy:
      return "energy";
    case kStatDexterity:
      return "dexterity";
    case kStatVitality:
      return "vitality";
    default:
      return "none";
  }
}

std::vector<std::string> RequiredSymbols() {
  std::vector<std::string> names;
  for (const AnchorSpec& a : kAnchors) {
    names.emplace_back(a.symbol);
  }
  for (const DirectSymbol& d : kDirectSymbols) {
    names.emplace_back(d.symbol);
  }
  return names;
}

void ResolveAddresses(const MemoryView& anchors, ResolveFn resolve, void* context,
                      CharacterAddresses* out, ResolveReport* report) {
  *out = CharacterAddresses{};
  *report = ResolveReport{};
  const std::vector<std::string> names = RequiredSymbols();
  report->total = static_cast<uint32_t>(names.size());
  for (const AnchorSpec& a : kAnchors) {
    const uint32_t base = resolve(context, a.symbol);
    if (base == 0) {
      report->missing.emplace_back(a.symbol);
      continue;
    }
    ++report->resolved;
    uint32_t value = 0;
    if (!ReadU32(anchors, base + a.offset, &value) || value == 0) {
      report->notes.push_back(std::string(a.symbol) + ": ancora ilegivel");
      continue;
    }
    out->*(a.field) = value;
  }
  for (const DirectSymbol& d : kDirectSymbols) {
    const uint32_t base = resolve(context, d.symbol);
    if (base == 0) {
      report->missing.emplace_back(d.symbol);
      continue;
    }
    ++report->resolved;
    out->*(d.field) = base;
  }
}

int32_t CharacterState::BaseStat(int32_t stat_id) const {
  switch (stat_id) {
    case kStatStrength:
      return base_strength;
    case kStatEnergy:
      return base_energy;
    case kStatDexterity:
      return base_dexterity;
    case kStatVitality:
      return base_vitality;
    case kStatPts:
      return stat_pts;
    case kStatNewSkills:
      return new_skills;
    case kStatLevel:
      return level;
    default:
      return 0;
  }
}

int32_t CharacterState::TotalStat(int32_t stat_id) const {
  switch (stat_id) {
    case kStatStrength:
      return strength;
    case kStatEnergy:
      return energy;
    case kStatDexterity:
      return dexterity;
    case kStatVitality:
      return vitality;
    case kStatPts:
      return stat_pts;
    case kStatNewSkills:
      return new_skills;
    case kStatLevel:
      return level;
    default:
      return 0;
  }
}

int32_t CharacterState::BaseSkillLevel(int32_t skill_id) const {
  for (const LearnedSkill& s : skills) {
    if (s.skill_id == skill_id) return s.base_level;
  }
  return 0;
}

bool ReadCharacterState(const MemoryView& mem, const CharacterAddresses& addr,
                        CharacterState* out) {
  *out = CharacterState{};
  uint32_t player = 0;
  if (!ReadU32(mem, addr.player_unit, &player) || player == 0) return false;
  if (addr.current_act != 0) {
    uint32_t act = 0;
    if (!ReadU32(mem, addr.current_act, &act) || act == 0) return false;
  }
  uint32_t type = 0xFFFFFFFFu;
  int32_t char_class = -1;
  uint32_t player_data = 0;
  uint32_t stat_list = 0;
  if (!ReadU32(mem, player + kUnitType, &type) || type != kTypePlayer ||
      !ReadI32(mem, player + kUnitClass, &char_class) || char_class < 0 || char_class > 6 ||
      !ReadU32(mem, player + kUnitData, &player_data) ||
      !ReadU32(mem, player + kUnitStats, &stat_list) || stat_list == 0) {
    return false;
  }
  out->in_game = true;
  out->unit = player;
  out->char_class = char_class;
  out->game_type_known = ReadU32(mem, addr.game_type, &out->game_type);
  if (player_data != 0) {
    char raw_name[16] = {};
    if (mem.Read(player_data, raw_name, sizeof(raw_name))) {
      raw_name[15] = '\0';
      std::memcpy(out->name, raw_name, sizeof(out->name));
    }
  }

  uint32_t flags = 0;
  uint32_t base_pairs = 0;
  uint16_t base_count = 0;
  if (!ReadU32(mem, stat_list + kStatListFlags, &flags) ||
      !ReadU32(mem, stat_list + kStatListBasePairs, &base_pairs) ||
      !ReadU16(mem, stat_list + kStatListBaseCount, &base_count)) {
    return false;
  }
  int32_t base_stats[16] = {};
  ReadStatArray(mem, base_pairs, base_count, base_stats);
  int32_t full_stats[16] = {};
  std::memcpy(full_stats, base_stats, sizeof(full_stats));
  if ((flags & kStatListFlagEx) != 0) {
    uint32_t full_pairs = 0;
    uint16_t full_count = 0;
    if (ReadU32(mem, stat_list + kStatListFullPairs, &full_pairs) &&
        ReadU16(mem, stat_list + kStatListFullCount, &full_count)) {
      ReadStatArray(mem, full_pairs, full_count, full_stats);
    }
  }

  out->base_strength = base_stats[kStatStrength];
  out->base_energy = base_stats[kStatEnergy];
  out->base_dexterity = base_stats[kStatDexterity];
  out->base_vitality = base_stats[kStatVitality];

  out->strength = full_stats[kStatStrength];
  out->energy = full_stats[kStatEnergy];
  out->dexterity = full_stats[kStatDexterity];
  out->vitality = full_stats[kStatVitality];

  out->stat_pts = base_stats[kStatPts];
  out->new_skills = base_stats[kStatNewSkills];
  out->level = base_stats[kStatLevel] > 0 ? base_stats[kStatLevel] : full_stats[kStatLevel];
  out->experience = static_cast<uint32_t>(base_stats[kStatExperience]);
  out->gold = base_stats[kStatGold];
  out->gold_bank = base_stats[kStatGoldBank];

  out->hp = full_stats[kStatHitPoints] >> 8;
  out->max_hp = full_stats[kStatMaxHp] >> 8;
  out->mana = full_stats[kStatMana] >> 8;
  out->max_mana = full_stats[kStatMaxMana] >> 8;
  out->stamina = full_stats[kStatStamina] >> 8;
  out->max_stamina = full_stats[kStatMaxStamina] >> 8;

  ReadSkills(mem, player, &out->skills);
  return true;
}

bool ReadCharStatsDef(const MemoryView& mem, const CharacterAddresses& addr,
                      int32_t char_class, CharStatsDef* out) {
  *out = CharStatsDef{};
  if (char_class < 0 || addr.data_tables == 0) return false;
  uint32_t tables = 0;
  uint32_t base = 0;
  uint32_t count = 0;
  if (!ReadU32(mem, addr.data_tables, &tables) || tables == 0 ||
      !ReadU32(mem, tables + kDataTablesCharStatsBase, &base) || base == 0 ||
      !ReadU32(mem, tables + kDataTablesCharStatsCount, &count) ||
      count == 0 || count > kMaxCharStatsTxtCount ||
      static_cast<uint32_t>(char_class) >= count) {
    return false;
  }
  const uint32_t rec =
      base + static_cast<uint32_t>(char_class) * kCharStatsTxtRecordSize;
  if (!mem.Read(rec + kCharStatsTxtBaseStr, &out->base_str, 1) ||
      !mem.Read(rec + kCharStatsTxtBaseDex, &out->base_dex, 1) ||
      !mem.Read(rec + kCharStatsTxtBaseInt, &out->base_int, 1) ||
      !mem.Read(rec + kCharStatsTxtBaseVit, &out->base_vit, 1) ||
      !ReadI32(mem, rec + kCharStatsTxtStatPerLvl, &out->stat_per_level)) {
    return false;
  }
  out->char_class = char_class;
  return true;
}

bool ReadSkillDef(const MemoryView& mem, const CharacterAddresses& addr, int32_t skill_id,
                  SkillDef* out) {
  *out = SkillDef{};
  if (skill_id < 0 || addr.data_tables == 0) return false;
  uint32_t tables = 0;
  uint32_t base = 0;
  uint32_t count = 0;
  if (!ReadU32(mem, addr.data_tables, &tables) || tables == 0 ||
      !ReadU32(mem, tables + kDataTablesSkillsBase, &base) || base == 0 ||
      !ReadU32(mem, tables + kDataTablesSkillsCount, &count) ||
      count > kMaxSkillsTxtCount || static_cast<uint32_t>(skill_id) >= count) {
    return false;
  }
  const uint32_t rec = base + static_cast<uint32_t>(skill_id) * kSkillsTxtRecordSize;
  uint16_t rec_id = 0;
  int8_t char_class = -1;
  int16_t max_lvl = 0;
  if (!ReadU16(mem, rec + kSkillsTxtId, &rec_id) ||
      !mem.Read(rec + kSkillsTxtClass, &char_class, 1) ||
      !mem.Read(rec + kSkillsTxtMaxLvl, &max_lvl, 2) ||
      !ReadU16(mem, rec + kSkillsTxtReqLvl, &out->req_level) ||
      !ReadU16(mem, rec + kSkillsTxtReqStr, &out->req_str) ||
      !ReadU16(mem, rec + kSkillsTxtReqDex, &out->req_dex) ||
      !ReadU16(mem, rec + kSkillsTxtReqInt, &out->req_int) ||
      !ReadU16(mem, rec + kSkillsTxtReqVit, &out->req_vit) ||
      !mem.Read(rec + kSkillsTxtReqSkill, out->req_skill, sizeof(out->req_skill))) {
    return false;
  }
  out->skill_id = static_cast<int32_t>(rec_id);
  out->char_class = static_cast<int32_t>(char_class);
  out->max_level = max_lvl > 0 ? static_cast<uint16_t>(max_lvl) : 20u;
  return true;
}

bool CanSpendSkillPoint(const MemoryView& mem, const CharacterAddresses& addr,
                        const CharacterState& state, int32_t skill_id) {
  if (!state.in_game || state.new_skills <= 0 || skill_id < 0) return false;
  SkillDef def;
  if (!ReadSkillDef(mem, addr, skill_id, &def)) return false;
  if (def.char_class != state.char_class) return false;
  const int32_t current_base = state.BaseSkillLevel(skill_id);
  if (current_base >= static_cast<int32_t>(def.max_level)) return false;
  // Nivel exigido do personagem cresce com o nivel base atual da habilidade:
  // para colocar o (current_base + 1)-esimo ponto, o personagem precisa ter
  // pelo menos req_level + current_base (D2Common.CheckSkillPrereqs, 0x6447D0).
  const int32_t needed_char_level = static_cast<int32_t>(def.req_level) + current_base;
  if (state.level < needed_char_level) return false;
  // Requisitos de atributos em SkillsTxt sao conferidos contra o valor efetivo
  // (D2Common.CheckSkillStatReqs, 0x644920).
  if (state.strength < static_cast<int32_t>(def.req_str) ||
      state.dexterity < static_cast<int32_t>(def.req_dex) ||
      state.energy < static_cast<int32_t>(def.req_int) ||
      state.vitality < static_cast<int32_t>(def.req_vit)) {
    return false;
  }
  // Pre-requisitos em SkillsTxt+0x17E..+0x182 (D2Common.CheckSkillPrereqs, 0x6447D0).
  for (int i = 0; i < 3; ++i) {
    if (def.req_skill[i] >= 0 && state.BaseSkillLevel(def.req_skill[i]) < 1) {
      return false;
    }
  }
  return true;
}

bool ParseBuildJson(const std::string& json_text, BuildPlan* out, std::string* error) {
  *out = BuildPlan{};
  if (error != nullptr) error->clear();
  std::string local_err;
  std::string* err = error != nullptr ? error : &local_err;

  JsonCursor cur{json_text, 0};
  if (!cur.Consume('{')) {
    *err = "esperado '{' no inicio do JSON";
    return false;
  }
  if (cur.Consume('}')) {
    *err = "plano de build vazio";
    return false;
  }
  for (;;) {
    std::string key;
    if (!cur.ParseString(&key) || !cur.Consume(':')) {
      *err = "chave invalida na raiz do JSON";
      return false;
    }
    if (key == "version") {
      if (!cur.ParseInt(&out->version) || out->version != 1) {
        *err = "versao de plano nao suportada (esperado 1)";
        return false;
      }
    } else if (key == "name") {
      if (!cur.ParseString(&out->name) || out->name.empty()) {
        *err = "nome da build invalido";
        return false;
      }
    } else if (key == "class" || key == "char_class") {
      cur.SkipWs();
      if (cur.Peek('"')) {
        std::string cls;
        if (!cur.ParseString(&cls)) {
          *err = "classe invalida";
          return false;
        }
        out->char_class = ParseClassName(cls);
      } else {
        int32_t cls_id = -1;
        if (cur.ParseInt(&cls_id) && cls_id >= 0 && cls_id <= 6) {
          out->char_class = cls_id;
        }
      }
      if (out->char_class < 0 || out->char_class > 6) {
        *err = "classe de personagem desconhecida";
        return false;
      }
    } else if (key == "auto_prereqs") {
      if (!cur.ParseBool(&out->auto_prereqs)) {
        *err = "valor booleano invalido em auto_prereqs";
        return false;
      }
    } else if (key == "attributes") {
      if (!ParseAttributesObject(&cur, out, err)) return false;
    } else if (key == "skills") {
      if (!ParseSkillsArray(&cur, &out->skills, err)) return false;
    } else {
      *err = "campo desconhecido na raiz do JSON: " + key;
      return false;
    }
    if (cur.Consume('}')) break;
    if (!cur.Consume(',')) {
      *err = "esperado ',' ou '}' na raiz do JSON";
      return false;
    }
  }
  cur.SkipWs();
  if (cur.pos != json_text.size()) {
    *err = "conteudo extra apos o fim do JSON";
    return false;
  }
  if (out->name.empty()) {
    *err = "campo obrigatorio 'name' ausente";
    return false;
  }
  if (out->char_class < 0 || out->char_class > 6) {
    *err = "campo obrigatorio 'class' ausente";
    return false;
  }
  return true;
}

std::string FormatBuildJson(const BuildPlan& plan) {
  std::string out = "{\n";
  out += "  \"version\": " + std::to_string(plan.version) + ",\n";
  out += "  \"name\": \"" + EscapeJsonString(plan.name) + "\",\n";
  out += "  \"class\": \"" + std::string(ClassName(plan.char_class)) + "\",\n";
  out += "  \"auto_prereqs\": " + std::string(plan.auto_prereqs ? "true" : "false") + ",\n";
  out += "  \"attributes\": {\n";
  out += "    \"strength\": " + std::to_string(plan.target_strength) + ",\n";
  out += "    \"dexterity\": " + std::to_string(plan.target_dexterity) + ",\n";
  out += "    \"vitality\": " + std::to_string(plan.target_vitality) + ",\n";
  out += "    \"energy\": " + std::to_string(plan.target_energy) + ",\n";
  out += "    \"overflow\": \"" + std::string(StatName(plan.overflow_stat)) + "\"\n";
  out += "  },\n";
  out += "  \"skills\": [\n";
  for (size_t i = 0; i < plan.skills.size(); ++i) {
    const SkillStep& s = plan.skills[i];
    out += "    { \"id\": " + std::to_string(s.skill_id) +
           ", \"level\": " + std::to_string(s.target_level) + " }";
    if (i + 1 < plan.skills.size()) out += ",";
    out += "\n";
  }
  out += "  ]\n";
  out += "}\n";
  return out;
}

BuildPlan CaptureBuildFromState(const CharacterState& state, const std::string& build_name) {
  BuildPlan plan;
  plan.version = 1;
  plan.name = !build_name.empty() ? build_name : (state.name[0] != '\0' ? state.name : "build");
  plan.char_class = state.char_class;
  plan.target_strength = state.base_strength;
  plan.target_dexterity = state.base_dexterity;
  plan.target_vitality = state.base_vitality;
  plan.target_energy = state.base_energy;
  plan.overflow_stat = kStatVitality;
  plan.auto_prereqs = true;
  for (const LearnedSkill& s : state.skills) {
    if (s.skill_id >= 0 && s.base_level > 0) {
      plan.skills.push_back(SkillStep{s.skill_id, s.base_level});
    }
  }
  std::sort(plan.skills.begin(), plan.skills.end(),
            [](const SkillStep& a, const SkillStep& b) { return a.skill_id < b.skill_id; });
  return plan;
}

int32_t NextAttributeToSpend(const CharacterState& state, const BuildPlan& plan) {
  if (!state.in_game || state.stat_pts <= 0) return -1;
  if (plan.char_class >= 0 && plan.char_class != state.char_class) return -1;
  if (state.base_strength < plan.target_strength) return kStatStrength;
  if (state.base_dexterity < plan.target_dexterity) return kStatDexterity;
  if (state.base_vitality < plan.target_vitality) return kStatVitality;
  if (state.base_energy < plan.target_energy) return kStatEnergy;
  if (plan.overflow_stat >= kStatStrength && plan.overflow_stat <= kStatVitality) {
    return plan.overflow_stat;
  }
  return -1;
}

int32_t NextSkillToSpend(const MemoryView& mem, const CharacterAddresses& addr,
                         const CharacterState& state, const BuildPlan& plan) {
  if (!state.in_game || state.new_skills <= 0) return -1;
  if (plan.char_class >= 0 && plan.char_class != state.char_class) return -1;
  for (const SkillStep& step : plan.skills) {
    if (step.skill_id < 0 || step.target_level <= 0) continue;
    const int32_t current = state.BaseSkillLevel(step.skill_id);
    if (current >= step.target_level) continue;
    if (CanSpendSkillPoint(mem, addr, state, step.skill_id)) {
      return step.skill_id;
    }
    if (plan.auto_prereqs) {
      const int32_t prereq = FindSpendablePrerequisite(mem, addr, state, step.skill_id, 6);
      if (prereq >= 0) return prereq;
    }
  }
  return -1;
}

}  // namespace build_planner
