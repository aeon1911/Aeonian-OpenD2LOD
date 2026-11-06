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

// Logica portavel do planejador de builds (item C3 do ROADMAP e M2.2 Personagem).
// Nao depende de Win32: le o estado do personagem, tabelas de habilidades e
// planos de build (.json) atraves de MemoryView, testavel no host.
//
// Fontes: docs/specs/game-exe-1.14d-character.md,
// docs/specs/game-exe-1.14d-units.md e aeonian/addresses/1.14d.ini.

#ifndef AEONIAN_EXAMPLES_BUILD_PLANNER_BUILD_PLAN_H_
#define AEONIAN_EXAMPLES_BUILD_PLANNER_BUILD_PLAN_H_

#include <cstdint>
#include <string>
#include <vector>

namespace build_planner {

// Leitura de memoria de 32 bits do processo do jogo (ou sintetica nos testes).
class MemoryView {
 public:
  virtual ~MemoryView() = default;
  virtual bool Read(uint32_t address, void* out, uint32_t size) const = 0;
};

bool ReadU32(const MemoryView& mem, uint32_t address, uint32_t* out);
bool ReadI32(const MemoryView& mem, uint32_t address, int32_t* out);
bool ReadU16(const MemoryView& mem, uint32_t address, uint16_t* out);

// IDs de stats (game-exe-1.14d-character.md sec. 2.4).
enum StatId : int32_t {
  kStatStrength = 0,
  kStatEnergy = 1,
  kStatDexterity = 2,
  kStatVitality = 3,
  kStatPts = 4,
  kStatNewSkills = 5,
  kStatHitPoints = 6,
  kStatMaxHp = 7,
  kStatMana = 8,
  kStatMaxMana = 9,
  kStatStamina = 10,
  kStatMaxStamina = 11,
  kStatLevel = 12,
  kStatExperience = 13,
  kStatGold = 14,
  kStatGoldBank = 15,
};

// Classes de personagem (0..6, game-exe-1.14d-character.md sec. 1).
enum CharClass : int32_t {
  kClassAmazon = 0,
  kClassSorceress = 1,
  kClassNecromancer = 2,
  kClassPaladin = 3,
  kClassBarbarian = 4,
  kClassDruid = 5,
  kClassAssassin = 6,
};

const char* ClassName(int32_t char_class);
int32_t ParseClassName(const std::string& text);
const char* StatName(int32_t stat_id);

// Enderecos resolvidos a partir de aeonian/addresses/1.14d.ini.
struct CharacterAddresses {
  // Globais extraidos de ancoras verificadas.
  uint32_t player_unit = 0;   // D2Client.GetPlayerUnit (+1) -> [0x7A6A70]
  uint32_t current_act = 0;   // D2Client.DrawAutomap (+2) -> [0x7A0634]
  uint32_t game_type = 0;     // D2Client.GetGameType (+1) -> [0x7A0610]
  uint32_t data_tables = 0;   // D2Common.MonStatsRecordAccess (+2) -> [0x744304]

  // Funcoes verificadas por bytes (para conferencia e envio de pacotes).
  uint32_t get_unit_base_stat = 0;         // D2Common.GetUnitBaseStat
  uint32_t get_unit_stat = 0;              // D2Common.GetUnitStat
  uint32_t get_first_skill = 0;            // D2Common.GetFirstSkillFromSkillList
  uint32_t get_next_skill = 0;             // D2Common.GetNextSkill
  uint32_t get_skill_by_id = 0;            // D2Common.GetSkillById
  uint32_t get_skill_id = 0;               // D2Common.GetSkillId
  uint32_t get_skill_level = 0;            // D2Common.GetSkillLevel
  uint32_t get_base_skill_level_by_id = 0; // D2Common.GetBaseSkillLevelById
  uint32_t get_skill_level_by_id = 0;      // D2Common.GetSkillLevelById
  uint32_t check_skill_prereqs = 0;        // D2Common.CheckSkillPrereqs
  uint32_t check_skill_stat_reqs = 0;      // D2Common.CheckSkillStatReqs
  uint32_t get_skill_max_level = 0;        // D2Client.GetSkillMaxLevel
  uint32_t send_packet_byte_word = 0;      // D2Client.SendPacket_ByteWord
};

struct ResolveReport {
  uint32_t resolved = 0;
  uint32_t total = 0;
  std::vector<std::string> missing;
  std::vector<std::string> notes;
};

using ResolveFn = uint32_t (*)(void* context, const char* symbol_name);

std::vector<std::string> RequiredSymbols();
void ResolveAddresses(const MemoryView& anchors, ResolveFn resolve, void* context,
                      CharacterAddresses* out, ResolveReport* report);

// Habilidade aprendida na SkillList da unidade (Unit+0xA8).
struct LearnedSkill {
  int32_t skill_id = -1;
  int32_t base_level = 0;
};

// Estado completo do personagem lido da memoria do cliente.
struct CharacterState {
  bool in_game = false;
  bool game_type_known = false;
  uint32_t game_type = 0;
  uint32_t unit = 0;
  int32_t char_class = -1;
  char name[16] = {};

  // Stats base (StatListEx+0x24) e totais (StatListEx+0x48).
  int32_t base_strength = 0;
  int32_t base_energy = 0;
  int32_t base_dexterity = 0;
  int32_t base_vitality = 0;

  int32_t strength = 0;
  int32_t energy = 0;
  int32_t dexterity = 0;
  int32_t vitality = 0;

  int32_t stat_pts = 0;
  int32_t new_skills = 0;
  int32_t level = 0;
  uint32_t experience = 0;
  int32_t gold = 0;
  int32_t gold_bank = 0;

  // Vida, mana e vigor ja convertidos de ponto fixo 24.8 (>> 8).
  int32_t hp = 0;
  int32_t max_hp = 0;
  int32_t mana = 0;
  int32_t max_mana = 0;
  int32_t stamina = 0;
  int32_t max_stamina = 0;

  std::vector<LearnedSkill> skills;

  int32_t BaseStat(int32_t stat_id) const;
  int32_t TotalStat(int32_t stat_id) const;
  int32_t BaseSkillLevel(int32_t skill_id) const;
};

bool ReadCharacterState(const MemoryView& mem, const CharacterAddresses& addr,
                        CharacterState* out);

// Linha relevante de CharStatsTxt ([DataTables]+0xBC4, stride 0xC4;
// game-exe-1.14d-datatables.md sec. 3.3).
struct CharStatsDef {
  int32_t char_class = -1;
  uint8_t base_str = 0;         // +0x30
  uint8_t base_dex = 0;         // +0x31
  uint8_t base_int = 0;         // +0x32
  uint8_t base_vit = 0;         // +0x33
  int32_t stat_per_level = 0;   // +0x50
};

bool ReadCharStatsDef(const MemoryView& mem, const CharacterAddresses& addr,
                      int32_t char_class, CharStatsDef* out);

// Linha relevante de SkillsTxt ([DataTables]+0xB98, stride 0x23C; spec sec. 3.3).
struct SkillDef {
  int32_t skill_id = -1;
  int32_t char_class = -1;            // +0x00C (int8_t; < 0 se comum)
  uint16_t max_level = 20;            // +0x12C (se <= 0, vale 20)
  uint16_t req_level = 0;             // +0x174
  uint16_t req_str = 0;               // +0x176
  uint16_t req_dex = 0;               // +0x178
  uint16_t req_int = 0;               // +0x17A
  uint16_t req_vit = 0;               // +0x17C
  int16_t req_skill[3] = {-1, -1, -1};  // +0x17E..+0x182
};

bool ReadSkillDef(const MemoryView& mem, const CharacterAddresses& addr, int32_t skill_id,
                  SkillDef* out);

// Verifica na memoria se o personagem cumpre classe, nivel, atributos base e
// pre-requisitos para investir 1 ponto na habilidade `skill_id`.
bool CanSpendSkillPoint(const MemoryView& mem, const CharacterAddresses& addr,
                        const CharacterState& state, int32_t skill_id);

// Plano de build (formato proprio versionado em JSON, builds/*.json).
struct SkillStep {
  int32_t skill_id = -1;
  int32_t target_level = 1;
};

struct BuildPlan {
  int32_t version = 1;
  std::string name;
  int32_t char_class = -1;

  // Metas de atributos base (0 = sem meta acima do valor inicial).
  int32_t target_strength = 0;
  int32_t target_dexterity = 0;
  int32_t target_vitality = 0;
  int32_t target_energy = 0;
  // Atributo que recebe os pontos excedentes depois de atingir as metas (-1 = nenhum).
  int32_t overflow_stat = kStatVitality;

  // Quando true, se uma habilidade da lista exigir pre-requisito ainda em nivel 0,
  // o planejador investe 1 ponto no pre-requisito antes.
  bool auto_prereqs = true;
  std::vector<SkillStep> skills;
};

bool ParseBuildJson(const std::string& json_text, BuildPlan* out, std::string* error);
std::string FormatBuildJson(const BuildPlan& plan);
BuildPlan CaptureBuildFromState(const CharacterState& state, const std::string& build_name);

// Decide o proximo atributo (0..3) ou -1 se nao houver ponto livre / meta aplicavel.
int32_t NextAttributeToSpend(const CharacterState& state, const BuildPlan& plan);

// Decide a proxima habilidade (skill_id) ou -1 se nao houver ponto livre / elegivel.
int32_t NextSkillToSpend(const MemoryView& mem, const CharacterAddresses& addr,
                         const CharacterState& state, const BuildPlan& plan);

}  // namespace build_planner

#endif  // AEONIAN_EXAMPLES_BUILD_PLANNER_BUILD_PLAN_H_
