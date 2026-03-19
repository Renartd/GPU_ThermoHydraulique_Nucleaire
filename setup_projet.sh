#!/bin/bash
# ============================================================
#  setup_projet.sh
#  À lancer UNE SEULE FOIS depuis la racine de ton dépôt
#  simulateur (GPU_ThermoHydraulique_Nucleaire)
#
#  Ce script :
#    1. Ajoute le dépôt assemblage comme submodule
#    2. Crée le Makefile racine (install/build/run)
#    3. Crée l'orchestrateur.sh
#    4. Committe tout
# ============================================================

set -e  # arrêt immédiat si une commande échoue

REPO_SIMULATEUR="$(pwd)"
SUBMODULE_URL="https://github.com/Renartd/Calcul_Assemblage_Nucl-aire.git"
SUBMODULE_DIR="assemblage_solver"
SUBMODULE_BRANCH="master"

echo "=== Répertoire courant : $REPO_SIMULATEUR ==="
echo ""

# ── 1. Vérifier qu'on est bien dans un dépôt git ────────────
if [ ! -d ".git" ]; then
    echo "ERREUR : lance ce script depuis la racine de ton dépôt git simulateur."
    exit 1
fi

# ── 2. Ajouter le submodule ───────────────────────────────────
echo "=== Ajout du submodule assemblage_solver ==="
if [ -d "$SUBMODULE_DIR" ]; then
    echo "  Dossier $SUBMODULE_DIR déjà présent — skip."
else
    git submodule add -b "$SUBMODULE_BRANCH" \
        "$SUBMODULE_URL" \
        "$SUBMODULE_DIR"
    echo "  Submodule ajouté."
fi

# Forcer l'init et le pull au cas où
git submodule update --init --recursive
echo ""

# ── 3. Créer l'orchestrateur ──────────────────────────────────
echo "=== Création de orchestrateur.sh ==="
cat > orchestrateur.sh << 'ORCH'
#!/bin/bash
# ============================================================
#  orchestrateur.sh
#  Lance le solveur d'assemblage, puis le simulateur.
#
#  Flux :
#    1. assemblage_solver  → génère calcul3d/data/Assemblage.txt
#    2. viewer_raylib      → lit Assemblage.txt et simule
# ============================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SOLVER_DIR="$SCRIPT_DIR/assemblage_solver"
SIM_DIR="$SCRIPT_DIR/calcul3d"
DATA_FILE="$SIM_DIR/data/Assemblage.txt"

echo "============================================"
echo "  Simulateur Neutronique-Thermique"
echo "  + Solveur d'assemblage"
echo "============================================"
echo ""

# ── Étape 1 : Solveur assemblage ─────────────────────────────
echo "[1/2] Solveur d'assemblage..."
cd "$SOLVER_DIR"

if [ ! -f "./assemblage_solver" ]; then
    echo "  ERREUR : binaire assemblage_solver introuvable."
    echo "  Lance d'abord : make build"
    exit 1
fi

# Le solveur écrit dans stdout ou dans un fichier selon sa config.
# On le lance et on récupère sa sortie vers Assemblage.txt
# ADAPTER cette ligne selon ce que fait réellement le binaire :
./assemblage_solver

# Vérifier que le fichier a été généré
if [ ! -f "$DATA_FILE" ]; then
    # Chercher le fichier généré ailleurs
    FOUND=$(find "$SOLVER_DIR" -name "Assemblage.txt" -newer "$SOLVER_DIR/assemblage_solver" 2>/dev/null | head -1)
    if [ -n "$FOUND" ]; then
        cp "$FOUND" "$DATA_FILE"
        echo "  Assemblage.txt copié depuis $FOUND"
    else
        echo "  ERREUR : Assemblage.txt non généré."
        exit 1
    fi
fi

echo "  → Assemblage.txt prêt : $DATA_FILE"
echo ""

# ── Étape 2 : Simulateur ─────────────────────────────────────
echo "[2/2] Lancement du simulateur..."
cd "$SIM_DIR"

if [ ! -f "./viewer_raylib" ]; then
    echo "  ERREUR : binaire viewer_raylib introuvable."
    echo "  Lance d'abord : make build"
    exit 1
fi

./viewer_raylib
ORCH
chmod +x orchestrateur.sh
echo "  orchestrateur.sh créé."
echo ""

# ── 4. Créer le Makefile racine ───────────────────────────────
echo "=== Création du Makefile ==="
cat > Makefile << 'MAKEFILE'
# ============================================================
#  Makefile racine — Simulateur Neutronique + Solveur
#
#  Commandes :
#    make install   → initialise les submodules git
#    make build     → compile les deux programmes
#    make run       → build + lance l'orchestrateur
#    make clean     → nettoie les binaires
# ============================================================

.PHONY: install build build-solver build-sim run clean help

# Répertoires
SOLVER_DIR  = assemblage_solver
SIM_DIR     = calcul3d

## Initialiser les submodules (à faire après un clone sans --recurse)
install:
	@echo "=== Initialisation submodules ==="
	git submodule update --init --recursive
	@echo "OK"

## Compiler le solveur C
build-solver:
	@echo "=== Compilation solveur assemblage (C) ==="
	@cd $(SOLVER_DIR) && \
	  CC=gcc CFLAGS="-std=c99 -O2 -Wall" && \
	  SRCS=$$(find . -name "*.c") && \
	  echo "Sources : $$SRCS" && \
	  gcc -std=c99 -O2 -Wall $$SRCS -o assemblage_solver -lm && \
	  echo "=== Solveur OK → $(SOLVER_DIR)/assemblage_solver ==="

## Compiler le simulateur C++
build-sim:
	@echo "=== Compilation simulateur (C++) ==="
	@cd $(SIM_DIR) && chmod +x build.sh && ./build.sh

## Compiler les deux
build: install build-solver build-sim

## Compiler et lancer
run: build
	@./orchestrateur.sh

## Nettoyage
clean:
	@rm -f $(SOLVER_DIR)/assemblage_solver
	@rm -f $(SIM_DIR)/viewer_raylib
	@echo "Binaires supprimés."

## Aide
help:
	@echo ""
	@echo "  make install    → init submodules git"
	@echo "  make build      → compile tout"
	@echo "  make run        → compile + lance"
	@echo "  make clean      → supprime les binaires"
	@echo ""
MAKEFILE
echo "  Makefile créé."
echo ""

# ── 5. Commit ─────────────────────────────────────────────────
echo "=== Commit ==="
git add .gitmodules "$SUBMODULE_DIR" orchestrateur.sh Makefile
git commit -m "feat: add assemblage_solver submodule + orchestrateur + Makefile"
echo ""

echo "============================================"
echo "  SETUP TERMINÉ"
echo ""
echo "  Prochaines étapes :"
echo "  1. git push"
echo "  2. Demande à Renartd de mettre son dépôt en public"
echo "     (GitHub → Settings → Danger Zone → Make public)"
echo "  3. Pour lancer : make run"
echo ""
echo "  Pour un clone depuis zéro :"
echo "    git clone --recurse-submodules <URL_TON_DEPOT>"
echo "    make run"
echo "============================================"
