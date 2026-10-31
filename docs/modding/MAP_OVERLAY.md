# Mod de mapa (`map-overlay`): monstros, objetos e saídas no automap

Primeiro mod de teste do devkit (itens **C1** e **C2** do
[`ROADMAP.md`](../ROADMAP.md)). Com o automap **do próprio jogo** aberto (tela
cheia ou minimapa), ele desenha por cima:

| Marca | Significado |
|---|---|
| quadrado vermelho pequeno | monstro hostil vivo |
| quadrado rosa pequeno | lacaio de um chefe |
| quadrado azul | campeão |
| losango dourado | único |
| losango laranja com anel | superúnico |
| quadrado verde-claro pequeno | aliado: mercenário ou invocação (nunca conta como hostil) |
| anel verde | NPC |
| quadrado ciano + `WP` | waypoint (coluna do jogo, qualquer ato) |
| sala hachurada ciano + `WP (?)` | sala do waypoint ainda não carregada |
| anel azul + `Portal » nível` | portal da cidade (classe 59) e o destino dele |
| anel vermelho + `Portal » nível` | portal vermelho (classe 60) e o destino dele |
| anel lilás + `Portal` | outros portais por objeto (classes 298 e 342); destino desconhecido |
| quadrado ciano + rótulo | objeto da seção `[objects]` (manual) |
| quadrado amarelo + `» nível` | saída por warp (caverna, escada) com a posição exata |
| sala hachurada amarela + `» nível (?)` | sala que contém um warp cuja posição exata ainda não se conhece |
| linha amarela + `» nível` | borda aberta com outro nível, com salas já ligadas |
| linha tracejada + `» nível (?)` | trecho onde a borda com outro nível está; a passagem exata só aparece depois |
| linha no topo | contagem: nível, monstros, especiais, objetos, saídas |

**F11** (configurável) liga e desliga as marcas. Nada é desenhado fora da partida,
com o automap fechado, com menu do Esc, diálogo de NPC, ajuda (H) ou painéis nos dois
lados, nem nos quadros em que o jogo não desenhou o automap. Em tela cheia, com um
painel lateral aberto, as marcas ficam só na metade livre e a linha de contagem some.

> O mod **não reconstrói o mapa**: o automap é o do jogo. O mod só **lê** o
> estado do cliente e põe marcas em cima. A única exceção é a opção `reveal_exits`,
> desligada por padrão (abaixo).

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
| `[overlay] monsters`, `allies`, `npcs`, `objects`, `exits` | `1` | o que marcar; `objects` vale para waypoints, portais e `[objects]` |
| `[overlay] labels`, `status` | `1` | rótulos e linha de contagem |
| `[overlay] only_current_level` | `1` | ignora unidades de salas carregadas de níveis vizinhos (tiles de saída de outro nível são sempre ignorados) |
| `[overlay] refresh_ms` | `100` | intervalo entre varreduras do estado (20–2000) |
| `[overlay] draw_hook` | `1` | gancho de contagem em `DrawAutomap` (ver abaixo) |
| `[overlay] reveal_exits` | `0` | **experimental**; chama funções do jogo para achar a posição exata dos warps (ver "Saídas exatas") |
| `[objects] <classe> = rótulo` | vazio | objetos que o jogo não classifica, como os de quest |
| `[level_names] <id> = nome` | cidades, 2 e 8 | nome das saídas (sem nome: `nível <id>`) |
| `[d2gl] game_width`, `game_height` | `800`, `600` | só valem se o tamanho do quadro não puder ser lido do jogo |
| `[d2gl] use_d2gl_ini` | `1` | encaixe do quadro conforme o `d2gl.ini` da pasta do jogo |
| `[d2gl] keep_aspect` | `1` | sem `d2gl.ini`: centraliza com barras (ou estica) |

Waypoints e portais **não** precisam de configuração. `[objects]` só rotula classes que
o jogo não classifica; uma classe de waypoint ou portal listada ali continua sendo
tratada pela coluna do jogo.

**Objetivos de quest não são detectados.** O jogo não tem coluna para eles
([spec de unidades §5.1](../specs/game-exe-1.14d-units.md)): `OperateFn` e `AutoMap`
são pistas sem teste, e o mod não as usa. Para marcar um, confira a linha no
`objects.txt` da **sua** instalação e acrescente-a em `[objects]`.

## Como funciona

Fontes: [`game-exe-1.14d-units.md`](../specs/game-exe-1.14d-units.md),
[`game-exe-1.14d-levels-automap.md`](../specs/game-exe-1.14d-levels-automap.md)
e [`addresses/1.14d.ini`](../../aeonian/addresses/1.14d.ini). Código:
`aeonian/examples/map-overlay/` (lógica portável em `map_logic.*`, testada no host
por `ae_map_overlay_test`).

- **Símbolos.** O mod resolve 18 símbolos **por nome** (`resolve_symbol`, que
  confere os bytes) e lê o endereço de cada global de dentro dos bytes
  verificados da âncora (o deslocamento que a descrição do INI dá), sempre com
  `read_original`: funciona mesmo se outro mod já desviou a função. Nenhum
  endereço está escrito no código. Globais que aparecem em duas âncoras são
  conferidos entre si; divergência zera o global. Símbolo ausente ou com bytes
  diferentes: o log diz qual e o mod desliga só o recurso que depende dele
  (sem o jogador e o estado do automap, o mod inteiro fica inativo).
- **Leitura (thread principal, `on_tick`).** A cada `refresh_ms`, com o automap
  aberto: jogador → sala → nível atual; `Vis`/`Warp` do nível (exceções do ato,
  depois `LevelDefs`); as `Room2` do nível (bordas, salas de warp e de waypoint,
  predefinidas) e a tabela de unidades do servidor (monstros, objetos, tiles de
  saída). Toda leitura passa por uma checagem de página (`VirtualQuery`) e todo laço
  tem limite, então um ponteiro ruim vira "nada desenhado", nunca uma exceção dentro
  do jogo. Custo medido no Wine, nos cinco atos: cerca de 0,06 ms em média por
  varredura (máximo 0,34 ms).
- **Monstros.** Tipo por `MonsterData+0x16`, na ordem da spec: `0x02` superúnico,
  `0x04` campeão, `0x08` único, `0x10` lacaio. Superúnico e campeão também têm o bit de
  único, então a ordem importa. Um monstro cujo id está na lista de aliados
  (`[0x7BB5BC]`) é aliado, mesmo passando nos bits de hostil de `monstats`.
- **Objetos.** A tabela de `objects` da instalação é lida em tempo de execução:
  `SubClass & 0x40` é waypoint; `& 0x04` é portal, azul na classe 59 e vermelho na 60,
  com o destino em `ObjectData+0x04`; `& 0x10` são os demais portais. A sala do waypoint
  vem dos bits `0x30000` de `Room2+0x28`, sem carregar nada.
- **Saídas.** Uma sala tem o warp *n* se tiver o bit `0x10 << n` com `Vis[n] ≠ 0` e
  `Warp[n] ≠ −1`. Uma vizinha de outro nível só é borda se o `n` daquele nível tiver
  `Warp[n] = −1`; senão é o destino de um warp. Vizinho aberto sem sala ligada ainda:
  o trecho comum dos retângulos dos dois níveis, marcado como aproximado. As regras
  são as mesmas em todos os atos e tipos de nível.
- **Painéis.** Lado em `[0x7A5210]` e estado de UI em `[0x7A27C0 + i*4]`. Lado 3,
  diálogo de NPC (8), menu do Esc (9) e ajuda (33) escondem tudo. Em tela cheia, lado 1
  corta em `x < largura/2` e lado 2 em `x ≥ largura/2`. No minimapa o recorte do jogo já
  acompanha o painel.
- **Tamanho do quadro.** `[0x71146C]`×`[0x711470]`: 640×480 ou 800×600. Os cortes e o
  encaixe do D2GL usam esses globais.
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
  símbolo depois ainda o recebe e lê a âncora com `read_original`. Se outro mod
  desviou antes, o gancho do `map-overlay` encadeia no dele e os dois rodam
  ([`PLUGIN_API.md`](PLUGIN_API.md), "Vários mods na mesma função").

### D2GL

Sob o D2GL o canvas tem o tamanho da janela. O D2GL 1.3.3 encaixa o quadro do jogo no
tamanho de janela do `d2gl.ini` e o ancora no canto **inferior** esquerdo da área
cliente (quando a área cliente é mais baixa que a janela pedida, o topo é cortado). O
mod lê esse `d2gl.ini` e reproduz o encaixe, com o tamanho do quadro lido do jogo. Isso
é um modelo **observado** (D2GL oficial 1.3.3, Wine 11.20), não uma spec.

## Só o que o cliente já carregou

O cliente só conhece as salas próximas do jogador, então:

- monstros, NPCs e objetos aparecem quando a sala deles carrega;
- a posição **exata** de um warp só aparece quando a sala dele carrega, e fica depois
  que ela descarrega. Antes disso o mod marca a sala inteira (`(?)`), que o nível já
  conhece desde a geração;
- a lista de vizinhas de uma sala só existe depois que ela foi ativada. Até lá, a
  borda com um nível vizinho é só o trecho tracejado.

## Saídas exatas (`reveal_exits`, experimental e desligado)

Com `reveal_exits = 1`, a cada varredura o mod chama `AddRoomData` e logo depois
`RemoveRoomData` na sala de um warp (no máximo uma sala por varredura e uma vez por
sala), só com o automap aberto, na thread principal. A `PresetUnit` da saída fica e a
próxima varredura a encontra. Sala já carregada ou pedida pelo servidor (`Room2+0x0E`)
não é tocada. Serve só para warps: bordas abertas continuam aproximadas até a sala
ativar.

**Limitações.** Isso chama código do jogo, ao contrário do resto do mod.

- A análise o testou só no Ato I, Normal, um jogador, em sessões curtas. Aqui foi
  exercitado também nos Atos II a V, em Normal, por poucos minutos (ver "Validação").
- **Não use em multijogador**: lá o servidor é remoto, esse caminho não foi exercitado e
  o mod não sabe detectar o modo de jogo.
- Não testado: Pesadelo e Inferno, sessões longas, efeito nos arquivos `.ma0`/`.map` do
  personagem e a corrida com um pedido do servidor para a mesma sala.

## Validação (2026-10-31)

Jogo real (`Game.exe` 1.14d, Wine 11.20, CachyOS), invisível (gamescope headless) e
mudo (nenhum fluxo de áudio do jogo em `pactl list sink-inputs`, nenhuma exceção de 1ª
chance no log), numa cópia-espelho e num prefixo de teste. O condutor
`aeonian/tests/integration/mapdrive-plugin/` (só teste) dirige o jogo e grava quadros com
`request_screenshot`; `test_save.py`, na mesma pasta, prepara o personagem de teste
(formato público do `.d2s`, recusa arquivos fora do prefixo de teste). Tudo com **um
jogador**, uma Feiticeira de teste em nível 90 e sessões de até 13 minutos.

| Item | Normal | Pesadelo | Inferno | Renderizador |
|---|---|---|---|---|
| Níveis percorridos | 1, 3, 9; 40, 42, 43, 46, 57, 74; 75, 76, 78, 79, 101; 103, 106, 107; 109, 111, 113 | 1, 3; 40, 42; 75, 76; 103, 106; 109, 111 | os mesmos do Pesadelo | GDI. D2GL: 1, 3, 40, 42, 75, 76, 103, 106, 109, 111, só em Normal |
| Waypoint pela coluna, classes 119, 156, 237, 238, 398, 429, 496 | ✅ | ✅ | ✅ | GDI; D2GL em Normal |
| Waypoint pela coluna, classes 324, 402, 511 | ✅ | não visitadas | não visitadas | GDI |
| Sala do waypoint pela flag, antes de carregar | ✅ nível 40 | ✅ nível 40 | ✅ nível 40 | GDI; D2GL em Normal |
| Lacaio | ✅ contagem (Atos I, II, III, V) | ✅ contagem (II, V) | ✅ contagem (I, II, V) | GDI; D2GL em Normal |
| Único | ✅ contagem (Atos II, III, V) | ✅ rótulo do jogo: "Blood Fist the Impaler" | ✅ rótulo: "Bone Growler the Wraith" | GDI; D2GL em Normal |
| Campeão | ✅ contagem: grupo de 4 no nível 78 | ✅ rótulos: "Champion Spear Cat", "Champion Burning Dead Archer" | ✅ rótulo: "Fanatic Undead Scavenger" (nível 42) | GDI |
| Superúnico | ✅ contagem (classes 58 e 453) | ✅ contagem | ✅ contagem | GDI; D2GL em Normal |
| Mercenário (271) como aliado, dono = jogador | ✅ | ✅ | ✅ | GDI; D2GL em Normal |
| Hidras (351–353) como aliados | ✅ nível 42 | ✅ nível 42 | ✅ nível 42 | GDI; D2GL em Normal |
| Portal azul (59) com destino | ✅ 76→75, 74→40 | — | — | GDI |
| Portal vermelho (60) com destino | 🟡 visto pela predefinida, sem destino | — | ✅ nível 111, destino 125 | GDI |
| Portal do Santuário Arcano (298), sem destino | ✅ nível 74 | — | — | GDI |
| Saída exata e sala de warp, em externo e labirinto | ✅ todos os atos | ✅ níveis 3, 42, 76, 106 | ✅ níveis 3, 42, 76, 106 | GDI; D2GL em Normal |
| Borda aproximada (tracejada) | ✅; no nível 3 uma delas passou a exata ao chegar perto | ✅ vista | ✅ vista | GDI; D2GL em Normal |
| Painel à direita, à esquerda, dois lados, ajuda; tela cheia e minimapa | ✅ | ✅ | ✅ | GDI; D2GL em Normal (800×600 e 1280×720) |
| Diálogo de NPC esconde as marcas | ✅ | ✅ | ✅ | GDI |
| 640×480: recorte e metade do painel seguem o quadro, tela cheia e minimapa | ✅ níveis 40 e 42 | — | — | GDI |

A comparação de quadros com as marcas ligadas e desligadas mostrou **0 pixels diferentes
sobre o painel**, à direita e à esquerda, e milhares na metade livre. A suíte confere isso
na fase E (Ato I, GDI, 124 PASS em 2026-10-31).

`reveal_exits`, GDI, Normal, um jogador, sessões de 1 a 3 minutos: 17 chamadas, no pior
caso 3,4 ms, nos níveis 3 e 9 (Ato I), 40, 42 e 46 (Ato II, as 7 tumbas apareceram de uma
vez), 101 (Ato III) e 113 (Ato V). Depois disso andar e salvar e sair seguiram normais.
Só no nível 3 o personagem foi até a posição revelada e **entrou no warp**, chegando ao
nível 9; nos demais a posição não foi conferida contra a unidade tile. Nos níveis 106 e
111 não havia sala para revelar.

**Não validado:** Windows nativo; multijogador; DirectDraw; D2GL em tela cheia, esticado,
em 640×480, em Pesadelo e em Inferno; diálogo de NPC sob o D2GL; campeão sob o D2GL;
`reveal_exits` em Pesadelo e Inferno ou em sessões longas; o portal azul visto do lado da
cidade; outros portais vermelhos; a classe 342; cubo, troca e os demais painéis não
listados; golem, esqueleto, valquíria e lobos como aliados; instalação oficial da loja
(os testes usam MPQs 1.14b + `Game.exe` 1.14d). O rótulo do jogo não foi conferido em
lacaios nem em superúnicos.

## `TODO(RE)` que dependem de nova análise

1. **Falso positivo na cidade do Ato III:** a classe 359 passa nos bits de `monstats` e
   aparece como hostil no nível 75, nas três dificuldades. O filtro da spec só foi
   conferido numa amostra de classes.
2. Objetos de quest: confirmar em jogo quais são (`OperateFn`, `AutoMap`). Até lá, só
   pela seção `[objects]`.
3. Destino das classes 298 e 342.
4. Posição exata da passagem entre níveis externos sem ativar sala: o trecho comum dos
   retângulos é só onde ela está.
5. Outros tipos de aliado e o que muda com outro jogador na partida.
6. Índices de UI não abertos (cubo, troca com NPC, jogadores, chat).
7. Encaixe do D2GL: hoje é o modelo observado, sem spec, e não foi visto em 640×480.
8. `reveal_exits`: multijogador, sessões longas, efeito nos arquivos `.ma0`/`.map` e a
   corrida com um pedido do servidor para a mesma sala.
9. Como detectar o modo de jogo (um jogador ou multijogador), para o mod poder recusar
   `reveal_exits` sozinho.
