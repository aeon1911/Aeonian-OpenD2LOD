#!/usr/bin/env bash
# Aeonian-OpenD2LOD
# Copyright (C) 2026 Aeonian-OpenD2LOD contributors
# Licença: GPLv3 (ver LICENSE)
#
# Menu unificado para seleção de versão e mod do Diablo II.

set -euo pipefail

PROJETOS_DIR="/mnt/STEAM/Projetos"
D2_114D_PLAY="$PROJETOS_DIR/d2-aeonian-play/play.sh"
D2_PD2_PLAY="$PROJETOS_DIR/d2-pd2-play/play.sh"
D2_MXL_PLAY="$PROJETOS_DIR/d2-mxl-play/play.sh"

launch_game() {
    local target="$1"
    if [[ ! -f "$target" ]]; then
        echo "Erro: executável de inicialização não encontrado em $target" >&2
        exit 1
    fi
    exec "$target" "$@"
}

# 1. Interface Gráfica com Zenity (se disponível e em sessão com display)
if [[ -n "${DISPLAY:-}" || -n "${WAYLAND_DISPLAY:-}" ]] && command -v zenity >/dev/null 2>&1; then
    CHOICE=$(zenity --list --radiolist \
        --title="Aeonian Diablo II Launcher" \
        --text="Selecione qual versão do Diablo II deseja iniciar:" \
        --width=620 --height=340 \
        --column="Escolha" --column="ID" --column="Versão / Mod" --column="Recursos Ativos" \
        TRUE  "114d" "Diablo II 1.14d (Canônico)"       "D2GL + Mapa + PlugY Stash + HUD + Planner" \
        FALSE "pd2"  "Project Diablo 2 (1.13c)"         "Multi-DLL 1.13c + PlugY Stash + HUD" \
        FALSE "mxl"  "Median XL Sigma 2.9.2 (1.13c)"    "D2Sigma + D2GL + PlugY Stash + HUD" \
        --hide-column=2 2>/dev/null || true)

    case "$CHOICE" in
        "114d") launch_game "$D2_114D_PLAY" ;;
        "pd2")  launch_game "$D2_PD2_PLAY" ;;
        "mxl")  launch_game "$D2_MXL_PLAY" ;;
        *)      exit 0 ;; # Usuário cancelou ou fechou a janela
    esac
fi

# 2. Interface de Terminal (Fallback elegante para CLI)
echo "============================================================"
echo "               Aeonian Diablo II Launcher"
echo "============================================================"
echo " 1) Diablo II 1.14d Canônico (D2GL, Mapa, PlugY, HUD)"
echo " 2) Project Diablo 2 (Multi-DLL 1.13c, PlugY, HUD)"
echo " 3) Median XL Sigma 2.9.2 (D2Sigma, D2GL, PlugY, HUD)"
echo " 4) Sair"
echo "============================================================"
read -r -p "Escolha uma opção [1-4]: " op

case "$op" in
    1) launch_game "$D2_114D_PLAY" ;;
    2) launch_game "$D2_PD2_PLAY" ;;
    3) launch_game "$D2_MXL_PLAY" ;;
    4) exit 0 ;;
    *) echo "Opção inválida."; exit 1 ;;
esac

