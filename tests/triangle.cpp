#include <Magnum/Platform/Sdl2Application.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Shader.h>
#include <Magnum/GL/Version.h>
#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/Math/Color.h>

using namespace Magnum;

class TriangleShader : public GL::AbstractShaderProgram {
public:
    TriangleShader() {
        GL::Shader vert{GL::Version::GL330, GL::Shader::Type::Vertex};
        GL::Shader frag{GL::Version::GL330, GL::Shader::Type::Fragment};

        vert.addSource(
            "layout(location = 0) in vec2 position;\n"
            "void main() {\n"
            "    gl_Position = vec4(position, 0.0, 1.0);\n"
            "}\n"
        );

        frag.addSource(
            "out vec4 color;\n"
            "void main() {\n"
            "    color = vec4(1.0, 0.2, 0.2, 1.0);\n"
            "}\n"
        );

        CORRADE_INTERNAL_ASSERT_OUTPUT(vert.compile());
        CORRADE_INTERNAL_ASSERT_OUTPUT(frag.compile());

        attachShaders({vert, frag});
        CORRADE_INTERNAL_ASSERT_OUTPUT(link());
    }
};

class TriangleApp : public Platform::Sdl2Application {
public:
    explicit TriangleApp(const Arguments& arguments);

    void drawEvent() override;

private:
    GL::Buffer _vertexBuffer;
    GL::Mesh _mesh;
    TriangleShader _shader;
};

TriangleApp::TriangleApp(const Arguments& arguments)
    : Platform::Sdl2Application{arguments, Configuration{}.setTitle("Triangle Magnum")}
{
    const Vector2 vertices[]{
        {-0.5f, -0.5f},
        { 0.5f, -0.5f},
        { 0.0f,  0.5f}
    };

    _vertexBuffer.setData(vertices);

    _mesh.setPrimitive(GL::MeshPrimitive::Triangles)
         .setCount(3)
         .addVertexBuffer(_vertexBuffer, 0, GL::Attribute<0, Vector2>{});

    redraw();
}

void TriangleApp::drawEvent() {
    GL::defaultFramebuffer.clear(GL::FramebufferClear::Color);

    _shader.draw(_mesh);

    swapBuffers();
    redraw();
}

MAGNUM_APPLICATION_MAIN(TriangleApp)
