#include <Magnum/Vk/Instance.h>
#include <Corrade/Utility/Debug.h>

using namespace Magnum;

int main() {
    // Instance Vulkan minimale (API simplifiée)
    Vk::Instance instance;

    Corrade::Utility::Debug{} << "Instance Vulkan OK";

    return 0;
}
