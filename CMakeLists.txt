cmake_minimum_required(VERSION 3.15)

project(hpRenderer)

set(CMAKE_CXX_STANDARD 17)

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

# ================= MAIN PROGRAM =================
file(GLOB_RECURSE SRC_FILES src/*.cpp)
file(GLOB_RECURSE HEADER_FILES include/*.h)
file(GLOB SHADER_FILES shaders/*)

add_executable(hpRenderer
    ${SRC_FILES}
    ${HEADER_FILES}
    ${SHADER_FILES}
)

source_group("Source Files" FILES ${SRC_FILES}) 
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
# ================= include =================
target_include_directories(hpRenderer PRIVATE
    include
    src

    third_party/glad/include
    third_party/glfw/include
    third_party/glm
    third_party/stb
    third_party/imgui
    third_party/imgui/backends
    third_party/ImGuiFileDialog
    third_party/assimp/include
)

# ================= LINK =================
target_link_libraries(hpRenderer
    glfw
    glad
    imgui
    ImGuiFileDialog
    assimp
    opengl32
)
