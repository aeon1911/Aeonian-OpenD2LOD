# Escopo do Produto — Aeonian-OpenD2LOD

> **Documento de alinhamento.** Define o que o projeto é, o que entrega e o
> que **não** faz. Em dúvida entre "reconstruir o jogo" e "abrir o jogo
> real", a resposta está aqui: **abrir o jogo real**.

## 1. O produto

O Aeonian é um **ponto de entrada de mods (*mod entry point*) para o
Diablo II: Lord of Destruction 1.14d original**, instalado e pago pelo
usuário. Ele **não reimplementa** o jogo e **não copia** assets. O usuário
continua rodando o `Game.exe` da Blizzard, e o Aeonian — um conjunto de DLLs
GPLv3 — roda **dentro** desse processo e abre o jogo a quem quiser modificá-lo:

| Camada | O que o modder faz | Estado |
|---|---|---|
| **1. Dados e assets** | Substitui qualquer arquivo de `data\` (UI/HUD em DC6, fontes, paletas, tabelas `.txt`/`.bin`, strings, sons, mapas) com uma pasta de mod, sem tocar nos MPQs | ✅ funcionando |
| **2. Plugins nativos** | Escreve uma DLL que recebe eventos do jogo (laço de mensagens, mensagens da janela/entrada, encerramento) | ✅ funcionando |
| **2b. Desenho sobre o jogo** | Escreve um HUD/menu de mod com GDI (`on_present`), nos renderizadores GDI e **D2GL/OpenGL**, sem alterar o quadro do jogo | ✅ funcionando (DirectDraw: código sem teste) |
| **3. Hooks no código do jogo** | Desvia/estende funções internas do `Game.exe` por endereço, com verificação de bytes | 🟡 mecanismo pronto; símbolos (endereços) entram sob demanda via engenharia reversa |
| **4. Sistemas novos** | Novas classes, menus, telas, mecânicas | ⬜ roadmap; depende da camada 3 |

A camada 1 é suficiente para boa parte de "modificar HUD/UI/UX" por arte e
dados. As camadas 3 e 4 é que abrem a **lógica** do jogo e dependem de
mapear o `Game.exe` 1.14d (ver [`ROADMAP.md`](ROADMAP.md)).

## 2. O que o projeto NÃO é

- **Não é uma reimplementação do motor.** Houve uma tentativa (um motor
  próprio que relia os assets e reconstruía mapa/combate/HUD); ela produziu
  um jogo parecido, mas quebrado (paredes invisíveis, movimento errado, UI
  desalinhada) e foi **descontinuada** e **apagada** do repositório. Não a
  retome.
- Não distribui código, binário ou asset da Blizzard.
- Não faz fork de OpenDiablo2/AbyssEngine nem copia código descompilado.
- Não é um *cheat*/bot, e não tem multiplayer/Battle.net como meta (o
  alvo é o jogo de um jogador e a base moddável).

## 3. Modelo de distribuição

| Item | No GitHub? | Observação |
|---|---|---|
| Código do Aeonian (C++20, GPLv3): DLLs, instalador, testes | ✅ | Original do projeto |
| Specs de engenharia reversa e bancos de endereços (`aeonian/addresses/`) | ✅ | Fatos de interoperabilidade; sem código da Blizzard |
| Exemplos de mod e de plugin | ✅ | Sem assets da Blizzard |
| `Game.exe`, DLLs e MPQs do jogo | ❌ | Do usuário |
| `.txt`/`.dc6`/`.dcc`/`.ds1`/`.dt1`/`.wav` originais | ❌ | Do usuário |
| Código descompilado / pseudocódigo do Ghidra | ❌ | Proibido pelo protocolo |

**Requisito para usar:** possuir o Diablo II + Lord of Destruction 1.14d
legalmente adquirido. Cada pessoa instala o Aeonian **na própria instalação**
(`tools/ae_install.py`); nada do jogo passa por este repositório.

## 4. Princípios técnicos

1. **Não destrutivo.** O `Game.exe` nunca é alterado no disco. O ponto de
   entrada é um proxy do `ijl11.dll` (a biblioteca JPEG que o jogo já
   importa); o original é preservado como `ijl11_real.dll` e restaurado byte
   a byte na desinstalação.
2. **Falha segura.** Em um `Game.exe` desconhecido o motor se desativa e o
   jogo roda vanilla. `AEONIAN_DISABLE=1` desliga tudo sem desinstalar.
3. **Verifique antes de escrever.** Todo hook/patch por endereço confere os
   bytes esperados *antes* de tocar na memória; numa build errada ele é
   recusado em vez de corromper o jogo.
4. **Dados, não código.** Endereços vivem em arquivos de dados
   (`aeonian/addresses/*.ini`), não espalhados em C++; novas builds e novos
   símbolos entram por contribuição de dados + spec.
5. **Fatos, não cópias.** A engenharia reversa produz *fatos de
   interoperabilidade* (endereço, assinatura, convenção de chamada, layout
   de struct) documentados em `docs/specs/`; nunca tradução linha a linha do
   descompilado. Ver [`CLEANROOM_PROTOCOL.md`](CLEANROOM_PROTOCOL.md).
6. **Sem rede, sem telemetria, sem persistência escondida.**

## 5. Portabilidade (leia com atenção)

O Aeonian é código **x86 de 32 bits (PE/Windows)** porque roda dentro do
`Game.exe`, que também é. Isso significa:

- ✅ **Windows** (alvo nativo) e **Linux/macOS via Wine** (validado em
  Wine 11/Linux; ver [`HANDOFF.md`](HANDOFF.md)).
- 🟡 **Nintendo Switch** é viável **pelo mesmo caminho do projeto-irmão
  "DIABLO 2 LOD SWITCH PORT"**: o `Game.exe` real sobre uma camada de
  compatibilidade Windows (AUTORUN/Wine-NX) com uma DLL injetada. O
  `aeonian.dll` foi feito para ser carregável por qualquer injetor (exporta
  `AeonianBootstrap`) e evita páginas RWX (trampolins RW→RX), por causa do
  W^X do Horizon. **Ainda não foi testado nessa camada.**
- ❌ **Plataformas sem camada x86/Windows** (um port "nativo" ARM sem Wine)
  **não são possíveis por este caminho**: exigiriam outro projeto
  (recompilação estática do binário ou reimplementação — a rota
  descontinuada). Isso é uma consequência de usar o jogo real, e não um
  defeito a corrigir.

## 5b. Alto desempenho gráfico (D2GL) — requisito obrigatório

O desempenho em monitores modernos (alta taxa de atualização, 1440p/4K,
ultrawide) vem do **[D2GL](modding/D2GL.md)** (OpenGL, GPL-3.0), que o Aeonian
**integra** (instalação, `-3dfx`, `skip_intro`, overlay de mods sobre o D2GL) em
vez de reescrever um renderizador. O que **não** está provado ainda: a imagem
completa sob o D2GL no nosso ambiente de teste e medições de FPS; ver
[`modding/D2GL.md`](modding/D2GL.md).

## 6. Critérios de sucesso (por marco)

1. **M1 — Ponto de entrada (feito):** instala sem alterar o `Game.exe`;
   mods de arquivos e plugins funcionam no jogo real; falha segura;
   desinstalação limpa; suíte de integração no jogo real.
2. **M2 — Superfície de hooks:** hook de apresentação/desenho e acesso ao
   estado do jogo (jogador, painéis, resolução) via símbolos verificados.
3. **M3 — UI/HUD de código:** mover/estender painéis e elementos de HUD por
   plugin, não só trocar arte.
4. **M4 — Sistemas novos:** classes, menus, telas.
   **Requisitos obrigatórios do client** (mapa com entrada/saída e inimigos,
   planejador de builds, compatibilidade com Project Diablo 2 e Median XL):
   [`ROADMAP.md`](ROADMAP.md#requisitos-obrigatórios-do-client-após-a-suíte).
5. **M5 — Portabilidade:** validação em Windows nativo e em camada tipo
   Wine-NX (Switch).

## 7. Nota legal

Este documento descreve o modelo técnico do projeto; **não é aconselhamento
jurídico** e não afirma o que a Blizzard permite ou não. O desenho
(nenhum arquivo da Blizzard no repositório, `Game.exe` intocado no disco,
uso somente sobre a cópia do próprio usuário) segue o modelo das
ferramentas de mod existentes para o Diablo II. Cada contribuidor e usuário
é responsável por respeitar os termos do jogo e as leis da sua jurisdição.
