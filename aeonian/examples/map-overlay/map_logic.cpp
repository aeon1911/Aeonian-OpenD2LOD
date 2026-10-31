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
// monstats+0x0C: 0x100 so nos NPCs, 0x8000 nos monstros hostis (U sec. 4).
constexpr uint32_t kMonStatsNpc = 0x100;
constexpr uint32_t kMonStatsKillable = 0x8000;
// MonsterData+0x16 (U sec. 4).
constexpr uint8_t kMonsterSuperUnique = 0x02;
constexpr uint8_t kMonsterChampion = 0x04;
constexpr uint8_t kMonsterUnique = 0x08;
constexpr uint8_t kMonsterMinion = 0x10;

// Lista de aliados (U sec. 7.1).
constexpr uint32_t kAllyId = 0x08;
constexpr uint32_t kAllyOwner = 0x0C;
constexpr uint32_t kAllyNext = 0x30;
constexpr uint32_t kAllyBlock = 0x34;
constexpr uint32_t kMaxAllies = 512;

// Registro de objects (U sec. 5.1) e ObjectData (U sec. 5.2).
constexpr uint32_t kObjectRecordSize = 0x1C0;
constexpr uint32_t kObjectSubClass = 0x167;
constexpr uint8_t kSubClassPlayerOrRedPortal = 0x04;
constexpr uint8_t kSubClassLevelPortal = 0x10;
constexpr uint8_t kSubClassWaypoint = 0x40;
constexpr uint32_t kObjectClassBluePortal = 59;
constexpr uint32_t kObjectClassRedPortal = 60;
constexpr uint32_t kObjectDataDestLevel = 0x04;  // BYTE

// Estado de UI e painel (L sec. 5.4).
constexpr uint32_t kUiNpcDialog = 8;
constexpr uint32_t kUiEscMenu = 9;
constexpr uint32_t kUiHelp = 33;
constexpr int32_t kPanelRight = 1;
constexpr int32_t kPanelLeft = 2;
constexpr int32_t kPanelBoth = 3;

// Sala ativa (L sec. 2.5).
constexpr uint32_t kRoom1Room2 = 0x10;

// Sala do DRLG (L sec. 2.4).
constexpr uint32_t kRoom2Neighbors = 0x08;
constexpr uint32_t kRoom2LoadCount = 0x0E;  // WORD
constexpr uint32_t kRoom2Next = 0x24;
constexpr uint32_t kRoom2Flags = 0x28;
constexpr uint32_t kRoom2NeighborCount = 0x2C;
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

// Nivel (L sec. 2.3).
constexpr uint32_t kLevelFirstRoom2 = 0x10;
constexpr uint32_t kLevelTileX = 0x1C;  // X, Y, largura, altura; validos antes da geracao
constexpr uint32_t kLevelNext = 0x1AC;
constexpr uint32_t kLevelDrlgAct = 0x1B4;
constexpr uint32_t kLevelId = 0x1D0;

// DrlgAct e a lista de excecoes de vizinhanca (L sec. 2.2).
constexpr uint32_t kDrlgActExceptions = 0x90;
constexpr uint32_t kDrlgActFirstLevel = 0x47C;
constexpr uint32_t kExceptionId = 0x00;
constexpr uint32_t kExceptionVis = 0x04;
constexpr uint32_t kExceptionWarp = 0x24;
constexpr uint32_t kExceptionNext = 0x44;

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
constexpr uint32_t kMaxNeighbors = 64;
constexpr uint32_t kMaxPresetsPerRoom = 512;
constexpr uint32_t kMaxUnitsPerType = 4096;
constexpr uint32_t kMaxExceptions = 512;
constexpr uint32_t kMaxLevels = 256;
constexpr int32_t kMaxLevelId = 1024;
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
};

struct Tables {
  uint32_t objects_base = 0;
  uint32_t objects_count = 0;
  uint32_t player_id = 0;
  std::vector<AllyEntry> allies;
};

// So ids de unidades tipo 1 sao comparados: os ids sao por tipo (U sec. 7.1).
void ReadAllies(const MemoryView& mem, const GameAddresses& addr, Tables* tables) {
  uint32_t node = 0;
  if (!ReadU32(mem, addr.ally_list, &node)) return;
  for (uint32_t i = 0; node != 0 && i < kMaxAllies; ++i) {
    Block<kAllyBlock> b;
    if (!b.Load(mem, node)) break;
    tables->allies.push_back(AllyEntry{b.U32(kAllyId), b.U32(kAllyOwner)});
    node = b.U32(kAllyNext);
  }
}

// Le o que um objeto e. false se nao for para marcar. O que o jogo classifica
// (waypoint, portal) nao precisa de configuracao; [objects] rotula o resto.
// TODO(RE): objetos de quest nao tem coluna propria (U sec. 5.1); OperateFn e
// AutoMap sao so pistas sem teste, entao eles dependem de [objects].
bool IdentifyObject(const MemoryView& mem, const ScanOptions& options, const Tables& tables,
                    uint32_t object_class, ObjectKind* kind, int32_t* label) {
  *kind = ObjectKind::kOther;
  *label = -1;
  if (tables.objects_base != 0 && object_class < tables.objects_count) {
    uint8_t sub_class = 0;
    if (mem.Read(tables.objects_base + object_class * kObjectRecordSize + kObjectSubClass,
                 &sub_class, 1)) {
      *kind = ClassifyObject(object_class, sub_class);
    }
  }
  if (*kind != ObjectKind::kOther) return true;
  *label = FindObjectLabel(options, object_class);
  return *label >= 0;
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

// A predefinida vem antes da unidade: a mesma marca ganha o destino que so a
// unidade conhece.
void AddObject(const Point& at, ObjectKind kind, int32_t label, uint32_t object_class,
               int32_t dest_level, Scan* scan) {
  const MarkKind mark_kind = MarkOfObject(kind);
  for (Mark& m : scan->marks) {
    if (m.kind == mark_kind && m.label == label && SameSpot(m, at)) {
      if (dest_level > 0) m.dest_level = dest_level;
      return;
    }
  }
  Mark mark;
  mark.kind = mark_kind;
  mark.x[0] = at.x;
  mark.y[0] = at.y;
  mark.label = label;
  mark.unit_class = static_cast<int32_t>(object_class);
  mark.dest_level = dest_level;
  scan->marks.push_back(mark);
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
    default:
      ++scan->stats.objects;
      break;
  }
}

// Com Warp[n] != -1 a vizinha e a sala de destino de um warp ja ligado, nao
// uma borda (L sec. 4.1). Sem Vis/Warp nao ha como distinguir: SharedEdge ainda
// exige que as salas se toquem.
bool IsOpenBorderWith(const Scan& scan, int32_t dest) {
  if (!scan.links_ok) return true;
  for (int n = 0; n < 8; ++n) {
    if (scan.vis[n] == dest && scan.warp[n] == -1) return true;
  }
  return false;
}

// Bordas com salas de outros niveis (L sec. 4.1): a lista de vizinhas so existe
// depois que a sala foi ativada.
void ScanPassages(const MemoryView& mem, uint32_t room, const Block<kRoom2Block>& r,
                  uint32_t level, Scan* scan) {
  const uint32_t list = r.U32(kRoom2Neighbors);
  const uint32_t count = r.U32(kRoom2NeighborCount);
  if (list == 0 || count > kMaxNeighbors) return;
  const TileRect rect = RoomRect(r);
  if (!ValidRect(rect)) return;
  for (uint32_t k = 0; k < count; ++k) {
    uint32_t neighbor = 0;
    if (!ReadU32(mem, list + 4 * k, &neighbor) || neighbor == 0 || neighbor == room) continue;
    Block<kRoom2Block> nb;
    if (!nb.Load(mem, neighbor)) continue;
    const uint32_t other = nb.U32(kRoom2Level);
    if (other == 0 || other == level) continue;
    int32_t dest = 0;
    if (!ReadI32(mem, other + kLevelId, &dest) || dest <= 0 || dest >= kMaxLevelId) continue;
    if (!IsOpenBorderWith(*scan, dest)) continue;
    const TileRect nrect = RoomRect(nb);
    Point from;
    Point to;
    if (!ValidRect(nrect) || !SharedEdge(rect, nrect, &from, &to)) continue;
    Mark mark;
    mark.kind = MarkKind::kPassage;
    mark.points = 2;
    const Point a = TileToClient(from.x, from.y);
    const Point b = TileToClient(to.x, to.y);
    mark.x[0] = a.x;
    mark.y[0] = a.y;
    mark.x[1] = b.x;
    mark.y[1] = b.y;
    mark.dest_level = dest;
    scan->marks.push_back(mark);
    ++scan->stats.passages;
  }
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
    } else if (type == kTypeObject && options.objects) {
      ObjectKind kind = ObjectKind::kOther;
      int32_t label = -1;
      if (IdentifyObject(mem, options, tables, object_class, &kind, &label)) {
        AddObject(at, kind, label, object_class, -1, scan);
      }
    }
    preset = p.U32(kPresetNext);
  }
}

void ScanRooms(const MemoryView& mem, const ScanOptions& options, const Tables& tables,
               Scan* scan, std::vector<TileRect>* waypoint_rooms) {
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
      ScanPassages(mem, room, r, scan->level, scan);
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

void ScanMonster(const MemoryView& mem, const Block<kUnitBlock>& u,
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
  const MonsterClass kind =
      ClassifyMonster(u.U32(kUnitMode), monstats_flags, type_flags, ally != nullptr);
  if (kind == MonsterClass::kHidden) return;
  if (kind == MonsterClass::kNpc && !options.npcs) return;
  if (kind == MonsterClass::kAlly && !options.allies) return;
  if (kind != MonsterClass::kNpc && kind != MonsterClass::kAlly && !options.monsters) return;
  Block<kDynBlock> path;
  if (!path.Load(mem, u.U32(kUnitPath))) return;
  if (options.only_current_level && LevelOfRoom1(mem, path.U32(kDynRoom1)) != scan->level) {
    return;
  }
  Mark mark;
  mark.x[0] = path.I32(kDynClientX);
  mark.y[0] = path.I32(kDynClientY);
  mark.unit_class = static_cast<int32_t>(monster_class);
  switch (kind) {
    case MonsterClass::kNpc:
      mark.kind = MarkKind::kNpc;
      ++scan->stats.npcs;
      break;
    case MonsterClass::kAlly:
      mark.kind = MarkKind::kAlly;
      mark.own = ally != nullptr && ally->owner == tables.player_id;
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
  if (kind != MonsterClass::kNpc && kind != MonsterClass::kAlly) ++scan->stats.monsters;
  scan->marks.push_back(mark);
}

// Objetos e tiles de saida pela tabela de unidades (caminho estatico).
void ScanStaticUnit(const MemoryView& mem, const Block<kUnitBlock>& u, uint32_t type,
                    const ScanOptions& options, const Tables& tables, Scan* scan) {
  const uint32_t unit_class = u.U32(kUnitClass);
  ObjectKind kind = ObjectKind::kOther;
  int32_t label = -1;
  if (type == kTypeObject &&
      !IdentifyObject(mem, options, tables, unit_class, &kind, &label)) {
    return;
  }
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
  // Destino so nas classes 59 e 60 (U sec. 5.3).
  // TODO(RE): destino das classes 298 e 342 (o campo vale 0 na 298).
  int32_t dest = -1;
  if (kind == ObjectKind::kPortalBlue || kind == ObjectKind::kPortalRed) {
    uint8_t value = 0;
    if (mem.Read(u.U32(kUnitData) + kObjectDataDestLevel, &value, 1) && value != 0) dest = value;
  }
  AddObject(at, kind, label, unit_class, dest, scan);
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
    if (type == kTypeObject && !options.objects) continue;
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
            ScanMonster(mem, u, options, tables, monstats_base, monstats_count, scan);
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

// Vizinhos abertos sem nenhuma sala ligada ainda: o trecho comum dos retangulos
// dos niveis diz onde a passagem esta, nao a passagem (L sec. 4.1).
// TODO(RE): a posicao exata fica em alguma estrutura do gerador do mapa externo.
void AddApproxPassages(const MemoryView& mem, Scan* scan) {
  uint32_t drlg_act = 0;
  const TileRect self = scan->level_rect;
  if (!scan->links_ok || !ValidRect(self) ||
      !ReadU32(mem, scan->level + kLevelDrlgAct, &drlg_act)) {
    return;
  }
  std::vector<int32_t> done;
  for (const Mark& m : scan->marks) {
    if (m.kind == MarkKind::kPassage) done.push_back(m.dest_level);
  }
  for (int n = 0; n < 8; ++n) {
    const int32_t dest = scan->vis[n];
    if (dest <= 0 || scan->warp[n] != -1) continue;
    if (std::find(done.begin(), done.end(), dest) != done.end()) continue;
    done.push_back(dest);
    // So leitura: o nivel pode ainda nao existir (nao chama GetLevel).
    uint32_t other = 0;
    if (!ReadU32(mem, drlg_act + kDrlgActFirstLevel, &other)) return;
    for (uint32_t i = 0; other != 0 && i < kMaxLevels; ++i) {
      int32_t id = 0;
      if (!ReadI32(mem, other + kLevelId, &id)) break;
      if (id == dest) break;
      if (!ReadU32(mem, other + kLevelNext, &other)) other = 0;
    }
    TileRect rect;
    Point from;
    Point to;
    if (other == 0 || !ReadLevelRect(mem, other, &rect) ||
        !SharedEdge(self, rect, &from, &to)) {
      continue;
    }
    Mark mark;
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
  scan->stats.passage_levels = static_cast<uint32_t>(done.size());
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

std::vector<std::string> RequiredSymbols() {
  std::vector<std::string> names;
  for (const AnchorSpec& a : Anchors()) {
    if (std::find(names.begin(), names.end(), a.symbol) == names.end()) {
      names.emplace_back(a.symbol);
    }
  }
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
    if (std::find(done.begin(), done.end(), r.room) == done.end()) return &r;
  }
  return nullptr;
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
  ReadAllies(mem, addr, &tables);
  if (ReadU32(mem, addr.objects_base, &tables.objects_base)) {
    ReadU32(mem, addr.objects_count, &tables.objects_count);
  }
  std::vector<TileRect> waypoint_rooms;
  ScanRooms(mem, options, tables, out, &waypoint_rooms);
  ScanUnits(mem, addr, options, tables, out);
  AddWaypointRooms(waypoint_rooms, out);
  if (options.exits) {
    AddExitRooms(out);
    LabelPassages(out);
    AddApproxPassages(mem, out);
  }
}

MonsterClass ClassifyMonster(uint32_t mode, uint32_t monstats_flags, uint8_t type_flags,
                             bool is_ally) {
  if (mode == kModeDying || mode == kModeDead) return MonsterClass::kHidden;
  // Mercenario e hidras passam nos bits de monstats: a lista vem antes (U sec. 4).
  // TODO(RE): so mercenario (tipo 7) e hidra (tipo 19) foram vistos na lista, com
  // um jogador; golem, esqueleto, valquiria e lobos nao (U sec. 7.1).
  if (is_ally) return MonsterClass::kAlly;
  if ((monstats_flags & kMonStatsNpc) != 0) return MonsterClass::kNpc;
  // Sem 0x8000: ambiente da cidade (galinhas etc.), nao e inimigo.
  // TODO(RE): a classe 359 passa nestes bits na cidade do Ato III (vista nas tres
  // dificuldades) e aparece como hostil; o filtro so foi conferido numa amostra de
  // classes (U sec. 8, item 6) e a coluna que a distingue nao esta na spec.
  if ((monstats_flags & kMonStatsKillable) == 0) return MonsterClass::kHidden;
  // Superunico e campeao tambem tem o bit de unico: a ordem importa (U sec. 4).
  if ((type_flags & kMonsterSuperUnique) != 0) return MonsterClass::kSuperUnique;
  if ((type_flags & kMonsterChampion) != 0) return MonsterClass::kChampion;
  if ((type_flags & kMonsterUnique) != 0) return MonsterClass::kUnique;
  if ((type_flags & kMonsterMinion) != 0) return MonsterClass::kMinion;
  return MonsterClass::kNormal;
}

ObjectKind ClassifyObject(uint32_t object_class, uint8_t sub_class) {
  if ((sub_class & kSubClassWaypoint) != 0) return ObjectKind::kWaypoint;
  if ((sub_class & kSubClassPlayerOrRedPortal) != 0) {
    if (object_class == kObjectClassBluePortal) return ObjectKind::kPortalBlue;
    if (object_class == kObjectClassRedPortal) return ObjectKind::kPortalRed;
    return ObjectKind::kPortalOther;
  }
  if ((sub_class & kSubClassLevelPortal) != 0) return ObjectKind::kPortalOther;
  return ObjectKind::kOther;
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

void ProjectMarks(const std::vector<Mark>& marks, const AutomapView& view,
                  std::vector<ScreenMark>* out) {
  out->clear();
  for (const Mark& m : marks) {
    ScreenMark s;
    s.kind = m.kind;
    s.points = m.points;
    s.primary = m.primary;
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
