#include "hpr/assets/Model.h"
#include "hpr/renderer/opengl/GpuModel.h"
#include "hpr/assets/AssetManager.h"
#include "hpr/renderer/passes/RenderPasses.h"
#include "hpr/renderer/passes/DrawHelpers.h"
#include "hpr/renderer/opengl/PrimitiveMeshes.h"
#include "hpr/renderer/RenderTargets.h"
#include "ModelImportFixtures.h"
#include <GLFW/glfw3.h>
#include <array>
#include <cmath>
#include <iostream>
#include <type_traits>

static_assert(std::is_same_v<decltype(std::declval<const GpuModel&>().mesh(0)), const Mesh&>);
namespace
{
std::array<float, 4> pixel(const FrameBuffer& target, int attachment, int x = 32, int y = 32)
{
    glBindFramebuffer(GL_READ_FRAMEBUFFER, target.getFBO());
    glReadBuffer(GL_COLOR_ATTACHMENT0 + attachment);
    std::array<float, 4> value{};
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_FLOAT, value.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    return value;
}
float depth(const FrameBuffer& target, int x = 32, int y = 32)
{
    glBindFramebuffer(GL_READ_FRAMEBUFFER, target.getFBO());
    float value = 0;
    glReadPixels(x, y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &value);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    return value;
}
void run()
{
    AssetManager assets;
    const std::string root = HPRENDERER_SOURCE_DIR;
    for (const auto& entry : {std::pair<ShaderId, const char*>{ShaderId::Model, "model"},
                              {ShaderId::GBuffer, "gBuffer"},
                              {ShaderId::DeferredLighting, "lightPass"},
                              {ShaderId::DirectionalShadow, "shadow"}})
        assets.LoadShader(entry.first, root + "/shaders/" + entry.second + ".vs",
                          root + "/shaders/" + entry.second + ".fs");
    assets.LoadShader(ShaderId::PointShadow, root + "/shaders/pointShadow.vs", root + "/shaders/pointShadow.fs",
                      root + "/shaders/pointShadow.gs");
    Rendering::ForwardPass forward(assets);
    Rendering::GBufferPass gbuffer(assets);
    Rendering::DeferredLightingPass lighting(assets);
    Rendering::ShadowPass shadow(assets);
    RenderTargets targets;
    targets.initialize({64, 64}, 64, ColorFormat::RGBA16F);
    auto plane = Rendering::CreatePlane();
    auto quad = Rendering::CreateScreenQuad();
    RenderSettings settings;
    settings.postProcess.enabled = true;
    CameraData camera;
    camera.view = camera.projection = glm::mat4(1);
    camera.position = {0, 0, 3};
    RenderFrameData frame;
    frame.directionalLightEnabled = true;
    GpuRenderScene scene;
    scene.directionalLight.direction = {0, 0, -1};
    const Rendering::RenderPassContext context{camera, settings, frame, {}, glm::mat4(1), {64, 64}};
    const Rendering::ShadowMapView shadows{*targets.directionalShadow, targets.pointShadows};
    glClearColor(0, 0, 0, 0);
    glClearDepth(1);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    ModelImportFixtures fixture;
    fixture.triangle();
    auto cpuModel = std::make_shared<Model>(fixture.path());
    auto model = std::make_shared<GpuModel>(*cpuModel, assets);
    importRequire(model->isValid() && model->meshCount() == 1 && model->data().draws.size() == 2,
                  "GPU meshes should not be duplicated for node instances");
    scene.objects.push_back({model});
    auto reloadModel = [&]() {
        if (!cpuModel->reload(fixture.path())) return false;
        model = std::make_shared<GpuModel>(*cpuModel, assets, true);
        scene.objects[0].model = model;
        return true;
    };
    gbuffer.execute(scene, context, *targets.gbuffer, *plane);
    for (int x : {16, 48})
    {
        const auto position = pixel(*targets.gbuffer, 0, x, 40);
        const auto albedo = pixel(*targets.gbuffer, 2, x, 40);
        const auto orm = pixel(*targets.gbuffer, 3, x, 40);
        importRequire(std::abs(position[0] - (x == 16 ? -.5f : .5f)) < .04f && std::abs(position[1] - .25f) < .04f,
                      "GPU node transform / mirrored culling");
        importRequire(std::abs(albedo[0] - .8f) < .005f && std::abs(albedo[1] - .2f) < .005f,
                      "untextured base color factor");
        importRequire(std::abs(orm[1] - .7f) < .005f && std::abs(orm[2] - .2f) < .005f, "PBR factors");
    }
    forward.execute(scene, context, shadows, *targets.hdr, *plane);
    importRequire(pixel(*targets.hdr, 0, 16, 40)[0] > .02f && pixel(*targets.hdr, 0, 48, 40)[0] > .02f,
                  "forward path lost a node instance");
    glBindFramebuffer(GL_FRAMEBUFFER, targets.directionalShadow->getFBO());
    glClear(GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, 64, 64);
    auto depthShader = assets.GetShader(ShaderId::DirectionalShadow);
    depthShader->use();
    depthShader->setUniform("lightSpaceMatrix", glm::mat4(1));
    Rendering::renderModel(glm::mat4(1), *model, *depthShader);
    importRequire(depth(*targets.directionalShadow, 16, 40) < .9f && depth(*targets.directionalShadow, 48, 40) < .9f,
                  "shadow helper lost node transforms");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    GLint winding = 0;
    glGetIntegerv(GL_FRONT_FACE, &winding);
    importRequire(winding == GL_CCW && glIsEnabled(GL_CULL_FACE), "mesh draw leaked culling state");

    fixture.triangle(R"({"pbrMetallicRoughness":{"baseColorFactor":[1,0.1,0.1,0.25],"metallicFactor":0},
                         "alphaMode":"MASK","alphaCutoff":0.5,"doubleSided":true})",
                     false);
    importRequire(reloadModel(), "reload mask model");
    gbuffer.execute(scene, context, *targets.gbuffer, *plane);
    importRequire(depth(*targets.gbuffer) > .99f, "masked fragments wrote GBuffer depth");
    forward.execute(scene, context, shadows, *targets.hdr, *plane);
    importRequire(depth(*targets.hdr) > .99f, "masked fragments wrote forward depth");
    settings.shadows = true;
    shadow.execute(scene, context, shadows, *plane);
    importRequire(depth(*targets.directionalShadow) > .99f, "masked fragments cast solid shadows");
    settings.shadows = false;

    fixture.triangle(
        R"({"pbrMetallicRoughness":{"baseColorFactor":[0.8,0.2,0.1,1],"metallicFactor":0},"doubleSided":true})", false);
    importRequire(reloadModel(), "reload opaque model");
    // A back-facing triangle must still render when the material is double-sided.
    scene.objects[0].transform = glm::rotate(glm::mat4(1), 3.14159265f, glm::vec3(0, 1, 0));
    gbuffer.execute(scene, context, *targets.gbuffer, *plane);
    importRequire(depth(*targets.gbuffer) < .9f, "double-sided material was culled");
    scene.objects[0].transform = glm::mat4(1);
    scene.objects[0].material.roughnessBias = -.2f;
    scene.objects[0].material.metallicBias = .3f;
    scene.objects[0].material.aoBias = -.25f;
    gbuffer.execute(scene, context, *targets.gbuffer, *plane);
    const auto biased = pixel(*targets.gbuffer, 3);
    importRequire(std::abs(biased[0] - .75f) < .005f &&
                      std::abs(biased[1] - (model->material(0).roughness - .2f)) < .005f &&
                      std::abs(biased[2] - .3f) < .005f, "object PBR biases ignored");
    scene.objects[0].material = {};
    forward.execute(scene, context, shadows, *targets.hdr, *plane);
    const auto opaque = pixel(*targets.hdr, 0);
    fixture.triangle(R"({"pbrMetallicRoughness":{"baseColorFactor":[0.8,0.2,0.1,0.5],"metallicFactor":0},
                         "alphaMode":"BLEND","doubleSided":true})",
                     false);
    importRequire(reloadModel(), "reload blended model");
    for (bool deferred : {false, true})
    {
        settings.deferred = deferred;
        auto& target = deferred ? *targets.deferredLighting : *targets.hdr;
        if (deferred)
        {
            gbuffer.execute(scene, context, *targets.gbuffer, *plane);
            importRequire(depth(*targets.gbuffer) > .99f, "blend must not enter GBuffer");
            lighting.execute(scene, context, shadows, *targets.gbuffer, target, *quad);
        }
        else
            forward.execute(scene, context, shadows, target, *plane);
        const auto before = pixel(target, 0);
        const float oldDepth = depth(target);
        forward.execute(scene, context, shadows, target, *plane, Rendering::ForwardPhase::Transparent);
        const auto after = pixel(target, 0);
        importRequire(std::abs(after[0] - (opaque[0] + before[0]) * .5f) < .005f,
                      "transparent forward overlay did not alpha-composite in linear light");
        importRequire(std::abs(depth(target) - oldDepth) < .001f && !glIsEnabled(GL_BLEND),
                      "transparent pass wrote depth or leaked blend state");
    }
    const auto vao = model->mesh(0).getVAO();
    fixture.text("triangle.gltf", "broken");
    importRequire(!reloadModel() && model->mesh(0).getVAO() == vao,
                  "failed reload must preserve old GPU model");
    GpuModel embedded(Model(root + "/third_party/assimp/test/models/glTF2/BoxTextured-glTF-Embedded/BoxTextured.gltf"), assets);
    bool embeddedTexture = false;
    for (std::size_t i = 0; i < embedded.meshCount(); ++i)
        for (const auto& texture : embedded.mesh(i).getTexture())
            embeddedTexture |= texture->getType() == Diffuse && texture->isValid();
    importRequire(embeddedTexture, "embedded image was not uploaded");
    GpuModel binary(Model(root + "/third_party/assimp/test/models/glTF2/BoxTextured-glTF-Binary/BoxTextured.glb"), assets);
    bool binaryTexture = false;
    for (std::size_t i = 0; i < binary.meshCount(); ++i)
        for (const auto& texture : binary.mesh(i).getTexture())
            binaryTexture |= texture->getType() == Diffuse && texture->isValid();
    importRequire(binaryTexture, "GLB binary image was not uploaded");
    const auto unicodeDir = fixture.directory / std::filesystem::u8path(u8"\u4e2d\u6587 \u8d34\u56fe");
    std::filesystem::create_directory(unicodeDir);
    const auto unicodeImage = unicodeDir / std::filesystem::u8path(u8"\u989c\u8272.tga");
    {
        std::ofstream stream(unicodeImage, std::ios::binary);
        unsigned char header[18]{};
        header[2] = 2; header[12] = 1; header[14] = 1; header[16] = 24;
        const unsigned char color[] = {0, 0, 255};
        stream.write(reinterpret_cast<const char*>(header), sizeof(header));
        stream.write(reinterpret_cast<const char*>(color), sizeof(color));
    }
    auto unicodeTexture = assets.LoadTexture(unicodeImage.u8string(), Diffuse);
    importRequire(unicodeTexture && unicodeTexture->isValid(), "Unicode external image upload");
    const auto unicodeFbx = unicodeDir / std::filesystem::u8path(u8"\u89d2\u8272.FBX");
    std::filesystem::copy_file(root + "/third_party/assimp/test/models/FBX/embedded_ascii/box.FBX", unicodeFbx);
    GpuModel fbx(Model(unicodeFbx.u8string()), assets);
    importRequire(fbx.isValid() && fbx.mesh(0).getIndexCount() > 0, "Unicode FBX GPU upload");
    const std::vector<unsigned char> rgba = {255, 0, 0, 255, 0, 255, 0, 0};
    Texture raw("raw embedded image", rgba, 2, 1, Diffuse, ColorSpace::SRGB);
    importRequire(raw.isValid() && raw.hasTransparency(), "raw embedded RGBA/alpha decode");
    importRequire(glGetError() == GL_NO_ERROR, "model import/render generated an OpenGL error");
}
} // namespace
int main()
{
    GLFWwindow* window = nullptr;
    try
    {
        importRequire(glfwInit(), "GLFW init");
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        window = glfwCreateWindow(64, 64, "Model import tests", nullptr, nullptr);
        importRequire(window != nullptr, "GL context");
        glfwMakeContextCurrent(window);
        importRequire(gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)), "GL loader");
        run();
        glfwDestroyWindow(window);
        glfwTerminate();
        std::cout << "GPU model import tests passed\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        if (window)
            glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
}
