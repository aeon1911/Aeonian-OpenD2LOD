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
// jogo, nem as que descreve (alinhamento, por exemplo, e lido pelos stats). Nao
// depende de Win32, entao roda (e e testada) no host.
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
  uint32_t panel_side = 0;       // lado dos paineis abertos (0..3)
  uint32_t ui_state = 0;         // array de estado de UI (0x26 entradas)
  uint32_t frame_width = 0;      // largura do quadro do jogo
  uint32_t frame_height = 0;     // altura do quadro do jogo
  uint32_t ally_list = 0;        // cabeca da lista de aliados
  uint32_t objects_base = 0;     // ponteiro para os registros de objects
  uint32_t objects_count = 0;    // quantidade de registros de objects
  // Entrada de D2Common.GetUnitAlignment. O mod nao a chama: os bytes conferidos
  // atestam o layout dos stats que ele le (U sec. 7.2).
  uint32_t unit_alignment = 0;
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

// Simbolos que o mod resolve, sem repeticao: os de Anchors() e os demais.
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
  kFeatureAllies = 1u << 5,       // lista de aliados (mercenario, invocacoes)
  kFeatureObjectTable = 1u << 6,  // colunas de objects (waypoints, portais)
  kFeaturePanels = 1u << 7,       // paineis abertos
  kFeatureFrameSize = 1u << 8,    // tamanho do quadro do jogo
  kFeatureAlignment = 1u << 9,    // alinhamento (filtro de hostis)
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
  // Recorte do jogo, ja cortado pelo painel aberto (ver ApplyPanelRules).
  bool has_clip = false;
  int32_t clip_left = 0;
  int32_t clip_top = 0;
  int32_t clip_right = 0;
  int32_t clip_bottom = 0;
  int32_t frame_width = 0;   // 0 = desconhecido
  int32_t frame_height = 0;
  int32_t panel_side = 0;    // 0 nenhum, 1 direita, 2 esquerda, 3 os dois lados
  bool npc_dialog = false;   // UI 8: nao muda o lado
  bool esc_menu = false;     // UI 9
  bool help = false;         // UI 33
  bool blocked = false;      // nada pode ser desenhado (ver ApplyPanelRules)
};

// Le o estado do automap e aplica os paineis. false se faltar algum global
// obrigatorio.
bool ReadAutomapView(const MemoryView& mem, const GameAddresses& addr,
                     AutomapView* out);

// Regras de painel (L sec. 5.4), a partir dos campos ja lidos: lado 3, dialogo
// de NPC, menu e ajuda bloqueiam o desenho; em tela cheia, um painel lateral
// corta o recorte na metade do quadro (o recorte do jogo nao o exclui). No
// minimapa o proprio recorte do jogo ja acompanha o painel.
void ApplyPanelRules(AutomapView* view);

// O jogo desenha o automap e nada o esconde por inteiro.
inline bool AutomapDrawable(const AutomapView& view) {
  return view.visible && !view.blocked;
}

// Subtiles -> coordenada de cliente: cx = (x - y)*16, cy = (x + y)*8.
Point SubtileToClient(int32_t x, int32_t y);
// O inverso, para pontos que vieram de SubtileToClient (logs e testes).
Point ClientToSubtile(int32_t cx, int32_t cy);
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
  kMinion,       // lacaio de um chefe
  kChampion,
  kUnique,
  kSuperUnique,
  kAlly,         // mercenario ou invocacao (lista de aliados)
  kFriendly,     // nao hostil, fora da lista de aliados (classes 359 e 522)
  kNpc,          // NPC (monstats bit 0x100)
  kObject,       // objeto da configuracao ([objects])
  kQuest,        // objetivo de quest (OperateFn no conjunto da spec)
  kWaypoint,     // waypoint (SubClass 0x40)
  kWaypointRoom, // sala com waypoint ainda nao carregada (aproximado)
  kPortalBlue,   // portal da cidade (classe 59)
  kPortalRed,    // portal vermelho (classe 60)
  kPortalOther,  // demais portais por objeto (SubClass 0x10)
  kExit,         // saida por warp com posicao exata (tile/predefinida tipo 5)
  kExitRoom,     // sala que contem um warp ainda nao carregado (aproximado)
  kPassage,      // passagem exata entre niveis abertos (ver PassageSource)
  kPassageApprox,  // trecho comum dos retangulos de dois niveis (aproximado)
};

// De onde veio uma marca kPassage (L sec. 4.1).
enum class PassageSource : uint8_t {
  kNone,
  kCollision,  // celulas livres dos dois lados, com as salas carregadas
  kAct1Rule,   // variante da sala, sem ativar nada (so nas bordas medidas do Ato I)
};

struct Mark {
  MarkKind kind = MarkKind::kMonster;
  uint8_t points = 1;     // 1 ponto, 2 (segmento) ou 4 (contorno da sala)
  bool primary = false;   // kPassage: o segmento que leva o rotulo do destino
  bool own = false;       // kAlly: o dono e o jogador local
  bool used = false;      // kQuest: a unidade esta em modo != 0 (U sec. 5.4)
  PassageSource source = PassageSource::kNone;  // kPassage
  int32_t x[4] = {};      // coordenadas de cliente
  int32_t y[4] = {};
  int32_t dest_level = -1;  // saidas: id do nivel de destino (-1 = ?)
  int32_t warp_class = -1;  // saidas por warp: id de lvlwarp
  int32_t label = -1;       // kObject, kQuest: indice em ScanOptions::objects_to_mark
  int32_t unit_class = -1;  // monstros e objetos: linha de monstats/objects
  int32_t operate_fn = -1;  // kQuest: coluna OperateFn do registro
  int32_t ally_type = -1;   // kAlly: tipo na lista de aliados (U sec. 7.1)
};

struct ObjectLabel {
  uint32_t object_class = 0;  // linha de objects.txt
  std::string label;          // UTF-8; kHideObjectLabel esconde a classe
};

// Rotulo de [objects] que tira a classe dos objetivos e da configuracao.
inline constexpr const char* kHideObjectLabel = "-";

// O que continua valendo depois que as salas descarregam: as passagens exatas ja
// calculadas pela colisao e os objetivos vistos em uso. Vale para um ato de um mapa;
// ScanGame a zera quando eles mudam.
struct ScanMemo {
  struct Passage {
    int32_t level = 0;
    int32_t dest = 0;
    int32_t width = 0;  // subtiles livres somados
    std::vector<Mark> marks;
  };
  struct UsedQuest {
    int32_t level = 0;
    int32_t object_class = 0;
  };
  uint32_t act = 0;
  uint32_t seed = 0;
  std::vector<Passage> passages;
  std::vector<UsedQuest> used_quests;
};

struct ScanOptions {
  bool monsters = true;
  bool allies = true;
  bool npcs = true;     // NPCs e os amigos fora da lista de aliados
  bool objects = true;  // waypoints, portais e os da configuracao
  bool quests = true;   // objetivos de quest
  bool exits = true;
  bool only_current_level = true;
  // false so para conferir a regra do Ato I contra a colisao (condutor de teste).
  bool passage_collision = true;
  std::vector<ObjectLabel> objects_to_mark;
  ScanMemo* memo = nullptr;  // opcional; so a thread principal usa
};

struct ScanStats {
  uint32_t rooms = 0;
  uint32_t rooms_loaded = 0;
  uint32_t presets = 0;
  uint32_t units = 0;
  uint32_t monsters = 0;  // hostis, incluindo lacaios e chefes
  uint32_t minions = 0;
  uint32_t champions = 0;
  uint32_t uniques = 0;
  uint32_t superuniques = 0;
  uint32_t allies = 0;
  uint32_t own_allies = 0;
  uint32_t friendly = 0;
  uint32_t alignment_unknown = 0;  // monstros cujos stats nao puderam ser lidos
  uint32_t npcs = 0;
  uint32_t objects = 0;  // os da configuracao
  uint32_t quests = 0;
  uint32_t quests_used = 0;
  uint32_t waypoints = 0;
  uint32_t waypoint_rooms = 0;
  uint32_t portals_blue = 0;
  uint32_t portals_red = 0;
  uint32_t portals_other = 0;
  uint32_t exits = 0;
  uint32_t exit_rooms = 0;
  uint32_t passages = 0;       // trechos exatos, das duas fontes
  uint32_t passages_rule = 0;  // deles, os da regra do Ato I
  uint32_t passages_approx = 0;
  uint32_t passage_levels = 0;
};

struct TileRect {
  int32_t x = 0;
  int32_t y = 0;
  int32_t w = 0;
  int32_t h = 0;
};

// Sala do nivel atual com o bit de um warp real (L sec. 4.2).
struct WarpRoom {
  uint32_t room = 0;  // Room2*
  int32_t x = 0;      // tiles
  int32_t y = 0;
  int32_t w = 0;
  int32_t h = 0;
  int n = 0;
  bool loaded = false;      // Room2+0x30 != 0
  uint16_t load_count = 0;  // Room2+0x0E
};

struct Scan {
  bool in_game = false;
  int32_t level_id = 0;  // 0 = desconhecido
  uint32_t level = 0;    // Level*
  TileRect level_rect;
  uint32_t act = 0;      // Act* do cliente
  int32_t act_index = -1;  // 0 = Ato I
  uint32_t player_room = 0;  // Room1* do jogador
  bool links_ok = false;
  int32_t vis[8] = {};
  int32_t warp[8] = {};
  int32_t player_x = 0;  // subtiles
  int32_t player_y = 0;
  std::vector<Mark> marks;
  std::vector<WarpRoom> warp_rooms;
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

// Sala de warp que pode ser ativada para revelar a posicao exata da saida
// (L sec. 3.1): sala fora de `done`, nao carregada, sem carga do servidor, cujo
// warp ainda nao tem posicao exata e leva a um nivel do mesmo ato (a ligacao do
// warp gera o destino, e pedir nivel de outro ato fecha o jogo). nullptr se nao
// houver.
const WarpRoom* NextRoomToReveal(const Scan& scan, const std::vector<uint32_t>& done);

// Por que nao chamar AddRoomData agora (L sec. 3.1), ou nullptr se pode. Confere
// que o ato do cliente e o do jogador, que o nivel atual esta na corrente desse
// ato e que ele tem salas ativas, para o nivel nao perder as Room2.
const char* RevealBlockReason(const MemoryView& mem, const Scan& scan);

// Ato (0 = Ato I) de um nivel, pelas cidades (L sec. 2.8); -1 fora da faixa.
int32_t ActOfLevel(int32_t level_id);

// Nome interno do objeto, da tabela do jogo em memoria (U sec. 5.1), so para o log.
bool ReadObjectName(const MemoryView& mem, const GameAddresses& addr, uint32_t object_class,
                    char* out, uint32_t out_size);

// --- Classificacao ---------------------------------------------------------

enum class MonsterClass : uint8_t {
  kHidden,  // morto, ambiente da cidade etc.
  kNormal,
  kMinion,
  kChampion,
  kUnique,
  kSuperUnique,
  kAlly,
  kFriendly,
  kNpc,
};

// Resultado do teste de alinhamento (U sec. 4 e 7.2).
enum class Alignment : uint8_t {
  kUnknown,   // simbolo ausente ou stats ilegiveis
  kOther,     // 0 ou 1. TODO(RE): 1 nunca foi visto vivo; o filtro o trata como hostil
  kFriendly,  // 2
};

// Le o alinhamento direto dos stats da unidade tipo 0 ou 1 (U sec. 7.2), que da o
// mesmo que D2Common.GetUnitAlignment sem chamar o jogo: o stat 172 da sub-lista
// do estado 105, 0 sem esse estado ou sem stats. false se a memoria nao for legivel.
bool ReadUnitAlignment(const MemoryView& mem, uint32_t unit, int32_t* out);

// mode: Unit+0x10; monstats_flags: registro de monstats +0x0C; type_flags:
// MonsterData+0x16; is_ally: o id da unidade esta na lista de aliados. Sem o
// alinhamento, recusa o bit 0x400 de monstats, que deu o mesmo nas classes vistas.
MonsterClass ClassifyMonster(uint32_t mode, uint32_t monstats_flags, uint8_t type_flags,
                             bool is_ally, Alignment alignment);

enum class ObjectKind : uint8_t {
  kOther,
  kWaypoint,
  kPortalBlue,
  kPortalRed,
  kPortalOther,
  kQuest,
};

// OperateFn no conjunto de objetivos de quest (U sec. 5.4).
bool IsQuestOperateFn(uint8_t operate_fn);

// Colunas do registro de objects. waypoint e portal vem de SubClass; objetivo vem
// de OperateFn, mais as classes que a spec aponta fora do conjunto: 460 (predefinida
// que marca a 558) e 473 (gaiolas).
ObjectKind ClassifyObject(uint32_t object_class, uint8_t sub_class, uint8_t operate_fn);

// Destino fixo pelo nivel atual das classes 298 e 342 (U sec. 5.3), -1 nas demais.
// Na 342 ele so vale com a missao concluida, o que o mod nao tem como saber.
int32_t FixedPortalDestination(uint32_t object_class, int32_t level_id);

// Niveis de cidade (L sec. 2.8).
bool IsTownLevel(int32_t level_id);

// --- Geometria -------------------------------------------------------------

// Lado comum (com comprimento > 0) de dois retangulos encostados, em tiles.
bool SharedEdge(const TileRect& a, const TileRect& b, Point* from, Point* to);

// Sala cujo lado toca o trecho [from, to] de uma fronteira, em tiles.
bool RoomTouchesEdge(const TileRect& room, const Point& from, const Point& to);

// Situacao de um par de celulas vizinhas atraves da fronteira.
enum class EdgeCell : uint8_t {
  kUnknown,  // alguma das salas nao esta carregada
  kBlocked,  // algum dos lados tem o bit de bloqueio de caminhada
  kFree,
};

struct EdgeRun {
  int32_t start = 0;  // indice da primeira celula livre
  int32_t length = 0;
};

// Trechos contiguos de celulas livres.
std::vector<EdgeRun> FreeRuns(const std::vector<EdgeCell>& cells);

// Bordas em que a regra do Ato I foi medida contra a colisao (L sec. 4.1).
bool IsAct1RuleBorder(int32_t level_a, int32_t level_b);

// Celulas livres da regra do Ato I, ao longo da fronteira: [origem + 7, origem + 35)
// em subtiles, com a origem da sala de variante >= 3.
EdgeRun Act1RuleRun(int32_t room_origin_subtile);

// Marca projetada em pixels do quadro do jogo.
struct ScreenMark {
  MarkKind kind = MarkKind::kMonster;
  uint8_t points = 1;
  bool primary = false;
  bool used = false;
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
