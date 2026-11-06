# Mod de Exemplo — PlugY Stash & Survival (`plugy-stash`)

O mod `plugy-stash` (`aeonian/examples/plugy-stash/`) traz as funcionalidades
consagradas do PlugY (The Survival Kit) para o `Game.exe` 1.14d através da API
nativa de plugins do Aeonian, com suporte moderno a D2GL/OpenGL e compatibilidade
completa de arquivos de save.

---

## 1. Funcionalidades

### 1.1 Baú Infinito e Compartilhado (Multi-Page & Shared Stash)
- **Modo Compartilhado (`.sss`):** Baú compartilhado entre todos os personagens,
  gravado em `save/_LOD_SharedStashSave.sss`.
- **Modo Pessoal (`.d2x`):** Páginas extras exclusivas do personagem atual,
  gravadas em `save/<NomePersonagem>.d2x`.
- **Compatibilidade:** Lê e grava no formato oficial do PlugY, permitindo usar
  baús existentes sem conversão ou risco de perda de itens.
- **Barra de Navegação Integrada (Overlay no Baú):**
  - `|<`: Primeira página
  - `<`: Página anterior
  - `Indicador`: Exibe o modo atual (`Comp.` ou `Pess.`) e `Página X/Y`
  - `>`: Próxima página
  - `>|`: Última página
  - `SHR / PER`: Alterna instantaneamente entre Baú Compartilhado e Pessoal
  - `+`: Cria uma nova página no baú ativo
- **Atalhos de Teclado:**
  - `PageUp` ou `Seta Esquerda`: Página anterior
  - `PageDown` ou `Seta Direita`: Próxima página

### 1.2 Respec Livre de Atributos (Stats Unallocation)
- Na **Folha de Personagem** (tecla `C` ou `A`):
  - Botões vermelhos `[-]` são exibidos ao lado de cada atributo (Força, Energia,
    Destreza, Vitalidade).
  - Clicar em `[-]` devolve o ponto do atributo de volta para os **Pontos Livres**
    (`statpts`), permitindo redistribuição imediata.
  - Mantém atributos mínimos básicos (10) para evitar quebrar requisitos do jogo.

---

## 2. Estrutura de Arquivos

```
aeonian/examples/plugy-stash/
  ├─ mod.ini                # Manifesto do mod (prioridade 25)
  ├─ plugy-stash.ini        # Configurações do baú e caminho de saves
  ├─ stash_storage.h        # Parser e serializador de arquivos .sss/.d2x
  ├─ stash_storage.cpp      # Implementação portável (independente de Win32)
  ├─ stash_storage_test.cpp # Testes unitários para execução no host
  └─ plugy_stash.cpp        # Plugin Win32 com hooks de UI, overlay e eventos
```

---

## 3. Configuração (`plugy-stash.ini`)

```ini
[stash]
enabled = 1
save_path = save
shared_stash = 1
personal_stash = 1
free_respec = 1
```

