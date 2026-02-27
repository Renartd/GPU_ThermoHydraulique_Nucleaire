#include <Magnum/Platform/Sdl2Application.h>
#include <Corrade/Containers/Pointer.h>

#include "viewer.h"

using namespace Magnum;

class App : public Platform::Application {
public:
    explicit App(const Arguments& arguments)
        : Platform::Application{arguments, Configuration{}
            .setTitle("Cube Rouge")
            .setSize({1280, 720})}
    {
        _viewer = Containers::pointer<Viewer>();
    }

private:
    Containers::Pointer<Viewer> _viewer;

    void drawEvent() override {
        _viewer->draw();
        swapBuffers();
        redraw();
    }
};

MAGNUM_APPLICATION_MAIN(App)
