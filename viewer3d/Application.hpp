#pragma once

#include <Magnum/Platform/Sdl2Application.h>
#include <Magnum/SceneGraph/Scene.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Magnum/SceneGraph/MatrixTransformation3D.h>
#include <Magnum/Shaders/Phong.h>
#include <Magnum/GL/Mesh.h>

using namespace Magnum;

class Application : public Platform::Sdl2Application {
public:
    explicit Application(const Arguments& arguments);

private:
    void drawEvent() override;

    SceneGraph::Scene<SceneGraph::MatrixTransformation3D> _scene;
    SceneGraph::Object<SceneGraph::MatrixTransformation3D> _cameraObject;
    SceneGraph::Camera3D* _camera;

    SceneGraph::Object<SceneGraph::MatrixTransformation3D> _cubeObject;
    GL::Mesh _cubeMesh;
    Shaders::Phong _shader;
};
