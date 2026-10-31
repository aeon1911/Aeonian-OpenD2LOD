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

// Testes da logica portavel do mod map-overlay. Rodam no host com uma memoria
// sintetica montada a partir dos layouts das specs; nao tocam no jogo.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "core/address_db.h"
#include "map_logic.h"

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

using namespace map_overlay;

// Memoria de 32 bits feita de blocos. Le fora dos blocos devolve false, como
// a leitura protegida do plugin faz com enderecos invalidos.
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
    next_ += (size + 15u) / 16u * 16u + 16u;  // folga: blocos nao se tocam
    segments_.push_back(Segment{base, std::vector<uint8_t>(size, 0)});
    return base;
  }
  void Map(uint32_t base, const std::vector<uint8_t>& bytes) {
    segments_.push_back(Segment{base, bytes});
  }
  void Put32(uint32_t address, uint32_t value) { Put(address, &value, 4); }
  void PutI(uint32_t address, int32_t value) { Put(address, &value, 4); }
  void Put16(uint32_t address, uint16_t value) { Put(address, &value, 2); }
  void Put8(uint32_t address, uint8_t value) { Put(address, &value, 1); }

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
  uint32_t next_ = 0x01000000;
};

// --- Ancoras x banco de enderecos real ---------------------------------------

constexpr uint32_t kImageBase = 0x400000;

struct DbResolver {
  const aeonian::AddressDb* db;
  std::vector<std::string> refuse;  // simbolos tratados como "bytes nao conferem"
};

uint32_t ResolveFromDb(void* context, const char* name) {
  auto* r = static_cast<DbResolver*>(context);
  for (const std::string& s : r->refuse) {
    if (s == name) return 0;
  }
  const aeonian::Symbol* symbol = r->db->Find(name);
  return symbol == nullptr ? 0 : kImageBase + symbol->rva;
}

std::string ReadText(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  std::stringstream buf;
  buf << f.rdbuf();
  return buf.str();
}

void TestAnchorsAgainstRealIni() {
  std::string error;
  auto db = aeonian::AddressDb::Parse(
      ReadText(std::string(AEONIAN_SOURCE_DIR) + "/addresses/1.14d.ini"), &error);
  CHECK(db.has_value());
  if (!db) return;
  // A "imagem" sintetica tem, em cada simbolo, os bytes que o motor confere.
  FakeMemory mem;
  for (const std::string& name : RequiredSymbols()) {
    const aeonian::Symbol* s = db->Find(name);
    CHECK(s != nullptr);
    if (s == nullptr) continue;
    CHECK(!s->expect.empty());
    mem.Map(kImageBase + s->rva, s->expect);
  }
  DbResolver resolver{&*db, {}};
  GameAddresses a;
  ResolveReport report;
  ResolveAddresses(mem, ResolveFromDb, &resolver, &a, &report);
  CHECK_EQ(report.resolved, report.total);
  CHECK_EQ(report.total, 18u);
  CHECK(report.missing.empty());
  CHECK(report.notes.empty());
  // Os VAs que as specs documentam.
  CHECK_EQ(a.player_unit, 0x7A6A70u);
  CHECK_EQ(a.current_act, 0x7A0634u);
  CHECK_EQ(a.server_units, 0x7A5E70u);
  CHECK_EQ(a.data_tables, 0x744304u);
  CHECK_EQ(a.level_defs, 0x96C890u);
  CHECK_EQ(a.automap_visible, 0x7A27E8u);
  CHECK_EQ(a.automap_mode, 0x7A5150u);
  CHECK_EQ(a.automap_divisor, 0x711254u);
  CHECK_EQ(a.automap_offset_x, 0x7A5198u);
  CHECK_EQ(a.automap_offset_y, 0x7A519Cu);
  CHECK_EQ(a.automap_clip, 0x7A51C8u);
  CHECK_EQ(a.panel_side, 0x7A5210u);
  CHECK_EQ(a.ui_state, 0x7A27C0u);
  CHECK_EQ(a.frame_width, 0x71146Cu);
  CHECK_EQ(a.frame_height, 0x711470u);
  CHECK_EQ(a.ally_list, 0x7BB5BCu);
  CHECK_EQ(a.objects_base, 0x96D470u);
  CHECK_EQ(a.objects_count, 0x96D474u);
  const uint32_t all = kFeatureAutomap | kFeatureMonsters | kFeatureObjectUnits |
                       kFeatureLevelLinks | kFeatureClip | kFeatureAllies |
                       kFeatureObjectTable | kFeaturePanels | kFeatureFrameSize;
  CHECK_EQ(AvailableFeatures(a), all);

  // Um simbolo recusado desliga so o que depende dele.
  resolver.refuse = {"D2Client.FindServerSideUnit"};
  ResolveAddresses(mem, ResolveFromDb, &resolver, &a, &report);
  CHECK_EQ(report.resolved + 1, report.total);
  CHECK(report.missing.size() == 1 && report.missing[0] == "D2Client.FindServerSideUnit");
  CHECK_EQ(a.server_units, 0u);
  CHECK_EQ(AvailableFeatures(a), all & ~(kFeatureMonsters | kFeatureObjectUnits));

  // Sem a ancora principal do recorte, a de AutomapUnitToScreen ainda o da.
  resolver.refuse = {"D2Client.AutomapClipCheck"};
  ResolveAddresses(mem, ResolveFromDb, &resolver, &a, &report);
  CHECK_EQ(a.automap_clip, 0x7A51C8u);

  // Cada recurso novo depende so do proprio simbolo.
  resolver.refuse = {"D2Client.FindPetEntry", "D2Client.RemovePetEntry"};
  ResolveAddresses(mem, ResolveFromDb, &resolver, &a, &report);
  CHECK_EQ(a.ally_list, 0u);
  CHECK_EQ(AvailableFeatures(a), all & ~static_cast<uint32_t>(kFeatureAllies));
  resolver.refuse = {"D2Common.ObjectsTxtBase"};
  ResolveAddresses(mem, ResolveFromDb, &resolver, &a, &report);
  CHECK_EQ(AvailableFeatures(a), all & ~static_cast<uint32_t>(kFeatureObjectTable));
  resolver.refuse = {"D2Client.GetUiState"};
  ResolveAddresses(mem, ResolveFromDb, &resolver, &a, &report);
  CHECK_EQ(AvailableFeatures(a), all & ~static_cast<uint32_t>(kFeaturePanels));
  resolver.refuse = {"D2Client.ScreenSizeAnchor"};
  ResolveAddresses(mem, ResolveFromDb, &resolver, &a, &report);
  CHECK_EQ(AvailableFeatures(a), all & ~static_cast<uint32_t>(kFeatureFrameSize));

  // Sem o jogador nao ha nada a desenhar.
  resolver.refuse = {"D2Client.GetPlayerUnit"};
  ResolveAddresses(mem, ResolveFromDb, &resolver, &a, &report);
  CHECK_EQ(AvailableFeatures(a) & kFeatureAutomap, 0u);

  // Funcao desviada: a memoria viva tem o jmp por cima da ancora em +2. Lida
  // dela, o endereco do ato sairia errado; por isso a ancora vem dos originais
  // (OriginalMemory). Os demais globais nao mudam.
  resolver.refuse.clear();
  const aeonian::Symbol* draw = db->Find("D2Client.DrawAutomap");
  std::vector<uint8_t> hooked = draw->expect;
  const uint8_t jmp[7] = {0xE9, 0x11, 0x22, 0x33, 0x44, 0x90, 0x90};
  std::memcpy(hooked.data(), jmp, sizeof(jmp));
  FakeMemory live;
  for (const std::string& name : RequiredSymbols()) {
    const aeonian::Symbol* s = db->Find(name);
    live.Map(kImageBase + s->rva, s == draw ? hooked : s->expect);
  }
  ResolveAddresses(live, ResolveFromDb, &resolver, &a, &report);
  CHECK(a.current_act != 0x7A0634u);
  ResolveAddresses(mem, ResolveFromDb, &resolver, &a, &report);
  CHECK_EQ(a.current_act, 0x7A0634u);

  // Duas ancoras do mesmo global que discordam: o global fica em 0.
  resolver.refuse.clear();
  const aeonian::Symbol* monstats = db->Find("D2Common.MonStatsRecordAccess");
  std::vector<uint8_t> altered = monstats->expect;
  altered[2] ^= 0x10;
  FakeMemory mem2;
  for (const std::string& name : RequiredSymbols()) {
    const aeonian::Symbol* s = db->Find(name);
    mem2.Map(kImageBase + s->rva, s == monstats ? altered : s->expect);
  }
  ResolveAddresses(mem2, ResolveFromDb, &resolver, &a, &report);
  CHECK_EQ(a.data_tables, 0u);
  CHECK(!report.notes.empty());
  CHECK_EQ(AvailableFeatures(a) & (kFeatureMonsters | kFeatureLevelLinks), 0u);
}

// --- Formula, recorte e classificacao ----------------------------------------

void TestFormula() {
  // Exemplo da spec de unidades sec. 3.1: jogador em (5653, 5523).
  const Point c = SubtileToClient(5653, 5523);
  CHECK_EQ(c.x, 2080);
  CHECK_EQ(c.y, 89408);
  CHECK_EQ(TileToClient(2, 1).x, (10 - 5) * 16);
  CHECK_EQ(TileToClient(2, 1).y, (10 + 5) * 8);

  AutomapView v;
  v.divisor = 10;
  v.offset_x = 100;
  v.offset_y = 8000;
  Point p = ClientToAutomap(2080, 89416, v);
  CHECK_EQ(p.x, 208 - 100 + 8);
  CHECK_EQ(p.y, 8941 - 8000 - 8);
  // Divisao inteira com sinal (truncada para zero, como no jogo).
  p = ClientToAutomap(-2085, 89416, v);
  CHECK_EQ(p.x, -208 - 100 + 8);
  // Modo minimapa: divisor 20 e o ajuste da cruz do jogo (x - 1, y + 5).
  v.mode = 1;
  v.divisor = 20;
  p = ClientToAutomap(2080, 89416, v);
  CHECK_EQ(p.x, 104 - 100 + 8 - 1);
  CHECK_EQ(p.y, 4470 - 8000 - 8 + 5);
  CHECK(ClientToAutomap(1, 1, AutomapView{}).x == 0);  // divisor 0: nada

  AutomapView clip;
  clip.divisor = 10;
  clip.has_clip = true;
  clip.clip_left = -16;
  clip.clip_top = -16;
  clip.clip_right = 800;
  clip.clip_bottom = 600;
  CHECK(InsideClip(Point{-16, 600}, clip));
  CHECK(InsideClip(Point{800, -16}, clip));
  CHECK(!InsideClip(Point{801, 10}, clip));
  CHECK(!InsideClip(Point{10, -17}, clip));

  std::vector<Mark> marks(3);
  marks[0].x[0] = 100 * 10;
  marks[0].y[0] = 100 * 10 + 80;  // -> (108, 100)
  marks[1].x[0] = 900 * 10;       // -> fora (x = 908)
  marks[2].kind = MarkKind::kPassage;
  marks[2].points = 2;
  marks[2].x[0] = 790 * 10;
  marks[2].x[1] = 900 * 10;  // segmento que entra no recorte: fica
  std::vector<ScreenMark> out;
  ProjectMarks(marks, clip, &out);
  CHECK_EQ(out.size(), 2u);
  if (out.size() == 2) {
    CHECK_EQ(out[0].x[0], 108);
    CHECK_EQ(out[0].y[0], 100);
    CHECK(out[1].kind == MarkKind::kPassage);
  }
}

void TestClassify() {
  // Valores de monstats+0x0C observados (spec de unidades sec. 4).
  CHECK(ClassifyMonster(1, 0x02418803, 0, false) == MonsterClass::kNormal);   // ZM
  CHECK(ClassifyMonster(2, 0x12018001, 0, false) == MonsterClass::kNormal);   // SI
  CHECK(ClassifyMonster(0, 0x12018001, 0, false) == MonsterClass::kHidden);   // morrendo
  CHECK(ClassifyMonster(12, 0x12018001, 0, false) == MonsterClass::kHidden);  // cadaver
  CHECK(ClassifyMonster(1, 0x03010703, 0, false) == MonsterClass::kNpc);      // Akara
  CHECK(ClassifyMonster(1, 0x02010703, 0, false) == MonsterClass::kNpc);      // Kashya
  CHECK(ClassifyMonster(1, 0x02018502, 0, false) == MonsterClass::kNpc);      // RG
  CHECK(ClassifyMonster(1, 0x02010402, 0, false) == MonsterClass::kHidden);   // CK
  CHECK(ClassifyMonster(1, 0x02010403, 0, false) == MonsterClass::kHidden);   // CW
  CHECK(ClassifyMonster(1, 0x02040401, 0, false) == MonsterClass::kHidden);   // 567-569
  // Bytes de tipo vistos ao vivo: superunico e campeao tem tambem o bit de unico.
  CHECK(ClassifyMonster(1, 0x02418803, 0x0A, false) == MonsterClass::kSuperUnique);
  CHECK(ClassifyMonster(1, 0x02418803, 0x0C, false) == MonsterClass::kChampion);
  CHECK(ClassifyMonster(1, 0x02418803, 0x08, false) == MonsterClass::kUnique);
  CHECK(ClassifyMonster(1, 0x02418803, 0x10, false) == MonsterClass::kMinion);
  CHECK(ClassifyMonster(1, 0x02418803, 0x02, false) == MonsterClass::kSuperUnique);
  CHECK(ClassifyMonster(1, 0x02418803, 0x04, false) == MonsterClass::kChampion);
  CHECK(ClassifyMonster(1, 0x02418803, 0x00, false) == MonsterClass::kNormal);
  // Aliado passa nos bits de hostil, mas a lista vem antes; morto continua oculto.
  CHECK(ClassifyMonster(1, 0x02418803, 0x00, true) == MonsterClass::kAlly);
  CHECK(ClassifyMonster(1, 0x02418803, 0x0C, true) == MonsterClass::kAlly);
  CHECK(ClassifyMonster(12, 0x02418803, 0x00, true) == MonsterClass::kHidden);

  // Colunas de objects (spec de unidades sec. 5).
  for (uint32_t cls : {119u, 156u, 237u, 238u, 324u, 402u, 496u, 511u}) {
    CHECK(ClassifyObject(cls, 0x40) == ObjectKind::kWaypoint);
  }
  CHECK(ClassifyObject(59, 0x04) == ObjectKind::kPortalBlue);
  CHECK(ClassifyObject(60, 0x04) == ObjectKind::kPortalRed);
  CHECK(ClassifyObject(298, 0x10) == ObjectKind::kPortalOther);
  CHECK(ClassifyObject(342, 0x10) == ObjectKind::kPortalOther);
  CHECK(ClassifyObject(267, 0x08) == ObjectKind::kOther);  // bau
  CHECK(ClassifyObject(2, 0x01) == ObjectKind::kOther);    // santuario
  CHECK(ClassifyObject(3, 0x00) == ObjectKind::kOther);
  // A classe sozinha nao faz waypoint nem portal: vale a coluna.
  CHECK(ClassifyObject(119, 0x00) == ObjectKind::kOther);
  CHECK(ClassifyObject(59, 0x00) == ObjectKind::kOther);

  for (int32_t town : {1, 40, 75, 103, 109}) CHECK(IsTownLevel(town));
  for (int32_t other : {0, 2, 41, 74, 76, 104, 108, 110, 137}) CHECK(!IsTownLevel(other));
}

// Paineis e tamanho do quadro (spec de niveis sec. 5.4 e 5.5).
void TestPanels() {
  AutomapView full;
  full.visible = true;
  full.divisor = 10;
  full.has_clip = true;
  full.clip_left = -16;
  full.clip_top = -16;
  full.clip_right = 800;
  full.clip_bottom = 600;
  full.frame_width = 800;
  full.frame_height = 600;

  AutomapView v = full;
  ApplyPanelRules(&v);
  CHECK(AutomapDrawable(v));
  CHECK_EQ(v.clip_left, -16);
  CHECK_EQ(v.clip_right, 800);

  v = full;
  v.panel_side = 1;  // a direita: so x < 400
  ApplyPanelRules(&v);
  CHECK(AutomapDrawable(v));
  CHECK_EQ(v.clip_left, -16);
  CHECK_EQ(v.clip_right, 399);
  CHECK(InsideClip(Point{399, 10}, v));
  CHECK(!InsideClip(Point{400, 10}, v));

  v = full;
  v.panel_side = 2;  // a esquerda: so x >= 400
  ApplyPanelRules(&v);
  CHECK_EQ(v.clip_left, 400);
  CHECK_EQ(v.clip_right, 800);
  CHECK(InsideClip(Point{400, 10}, v));
  CHECK(!InsideClip(Point{399, 10}, v));

  // 640x480: o corte acompanha o tamanho do quadro.
  v = full;
  v.clip_right = 640;
  v.clip_bottom = 480;
  v.frame_width = 640;
  v.frame_height = 480;
  v.panel_side = 1;
  ApplyPanelRules(&v);
  CHECK_EQ(v.clip_right, 319);
  v.panel_side = 2;
  v.clip_right = 640;
  ApplyPanelRules(&v);
  CHECK_EQ(v.clip_left, 320);

  // Marcas do lado do painel somem na projecao.
  std::vector<Mark> marks(2);
  marks[0].x[0] = 100 * 10;  // -> x = 108
  marks[1].x[0] = 500 * 10;  // -> x = 508
  std::vector<ScreenMark> out;
  v = full;
  v.panel_side = 1;
  ApplyPanelRules(&v);
  ProjectMarks(marks, v, &out);
  CHECK_EQ(out.size(), 1u);
  if (!out.empty()) CHECK_EQ(out[0].x[0], 108);
  v = full;
  v.panel_side = 2;
  ApplyPanelRules(&v);
  ProjectMarks(marks, v, &out);
  CHECK_EQ(out.size(), 1u);
  if (!out.empty()) CHECK_EQ(out[0].x[0], 508);

  // Os dois lados, dialogo, menu e ajuda: nada e desenhado.
  v = full;
  v.panel_side = 3;
  ApplyPanelRules(&v);
  CHECK(!AutomapDrawable(v));
  v = full;
  v.npc_dialog = true;  // lado 0, automap ligado
  ApplyPanelRules(&v);
  CHECK(!AutomapDrawable(v));
  v = full;
  v.esc_menu = true;
  ApplyPanelRules(&v);
  CHECK(!AutomapDrawable(v));
  v = full;
  v.help = true;
  ApplyPanelRules(&v);
  CHECK(!AutomapDrawable(v));
  v = full;
  v.visible = false;
  ApplyPanelRules(&v);
  CHECK(!AutomapDrawable(v));

  // Minimapa: o recorte do jogo ja acompanha o painel e nao e cortado.
  AutomapView mini = full;
  mini.mode = 1;
  mini.divisor = 20;
  mini.clip_left = 525;
  mini.clip_top = 78;
  mini.clip_right = 799;
  mini.clip_bottom = 278;
  mini.panel_side = 2;
  ApplyPanelRules(&mini);
  CHECK(AutomapDrawable(mini));
  CHECK_EQ(mini.clip_left, 525);
  CHECK_EQ(mini.clip_right, 799);
  mini.clip_left = -8;
  mini.clip_right = 266;
  mini.panel_side = 1;
  ApplyPanelRules(&mini);
  CHECK(AutomapDrawable(mini));
  CHECK_EQ(mini.clip_right, 266);

  // Sem o tamanho do quadro: a direita do recorte em tela cheia e a largura; sem
  // nenhum dos dois nao ha como cortar, entao nada e desenhado.
  v = full;
  v.frame_width = 0;
  v.panel_side = 1;
  ApplyPanelRules(&v);
  CHECK(AutomapDrawable(v));
  CHECK_EQ(v.clip_right, 399);
  v = full;
  v.frame_width = 0;
  v.has_clip = false;
  v.panel_side = 1;
  ApplyPanelRules(&v);
  CHECK(!AutomapDrawable(v));
  v = full;
  v.has_clip = false;
  v.panel_side = 2;
  ApplyPanelRules(&v);
  CHECK(v.has_clip);
  CHECK_EQ(v.clip_left, 400);
}

void TestGeometry() {
  Point a;
  Point b;
  // Borda sul da cidade (y = 1112 + 8) com uma sala da Charneca em y = 1120.
  CHECK(SharedEdge(TileRect{100, 1112, 8, 8}, TileRect{96, 1120, 40, 40}, &a, &b));
  CHECK_EQ(a.x, 100);
  CHECK_EQ(b.x, 108);
  CHECK_EQ(a.y, 1120);
  CHECK_EQ(b.y, 1120);
  CHECK(SharedEdge(TileRect{8, 0, 8, 8}, TileRect{0, 4, 8, 8}, &a, &b));  // oeste
  CHECK_EQ(a.x, 8);
  CHECK_EQ(a.y, 4);
  CHECK_EQ(b.y, 8);
  CHECK(!SharedEdge(TileRect{0, 0, 8, 8}, TileRect{8, 8, 8, 8}, &a, &b));   // so o canto
  CHECK(!SharedEdge(TileRect{0, 0, 8, 8}, TileRect{20, 0, 8, 8}, &a, &b));  // separadas

  CanvasMap m = FitFrame(800, 600, 1280, 720, true);
  CHECK(m.scale_x > 1.199 && m.scale_x < 1.201);
  CHECK(m.offset_x > 159.9 && m.offset_x < 160.1);
  CHECK(m.offset_y > -0.1 && m.offset_y < 0.1);
  m = FitFrame(800, 600, 1280, 720, false);
  CHECK(m.scale_x > 1.599 && m.scale_x < 1.601);
  CHECK(m.scale_y > 1.199 && m.scale_y < 1.201);
  m = FitFrame(800, 600, 800, 600, true);
  CHECK(m.scale_x == 1.0 && m.offset_x == 0.0);

  // D2GL: janela pedida 1280x720, area cliente 1280x700 (o caso observado):
  // quadro 960x720 em x = 160, com o topo cortado em 20 px.
  m = FitFrameD2gl(800, 600, 1280, 700, 1280, 720, false, false);
  CHECK(m.scale_x > 1.199 && m.scale_x < 1.201);
  CHECK(m.offset_x > 159.9 && m.offset_x < 160.1);
  CHECK(m.offset_y > -20.1 && m.offset_y < -19.9);
  // Janela do tamanho do quadro: identidade.
  m = FitFrameD2gl(800, 600, 800, 600, 800, 600, false, false);
  CHECK(m.scale_x == 1.0 && m.scale_y == 1.0 && m.offset_x == 0.0 && m.offset_y == 0.0);
  // Canvas reduzido (overlay_max_width) numa janela 3840x2160.
  m = FitFrameD2gl(800, 600, 1920, 1080, 3840, 2160, false, false);
  CHECK(m.scale_x > 1.799 && m.scale_x < 1.801);  // 3,6 x 0,5
  CHECK(m.offset_x > 239.9 && m.offset_x < 240.1);
  // Esticado na horizontal.
  m = FitFrameD2gl(800, 600, 1280, 720, 1280, 720, true, false);
  CHECK(m.scale_x > 1.599 && m.scale_x < 1.601);
  CHECK(m.offset_x > -0.1 && m.offset_x < 0.1);
}

// --- Varredura de um mundo sintetico -----------------------------------------

// Monta, com os offsets das specs, um nivel (id 2) com salas, predefinidas,
// excecoes, LevelDefs e a tabela de unidades.
struct World {
  FakeMemory mem;
  GameAddresses addr;
  uint32_t player = 0;
  uint32_t act = 0;
  uint32_t level = 0;
  uint32_t town = 0;
  uint32_t drlg_act = 0;
  uint32_t monstats = 0;
  uint32_t room1 = 0;
  uint32_t town_room1 = 0;
  uint32_t first_room = 0;
  uint32_t last_room = 0;
  uint32_t level_defs_record = 0;
  uint32_t objects = 0;
  uint32_t player_data = 0;
  uint32_t bucket_used[6] = {};

  uint32_t NewRoom2(uint32_t of_level, int32_t x, int32_t y, int32_t w, int32_t h,
                    uint32_t flags, uint32_t room1_ptr) {
    const uint32_t r = mem.Alloc(0xEC);
    mem.Put32(r + 0x28, flags);
    mem.Put32(r + 0x30, room1_ptr);
    mem.PutI(r + 0x34, x);
    mem.PutI(r + 0x38, y);
    mem.PutI(r + 0x3C, w);
    mem.PutI(r + 0x40, h);
    mem.Put32(r + 0x58, of_level);
    return r;
  }
  void Link(uint32_t room) {  // encadeia no nivel atual (Room2+0x24)
    if (first_room == 0) {
      first_room = room;
      mem.Put32(level + 0x10, room);
    } else {
      mem.Put32(last_room + 0x24, room);
    }
    last_room = room;
  }
  void Neighbors(uint32_t room, const std::vector<uint32_t>& list) {
    const uint32_t array = mem.Alloc(static_cast<uint32_t>(4 * list.size()));
    for (size_t i = 0; i < list.size(); ++i) {
      mem.Put32(array + static_cast<uint32_t>(4 * i), list[i]);
    }
    mem.Put32(room + 0x08, array);
    mem.Put32(room + 0x2C, static_cast<uint32_t>(list.size()));
  }
  uint32_t Preset(uint32_t room, uint32_t type, uint32_t cls, int32_t x, int32_t y) {
    const uint32_t p = mem.Alloc(0x20);
    mem.Put32(p + 0x04, cls);
    mem.PutI(p + 0x08, x);
    mem.Put32(p + 0x14, type);
    mem.PutI(p + 0x18, y);
    uint32_t head = 0;
    mem.Read(room + 0x5C, &head, 4);
    mem.Put32(p + 0x0C, head);
    mem.Put32(room + 0x5C, p);
    return p;
  }
  void InsertUnit(uint32_t type, uint32_t unit) {
    const uint32_t bucket = addr.server_units + type * 0x200 + 4 * (bucket_used[type]++ % 3);
    uint32_t head = 0;
    mem.Read(bucket, &head, 4);
    mem.Put32(unit + 0xE4, head);
    mem.Put32(bucket, unit);
  }
  uint32_t Monster(uint32_t cls, uint32_t mode, uint8_t type_flags, uint32_t in_room1,
                   int32_t cx, int32_t cy, uint32_t id = 0) {
    const uint32_t u = mem.Alloc(0xF4);
    const uint32_t data = mem.Alloc(0x30);
    const uint32_t path = mem.Alloc(0x40);
    mem.Put32(u + 0x00, 1);
    mem.Put32(u + 0x04, cls);
    mem.Put32(u + 0x0C, id);
    mem.Put32(u + 0x10, mode);
    mem.Put32(u + 0x14, data);
    mem.Put32(u + 0x2C, path);
    mem.Put8(data + 0x16, type_flags);
    mem.PutI(path + 0x08, cx);
    mem.PutI(path + 0x0C, cy);
    mem.Put32(path + 0x1C, in_room1);
    InsertUnit(1, u);
    return u;
  }
  uint32_t StaticUnit(uint32_t type, uint32_t cls, uint32_t in_room1, int32_t cx, int32_t cy) {
    const uint32_t u = mem.Alloc(0xF4);
    const uint32_t path = mem.Alloc(0x20);
    mem.Put32(u + 0x00, type);
    mem.Put32(u + 0x04, cls);
    mem.Put32(u + 0x2C, path);
    mem.Put32(path + 0x00, in_room1);
    mem.PutI(path + 0x04, cx);
    mem.PutI(path + 0x08, cy);
    InsertUnit(type, u);
    return u;
  }
  // Objeto com ObjectData (portais: nivel de destino em +0x04).
  uint32_t ObjectUnit(uint32_t cls, uint32_t in_room1, int32_t cx, int32_t cy, uint8_t dest) {
    const uint32_t u = StaticUnit(2, cls, in_room1, cx, cy);
    const uint32_t data = mem.Alloc(0x38);
    mem.Put32(u + 0x14, data);
    mem.Put8(data + 0x04, dest);
    return u;
  }
  void MonStats(uint32_t cls, uint32_t flags) {
    mem.Put32(monstats + cls * 0x1A8 + 0x0C, flags);
  }
  void ObjectRecord(uint32_t cls, uint8_t sub_class) {
    mem.Put8(objects + cls * 0x1C0 + 0x167, sub_class);
  }
  void Ally(uint32_t id, uint32_t owner) {  // no inicio da lista, como um no novo
    const uint32_t node = mem.Alloc(0x34);
    uint32_t head = 0;
    mem.Read(addr.ally_list, &head, 4);
    mem.Put32(node + 0x08, id);
    mem.Put32(node + 0x0C, owner);
    mem.Put32(node + 0x30, head);
    mem.Put32(addr.ally_list, node);
  }
  void LevelRect(uint32_t of_level, int32_t x, int32_t y, int32_t w, int32_t h) {
    mem.PutI(of_level + 0x1C, x);
    mem.PutI(of_level + 0x20, y);
    mem.PutI(of_level + 0x24, w);
    mem.PutI(of_level + 0x28, h);
  }
  // Acrescenta um nivel a corrente de DrlgAct+0x47C (Level+0x1AC).
  void Chain(uint32_t of_level) {
    uint32_t cursor = 0;
    mem.Read(drlg_act + 0x47C, &cursor, 4);
    if (cursor == 0) {
      mem.Put32(drlg_act + 0x47C, of_level);
      return;
    }
    for (;;) {
      uint32_t next = 0;
      mem.Read(cursor + 0x1AC, &next, 4);
      if (next == 0) break;
      cursor = next;
    }
    mem.Put32(cursor + 0x1AC, of_level);
  }

  World() {
    const uint32_t globals = mem.Alloc(0x80);
    addr.player_unit = globals;
    addr.current_act = globals + 4;
    addr.data_tables = globals + 8;
    addr.level_defs = globals + 12;
    addr.automap_visible = globals + 16;
    addr.ally_list = globals + 20;
    addr.objects_base = globals + 24;
    addr.objects_count = globals + 28;
    addr.panel_side = globals + 32;
    addr.frame_width = globals + 36;
    addr.frame_height = globals + 40;
    addr.automap_mode = globals + 44;
    addr.automap_divisor = globals + 48;
    addr.automap_offset_x = globals + 52;
    addr.automap_offset_y = globals + 56;
    addr.ui_state = mem.Alloc(0x26 * 4);
    objects = mem.Alloc(573 * 0x1C0);
    mem.Put32(addr.objects_base, objects);
    mem.Put32(addr.objects_count, 573);
    addr.server_units = mem.Alloc(6 * 0x200);

    // DataTables: monstats (200 registros) e quantidade de LevelDefs.
    const uint32_t tables = mem.Alloc(0xD00);
    monstats = mem.Alloc(200 * 0x1A8);
    mem.Put32(tables + 0xA78, monstats);
    mem.Put32(tables + 0xA80, 200);
    mem.Put32(tables + 0xC5C, 137);
    mem.Put32(addr.data_tables, tables);
    const uint32_t level_defs = mem.Alloc(137 * 0x9C);
    mem.Put32(addr.level_defs, level_defs);
    // Nivel 2 (como a Charneca): Vis[3..6] = 8, Warp[3..6] = 0..3; Vis[0] = 1
    // e Vis[1] = 3 sem warp (passagens abertas).
    level_defs_record = level_defs + 2 * 0x9C;
    const int32_t vis[8] = {1, 3, 0, 8, 8, 8, 8, 0};
    const int32_t warp[8] = {-1, -1, -1, 0, 1, 2, 3, -1};
    for (uint32_t n = 0; n < 8; ++n) {
      mem.PutI(level_defs_record + 0x48 + 4 * n, vis[n]);
      mem.PutI(level_defs_record + 0x68 + 4 * n, warp[n]);
    }

    drlg_act = mem.Alloc(0x480);
    level = mem.Alloc(0x230);
    mem.Put32(level + 0x1B4, drlg_act);
    mem.PutI(level + 0x1D0, 2);
    town = mem.Alloc(0x230);
    mem.Put32(town + 0x1B4, drlg_act);
    mem.PutI(town + 0x1D0, 1);

    room1 = mem.Alloc(0x80);
    town_room1 = mem.Alloc(0x80);
    act = mem.Alloc(0x50);
    mem.Put32(addr.current_act, act);

    // Jogador (tipo 0) parado numa sala do nivel 2.
    player = mem.Alloc(0xF4);
    const uint32_t path = mem.Alloc(0x40);
    mem.Put32(player + 0x0C, 1);  // id do jogador
    mem.Put32(player + 0x18, 1);  // Ato II
    mem.Put32(player + 0x2C, path);
    mem.Put16(path + 0x02, 5653);
    mem.Put16(path + 0x06, 5523);
    mem.PutI(path + 0x08, 2080);
    mem.PutI(path + 0x0C, 89416);
    mem.Put32(path + 0x1C, room1);
    mem.Put32(addr.player_unit, player);
  }
};

const Mark* FindMark(const Scan& scan, MarkKind kind, int32_t unit_class = -1) {
  for (const Mark& m : scan.marks) {
    if (m.kind == kind && (unit_class < 0 || m.unit_class == unit_class)) return &m;
  }
  return nullptr;
}

void TestScanWorld() {
  World w;
  // Sala carregada com a entrada do warp 5 (bit 0x200) e os bits 0x10 e 0x20, de
  // indices sem warp, que so marcam vinculo com nivel aberto.
  const uint32_t ra = w.NewRoom2(w.level, 1120, 1120, 8, 8, 0x230, w.room1);
  w.mem.Put32(w.room1 + 0x10, ra);
  // Sala da cidade ao norte (outro nivel): borda y = 1120, x 1120..1128.
  const uint32_t rt = w.NewRoom2(w.town, 1120, 1112, 8, 8, 0x10, w.town_room1);
  w.mem.Put32(w.town_room1 + 0x10, rt);
  // Sala nao carregada com o warp 6 (bit 0x400): posicao aproximada.
  const uint32_t rb = w.NewRoom2(w.level, 1128, 1120, 8, 8, 0x400, 0);
  // Destino de um warp ja ligado, encostado na sala: nao e borda (Warp[n] != -1).
  const uint32_t cave = w.mem.Alloc(0x230);
  w.mem.PutI(cave + 0x1D0, 8);
  const uint32_t rcave = w.NewRoom2(cave, 1112, 1120, 8, 8, 0, 0);
  w.Neighbors(ra, {ra, rt, rb, rcave});
  w.Neighbors(rt, {ra});
  w.Link(ra);
  w.Link(rb);
  // Uma sala so com o warp 3 (bit 0x80), nao carregada.
  const uint32_t rc = w.NewRoom2(w.level, 1200, 1200, 8, 8, 0x80, 0);
  w.Link(rc);
  // Sala que ja descarregou (warp 4, bit 0x100): a predefinida fica.
  const uint32_t rd = w.NewRoom2(w.level, 1300, 1300, 8, 8, 0x100, 0);
  w.Link(rd);
  const Point unloaded_exit = SubtileToClient(1300 * 5 + 3, 1300 * 5 + 4);
  w.Preset(rd, 5, 1, 3, 4);

  // Colunas de objects: waypoints, portais, bau, e um objeto so da configuracao.
  w.ObjectRecord(119, 0x40);
  w.ObjectRecord(156, 0x40);
  w.ObjectRecord(59, 0x04);
  w.ObjectRecord(60, 0x04);
  w.ObjectRecord(298, 0x10);
  w.ObjectRecord(267, 0x08);

  // Predefinidas da sala carregada: a saida (tipo 5, lvlwarp 2 = Warp[5]), um
  // waypoint (objeto 119), um bau (267, nao marcado), um objeto da configuracao
  // (500), uma classe fora da tabela e um monstro.
  w.Preset(ra, 5, 2, 10, 15);
  w.Preset(ra, 2, 119, 20, 5);
  w.Preset(ra, 2, 267, 30, 5);
  w.Preset(ra, 2, 500, 35, 5);
  w.Preset(ra, 2, 9000, 36, 5);
  w.Preset(ra, 1, 5, 1, 1);

  // Monstats: hostis, NPC e ambiente; o mercenario passa nos bits de hostil.
  w.MonStats(5, 0x02418803);
  w.MonStats(63, 0x12018001);
  w.MonStats(148, 0x03010703);
  w.MonStats(149, 0x02010402);
  w.MonStats(180, 0x02418803);
  w.Monster(5, 1, 0x00, w.room1, 100, 200);     // normal
  w.Monster(63, 2, 0x0C, w.room1, 110, 210);    // campeao (com o bit de unico)
  w.Monster(63, 12, 0x00, w.room1, 120, 220);   // cadaver: some
  w.Monster(148, 1, 0x00, w.room1, 130, 230);   // NPC
  w.Monster(149, 1, 0x00, w.room1, 140, 240);   // ambiente: some
  w.Monster(5, 1, 0x0A, w.town_room1, 150, 250);  // outro nivel: some
  w.Monster(5, 1, 0x08, w.room1, 160, 260);     // unico
  w.Monster(250, 1, 0x00, w.room1, 170, 270);   // classe fora da tabela: some
  w.Monster(5, 1, 0x10, w.room1, 175, 275);     // lacaio
  // Aliados: mercenario e hidra do jogador, invocacao de outro; ids so valem para
  // o tipo 1 (o mercenario tem o id do jogador).
  w.Monster(180, 1, 0x00, w.room1, 180, 280, 1);
  w.Monster(180, 1, 0x00, w.room1, 181, 281, 0x13);
  w.Monster(180, 1, 0x00, w.room1, 182, 282, 0x20);
  w.Monster(180, 12, 0x00, w.room1, 183, 283, 0x21);  // aliado morto: some
  w.Ally(1, 1);
  w.Ally(0x13, 1);
  w.Ally(0x20, 9);
  w.Ally(0x21, 1);
  // A unidade do waypoint coincide com a predefinida (cy 8 maior: centro do
  // subtile); um segundo waypoint so existe como unidade, de outra classe.
  const Point wp = SubtileToClient(1120 * 5 + 20, 1120 * 5 + 5);
  w.StaticUnit(2, 119, w.room1, wp.x, wp.y + 8);
  w.StaticUnit(2, 156, w.room1, 5000, 9000);
  w.StaticUnit(2, 267, w.room1, 5200, 9000);  // bau: nao marca
  // Portais: azul e vermelho com destino; o da classe 298 tem destino 0.
  w.ObjectUnit(59, w.room1, 6000, 9000, 1);
  w.ObjectUnit(60, w.room1, 6200, 9000, 125);
  w.ObjectUnit(298, w.room1, 6400, 9000, 0);
  // Portal de outro nivel: some.
  w.ObjectUnit(59, w.town_room1, 6600, 9000, 3);
  // Cinco tiles de saida na mesma posicao (como o jogo cria).
  const Point exit = SubtileToClient(1120 * 5 + 10, 1120 * 5 + 15);
  for (int i = 0; i < 5; ++i) w.StaticUnit(5, 2, w.room1, exit.x, exit.y + 8);
  // O tile do outro lado do warp: nunca e saida deste nivel.
  w.StaticUnit(5, 7, w.town_room1, 7000, 7000);

  // Retangulos dos niveis: o 3 (aberto, Vis[1]) toca o nivel 2 sem sala ligada;
  // o 1 (aberto, Vis[0]) ja tem sala ligada e nao ganha aproximacao.
  w.LevelRect(w.level, 888, 1064, 96, 56);
  const uint32_t level3 = w.mem.Alloc(0x230);
  w.mem.PutI(level3 + 0x1D0, 3);
  w.LevelRect(level3, 984, 1080, 80, 80);
  w.LevelRect(w.town, 888, 1000, 96, 64);
  w.Chain(w.town);
  w.Chain(w.level);
  w.Chain(level3);

  ScanOptions options;
  options.objects_to_mark.push_back(ObjectLabel{500, "Pedra"});
  options.objects_to_mark.push_back(ObjectLabel{9000, "Fora"});
  Scan scan;
  ScanGame(w.mem, w.addr, options, &scan);
  CHECK(scan.in_game);
  CHECK_EQ(scan.level_id, 2);
  CHECK_EQ(scan.act_index, 1);
  CHECK_EQ(scan.act, w.act);
  CHECK_EQ(scan.player_room, w.room1);
  CHECK(scan.links_ok);
  CHECK_EQ(scan.player_x, 5653);
  CHECK_EQ(scan.player_y, 5523);
  CHECK_EQ(scan.stats.rooms, 4u);
  CHECK_EQ(scan.stats.rooms_loaded, 1u);
  CHECK_EQ(scan.stats.presets, 7u);
  CHECK_EQ(scan.stats.monsters, 4u);
  CHECK_EQ(scan.stats.minions, 1u);
  CHECK_EQ(scan.stats.champions, 1u);
  CHECK_EQ(scan.stats.uniques, 1u);
  CHECK_EQ(scan.stats.superuniques, 0u);
  CHECK_EQ(scan.stats.allies, 3u);
  CHECK_EQ(scan.stats.own_allies, 2u);
  CHECK_EQ(scan.stats.npcs, 1u);
  CHECK_EQ(scan.stats.waypoints, 2u);
  CHECK_EQ(scan.stats.waypoint_rooms, 0u);
  CHECK_EQ(scan.stats.portals_blue, 1u);
  CHECK_EQ(scan.stats.portals_red, 1u);
  CHECK_EQ(scan.stats.portals_other, 1u);
  CHECK_EQ(scan.stats.objects, 2u);
  CHECK_EQ(scan.stats.exits, 2u);
  CHECK_EQ(scan.stats.exit_rooms, 2u);
  CHECK_EQ(scan.stats.passages, 1u);
  CHECK_EQ(scan.stats.passages_approx, 1u);
  CHECK_EQ(scan.stats.passage_levels, 2u);
  CHECK(FindMark(scan, MarkKind::kWaypoint, 119) != nullptr);
  CHECK(FindMark(scan, MarkKind::kWaypoint, 156) != nullptr);
  CHECK(FindMark(scan, MarkKind::kObject, 267) == nullptr);
  const Mark* blue = FindMark(scan, MarkKind::kPortalBlue);
  CHECK(blue != nullptr && blue->dest_level == 1 && blue->x[0] == 6000);
  const Mark* red = FindMark(scan, MarkKind::kPortalRed);
  CHECK(red != nullptr && red->dest_level == 125);
  const Mark* level_portal = FindMark(scan, MarkKind::kPortalOther);
  CHECK(level_portal != nullptr && level_portal->dest_level == -1);
  const Mark* manual = FindMark(scan, MarkKind::kObject, 500);
  CHECK(manual != nullptr && manual->label == 0);
  const Mark* outside = FindMark(scan, MarkKind::kObject, 9000);
  CHECK(outside != nullptr && outside->label == 1);
  int own = 0;
  for (const Mark& m : scan.marks) {
    if (m.kind == MarkKind::kAlly && m.own) ++own;
    if (m.kind == MarkKind::kMonster || m.kind == MarkKind::kMinion) CHECK(m.unit_class == 5);
    if (m.kind == MarkKind::kExit && m.warp_class == 2) {
      CHECK_EQ(m.dest_level, 8);
      CHECK_EQ(m.x[0], exit.x);
      CHECK_EQ(m.y[0], exit.y);
    }
    if (m.kind == MarkKind::kExit && m.warp_class == 1) {
      CHECK_EQ(m.dest_level, 8);
      CHECK_EQ(m.x[0], unloaded_exit.x);
      CHECK_EQ(m.y[0], unloaded_exit.y);
    }
    if (m.kind == MarkKind::kExitRoom && m.warp_class == 3) {
      CHECK_EQ(m.dest_level, 8);
      CHECK_EQ(m.x[0], TileToClient(1128, 1120).x);
      CHECK_EQ(m.y[2], TileToClient(1136, 1128).y);
    }
    if (m.kind == MarkKind::kPassage) {
      CHECK_EQ(m.dest_level, 1);
      CHECK(m.primary);
      CHECK_EQ(m.x[0], TileToClient(1120, 1120).x);
      CHECK_EQ(m.x[1], TileToClient(1128, 1120).x);
    }
    if (m.kind == MarkKind::kPassageApprox) {  // spec L sec. 4.1: x = 984, y 1080..1120
      CHECK_EQ(m.dest_level, 3);
      CHECK(m.primary);
      CHECK_EQ(m.x[0], TileToClient(984, 1080).x);
      CHECK_EQ(m.y[0], TileToClient(984, 1080).y);
      CHECK_EQ(m.x[1], TileToClient(984, 1120).x);
      CHECK_EQ(m.y[1], TileToClient(984, 1120).y);
    }
  }
  CHECK_EQ(own, 2);

  // Salas para revelar: so a dos warps 6 e 3 (sem posicao exata e nao carregadas).
  std::vector<uint32_t> done;
  const WarpRoom* next = NextRoomToReveal(scan, done);
  CHECK(next != nullptr && next->room == rb && next->x == 1128 && next->y == 1120);
  CHECK_EQ(next != nullptr ? next->n : -1, 6);
  done.push_back(rb);
  next = NextRoomToReveal(scan, done);
  CHECK(next != nullptr && next->room == rc);
  done.push_back(rc);
  CHECK(NextRoomToReveal(scan, done) == nullptr);
  // Sala ja pedida pelo servidor (Room2+0x0E) fica com o servidor.
  w.mem.Put16(rb + 0x0E, 1);
  ScanGame(w.mem, w.addr, options, &scan);
  done.clear();
  next = NextRoomToReveal(scan, done);
  CHECK(next != nullptr && next->room == rc);
  w.mem.Put16(rb + 0x0E, 0);
  // A predefinida tipo 5 que a ativacao cria encerra a aproximacao daquela sala.
  w.Preset(rb, 5, 3, 1, 1);
  ScanGame(w.mem, w.addr, options, &scan);
  CHECK_EQ(scan.stats.exits, 3u);
  CHECK_EQ(scan.stats.exit_rooms, 1u);
  next = NextRoomToReveal(scan, done);
  CHECK(next != nullptr && next->room == rc);
  w.mem.Put32(rb + 0x5C, 0);

  // Sem as colunas de objects so a configuracao marca.
  GameAddresses no_objects = w.addr;
  no_objects.objects_base = 0;
  no_objects.objects_count = 0;
  ScanOptions with_119 = options;
  with_119.objects_to_mark.push_back(ObjectLabel{119, "WP"});
  ScanGame(w.mem, no_objects, with_119, &scan);
  CHECK_EQ(scan.stats.waypoints, 0u);
  CHECK_EQ(scan.stats.portals_blue, 0u);
  CHECK_EQ(scan.stats.objects, 3u);  // 500 e 9000 + a 119 da configuracao

  // A coluna tem precedencia: [objects] nao transforma um waypoint em objeto.
  ScanGame(w.mem, w.addr, with_119, &scan);
  CHECK_EQ(scan.stats.waypoints, 2u);
  CHECK_EQ(scan.stats.objects, 2u);

  // Sem a lista de aliados eles aparecem como hostis (o mod avisa no log).
  GameAddresses no_allies = w.addr;
  no_allies.ally_list = 0;
  ScanGame(w.mem, no_allies, options, &scan);
  CHECK_EQ(scan.stats.allies, 0u);
  CHECK_EQ(scan.stats.monsters, 7u);

  // Aliados desligados na configuracao: nao marcam nem viram hostis.
  ScanOptions no_ally_marks = options;
  no_ally_marks.allies = false;
  ScanGame(w.mem, w.addr, no_ally_marks, &scan);
  CHECK_EQ(scan.stats.allies, 0u);
  CHECK_EQ(scan.stats.monsters, 4u);

  // Com only_current_level = false o monstro da cidade tambem aparece (era
  // superunico), mas o tile do outro nivel continua fora.
  options.only_current_level = false;
  ScanGame(w.mem, w.addr, options, &scan);
  CHECK_EQ(scan.stats.superuniques, 1u);
  CHECK_EQ(scan.stats.monsters, 5u);
  CHECK_EQ(scan.stats.portals_blue, 2u);
  CHECK_EQ(scan.stats.exits, 2u);

  // Com a saida exata ausente, o warp 5 vira aproximacao da sala ra.
  options.only_current_level = true;
  options.exits = true;
  ScanOptions no_units = options;
  GameAddresses no_table = w.addr;
  no_table.server_units = 0;
  w.mem.Put32(ra + 0x5C, 0);  // sem predefinidas
  w.mem.Put32(rd + 0x5C, 0);
  ScanGame(w.mem, no_table, no_units, &scan);
  CHECK_EQ(scan.stats.exits, 0u);
  CHECK_EQ(scan.stats.exit_rooms, 4u);
  CHECK_EQ(scan.stats.monsters, 0u);

  // Sala com os bits de waypoint sem nenhum waypoint exato: marca a sala.
  CHECK_EQ(scan.stats.waypoint_rooms, 0u);
  w.mem.Put32(rc + 0x28, 0x80 | 0x10000);
  ScanGame(w.mem, no_table, no_units, &scan);
  CHECK_EQ(scan.stats.waypoints, 0u);
  CHECK_EQ(scan.stats.waypoint_rooms, 1u);
  const Mark* wp_room = FindMark(scan, MarkKind::kWaypointRoom);
  CHECK(wp_room != nullptr && wp_room->points == 4 &&
        wp_room->x[0] == TileToClient(1200, 1200).x);
  // Com o waypoint exato conhecido, a sala nao e marcada.
  ScanGame(w.mem, w.addr, no_units, &scan);
  CHECK_EQ(scan.stats.waypoints, 2u);
  CHECK_EQ(scan.stats.waypoint_rooms, 0u);

  // Estado do automap lido da memoria: paineis e tamanho do quadro.
  w.mem.Put32(w.addr.automap_visible, 1);
  w.mem.PutI(w.addr.automap_divisor, 10);
  w.mem.PutI(w.addr.frame_width, 640);
  w.mem.PutI(w.addr.frame_height, 480);
  w.mem.PutI(w.addr.panel_side, 1);
  AutomapView view;
  CHECK(ReadAutomapView(w.mem, w.addr, &view));
  CHECK(AutomapDrawable(view));
  CHECK_EQ(view.frame_width, 640);
  CHECK_EQ(view.frame_height, 480);
  CHECK_EQ(view.panel_side, 1);
  CHECK_EQ(view.clip_right, 319);
  w.mem.Put32(w.addr.ui_state + 8 * 4, 1);  // dialogo de NPC
  CHECK(ReadAutomapView(w.mem, w.addr, &view));
  CHECK(view.npc_dialog);
  CHECK(!AutomapDrawable(view));
  w.mem.Put32(w.addr.ui_state + 8 * 4, 0);
  w.mem.PutI(w.addr.panel_side, 3);
  CHECK(ReadAutomapView(w.mem, w.addr, &view));
  CHECK(!AutomapDrawable(view));
  w.mem.PutI(w.addr.panel_side, 7);  // fora da faixa: ignorado
  w.mem.PutI(w.addr.frame_width, 0);
  CHECK(ReadAutomapView(w.mem, w.addr, &view));
  CHECK_EQ(view.panel_side, 0);
  CHECK_EQ(view.frame_width, 0);
  CHECK(AutomapDrawable(view));

  // Excecoes do ato tem prioridade sobre LevelDefs (L sec. 2.2/2.7).
  const uint32_t node2 = w.mem.Alloc(0x48);
  const uint32_t node1 = w.mem.Alloc(0x48);
  w.mem.PutI(node1 + 0x00, 40);
  w.mem.Put32(node1 + 0x44, node2);
  w.mem.PutI(node2 + 0x00, 2);
  w.mem.PutI(node2 + 0x04, 9);
  w.mem.PutI(node2 + 0x24, 7);
  w.mem.Put32(w.drlg_act + 0x90, node1);
  int32_t vis[8];
  int32_t warp[8];
  CHECK(ReadLevelLinks(w.mem, w.addr, w.level, 2, vis, warp));
  CHECK_EQ(vis[0], 9);
  CHECK_EQ(warp[0], 7);
  CHECK_EQ(vis[1], 0);
  w.mem.Put32(w.drlg_act + 0x90, 0);
  CHECK(ReadLevelLinks(w.mem, w.addr, w.level, 2, vis, warp));
  CHECK_EQ(vis[3], 8);
  CHECK_EQ(warp[5], 2);
  CHECK(!ReadLevelLinks(w.mem, w.addr, w.level, 500, vis, warp));  // fora da faixa
  CHECK_EQ(warp[0], -1);

  // Fora da partida: ato nulo ou jogador nulo.
  w.mem.Put32(w.addr.current_act, 0);
  ScanGame(w.mem, w.addr, options, &scan);
  CHECK(!scan.in_game);
  CHECK(scan.marks.empty());
  w.mem.Put32(w.addr.current_act, w.act);
  w.mem.Put32(w.addr.player_unit, 0);
  ScanGame(w.mem, w.addr, options, &scan);
  CHECK(!scan.in_game);
}

void TestCorruptedMemory() {
  World w;
  const uint32_t ra = w.NewRoom2(w.level, 0, 0, 8, 8, 0, w.room1);
  w.mem.Put32(w.room1 + 0x10, ra);
  w.Link(ra);
  w.mem.Put32(ra + 0x24, ra);  // ciclo na lista de salas
  w.MonStats(5, 0x02418803);
  const uint32_t m = w.Monster(5, 1, 0, w.room1, 0, 0);
  w.mem.Put32(m + 0xE4, m);  // ciclo no balde
  w.mem.Put32(ra + 0x08, 0xDEAD0000);  // vizinhas em endereco invalido
  w.mem.Put32(ra + 0x2C, 4);
  // Ciclo na lista de aliados e no dela mesma.
  const uint32_t node = w.mem.Alloc(0x34);
  w.mem.Put32(node + 0x08, 999);
  w.mem.Put32(node + 0x30, node);
  w.mem.Put32(w.addr.ally_list, node);
  // Tabela de objects em endereco invalido.
  w.mem.Put32(w.addr.objects_base, 0xDEAD0000);
  w.Preset(ra, 2, 119, 1, 1);
  w.StaticUnit(2, 119, w.room1, 0, 0);
  ScanOptions options;
  Scan scan;
  ScanGame(w.mem, w.addr, options, &scan);  // termina, sem ler fora
  CHECK(scan.in_game);
  CHECK_EQ(scan.stats.rooms, 4096u);
  CHECK_EQ(scan.stats.passages, 0u);
  CHECK_EQ(scan.stats.monsters, 4096u);
  CHECK_EQ(scan.stats.waypoints, 0u);
}

}  // namespace

int main() {
  TestAnchorsAgainstRealIni();
  TestFormula();
  TestClassify();
  TestPanels();
  TestGeometry();
  TestScanWorld();
  TestCorruptedMemory();
  if (g_failures == 0) {
    std::printf("map_logic_test: tudo passou\n");
    return 0;
  }
  std::printf("map_logic_test: %d falha(s)\n", g_failures);
  return 1;
}
