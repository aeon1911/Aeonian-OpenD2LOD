# Mod de mapa (`map-overlay`): monstros, objetivos e saídas no automap

Primeiro mod de teste do devkit (itens **C1** e **C2** do
[`ROADMAP.md`](../ROADMAP.md)). Com o automap **do próprio jogo** aberto (tela
cheia ou minimapa), ele desenha por cima:

| Marca | Significado |
|---|---|
| quadrado vermelho pequeno | monstro hostil vivo |
| quadrado azul | campeão |
| losango dourado | único |
| losango laranja com anel | superúnico |
| anel verde | NPC (cidade, Caim etc.) |
| quadrado ciano + rótulo | objeto da seção `[objects]` (ex.: `WP`) |
| quadrado amarelo + `» nível` | saída por warp (caverna, escada) com a posição exata |
| sala hachurada + `» nível (?)` | sala que contém um warp ainda não carregado (posição aproximada) |
| linha amarela + `» nível` | borda aberta com outro nível (ex.: cidade ↔ Charneca) |
| linha no topo | contagem: nível, monstros, especiais, objetivos, saídas |

**F11** (configurável) liga e desliga as marcas. Nada é desenhado fora da
partida, com o automap fechado, com o menu do Esc aberto ou nos quadros em que o
jogo não desenhou o automap.

> O mod **não reconstrói o mapa**: o automap é o do jogo. O mod só **lê** o
> estado do cliente e põe marcas em cima.

## Instalar

```
<jogo>\aeonian\mods\map-overlay\mod.ini           (aeonian/examples/map-overlay/mod.ini)
<jogo>\aeonian\mods\map-overlay\map_overlay.dll   (build/win32/map_overlay.dll)
<jogo>\aeonian\mods\map-overlay\map-overlay.ini   (opcional; aeonian/examples/map-overlay/)
```

## Configuração (`map-overlay.ini`)

| Chave | Padrão | Efeito |
|---|---|---|
| `[overlay] enabled` | `1` | começa com as marcas ligadas |
| `[overlay] toggle_key` | `f11` | `f1`…`f24` ou letra/número, com `ctrl+`/`shift+`; a tecla é consumida pelo mod |
| `[overlay] monsters`, `npcs`, `objects`, `exits` | `1` | o que marcar |
| `[overlay] labels`, `status` | `1` | rótulos e linha de contagem |
| `[overlay] only_current_level` | `1` | ignora unidades de salas carregadas de níveis vizinhos |
| `[overlay] refresh_ms` | `100` | intervalo entre varreduras do estado (20–2000) |
| `[overlay] draw_hook` | `1` | gancho de contagem em `DrawAutomap` (ver abaixo) |
| `[objects] <classe> = rótulo` | `119 = WP` | linhas de `objects.txt` marcadas como objetivo |
| `[level_names] <id> = nome` | 1, 2, 8 | nome das saídas (sem nome: `nível <id>`) |
| `[d2gl] game_width`, `game_height` | `800`, `600` | tamanho do quadro do jogo sob o D2GL |
| `[d2gl] use_d2gl_ini` | `1` | encaixe do quadro conforme o `d2gl.ini` da pasta do jogo |
| `[d2gl] keep_aspect` | `1` | sem `d2gl.ini`: centraliza com barras (ou estica) |

Só a classe **119 (waypoint do Ato I)** está confirmada na spec. Para marcar
outros objetos (outros waypoints, objetos de quest), confira a linha no
`objects.txt` da **sua** instalação e acrescente-a em `[objects]`.

## Como funciona

Fontes: [`game-exe-1.14d-units.md`](../specs/game-exe-1.14d-units.md),
[`game-exe-1.14d-levels-automap.md`](../specs/game-exe-1.14d-levels-automap.md)
e [`addresses/1.14d.ini`](../../aeonian/addresses/1.14d.ini). Código:
`aeonian/examples/map-overlay/` (lógica portável em `map_logic.*`, testada no host
por `ae_map_overlay_test`).

- **Símbolos.** O mod resolve 10 símbolos **por nome** (`resolve_symbol`, que
  confere os bytes) e lê o endereço de cada global de dentro dos bytes
  verificados da âncora (o deslocamento que a descrição do INI dá). Nenhum
  endereço está escrito no código. Globais que aparecem em duas âncoras são
  conferidos entre si; divergência zera o global. Símbolo ausente ou com bytes
  diferentes: o log diz qual e o mod desliga só o recurso que depende dele
  (sem o jogador e o estado do automap, o mod inteiro fica inativo).
- **Leitura (thread principal, `on_tick`).** A cada `refresh_ms`, com o automap
  aberto: jogador → sala → nível atual; `Vis`/`Warp` do nível (exceções do ato,
  depois `LevelDefs`); as `Room2` do nível (bordas com outros níveis, salas com o
  bit de warp, predefinidas das salas carregadas) e a tabela de unidades do
  servidor (monstros, objetos, tiles de saída). Toda leitura passa por uma
  checagem de página (`VirtualQuery`) e todo laço tem limite, então um ponteiro
  ruim vira "nada desenhado", nunca uma exceção dentro do jogo. Custo medido:
  0,045 ms em média por varredura (máximo 0,32 ms) no Wine.
- **Desenho (`on_present`).** A lista de marcas é publicada sob um `SRWLOCK`; o
  `on_present` só desenha. No GDI (thread principal) ele ainda relê o estado do
  automap do próprio quadro; no D2GL (thread de renderização) não lê memória do
  jogo e usa o estado do último tick.
- **Fórmula** (spec de níveis §5.2): `ax = cx/div − offX + 8`,
  `ay = cy/div − offY − 8`, mais o ajuste da cruz do jogo no minimapa
  (`x − 1`, `y + 5`); o recorte do jogo descarta pontos fora dele.
- **Gancho de contagem.** `D2Client.DrawAutomap` é desviado nos 7 bytes da 1ª
  instrução (sem operando relativo, como a spec recomenda); o desvio só
  incrementa um contador e segue para a original, sem tocar em pilha nem
  registradores. Serve para desenhar só nos quadros em que o jogo desenhou o
  automap. Depois do desvio os bytes de `DrawAutomap` mudam (o `jmp` cobre a
  âncora em `+2`), mas o motor guarda os originais: outro mod que resolver esse
  símbolo depois ainda o recebe e lê a âncora com `read_original`
  ([`PLUGIN_API.md`](PLUGIN_API.md), "Vários mods na mesma função").

### D2GL

Sob o D2GL o canvas tem o tamanho da janela. O D2GL 1.3.3 encaixa o quadro de
800×600 no tamanho de janela do `d2gl.ini` e o ancora no canto **inferior**
esquerdo da área cliente (quando a área cliente é mais baixa que a janela pedida,
o topo é cortado). O mod lê esse `d2gl.ini` e reproduz o encaixe. Isso é um modelo
**observado** (D2GL oficial 1.3.3, Wine 11.20), não uma spec.

## Só o que o cliente já carregou

O mod **não chama** `AddRoomData`, `InitLevel` nem nada que altere o jogo. O
cliente só conhece as salas próximas do jogador, então:

- monstros, NPCs e objetos aparecem quando a sala deles carrega;
- a posição **exata** de um warp só aparece quando a sala dele carrega; antes
  disso o mod marca a sala inteira (hachurada, `(?)`), que o nível já conhece;
- bordas com outros níveis existem desde a geração do nível.

## Validação (2026-10-29)

Jogo real (`Game.exe` 1.14d, Wine 11.20, CachyOS), invisível (gamescope
headless) e mudo, numa cópia-espelho com personagem de teste novo. O condutor
`aeonian/tests/integration/mapdrive-plugin/` (só teste) cria o personagem, abre o
automap e grava quadros com `request_screenshot`.

| Cenário | Resultado |
|---|---|
| 10/10 símbolos resolvidos, gancho instalado | ✅ suíte, fase A3 |
| bytes errados num símbolo → só o recurso dele desligado | ✅ suíte, fase A3 |
| cidade do Ato I: NPCs sobre as cruzes do jogo, waypoint, borda com a Charneca, tela cheia e minimapa | ✅ suíte, fase E (com `AEONIAN_D2_LNG`) e quadros |
| Charneca Sangrenta: monstros, borda com a cidade, sala do Covil do Mal (aproximada) e depois a entrada exata | ✅ quadros (roteiro `charneca.txt`, manual) |
| F11 desliga/religa; menu do Esc aberto não mostra marcas | ✅ quadros |
| GDI 800×600 | ✅ |
| D2GL oficial 1.3.3, janela 800×600 e 1280×720 (área cliente 1280×700) | ✅ quadros (marcas sobre as cruzes do jogo) |

**Não validado:** campeões, únicos e superúnicos (nenhum apareceu nos testes; os
bits são 🔷 na spec); lacaios; mercenários e invocações; Atos II–V; dificuldades
acima do Normal; multijogador; D2GL em tela cheia ou esticado; DirectDraw;
painéis abertos (inventário etc.) sobre o automap; Windows nativo.

## `TODO(RE)` que dependem de nova análise

1. Bit de lacaio (`MonsterData+0x16`, `0x10`): hoje lacaios aparecem como
   normais.
2. Dono da unidade (mercenários/invocações aparecem como monstros hostis).
3. Colunas do registro de `objects` (para classificar waypoints e objetos de
   quest sem configuração manual).
4. Estado de painel aberto (`0x7A5210` não tem âncora no INI): com um painel
   aberto o `on_present` desenha por cima dele.
5. Resolução do quadro fora de 800×600 e o encaixe do D2GL (hoje: configuração +
   modelo observado).
6. Semântica do bit `0x10 << n` de `Room2+0x28` em labirintos (o mod descarta
   aproximações com mais de 3 salas candidatas).
7. Na Charneca, só a borda com a cidade apareceu; a borda com os Campos Frios
   (nível 3) não veio das listas de vizinhas: confirmar como o jogo liga níveis
   externos ainda não visitados.
8. Portais que são objetos (portal da cidade, portais vermelhos) não são
   marcados: faltam as classes.
