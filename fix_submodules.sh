#!/bin/bash
# ============================================================
#  fix_submodules.sh
#  Corrige les submodules cassés et ajoute assemblage_solver
# ============================================================
set -e

echo "=== État actuel ==="
cat .gitmodules
echo ""
git submodule status || true
echo ""

# ── 1. Supprimer le submodule cassé archive/corrade ──────────
echo "=== Suppression de archive/corrade (submodule cassé) ==="

# Retirer de .gitmodules
git config -f .gitmodules --remove-section submodule.archive/corrade 2>/dev/null || true

# Retirer de .git/config
git config --remove-section submodule.archive/corrade 2>/dev/null || true

# Retirer du cache git
git rm --cached archive/corrade 2>/dev/null || true

# Supprimer les données git du submodule
rm -rf .git/modules/archive/corrade 2>/dev/null || true

echo "  archive/corrade supprimé."
echo ""

# ── 2. Nettoyer assemblage_solver s'il est mal initialisé ────
echo "=== Nettoyage assemblage_solver ==="

# Vérifier s'il est vide ou mal configuré
if [ -z "$(ls -A assemblage_solver 2>/dev/null)" ]; then
    echo "  Dossier vide — suppression."
    git rm --cached assemblage_solver 2>/dev/null || true
    rm -rf assemblage_solver
    rm -rf .git/modules/assemblage_solver 2>/dev/null || true
    git config -f .gitmodules --remove-section submodule.assemblage_solver 2>/dev/null || true
    git config --remove-section submodule.assemblage_solver 2>/dev/null || true
elif git submodule status assemblage_solver 2>/dev/null | grep -q "^-"; then
    echo "  Non initialisé — suppression et réajout."
    git rm --cached assemblage_solver 2>/dev/null || true
    rm -rf assemblage_solver
    rm -rf .git/modules/assemblage_solver 2>/dev/null || true
    git config -f .gitmodules --remove-section submodule.assemblage_solver 2>/dev/null || true
    git config --remove-section submodule.assemblage_solver 2>/dev/null || true
else
    echo "  Semble OK — on garde."
fi
echo ""

# ── 3. Committer le nettoyage ─────────────────────────────────
echo "=== Commit nettoyage ==="
git add .gitmodules
git commit -m "fix: remove broken corrade submodule, clean assemblage_solver" || \
    echo "  (rien à committer)"
echo ""

# ── 4. Ajouter assemblage_solver proprement ───────────────────
SUBMODULE_URL="https://github.com/Renartd/Calcul_Assemblage_Nucl-aire.git"

if [ ! -d "assemblage_solver" ]; then
    echo "=== Ajout du submodule assemblage_solver ==="
    git submodule add -b master \
        "$SUBMODULE_URL" \
        assemblage_solver
    git submodule update --init --recursive
    git add .gitmodules assemblage_solver
    git commit -m "feat: add assemblage_solver as submodule"
    echo "  Submodule ajouté et commité."
else
    echo "=== assemblage_solver déjà présent et valide ==="
    git submodule update --init --recursive
fi
echo ""

echo "=== État final ==="
cat .gitmodules
echo ""
git submodule status
echo ""

echo "=== Reste à faire : git push ==="
