# Mod planejador de builds (`build-planner`): estado do personagem, planos JSON e pontos

Implementação dos itens **M2.2 (Personagem)** e **C3 (Planejador de builds)** do
[`ROADMAP.md`](../ROADMAP.md). Roda dentro do `Game.exe` 1.14d original:

- **Lê o estado completo do personagem** diretamente da memória do cliente
  (`Unit+0x04` classe, `PlayerData+0x00` nome, `StatListEx+0x24` stats base,
  `StatListEx+0x48` stats totais/efetivos, `Unit+0xA8` / `SkillList+0x04`
  habilidades aprendidas, `DataTables+0xB98` `SkillsTxt` e `DataTables+0xBC4`
  `CharStatsTxt`) e **confere em partida** contra as funções do jogo
  (`D2Common.GetUnitBaseStat`, `D2Common.GetUnitStat`,
  `D2Common.GetBaseSkillLevelById`, `D2Common.CheckSkillPrereqs`).
- **Salva e carrega planos de build versionados** (`builds/*.json`, versão `1`),
  podendo capturar o estado atual do personagem ao apertar **F8** (ou na entrada
  da partida com `AEONIAN_BUILD_CAPTURE_ON_ENTER=1`).
- **Aplica planos em personagens no jogo real** (quando `auto_apply = 1`, apenas
  em partidas de um jogador `[0x7A0610] <= 1`): distribui pontos livres de
  atributo (`statpts`, id `4`) e de habilidade (`newskills`, id `5`) enviando os
  mesmos pacotes de 3 bytes da interface do cliente via
  `D2Client.SendPacket_ByteWord` (`0x3A` para atributos, `0x3B` para
  habilidades), respeitando o intervalo mínimo de 220 ms do filtro de pacotes
  idênticos (`0x478350`). **Nunca edita arquivos `.d2s`**.
- **F10** liga e desliga o painel de resumo no canto superior esquerdo.

## Instalar

```
<jogo>\aeonian\mods\build-planner\mod.ini                (aeonian/examples/build-planner/mod.ini)
<jogo>\aeonian\mods\build-planner\build_planner.dll      (build/win32/build_planner.dll)
<jogo>\aeonian\mods\build-planner\build-planner.ini      (aeonian/examples/build-planner/build-planner.ini)
<jogo>\aeonian\mods\build-planner\builds\*.json          (aeonian/examples/build-planner/builds/)
```

## Configuração (`build-planner.ini`)

| Chave | Padrão | Efeito |
|---|---|---|
| `[planner] enabled` | `1` | mostra o painel de estado e próxima decisão no canto superior esquerdo |
| `[planner] auto_apply` | `0` | quando `1`, distribui automaticamente pontos livres conforme o plano (somente em um jogador) |
| `[planner] toggle_key` | `f10` | liga/desliga o painel |
| `[planner] capture_key` | `f8` | salva a build atual do personagem em `builds\<nome>.json` |
| `[planner] plan_file` | `builds\sorceress-orb-hydra.json` | caminho relativo ao diretório do mod para o plano ativo |
| `[planner] step_delay_ms` | `250` | intervalo mínimo entre envios de pontos (mínimo `220` ms por causa do filtro de 200 ms em `0x478350`) |

## Formato do plano (`builds/*.json`)

```json
{
  "version": 1,
  "name": "Sorceress Hydra/Orb",
  "class": "sorceress",
  "auto_prereqs": true,
  "attributes": {
    "strength": 156,
    "dexterity": 35,
    "vitality": 300,
    "energy": 50,
    "overflow": "vitality"
  },
  "skills": [
    { "id": 37, "level": 1 },
    { "id": 42, "level": 1 },
    { "id": 54, "level": 1 },
    { "id": 62, "level": 20 },
    { "id": 64, "level": 20 }
  ]
}
```

- **`class`**: `"amazon"`, `"sorceress"`, `"necromancer"`, `"paladin"`,
  `"barbarian"`, `"druid"`, `"assassin"` (ou `0`…`6`). Planos de outra classe não
  são aplicados ao personagem atual.
- **`attributes`**: metas de atributos **base** (`StatListEx+0x24`, sem bônus de
  equipamento), verificadas na ordem Força → Destreza → Vitalidade → Energia,
  seguidas por `overflow` (`"strength"`, `"dexterity"`, `"vitality"`, `"energy"`
  ou `"none"`).
- **`skills`**: lista ordenada de passos `{ "id": <skill_id>, "level": <alvo> }`.
  Quando `auto_prereqs` é `true`, se a habilidade da vez exigir pré-requisitos
  (`SkillsTxt+0x17E..+0x182`) ainda em nível base `0`, o planejador investe `1`
  ponto em cada pré-requisito elegível antes.

## Fontes e testes

- Specs: [`docs/specs/game-exe-1.14d-character.md`](../specs/game-exe-1.14d-character.md) e [`docs/specs/game-exe-1.14d-datatables.md`](../specs/game-exe-1.14d-datatables.md)
- Símbolos: [`aeonian/addresses/1.14d.ini`](../../aeonian/addresses/1.14d.ini) (17 símbolos verificados por bytes)
- Teste de host: `aeonian/examples/build-planner/build_plan_test.cpp` (`ae_build_planner_test`)
- Teste em jogo real: fases E2 e E3 de `aeonian/tests/integration/run_wine_smoke.sh` (incluindo modificação de `charstats.txt` compilada por `-txt` e lida de `DataTables`)

