#!/bin/bash
# ============================================================
#  push_all.sh — Pousse le submodule ET le dépôt principal
#  Usage : ./push_all.sh "message de commit"
# ============================================================
set -e

MSG="${1:-update}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== Push submodule assemblage_solver ==="
cd "$SCRIPT_DIR/assemblage_solver"
git add -A
git diff --cached --quiet || git commit -m "$MSG"
git push origin master

echo "=== Push dépôt principal ==="
cd "$SCRIPT_DIR"
git add assemblage_solver
git add -A
git diff --cached --quiet || git commit -m "$MSG"
git push

echo "=== Tout poussé ==="
