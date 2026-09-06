#include "EnvironmentAsset.h"
#include <memory>
#include <type_traits>

#if defined(GL_VERSION_1_0) || defined(IMGUI_VERSION)
#error EnvironmentAsset must not include a graphics or editor API
#endif

int main()
{
    auto asset = std::make_shared<const EnvironmentAsset>("environment.hdr");
    auto selection = asset;
    asset.reset();
    if (selection->getPath() != "environment.hdr" || selection->getRevision() != 1) return 1;
    static_assert(!std::is_assignable_v<decltype(selection->getPath()), std::string>,
        "a selected asset path must not mutate behind the cache");
    return 0;
}
