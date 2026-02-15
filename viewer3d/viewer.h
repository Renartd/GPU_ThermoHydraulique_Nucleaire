#pragma once

#include <Magnum/Platform/Sdl2Application.h>
#include <Magnum/SceneGraph/Scene.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Magnum/SceneGraph/MatrixTransformation3D.h>
#include <Magnum/Shaders/Phong.h>
#include <Magnum/GL/Mesh.h>

using namespace Magnum;

class Viewer : public Platform::Sdl2Application {
public:
    explicit Viewer(const Arguments& arguments);

private:
    void drawEvent() override;
    void mousePressEvent(MouseEvent& event) override;
    void mouseMoveEvent(MouseMoveEvent& event) override;
    void mouseScrollEvent(MouseScrollEvent& event) override;

    /* Scene */
    SceneGraph::Scene<SceneGraph::MatrixTransformation3D> _scene;

    /* Camera */
    SceneGraph::Object<SceneGraph::MatrixTransformation3D> _cameraObject;
    SceneGraph::Camera3D* _camera{};

    /* Target (pivot) */
    SceneGraph::Object<SceneGraph::MatrixTransformation3D> _target;

    /* Cube */
    SceneGraph::Object<SceneGraph::MatrixTransformation3D> _cubeObject;
    GL::Mesh _cubeMesh;
    Shaders::Phong _shader;

    /* Ground */
    SceneGraph::Object<SceneGraph::MatrixTransformation3D> _groundObject;
    GL::Mesh _groundMesh;

    /* Grid */
    GL::Mesh _gridMesh;

    /* Gizmo */
    GL::Mesh _gizmoCylinder;
    GL::Mesh _gizmoCone;

    /* Camera parameters */
    float _distance    = 5.0f;
    float _minDistance = 1.0f;
    float _maxDistance = 20.0f;

    float _yaw   = 0.0f;
    float _pitch = 0.0f;

    Vector2i _previousMousePosition;
};
