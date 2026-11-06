# Spec — Arquitetura Multi-DLL: Base 1.13c, Project Diablo 2 e Median XL

> **Tipo:** análise de interoperabilidade e arquitetura (C4).
> Alimenta `aeonian/addresses/1.13c.ini` e o requisito **C4** de [`../ROADMAP.md`](../ROADMAP.md).
>
> **Evidência e método:** Arquivos binários da instalação 1.13c em
> `/mnt/STEAM/Diablo II LoD 1.13c -D2SE/` (SHA-256 do `Game.exe`:
> `74fe9c092a521f7710392548c82f81544e531107db2358617e83818874db40a2`).
> Análise de cabeçalhos PE32, ImageBase e tabelas de exportação por ordinais
> de `D2Client.dll`, `D2Common.dll`, `D2Game.dll`, `D2Win.dll`, `Fog.dll` e `Storm.dll`.

## 1. Diferença Estrutural: 1.14d vs 1.13c

No patch 1.14d (2016), a Blizzard fundiu todas as bibliotecas do jogo em um único
executável monolítico (`Game.exe` de ~3.6 MB). Todos os ponteiros de função e tabelas
residem no espaço do próprio `Game.exe`.

Na versão 1.13c (2010), utilizada como padrão absoluto pelos grandes mods
(**Project Diablo 2**, **Median XL Σ**, **PlugY**, **Eastern Sun**), a arquitetura
é dividida em múltiplos módulos PE32 (DLLs) carregados dinamicamente:

| Módulo | ImageBase Base | SizeOfImage | Função principal no ecossistema |
|---|---|---|---|
| `Game.exe` | `0x00400000` | `0x11000` | Launcher / inicializador CRT mínimo (~60 KB) |
| `D2Client.dll` | `0x6FAB0000` | `0x135000` | HUD, renderização do cliente, input, rede, UI |
| `D2Common.dll` | `0x6FD50000` | `0x0A9000` | Dados, colisão, inventário, física, tabelas |
| `D2Game.dll` | `0x6FC20000` | `0x122000` | Lógica de servidor / partida local, monstros, IA |
| `D2Win.dll` | `0x6F8E0000` | `0x024000` | Primitivas de janela, fontes, textos, importa `ijl11.dll` |
| `D2gfx.dll` | `0x6FA70000` | `0x013000` | Renderizador 2D / seleção de driver de vídeo |
| `Fog.dll` | `0x6FF50000` | `0x05C000` | Sistema de log, memória, asserts, SO |
| `Storm.dll` | `0x6FBF0000` | `0x060000` | Leitor de arquivos MPQ, I/O de disco, threads |

---

## 2. Ponto de Entrada Universal: Grafo de Carregamento PE

### 2.1 Proxy `ijl11.dll` no 1.13c
A cadeia de importação estática no 1.13c garante o carregamento automático do proxy
do Aeonian:
1. `Game.exe` importa estaticamente `D2Win.dll` (tabela de importação em `0x40a678`).
2. `D2Win.dll` importa estaticamente `ijl11.dll` (ordinais 2, 3 e 5 da Intel JPEG Library).
3. Ao resolver as importações no início do processo, o carregador do Windows mapeia
   nosso proxy `ijl11.dll` no diretório da aplicação antes de o código do jogo iniciar.
4. O `DllMain` do `ijl11.dll` carrega `aeonian/aeonian.dll` e dispara `AeonianBootstrap()`.
5. Em seguida, todas as chamadas legítimas da JPEG Library são repassadas a `ijl11_real.dll`.

### 2.2 Compatibilidade com Mods (Project Diablo 2 e Median XL)
- **Project Diablo 2 (PD2):** Inicia via `SteamPD2.exe` / `Game.exe`, carregando
  `ProjectDiablo.dll` através do ecossistema de DLLs do 1.13c. Como `D2Win.dll` permanece
  na cadeia de dependências, o proxy `ijl11.dll` é acionado perfeitamente.
- **Median XL Σ:** Utiliza o motor 1.13c com o supermódulo `D2Sigma.dll` (7.9 MB) e
  `P113c.dll`. O `D2Win.dll` da pasta do Median XL mantém o import de `ijl11.dll`,
  garantindo que o Aeonian intercepte e proteja o processo da mesma forma.

---

## 3. Arquitetura de Interceptação Multi-Módulo

### 3.1 O Problema do Monolítico vs Multi-Módulo
No 1.14d, apenas os imports de `Game.exe` precisavam de hooks de IAT. No 1.13c:
- `Game.exe` **não** importa `CreateFileA` ou `GetFileAttributesA`.
- As chamadas de leitura de disco e MPQ são executadas por `Storm.dll` e `Fog.dll`.
- As chamadas de renderização ocorrem em `D2Client.dll` e `D2gfx.dll`.
- Módulos de mods (`ProjectDiablo.dll`, `D2Sigma.dll`) fazem chamadas próprias de I/O.

### 3.2 Solução: Hooking em Duas Etapas
1. **Varredura no Bootstrap (`InstallHooks`):**
   - Intercepta `Game.exe`.
   - Localiza módulos já mapeados pelo PE loader (`Storm.dll`, `Fog.dll`, `D2Win.dll`,
     `D2gfx.dll`, `D2sound.dll`) via `GetModuleHandleA` e instala os hooks de IAT em cada um.
2. **Interceptação de Carregamento Tardio (`HookLoadLibraryA`):**
   - Quando o jogo ou um mod invoca `LoadLibraryA` para carregar `D2Client.dll`,
     `D2Common.dll`, `D2Game.dll`, `ProjectDiablo.dll`, `D2Sigma.dll` ou `glide3x.dll`,
     o retorno é interceptado e o módulo recém-carregado recebe imediatamente os hooks de IAT.

### 3.3 Tabela de APIs Interceptadas por Módulo
| Módulo Destino | API Interceptada | Finalidade no Aeonian |
|---|---|---|
| `Storm.dll`, `Fog.dll`, `D2Common.dll` | `KERNEL32!CreateFileA` | VFS de mods (`mods/<id>/data/...`) |
| `Storm.dll`, `Fog.dll`, `D2Common.dll` | `KERNEL32!GetFileAttributesA` | Detecção transparente de arquivos de mod |
| `Game.exe`, `D2Client.dll` | `KERNEL32!GetCommandLineA` | Injeção de `-direct -txt` e parâmetros de mod |
| Todos os módulos carregados | `KERNEL32!LoadLibraryA` | Propagação de hooks para novos módulos e D2GL |
| `D2Win.dll`, `D2gfx.dll`, `D2Client.dll` | `USER32!CreateWindowExA` / `RegisterClassA` | Captura da janela e tratamento de input |
| `D2gfx.dll`, `glide3x.dll` | `GDI32!SwapBuffers` / `StretchBlt` | Renderização do canvas de plugins (`on_present`) |

---

## 4. Resolução Multi-Módulo de Símbolos (`Runtime::ResolveSymbol`)

### 4.1 Notação de Símbolos
Para manter **100% de compatibilidade** entre o código dos plugins existentes
(`plugy-stash`, `map-overlay`, `build-planner`) em 1.14d e 1.13c, os símbolos mantêm
o padrão qualificado com prefixo:
`D2Client.GetPlayerUnit`
`D2Common.GetUnitStat`
`Storm.SFileOpenFile`
`Game.EntryPoint`

### 4.2 Lógica de Resolução por Build
```text
Se build == 1.14d:
    module_base = game_base_ (monolítico)
Se build == 1.13c:
    prefix = extrai_modulo_antes_do_ponto(symbol_name)
    se prefix == "Game" ou sem ponto:
        module_base = game_base_
    senão:
        module_base = GetModuleHandleA(prefix + ".dll")
        se module_base == null: retorne 0 (módulo ainda não carregado)
endereco = module_base + symbol.rva
conferir bytes de symbol.expect no endereco
```

---

## 5. Tabela de Interoperabilidade — Símbolos do 1.13c

Os seguintes símbolos e endereços RVA foram extraídos da instalação oficial 1.13c
e validados por ordinais exportados e assinaturas de cabeçalho:

| Símbolo | Módulo | Ordinal | RVA | Convenção | Assinatura (Bytes) | Descrição |
|---|---|---|---|---|---|---|
| `Game.EntryPoint` | `Game.exe` | N/A | `0x00122E` | `cdecl` | `6a 60 68 30 92 40 00 e8 de 1c 00 00` | Ponto de entrada CRT do executável |
| `D2Client.GetPlayerUnit` | `D2Client.dll` | 10001 | `0x062E20` | `fastcall` | `8b 0d 24 bc bb 6f 85 c9 74 05` | Ponteiro da unidade do jogador local |
| `D2Client.InitStashUi` | `D2Client.dll` | N/A | `0x01B840` | `stdcall` | `83 ec 14 53 55 56 57 8b f9` | Inicializador da interface do baú |
| `D2Client.CloseStashUi` | `D2Client.dll` | N/A | `0x01B7F0` | `stdcall` | `56 8b f1 8b 46 04 85 c0` | Encerramento da interface do baú |
| `D2Client.DrawStashPanel` | `D2Client.dll` | N/A | `0x01B5A0` | `fastcall` | `55 8b ec 83 ec 1c 53 56 57` | Desenho do painel do baú |
| `D2Common.GetUnitBaseStat` | `D2Common.dll` | 10519 | `0x0245D0` | `stdcall` | `56 8b 74 24 08 85 f6 74 37` | Leitura de atributo base de unidade |
| `D2Common.GetUnitStat` | `D2Common.dll` | 10520 | `0x0346F0` | `stdcall` | `56 8b 74 24 08 85 f6 74 38` | Leitura de atributo efetivo com bônus |
| `D2Common.GetFirstInventoryItem` | `D2Common.dll` | 10249 | `0x05A810` | `stdcall` | `8b 44 24 04 85 c0 74 06 8b 40 0c` | Primeiro item da lista do inventário |
| `D2Common.GetNextInventoryItem` | `D2Common.dll` | 10304 | `0x04E2B0` | `stdcall` | `8b 44 24 04 85 c0 74 06 8b 40 00` | Próximo nó da lista de itens |
| `D2Common.RemoveItemFromInventory` | `D2Common.dll` | 10250 | `0x034AE0` | `stdcall` | `55 8b ec 53 8b 5d 08 56 57` | Desvincula item da lista de inventário |
| `D2Common.GetSkillById` | `D2Common.dll` | 10852 | `0x068540` | `stdcall` | `8b 44 24 08 85 c0 74 20` | Registro da habilidade por ID |
| `D2Common.GetSkillLevel` | `D2Common.dll` | 10940 | `0x069680` | `stdcall` | `55 8b ec 83 ec 08 53 56` | Nível efetivo da habilidade do jogador |
| `Storm.SFileOpenFile` | `Storm.dll` | 268 | `0x01A090` | `stdcall` | `55 8b ec 83 ec 0c 53 56` | Abertura de arquivo no container MPQ |
| `Fog.Alloc` | `Fog.dll` | 10023 | `0x008F30` | `cdecl` | `8b 44 24 04 85 c0 74 05` | Alocador de memória do motor Fog |
| `Fog.Free` | `Fog.dll` | 10024 | `0x008F80` | `cdecl` | `8b 44 24 04 85 c0 74 05` | Liberador de memória do motor Fog |

---

## 6. Validação Cleanroom e Garantias

1. **Isolamento de Código:** Nenhuma instrução proprietária descompilada do jogo ou
   de mods foi incorporada ao código fonte do Aeonian. Todos os dados são fatos de
   interoperabilidade (endereços, ordinais e assinaturas de cabeçalho).
2. **Segurança de Execução:** Tentativas de resolução de símbolos em módulos não
   carregados ou cujas assinaturas de bytes não coincidam retornam `0`, impedindo
   falhas de proteção de memória ou crashes.
3. **Coexistência com Mods Injetores:** Como o Aeonian opera estritamente nos desvios
   de IAT e não sobrescreve os desvios inline próprios de `ProjectDiablo.dll` ou
   `D2Sigma.dll`, a compatibilidade mútua é preservada integralmente.
