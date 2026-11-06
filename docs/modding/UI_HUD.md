# Guia de Modding — Customização de UI e HUD

O Aeonian permite inspecionar, controlar e customizar a interface do usuário (UI)
e o HUD do Diablo II 1.14d diretamente a partir de plugins nativos C/C++, sem
precisar editar o executável em disco.

## 1. Controle Programático de Painéis (`SetUiState`)

O cliente gerencia a abertura e o fechamento de painéis através do vetor `UiState`
em `[0x7A27C0]`. Em vez de emular cliques de mouse ou pressionamentos de tecla,
os plugins podem abrir, fechar ou alternar painéis chamando `D2Client.SetUiState`:

```cpp
using SetUiStateFn = int (__fastcall*)(int ui_id, int action, int a3);

auto set_ui = reinterpret_cast<SetUiStateFn>(
    host->resolve_symbol(host, "D2Client.SetUiState"));

// Ações disponíveis:
// 0 = Abrir painel
// 1 = Fechar painel
// 2 = Alternar (toggle)

set_ui(0x01, 2, 0); // Alterna o Inventário
set_ui(0x02, 2, 0); // Alterna a Folha de Personagem (Stats)
set_ui(0x04, 2, 0); // Alterna a Árvore de Habilidades
set_ui(0x0F, 2, 0); // Alterna o Livro de Missões (Quests)
```

### Principais IDs de Interface:
- `0x01`: Inventário
- `0x02`: Atributos do Personagem
- `0x03`: Menu de Seleção Rápida de Habilidades
- `0x04`: Árvore de Habilidades (Skill Tree)
- `0x05`: Chat
- `0x08`: Menu de Interação com NPC
- `0x09`: Menu Esc
- `0x0A`: Automap
- `0x0F`: Missões (Quest Log)
- `0x11`: Seleção de Waypoints
- `0x13`: Painel do Mercenário
- `0x19`: Baú da Cidade (Stash)
- `0x1A`: Cubo Horádrico

## 2. Leitura de Estados de Painel (`GetUiState`)

Para verificar se um painel específico está aberto na tela:

```cpp
using GetUiStateFn = int (__fastcall*)(int ui_id);

auto get_ui = reinterpret_cast<GetUiStateFn>(
    host->resolve_symbol(host, "D2Client.GetUiState"));

if (get_ui(0x01)) {
    // Inventário está visível
}
```

## 3. Posicionamento de Elementos e Resolução

Todos os painéis e elementos do HUD respeitam os globais de deslocamento de tela:
- `[0x7A2858]` (`ScreenOffsetLeftX`): deslocamento horizontal (`0` em 640×480, `80` em 800×600).
- `[0x7A285C]` (`ScreenOffsetTopY`): deslocamento vertical (`0` em 640×480, `-60` em 800×600).
- `[0x71146C]` e `[0x711470]`: largura e altura do quadro do jogo.

Ao desenhar elementos customizados pelo callback `on_present`, consulte esses
deslocamentos para alinhar seus widgets perfeitamente com os painéis nativos.

## 4. Exemplo Completo no Repositório

O plugin de demonstração [`aeonian/examples/hud-demo/hud.cpp`](../aeonian/examples/hud-demo/hud.cpp)
demonstra a integração completa:
- Inspeciona o estado de todos os painéis e exibe no overlay com a tecla `F9`;
- Liga as teclas `F6`, `F7` e `F8` ao controle nativo de Inventário, Personagem e Skills;
- Renderiza caixas e textos translúcidos sobre a superfície do jogo com GDI e D2GL/OpenGL.
