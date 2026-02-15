#include "Application.hpp"

#include <Magnum/Primitives/Cube.h>
#include <Magnum/Trade/MeshData3D.h>
#include <Magnum/MeshTools/Compile.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>

using namespace Magnum;

Application::Application(const Arguments& arguments) :
    Platform::Sdl2Application{arguments}
{
    _cameraObject.translate(Vector3::zAxis(5.0f));

    _camera = new SceneGraph::Camera3D{_cameraObject};
    _camera->setProjectionMatrix(Matrix4::perspectiveProjection(
        Deg(45.0f), Vector2{windowSize()}.aspectRatio(), 0.01f, 100.0f));

    Trade::MeshData3D cube = Primitives::cubeSolid();
    _cubeMesh = MeshTools::compile(cube);

    _shader = Shaders::Phong{};
    _shader.setDiffuseColor(Color3{1.0f, 0.0f, 0.0f})
           .setSpecularColor(Color3{1.0f})
           .setShininess(80.0f);
}

void Application::drawEvent() {
    GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
    GL::Renderer::enable(GL::Renderer::Feature::FaceCulling);

    GL::defaultFramebuffer.clear(
        GL::FramebufferClear::Color |
        GL::FramebufferClear::Depth);

    _shader
        .setLightPosition({5.0f, 5.0f, 5.0f})
        .setTransformationMatrix(_cubeObject.transformationMatrix())
        .setNormalMatrix(_cubeObject.transformationMatrix().normalMatrix())
        .setProjectionMatrix(_camera->projectionMatrix()*_camera->cameraMatrix());

    _cubeMesh.draw(_shader);

    swapBuffers();
    redraw();
}
