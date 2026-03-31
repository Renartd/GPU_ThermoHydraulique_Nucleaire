# ============================================================
#  Makefile racine — Compatible Linux / macOS / Windows(Git Bash)
#
#  make install   → initialise les submodules git
#  make build     → compile les deux programmes
#  make run       → build + lance l'orchestrateur
#  make clean     → supprime les binaires
# ============================================================

.PHONY: install build build-solver build-sim run clean help

SOLVER_DIR = assemblage_solver/code_c
SIM_DIR    = calcul3d

# Détection Windows (Git Bash / MSYS)
ifeq ($(OS),Windows_NT)
    SHELL_CMD = bash
    RM_BIN    = del /Q
else
    SHELL_CMD = bash
    RM_BIN    = rm -f
endif

# ── Submodules ───────────────────────────────────────────────
install:
	@echo "=== Initialisation submodules ==="
	git submodule update --init --recursive
	@echo "OK"

# ── Solveur C ────────────────────────────────────────────────
build-solver:
	@echo "=== Compilation solveur (C) ==="
	@chmod +x $(SOLVER_DIR)/compile.sh
	@cd $(SOLVER_DIR) && ./compile.sh

# ── Simulateur C++ ───────────────────────────────────────────
build-sim:
	@echo "=== Compilation simulateur (C++) ==="
	@chmod +x $(SIM_DIR)/build.sh
	@cd $(SIM_DIR) && ./build.sh

# ── Build complet ────────────────────────────────────────────
build: install build-solver build-sim
	@echo ""
	@echo "=== Build terminé ==="
	@echo "Lance : make run"

# ── Lancement ────────────────────────────────────────────────
run: build
	@chmod +x orchestrateur.sh
	@./orchestrateur.sh

# ── Nettoyage ────────────────────────────────────────────────
clean:
	@$(RM_BIN) $(SOLVER_DIR)/assemblage_solver 2>/dev/null || true
	@$(RM_BIN) $(SIM_DIR)/viewer_raylib        2>/dev/null || true
	@echo "Binaires supprimés."

# ── Aide ─────────────────────────────────────────────────────
help:
	@echo ""
	@echo "  make install    → init submodules git"
	@echo "  make build      → compile solveur + simulateur"
	@echo "  make run        → compile et lance"
	@echo "  make clean      → supprime les binaires"
	@echo ""
	@echo "  Première utilisation :"
	@echo "    git clone --recurse-submodules <URL>"
	@echo "    make run"
	@echo ""
