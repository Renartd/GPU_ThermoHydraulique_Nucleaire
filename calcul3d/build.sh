#!/bin/bash
echo "=== Build calcul3d ==="

# --- 1. Shaders GLSL → SPIR-V ---
echo "[1/2] Compilation shaders..."

compile_shader() {
    glslangValidator -V --target-env vulkan1.3 "$1" -o "$2" \
        && echo "      OK : $2" \
        || echo "      WARN : echec $1"
}

compile_shader compute/shaders/shadow_rq.comp  compute/shaders/shadow_rq.spv
compile_shader compute/shaders/diffusion.comp  compute/shaders/diffusion.spv

# --- 2. C++ ---
echo "[2/2] Compilation C++..."
g++ main.cpp \
    core/AssemblageLoader.cpp \
    -I. \
    -o viewer_raylib \
    -lraylib -lvulkan -lGL -lm -lpthread -ldl -lrt -lX11 \
    -std=c++17 \
    -O2

if [ $? -eq 0 ]; then
    echo "=== OK : ./viewer_raylib ==="
else
    echo "=== Echec compilation C++ ==="
fi
