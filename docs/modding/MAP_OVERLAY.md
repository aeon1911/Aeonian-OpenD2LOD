# Mod de mapa (`map-overlay`): monstros, objetivos, objetos e saídas no automap

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
| anel verde-pálido pequeno | amigo fora da lista de aliados: classes 359 e 522 da spec, e a 534 vista aqui (nunca conta como hostil) |
| anel verde | NPC |
| losango magenta em moldura + `Objetivo` | objetivo de quest, pela coluna `OperateFn` do jogo |
| moldura cinza + `Objetivo (usado)` | objetivo já operado (modo ≠ 0). **Sem** "(usado)" não quer dizer que a missão esteja pendente |
| quadrado ciano + `WP` | waypoint (coluna do jogo, qualquer ato) |
| sala hachurada ciano + `WP (?)` | sala do waypoint ainda não carregada |
| anel azul + `Portal » nível` | portal da cidade (classe 59) e o destino dele |
| anel vermelho + `Portal » nível` | portal vermelho (classe 60) e o destino dele |
| anel lilás + `Portal » nível` | portais das classes 298 e 342, com o destino pelo nível atual: 74 → 54, 54 → 74, 102 → 103. A 342 só leva ao destino com a missão concluída, e o mod não sabe se ela está |
| quadrado ciano + rótulo | objeto da seção `[objects]` (manual) |
| quadrado amarelo + `» nível` | saída por warp (caverna, escada) com a posição exata |
| sala hachurada amarela + `» nível (?)` | sala que contém um warp cuja posição exata ainda não se conhece |
| linha amarela + `» nível` | passagem **exata** para o nível vizinho, pela colisão ou pela regra do Ato I |
| linha tracejada + `» nível (aprox.)` | trecho comum dos dois níveis: a passagem está em algum lugar dele, **não** é a passagem |
| linha no topo | contagem: nível, monstros, especiais, objetos, objetivos, saídas |

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
| `[overlay] monsters`, `allies`, `npcs`, `objects`, `quests`, `exits` | `1` | o que marcar; `npcs` inclui os amigos fora da lista de aliados; `objects` vale para waypoints, portais e `[objects]` |
| `[overlay] labels`, `status` | `1` | rótulos e linha de contagem |
| `[overlay] only_current_level` | `1` | ignora unidades de salas carregadas de níveis vizinhos (tiles de saída de outro nível são sempre ignorados) |
| `[overlay] refresh_ms` | `100` | intervalo entre varreduras do estado (20–2000) |
| `[overlay] draw_hook` | `1` | gancho de contagem em `DrawAutomap` (ver abaixo) |
| `[overlay] reveal_exits` | `0` | **experimental**; chama funções do jogo para achar a posição exata dos warps (ver "Saídas exatas") |
| `[objects] <classe> = rótulo` | vazio | marca outra classe, ou troca o rótulo de um objetivo |
| `[objects] <classe> = -` | vazio | tira a classe dos objetivos |
| `[level_names] <id> = nome` | cidades, 2 e 8 | nome das saídas (sem nome: `nível <id>`) |
| `[d2gl] game_width`, `game_height` | `800`, `600` | só valem se o tamanho do quadro não puder ser lido do jogo |
| `[d2gl] use_d2gl_ini` | `1` | encaixe do quadro conforme o `d2gl.ini` da pasta do jogo |
| `[d2gl] keep_aspect` | `1` | sem `d2gl.ini`: centraliza com barras (ou estica) |

Waypoints, portais e objetivos **não** precisam de configuração. Uma classe de waypoint ou
portal listada em `[objects]` continua sendo tratada pela coluna do jogo.

## Como funciona

Fontes: [`game-exe-1.14d-units.md`](../specs/game-exe-1.14d-units.md),
[`game-exe-1.14d-levels-automap.md`](../specs/game-exe-1.14d-levels-automap.md)
e [`addresses/1.14d.ini`](../../aeonian/addresses/1.14d.ini). Código:
`aeonian/examples/map-overlay/` (lógica portável em `map_logic.*`, testada no host
por `ae_map_overlay_test`).

- **Símbolos.** O mod resolve 19 símbolos **por nome** (`resolve_symbol`, que
  confere os bytes) e lê o endereço de cada global de dentro dos bytes
  verificados da âncora (o deslocamento que a descrição do INI dá), sempre com
  `read_original`: funciona mesmo se outro mod já desviou a função. Nenhum
  endereço está escrito no código. Globais que aparecem em duas âncoras são
  conferidos entre si; divergência zera o global. Símbolo ausente ou com bytes
  diferentes: o log diz qual e o mod desliga só o recurso que depende dele
  (sem o jogador e o estado do automap, o mod inteiro fica inativo). O
  `D2Common.GetUnitAlignment` é o único que entra pelo próprio endereço: o mod não o
  chama, mas só usa o layout dos stats se os bytes dele conferirem.
- **Leitura (thread principal, `on_tick`).** A cada `refresh_ms`, com o automap
  aberto: jogador → sala → nível atual; `Vis`/`Warp` do nível (exceções do ato,
  depois `LevelDefs`); as `Room2` do nível (salas de warp e de waypoint, predefinidas),
  a tabela de unidades do servidor (monstros, objetos, tiles de saída) e, nas fronteiras
  abertas, a colisão das salas carregadas. Toda leitura passa por uma checagem de página (`VirtualQuery`) e todo laço
  tem limite, então um ponteiro ruim vira "nada desenhado", nunca uma exceção dentro
  do jogo. Custo medido no Wine, nos cinco atos: até 0,08 ms em média por varredura
  (máximo 1,9 ms).
- **Monstros.** Inimigo é `monstats+0x0C` sem `0x100`, com `0x8000`, vivo e com alinhamento
  ≠ 2 ([spec de unidades §4](../specs/game-exe-1.14d-units.md)). O alinhamento é lido dos
  stats da unidade (§7.2), sem chamar o jogo. Alinhamento 2 fora da lista de aliados vira
  **amigo** (classes 359 e 522 da spec; a 534 também, vista em jogo). A lista (`[0x7BB5BC]`) só diz quem é aliado e de quem; ela
  não filtra por tipo, então os tipos novos de §7.1 já entram. Tipo do monstro por
  `MonsterData+0x16`, na ordem da spec: `0x02` superúnico, `0x04` campeão, `0x08` único,
  `0x10` lacaio. Superúnico e campeão também têm o bit de único, então a ordem importa.
- **Falha segura do alinhamento.** Sem o símbolo, ou com os stats de uma unidade ilegíveis, o
  mod recusa o bit `0x400` de `monstats` e avisa no log: as 13 classes com esse bit vistas
  vivas são amigas, e nenhuma hostil apareceu com ele. As outras 30 classes com o bit nunca
  foram vistas vivas (§4); nesse modo, uma hostil entre elas apareceria como amiga.
- **Objetos.** A tabela de `objects` da instalação é lida em tempo de execução:
  `SubClass & 0x40` é waypoint; `& 0x04` é portal, azul na classe 59 e vermelho na 60,
  com o destino em `ObjectData+0x04`; `& 0x10` são os portais 298 e 342, com o destino
  pelo nível atual. A sala do waypoint vem dos bits `0x30000` de `Room2+0x28`, sem carregar
  nada.
- **Objetivos de quest.** `OperateFn` (`registro+0x1B3`) em {6, 9, 10, 12, 21, 24, 25, 28,
  31, 39–42, 49, 52–59, 65, 67}, sem lista de classes (§5.4). Valem as duas fontes: a
  predefinida (sala carregada) e a unidade. Mais as classes que a spec aponta fora do
  conjunto: 460, a predefinida que marca o lugar da 558 (mesma marca), e 473, as gaiolas.
  Waypoint e portal vêm antes, e os falsos positivos da spec (44, 45, 62–64, 66, 69–73)
  ficam de fora. O mod só diz que o objeto foi usado (modo ≠ 0, lembrado até o mapa mudar):
  modo 0 **não** prova que a missão está pendente, e por isso ele nunca diz "pendente".
- **Saídas por warp.** Uma sala tem o warp *n* se tiver o bit `0x10 << n` com `Vis[n] ≠ 0` e
  `Warp[n] ≠ −1`. As regras são as mesmas em todos os atos e tipos de nível.
- **Passagens abertas** (`Vis[n] ≠ 0`, `Warp[n] = −1`, retângulos dos níveis encostados;
  [spec de níveis §4.1](../specs/game-exe-1.14d-levels-automap.md)). A vizinhança das `Room2`
  só dá a fronteira, então **não** entra mais. Em ordem:
  1. **Colisão**, em qualquer ato, com `Room1` dos dois lados: pares de células vizinhas
     através da fronteira, livres dos dois lados (bit `0x0001` de `Room1+0x20`). Trechos com
     menos de 7 subtiles, a menor passagem medida, não contam (ver "Validação"). A passagem
     calculada fica lembrada depois que as salas descarregam, até o mapa mudar.
  2. **Regra do Ato I**, sem ativar nada, só nas bordas medidas (2|3, 3|4, 3|17, 5|6, 6|7):
     a única sala de `variante ≥ 3` entre as que tocam o trecho comum, células de +7 a +34.
     Mais de uma ou nenhuma: não vale.
  3. **Aproximada** nos demais casos: o trecho comum dos retângulos, tracejado e com
     `(aprox.)`. Inclui as outras bordas do Ato I (1|2, 7|26…) até as salas carregarem.
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
- os objetivos também. Saber de longe em que nível está um objetivo, como a tumba
  verdadeira do Ato II, que muda com o mapa, exigiria carregar as salas, e o mod não faz isso;
- a posição **exata** de um warp só aparece quando a sala dele carrega, e fica depois
  que ela descarrega. Antes disso o mod marca a sala inteira (`(?)`), que o nível já
  conhece desde a geração;
- a passagem aberta é exata de longe só nas bordas medidas do Ato I. Nas demais, ela
  fica tracejada até as salas dos dois lados carregarem.

## Saídas exatas (`reveal_exits`, experimental e desligado)

Com `reveal_exits = 1`, a cada varredura o mod chama `AddRoomData` e logo depois
`RemoveRoomData` na sala de um warp (no máximo uma sala por varredura e uma vez por
sala), só com o automap aberto, na thread principal. A `PresetUnit` da saída fica e a
próxima varredura a encontra. Sala já carregada ou pedida pelo servidor (`Room2+0x0E`)
não é tocada. Serve só para warps: passagens abertas seguem as regras acima.

**Proteções** ([spec de níveis §3.1](../specs/game-exe-1.14d-levels-automap.md)). Pedir
nível de outro ato fecha o jogo, e nível sem sala ativa perde as `Room2` e as predefinidas.
Antes de cada chamada o mod confere, só lendo memória, e recusa com aviso no log se algo não
bater:

- a sala do jogador (`Room1+0x2C`) está no ato do cliente, e `Act+0x14` é o ato do jogador;
- o id do nível pertence a esse ato, e o `Level*` está na corrente de `DrlgAct+0x47C`, cujo
  `DrlgAct+0x46C` volta ao mesmo `Act*`;
- o nível atual tem sala carregada: o mod só chama no nível em que o jogador está;
- o warp leva a um nível do mesmo ato, porque a ligação do warp gera o destino.

**Limitações.** Isso chama código do jogo, ao contrário do resto do mod.

- Testado só em Normal, um jogador, em sessões curtas: a análise chamou as mesmas funções
  nos Atos I–V, e aqui o mod as exercitou nos níveis de "Validação".
- **Não use em multijogador**: lá o servidor é remoto, esse caminho não foi exercitado e
  o mod não sabe detectar o modo de jogo.
- Não testado: Pesadelo e Inferno, sessões longas, efeito nos arquivos `.ma0`/`.map` do
  personagem e a corrida com um pedido do servidor para a mesma sala.

## Validação da terceira análise (2026-10-31)

Mesmas condições da seção seguinte: jogo real, invisível, mudo, **um jogador**, Feiticeira de
teste em nível 90, numa cópia-espelho. Cerca de 30 execuções curtas (de 33 s a 9 min), **nenhuma exceção de
1ª chance** e **nenhum fluxo de áudio do jogo** nas amostras de `pactl list sink-inputs`. Varredura:
média de até 0,082 ms e máxima de 1,9 ms, agora com a colisão. "D2GL" é a janela de 800×600 em
Normal, salvo onde indicado.

### Objetivos de quest

Valores de `OperateFn` da [spec de unidades §5.4](../specs/game-exe-1.14d-units.md), separados pelo
que os confirma:

| Confirmação | `OperateFn` |
|---|---|
| ✅ visto em jogo **com este mod** | 6, 9, 12, 42, 49, 53, 65; e a classe 473 (`OperateFn` 0) |
| 🟡 visto em jogo só **na análise** | 21, 25, 31, 67 (e a 460 como predefinida da 558) |
| 🔷 só **no binário e no censo**, nunca vistos em jogo | 10, 24, 28, 39, 40, 41, 52, 54, 55, 56, 57, 58, 59 |

| Ato | Classe | Nível | Normal | Pesadelo | Inferno | D2GL |
|---|---|---|---|---|---|---|
| I | 8 e as 5 pedras 17–21 | 4 | ✅ | não visitado | não visitado | não visitado |
| I | 30 (rótulo do jogo "Tree of Inifuss") | 5 | ✅ | ✅ | não achada no tempo da rodada | não: o clique no waypoint da cidade do Ato I falhou |
| II | 357 | 74 | ✅ | não visitado | não visitado | não visitado |
| III | 404 | 83 | ✅ | ✅ | ✅ | ✅ |
| IV | 376 | 107 | ✅ | não visitado | não visitado | não visitado |
| V | 473, as 3 gaiolas | 111 | ✅ | ✅ | ✅ | ✅ |
| V | 546 | 120 | ✅ | não visitado | não visitado | não visitado |

- **"(usado)":** modo 0 → ≠ 0 ao clicar, no próprio log: classes 30 (Normal, Pesadelo), 546 e 357
  (Normal). Continua marcado depois que a unidade sai da tabela. Com as pedras, o clique não mudou nada,
  como a spec descreve. O personagem de teste não tinha nenhum objetivo marcado antes dos cliques.
- **Rótulo manual e `-` em `[objects]`:** só teste de host.
- **Não alcançados em jogo:** 558/460 (o clique na entrada do nível 114 não respondeu em 3 tentativas) e
  252 (duas varreduras de quase 3 minutos no nível 78 não a acharam). A tumba verdadeira e os selos do
  nível 108 não foram visitados.
- **Diferença com a spec:** nesta rodada, clicar na classe 357 tirou o objeto do modo 0; a spec registra
  "clique sem efeito". Não foi investigado (`TODO(RE)`).

### Filtro de monstros

- **Alinhamento lido pelo mod × função do jogo:** o condutor de teste chama
  `D2Common.GetUnitAlignment` em todos os monstros da tabela e compara: 14 conferências, 258 monstros, 48
  classes, nos Atos I–V e nas três dificuldades, **0 diferenças**. Quem chama é só o condutor; o mod lê
  os stats.
- **Classe 359, cidade do Ato III:** amigo, com 0 hostis na cidade, em Normal, Pesadelo e Inferno, e em
  D2GL Normal. **Nenhum monstro hostil apareceu em nenhuma cidade** em nenhuma execução.
- **Classe 522, nível 111:** amigo nas três dificuldades e em D2GL Normal.
- **Classe 534, nível 111:** alinhamento 2 sem nó na lista, em volta das gaiolas, nas mesmas execuções. A
  spec só cita 359 e 522; a regra dela cobre a 534 sem nenhuma mudança (`TODO(RE)` para a análise).
- **Falha segura:** com os bytes do símbolo adulterados, a suíte confere o aviso no log e que o mapa segue
  ativo (fase A3). O resultado do bit `0x400` nesse modo só tem teste de host.
- **Aliados:** o mercenário (tipo 7) e as hidras (tipo 19) seguem como aliados. Os demais tipos de §7.1 não
  foram revistos: a análise os viu, e o código não filtra por tipo.

### Passagens abertas

| Caso | Borda | Calculado | Conferência | Dificuldade, renderizador |
|---|---|---|---|---|
| Regra do Ato I | 2\|3 | (5687..5715, 5480) | igual à colisão; **cruzou a pé** em (5706, 5480) | Normal, GDI |
| Regra do Ato I | 3\|4 | (5400, 5207..5235) | igual à colisão | Normal, GDI |
| Regra do Ato I | 3\|17 | (5727..5755, 5080) | igual à colisão | Normal, GDI |
| Regra do Ato I | 5\|6 | (15527..15555, 5570) | igual à colisão; **cruzou a pé** em (15530, 5569) | Normal, Pesadelo, Inferno, GDI |
| Regra do Ato I | 6\|7 | (15400, 5297..5325) | igual à colisão; **cruzou a pé** em (15399, 5319) | Normal, GDI |
| Colisão | 1\|2 | (5745..5760, 5960) | **cruzou a pé** nos dois renderizadores | Normal, GDI e D2GL 1280×720 |
| Colisão | 42\|43 | (4760, 5713..5722) | **cruzou a pé** em (4760, 5720) | Normal, GDI |
| Colisão | 111\|112 | (3000, 4991..5010) | **cruzou a pé** em (2999, 5001) | Normal, GDI |
| Colisão | 75\|76 (7), 43\|44 (9), 76\|77 (26 + 25), 78\|79 (26), 110\|111 (17) | larguras dentro das faixas da spec | só calculado; a travessia de 76\|77 foi por Teletransporte | Normal; 75\|76 também em Pesadelo e Inferno; 75\|76 e 76\|77 em D2GL |
| Colisão | 107\|108 (10) | (7794..7804, 5600) | só calculado: a spec não tinha medida do Ato IV, e o personagem não chegou a pé | Normal, GDI |
| Aproximada | 7\|26, 41\|42, 76\|78, 82\|83, 105\|106 e outras | trecho comum | tracejada com `(aprox.)`; nenhuma apresentada como exata | Normal; D2GL em 76 e 83 |

As coordenadas são de um mapa só, o do personagem de teste. A análise mediu a regra do Ato I em duas
sementes; aqui, em uma.

**Limite de 7 subtiles:** decisão do mod, não da spec. A colisão da borda 1\|2 mostrou também uma fresta de
2 subtiles em (5821..5823, 5960). O personagem foi até ela a pé e **parou** em (5821, 5959), sem cruzar, e a
menor passagem medida pela spec tem 7. Trechos menores que isso não são desenhados.

**Teste negativo incompleto:** tentar cruzar a borda **longe** da passagem não deu resultado, porque em todas
as tentativas (3|4, 5|6, 6|7, 42|43, 76|77, 111|112) o Teletransporte não chegou perto da borda naquele ponto.
A prova negativa desta rodada é só a fresta acima; a da análise segue em §4.1 da spec.

### `reveal_exits` e portais

- **`reveal_exits`** (GDI, Normal, ligado só nesta execução): 5 chamadas, pior caso 1,8 ms, nos níveis 3, 42 e
  113. No nível 76 não havia sala para revelar. Jogo salvo e encerrado normalmente, sem recusa nem exceção.
  As recusas das proteções só têm teste de host: nenhuma situação de risco ocorreu em jogo.
- **Destino dos portais:** classe 298 → nível 54 no nível 74, e classe 342 → nível 103 no nível 102, só no log
  e em Normal, GDI. Nenhum dos dois foi atravessado nesta rodada; a análise usou os dois.

### D2GL

Release oficial v1.3.3, Wine 11.20, janela de 800×600, Normal: **imagem completa dentro da partida**, com as
marcas de objetivo, amigo, passagem exata e aproximada alinhadas nos quadros gravados, no Ato III e no Ato V. Em
1280×720 só a cidade do Ato I e o nível 2. Sem novidade sobre tela cheia, esticado, 640×480, Pesadelo e Inferno.

## Validação da segunda análise (2026-10-31)

Feita antes do lote acima, com as regras de então: as linhas de borda e o filtro de hostis mudaram
depois dela.

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
| Portal do Santuário Arcano (298) | ✅ nível 74 | — | — | GDI |
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
cidade; outros portais vermelhos; cubo, troca e os demais painéis não
listados; golem, esqueleto, valquíria e lobos como aliados; instalação oficial da loja
(os testes usam MPQs 1.14b + `Game.exe` 1.14d). O rótulo do jogo não foi conferido em
lacaios nem em superúnicos.

## `TODO(RE)` que dependem de nova análise

1. Objetivos: 13 dos 24 valores de `OperateFn` só têm o tratador e a predefinida; de onde o
   cliente tira o estado das missões, para saber o que está pendente ([spec de unidades
   §5.4](../specs/game-exe-1.14d-units.md)); o clique na classe 357, que aqui mudou o modo.
2. Passagens: posição exata sem ativar sala fora do Ato I; a passagem 107|108, calculada aqui e
   não atravessada; a regra nas bordas do Ato I não medidas (1|2, 7|26, 26|27); demais bits da
   colisão; frestas estreitas como a de 1|2.
3. Filtro: alinhamento 1 e monstro convertido nunca vistos vivos; 529 das 626 classes que
   passam nos bits não foram vistas vivas. Classe 534, amiga e fora da spec.
4. Portais: classes 100 e 377; outros portais vermelhos.
5. Aliados: esqueleto, revivido e o que muda com outro jogador na partida.
6. Índices de UI não abertos (cubo, troca com NPC, jogadores, chat).
7. Encaixe do D2GL: hoje é o modelo observado, sem spec, e não foi visto em 640×480.
8. `reveal_exits`: multijogador, sessões longas, efeito nos arquivos `.ma0`/`.map` e a
   corrida com um pedido do servidor para a mesma sala.
9. Como detectar o modo de jogo (um jogador ou multijogador), para o mod poder recusar
   `reveal_exits` sozinho.
