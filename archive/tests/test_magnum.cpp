#include <Magnum/Platform/Sdl2Application.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/Math/Color.h>

using namespace Magnum;

class TestApp : public Platform::Sdl2Application {
public:
    explicit TestApp(const Arguments& arguments)
        : Platform::Sdl2Application{arguments, Configuration{}.setTitle("Magnum Test")}
    {}

    void drawEvent() override {
        GL::defaultFramebuffer.clear(GL::FramebufferClear::Color);
        swapBuffers();
        redraw();
    }
};

MAGNUM_APPLICATION_MAIN(TestApp)
