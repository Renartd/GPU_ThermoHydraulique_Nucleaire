#!/bin/bash
# ============================================================
#  compile.sh — Compilation du solveur d'assemblage (C)
#  Ne lance PAS le programme (géré par l'orchestrateur)
# ============================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== Compilation solveur assemblage ==="

SRCS=$(find . -name "*.c" -not -path "*/python*" -not -path "*/ancien*")
echo "Sources : $SRCS"

gcc -std=c99 -Wall -Wextra -O2 \
    $SRCS \
    -o assemblage_solver \
    -lm

echo "=== OK → $SCRIPT_DIR/assemblage_solver ==="
