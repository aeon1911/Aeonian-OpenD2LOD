# Versões do Diablo II — alvo do projeto

## Alvo canônico: **1.14d**

O projeto tem como alvo principal o **Diablo II: Lord of Destruction 1.14d**
(última atualização oficial da Blizzard). O usuário deve possuir uma
instalação legal dessa versão; o Aeonian é instalado **nessa** instalação e
roda dentro do `Game.exe` dela (ver [`SCOPE.md`](SCOPE.md)).

A **1.13c** deixa de ser o alvo canônico. Todo o material já produzido para
ela (specs, hashes, endereços, ordinais, testes) permanece no repositório
como **evidência histórica** e referência de comparação, mas **não é
automaticamente válido para 1.14d**.

## Por que 1.14d

- É a versão oficial mais recente distribuída pela Blizzard.
- Tem correções de compatibilidade com Windows moderno e modo janela.
- O ecossistema de modding ainda usa 1.13c, mas o alvo do produto é a versão
  que o usuário consegue instalar hoje.
- 1.13c continua útil como referência: muitas funções e formatos são
  estáveis, e o diff entre as duas versões reduz o trabalho de análise.

## O que precisa ser revalidado na 1.14d

| Item | Ação |
|---|---|
| Hashes dos binários (`Game.exe`, DLLs) | Recalcular e registrar |
| Tabela de exports/ordinais da Storm/Fog/etc. | Reextrair; comparar com 1.13c |
| Endereços/VA usados nas specs | Reanalisar no Ghidra 1.14d |
| Assinaturas e comportamento de SFile/SStr/SMem | Testar/confirmar; documentar deltas |
| Formatos de arquivo (MPQ, DC6, PAL, DS1, DT1, DCC) | Verificar se mudaram; provavelmente estáveis |
| Layout dos MPQs e nomes de arquivos | Inventariar na instalação 1.14d |
| `Game.exe`/launcher e fluxo de inicialização | Reanalisar |
| Tabelas (`data/global/excel`) | Comparar; registrar diferenças de conteúdo |

## Estratégia de compatibilidade

1. **Uma build = um banco de endereços.** `aeonian/addresses/<build>.ini`
   identifica a build (`timestamp` + `size_of_image` + `entry_rva`, `sha256`) e
   lista símbolos com `bytes` de verificação. Build desconhecida ⇒ motor
   inerte.
2. **Specs versionadas.** Cada spec declara a versão e o SHA-256 da evidência.
   Specs de 1.13c são histórico e nunca valem como contrato da 1.14d.
3. **Só a camada de código depende da build.** Os hooks de IAT (arquivos, linha
   de comando, janela, laço de mensagens) usam importações do sistema e não
   dependem de endereços internos; só os símbolos do banco são específicos.
4. **Nova build?** Gere SHA-256, imports/exports e a spec de evidência; crie o
   `.ini`; rode a suíte de integração; só então marque como suportada.

## Estado atual

- **1.14d ingerida e suportada:** `aeonian/addresses/1.14d.ini`. Inventário em
  [`versions/1.14d.md`](versions/1.14d.md); superfície de interceptação em
  [`specs/game-exe-1.14d-hooking-surface.md`](specs/game-exe-1.14d-hooking-surface.md).
- A 1.14d é **monolítica**: um único `Game.exe` 1.14.3.71 (PE32, ImageBase
  `0x400000`, ~3,5 MB) com os módulos linkados estaticamente.
- Validado numa instalação **reconstruída** (MPQs 1.14b + `Game.exe` 1.14d);
  instalação oficial completa: pendente (ver [`HANDOFF.md`](HANDOFF.md)).

## Política para agentes

- Toda spec nova deve começar com a versão e o hash da evidência.
- Não misture evidência 1.13c com contrato 1.14d sem marcar a diferença.
- Se a única evidência for 1.13c, escreva `TODO(1.14d)` e não apresente como
  confirmado.
- Nunca use offsets/VA de uma versão em outra sem reanálise.
