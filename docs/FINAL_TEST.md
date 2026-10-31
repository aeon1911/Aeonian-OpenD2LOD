# Roteiro do teste final

Para você conferir, no **seu** hardware, o que eu só pude validar num prefixo Wine
de teste. Tudo é reversível e nada da Blizzard passa pelo repositório.

## O que já foi provado (por mim) e o que depende de você

| Já provado no jogo real (Wine 11, 1.14d) | Depende do seu teste |
|---|---|
| instalar/desinstalar sem alterar o `Game.exe`; falha segura | a imagem sob o D2GL no **seu** monitor e na **sua** GPU, e em tela cheia |
| mod de arte: botões recoloridos no menu (diff de 11 mil pixels) | **desempenho** (FPS) em 144/240 Hz e 4K |
| overlay/HUD sobre o jogo: GDI e D2GL/OpenGL | Windows nativo (não testei) |
| D2GL instalado, `-3dfx` automático, `-w` removido | instalação **oficial** da loja (testei numa instalação reconstruída) |
| vídeos de abertura pulados | — |
| imagem completa sob o D2GL **oficial v1.3.3**, no menu e em partida (Normal, em janela, Wine 11.20, RX 9070 XT) | — |

## 1. Prepare (recomendado: cópia-espelho, sem tocar no original)

```bash
tools/ae_mirror.sh "/caminho/do/Diablo II" /caminho/teste-d2        # symlinks nos MPQs
```

(Se preferir, instale direto no jogo: o instalador é reversível.)

## 2. Compile ou use o pacote

```bash
export AEONIAN_MINGW_ROOT=/caminho/do/llvm-mingw   # ou use o mingw-w64 do sistema
cmake -S aeonian -B build/win32 -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/toolchain-i686-mingw.cmake && cmake --build build/win32
```

(O zip `dist/aeonian-0.1.0-win32.zip` já traz as DLLs; extraia e use `tools/ae_install.py` de dentro.)

## 3. Instale com o D2GL

```bash
python3 tools/ae_install.py install --game-dir /caminho/teste-d2 \
        --build-dir build/win32 --d2gl-dir "/pasta/com/glide3x.dll e d2gl.mpq"
python3 tools/ae_install.py status --game-dir /caminho/teste-d2
```

O seu `d2gl.ini` **não** é sobrescrito. Para um teste de desempenho em monitor
moderno veja [`modding/D2GL.md`](modding/D2GL.md) (vsync, `motion_prediction`,
`fullscreen`). Para exibir FPS: `show_fps=true` no `d2gl.ini`.

## 4. Mod de arte e HUD de exemplo

```bash
# mod de arte (botões azuis), gerado a partir do SEU jogo (não redistribua)
python3 tools/make_art_demo.py --d2dir /caminho/teste-d2 \
        --extractor /caminho/ae_mpq_extract --out /caminho/teste-d2/aeonian/mods/art-demo
# HUD de exemplo (distintivo + painel F9)
mkdir -p /caminho/teste-d2/aeonian/mods/hud-demo
cp build/win32/hud.dll aeonian/examples/hud-demo/mod.ini /caminho/teste-d2/aeonian/mods/hud-demo/
```

## 5. Rode

- **Linux/Wine:** `cd /caminho/teste-d2 && wine Game.exe` (no seu prefixo; o D2GL pede
  um runtime VC++ — o seu `~/.wine` já tem). Se a janela abrir estranha ou a abertura
  tocar pequena no canto, defina `Render` ≠ 1 em
  `HKCU\Software\Blizzard Entertainment\Diablo II\VideoConfig`
  (`wine reg add 'HKCU\...\VideoConfig' /v Render /t REG_DWORD /d 3 /f`).
- **Windows:** execute o `Game.exe` da pasta.

## 6. O que observar (e relatar)

1. Aparece o **distintivo "AEONIAN F9"** no canto superior direito; **F9** abre o painel
   com os mods carregados, FPS e resolução.
2. Com `art-demo`: os botões do menu com bordas **azul-ciano** (gemas vermelhas).
3. **Sem** a abertura da Blizzard (`skip_intro`); para ver a abertura: `[game] skip_intro = false`.
4. Sob o D2GL: janela no tamanho/modo do `d2gl.ini`, sem abertura pequena no canto. **A cena
   (fundo, botões, jogo) aparece completa?** Se faltar, rode com `AEONIAN_DISABLE=1` para ver se
   é do D2GL; relate GPU/driver e anexe `d2gl.log` (rode o jogo com `-log`).
5. **Desempenho:** FPS parado no menu e andando na cidade/dungeon, com `vsync` ligado e
   desligado, no seu monitor (resolução/taxa).
6. `AEONIAN_DISABLE=1 wine Game.exe` → jogo **normal**, sem overlay.
7. Log do motor: `<jogo>/aeonian/aeonian.log` (anexe se algo falhar).

## 7. Desinstale

```bash
python3 tools/ae_install.py uninstall --game-dir /caminho/teste-d2    # restaura o ijl11.dll e devolve os arquivos do D2GL que existiam
```

## Limitações conhecidas

- `on_present` em **DirectDraw**: código sem teste; no D2GL, só o modo **Glide**.
- O wrapper **DDraw** do D2GL travou sob Wine 11 (sem o Aeonian também).
- Não há ainda: mapa com entrada/saída e inimigos, planejador de builds, compatibilidade
  com Project Diablo 2 / Median XL — ver [`ROADMAP.md`](ROADMAP.md).
