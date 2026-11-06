# Spec — Arquitetura Multi-DLL: Base 1.13c, Project Diablo 2 e Median XL

> **Tipo:** análise de interoperabilidade e arquitetura (C4).
> Alimenta `aeonian/addresses/1.13c.ini` e o requisito **C4** de [`../ROADMAP.md`](../ROADMAP.md).
>
> **Evidência e método:** Arquivos binários da instalação 1.13c em
> `/mnt/STEAM/Diablo II LoD 1.13c -D2SE/` (SHA-256 do `Game.exe`:
> `74fe9c092a521f7710392548c82f81544e531107db2358617e83818874db40a2`).
> Análise de cabeçalhos PE32, ImageBase e tabelas de exportação por ordinais
> de `D2Client.dll`, `D2Common.dll`, `D2Game.dll`, `Fog.dll` e `Storm.dll`.

## 1. Diferença Estrutural: 1.14d vs 1.13c

No patch 1.14d (2016), a Blizzard fundiu todas as DLLs de jogo em um único
executável monolítico (`Game.exe` de ~3.6 MB).

Na versão 1.13c (2010), utilizada pela quase totalidade dos grandes mods
(Project Diablo 2, Median XL Σ, PlugY, Eastern Sun), a lógica do jogo reside
em múltiplas DLLs carregadas dinamicamente:

| Módulo | ImageBase Base | SizeOfImage | Função principal |
|---|---|---|---|
| `Game.exe` | `0x00400000` | `0x11000` | Launcher / inicializador mínimo |
| `D2Client.dll` | `0x6FAB0000` | `0x135000` | HUD, renderização do cliente, input, rede |
| `D2Common.dll` | `0x6FD50000` | `0x0A9000` | Dados, colisão, física de projéteis, tabelas |
| `D2Game.dll` | `0x6FC20000` | `0x122000` | Lógica de servidor / partida local, monstros, IA |
| `D2Win.dll` | `0x6F8E0000` | `0x024000` | Primitivas de janela, fontes, textos |
| `D2gfx.dll` | `0x6FA70000` | `0x013000` | Renderizador 2D / seleção de driver de vídeo |
| `Fog.dll` | `0x6FF50000` | `0x05C000` | Sistema de log, memória, asserts, SO |
| `Storm.dll` | `0x6FBF0000` | `0x060000` | Leitor de arquivos MPQ, threads, sincronização |

## 2. Ponto de Entrada e Interceptação no 1.13c

### 2.1 Proxy `ijl11.dll` no 1.13c
O `Game.exe` 1.13c importa diretamente `ijl11.dll`. Portanto, o mecanismo
de proxy `ijl11.dll` do Aeonian funciona identicamente na 1.13c:
1. `Game.exe` carrega `ijl11.dll` (proxy do Aeonian);
2. O proxy carrega `aeonian/aeonian.dll` e chama `AeonianBootstrap()`;
3. O proxy encaminha todas as chamadas legítimas da Intel JPEG Library para
   `ijl11_real.dll`.

### 2.2 Injeção de Seção `.aeon` (Alternativa para Switch / Wine-NX)
Em plataformas como o Nintendo Switch (camada Wine-NX / D2Autorun), o utilitário
`tools/ae_inject_import.py` injeta um descritor de importação para `aeonian.dll`
(`AeonianBootstrap`), garantindo inicialização imediata pelo carregador PE do Windows
sem depender de proxies de bibliotecas do sistema.

### 2.3 Interceptação Multi-Módulo (VFS de Mods)
No 1.14d, apenas os imports de `Game.exe` precisam ser hookados.
No 1.13c:
- `Storm.dll` e `Fog.dll` realizam as leituras de disco (`CreateFileA`, `ReadFile`).
- O hook de `LoadLibraryA` intercepta o carregamento de cada DLL do jogo e
  aplica os desvios de IAT aos módulos carregados, assegurando que o VFS de mods
  funcione de forma transparente em todas as DLLs.

## 3. Banco de Símbolos por Ordinais

Ao contrário do 1.14d monolítico (onde nomes de exportação foram removidos),
as DLLs do 1.13c exportam funções por **ordinais padronizados**:
- `D2Client.dll`: Ordinal `10001` = `GetPlayerUnit` (RVA `0x62E20`).
- `D2Common.dll`:
  - Ordinal `10519` = `GetUnitBaseStat` (RVA `0x245D0`).
  - Ordinal `10520` = `GetUnitStat` (RVA `0x346F0`).
  - Ordinal `10852` = `GetSkillById`.
  - Ordinal `10940` = `GetSkillLevel`.

