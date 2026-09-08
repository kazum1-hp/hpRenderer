#include "hpr/assets/Model.h"
#include "hpr/assets/SkyboxAsset.h"
#include "hpr/scene/Scene.h"
#if defined(GL_VERSION_1_0) || defined(_glfw3_h_) || defined(IMGUI_VERSION)
#error CPU models, images and scene data must not import graphics/window/editor APIs
#endif
#include "ModelImportFixtures.h"
#include <iostream>

int main()
{
    try
    {
        ModelImportFixtures fixture;
        fixture.triangle();
        Model model(fixture.path()); // No GL loader or context exists in this executable.
        importRequire(model.isValid() && model.getRevision() == 1, "CPU model construction");
        auto snapshot = model.snapshot();
        fixture.text("triangle.gltf", "invalid");
        importRequire(!model.reload(fixture.path()) && model.snapshot() == snapshot && model.getRevision() == 1,
                      "failed CPU reload discarded snapshot");
        fixture.triangle();
        importRequire(model.reload(fixture.path()) && model.getRevision() == 2 && model.snapshot() != snapshot,
                      "successful CPU reload did not publish a new revision");
        importRequire(!snapshot->draws.empty(), "old snapshot lost owned data");
        Scene scene;
        importRequire(scene.GetEnvironment().mode == EnvironmentMode::IBL, "IBL must remain the default");

        ImageData invalid;
        invalid.width = invalid.height = 2;
        invalid.rgba = {255};
        importRequire(!invalid.valid(), "short pixel buffers must be rejected");
        invalid.rgba.resize(16);
        importRequire(invalid.valid(), "valid RGBA pixels rejected");
        invalid.hdr.resize(16);
        importRequire(!invalid.valid(), "ambiguous float/byte image accepted");
        importRequire(!ImageData::Decode({1, 2, 3}).valid(), "corrupt image accepted");
        // Six tiny, distinct faces in a Unicode directory, decoded entirely on the CPU.
        auto images = fixture.directory / std::filesystem::u8path(u8"\u5929\u7a7a\u76d2");
        std::filesystem::create_directory(images);
        std::array<std::string, 6> paths;
        for (size_t i = 0; i < paths.size(); ++i)
        {
            auto path = images / (std::to_string(i) + ".tga");
            unsigned char tga[21]{};
            tga[2] = 2;
            tga[12] = tga[14] = 1;
            tga[16] = 24;
            tga[17] = 0x20;
            tga[20] = static_cast<unsigned char>(40 + i);
            std::ofstream stream(path, std::ios::binary);
            stream.write(reinterpret_cast<const char*>(tga), sizeof(tga));
            paths[i] = path.generic_u8string();
        }
        std::string error;
        const std::array<std::string, 6> mixed{"nx.png", "px.png", "py.png", "ny.png", "nz.png", "pz.png"};
        std::array<std::string, 6> ordered;
        importRequire(SkyboxAsset::MatchNamedFaces(mixed, ordered, error), "named face matching failed");
        importRequire(ordered == std::array<std::string, 6>{"px.png", "nx.png", "py.png", "ny.png", "pz.png", "nz.png"},
                      "named faces mapped to incorrect axes");
        const auto previous = ordered;
        auto duplicate = mixed;
        duplicate[0] = "px.png";
        importRequire(!SkyboxAsset::MatchNamedFaces(duplicate, ordered, error) && ordered == previous,
                      "invalid matching changed existing paths");
        auto skybox = SkyboxAsset::Load(paths, error);
        importRequire(skybox && error.empty(), "six-image CPU load / Unicode path failed");
        for (size_t i = 0; i < 6; ++i)
            importRequire(skybox->faces[i].rgba[0] == 40 + i && skybox->faces[i].rgba[3] == 255,
                          "face order or RGBA decoding changed");
        paths[0] = fixture.path("missing.png");
        importRequire(!SkyboxAsset::Load(paths, error) && !error.empty(), "missing skybox face accepted");
        std::cout << "CPU asset boundary tests passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
