# D2GL — alto desempenho gráfico em monitores modernos

O Aeonian **não renderiza o jogo**: quem dá a qualidade e o desempenho em
monitores modernos é o **[D2GL](https://github.com/bayaraa/d2gl)** (wrapper
Glide/DDraw → OpenGL, GPL-3.0), que o Aeonian **integra**: instala, liga,
mantém compatível com mods/plugins e desenha por cima dele.

O que o D2GL traz (segundo o projeto upstream): OpenGL 3.3–4.6; janela ou tela
cheia sem trocar o modo de vídeo do monitor; V-Sync / limite de FPS /
sincronização com a taxa de atualização; *motion prediction* (do D2DX, para
animação fluida acima de 25 FPS); texto HD; shaders de *upscale*, FXAA, nitidez,
bloom; widescreen/estiramento; `Alt+Enter` alterna janela/tela cheia. Suporta a
**1.14d**, Windows 7–11 e Linux/macOS via Wine.

## Licença e distribuição

O D2GL é **GPL-3.0**, compatível com o Aeonian (GPLv3). Mesmo assim este
repositório **não embute** o binário: você instala o D2GL por uma pasta que o
contenha (a sua cópia, ou o release oficial `D2GL.v1.3.3.zip` do GitHub do
autor). O instalador só copia; não baixa nada.

Testado: **D2GL v1.3.3** (`glide3x.dll` + `d2gl.mpq`). Cabe a você
cumprir a GPL-3.0 ao redistribuir o D2GL (o código-fonte está no repositório
do autor).

## Instalar

```bash
python3 tools/ae_install.py install --game-dir "/caminho/Diablo II" \
        --build-dir build/win32 --d2gl-dir "/pasta/com/o/D2GL"
```

A pasta do D2GL precisa ter `d2gl.mpq` e `glide3x.dll` e/ou `ddraw.dll` (e,
opcionalmente, `d2gl.ini`). O instalador:

- confirma que a DLL é do D2GL (procura `d2gl.mpq` dentro dela);
- guarda como `*.aeonian-orig` qualquer `glide3x.dll`/`ddraw.dll`/`d2gl.mpq`
  que **já existam** na pasta do jogo e os restaura na desinstalação;
- **nunca sobrescreve o seu `d2gl.ini`** (é a sua configuração) e a
  desinstalação **também não o apaga**, mesmo que o instalador o tenha copiado.

## O que o Aeonian faz sozinho (`[video]` em `aeonian.ini`)

| Chave | Padrão | Efeito |
|---|---|---|
| `backend` | `auto` | `auto`: usa o D2GL se estiver na pasta do jogo; `d2gl`: exige (avisa se faltar); `software`: renderizador original (GDI/DirectDraw) |
| `d2gl_mode` | `auto` | qual wrapper: `glide` (`glide3x.dll`; o caminho validado), `ddraw` (`ddraw.dll`), ou `auto` = glide se existir |
| `overlay_max_width` | `1920` | largura máxima (px) do canvas dos plugins; esticado sobre a janela (4K não custa 4× de CPU) |
| `overlay_hz` | `60` | taxa de redesenho do canvas dos plugins; entre redesenhos só se desenha o quadrilátero (barato em 144/240 Hz) |

No modo `glide` o Aeonian **acrescenta `-3dfx`** e **remove `-w`** da linha de
comando (o wrapper Glide do D2GL é incompatível com `-w`), mesmo se `-w` vier de
um atalho, do `extra_args` ou de um mod. `[game] skip_intro` (padrão `true`)
pula os 2 vídeos de abertura da Blizzard nos primeiros 30 s.

## Configuração recomendada para monitores de alta taxa/resolução

No **seu `d2gl.ini`** (use `Ctrl+O` no jogo, ou edite):

| Objetivo | Chaves |
|---|---|
| Sem trocar o modo do monitor | `fullscreen=true` (janela de tela cheia sem bordas) |
| Fluidez em 120/144/240 Hz | `vsync=true` + `motion_prediction=true` |
| Sem V-Sync, FPS limitado | `vsync=false`, `foreground_fps=true`, `foreground_fps_value=<taxa do monitor>` |
| Latência baixa | `frame_latency=1` |
| Mais barato na GPU | `fxaa=false`, `bloom=false`, `sharpen=false`, `shader_preset=bilinear.slangp` |
| Widescreen | `stretched_horizontal=true` |
| Se o seu driver for antigo | `gl_ver_major=3`, `gl_ver_minor=3` |

O D2GL já se declara *DPI aware* (`SetProcessDPIAware`), então o Windows não
reescala a janela em monitores de alta densidade.

## Pegadinhas conhecidas

- **Registro `Render`.** Em `HKCU\Software\Blizzard Entertainment\Diablo II\VideoConfig`,
  `Render=1` é o modo GDI. Com `Render=1` + D2GL a janela abre em tamanho
  estranho e a **abertura da Blizzard toca pequena no canto** (aparentemente o
  vídeo não passa pelo framebuffer OpenGL do D2GL: a leitura do framebuffer não o via). Use um valor ≠ 1 (2–5 se comportaram
  igual nos testes) ou rode o `D2VidTst.exe` e escolha Glide. O `skip_intro` do
  Aeonian elimina a abertura nesse caso.
- **Wine/Proton:** o wrapper **DDraw** do D2GL (`ddraw.dll`) **travou na
  inicialização** nos nossos testes sob Wine 11, **mesmo sem o Aeonian**; por
  isso o padrão é o Glide. Para usar o `ddraw.dll` no Wine é preciso
  `WINEDLLOVERRIDES=ddraw=n,b`.
- O `d2gl.ini` é **reescrito pelo próprio D2GL** ao rodar.

## Overlay (HUD de mods) sobre o D2GL

O evento `on_present` funciona no modo D2GL: o Aeonian intercepta o
`SwapBuffers` dentro do `glide3x.dll`, desenha a tela dos plugins como textura
OpenGL e **salva/restaura todo o estado GL** que toca (programa, VAO, texturas,
blend, FBOs, viewport, sRGB...). **Atenção:** nesse modo `on_present` roda na
**thread de renderização do D2GL**, não na thread principal do jogo; proteja o
estado compartilhado com `on_tick`/`on_window_message` (um `bool`/inteiro simples
já basta, como no `hud-demo`). O canvas tem o tamanho da janela (limitado por
`overlay_max_width`); o D2GL pode usar uma janela de qualquer tamanho.

## O que está e o que NÃO está validado

Validado no jogo real sob Wine 11 (Linux), D2GL v1.3.3, modo Glide: instalação e
restauração; `-3dfx` injetado e `-w` removido; `SwapBuffers` interceptado; overlay
composto no framebuffer do D2GL e capturado; vídeos de abertura pulados.

**Não validado:** (1) a **imagem completa** do jogo sob o D2GL neste ambiente de
teste — o D2GL desenhou o texto HD e o cursor do menu mas **não o fundo e a arte
dos botões**, e o mesmo ocorre **sem o Aeonian** (controle), então não é uma
regressão do Aeonian, mas **confira no seu hardware**; (2) o modo `ddraw`; (3)
**medição de desempenho** em 144/240 Hz e 4K (o Aeonian só tem o custo do
overlay: ≤`overlay_hz` redesenhos/s do canvas + 1 quadrilátero por quadro);
(4) Windows nativo.
