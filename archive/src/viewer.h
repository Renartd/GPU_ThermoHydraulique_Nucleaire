#pragma once

#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/Shaders/FlatGL.h>
#include <Magnum/Math/Matrix4.h>

class Viewer {
public:
    Viewer();
    void draw();

private:
    Magnum::GL::Mesh _mesh;
    Magnum::Shaders::FlatGL3D _shader;
    Magnum::Matrix4 _transform;
};
