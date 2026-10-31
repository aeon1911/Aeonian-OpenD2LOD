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

#include "map_logic.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace map_overlay {

namespace {

// Layout das estruturas do cliente. Cada constante cita a secao da spec de
// onde veio: U = game-exe-1.14d-units.md, L = game-exe-1.14d-levels-automap.md.

// Unidade (U sec. 3).
constexpr uint32_t kUnitType = 0x00;
constexpr uint32_t kUnitClass = 0x04;
constexpr uint32_t kUnitId = 0x0C;
constexpr uint32_t kUnitMode = 0x10;
constexpr uint32_t kUnitData = 0x14;
constexpr uint32_t kUnitActIndex = 0x18;
constexpr uint32_t kUnitPath = 0x2C;
constexpr uint32_t kUnitStats = 0x5C;
constexpr uint32_t kUnitNextInBucket = 0xE4;
constexpr uint32_t kUnitBlock = 0x30;

// Tipos de unidade (U sec. 2.1).
constexpr uint32_t kTypePlayer = 0;
constexpr uint32_t kTypeMonster = 1;
constexpr uint32_t kTypeObject = 2;
constexpr uint32_t kTypeTile = 5;

// Caminho dinamico: jogador, monstro (U sec. 3.1).
constexpr uint32_t kDynSubtileX = 0x02;  // WORD
constexpr uint32_t kDynSubtileY = 0x06;  // WORD
constexpr uint32_t kDynClientX = 0x08;
constexpr uint32_t kDynClientY = 0x0C;
constexpr uint32_t kDynRoom1 = 0x1C;
constexpr uint32_t kDynBlock = 0x20;

// Caminho estatico: objeto, tile (U sec. 3.1).
constexpr uint32_t kStaticRoom1 = 0x00;
constexpr uint32_t kStaticClientX = 0x04;
constexpr uint32_t kStaticClientY = 0x08;
constexpr uint32_t kStaticBlock = 0x0C;

// Tabela de unidades: 6 tipos x 128 baldes de Unit* (U sec. 2).
constexpr uint32_t kUnitTableTypeStride = 0x200;
constexpr uint32_t kUnitTableBuckets = 128;

// MonsterData e monstats (U sec. 4).
constexpr uint32_t kMonsterTypeFlags = 0x16;  // BYTE
constexpr uint32_t kDataTablesMonStatsBase = 0xA78;
constexpr uint32_t kDataTablesMonStatsCount = 0xA80;
constexpr uint32_t kMonStatsRecordSize = 0x1A8;
constexpr uint32_t kMonStatsFlags = 0x0C;

// Modos do monstro: 0 morrendo, 12 cadaver (U sec. 3.2).
constexpr uint32_t kModeDying = 0;
constexpr uint32_t kModeDead = 12;
// monstats+0x0C: 0x100 so nos NPCs, 0x8000 nos monstros hostis; 0x400 nas 13 classes
// vistas vivas, todas amigas, e nenhuma hostil (U sec. 4).
constexpr uint32_t kMonStatsNpc = 0x100;
constexpr uint32_t kMonStatsInTown = 0x400;
constexpr uint32_t kMonStatsKillable = 0x8000;
// MonsterData+0x16 (U sec. 4).
constexpr uint8_t kMonsterSuperUnique = 0x02;
constexpr uint8_t kMonsterChampion = 0x04;
constexpr uint8_t kMonsterUnique = 0x08;
constexpr uint8_t kMonsterMinion = 0x10;

// Stats da unidade e alinhamento (U sec. 7.2).
constexpr uint32_t kStatsFirstSubList = 0x3C;
constexpr uint32_t kSubListState = 0x14;
constexpr uint32_t kSubListPairs = 0x24;
constexpr uint32_t kSubListPairCount = 0x28;  // WORD
constexpr uint32_t kSubListNext = 0x2C;
constexpr uint32_t kSubListBlock = 0x2C + 4;
constexpr uint32_t kStatPairSize = 8;  // (stat << 16, valor)
constexpr uint32_t kStateAlignment = 105;
constexpr uint32_t kStatAlignment = 172;
constexpr int32_t kAlignmentFriendly = 2;
constexpr uint32_t kMaxSubLists = 256;
constexpr uint32_t kMaxStatPairs = 1024;

// Lista de aliados (U sec. 7.1).
constexpr uint32_t kAllyType = 0x04;
constexpr uint32_t kAllyId = 0x08;
constexpr uint32_t kAllyOwner = 0x0C;
constexpr uint32_t kAllyNext = 0x30;
constexpr uint32_t kAllyBlock = 0x34;
constexpr uint32_t kMaxAllies = 512;

// Registro de objects (U sec. 5.1) e ObjectData (U sec. 5.2).
constexpr uint32_t kObjectRecordSize = 0x1C0;
constexpr uint32_t kObjectSubClass = 0x167;
constexpr uint32_t kObjectOperateFn = 0x1B3;
constexpr uint32_t kObjectNameSize = 64;  // char[64] em +0x000
constexpr uint8_t kSubClassPlayerOrRedPortal = 0x04;
constexpr uint8_t kSubClassLevelPortal = 0x10;
constexpr uint8_t kSubClassWaypoint = 0x40;
constexpr uint32_t kObjectClassBluePortal = 59;
constexpr uint32_t kObjectClassRedPortal = 60;
constexpr uint32_t kObjectDataDestLevel = 0x04;  // BYTE
// Classes ligadas a missao fora do conjunto de OperateFn (U sec. 5.4): a predefinida
// que marca o lugar da 558 e as gaiolas.
constexpr uint32_t kObjectClassQuestPlaceholder = 460;
constexpr uint32_t kObjectClassQuestUnit = 558;
constexpr uint32_t kObjectClassQuestCage = 473;
// Portais de destino fixo pelo nivel atual (U sec. 5.3).
constexpr uint32_t kObjectClassArcanePortal = 298;
constexpr uint32_t kObjectClassDurancePortal = 342;

// Estado de UI e painel (L sec. 5.4).
constexpr uint32_t kUiNpcDialog = 8;
constexpr uint32_t kUiEscMenu = 9;
constexpr uint32_t kUiHelp = 33;
constexpr int32_t kPanelRight = 1;
constexpr int32_t kPanelLeft = 2;
constexpr int32_t kPanelBoth = 3;

// Sala ativa e colisao (L sec. 2.5).
constexpr uint32_t kRoom1Room2 = 0x10;
constexpr uint32_t kRoom1Collision = 0x20;
constexpr uint32_t kRoom1Act = 0x2C;
constexpr uint32_t kCollisionX = 0x00;  // X, Y, largura, altura em subtiles
constexpr uint32_t kCollisionCells = 0x20;
constexpr uint32_t kCollisionBlock = 0x24;
constexpr uint16_t kCollisionBlocksWalk = 0x0001;
// A menor passagem medida tem 7 subtiles (L sec. 4.1). Frestas menores aparecem na
// fronteira, como uma de 2 na cidade do Ato I, e nao contam como passagem.
constexpr int32_t kMinPassageCells = 7;

// Sala do DRLG (L sec. 2.4).
constexpr uint32_t kRoom2LoadCount = 0x0E;  // WORD
constexpr uint32_t kRoom2TypeData = 0x20;
constexpr uint32_t kRoom2Next = 0x24;
constexpr uint32_t kRoom2Flags = 0x28;
constexpr uint32_t kRoom2Room1 = 0x30;
constexpr uint32_t kRoom2TileX = 0x34;
constexpr uint32_t kRoom2TileY = 0x38;
constexpr uint32_t kRoom2TileW = 0x3C;
constexpr uint32_t kRoom2TileH = 0x40;
constexpr uint32_t kRoom2Level = 0x58;
constexpr uint32_t kRoom2Presets = 0x5C;
constexpr uint32_t kRoom2Block = 0x60;
constexpr uint32_t kRoom2WarpBit = 0x10;  // bit 0x10 << n = contem o warp n
constexpr uint32_t kRoom2WaypointMask = 0x30000;

// Dados da sala em nivel externo e a celula do gerador (L sec. 4.1).
constexpr uint32_t kTypeDataGeneratorCell = 0x08;
constexpr uint32_t kGeneratorCellVariant = 0x04;
constexpr int32_t kAct1PassageVariant = 3;
constexpr int32_t kAct1RuleFirstCell = 7;
constexpr int32_t kAct1RuleCells = 28;  // +7 a +34

// Nivel (L sec. 2.3).
constexpr uint32_t kLevelDrlgType = 0x00;
constexpr uint32_t kLevelTypeExternal = 3;
constexpr uint32_t kLevelFirstRoom2 = 0x10;
constexpr uint32_t kLevelTileX = 0x1C;  // X, Y, largura, altura; validos antes da geracao
constexpr uint32_t kLevelNext = 0x1AC;
constexpr uint32_t kLevelDrlgAct = 0x1B4;
constexpr uint32_t kLevelId = 0x1D0;

// DrlgAct e a lista de excecoes de vizinhanca (L sec. 2.2).
constexpr uint32_t kDrlgActExceptions = 0x90;
constexpr uint32_t kDrlgActBackToAct = 0x46C;
constexpr uint32_t kDrlgActFirstLevel = 0x47C;
constexpr uint32_t kExceptionId = 0x00;
constexpr uint32_t kExceptionVis = 0x04;
constexpr uint32_t kExceptionWarp = 0x24;
constexpr uint32_t kExceptionNext = 0x44;

// Act (L sec. 2.1).
constexpr uint32_t kActSeed = 0x0C;
constexpr uint32_t kActIndex = 0x14;
constexpr uint32_t kActDrlgAct = 0x48;

// LevelDefs (L sec. 2.7).
constexpr uint32_t kDataTablesLevelDefsCount = 0xC5C;
constexpr uint32_t kLevelDefsRecordSize = 0x9C;
constexpr uint32_t kLevelDefsVis = 0x48;
constexpr uint32_t kLevelDefsWarp = 0x68;

// Unidade predefinida (L sec. 2.6).
constexpr uint32_t kPresetClass = 0x04;
constexpr uint32_t kPresetX = 0x08;
constexpr uint32_t kPresetNext = 0x0C;
constexpr uint32_t kPresetType = 0x14;
constexpr uint32_t kPresetY = 0x18;
constexpr uint32_t kPresetBlock = 0x20;

// Limites contra memoria corrompida (nenhum laco depende so dos ponteiros).
constexpr uint32_t kMaxRooms = 4096;
constexpr uint32_t kMaxPresetsPerRoom = 512;
constexpr uint32_t kMaxUnitsPerType = 4096;
constexpr uint32_t kMaxExceptions = 512;
constexpr uint32_t kMaxLevels = 256;
constexpr int32_t kMaxLevelId = 1024;
constexpr int32_t kLevelDefsSeen = 137;  // linhas vistas em LevelDefs (L sec. 2.7)
constexpr int32_t kMaxRoomTiles = 4096;

// Duas marcas estaticas do mesmo tipo a menos disto (pixels de cliente) sao a
// mesma coisa (a predefinida e a unidade criada a partir dela).
constexpr int32_t kSameSpotX = 48;
constexpr int32_t kSameSpotY = 24;

// Ajuste da cruz do jogo no modo minimapa (L sec. 5.2).
constexpr int32_t kMiniMarkerDx = -1;
constexpr int32_t kMiniMarkerDy = 5;

template <uint32_t N>
struct Block {
  uint8_t bytes[N];
  bool Load(const MemoryView& mem, uint32_t address) {
    return address != 0 && mem.Read(address, bytes, N);
  }
  uint32_t U32(uint32_t offset) const {
    uint32_t v;
    std::memcpy(&v, bytes + offset, sizeof(v));
    return v;
  }
  int32_t I32(uint32_t offset) const {
    int32_t v;
    std::memcpy(&v, bytes + offset, sizeof(v));
    return v;
  }
  uint16_t U16(uint32_t offset) const {
    uint16_t v;
    std::memcpy(&v, bytes + offset, sizeof(v));
    return v;
  }
};

uint32_t LevelOfRoom1(const MemoryView& mem, uint32_t room1) {
  uint32_t room2 = 0;
  uint32_t level = 0;
  if (room1 == 0 || !ReadU32(mem, room1 + kRoom1Room2, &room2) || room2 == 0 ||
      !ReadU32(mem, room2 + kRoom2Level, &level)) {
    return 0;
  }
  return level;
}

bool ValidRect(const TileRect& r) {
  return r.w > 0 && r.h > 0 && r.w <= kMaxRoomTiles && r.h <= kMaxRoomTiles;
}

TileRect RoomRect(const Block<kRoom2Block>& room) {
  TileRect r;
  r.x = room.I32(kRoom2TileX);
  r.y = room.I32(kRoom2TileY);
  r.w = room.I32(kRoom2TileW);
  r.h = room.I32(kRoom2TileH);
  return r;
}

bool SameSpot(const Mark& a, const Point& p) {
  return std::abs(a.x[0] - p.x) <= kSameSpotX && std::abs(a.y[0] - p.y) <= kSameSpotY;
}

int32_t FindObjectLabel(const ScanOptions& options, uint32_t object_class) {
  for (size_t i = 0; i < options.objects_to_mark.size(); ++i) {
    if (options.objects_to_mark[i].object_class == object_class) return static_cast<int32_t>(i);
  }
  return -1;
}

// O que a varredura le uma vez por passagem.
struct AllyEntry {
  uint32_t id = 0;
  uint32_t owner = 0;
  int32_t type = -1;
};

struct Tables {
  uint32_t objects_base = 0;
  uint32_t objects_count = 0;
  uint32_t player_id = 0;
  bool alignment_ok = false;
  std::vector<AllyEntry> allies;
};

// So ids de unidades tipo 1 sao comparados: os ids sao por tipo (U sec. 7.1).
void ReadAllies(const MemoryView& mem, const GameAddresses& addr, Tables* tables) {
  uint32_t node = 0;
  if (!ReadU32(mem, addr.ally_list, &node)) return;
  for (uint32_t i = 0; node != 0 && i < kMaxAllies; ++i) {
    Block<kAllyBlock> b;
    if (!b.Load(mem, node)) break;
    tables->allies.push_back(AllyEntry{b.U32(kAllyId), b.U32(kAllyOwner), b.I32(kAllyType)});
    node = b.U32(kAllyNext);
  }
}

// O que um objeto e, para as duas fontes (predefinida e unidade).
struct ObjectInfo {
  ObjectKind kind = ObjectKind::kOther;
  int32_t label = -1;
  int32_t operate_fn = -1;
};

// Le o que um objeto e. false se nao for para marcar. O que o jogo classifica
// (waypoint, portal, objetivo) nao precisa de configuracao; [objects] rotula o
// resto, troca o rotulo de um objetivo ou esconde a classe.
bool IdentifyObject(const MemoryView& mem, const ScanOptions& options, const Tables& tables,
                    uint32_t object_class, ObjectInfo* info) {
  *info = ObjectInfo{};
  // A classe de uma predefinida pode estar fora da tabela (L sec. 4.3).
  if (tables.objects_base != 0 && object_class < tables.objects_count) {
    const uint32_t record = tables.objects_base + object_class * kObjectRecordSize;
    uint8_t sub_class = 0;
    uint8_t operate_fn = 0;
    if (mem.Read(record + kObjectSubClass, &sub_class, 1) &&
        mem.Read(record + kObjectOperateFn, &operate_fn, 1)) {
      info->kind = ClassifyObject(object_class, sub_class, operate_fn);
      info->operate_fn = operate_fn;
    }
  }
  if (info->kind != ObjectKind::kOther && info->kind != ObjectKind::kQuest) return options.objects;
  info->label = FindObjectLabel(options, object_class);
  if (info->label >= 0 &&
      options.objects_to_mark[static_cast<size_t>(info->label)].label == kHideObjectLabel) {
    return false;
  }
  if (info->kind == ObjectKind::kQuest) {
    if (options.quests) return true;
    info->kind = ObjectKind::kOther;
  }
  return options.objects && info->label >= 0;
}

MarkKind MarkOfObject(ObjectKind kind) {
  switch (kind) {
    case ObjectKind::kWaypoint:
      return MarkKind::kWaypoint;
    case ObjectKind::kPortalBlue:
      return MarkKind::kPortalBlue;
    case ObjectKind::kPortalRed:
      return MarkKind::kPortalRed;
    case ObjectKind::kPortalOther:
      return MarkKind::kPortalOther;
    case ObjectKind::kQuest:
      return MarkKind::kQuest;
    case ObjectKind::kOther:
      break;
  }
  return MarkKind::kObject;
}

void AddExit(const Point& at, uint32_t warp_class, Scan* scan) {
  for (const Mark& m : scan->marks) {
    if (m.kind == MarkKind::kExit && SameSpot(m, at)) return;
  }
  Mark mark;
  mark.kind = MarkKind::kExit;
  mark.x[0] = at.x;
  mark.y[0] = at.y;
  mark.warp_class = static_cast<int32_t>(warp_class);
  // O destino e Vis[n] do n cujo Warp[n] e este registro de lvlwarp.
  if (scan->links_ok) {
    for (int n = 0; n < 8; ++n) {
      if (scan->warp[n] == mark.warp_class && scan->vis[n] != 0) {
        mark.dest_level = scan->vis[n];
        break;
      }
    }
  }
  scan->marks.push_back(mark);
  ++scan->stats.exits;
}

// Objetivos com classes proximas, como as 5 pedras, sao marcas separadas. A 460 e
// a 558 sao o mesmo: a predefinida e a unidade na mesma posicao (U sec. 5.4).
bool SameQuestObject(const Mark& m, uint32_t object_class) {
  const uint32_t other = static_cast<uint32_t>(m.unit_class);
  return other == object_class || (other == kObjectClassQuestPlaceholder &&
                                   object_class == kObjectClassQuestUnit) ||
         (other == kObjectClassQuestUnit && object_class == kObjectClassQuestPlaceholder);
}

// Objeto usado continua usado depois que a unidade sai da tabela. A 460 fica no
// lugar da 558, entao as duas contam como a mesma classe.
bool RememberUsedQuest(int32_t level, uint32_t object_class, bool used, ScanMemo* memo) {
  const int32_t key = static_cast<int32_t>(
      object_class == kObjectClassQuestPlaceholder ? kObjectClassQuestUnit : object_class);
  for (const ScanMemo::UsedQuest& q : memo->used_quests) {
    if (q.level == level && q.object_class == key) return true;
  }
  if (used) memo->used_quests.push_back(ScanMemo::UsedQuest{level, key});
  return used;
}

// A predefinida vem antes da unidade: a mesma marca ganha o que so a unidade
// conhece, o destino do portal e se o objeto ja foi usado.
void AddObject(const Point& at, const ObjectInfo& info, uint32_t object_class, int32_t dest_level,
               bool from_unit, bool used, ScanMemo* memo, Scan* scan) {
  const MarkKind mark_kind = MarkOfObject(info.kind);
  if (mark_kind == MarkKind::kQuest && memo != nullptr) {
    used = RememberUsedQuest(scan->level_id, object_class, used, memo);
  }
  for (Mark& m : scan->marks) {
    if (m.kind != mark_kind || !SameSpot(m, at)) continue;
    if (mark_kind == MarkKind::kQuest) {
      if (!SameQuestObject(m, object_class)) continue;
    } else if (m.label != info.label) {
      continue;
    }
    if (dest_level > 0) m.dest_level = dest_level;
    if (mark_kind != MarkKind::kQuest) return;
    if (from_unit) {
      m.unit_class = static_cast<int32_t>(object_class);
      m.operate_fn = info.operate_fn;
    }
    if (used && !m.used) {
      m.used = true;
      ++scan->stats.quests_used;
    }
    return;
  }
  Mark mark;
  mark.kind = mark_kind;
  mark.x[0] = at.x;
  mark.y[0] = at.y;
  mark.label = info.label;
  mark.unit_class = static_cast<int32_t>(object_class);
  mark.dest_level = dest_level;
  switch (mark_kind) {
    case MarkKind::kWaypoint:
      ++scan->stats.waypoints;
      break;
    case MarkKind::kPortalBlue:
      ++scan->stats.portals_blue;
      break;
    case MarkKind::kPortalRed:
      ++scan->stats.portals_red;
      break;
    case MarkKind::kPortalOther:
      ++scan->stats.portals_other;
      break;
    case MarkKind::kQuest:
      mark.operate_fn = info.operate_fn;
      mark.used = used;
      ++scan->stats.quests;
      if (used) ++scan->stats.quests_used;
      break;
    default:
      ++scan->stats.objects;
      break;
  }
  scan->marks.push_back(mark);
}

// Unidades predefinidas (L sec. 2.6): saidas (tipo 5) e objetos (tipo 2). Sao
// criadas quando a sala carrega e ficam depois que ela descarrega (L sec. 4.2).
void ScanPresets(const MemoryView& mem, const Block<kRoom2Block>& r,
                 const ScanOptions& options, const Tables& tables, Scan* scan) {
  const TileRect rect = RoomRect(r);
  uint32_t preset = r.U32(kRoom2Presets);
  for (uint32_t j = 0; preset != 0 && j < kMaxPresetsPerRoom; ++j) {
    Block<kPresetBlock> p;
    if (!p.Load(mem, preset)) break;
    ++scan->stats.presets;
    const uint32_t type = p.U32(kPresetType);
    const uint32_t object_class = p.U32(kPresetClass);
    const Point at = SubtileToClient(rect.x * 5 + p.I32(kPresetX), rect.y * 5 + p.I32(kPresetY));
    if (type == kTypeTile && options.exits) {
      AddExit(at, object_class, scan);
    } else if (type == kTypeObject && (options.objects || options.quests)) {
      ObjectInfo info;
      if (IdentifyObject(mem, options, tables, object_class, &info)) {
        AddObject(at, info, object_class, FixedPortalDestination(object_class, scan->level_id),
                  false, false, options.memo, scan);
      }
    }
    preset = p.U32(kPresetNext);
  }
}

// Sala do nivel atual, para o calculo das bordas.
struct RoomRef {
  uint32_t room2 = 0;
  uint32_t room1 = 0;
  TileRect rect;
};

void ScanRooms(const MemoryView& mem, const ScanOptions& options, const Tables& tables,
               Scan* scan, std::vector<TileRect>* waypoint_rooms, std::vector<RoomRef>* rooms) {
  uint32_t room = 0;
  if (!ReadU32(mem, scan->level + kLevelFirstRoom2, &room)) return;
  for (uint32_t i = 0; room != 0 && i < kMaxRooms; ++i) {
    Block<kRoom2Block> r;
    if (!r.Load(mem, room)) break;
    ++scan->stats.rooms;
    const bool loaded = r.U32(kRoom2Room1) != 0;
    if (loaded) ++scan->stats.rooms_loaded;
    const uint32_t flags = r.U32(kRoom2Flags);
    const TileRect rect = RoomRect(r);
    if (options.exits) {
      if (ValidRect(rect)) rooms->push_back(RoomRef{room, r.U32(kRoom2Room1), rect});
      // Warps (L sec. 4.2): so os n com Vis[n] != 0 e Warp[n] != -1; os
      // demais bits marcam vinculo com nivel aberto e aparecem em muitas salas.
      for (int n = 0; scan->links_ok && n < 8; ++n) {
        if (scan->vis[n] == 0 || scan->warp[n] == -1) continue;
        if ((flags & (kRoom2WarpBit << n)) == 0 || !ValidRect(rect)) continue;
        WarpRoom warp_room;
        warp_room.room = room;
        warp_room.x = rect.x;
        warp_room.y = rect.y;
        warp_room.w = rect.w;
        warp_room.h = rect.h;
        warp_room.n = n;
        warp_room.loaded = loaded;
        warp_room.load_count = r.U16(kRoom2LoadCount);
        scan->warp_rooms.push_back(warp_room);
      }
    }
    // A sala do waypoint e conhecida sem carregar nada (L sec. 2.4).
    if (options.objects && (flags & kRoom2WaypointMask) != 0 && ValidRect(rect)) {
      waypoint_rooms->push_back(rect);
    }
    ScanPresets(mem, r, options, tables, scan);
    room = r.U32(kRoom2Next);
  }
}

void ScanMonster(const MemoryView& mem, uint32_t unit, const Block<kUnitBlock>& u,
                 const ScanOptions& options, const Tables& tables, uint32_t monstats_base,
                 uint32_t monstats_count, Scan* scan) {
  const uint32_t monster_class = u.U32(kUnitClass);
  if (monster_class >= monstats_count) return;
  uint32_t monstats_flags = 0;
  if (!ReadU32(mem, monstats_base + monster_class * kMonStatsRecordSize + kMonStatsFlags,
               &monstats_flags)) {
    return;
  }
  uint8_t type_flags = 0;
  const uint32_t data = u.U32(kUnitData);
  if (data != 0 && !mem.Read(data + kMonsterTypeFlags, &type_flags, 1)) return;
  const uint32_t unit_id = u.U32(kUnitId);
  const AllyEntry* ally = nullptr;
  for (const AllyEntry& a : tables.allies) {
    if (a.id == unit_id) ally = &a;
  }
  // A lista de aliados diz o dono; hostil ou nao vem do alinhamento (U sec. 4).
  Alignment alignment = Alignment::kUnknown;
  bool unreadable = false;
  if (tables.alignment_ok && ally == nullptr) {
    int32_t value = 0;
    if (ReadUnitAlignment(mem, unit, &value)) {
      alignment = value == kAlignmentFriendly ? Alignment::kFriendly : Alignment::kOther;
    } else {
      unreadable = true;
    }
  }
  const MonsterClass kind =
      ClassifyMonster(u.U32(kUnitMode), monstats_flags, type_flags, ally != nullptr, alignment);
  if (kind == MonsterClass::kHidden) return;
  const bool npc_like = kind == MonsterClass::kNpc || kind == MonsterClass::kFriendly;
  if (npc_like && !options.npcs) return;
  if (kind == MonsterClass::kAlly && !options.allies) return;
  if (!npc_like && kind != MonsterClass::kAlly && !options.monsters) return;
  Block<kDynBlock> path;
  if (!path.Load(mem, u.U32(kUnitPath))) return;
  if (options.only_current_level && LevelOfRoom1(mem, path.U32(kDynRoom1)) != scan->level) {
    return;
  }
  Mark mark;
  mark.x[0] = path.I32(kDynClientX);
  mark.y[0] = path.I32(kDynClientY);
  mark.unit_class = static_cast<int32_t>(monster_class);
  if (unreadable) ++scan->stats.alignment_unknown;
  switch (kind) {
    case MonsterClass::kNpc:
      mark.kind = MarkKind::kNpc;
      ++scan->stats.npcs;
      break;
    case MonsterClass::kFriendly:
      mark.kind = MarkKind::kFriendly;
      ++scan->stats.friendly;
      break;
    case MonsterClass::kAlly:
      mark.kind = MarkKind::kAlly;
      mark.own = ally != nullptr && ally->owner == tables.player_id;
      mark.ally_type = ally != nullptr ? ally->type : -1;
      ++scan->stats.allies;
      if (mark.own) ++scan->stats.own_allies;
      break;
    case MonsterClass::kMinion:
      mark.kind = MarkKind::kMinion;
      ++scan->stats.minions;
      break;
    case MonsterClass::kSuperUnique:
      mark.kind = MarkKind::kSuperUnique;
      ++scan->stats.superuniques;
      break;
    case MonsterClass::kUnique:
      mark.kind = MarkKind::kUnique;
      ++scan->stats.uniques;
      break;
    case MonsterClass::kChampion:
      mark.kind = MarkKind::kChampion;
      ++scan->stats.champions;
      break;
    default:
      mark.kind = MarkKind::kMonster;
      break;
  }
  if (!npc_like && kind != MonsterClass::kAlly) ++scan->stats.monsters;
  scan->marks.push_back(mark);
}

// Objetos e tiles de saida pela tabela de unidades (caminho estatico).
void ScanStaticUnit(const MemoryView& mem, const Block<kUnitBlock>& u, uint32_t type,
                    const ScanOptions& options, const Tables& tables, Scan* scan) {
  const uint32_t unit_class = u.U32(kUnitClass);
  ObjectInfo info;
  if (type == kTypeObject && !IdentifyObject(mem, options, tables, unit_class, &info)) return;
  Block<kStaticBlock> path;
  if (!path.Load(mem, u.U32(kUnitPath))) return;
  // A tabela tambem traz o tile do outro lado do warp (L sec. 4.2): tiles de
  // outro nivel nunca sao saidas deste.
  if ((options.only_current_level || type == kTypeTile) &&
      LevelOfRoom1(mem, path.U32(kStaticRoom1)) != scan->level) {
    return;
  }
  Point at;
  at.x = path.I32(kStaticClientX);
  at.y = path.I32(kStaticClientY);
  if (type == kTypeTile) {
    AddExit(at, unit_class, scan);
    return;
  }
  // Destino em ObjectData nas classes 59 e 60; pelo nivel atual nas 298 e 342, em
  // que o campo vale 0 (U sec. 5.3).
  // TODO(RE): classes 100 e 377, sem predefinida no censo e nunca vistas vivas.
  int32_t dest = FixedPortalDestination(unit_class, scan->level_id);
  if (info.kind == ObjectKind::kPortalBlue || info.kind == ObjectKind::kPortalRed) {
    uint8_t value = 0;
    if (mem.Read(u.U32(kUnitData) + kObjectDataDestLevel, &value, 1) && value != 0) dest = value;
  }
  // Modo != 0 diz que o objeto foi usado; modo 0 nao prova nada (U sec. 5.4).
  AddObject(at, info, unit_class, dest, true, u.U32(kUnitMode) != 0, options.memo, scan);
}

// Tabela de unidades do servidor (U sec. 2): so o que o cliente ja carregou.
void ScanUnits(const MemoryView& mem, const GameAddresses& addr, const ScanOptions& options,
               const Tables& tables, Scan* scan) {
  if (addr.server_units == 0) return;
  uint32_t monstats_base = 0;
  uint32_t monstats_count = 0;
  uint32_t data_tables = 0;
  if (addr.data_tables != 0 && ReadU32(mem, addr.data_tables, &data_tables) && data_tables != 0) {
    ReadU32(mem, data_tables + kDataTablesMonStatsBase, &monstats_base);
    ReadU32(mem, data_tables + kDataTablesMonStatsCount, &monstats_count);
  }
  const uint32_t types[] = {kTypeMonster, kTypeObject, kTypeTile};
  for (uint32_t type : types) {
    if (type == kTypeMonster &&
        (monstats_base == 0 || !(options.monsters || options.npcs || options.allies))) {
      continue;
    }
    if (type == kTypeObject && !options.objects && !options.quests) continue;
    if (type == kTypeTile && !options.exits) continue;
    uint32_t seen = 0;
    for (uint32_t b = 0; b < kUnitTableBuckets && seen < kMaxUnitsPerType; ++b) {
      uint32_t unit = 0;
      if (!ReadU32(mem, addr.server_units + type * kUnitTableTypeStride + 4 * b, &unit)) break;
      while (unit != 0 && seen < kMaxUnitsPerType) {
        ++seen;
        Block<kUnitBlock> u;
        uint32_t next = 0;
        if (!u.Load(mem, unit) || !ReadU32(mem, unit + kUnitNextInBucket, &next)) break;
        ++scan->stats.units;
        if (u.U32(kUnitType) == type) {
          if (type == kTypeMonster) {
            ScanMonster(mem, unit, u, options, tables, monstats_base, monstats_count, scan);
          } else {
            ScanStaticUnit(mem, u, type, options, tables, scan);
          }
        }
        unit = next;
      }
    }
  }
}

bool HasExactExit(const Scan& scan, int32_t warp_class) {
  for (const Mark& m : scan.marks) {
    if (m.kind == MarkKind::kExit && m.warp_class == warp_class) return true;
  }
  return false;
}

void SetRoomOutline(const TileRect& r, Mark* mark) {
  mark->points = 4;
  const Point corners[4] = {TileToClient(r.x, r.y), TileToClient(r.x + r.w, r.y),
                            TileToClient(r.x + r.w, r.y + r.h), TileToClient(r.x, r.y + r.h)};
  for (int i = 0; i < 4; ++i) {
    mark->x[i] = corners[i].x;
    mark->y[i] = corners[i].y;
  }
}

// Warps cuja posicao exata ainda nao e conhecida: marca a sala inteira. Houve no
// maximo uma sala por n real nos Atos I-V (L sec. 4.2), em qualquer tipo de nivel.
void AddExitRooms(Scan* scan) {
  for (const WarpRoom& r : scan->warp_rooms) {
    if (HasExactExit(*scan, scan->warp[r.n])) continue;
    Mark mark;
    mark.kind = MarkKind::kExitRoom;
    SetRoomOutline(TileRect{r.x, r.y, r.w, r.h}, &mark);
    mark.dest_level = scan->vis[r.n];
    mark.warp_class = scan->warp[r.n];
    scan->marks.push_back(mark);
    ++scan->stats.exit_rooms;
  }
}

// Sala do waypoint enquanto a posicao exata nao aparece.
void AddWaypointRooms(const std::vector<TileRect>& rooms, Scan* scan) {
  if (scan->stats.waypoints != 0) return;
  for (const TileRect& r : rooms) {
    Mark mark;
    mark.kind = MarkKind::kWaypointRoom;
    SetRoomOutline(r, &mark);
    scan->marks.push_back(mark);
    ++scan->stats.waypoint_rooms;
  }
}

bool ReadLevelRect(const MemoryView& mem, uint32_t level, TileRect* rect) {
  Block<16> b;
  if (!b.Load(mem, level + kLevelTileX)) return false;
  rect->x = b.I32(0);
  rect->y = b.I32(4);
  rect->w = b.I32(8);
  rect->h = b.I32(12);
  return ValidRect(*rect);
}

uint32_t FindLevelInAct(const MemoryView& mem, uint32_t drlg_act, int32_t id) {
  uint32_t level = 0;
  if (!ReadU32(mem, drlg_act + kDrlgActFirstLevel, &level)) return 0;
  for (uint32_t i = 0; level != 0 && i < kMaxLevels; ++i) {
    int32_t level_id = 0;
    if (!ReadI32(mem, level + kLevelId, &level_id)) return 0;
    if (level_id == id) return level;
    if (!ReadU32(mem, level + kLevelNext, &level)) return 0;
  }
  return 0;
}

struct CollisionGrid {
  int32_t x = 0;  // subtiles
  int32_t y = 0;
  int32_t w = 0;
  int32_t h = 0;
  uint32_t cells = 0;  // WORD[w * h], linha por linha
};

bool ReadCollisionGrid(const MemoryView& mem, uint32_t room1, CollisionGrid* grid) {
  uint32_t address = 0;
  Block<kCollisionBlock> b;
  if (!ReadU32(mem, room1 + kRoom1Collision, &address) || !b.Load(mem, address)) return false;
  grid->x = b.I32(kCollisionX);
  grid->y = b.I32(kCollisionX + 4);
  grid->w = b.I32(kCollisionX + 8);
  grid->h = b.I32(kCollisionX + 12);
  grid->cells = b.U32(kCollisionCells);
  return grid->cells != 0 && grid->w > 0 && grid->h > 0 && grid->w <= kMaxRoomTiles &&
         grid->h <= kMaxRoomTiles;
}

// false se a celula nao estiver em nenhuma sala carregada.
bool ReadCellBlocked(const MemoryView& mem, const std::vector<CollisionGrid>& grids, int32_t x,
                     int32_t y, bool* blocked) {
  for (const CollisionGrid& g : grids) {
    if (x < g.x || y < g.y || x >= g.x + g.w || y >= g.y + g.h) continue;
    const uint32_t index = static_cast<uint32_t>((y - g.y) * g.w + (x - g.x));
    uint16_t cell = 0;
    if (!mem.Read(g.cells + index * 2, &cell, 2)) return false;
    *blocked = (cell & kCollisionBlocksWalk) != 0;
    return true;
  }
  return false;
}

// Segmento de uma fronteira de `from` a `to` (tiles), com os indices de `run` contados
// em subtiles a partir de `from`.
Mark PassageMark(const Point& from, const Point& to, const EdgeRun& run, int32_t dest) {
  const bool vertical = from.x == to.x;
  const int32_t line = (vertical ? from.x : from.y) * 5;
  const int32_t begin = (vertical ? from.y : from.x) * 5 + run.start;
  const int32_t end = begin + run.length;
  const Point a = vertical ? SubtileToClient(line, begin) : SubtileToClient(begin, line);
  const Point b = vertical ? SubtileToClient(line, end) : SubtileToClient(end, line);
  Mark mark;
  mark.kind = MarkKind::kPassage;
  mark.points = 2;
  mark.x[0] = a.x;
  mark.y[0] = a.y;
  mark.x[1] = b.x;
  mark.y[1] = b.y;
  mark.dest_level = dest;
  return mark;
}

// Passagem pela colisao (L sec. 4.1): pares de celulas vizinhas atraves da fronteira,
// livres dos dois lados, com as salas carregadas. Devolve os subtiles livres.
int32_t CollisionPassage(const MemoryView& mem, const std::vector<RoomRef>& rooms, uint32_t other,
                         const Point& from, const Point& to, int32_t dest,
                         std::vector<Mark>* marks) {
  std::vector<CollisionGrid> grids;
  for (const RoomRef& room : rooms) {
    CollisionGrid grid;
    if (room.room1 != 0 && RoomTouchesEdge(room.rect, from, to) &&
        ReadCollisionGrid(mem, room.room1, &grid)) {
      grids.push_back(grid);
    }
  }
  if (grids.empty()) return 0;
  const size_t own_grids = grids.size();
  uint32_t room = 0;
  if (!ReadU32(mem, other + kLevelFirstRoom2, &room)) return 0;
  for (uint32_t i = 0; room != 0 && i < kMaxRooms; ++i) {
    Block<kRoom2Block> r;
    if (!r.Load(mem, room)) break;
    const TileRect rect = RoomRect(r);
    CollisionGrid grid;
    if (r.U32(kRoom2Room1) != 0 && ValidRect(rect) && RoomTouchesEdge(rect, from, to) &&
        ReadCollisionGrid(mem, r.U32(kRoom2Room1), &grid)) {
      grids.push_back(grid);
    }
    room = r.U32(kRoom2Next);
  }
  if (grids.size() == own_grids) return 0;  // nada carregado do outro lado
  const bool vertical = from.x == to.x;
  const int32_t line = (vertical ? from.x : from.y) * 5;
  const int32_t begin = (vertical ? from.y : from.x) * 5;
  const int32_t end = (vertical ? to.y : to.x) * 5;
  std::vector<EdgeCell> cells;
  cells.reserve(static_cast<size_t>(end - begin));
  for (int32_t at = begin; at < end; ++at) {
    bool blocked_a = false;
    bool blocked_b = false;
    const bool known_a = vertical ? ReadCellBlocked(mem, grids, line - 1, at, &blocked_a)
                                  : ReadCellBlocked(mem, grids, at, line - 1, &blocked_a);
    const bool known_b = vertical ? ReadCellBlocked(mem, grids, line, at, &blocked_b)
                                  : ReadCellBlocked(mem, grids, at, line, &blocked_b);
    if ((known_a && blocked_a) || (known_b && blocked_b)) {
      cells.push_back(EdgeCell::kBlocked);
    } else {
      cells.push_back(known_a && known_b ? EdgeCell::kFree : EdgeCell::kUnknown);
    }
  }
  int32_t width = 0;
  for (const EdgeRun& run : FreeRuns(cells)) {
    if (run.length < kMinPassageCells) continue;
    Mark mark = PassageMark(from, to, run, dest);
    mark.source = PassageSource::kCollision;
    marks->push_back(mark);
    width += run.length;
  }
  return width;
}

// Regra do Ato I, sem ativar nada (L sec. 4.1): entre as salas do nivel que tocam o
// trecho comum, a da passagem e a unica de variante >= 3.
bool Act1RulePassage(const MemoryView& mem, const Scan& scan, const std::vector<RoomRef>& rooms,
                     const Point& from, const Point& to, int32_t dest, Mark* mark) {
  uint32_t drlg_type = 0;
  if (scan.act_index != 0 || !IsAct1RuleBorder(scan.level_id, dest) ||
      !ReadU32(mem, scan.level + kLevelDrlgType, &drlg_type) || drlg_type != kLevelTypeExternal) {
    return false;
  }
  const RoomRef* found = nullptr;
  for (const RoomRef& room : rooms) {
    if (!RoomTouchesEdge(room.rect, from, to)) continue;
    uint32_t data = 0;
    uint32_t cell = 0;
    int32_t variant = 0;
    if (!ReadU32(mem, room.room2 + kRoom2TypeData, &data) ||
        !ReadU32(mem, data + kTypeDataGeneratorCell, &cell) ||
        !ReadI32(mem, cell + kGeneratorCellVariant, &variant)) {
      return false;
    }
    if (variant < kAct1PassageVariant) continue;
    if (found != nullptr) return false;  // a regra pede exatamente uma sala
    found = &room;
  }
  if (found == nullptr) return false;
  const bool vertical = from.x == to.x;
  const int32_t origin = (vertical ? found->rect.y : found->rect.x) * 5;
  const int32_t begin = (vertical ? from.y : from.x) * 5;
  const int32_t end = (vertical ? to.y : to.x) * 5;
  EdgeRun run = Act1RuleRun(origin);
  if (run.start < begin || run.start + run.length > end) return false;
  run.start -= begin;
  *mark = PassageMark(from, to, run, dest);
  mark->source = PassageSource::kAct1Rule;
  return true;
}

// O que ja foi calculado pela colisao continua valendo depois que as salas
// descarregam. Fica o maior trecho visto: um trecho visto com parte das salas
// carregadas pode estar cortado.
void UpdateMemo(const Scan& scan, ScanMemo* memo, int32_t dest, int32_t* width,
                std::vector<Mark>* marks) {
  if (memo == nullptr) return;
  for (ScanMemo::Passage& e : memo->passages) {
    if (e.level != scan.level_id || e.dest != dest) continue;
    if (*width >= e.width) {
      e.width = *width;
      e.marks = *marks;
    } else {
      *width = e.width;
      *marks = e.marks;
    }
    return;
  }
  if (*width > 0) memo->passages.push_back(ScanMemo::Passage{scan.level_id, dest, *width, *marks});
}

// Vizinhos abertos: Vis[n] != 0 com Warp[n] = -1 e retangulos que se tocam (L sec.
// 4.1). A lista de vizinhas das salas nao entra: ela da a fronteira, nao a passagem.
// Exata so pela colisao ou pela regra do Ato I; o resto e o trecho comum, que diz onde
// a passagem esta, nao a passagem.
// TODO(RE): posicao exata sem ativar sala fora do Ato I; passagem aberta no Ato IV.
void ScanOpenBorders(const MemoryView& mem, const ScanOptions& options,
                     const std::vector<RoomRef>& rooms, Scan* scan) {
  uint32_t drlg_act = 0;
  const TileRect self = scan->level_rect;
  if (!scan->links_ok || !ValidRect(self) ||
      !ReadU32(mem, scan->level + kLevelDrlgAct, &drlg_act)) {
    return;
  }
  std::vector<int32_t> done;
  for (int n = 0; n < 8; ++n) {
    const int32_t dest = scan->vis[n];
    if (dest <= 0 || scan->warp[n] != -1) continue;
    if (std::find(done.begin(), done.end(), dest) != done.end()) continue;
    done.push_back(dest);
    // So leitura: o nivel pode ainda nao existir (nao chama GetLevel).
    const uint32_t other = FindLevelInAct(mem, drlg_act, dest);
    TileRect rect;
    Point from;
    Point to;
    if (other == 0 || !ReadLevelRect(mem, other, &rect) || !SharedEdge(self, rect, &from, &to)) {
      continue;
    }
    std::vector<Mark> exact;
    int32_t width = 0;
    if (options.passage_collision) {
      width = CollisionPassage(mem, rooms, other, from, to, dest, &exact);
      UpdateMemo(*scan, options.memo, dest, &width, &exact);
    }
    if (width > 0) {
      scan->marks.insert(scan->marks.end(), exact.begin(), exact.end());
      scan->stats.passages += static_cast<uint32_t>(exact.size());
      continue;
    }
    Mark mark;
    if (Act1RulePassage(mem, *scan, rooms, from, to, dest, &mark)) {
      scan->marks.push_back(mark);
      ++scan->stats.passages;
      ++scan->stats.passages_rule;
      continue;
    }
    mark = Mark{};
    mark.kind = MarkKind::kPassageApprox;
    mark.points = 2;
    mark.primary = true;
    const Point a = TileToClient(from.x, from.y);
    const Point b = TileToClient(to.x, to.y);
    mark.x[0] = a.x;
    mark.y[0] = a.y;
    mark.x[1] = b.x;
    mark.y[1] = b.y;
    mark.dest_level = dest;
    scan->marks.push_back(mark);
    ++scan->stats.passages_approx;
    ++scan->stats.passage_levels;
  }
}

// Um rotulo por nivel vizinho: no segmento mais perto do centro da borda.
void LabelPassages(Scan* scan) {
  std::vector<int32_t> done;
  for (const Mark& first : scan->marks) {
    if (first.kind != MarkKind::kPassage) continue;
    const int32_t dest = first.dest_level;
    if (std::find(done.begin(), done.end(), dest) != done.end()) continue;
    done.push_back(dest);
    int64_t sum_x = 0;
    int64_t sum_y = 0;
    int64_t n = 0;
    for (const Mark& m : scan->marks) {
      if (m.kind != MarkKind::kPassage || m.dest_level != dest) continue;
      sum_x += m.x[0] + m.x[1];
      sum_y += m.y[0] + m.y[1];
      n += 2;
    }
    const int64_t cx = sum_x / n;
    const int64_t cy = sum_y / n;
    Mark* best = nullptr;
    int64_t best_d = 0;
    for (Mark& m : scan->marks) {
      if (m.kind != MarkKind::kPassage || m.dest_level != dest) continue;
      const int64_t dx = (m.x[0] + m.x[1]) / 2 - cx;
      const int64_t dy = (m.y[0] + m.y[1]) / 2 - cy;
      const int64_t d = dx * dx + dy * dy;
      if (best == nullptr || d < best_d) {
        best = &m;
        best_d = d;
      }
    }
    if (best != nullptr) best->primary = true;
  }
  scan->stats.passage_levels += static_cast<uint32_t>(done.size());
}

}  // namespace

bool ReadU32(const MemoryView& mem, uint32_t address, uint32_t* out) {
  return address != 0 && mem.Read(address, out, sizeof(*out));
}

bool ReadI32(const MemoryView& mem, uint32_t address, int32_t* out) {
  return address != 0 && mem.Read(address, out, sizeof(*out));
}

const std::vector<AnchorSpec>& Anchors() {
  // offset = posicao do endereco do global dentro dos bytes verificados (ver a
  // descricao de cada simbolo em aeonian/addresses/1.14d.ini). Ancoras que
  // repetem um global servem de conferencia; o recorte e um RECT e as ancoras
  // de topo/direita confirmam o layout (+4/+8).
  static const std::vector<AnchorSpec> kAnchors = {
      {"D2Client.GetPlayerUnit", 1, &GameAddresses::player_unit, 0},
      {"D2Client.DrawAutomap", 2, &GameAddresses::current_act, 0},
      {"D2Client.FindServerSideUnit", 11, &GameAddresses::server_units, 0},
      {"D2Common.MonStatsRecordAccess", 2, &GameAddresses::data_tables, 0},
      {"D2Common.GetLevelDefRecord", 8, &GameAddresses::data_tables, 0},
      {"D2Common.GetLevelDefRecord", 28, &GameAddresses::level_defs, 0},
      {"D2Client.DrawAutomapCallSite", 2, &GameAddresses::automap_visible, 0},
      {"D2Client.GetAutomapMode", 1, &GameAddresses::automap_mode, 0},
      {"D2Client.SetAutomapMode", 4, &GameAddresses::automap_mode, 0},
      {"D2Client.SetAutomapMode", 20, &GameAddresses::automap_divisor, 0},
      {"D2Client.AutomapUnitToScreen", 2, &GameAddresses::automap_offset_x, 0},
      {"D2Client.AutomapUnitToScreen", 13, &GameAddresses::automap_offset_y, 0},
      {"D2Client.AutomapUnitToScreen", 22, &GameAddresses::automap_clip, 0},
      {"D2Client.AutomapClipCheck", 2, &GameAddresses::automap_clip, 0},
      {"D2Client.AutomapClipCheck", 26, &GameAddresses::automap_clip, 4},
      {"D2Client.AutomapClipCheck", 14, &GameAddresses::automap_clip, 8},
      {"D2Client.GetAutomapPanelSide", 1, &GameAddresses::panel_side, 0},
      {"D2Client.GetUiState", 3, &GameAddresses::ui_state, 0},
      {"D2Client.ScreenSizeAnchor", 6, &GameAddresses::frame_width, 0},
      {"D2Client.ScreenSizeAnchor", 12, &GameAddresses::frame_height, 0},
      {"D2Client.SetAutomapPanelSide", 25, &GameAddresses::frame_width, 0},
      {"D2Client.FindPetEntry", 1, &GameAddresses::ally_list, 0},
      {"D2Client.RemovePetEntry", 3, &GameAddresses::ally_list, 0},
      {"D2Common.GetObjectsTxtRecord", 8, &GameAddresses::objects_count, 0},
      {"D2Common.ObjectsTxtBase", 8, &GameAddresses::objects_base, 0},
  };
  return kAnchors;
}

// Simbolo que entra pelo proprio endereco: a conferencia dos bytes e o que importa.
struct GatedSymbol {
  const char* symbol;
  uint32_t GameAddresses::*field;
};
constexpr GatedSymbol kGatedSymbols[] = {
    {"D2Common.GetUnitAlignment", &GameAddresses::unit_alignment},
};

std::vector<std::string> RequiredSymbols() {
  std::vector<std::string> names;
  for (const AnchorSpec& a : Anchors()) {
    if (std::find(names.begin(), names.end(), a.symbol) == names.end()) {
      names.emplace_back(a.symbol);
    }
  }
  for (const GatedSymbol& g : kGatedSymbols) names.emplace_back(g.symbol);
  return names;
}

void ResolveAddresses(const MemoryView& anchors, ResolveFn resolve, void* context,
                      GameAddresses* out, ResolveReport* report) {
  *out = GameAddresses{};
  *report = ResolveReport{};
  const std::vector<std::string> names = RequiredSymbols();
  std::vector<uint32_t> bases(names.size(), 0);
  report->total = static_cast<uint32_t>(names.size());
  for (size_t i = 0; i < names.size(); ++i) {
    bases[i] = resolve(context, names[i].c_str());
    if (bases[i] != 0) {
      ++report->resolved;
    } else {
      report->missing.push_back(names[i]);
    }
  }
  // Campos que duas ancoras discordaram: ficam em 0 de vez.
  std::vector<uint32_t GameAddresses::*> conflicts;
  for (const AnchorSpec& a : Anchors()) {
    const size_t i = static_cast<size_t>(
        std::find(names.begin(), names.end(), a.symbol) - names.begin());
    if (bases[i] == 0) continue;
    if (std::find(conflicts.begin(), conflicts.end(), a.field) != conflicts.end()) continue;
    uint32_t value = 0;
    if (!ReadU32(anchors, bases[i] + a.offset, &value) || value <= a.delta) {
      report->notes.push_back(std::string(a.symbol) + ": ancora ilegivel");
      continue;
    }
    value -= a.delta;
    uint32_t& field = out->*(a.field);
    if (field != 0 && field != value) {
      report->notes.push_back(std::string(a.symbol) + ": ancora diverge das demais");
      field = 0;
      conflicts.push_back(a.field);
      continue;
    }
    field = value;
  }
  for (const GatedSymbol& g : kGatedSymbols) {
    const size_t i =
        static_cast<size_t>(std::find(names.begin(), names.end(), g.symbol) - names.begin());
    out->*(g.field) = bases[i];
  }
}

uint32_t AvailableFeatures(const GameAddresses& a) {
  uint32_t f = 0;
  if (a.player_unit != 0 && a.automap_visible != 0 && a.automap_mode != 0 &&
      a.automap_divisor != 0 && a.automap_offset_x != 0 && a.automap_offset_y != 0) {
    f |= kFeatureAutomap;
  }
  if (a.server_units != 0 && a.data_tables != 0) f |= kFeatureMonsters;
  if (a.server_units != 0) f |= kFeatureObjectUnits;
  if (a.data_tables != 0 && a.level_defs != 0) f |= kFeatureLevelLinks;
  if (a.automap_clip != 0) f |= kFeatureClip;
  if (a.ally_list != 0) f |= kFeatureAllies;
  if (a.objects_base != 0 && a.objects_count != 0) f |= kFeatureObjectTable;
  if (a.panel_side != 0 && a.ui_state != 0) f |= kFeaturePanels;
  if (a.frame_width != 0 && a.frame_height != 0) f |= kFeatureFrameSize;
  if (a.unit_alignment != 0) f |= kFeatureAlignment;
  return f;
}

bool ReadAutomapView(const MemoryView& mem, const GameAddresses& addr, AutomapView* out) {
  *out = AutomapView{};
  uint32_t visible = 0;
  if (!ReadU32(mem, addr.automap_visible, &visible) ||
      !ReadI32(mem, addr.automap_mode, &out->mode) ||
      !ReadI32(mem, addr.automap_divisor, &out->divisor) ||
      !ReadI32(mem, addr.automap_offset_x, &out->offset_x) ||
      !ReadI32(mem, addr.automap_offset_y, &out->offset_y)) {
    return false;
  }
  // O jogo so usa 10 ou 20; outro valor indica leitura errada.
  if (out->divisor <= 0 || out->divisor > 100) return false;
  out->visible = visible != 0;
  int32_t clip[4];
  if (addr.automap_clip != 0 && mem.Read(addr.automap_clip, clip, sizeof(clip)) &&
      clip[0] < clip[2] && clip[1] < clip[3]) {
    out->has_clip = true;
    out->clip_left = clip[0];
    out->clip_top = clip[1];
    out->clip_right = clip[2];
    out->clip_bottom = clip[3];
  }
  int32_t size[2] = {};
  if (ReadI32(mem, addr.frame_width, &size[0]) && ReadI32(mem, addr.frame_height, &size[1]) &&
      size[0] > 0 && size[0] <= 16384 && size[1] > 0 && size[1] <= 16384) {
    out->frame_width = size[0];
    out->frame_height = size[1];
  }
  int32_t side = 0;
  if (ReadI32(mem, addr.panel_side, &side) && side >= 0 && side <= kPanelBoth) {
    out->panel_side = side;
  }
  if (addr.ui_state != 0) {
    uint32_t ui = 0;
    out->npc_dialog = ReadU32(mem, addr.ui_state + kUiNpcDialog * 4, &ui) && ui != 0;
    out->esc_menu = ReadU32(mem, addr.ui_state + kUiEscMenu * 4, &ui) && ui != 0;
    out->help = ReadU32(mem, addr.ui_state + kUiHelp * 4, &ui) && ui != 0;
  }
  ApplyPanelRules(out);
  return true;
}

void ApplyPanelRules(AutomapView* view) {
  // TODO(RE): cubo, troca com NPC, jogadores e chat nao foram abertos na analise
  // (L sec. 5.4); valem so se mudarem o lado.
  view->blocked = view->panel_side == kPanelBoth || view->npc_dialog || view->esc_menu ||
                  view->help;
  if (view->blocked || view->mode == 1) return;
  if (view->panel_side != kPanelRight && view->panel_side != kPanelLeft) return;
  const int32_t width = view->frame_width > 0 ? view->frame_width
                        : view->has_clip      ? view->clip_right
                                              : 0;
  if (width <= 0) {
    view->blocked = true;
    return;
  }
  if (!view->has_clip) {
    view->has_clip = true;
    view->clip_left = 0;
    view->clip_top = 0;
    view->clip_right = width;
    view->clip_bottom = view->frame_height > 0 ? view->frame_height : 1 << 20;
  }
  if (view->panel_side == kPanelRight) {
    view->clip_right = std::min(view->clip_right, width / 2 - 1);
  } else {
    view->clip_left = std::max(view->clip_left, width / 2);
  }
}

Point SubtileToClient(int32_t x, int32_t y) {
  Point p;
  p.x = (x - y) * 16;
  p.y = (x + y) * 8;
  return p;
}

Point ClientToSubtile(int32_t cx, int32_t cy) {
  Point p;
  p.x = (cx / 16 + cy / 8) / 2;
  p.y = (cy / 8 - cx / 16) / 2;
  return p;
}

Point ClientToAutomap(int32_t cx, int32_t cy, const AutomapView& view) {
  Point p;
  if (view.divisor <= 0) return p;
  p.x = cx / view.divisor - view.offset_x + 8;
  p.y = cy / view.divisor - view.offset_y - 8;
  if (view.mode == 1) {
    p.x += kMiniMarkerDx;
    p.y += kMiniMarkerDy;
  }
  return p;
}

bool InsideClip(const Point& p, const AutomapView& view) {
  if (!view.has_clip) return true;
  return p.x >= view.clip_left && p.x <= view.clip_right && p.y >= view.clip_top &&
         p.y <= view.clip_bottom;
}

bool ReadLevelLinks(const MemoryView& mem, const GameAddresses& addr, uint32_t level,
                    int32_t level_id, int32_t vis[8], int32_t warp[8]) {
  for (int n = 0; n < 8; ++n) {
    vis[n] = 0;
    warp[n] = -1;
  }
  // 1) Excecoes do ato (o jogo as consulta antes de LevelDefs).
  uint32_t drlg_act = 0;
  uint32_t node = 0;
  if (ReadU32(mem, level + kLevelDrlgAct, &drlg_act) && drlg_act != 0 &&
      ReadU32(mem, drlg_act + kDrlgActExceptions, &node)) {
    for (uint32_t i = 0; node != 0 && i < kMaxExceptions; ++i) {
      int32_t id = 0;
      if (!ReadI32(mem, node + kExceptionId, &id)) break;
      if (id == level_id) {
        return mem.Read(node + kExceptionVis, vis, 8 * sizeof(int32_t)) &&
               mem.Read(node + kExceptionWarp, warp, 8 * sizeof(int32_t));
      }
      if (!ReadU32(mem, node + kExceptionNext, &node)) break;
    }
  }
  // 2) LevelDefs: [base] + id*0x9C, id < [[DataTables]+0xC5C].
  uint32_t tables = 0;
  uint32_t count = 0;
  uint32_t base = 0;
  if (addr.data_tables == 0 || addr.level_defs == 0 || level_id <= 0 ||
      !ReadU32(mem, addr.data_tables, &tables) || tables == 0 ||
      !ReadU32(mem, tables + kDataTablesLevelDefsCount, &count) ||
      static_cast<uint32_t>(level_id) >= count || !ReadU32(mem, addr.level_defs, &base) ||
      base == 0) {
    return false;
  }
  const uint32_t record = base + static_cast<uint32_t>(level_id) * kLevelDefsRecordSize;
  if (!mem.Read(record + kLevelDefsVis, vis, 8 * sizeof(int32_t)) ||
      !mem.Read(record + kLevelDefsWarp, warp, 8 * sizeof(int32_t))) {
    for (int n = 0; n < 8; ++n) {
      vis[n] = 0;
      warp[n] = -1;
    }
    return false;
  }
  return true;
}

const WarpRoom* NextRoomToReveal(const Scan& scan, const std::vector<uint32_t>& done) {
  for (const WarpRoom& r : scan.warp_rooms) {
    if (r.loaded || r.load_count != 0 || HasExactExit(scan, scan.warp[r.n])) continue;
    if (ActOfLevel(scan.vis[r.n]) != scan.act_index) continue;
    if (std::find(done.begin(), done.end(), r.room) == done.end()) return &r;
  }
  return nullptr;
}

const char* RevealBlockReason(const MemoryView& mem, const Scan& scan) {
  if (!scan.in_game || scan.act == 0 || scan.player_room == 0 || scan.level == 0) {
    return "fora da partida";
  }
  // Sem sala ativa o nivel perde as Room2 e as predefinidas (L sec. 3.1).
  if (scan.stats.rooms_loaded == 0) return "nivel sem sala ativa";
  // Pedir nivel de outro ato fecha o jogo (L sec. 3.1).
  uint32_t room_act = 0;
  int32_t act_number = -1;
  if (!ReadU32(mem, scan.player_room + kRoom1Act, &room_act) || room_act != scan.act) {
    return "sala do jogador fora do ato do cliente";
  }
  if (!ReadI32(mem, scan.act + kActIndex, &act_number) || act_number != scan.act_index ||
      ActOfLevel(scan.level_id) != act_number) {
    return "nivel fora do ato do cliente";
  }
  uint32_t drlg_act = 0;
  uint32_t back = 0;
  if (!ReadU32(mem, scan.act + kActDrlgAct, &drlg_act) ||
      !ReadU32(mem, drlg_act + kDrlgActBackToAct, &back) || back != scan.act) {
    return "DrlgAct nao confere com o ato";
  }
  if (FindLevelInAct(mem, drlg_act, scan.level_id) != scan.level) {
    return "nivel fora da corrente do ato";
  }
  return nullptr;
}

int32_t ActOfLevel(int32_t level_id) {
  if (level_id < 1 || level_id >= kLevelDefsSeen) return -1;
  if (level_id >= 109) return 4;
  if (level_id >= 103) return 3;
  if (level_id >= 75) return 2;
  if (level_id >= 40) return 1;
  return 0;
}

bool ReadObjectName(const MemoryView& mem, const GameAddresses& addr, uint32_t object_class,
                    char* out, uint32_t out_size) {
  uint32_t base = 0;
  uint32_t count = 0;
  char name[kObjectNameSize];
  if (out_size == 0 || !ReadU32(mem, addr.objects_base, &base) || base == 0 ||
      !ReadU32(mem, addr.objects_count, &count) || object_class >= count ||
      !mem.Read(base + object_class * kObjectRecordSize, name, kObjectNameSize)) {
    return false;
  }
  const uint32_t limit = std::min(out_size - 1, kObjectNameSize - 1);
  uint32_t n = 0;
  for (; n < limit && name[n] != '\0'; ++n) out[n] = name[n];
  out[n] = '\0';
  return true;
}

bool ReadUnitAlignment(const MemoryView& mem, uint32_t unit, int32_t* out) {
  *out = 0;
  uint32_t list = 0;
  if (!ReadU32(mem, unit + kUnitStats, &list)) return false;
  if (list == 0) return true;  // sem stats: 0
  uint32_t sub = 0;
  if (!ReadU32(mem, list + kStatsFirstSubList, &sub)) return false;
  for (uint32_t i = 0; sub != 0 && i < kMaxSubLists; ++i) {
    Block<kSubListBlock> b;
    if (!b.Load(mem, sub)) return false;
    if (b.U32(kSubListState) == kStateAlignment) {
      const uint32_t pairs = b.U32(kSubListPairs);
      const uint32_t count = b.U16(kSubListPairCount);
      if (count > kMaxStatPairs) return false;
      for (uint32_t k = 0; k < count; ++k) {
        uint32_t stat = 0;
        if (!ReadU32(mem, pairs + k * kStatPairSize, &stat)) return false;
        if ((stat >> 16) == kStatAlignment) {
          return ReadI32(mem, pairs + k * kStatPairSize + 4, out);
        }
      }
      return true;  // estado sem o stat: 0
    }
    sub = b.U32(kSubListNext);
  }
  return sub == 0;  // lista longa demais: memoria corrompida
}

void ScanGame(const MemoryView& mem, const GameAddresses& addr, const ScanOptions& options,
              Scan* out) {
  *out = Scan{};
  uint32_t player = 0;
  if (!ReadU32(mem, addr.player_unit, &player) || player == 0) return;
  // Partida em andamento = jogador e ato do cliente nao nulos (U sec. 1).
  if (addr.current_act != 0) {
    if (!ReadU32(mem, addr.current_act, &out->act) || out->act == 0) return;
  }
  Block<kUnitBlock> unit;
  if (!unit.Load(mem, player) || unit.U32(kUnitType) != kTypePlayer) return;
  out->in_game = true;
  out->act_index = unit.I32(kUnitActIndex);
  Block<kDynBlock> path;
  if (!path.Load(mem, unit.U32(kUnitPath))) return;
  out->player_x = path.U16(kDynSubtileX);
  out->player_y = path.U16(kDynSubtileY);
  out->player_room = path.U32(kDynRoom1);
  // Nivel atual = jogador -> Room1 -> Room2 -> Level (L sec. 2).
  const uint32_t level = LevelOfRoom1(mem, out->player_room);
  int32_t level_id = 0;
  if (level == 0 || !ReadI32(mem, level + kLevelId, &level_id) || level_id <= 0 ||
      level_id >= kMaxLevelId) {
    return;
  }
  out->level = level;
  out->level_id = level_id;
  ReadLevelRect(mem, level, &out->level_rect);
  out->links_ok = ReadLevelLinks(mem, addr, level, level_id, out->vis, out->warp);
  Tables tables;
  tables.player_id = unit.U32(kUnitId);
  tables.alignment_ok = addr.unit_alignment != 0;
  ReadAllies(mem, addr, &tables);
  if (ReadU32(mem, addr.objects_base, &tables.objects_base)) {
    ReadU32(mem, addr.objects_count, &tables.objects_count);
  }
  if (options.memo != nullptr) {
    uint32_t seed = 0;
    ReadU32(mem, out->act + kActSeed, &seed);
    if (options.memo->act != out->act || options.memo->seed != seed) {
      *options.memo = ScanMemo{};
      options.memo->act = out->act;
      options.memo->seed = seed;
    }
  }
  std::vector<TileRect> waypoint_rooms;
  std::vector<RoomRef> rooms;
  ScanRooms(mem, options, tables, out, &waypoint_rooms, &rooms);
  ScanUnits(mem, addr, options, tables, out);
  AddWaypointRooms(waypoint_rooms, out);
  if (options.exits) {
    AddExitRooms(out);
    ScanOpenBorders(mem, options, rooms, out);
    LabelPassages(out);
  }
}

MonsterClass ClassifyMonster(uint32_t mode, uint32_t monstats_flags, uint8_t type_flags,
                             bool is_ally, Alignment alignment) {
  if (mode == kModeDying || mode == kModeDead) return MonsterClass::kHidden;
  // TODO(RE): esqueleto, revivido e monstro convertido nao foram vistos (U sec. 7.1).
  if (is_ally) return MonsterClass::kAlly;
  if ((monstats_flags & kMonStatsNpc) != 0) return MonsterClass::kNpc;
  // Sem 0x8000: ambiente da cidade (galinhas etc.), nao e inimigo.
  if ((monstats_flags & kMonStatsKillable) == 0) return MonsterClass::kHidden;
  // Passa nos bits, mas nao e hostil: classes 359 e 522, sem no na lista (U sec. 4).
  if (alignment == Alignment::kFriendly) return MonsterClass::kFriendly;
  if (alignment == Alignment::kUnknown && (monstats_flags & kMonStatsInTown) != 0) {
    return MonsterClass::kFriendly;
  }
  // Superunico e campeao tambem tem o bit de unico: a ordem importa (U sec. 4).
  if ((type_flags & kMonsterSuperUnique) != 0) return MonsterClass::kSuperUnique;
  if ((type_flags & kMonsterChampion) != 0) return MonsterClass::kChampion;
  if ((type_flags & kMonsterUnique) != 0) return MonsterClass::kUnique;
  if ((type_flags & kMonsterMinion) != 0) return MonsterClass::kMinion;
  return MonsterClass::kNormal;
}

bool IsQuestOperateFn(uint8_t operate_fn) {
  switch (operate_fn) {
    case 6:
    case 9:
    case 10:
    case 12:
    case 21:
    case 24:
    case 25:
    case 28:
    case 31:
    case 39:
    case 40:
    case 41:
    case 42:
    case 49:
    case 52:
    case 53:
    case 54:
    case 55:
    case 56:
    case 57:
    case 58:
    case 59:
    case 65:
    case 67:
      return true;
    default:
      return false;
  }
}

ObjectKind ClassifyObject(uint32_t object_class, uint8_t sub_class, uint8_t operate_fn) {
  if ((sub_class & kSubClassWaypoint) != 0) return ObjectKind::kWaypoint;
  if ((sub_class & kSubClassPlayerOrRedPortal) != 0) {
    if (object_class == kObjectClassBluePortal) return ObjectKind::kPortalBlue;
    if (object_class == kObjectClassRedPortal) return ObjectKind::kPortalRed;
    return ObjectKind::kPortalOther;
  }
  if ((sub_class & kSubClassLevelPortal) != 0) return ObjectKind::kPortalOther;
  if (IsQuestOperateFn(operate_fn) || object_class == kObjectClassQuestPlaceholder ||
      object_class == kObjectClassQuestCage) {
    return ObjectKind::kQuest;
  }
  return ObjectKind::kOther;
}

int32_t FixedPortalDestination(uint32_t object_class, int32_t level_id) {
  if (object_class == kObjectClassArcanePortal) {
    if (level_id == 74) return 54;
    if (level_id == 54) return 74;
  }
  if (object_class == kObjectClassDurancePortal && level_id == 102) return 103;
  return -1;
}

bool IsTownLevel(int32_t level_id) {
  return level_id == 1 || level_id == 40 || level_id == 75 || level_id == 103 || level_id == 109;
}

bool SharedEdge(const TileRect& a, const TileRect& b, Point* from, Point* to) {
  if (a.x + a.w == b.x || b.x + b.w == a.x) {
    const int32_t x = a.x + a.w == b.x ? b.x : a.x;
    const int32_t y0 = std::max(a.y, b.y);
    const int32_t y1 = std::min(a.y + a.h, b.y + b.h);
    if (y1 > y0) {
      *from = Point{x, y0};
      *to = Point{x, y1};
      return true;
    }
  }
  if (a.y + a.h == b.y || b.y + b.h == a.y) {
    const int32_t y = a.y + a.h == b.y ? b.y : a.y;
    const int32_t x0 = std::max(a.x, b.x);
    const int32_t x1 = std::min(a.x + a.w, b.x + b.w);
    if (x1 > x0) {
      *from = Point{x0, y};
      *to = Point{x1, y};
      return true;
    }
  }
  return false;
}

bool RoomTouchesEdge(const TileRect& room, const Point& from, const Point& to) {
  if (from.x == to.x) {
    return (room.x == from.x || room.x + room.w == from.x) && room.y < to.y &&
           room.y + room.h > from.y;
  }
  return (room.y == from.y || room.y + room.h == from.y) && room.x < to.x &&
         room.x + room.w > from.x;
}

std::vector<EdgeRun> FreeRuns(const std::vector<EdgeCell>& cells) {
  std::vector<EdgeRun> runs;
  int32_t start = -1;
  const int32_t n = static_cast<int32_t>(cells.size());
  for (int32_t i = 0; i <= n; ++i) {
    const bool free_cell = i < n && cells[static_cast<size_t>(i)] == EdgeCell::kFree;
    if (free_cell && start < 0) start = i;
    if (!free_cell && start >= 0) {
      runs.push_back(EdgeRun{start, i - start});
      start = -1;
    }
  }
  return runs;
}

bool IsAct1RuleBorder(int32_t level_a, int32_t level_b) {
  const int32_t low = std::min(level_a, level_b);
  const int32_t high = std::max(level_a, level_b);
  return (low == 2 && high == 3) || (low == 3 && high == 4) || (low == 3 && high == 17) ||
         (low == 5 && high == 6) || (low == 6 && high == 7);
}

EdgeRun Act1RuleRun(int32_t room_origin_subtile) {
  return EdgeRun{room_origin_subtile + kAct1RuleFirstCell, kAct1RuleCells};
}

void ProjectMarks(const std::vector<Mark>& marks, const AutomapView& view,
                  std::vector<ScreenMark>* out) {
  out->clear();
  for (const Mark& m : marks) {
    ScreenMark s;
    s.kind = m.kind;
    s.points = m.points;
    s.primary = m.primary;
    s.used = m.used;
    s.dest_level = m.dest_level;
    s.label = m.label;
    int32_t min_x = 0;
    int32_t min_y = 0;
    int32_t max_x = 0;
    int32_t max_y = 0;
    for (uint8_t i = 0; i < m.points && i < 4; ++i) {
      const Point p = ClientToAutomap(m.x[i], m.y[i], view);
      s.x[i] = p.x;
      s.y[i] = p.y;
      min_x = i == 0 ? p.x : std::min(min_x, p.x);
      min_y = i == 0 ? p.y : std::min(min_y, p.y);
      max_x = i == 0 ? p.x : std::max(max_x, p.x);
      max_y = i == 0 ? p.y : std::max(max_y, p.y);
    }
    if (view.has_clip) {
      if (m.points == 1 && !InsideClip(Point{s.x[0], s.y[0]}, view)) continue;
      if (max_x < view.clip_left || min_x > view.clip_right || max_y < view.clip_top ||
          min_y > view.clip_bottom) {
        continue;
      }
    }
    out->push_back(s);
  }
}

CanvasMap FitFrame(int32_t frame_w, int32_t frame_h, int32_t canvas_w, int32_t canvas_h,
                   bool keep_aspect) {
  CanvasMap map;
  if (frame_w <= 0 || frame_h <= 0 || canvas_w <= 0 || canvas_h <= 0) return map;
  const double sx = static_cast<double>(canvas_w) / frame_w;
  const double sy = static_cast<double>(canvas_h) / frame_h;
  if (keep_aspect) {
    const double s = std::min(sx, sy);
    map.scale_x = s;
    map.scale_y = s;
    map.offset_x = (canvas_w - frame_w * s) / 2.0;
    map.offset_y = (canvas_h - frame_h * s) / 2.0;
  } else {
    map.scale_x = sx;
    map.scale_y = sy;
  }
  return map;
}

CanvasMap FitFrameD2gl(int32_t frame_w, int32_t frame_h, int32_t canvas_w, int32_t canvas_h,
                       int32_t window_w, int32_t window_h, bool stretch_h, bool stretch_v) {
  CanvasMap map;
  if (frame_w <= 0 || frame_h <= 0 || canvas_w <= 0 || canvas_h <= 0 || window_w <= 0 ||
      window_h <= 0) {
    return map;
  }
  // Canvas -> area cliente (o canvas so e reduzido acima de overlay_max_width).
  const double r = canvas_w < window_w ? static_cast<double>(canvas_w) / window_w : 1.0;
  const double client_h = canvas_h / r;
  const double s = std::min(static_cast<double>(window_w) / frame_w,
                            static_cast<double>(window_h) / frame_h);
  const double sx = stretch_h ? static_cast<double>(window_w) / frame_w : s;
  const double sy = stretch_v ? static_cast<double>(window_h) / frame_h : s;
  map.scale_x = sx * r;
  map.scale_y = sy * r;
  map.offset_x = (window_w - frame_w * sx) / 2.0 * r;
  map.offset_y = ((window_h - frame_h * sy) / 2.0 + (client_h - window_h)) * r;
  return map;
}

}  // namespace map_overlay
