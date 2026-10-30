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

// Logica portavel do mod map-overlay (itens C1/C2 do docs/ROADMAP.md).
//
// Le o estado do cliente do Game.exe 1.14d por uma MemoryView e produz marcas
// em coordenadas de cliente (pixels isometricos do mundo); projeta-as no
// automap pela formula da spec. So LE memoria: nao chama nenhuma funcao do
// jogo. Nao depende de Win32, entao roda (e e testada) no host.
//
// Fontes de cada fato (offsets, flags, formula):
//   docs/specs/game-exe-1.14d-units.md            (unidades, monstros)
//   docs/specs/game-exe-1.14d-levels-automap.md   (niveis, salas, automap)
//   aeonian/addresses/1.14d.ini                   (ancoras verificadas)

#ifndef AEONIAN_EXAMPLES_MAP_OVERLAY_MAP_LOGIC_H_
#define AEONIAN_EXAMPLES_MAP_OVERLAY_MAP_LOGIC_H_

#include <cstdint>
#include <string>
#include <vector>

namespace map_overlay {

// Memoria do processo do jogo (enderecos de 32 bits). Uma leitura invalida
// devolve false em vez de derrubar o jogo.
class MemoryView {
 public:
  virtual ~MemoryView() = default;
  virtual bool Read(uint32_t address, void* out, uint32_t size) const = 0;
};

bool ReadU32(const MemoryView& mem, uint32_t address, uint32_t* out);
bool ReadI32(const MemoryView& mem, uint32_t address, int32_t* out);

// --- Enderecos dos globais -------------------------------------------------

// Globais de dados do cliente. Nenhum endereco e escrito no codigo: cada um e
// lido de dentro dos bytes de um simbolo do banco de enderecos (a "ancora"),
// que o motor so entrega depois de conferir os bytes.
struct GameAddresses {
  uint32_t player_unit = 0;      // Unit* do jogador local
  uint32_t current_act = 0;      // Act* do cliente
  uint32_t server_units = 0;     // tabela de unidades do servidor (6 x 128)
  uint32_t data_tables = 0;      // ponteiro para os DataTables
  uint32_t level_defs = 0;       // ponteiro para os registros de LevelDefs
  uint32_t automap_visible = 0;  // automap visivel (!= 0)
  uint32_t automap_mode = 0;     // 0 tela cheia, 1 minimapa
  uint32_t automap_divisor = 0;  // 10 ou 20
  uint32_t automap_offset_x = 0;
  uint32_t automap_offset_y = 0;
  uint32_t automap_clip = 0;     // RECT: esquerda, topo, direita, base
};

// Um global dentro dos bytes de uma ancora: o DWORD little-endian em `offset`
// vale `field + delta` (delta != 0 so nas ancoras que conferem um campo
// vizinho, como o topo e a direita do RECT de recorte).
struct AnchorSpec {
  const char* symbol;
  uint32_t offset;
  uint32_t GameAddresses::*field;
  uint32_t delta;
};

// Todas as ancoras que o mod usa (e a lista de simbolos que ele resolve).
const std::vector<AnchorSpec>& Anchors();

// Simbolos que o mod resolve, sem repeticao, na ordem de Anchors().
std::vector<std::string> RequiredSymbols();

using ResolveFn = uint32_t (*)(void* context, const char* symbol);

struct ResolveReport {
  uint32_t total = 0;
  uint32_t resolved = 0;
  std::vector<std::string> missing;  // simbolos que vieram 0
  std::vector<std::string> notes;    // inconsistencias detectadas
};

// Resolve os simbolos por nome (resolve devolve 0 se o simbolo nao existe ou
// se os bytes nao conferem) e le os enderecos dos globais das ancoras. Um
// simbolo ausente deixa os campos dele em 0. `anchors` deve dar os bytes
// ORIGINAIS do jogo (OriginalMemory, em game_memory.h): uma funcao desviada por
// outro mod tem o jmp dele sobre os bytes verificados.
void ResolveAddresses(const MemoryView& anchors, ResolveFn resolve, void* context,
                      GameAddresses* out, ResolveReport* report);

// Recursos do mod e os globais de que cada um depende.
enum Feature : uint32_t {
  kFeatureAutomap = 1u << 0,   // base: jogador + estado e formula do automap
  kFeatureMonsters = 1u << 1,  // tabela de unidades + monstats
  kFeatureObjectUnits = 1u << 2,  // objetos/saidas tambem pela tabela de unidades
  kFeatureLevelLinks = 1u << 3,   // Vis/Warp de LevelDefs (destino das saidas)
  kFeatureClip = 1u << 4,         // recorte do automap
};
uint32_t AvailableFeatures(const GameAddresses& addr);

// --- Automap ---------------------------------------------------------------

struct Point {
  int32_t x = 0;
  int32_t y = 0;
};

struct AutomapView {
  bool visible = false;
  int32_t mode = 0;     // 0 tela cheia, 1 minimapa
  int32_t divisor = 0;  // 10 (modo 0) ou 20 (modo 1)
  int32_t offset_x = 0;
  int32_t offset_y = 0;
  bool has_clip = false;
  int32_t clip_left = 0;
  int32_t clip_top = 0;
  int32_t clip_right = 0;
  int32_t clip_bottom = 0;
};

// Le o estado do automap. false se faltar algum global obrigatorio.
bool ReadAutomapView(const MemoryView& mem, const GameAddresses& addr,
                     AutomapView* out);

// Subtiles -> coordenada de cliente: cx = (x - y)*16, cy = (x + y)*8.
Point SubtileToClient(int32_t x, int32_t y);
inline Point TileToClient(int32_t x, int32_t y) {
  return SubtileToClient(x * 5, y * 5);  // 1 tile = 5 subtiles
}

// Coordenada de cliente -> pixel do quadro do jogo no automap (spec sec. 5.2):
// ax = cx/div - offX + 8, ay = cy/div - offY - 8 (divisao inteira com sinal).
// No modo 1 soma o mesmo ajuste que o jogo aplica a sua cruz (x - 1, y + 5).
Point ClientToAutomap(int32_t cx, int32_t cy, const AutomapView& view);

bool InsideClip(const Point& p, const AutomapView& view);

// --- Marcas ----------------------------------------------------------------

enum class MarkKind : uint8_t {
  kMonster,      // monstro hostil normal
  kChampion,
  kUnique,
  kSuperUnique,
  kNpc,          // NPC (monstats bit 0x100)
  kObject,       // objeto marcado como objetivo (classe na configuracao)
  kExit,         // saida por warp com posicao exata (tile/predefinida tipo 5)
  kExitRoom,     // sala que contem um warp ainda nao carregado (aproximado)
  kPassage,      // borda aberta com uma sala de outro nivel
};

struct Mark {
  MarkKind kind = MarkKind::kMonster;
  uint8_t points = 1;     // 1 ponto, 2 (segmento) ou 4 (contorno da sala)
  bool primary = false;   // kPassage: o segmento que leva o rotulo do destino
  int32_t x[4] = {};      // coordenadas de cliente
  int32_t y[4] = {};
  int32_t dest_level = -1;  // saidas: id do nivel de destino (-1 = ?)
  int32_t warp_class = -1;  // saidas por warp: id de lvlwarp
  int32_t label = -1;       // kObject: indice em ScanOptions::objects
};

struct ObjectLabel {
  uint32_t object_class = 0;  // linha de objects.txt
  std::string label;          // UTF-8
};

struct ScanOptions {
  bool monsters = true;
  bool npcs = true;
  bool objects = true;
  bool exits = true;
  bool only_current_level = true;
  // Salas candidatas a conter o mesmo warp acima disto: o sinal nao e
  // confiavel e as aproximacoes daquele warp sao descartadas.
  uint32_t max_exit_rooms = 3;
  std::vector<ObjectLabel> objects_to_mark;
};

struct ScanStats {
  uint32_t rooms = 0;
  uint32_t rooms_loaded = 0;
  uint32_t presets = 0;
  uint32_t units = 0;
  uint32_t monsters = 0;
  uint32_t champions = 0;
  uint32_t uniques = 0;
  uint32_t superuniques = 0;
  uint32_t npcs = 0;
  uint32_t objects = 0;
  uint32_t exits = 0;
  uint32_t exit_rooms = 0;
  uint32_t passages = 0;
  uint32_t passage_levels = 0;
};

struct Scan {
  bool in_game = false;
  int32_t level_id = 0;  // 0 = desconhecido
  uint32_t level = 0;    // Level*
  bool links_ok = false;
  int32_t vis[8] = {};
  int32_t warp[8] = {};
  int32_t player_x = 0;  // subtiles
  int32_t player_y = 0;
  std::vector<Mark> marks;
  ScanStats stats;
};

// Percorre o estado do cliente e monta as marcas do nivel atual. Devolve
// in_game em out (false fora da partida). So le memoria.
void ScanGame(const MemoryView& mem, const GameAddresses& addr,
              const ScanOptions& options, Scan* out);

// Vis[8]/Warp[8] do nivel: primeiro a lista de excecoes de DrlgAct+0x90,
// depois LevelDefs (spec sec. 2.7).
bool ReadLevelLinks(const MemoryView& mem, const GameAddresses& addr,
                    uint32_t level, int32_t level_id, int32_t vis[8],
                    int32_t warp[8]);

// --- Classificacao ---------------------------------------------------------

enum class MonsterClass : uint8_t {
  kHidden,  // morto, ambiente da cidade etc.
  kNormal,
  kChampion,
  kUnique,
  kSuperUnique,
  kNpc,
};

// mode: Unit+0x10; monstats_flags: registro de monstats +0x0C; type_flags:
// MonsterData+0x16.
MonsterClass ClassifyMonster(uint32_t mode, uint32_t monstats_flags,
                             uint8_t type_flags);

// --- Geometria -------------------------------------------------------------

struct TileRect {
  int32_t x = 0;
  int32_t y = 0;
  int32_t w = 0;
  int32_t h = 0;
};

// Lado comum (com comprimento > 0) de dois retangulos encostados, em tiles.
bool SharedEdge(const TileRect& a, const TileRect& b, Point* from, Point* to);

// Marca projetada em pixels do quadro do jogo.
struct ScreenMark {
  MarkKind kind = MarkKind::kMonster;
  uint8_t points = 1;
  bool primary = false;
  int32_t x[4] = {};
  int32_t y[4] = {};
  int32_t dest_level = -1;
  int32_t label = -1;
};

// Projeta as marcas no quadro. Marcas de um ponto fora do recorte sao
// descartadas, como o jogo faz com as suas.
void ProjectMarks(const std::vector<Mark>& marks, const AutomapView& view,
                  std::vector<ScreenMark>* out);

// Quadro do jogo (frame_w x frame_h) -> canvas do on_present. Com
// keep_aspect o quadro e centralizado com barras; senao e esticado.
struct CanvasMap {
  double scale_x = 1.0;
  double scale_y = 1.0;
  double offset_x = 0.0;
  double offset_y = 0.0;
};
CanvasMap FitFrame(int32_t frame_w, int32_t frame_h, int32_t canvas_w,
                   int32_t canvas_h, bool keep_aspect);

// Modelo do D2GL (observado com o 1.3.3 no Wine, nao documentado na spec):
// o quadro e encaixado no tamanho de janela do d2gl.ini (window_w x
// window_h; esticado no eixo com stretch_*) e ancorado no canto INFERIOR
// esquerdo da area cliente (origem do OpenGL), entao uma area cliente mais
// baixa que a janela pedida corta o topo. O canvas pode ser menor que a area
// cliente (overlay_max_width): a largura da janela da a proporcao.
CanvasMap FitFrameD2gl(int32_t frame_w, int32_t frame_h, int32_t canvas_w, int32_t canvas_h,
                       int32_t window_w, int32_t window_h, bool stretch_h, bool stretch_v);

}  // namespace map_overlay

#endif  // AEONIAN_EXAMPLES_MAP_OVERLAY_MAP_LOGIC_H_
