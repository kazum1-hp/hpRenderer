cmake_minimum_required(VERSION 3.15)

project(hpRenderer)

set(CMAKE_CXX_STANDARD 17)

include(CTest)

# ================= GLFW =================
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)

add_subdirectory(third_party/glfw)

# ================= ASSIMP =================
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(third_party/assimp)

# ================= GLAD =================
add_library(glad
    third_party/glad/src/glad.c
)

target_include_directories(glad PUBLIC
    third_party/glad/include
)

# ================= IMGUI =================
file(GLOB IMGUI_SRC
    third_party/imgui/*.cpp
    third_party/imgui/backends/imgui_impl_glfw.cpp
    third_party/imgui/backends/imgui_impl_opengl3.cpp
)

add_library(imgui ${IMGUI_SRC})

target_include_directories(imgui PUBLIC
    third_party/imgui
    third_party/imgui/backends
)

target_link_libraries(imgui glfw)

# ================= ImGuiFileDialog =================
add_library(ImGuiFileDialog
    third_party/ImGuiFileDialog/ImGuiFileDialog.cpp
)

target_include_directories(ImGuiFileDialog PUBLIC
    third_party/ImGuiFileDialog
)

target_link_libraries(ImGuiFileDialog imgui)
# Keep UTF-8 at the UI boundary and use native wide paths on Windows.
target_compile_definitions(ImGuiFileDialog PUBLIC USE_STD_FILESYSTEM)

# ================= CPU ASSETS / OPENGL / RUNTIME / EDITOR =================
# CPU decoding/import is usable without GLFW, GLAD or an OpenGL context.
add_library(hpRenderer_import STATIC
    src/assets/AssimpModelImporter.cpp
    src/assets/Model.cpp
    src/assets/ImageData.cpp
    src/assets/SkyboxAsset.cpp
    src/assets/stb_image.cpp
)
target_include_directories(hpRenderer_import PUBLIC include third_party/glm PRIVATE third_party/stb)
target_link_libraries(hpRenderer_import PUBLIC assimp)

# Concrete OpenGL resources; intentionally not a cross-API RHI.
add_library(hpRenderer_opengl STATIC
    src/renderer/opengl/FrameBuffer.cpp
    src/renderer/opengl/GpuModel.cpp
    src/renderer/opengl/Mesh.cpp
    src/renderer/opengl/PrimitiveMeshes.cpp
    src/renderer/opengl/Shader.cpp
    src/renderer/opengl/Skybox.cpp
    src/renderer/opengl/Texture.cpp
)
target_link_libraries(hpRenderer_opengl PUBLIC hpRenderer_import glad opengl32)

# Neither runtime target inherits ImGui or file-dialog dependencies.
add_library(hpRenderer_core STATIC
    src/core/Window.cpp
    src/core/InputManager.cpp
    src/scene/Light.cpp
    src/scene/Camera.cpp
    src/assets/AssetManager.cpp
    src/renderer/Renderer.cpp
    src/renderer/RenderExtraction.cpp
    src/renderer/RenderTargets.cpp
    src/renderer/RenderPipeline.cpp
    src/renderer/ModelGpuCache.cpp
    src/renderer/ibl/IBLCache.cpp
    src/renderer/ibl/IBLPrecompute.cpp
    src/renderer/passes/DrawHelpers.cpp
    src/renderer/passes/ShadowPass.cpp
    src/renderer/passes/ForwardPass.cpp
    src/renderer/passes/GBufferPass.cpp
    src/renderer/passes/DeferredLightingPass.cpp
    src/renderer/passes/LightMarkerPass.cpp
    src/renderer/passes/SkyboxPass.cpp
    src/renderer/passes/GBufferDebugPass.cpp
    src/renderer/passes/BloomPass.cpp
    src/renderer/passes/ToneMappingPass.cpp
)
target_link_libraries(hpRenderer_core PUBLIC hpRenderer_opengl glfw)

add_library(hpRenderer_editor STATIC
    src/editor/layers/EditorLayer.cpp
    src/editor/console/ConsoleCapture.cpp
    src/editor/panels/AssetPanel.cpp
    src/editor/panels/InspectorPanel.cpp
    src/editor/panels/LightingPanel.cpp
    src/editor/panels/RenderSettingsPanel.cpp
    src/editor/panels/SceneViewportPanel.cpp
    src/editor/panels/ConsolePanel.cpp
)
target_link_libraries(hpRenderer_editor PUBLIC hpRenderer_core PRIVATE imgui ImGuiFileDialog)

file(GLOB_RECURSE HEADER_FILES CONFIGURE_DEPENDS include/hpr/*.h)
file(GLOB SHADER_FILES shaders/*)
add_executable(hpRenderer src/app/main.cpp src/app/Application.cpp ${HEADER_FILES} ${SHADER_FILES})
target_link_libraries(hpRenderer PRIVATE hpRenderer_editor)
source_group("Header Files" FILES ${HEADER_FILES})
source_group("Shaders" FILES ${SHADER_FILES})

# shader
file(COPY shaders DESTINATION ${CMAKE_BINARY_DIR})
add_custom_command(TARGET hpRenderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/shaders
            ${CMAKE_BINARY_DIR}/shaders
)

# assets
file(COPY assets DESTINATION ${CMAKE_BINARY_DIR})
# ================= TESTS =================
# These tests deliberately avoid creating an OpenGL context. They protect the
# scene-level behavior that the renderer refactor will move between modules.
if(BUILD_TESTING)
    add_executable(hpRenderer_cpu_asset_tests tests/CpuAssetBoundaryTests.cpp src/scene/Light.cpp)
    target_link_libraries(hpRenderer_cpu_asset_tests PRIVATE hpRenderer_import)
    add_test(NAME cpu_asset_boundary COMMAND hpRenderer_cpu_asset_tests)
    add_executable(hpRenderer_model_import_tests tests/ModelImportTests.cpp)
    target_link_libraries(hpRenderer_model_import_tests PRIVATE hpRenderer_import)
    target_compile_definitions(hpRenderer_model_import_tests PRIVATE HPRENDERER_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME model_import COMMAND hpRenderer_model_import_tests)
    add_executable(hpRenderer_model_import_gpu_tests tests/ModelImportGpuTests.cpp)
    target_link_libraries(hpRenderer_model_import_gpu_tests PRIVATE hpRenderer_core)
    target_compile_definitions(hpRenderer_model_import_gpu_tests PRIVATE HPRENDERER_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(hpRenderer_asset_tests tests/AssetManagerTests.cpp)
    target_link_libraries(hpRenderer_asset_tests PRIVATE hpRenderer_core)
    add_test(NAME asset_cache COMMAND hpRenderer_asset_tests)
    add_executable(hpRenderer_scene_tests
        tests/SceneBehaviorTests.cpp
        src/scene/Light.cpp
    )

    target_link_libraries(hpRenderer_scene_tests PRIVATE hpRenderer_import)

    add_test(NAME scene_behavior COMMAND hpRenderer_scene_tests)

    add_executable(hpRenderer_environment_tests tests/EnvironmentAssetTests.cpp)
    target_include_directories(hpRenderer_environment_tests PRIVATE include)
    add_test(NAME environment_asset COMMAND hpRenderer_environment_tests)

    add_executable(hpRenderer_ibl_tests tests/IBLTests.cpp)
    target_link_libraries(hpRenderer_ibl_tests PRIVATE hpRenderer_core)
    target_compile_definitions(hpRenderer_ibl_tests PRIVATE HPRENDERER_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

    add_executable(hpRenderer_runtime_tests tests/RuntimeBoundaryTests.cpp src/editor/console/ConsoleCapture.cpp)
    target_link_libraries(hpRenderer_runtime_tests PRIVATE hpRenderer_core)
    add_test(NAME runtime_boundary COMMAND hpRenderer_runtime_tests)

    add_executable(hpRenderer_submission_tests tests/RenderSubmissionTests.cpp)
    target_link_libraries(hpRenderer_submission_tests PRIVATE hpRenderer_core)
    add_test(NAME render_submission COMMAND hpRenderer_submission_tests)

    add_executable(hpRenderer_renderer_gpu_tests tests/RendererGpuTests.cpp)
    target_link_libraries(hpRenderer_renderer_gpu_tests PRIVATE hpRenderer_core)
    target_compile_definitions(hpRenderer_renderer_gpu_tests PRIVATE HPRENDERER_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

    add_executable(hpRenderer_editor_tests tests/EditorTests.cpp)
    target_link_libraries(hpRenderer_editor_tests PRIVATE hpRenderer_editor imgui ImGuiFileDialog)
    add_test(NAME editor_panels COMMAND hpRenderer_editor_tests)

    add_executable(hpRenderer_framebuffer_tests
        tests/FramebufferTests.cpp
        src/renderer/opengl/FrameBuffer.cpp
        src/renderer/RenderTargets.cpp
    )
    target_include_directories(hpRenderer_framebuffer_tests PRIVATE include)
    target_link_libraries(hpRenderer_framebuffer_tests PRIVATE glad glfw opengl32)
    add_test(NAME framebuffer_descriptors COMMAND hpRenderer_framebuffer_tests)
    # Explicit opt-in for machines with an OpenGL 3.3-capable graphics driver.
    option(HPRENDERER_GPU_TESTS "Run hidden-context OpenGL framebuffer, editor and IBL tests with CTest" OFF)
    if(HPRENDERER_GPU_TESTS)
        add_test(NAME model_import_gpu COMMAND hpRenderer_model_import_gpu_tests)
        add_test(NAME asset_cache_gpu COMMAND hpRenderer_asset_tests --gpu)
        add_test(NAME framebuffer_gpu COMMAND hpRenderer_framebuffer_tests --gpu)
        add_test(NAME editor_gpu COMMAND hpRenderer_editor_tests --gpu)
        add_test(NAME ibl_gpu COMMAND hpRenderer_ibl_tests)
        add_test(NAME renderer_gpu COMMAND hpRenderer_renderer_gpu_tests)
    endif()
endif()
