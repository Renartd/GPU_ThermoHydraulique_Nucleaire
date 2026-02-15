#include "viewer.h"

#include <Magnum/Primitives/Cube.h>
#include <Magnum/Primitives/Plane.h>
#include <Magnum/Primitives/Cylinder.h>
#include <Magnum/Primitives/Cone.h>
#include <Magnum/Trade/MeshData3D.h>
#include <Magnum/MeshTools/Compile.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/Buffer.h>
#include <Corrade/Containers/ArrayView.h>

using namespace Magnum;

Viewer::Viewer(const Arguments& arguments) :
    Platform::Sdl2Application{arguments}
{
    /* Target */
    _target.setParent(&_scene);

    /* Camera */
    _cameraObject.setParent(&_scene);
    _camera = new SceneGraph::Camera3D{_cameraObject};
    _camera->setProjectionMatrix(Matrix4::perspectiveProjection(
        Deg(45.0f), Vector2{windowSize()}.aspectRatio(), 0.01f, 100.0f));
    _cameraObject.translate(Vector3::zAxis(_distance));

    /* Cube */
    Trade::MeshData3D cube = Primitives::cubeSolid();
    _cubeObject.setParent(&_scene);
    _cubeObject.translate(Vector3::yAxis(1.0f));   // posé sur le sol
    _cubeMesh = MeshTools::compile(cube);

    /* Ground plane */
    Trade::MeshData3D plane = Primitives::planeSolid();
    _groundObject.setParent(&_scene);
    _groundObject.rotateX(Deg(-90.0f));            // sol horizontal
    _groundObject.scale(Vector3{20.0f, 1.0f, 20.0f});
    _groundMesh = MeshTools::compile(plane);

    /* === GRID MESH (Magnum 2020 compatible) === */
    {
        const int N = 20;
        std::vector<Vector3> lines;
        lines.reserve((N*2+1)*4);

        for(int i = -N; i <= N; ++i) {
            // lignes X
            lines.push_back({(float)-N, 0.001f, (float)i});
            lines.push_back({(float) N, 0.001f, (float)i});

            // lignes Z
            lines.push_back({(float)i, 0.001f, (float)-N});
            lines.push_back({(float)i, 0.001f, (float) N});
        }

        GL::Buffer buffer;
        buffer.setData(
            Containers::ArrayView<const void>{lines.data(), lines.size()*sizeof(Vector3)},
            GL::BufferUsage::StaticDraw
        );

        _gridMesh = GL::Mesh{};
        _gridMesh.setPrimitive(GL::MeshPrimitive::Lines)
                 .setCount(lines.size())
                 .addVertexBuffer(std::move(buffer), 0, GL::Attribute<0, Vector3>{});
    }

    /* Gizmo meshes (Magnum 2020 signatures) */
    _gizmoCylinder = MeshTools::compile(
        Primitives::cylinderSolid(1, 8, 0.02f)
    );

    _gizmoCone = MeshTools::compile(
        Primitives::coneSolid(1, 8, 0.1f)
    );

    /* Shader */
    _shader = Shaders::Phong{};
}

void Viewer::drawEvent() {
    GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
    GL::Renderer::enable(GL::Renderer::Feature::FaceCulling);

    GL::defaultFramebuffer.clear(
        GL::FramebufferClear::Color |
        GL::FramebufferClear::Depth);

    /* Orbit camera */
    Vector3 pivot = _target.transformation().translation();

    Matrix4 rotation =
        Matrix4::rotationX(Deg(_pitch)) *
        Matrix4::rotationY(Deg(_yaw));

    Vector3 camPos = pivot + rotation.transformVector(Vector3::zAxis(_distance));

    Matrix4 world =
        Matrix4::translation(camPos) *
        rotation;

    _cameraObject.setTransformation(world);

    /* Light */
    Vector4 lightDir = {Vector3{-1.0f, -1.0f, -1.0f}, 0.0f};

    /* Ground */
    _shader
        .setAmbientColor(Color3{0.7f})
        .setDiffuseColor(Color3{0.0f})
        .setSpecularColor(Color3{0.0f})
        .setLightPositions({ lightDir })
        .setTransformationMatrix(_groundObject.transformationMatrix())
        .setNormalMatrix(_groundObject.transformationMatrix().normalMatrix())
        .setProjectionMatrix(_camera->projectionMatrix()*_camera->cameraMatrix());

    _groundMesh.draw(_shader);

    /* Grid */
    _shader.setAmbientColor(Color3{0.2f});
    _shader.setTransformationMatrix(Matrix4::translation({0.0f, 0.0f, 0.0f}));
    _gridMesh.draw(_shader);

    /* Cube */
    _shader
        .setAmbientColor(Color3{1.0f, 0.0f, 0.0f})
        .setDiffuseColor(Color3{0.0f})
        .setSpecularColor(Color3{0.0f})
        .setLightPositions({ lightDir })
        .setTransformationMatrix(_cubeObject.transformationMatrix())
        .setNormalMatrix(_cubeObject.transformationMatrix().normalMatrix())
        .setProjectionMatrix(_camera->projectionMatrix()*_camera->cameraMatrix());

    _cubeMesh.draw(_shader);

    /* Cube edges */
    GL::Renderer::setPolygonMode(GL::Renderer::PolygonMode::Line);
    _shader.setAmbientColor(Color3{0.0f});
    _cubeMesh.draw(_shader);
    GL::Renderer::setPolygonMode(GL::Renderer::PolygonMode::Fill);

    /* Gizmo */
    GL::Renderer::disable(GL::Renderer::Feature::DepthTest);

    Matrix4 gizmoBase =
        Matrix4::translation(Vector3{-3.0f, -2.0f, -5.0f}) *
        Matrix4::scaling(Vector3{0.5f});

    /* X axis */
    _shader.setAmbientColor(Color3{1.0f, 0.0f, 0.0f});
    _shader.setTransformationMatrix(gizmoBase * Matrix4::rotationZ(Deg(-90.0f)));
    _gizmoCylinder.draw(_shader);
    _shader.setTransformationMatrix(gizmoBase * Matrix4::rotationZ(Deg(-90.0f)) *
                                   Matrix4::translation(Vector3::zAxis(1.0f)));
    _gizmoCone.draw(_shader);

    /* Y axis */
    _shader.setAmbientColor(Color3{0.0f, 1.0f, 0.0f});
    _shader.setTransformationMatrix(gizmoBase);
    _gizmoCylinder.draw(_shader);
    _shader.setTransformationMatrix(gizmoBase *
                                   Matrix4::translation(Vector3::zAxis(1.0f)));
    _gizmoCone.draw(_shader);

    /* Z axis */
    _shader.setAmbientColor(Color3{0.0f, 0.0f, 1.0f});
    _shader.setTransformationMatrix(gizmoBase * Matrix4::rotationX(Deg(90.0f)));
    _gizmoCylinder.draw(_shader);
    _shader.setTransformationMatrix(gizmoBase * Matrix4::rotationX(Deg(90.0f)) *
                                   Matrix4::translation(Vector3::zAxis(1.0f)));
    _gizmoCone.draw(_shader);

    GL::Renderer::enable(GL::Renderer::Feature::DepthTest);

    swapBuffers();
}

void Viewer::mousePressEvent(MouseEvent& event) {
    _previousMousePosition = event.position();
}

void Viewer::mouseMoveEvent(MouseMoveEvent& event) {
    Vector2 delta = 0.3f * Vector2{event.position() - _previousMousePosition};
    _previousMousePosition = event.position();

    if(event.buttons() & MouseMoveEvent::Button::Left &&
      !(event.modifiers() & InputEvent::Modifier::Shift)) {

        _yaw   -= delta.x();
        _pitch -= delta.y();

        if(_pitch > 89.0f)  _pitch = 89.0f;
        if(_pitch < -89.0f) _pitch = -89.0f;

        redraw();
    }

    if((event.buttons() & MouseMoveEvent::Button::Right) ||
       (event.buttons() & MouseMoveEvent::Button::Left &&
        (event.modifiers() & InputEvent::Modifier::Shift))) {

        Vector3 right = _cameraObject.transformation().right();
        Vector3 up    = _cameraObject.transformation().up();

        _target.translateLocal((-delta.x()*0.01f)*right);
        _target.translateLocal(( delta.y()*0.01f)*up);

        redraw();
    }
}

void Viewer::mouseScrollEvent(MouseScrollEvent& event) {
    _distance -= event.offset().y() * 0.5f;

    if(_distance < _minDistance) _distance = _minDistance;
    if(_distance > _maxDistance) _distance = _maxDistance;

    redraw();
}
