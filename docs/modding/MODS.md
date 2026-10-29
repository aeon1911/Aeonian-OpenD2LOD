# Guia de mods

Um **mod** é uma pasta em `<jogo>\aeonian\mods\<id>\` com um `mod.ini` e,
opcionalmente, arquivos que substituem os do jogo e/ou plugins (DLLs).

```
<jogo>\aeonian\mods\meu-hud\
    mod.ini                        ← manifesto (obrigatório)
    data\global\ui\panel\...dc6    ← substitui <jogo>\data\global\ui\panel\...dc6
    data\global\excel\skills.txt   ← idem (ver "Tabelas")
    meu_plugin.dll                 ← plugin nativo (opcional)
```

## Instalação do Aeonian no jogo

```bash
# 1) compile (ver README.md) -> build/win32/{aeonian.dll,ijl11.dll}
# 2) instale na SUA instalação do Diablo II LoD 1.14d
python3 tools/ae_install.py install --game-dir "/caminho/Diablo II" --build-dir build/win32
python3 tools/ae_install.py status  --game-dir "/caminho/Diablo II"
python3 tools/ae_install.py uninstall --game-dir "/caminho/Diablo II"   # restaura o original
```

O instalador confere o SHA-256 do `Game.exe`, renomeia o `ijl11.dll` original
para `ijl11_real.dll` e instala o proxy. Reinstalar **não** sobrescreve o seu
`aeonian.ini` nem os seus mods.

## `mod.ini`

```ini
[mod]
id       = meu-hud        ; [a-z0-9._-], até 64. Padrão: nome da pasta
name     = Meu HUD
version  = 1.0.0
author   = Fulano
priority = 100            ; maior vence quando dois mods têm o mesmo arquivo
plugin   = hud.dll        ; opcional; lista separada por vírgula; relativo à pasta do mod
args     = -txt           ; opcional; flags de linha de comando do Game.exe
builds   = 1.14d          ; opcional; builds suportadas (vazio = qualquer)
```

Regras: `plugin` precisa ser caminho relativo sem `..`; cada item de `args`
começa com `-`; ids duplicados: só um é usado e o outro vira erro no log; um mod inválido é
ignorado com o motivo no log — nunca derruba o jogo.

## Substituir arquivos (camada 1)

Tudo que o jogo lê de `data\` pode ser substituído colocando o arquivo **no
mesmo caminho relativo** dentro de `mods\<id>\data\`. Se mais de um mod tem o
arquivo, vence a maior `priority` (empate: ordem alfabética do `id`).

- A caixa das letras não importa (como no Windows); use `/` ou `\`.
- O Aeonian acrescenta `-direct` à linha de comando automaticamente quando
  algum mod tem uma pasta `data\` (`force_direct = auto`).
- Arquivos que nenhum mod fornece continuam vindo dos MPQs, como sempre.
- **Só leituras** são redirecionadas; saves/logs nunca.
- Arquivo **novo** (que não existia quando o jogo abriu) só vale ao reiniciar;
  editar o conteúdo de um arquivo já existente vale na próxima leitura.

### Descobrir o caminho do que você quer trocar

1. Em `aeonian\aeonian.ini` ponha `trace_files = 1` e `log_level = debug`.
2. Abra o jogo até a tela desejada e feche.
3. Em `aeonian\aeonian.log`, as linhas `trace: data\...` mostram cada arquivo
   que o jogo pediu e `vfs: data\... -> mod 'x'` os que vieram de mods.

### Obter o arquivo original para editar

Extraia do **seu** MPQ (nunca versione o que extrair):

```bash
# ferramenta do projeto: cmake -DAEONIAN_BUILD_TOOLS=ON (baixa o StormLib)
ae_mpq_extract "<jogo>/d2data.mpq" extract ./extraido 'data\global\ui\FrontEnd\WideButtonBlank'
# edite com sua ferramenta de DC6 e coloque em mods/<id>/data/global/ui/FrontEnd/
```

### Tabelas (`.txt`/`.bin`)

O jogo lê tabelas de `data\global\excel\`; elas também passam pelo VFS, então
um `.bin`/`.txt` no mesmo caminho dentro do seu mod é visto pelo jogo. A flag
`-txt` existe na 1.14d (ver a spec de superfície) e, segundo a prática da
comunidade, faz o jogo ler os `.txt` em vez dos `.bin` — use `args = -txt` no
`mod.ini`. **Esse comportamento ainda não foi validado por este projeto**
(só o redirecionamento de arquivos foi). Veja `DATA_TABLES.md`.

## Configuração (`aeonian\aeonian.ini`)

| Chave | Padrão | Efeito |
|---|---|---|
| `[core] log_level` | `info` | `debug`/`info`/`warn`/`error` |
| `[core] force_direct` | `auto` | `auto` (com mods que têm `data\`), `on`, `off` |
| `[core] allow_unknown_build` | `0` | `1` força os hooks de arquivo/evento numa build não reconhecida |
| `[core] trace_files` | `0` | registra cada `data\...` pedido (nível debug) |
| `[core] extra_args` | — | flags extras do `Game.exe` (ex.: `-w -skiptobnet`) |
| `[mods] disabled` | — | ids a ignorar sem apagar a pasta |
| `[video] backend` | `auto` | `auto` (usa o D2GL se instalado), `d2gl`, `software` — ver [`D2GL.md`](D2GL.md) |
| `[video] d2gl_mode` | `auto` | `glide` (padrão do auto), `ddraw` |
| `[video] overlay_max_width`, `overlay_hz` | `1920`, `60` | custo do overlay dos plugins no modo D2GL |
| `[game] skip_intro` | `true` | pula os 2 vídeos de abertura (nos primeiros `skip_intro_window` s, padrão 30); `false` não pula; um número = quantos |

Variável de ambiente `AEONIAN_DISABLE=1`: o proxy não carrega o motor (jogo
vanilla, sem desinstalar).

## Diagnóstico

O log fica em `aeonian\aeonian.log` (recriado a cada execução): build
reconhecida, hooks, mods e plugins carregados, cada redirecionamento, falhas
de abertura de override e as primeiras exceções de 1ª chance do processo.

## Plugins

Veja [`PLUGIN_API.md`](PLUGIN_API.md) e `aeonian/examples/hello-plugin/`.
