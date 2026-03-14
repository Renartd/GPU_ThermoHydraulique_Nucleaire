#!/bin/bash
# ============================================================
#  build.sh  v2.0
#  Compilation du simulateur neutronique-thermique
# ============================================================

echo "=== Compilation shaders SPIR-V ==="

SHADER_DIR="compute/shaders"

compile_shader() {
    local src="$1"
    local spv="${src%.comp}.spv"
    echo "  $src → $spv"
    glslangValidator -V --target-env vulkan1.3 "$src" -o "$spv"
    if [ $? -ne 0 ]; then
        echo "ERREUR shader : $src"
        exit 1
    fi
}

# Shaders thermiques (inchangés)
compile_shader "$SHADER_DIR/diffusion.comp"

# Shaders neutroniques v2 (SoA + FVM)
compile_shader "$SHADER_DIR/neutron_fvm.comp"
compile_shader "$SHADER_DIR/neutron_reduce.comp"

echo "=== Shaders OK ==="
echo ""
echo "=== Compilation C++ ==="

CXX=g++
CXXFLAGS="-std=c++17 -O2 -Wno-deprecated-declarations"

INCLUDES="-I. -I/usr/include"

LIBS="-lraylib -lvulkan -lGL -lm -lpthread -ldl -lrt -lX11"

# Trouver tous les .cpp sauf les doublons obsolètes
SRCS=$(find . -name "*.cpp" \
       -not -path "*/files/*" \
       -not -name "ReactorParams.cpp")

echo "Sources :"
echo "$SRCS"

$CXX $CXXFLAGS $INCLUDES $SRCS -o viewer_raylib $LIBS

if [ $? -eq 0 ]; then
    echo "=== Build OK → ./viewer_raylib ==="
else
    echo "=== ERREUR de compilation ==="
    exit 1
fi