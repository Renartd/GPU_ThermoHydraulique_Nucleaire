#!/bin/bash

FILE="triangle.cpp"

echo "=== Vérification Magnum GL ==="

# 1. Vérification enums GL330
echo -n "• Vérification enums GL330... "
grep -q "GL330" /usr/local/include/Magnum/GL/Version.h && echo "OK" || echo "ABSENT"

# 2. Vérification constructeur Shader(Version, Type)
echo -n "• Vérification constructeur Shader(Version, Type)... "
grep -q "Shader(Version version, Type type)" /usr/local/include/Magnum/GL/Shader.h && echo "OK" || echo "ABSENT"

# 3. Vérification attachShaders
echo -n "• Vérification attachShaders... "
grep -R "attachShaders" /usr/local/include/Magnum/GL >/dev/null && echo "OK" || echo "ABSENT"

# 4. Vérification module GL activé
echo -n "• Vérification MAGNUM_WITH_GL dans CMakeCache... "
grep -q "MAGNUM_WITH_GL:BOOL=ON" ~/dev/magnum/magnum/build/CMakeCache.txt && echo "OK" || echo "DÉSACTIVÉ"

echo ""

echo "=== Vérification includes ==="

check_include() {
    local header=$1
    local name=$2
    echo -n "• $name... "
    grep -q "#include <$header>" "$FILE" && echo "OK" || echo "MANQUANT"
}

check_include "Magnum/GL/Shader.h" "Shader.h"
check_include "Magnum/GL/Version.h" "Version.h"
check_include "Magnum/GL/Mesh.h" "Mesh.h"
check_include "Magnum/GL/Buffer.h" "Buffer.h"
check_include "Magnum/GL/AbstractShaderProgram.h" "AbstractShaderProgram.h"
check_include "Magnum/GL/DefaultFramebuffer.h" "DefaultFramebuffer.h"

echo ""

echo "=== Analyse du code ==="

# Détection utilisation GL::Version sans include
if grep -q "GL::Version" "$FILE" && ! grep -q "Version.h" "$FILE"; then
    echo "⚠️  Utilisation de GL::Version sans include <Magnum/GL/Version.h>"
fi

# Détection utilisation Shader sans include
if grep -q "GL::Shader" "$FILE" && ! grep -q "Shader.h" "$FILE"; then
    echo "⚠️  Utilisation de GL::Shader sans include <Magnum/GL/Shader.h>"
fi

# Détection utilisation Mesh sans include
if grep -q "GL::Mesh" "$FILE" && ! grep -q "Mesh.h" "$FILE"; then
    echo "⚠️  Utilisation de GL::Mesh sans include <Magnum/GL/Mesh.h>"
fi

# Détection utilisation Buffer sans include
if grep -q "GL::Buffer" "$FILE" && ! grep -q "Buffer.h" "$FILE"; then
    echo "⚠️  Utilisation de GL::Buffer sans include <Magnum/GL/Buffer.h>"
fi

echo ""
echo "=== Vérification GLSL ==="

# Vérifier BOM UTF-8
if grep -q $'\xEF\xBB\xBF' "$FILE"; then
    echo "⚠️  BOM UTF-8 détecté dans le fichier — à supprimer"
else
    echo "• Pas de BOM UTF-8... OK"
fi


echo ""
echo "=== Vérification terminée ==="
