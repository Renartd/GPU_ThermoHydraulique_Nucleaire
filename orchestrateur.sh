#!/bin/bash
# ============================================================
#  orchestrateur.sh
#  Compatible : Linux, macOS, Windows (Git Bash / WSL)
#
#  Flux :
#    1. assemblage_solver  → génère calcul3d/data/Assemblage.txt
#    2. viewer_raylib      → lit Assemblage.txt et simule
# ============================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SOLVER_BIN="$SCRIPT_DIR/assemblage_solver/nuclear/code_c/assemblage_solver"
SIM_DIR="$SCRIPT_DIR/calcul3d"
SIM_BIN="$SIM_DIR/viewer_raylib"
DATA_DIR="$SIM_DIR/data"
DATA_FILE="$DATA_DIR/Assemblage.txt"

# ── Détection OS ─────────────────────────────────────────────
detect_os() {
    case "$(uname -s)" in
        Linux*)   echo "linux"  ;;
        Darwin*)  echo "mac"    ;;
        MINGW*|MSYS*|CYGWIN*) echo "windows" ;;
        *)        echo "unknown" ;;
    esac
}
OS=$(detect_os)
echo "============================================"
echo "  Simulateur Neutronique-Thermique"
echo "  Système détecté : $OS"
echo "============================================"
echo ""

# ── Vérification des binaires ────────────────────────────────
if [ ! -f "$SOLVER_BIN" ]; then
    echo "ERREUR : solveur non compilé."
    echo "Lance : make build"
    exit 1
fi

if [ ! -f "$SIM_BIN" ]; then
    echo "ERREUR : simulateur non compilé."
    echo "Lance : make build"
    exit 1
fi

# ── Créer le dossier data si absent ──────────────────────────
mkdir -p "$DATA_DIR"

# ── Étape 1 : Solveur d'assemblage ───────────────────────────
echo "[1/2] Solveur d'assemblage..."
echo "      (réponds aux questions pour configurer le cœur)"
echo ""

cd "$SCRIPT_DIR/assemblage_solver/nuclear/code_c"
./assemblage_solver

# Vérifier que Assemblage.txt a bien été écrit
if [ ! -f "$DATA_FILE" ]; then
    echo ""
    echo "ERREUR : $DATA_FILE non généré."
    echo "Vérifiez le chemin de sortie dans main.c"
    exit 1
fi

echo ""
echo "  → Assemblage.txt prêt"
echo ""

# ── Étape 2 : Simulateur ─────────────────────────────────────
echo "[2/2] Lancement du simulateur..."
cd "$SIM_DIR"
./viewer_raylib
