#include "hpr/assets/AssetManager.h"
#include "hpr/renderer/ibl/CubemapMath.h"
#if defined(GL_VERSION_1_0) || defined(_glfw3_h_) || defined(IMGUI_VERSION)
#error AssetManager and cubemap math headers must not import graphics/window/editor APIs
#endif

#include "hpr/assets/Model.h"
#include "hpr/renderer/opengl/GpuModel.h"
#include "hpr/renderer/opengl/Skybox.h"
#include "hpr/renderer/ModelGpuCache.h"
#include "hpr/renderer/opengl/Shader.h"
#include <GLFW/glfw3.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <type_traits>

namespace
{
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
struct Fixtures
{
    std::filesystem::path directory =
        std::filesystem::temp_directory_path() /
        ("hpRenderer-assets-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Fixtures()
    {
        require(std::filesystem::create_directory(directory), "create fixture directory");
    }
    ~Fixtures()
    {
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored); // Only our unique temporary fixture.
    }
    std::string path(const char* name) const
    {
        return (directory / name).generic_u8string();
    }
    void text(const char* name, const char* value) const
    {
        std::ofstream stream(directory / name);
        stream << value;
        require(stream.good(), "write fixture");
    }
    void texture() const
    {
        // 3x2 RGB TGA: odd row byte count catches inherited unpack alignment.
        unsigned char header[18]{};
        header[2] = 2;
        header[12] = 3;
        header[14] = 2;
        header[16] = 24;
        header[17] = 0x20;
        std::ofstream stream(directory / "shared.tga", std::ios::binary);
        stream.write(reinterpret_cast<const char*>(header), 18);
        const unsigned char bgr[] = {32, 64, 128};
        for (int i = 0; i < 6; ++i)
            stream.write(reinterpret_cast<const char*>(bgr), 3);
        require(stream.good(), "write texture fixture");
    }
    void model() const
    {
        text("triangle.obj", "mtllib material.mtl\nusemtl shared\nv -1 -1 0\nv 1 -1 0\nv 0 1 0\n"
                             "vt 0 0\nvt 1 0\nvt 0.5 1\nvn 0 0 1\nf 1/1/1 2/2/1 3/3/1\n");
        text("material.mtl", "newmtl shared\nKd 1 1 1\nKs 1 1 1\nmap_Kd shared.tga\nmap_Ks shared.tga\n");
    }
};

void cpuTests()
{
    static_assert(!std::is_copy_constructible_v<AssetManager>);
    static_assert(!std::is_invocable_v<decltype(&AssetManager::GetShader), AssetManager&, const char*>,
                  "shader lookup must require a typed id");
    AssetManager assets;
    assets.Clear();
    require(!assets.GetShader(ShaderId::Model), "empty manager inherited shader");
    require(!assets.LoadModel("missing-asset-test-model.obj"), "failed model was reported as valid");
    const auto path = NormalizeAssetPath("assets/./textures/../sample.tga");
    const TextureKey color{path, Diffuse, ColorSpace::SRGB};
    const TextureKey linear{path, Diffuse, ColorSpace::Linear};
    const TextureKey normal{path, Normal, ColorSpace::Linear};
    std::unordered_map<TextureKey, int, TextureKeyHash> keys{{color, 1}, {linear, 2}, {normal, 3}};
    require(keys.size() == 3 && keys.at(color) == 1, "texture identity collapsed");
    require(path == NormalizeAssetPath("assets/sample.tga"), "lexical path normalization");
    const auto matrices = CalculateCubemapMatrices(glm::vec3(0), 1, 10);
    const glm::vec3 directions[] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (int i = 0; i < 6; ++i)
    {
        const auto clip = matrices[i] * glm::vec4(directions[i] * 2.0f, 1);
        require(std::abs(clip.x) < 0.0001f && std::abs(clip.y) < 0.0001f && clip.w > 0,
                "cubemap face direction changed");
    }
}

GLint format(const Texture& texture)
{
    glBindTexture(GL_TEXTURE_2D, texture.getID());
    GLint value = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &value);
    return value;
}
void gpuTests()
{
    {
        SkyboxAsset data;
        for (size_t i = 0; i < 6; ++i)
        {
            data.faces[i].width = data.faces[i].height = 1;
            data.faces[i].rgba = {static_cast<unsigned char>(30 + i * 20), 64, 128, 255};
        }
        Skybox skybox;
        GLuint previous = 0, unpackBuffer = 0;
        glGenTextures(1, &previous);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_CUBE_MAP, previous);
        glGenBuffers(1, &unpackBuffer);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpackBuffer);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, 64, nullptr, GL_STATIC_DRAW);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 7);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 2);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 3);
        require(skybox.load(data), "six-face upload failed");
        GLint value = 0;
        const std::pair<GLenum, GLint> states[] = {
            {GL_ACTIVE_TEXTURE, GL_TEXTURE3}, {GL_TEXTURE_BINDING_CUBE_MAP, static_cast<GLint>(previous)},
            {GL_PIXEL_UNPACK_BUFFER_BINDING, static_cast<GLint>(unpackBuffer)}, {GL_UNPACK_ALIGNMENT, 8},
            {GL_UNPACK_ROW_LENGTH, 7}, {GL_UNPACK_SKIP_ROWS, 2}, {GL_UNPACK_SKIP_PIXELS, 3}};
        for (const auto& state : states)
        {
            glGetIntegerv(state.first, &value);
            require(value == state.second, "skybox upload leaked GL state");
        }
        glBindTexture(GL_TEXTURE_CUBE_MAP, skybox.getID());
        for (size_t i = 0; i < 6; ++i)
        {
            unsigned char pixel[4]{};
            const auto face = GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<GLenum>(i);
            glGetTexImage(face, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
            glGetTexLevelParameteriv(face, 0, GL_TEXTURE_INTERNAL_FORMAT, &value);
            require(pixel[0] == 30 + i * 20 && pixel[1] == 64 && pixel[2] == 128 && value == GL_SRGB8_ALPHA8,
                    "skybox face order / color storage changed");
        }
        const auto id = skybox.getID();
        data.faces[2].height = 2;
        require(!skybox.load(data) && skybox.getID() == id, "invalid faces replaced a working skybox");
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glDeleteTextures(1, &previous);
        glDeleteBuffers(1, &unpackBuffer);
    }
    Fixtures fixture;
    fixture.texture();
    AssetManager assets;
    const auto path = fixture.path("shared.tga");
    glActiveTexture(GL_TEXTURE3);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
    auto color = assets.LoadTexture(path, Diffuse);
    auto linear = assets.LoadTexture(path, Diffuse, ColorSpace::Linear);
    auto normal = assets.LoadTexture(path, Normal);
    auto arm = assets.LoadTexture(path, ARM);
    require(color && linear && normal && arm, "valid textures failed to load");
    require(color != linear && linear != normal && normal != arm, "semantic/color-space cache alias");
    require(assets.LoadTexture(fixture.path("./shared.tga"), Diffuse) == color, "normalized path missed cache");
    require(color->getColorSpace() == ColorSpace::SRGB && normal->getType() == Normal,
            "cached texture metadata mismatch");
    GLint active = 0, unpack = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack);
    require(active == GL_TEXTURE3 && unpack == 8, "texture upload leaked state");
    require(format(*color) == GL_SRGB8_ALPHA8 && format(*linear) == GL_RGBA8 && format(*normal) == GL_RGBA8,
            "color space did not control actual GPU storage");

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, linear->getID());
    unsigned char pixels[18]{};
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    for (int i = 0; i < 6; ++i)
        require(pixels[i * 3] == 128 && pixels[i * 3 + 1] == 64 && pixels[i * 3 + 2] == 32,
                "odd-width texture rows uploaded incorrectly");
    require(!assets.LoadTexture(path, HDR, ColorSpace::SRGB), "sRGB HDR must be rejected");
    auto replacement = assets.ReloadTexture(path, Diffuse);
    require(replacement && replacement != color && assets.LoadTexture(path, Diffuse) == replacement,
            "texture reload did not replace the selected variant");
    require(assets.LoadTexture(path, Normal) == normal &&
                assets.LoadTexture(path, Diffuse, ColorSpace::Linear) == linear,
            "texture reload replaced unrelated variants");
    std::filesystem::rename(fixture.directory / "shared.tga", fixture.directory / "saved.tga");
    require(!assets.ReloadTexture(path, Diffuse) && assets.LoadTexture(path, Diffuse) == replacement,
            "failed texture reload discarded the valid cache");
    std::filesystem::rename(fixture.directory / "saved.tga", fixture.directory / "shared.tga");

    const auto modelPath = fixture.path("triangle.obj");
    require(!assets.LoadModel(modelPath), "missing model accepted");
    fixture.model();
    auto model = assets.LoadModel(modelPath);
    require(model && model->isValid(), "failed load poisoned the model cache");
    require(assets.LoadModel(fixture.path("./triangle.obj")) == model, "model identity missed normalized cache");
    ModelGpuCache gpuCache(assets);
    RenderScene submitted;
    submitted.objects.push_back({model});
    submitted.objects.push_back({model});
    auto prepared = gpuCache.prepare(submitted);
    auto firstGpu = prepared.objects[0].model;
    require(firstGpu == prepared.objects[1].model && gpuCache.prepare(submitted).objects[0].model == firstGpu,
            "GPU cache did not share unchanged model uploads");
    std::shared_ptr<Texture> diffuse, specular;
    GpuModel uploaded(*model, assets);
    for (std::size_t i = 0; i < uploaded.meshCount(); ++i)
        for (const auto& texture : uploaded.mesh(i).getTexture())
        {
            if (texture->getType() == Diffuse)
                diffuse = texture;
            if (texture->getType() == Specular)
                specular = texture;
        }
    require(diffuse == replacement && specular && specular != diffuse &&
                specular->getColorSpace() == ColorSpace::Linear,
            "model loader collapsed texture semantics");
    const auto oldMesh = model->snapshot();
    fixture.text("triangle.obj", "invalid model");
    require(!assets.ReloadModel(modelPath) && model->snapshot() == oldMesh,
            "failed model reload changed live geometry");
    fixture.model();
    require(assets.ReloadModel(modelPath) && assets.LoadModel(modelPath) == model &&
                model->snapshot() != oldMesh,
            "model reload did not preserve shared instance identity");
    prepared = gpuCache.prepare(submitted);
    require(prepared.objects[0].model != firstGpu && prepared.objects[0].model == prepared.objects[1].model &&
                prepared.objects[0].model->getRevision() == model->getRevision() && firstGpu->isValid(),
            "GPU cache did not publish a new upload while retaining the old snapshot");
    auto refreshed = assets.LoadTexture(path, Diffuse);
    require(refreshed != replacement, "model reload did not refresh external textures");
    std::filesystem::rename(fixture.directory / "shared.tga", fixture.directory / "saved.tga");
    require(assets.ReloadModel(modelPath), "model reload without external image");
    prepared = gpuCache.prepare(submitted);
    bool keptTexture = false;
    for (size_t i = 0; i < prepared.objects[0].model->meshCount(); ++i)
        for (const auto& texture : prepared.objects[0].model->mesh(i).getTexture())
            keptTexture |= texture == refreshed;
    require(keptTexture, "failed external image refresh discarded the valid cached texture");
    std::filesystem::rename(fixture.directory / "saved.tga", fixture.directory / "shared.tga");

    GLuint isolatedTexture = 0;
    std::shared_ptr<Model> survivor;
    {
        AssetManager other;
        auto separate = other.LoadTexture(path, Normal);
        isolatedTexture = separate->getID();
        require(separate != normal && !other.GetShader(ShaderId::Model), "managers share global cache");
        survivor = other.LoadModel(modelPath);
        require(survivor && survivor != model, "model cache is global");
    }
    require(!glIsTexture(isolatedTexture) && glIsTexture(normal->getID()), "manager destruction crossed ownership");
    require(survivor->isValid(), "model retained a dead loader instead of owning assets");
    survivor.reset();

    fixture.text("shader.vs", "#version 330 core\nvoid main(){gl_Position=vec4(0,0,0,1);}");
    fixture.text("shader.fs", "#version 330 core\nout vec4 color; void main(){ color=vec4(1); }");
    auto shader = assets.LoadShader(ShaderId::Model, fixture.path("shader.vs"), fixture.path("shader.fs"));
    require(shader && assets.GetShader(ShaderId::Model) == shader, "typed shader cache lookup");
    require(assets.LoadShader(ShaderId::Model, "unused.vs", "unused.fs") == shader, "shader cache identity");
    require(glGetError() == GL_NO_ERROR, "asset tests produced a GL error");
}
} // namespace

int main(int argc, char** argv)
{
    GLFWwindow* window = nullptr;
    int result = 0;
    try
    {
        cpuTests();
        if (argc > 1 && std::string(argv[1]) == "--gpu")
        {
            require(glfwInit() == GLFW_TRUE, "GLFW init");
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            window = glfwCreateWindow(32, 32, "Asset cache tests", nullptr, nullptr);
            require(window != nullptr, "hidden GL context");
            glfwMakeContextCurrent(window);
            require(gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) != 0, "GL loader");
            gpuTests(); // All manager/asset RAII destruction precedes context teardown, also on exceptions.
        }
        std::cout << "Asset manager tests passed.\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    if (window)
    {
        glfwDestroyWindow(window);
        glfwTerminate();
    }
    return result;
}
