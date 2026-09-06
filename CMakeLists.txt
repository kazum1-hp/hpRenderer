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

# ================= RUNTIME / EDITOR BOUNDARY =================
file(GLOB CORE_SOURCES CONFIGURE_DEPENDS src/*.cpp)
list(REMOVE_ITEM CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Application.cpp")
file(GLOB EDITOR_SOURCES CONFIGURE_DEPENDS src/Editor/*.cpp)
file(GLOB_RECURSE HEADER_FILES CONFIGURE_DEPENDS include/*.h)
file(GLOB SHADER_FILES shaders/*)

# No ImGui or file-dialog include/link dependency is propagated to the runtime.
file(GLOB RENDER_PASS_SOURCES CONFIGURE_DEPENDS src/Rendering/*.cpp)
add_library(hpRenderer_core STATIC ${CORE_SOURCES} ${RENDER_PASS_SOURCES})
target_include_directories(hpRenderer_core PUBLIC
    include
    third_party/glm
    third_party/stb
)
target_link_libraries(hpRenderer_core PUBLIC glfw glad assimp opengl32)

add_library(hpRenderer_editor STATIC ${EDITOR_SOURCES})
target_link_libraries(hpRenderer_editor PUBLIC hpRenderer_core PRIVATE imgui ImGuiFileDialog)

add_executable(hpRenderer src/main.cpp src/Application.cpp ${HEADER_FILES} ${SHADER_FILES})
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
    add_executable(hpRenderer_scene_tests
        tests/SceneBehaviorTests.cpp
        src/Light.cpp
    )

    target_include_directories(hpRenderer_scene_tests PRIVATE
        include
        third_party/glad/include
        third_party/glfw/include
        third_party/glm
    )

    target_link_libraries(hpRenderer_scene_tests PRIVATE
        assimp
    )

    add_test(NAME scene_behavior COMMAND hpRenderer_scene_tests)

    add_executable(hpRenderer_environment_tests tests/EnvironmentAssetTests.cpp)
    target_include_directories(hpRenderer_environment_tests PRIVATE include)
    add_test(NAME environment_asset COMMAND hpRenderer_environment_tests)

    add_executable(hpRenderer_ibl_tests tests/IBLTests.cpp)
    target_link_libraries(hpRenderer_ibl_tests PRIVATE hpRenderer_core)
    target_compile_definitions(hpRenderer_ibl_tests PRIVATE HPRENDERER_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

    add_executable(hpRenderer_runtime_tests tests/RuntimeBoundaryTests.cpp src/Editor/ConsoleCapture.cpp)
    target_link_libraries(hpRenderer_runtime_tests PRIVATE hpRenderer_core)
    add_test(NAME runtime_boundary COMMAND hpRenderer_runtime_tests)

    add_executable(hpRenderer_submission_tests tests/RenderSubmissionTests.cpp)
    target_link_libraries(hpRenderer_submission_tests PRIVATE hpRenderer_core)
    add_test(NAME render_submission COMMAND hpRenderer_submission_tests)

    add_executable(hpRenderer_renderer_gpu_tests tests/RendererGpuTests.cpp)
    target_link_libraries(hpRenderer_renderer_gpu_tests PRIVATE hpRenderer_core)
    target_compile_definitions(hpRenderer_renderer_gpu_tests PRIVATE HPRENDERER_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

    add_executable(hpRenderer_editor_tests tests/EditorTests.cpp)
    target_link_libraries(hpRenderer_editor_tests PRIVATE hpRenderer_editor imgui)
    add_test(NAME editor_panels COMMAND hpRenderer_editor_tests)

    add_executable(hpRenderer_framebuffer_tests
        tests/FramebufferTests.cpp
        src/FrameBuffer.cpp
        src/RenderTargets.cpp
    )
    target_include_directories(hpRenderer_framebuffer_tests PRIVATE include)
    target_link_libraries(hpRenderer_framebuffer_tests PRIVATE glad glfw opengl32)
    add_test(NAME framebuffer_descriptors COMMAND hpRenderer_framebuffer_tests)
    # Explicit opt-in for machines with an OpenGL 3.3-capable graphics driver.
    option(HPRENDERER_GPU_TESTS "Run hidden-context OpenGL framebuffer, editor and IBL tests with CTest" OFF)
    if(HPRENDERER_GPU_TESTS)
        add_test(NAME framebuffer_gpu COMMAND hpRenderer_framebuffer_tests --gpu)
        add_test(NAME editor_gpu COMMAND hpRenderer_editor_tests --gpu)
        add_test(NAME ibl_gpu COMMAND hpRenderer_ibl_tests)
        add_test(NAME renderer_gpu COMMAND hpRenderer_renderer_gpu_tests)
    endif()
endif()
