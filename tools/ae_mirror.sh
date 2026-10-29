#!/usr/bin/env bash
# Aeonian-OpenD2LOD - GPLv3
#
# Cria uma COPIA-ESPELHO leve de uma instalacao do Diablo II para testar o
# Aeonian sem tocar na instalacao original: os arquivos grandes (MPQs...) viram
# symlinks; Game.exe e as DLLs pequenas sao copiados (o instalador altera so o
# ijl11.dll e acrescenta aeonian/ e, com --d2gl-dir, os arquivos do D2GL).
#
# Uso: ae_mirror.sh <pasta do jogo original> <pasta de destino>
set -euo pipefail
src="${1:?pasta do jogo (a que tem Game.exe)}"; dst="${2:?pasta de destino}"
[[ -f "$src/Game.exe" ]] || { echo "erro: $src/Game.exe nao existe" >&2; exit 1; }
[[ ! -e "$dst" ]] || { echo "erro: $dst ja existe" >&2; exit 1; }
mkdir -p "$dst"
for f in "$src"/*; do
  name="$(basename "$f")"
  case "${name,,}" in
    game.exe|ijl11.dll|binkw32.dll|smackw32.dll|*.ini|*.exe|*.dll) cp --reflink=auto "$f" "$dst/" ;;
    data|aeonian) ;;  # nunca herdar (o overlay 'data' do jogo pode conter arquivos nao aplicados)
    *) ln -s "$f" "$dst/$name" ;;
  esac
done
echo "espelho criado em $dst ($(ls "$dst" | wc -l) entradas; MPQs por symlink)"
