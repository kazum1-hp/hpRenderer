#pragma once
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

inline void importRequire(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
struct ModelImportFixtures
{
    std::filesystem::path directory =
        std::filesystem::temp_directory_path() /
        ("hpRenderer-import-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ModelImportFixtures()
    {
        importRequire(std::filesystem::create_directory(directory), "fixture directory");
        std::ofstream bin(directory / "triangle.bin", std::ios::binary);
        const float positions[] = {-.6f, -.6f, 0, .6f, -.6f, 0, 0, .6f, 0};
        const std::uint16_t indices[] = {0, 1, 2, 0}; // Last entry is alignment padding.
        const float uv[] = {0, 0, 1, 0, .5f, 1};
        bin.write(reinterpret_cast<const char*>(positions), sizeof(positions));
        bin.write(reinterpret_cast<const char*>(indices), sizeof(indices));
        bin.write(reinterpret_cast<const char*>(uv), sizeof(uv));
        importRequire(bin.good(), "fixture binary");
    }
    ~ModelImportFixtures()
    {
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored); // Only this test's unique temporary directory.
    }
    std::string path(const char* name = "triangle.gltf") const
    {
        return (directory / name).generic_u8string();
    }
    void text(const char* name, const std::string& contents) const
    {
        std::ofstream stream(directory / name);
        stream << contents;
        importRequire(stream.good(), "fixture text");
    }
    void triangle(
        const std::string& material =
            R"({"pbrMetallicRoughness":{"baseColorFactor":[0.8,0.2,0.1,1],"roughnessFactor":0.7,"metallicFactor":0.2}})",
        bool hierarchy = true, const std::string& extra = "") const
    {
        const std::string nodes = hierarchy ?
                                            R"([{"name":"parent","translation":[0,0.25,0],"children":[1,2]},
                {"name":"mirrored","translation":[0.5,0,0],"scale":[-0.5,0.5,1],"mesh":0},
                {"name":"matrix","matrix":[0.5,0,0,0,0,0.5,0,0,0,0,1,0,-0.5,0,0,1],"mesh":0}])"
                                            : R"([{"mesh":0}])";
        text("triangle.gltf", std::string(R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":)") +
                                  nodes +
                                  R"(,"buffers":[{"uri":"triangle.bin","byteLength":68}],
            "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6},
                           {"buffer":0,"byteOffset":44,"byteLength":24}],
            "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[-0.6,-0.6,0],"max":[0.6,0.6,0]},
                         {"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"},
                         {"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"}],
            "meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":2},"indices":1,"material":0}]}],"materials":[)" +
                                  material + "]" + extra + "}");
    }
};
