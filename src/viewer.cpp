#include "viewer.h"

#include <Magnum/Primitives/Cube.h>
#include <Magnum/MeshTools/Compile.h>

using namespace Magnum;

Viewer::Viewer() :
    _mesh{MeshTools::compile(Primitives::cubeSolid())},
    _shader{},
    _transform{Matrix4::scaling({1.0f, 1.0f, 1.0f})}
{
}

void Viewer::draw() {
    _shader
        .setColor(0xff0000_rgbf)   // Rouge
        .setTransformationProjectionMatrix(Matrix4::perspectiveProjection(
            35.0_degf, 1.33f, 0.01f, 100.0f) *
            Matrix4::translation({0.0f, 0.0f, -5.0f}) *
            _transform);

    _mesh.draw(_shader);
}
