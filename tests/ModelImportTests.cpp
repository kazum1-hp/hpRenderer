#include "hpr/assets/AssimpModelImporter.h"
#if defined(GL_VERSION_1_0) || defined(_glfw3_h_) || defined(IMGUI_VERSION)
#error Model import must compile without graphics/window/editor headers
#endif
#include "ModelImportFixtures.h"
#include <cmath>
#include <iostream>

int main()
{
    try
    {
        ModelImportFixtures fixture;
        fixture.triangle();
        ModelAsset asset;
        std::string error;
        importRequire(AssimpModelImporter::Import(fixture.path(), asset, error), error.c_str());
        importRequire(asset.meshes.size() == 1 && asset.draws.size() == 2, "node instances must share one mesh");
        const auto& left = asset.nodes.at(asset.draws[0].nodeIndex);
        const auto& right = asset.nodes.at(asset.draws[1].nodeIndex);
        importRequire(
            std::abs(left.worldTransform[3].x - .5f) < .001f && std::abs(left.worldTransform[3].y - .25f) < .001f &&
                std::abs(right.worldTransform[3].x + .5f) < .001f && std::abs(right.worldTransform[3].y - .25f) < .001f,
            "matrix conversion or parent multiplication");
        importRequire(left.parent >= 0 && glm::determinant(left.worldTransform) < 0, "hierarchy/mirroring lost");
        importRequire(asset.meshes[0].vertices.size() == 42 && asset.meshes[0].hasTangents,
                      "CPU vertex data/normals/tangents were not retained");
        const auto& values = asset.materials.at(asset.meshes[0].materialIndex);
        importRequire(std::abs(values.baseColor.r - .8f) < .001f && std::abs(values.roughness - .7f) < .001f &&
                          std::abs(values.metallic - .2f) < .001f,
                      "material factors were discarded");
        fixture.triangle(R"({"pbrMetallicRoughness":{"baseColorFactor":[1,1,1,0.25],
                         "metallicRoughnessTexture":{"index":0}},"occlusionTexture":{"index":1},
                         "alphaMode":"MASK","alphaCutoff":0.6,"doubleSided":true})",
                         false,
                         R"(,"images":[{"uri":"mr.png"},{"uri":"ao.png"}],"textures":[{"source":0},{"source":1}])");
        importRequire(AssimpModelImporter::Import(fixture.path(), asset, error), error.c_str());
        const auto& material = asset.materials.at(asset.meshes[0].materialIndex);
        importRequire(material.alphaMode == AlphaMode::Mask && material.doubleSided &&
                          std::abs(material.alphaCutoff - .6f) < .001f,
                      "alpha/double-sided import");
        bool ao = false, roughness = false, metallic = false;
        for (const auto& texture : material.textures)
        {
            ao |=
                texture.semantic == AmbientOcclusion && texture.path == fixture.path("ao.png") && texture.channel == 0;
            roughness |=
                texture.semantic == Roughness && texture.path == fixture.path("mr.png") && texture.channel == 1;
            metallic |= texture.semantic == Metallic && texture.path == fixture.path("mr.png") && texture.channel == 2;
        }
        importRequire(ao && roughness && metallic, "separate AO vs packed glTF G/B channels");
        fixture.text("broken.gltf", "invalid");
        const auto oldPath = asset.path;
        importRequire(!AssimpModelImporter::Import(fixture.path("broken.gltf"), asset, error) &&
                          asset.path == oldPath && !error.empty(),
                      "failed import must be transactional");
        importRequire(!AssimpModelImporter::Import(fixture.path("model.blend"), asset, error) &&
                          error.find("Blender") != std::string::npos,
                      "blend conversion guidance");
        const std::string root = HPRENDERER_SOURCE_DIR;
        // UTF-8 directory and filename, with a relative external .bin dependency.
        fixture.triangle();
        const auto unicodeDir = fixture.directory / std::filesystem::u8path(u8"\u4e0b\u8f7d \u6a21\u578b\u96c6");
        std::filesystem::create_directory(unicodeDir);
        const auto unicodeModel = unicodeDir / std::filesystem::u8path(u8"\u89d2\u8272.gltf");
        std::filesystem::copy_file(fixture.path(), unicodeModel);
        std::filesystem::copy_file(fixture.path("triangle.bin"), unicodeDir / "triangle.bin");
        importRequire(AssimpModelImporter::Import(unicodeModel.u8string(), asset, error), error.c_str());
        importRequire(asset.draws.size() == 2, "Unicode glTF and external binary import");
        // Both FBX encodings must work through the same model import path.
        for (const char* fbx : {"cubes_with_names.fbx", "embedded_ascii/box.FBX"})
        {
            const auto target = unicodeDir / std::filesystem::u8path(u8"\u89d2\u8272.FBX");
            std::filesystem::copy_file(root + "/third_party/assimp/test/models/FBX/" + fbx, target,
                                      std::filesystem::copy_options::overwrite_existing);
            importRequire(AssimpModelImporter::Import(target.u8string(), asset, error), error.c_str());
            importRequire(!asset.draws.empty(), "FBX static mesh import");
        }
        importRequire(AssimpModelImporter::Import(
                          root + "/third_party/assimp/test/models/glTF2/BoxTextured-glTF-Embedded/BoxTextured.gltf",
                          asset, error),
                      error.c_str());
        importRequire(!asset.images.empty() && !asset.images[0].bytes.empty(),
                      "embedded images outlive Assimp importer");
        importRequire(AssimpModelImporter::Import(
                          root + "/third_party/assimp/test/models/glTF2/BoxBadNormals-glTF-Binary/BoxBadNormals.glb",
                          asset, error),
                      error.c_str());
        importRequire(!asset.draws.empty(), "GLB static geometry");
        importRequire(
            AssimpModelImporter::Import(
                root + "/third_party/assimp/test/models/glTF2/BoxTextured-glTF-Binary/BoxTextured.glb", asset, error) &&
                !asset.images.empty(),
            "GLB binary embedded image");
        // Generate our own tiny PMX rather than relying on a non-redistributable character asset.
        std::ofstream pmx(fixture.directory / "triangle.pmx", std::ios::binary);
        auto bytes = [&](const void* p, std::size_t n) { pmx.write(static_cast<const char*>(p), n); };
        auto integer = [&](std::int32_t n) { bytes(&n, 4); };
        auto real = [&](float n) { bytes(&n, 4); };
        auto byte = [&](unsigned char n) { bytes(&n, 1); };
        bytes("PMX ", 4);
        real(2.0f);
        byte(8);
        const unsigned char globals[] = {1, 0, 1, 1, 1, 1, 1, 1};
        bytes(globals, 8);
        integer(0);
        integer(0);                           // UTF-8 names.
        const std::string comment(1024, 'x'); // Valid long comment also clears Assimp's struct-size file check.
        integer(static_cast<std::int32_t>(comment.size()));
        bytes(comment.data(), comment.size());
        integer(0);
        integer(3);
        for (int i = 0; i < 3; ++i)
        {
            real(i == 1 ? 1.0f : 0.0f);
            real(i == 2 ? 1.0f : 0.0f);
            real(0);
            real(0);
            real(0);
            real(1);
            real(i == 1 ? 1.0f : 0.0f);
            real(i == 2 ? 1.0f : 0.0f);
            byte(0);
            byte(0);
            real(1); // BDEF1, bone 0, edge scale.
        }
        integer(3);
        byte(0);
        byte(1);
        byte(2);
        integer(0);
        integer(1); // Indices, no textures, one material.
        integer(0);
        integer(0);
        real(.8f);
        real(.2f);
        real(.1f);
        real(1);
        for (int i = 0; i < 3; ++i)
            real(0);
        real(1);
        for (int i = 0; i < 3; ++i)
            real(0);
        byte(1);
        for (int i = 0; i < 4; ++i)
            real(0);
        real(1);
        byte(255);
        byte(255);
        byte(0);
        byte(1);
        byte(0);
        integer(0);
        integer(3);
        integer(1);
        integer(0);
        integer(0);
        real(0);
        real(0);
        real(0);
        byte(255);
        integer(0);
        const std::uint16_t flags = 0;
        bytes(&flags, 2);
        real(0);
        real(1);
        real(0);
        for (int i = 0; i < 4; ++i)
            integer(0); // Morphs, display frames, rigid bodies, joints.
        pmx.close();
        importRequire(AssimpModelImporter::Import(fixture.path("triangle.pmx"), asset, error), error.c_str());
        importRequire(asset.draws.size() == 1 && asset.meshes[0].indices.size() == 3, "PMX surface import");
        importRequire(asset.materials.at(asset.meshes[0].materialIndex).doubleSided,
                      "PMX double-sided flag was lost in Assimp");
        std::cout << "CPU model import tests passed\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
