#!/usr/bin/env bash
set -e

echo "=== Nettoyage complet de Corrade & Magnum ==="

sudo rm -rf /usr/local/include/Magnum
sudo rm -rf /usr/local/include/Corrade
sudo rm -rf /usr/local/lib/libMagnum*
sudo rm -rf /usr/local/lib/libCorrade*

echo "=== Installation des dépendances système ==="
sudo apt update
sudo apt install -y cmake git build-essential libsdl2-dev

echo "=== Clonage de Corrade ==="
rm -rf corrade
git clone https://github.com/mosra/corrade

echo "=== Compilation de Corrade ==="
cd corrade
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
make -j$(nproc)
sudo make install
cd ../..

echo "=== Clonage de Magnum ==="
rm -rf magnum
git clone https://github.com/mosra/magnum

echo "=== Compilation de Magnum en mode OpenGL DESKTOP ==="
cd magnum
mkdir build && cd build

cmake .. \
  -DTARGET_GLES=OFF \
  -DTARGET_GL=ON \
  -DWITH_GL=ON \
  -DWITH_WINDOWLESSGLAPPLICATION=ON \
  -DWITH_SDL2APPLICATION=ON \
  -DWITH_OPENGLTESTER=ON \
  -DWITH_ANYIMAGEIMPORTER=ON \
  -DWITH_ANYSCENEIMPORTER=ON \
  -DWITH_MESHTOOLS=ON \
  -DWITH_PRIMITIVES=ON \
  -DWITH_SHADERS=ON \
  -DWITH_SHADERTOOLS=ON \
  -DCMAKE_INSTALL_PREFIX=/usr/local

make -j$(nproc)
sudo make install
cd ../..

echo "=== Vérification du backend Magnum ==="
grep MAGNUM_TARGET_GL /usr/local/include/Magnum/configure.h || echo "MAGNUM_TARGET_GL ABSENT"
grep MAGNUM_TARGET_GLES /usr/local/include/Magnum/configure.h || echo "MAGNUM_TARGET_GLES ABSENT"

echo "=== Vérification des versions GL ==="
grep "GL330" /usr/local/include/Magnum/GL/Version.h && \
  echo "[OK] Magnum expose bien GL330 (OpenGL desktop actif)" || \
  echo "[ERREUR] GL330 introuvable — Magnum n'est pas en mode desktop"

echo "=== Installation terminée ==="
