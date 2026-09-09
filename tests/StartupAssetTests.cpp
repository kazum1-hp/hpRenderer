#include "hpr/assets/AssimpModelImporter.h"
#include "hpr/assets/ImageData.h"
#include "hpr/core/RuntimePaths.h"
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>

int main(int argc, char** argv)
{
    try
    {
        if (argc != 2)
            throw std::runtime_error("Usage: hpRenderer_startup_asset_tests <assets-directory>");
        const auto root = std::filesystem::absolute(std::filesystem::u8path(argv[1]));
        // Changing the caller's directory must not change the application's asset lookup base.
        SetExecutableWorkingDirectory();
        const auto executableDirectory = std::filesystem::current_path();
        std::filesystem::current_path(root);
        SetExecutableWorkingDirectory();
        if (std::filesystem::current_path() != executableDirectory)
            throw std::runtime_error("Executable working directory was not restored");

        std::set<std::string> images;
        const char* models[] = {"marble_bust_01_4k/marble_bust_01_4k.gltf",
                                "blue_metal_plate_4k.gltf/blue_metal_plate_4k.gltf",
                                "metal_office_desk_4k/metal_office_desk_4k.gltf"};
        for (int i = 0; i < 3; ++i)
        {
            const auto path = root / "models" / models[i];
            if (i != 0 && !std::filesystem::exists(path))
                continue; // Optional full demo.
            ModelAsset model;
            std::string error;
            if (!AssimpModelImporter::Import(path.u8string(), model, error))
                throw std::runtime_error(error);
            if (model.draws.empty())
                throw std::runtime_error("Demo model has no draw data");
            for (const auto& material : model.materials)
                for (const auto& texture : material.textures)
                    if (texture.embeddedImage < 0 && !texture.path.empty())
                        images.insert(texture.path);
        }
        for (const char* name : {"bricks2.jpg", "bricks2_normal.jpg", "bricks2_disp.jpg"})
            images.insert((root / "textures/bricks2" / name).u8string());
        for (const auto& image : images)
            if (!ImageData::Load(image).valid())
                throw std::runtime_error("Missing/invalid texture: " + image);
        const auto hdr = ImageData::Load((root / "hdr/newport_loft.hdr").u8string(), true);
        if (!hdr.valid() || !hdr.isHDR())
            throw std::runtime_error("Missing/invalid demo HDR");
        std::cout << "Startup model dependencies, " << images.size() << " textures, HDR and runtime paths passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
