# Startup assets are ordinary Git files: a fresh clone can build/run offline.
option(HPRENDERER_FULL_DEMO "Start the original three-model scene and bundle all tracked demo assets" OFF)
set(HPRENDERER_STARTUP_ASSETS
    hdr/newport_loft.hdr
    models/marble_bust_01_4k/marble_bust_01_4k.gltf
    models/marble_bust_01_4k/marble_bust_01.bin
    models/marble_bust_01_4k/textures/marble_bust_01_diff_4k.jpg
    models/marble_bust_01_4k/textures/marble_bust_01_arm_4k.jpg
    models/marble_bust_01_4k/textures/marble_bust_01_nor_gl_4k.jpg
    textures/bricks2/bricks2.jpg
    textures/bricks2/bricks2_normal.jpg
    textures/bricks2/bricks2_disp.jpg
)
if(HPRENDERER_FULL_DEMO)
    target_compile_definitions(hpRenderer PRIVATE HPRENDERER_FULL_DEMO=1)
    include("${CMAKE_CURRENT_LIST_DIR}/DemoAssets.cmake")
    set(HPRENDERER_STARTUP_ASSETS ${HPRENDERER_FULL_DEMO_ASSETS})
endif()
foreach(asset IN LISTS HPRENDERER_STARTUP_ASSETS)
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/assets/${asset}")
        message(FATAL_ERROR "Required demo asset is missing: assets/${asset}. Use a complete repository checkout.")
    endif()
    get_filename_component(directory "${asset}" DIRECTORY)
    # configure_file tracks source edits; do not copy the entire assets tree on configure.
    configure_file("${PROJECT_SOURCE_DIR}/assets/${asset}" "${PROJECT_BINARY_DIR}/assets/${asset}" COPYONLY)
    install(FILES "${PROJECT_SOURCE_DIR}/assets/${asset}" DESTINATION "assets/${directory}" COMPONENT Runtime)
endforeach()

install(TARGETS hpRenderer RUNTIME DESTINATION bin COMPONENT Runtime)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/shaders/" DESTINATION shaders COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" DESTINATION . COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/cmake/PackageReadme.md" DESTINATION . RENAME README.md COMPONENT Runtime)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/docs/" DESTINATION docs COMPONENT Runtime FILES_MATCHING PATTERN "*.md")
# Preserve the separate licenses, including licenses in Assimp's compiled dependencies.
# Glob only notices, not translation units; install files without thousands of empty source directories.
file(GLOB_RECURSE dependency_notices CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/third_party/LICENSE*" "${PROJECT_SOURCE_DIR}/third_party/COPYING*"
    "${PROJECT_SOURCE_DIR}/third_party/license*" "${PROJECT_SOURCE_DIR}/third_party/copying*"
    "${PROJECT_SOURCE_DIR}/third_party/UNLICENSE")
foreach(notice IN LISTS dependency_notices)
    file(RELATIVE_PATH relative_notice "${PROJECT_SOURCE_DIR}/third_party" "${notice}")
    get_filename_component(notice_directory "${relative_notice}" DIRECTORY)
    install(FILES "${notice}" DESTINATION "licenses/third_party/${notice_directory}" COMPONENT Runtime)
endforeach()
install(FILES "${PROJECT_SOURCE_DIR}/third_party/README.md" DESTINATION licenses COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/third_party/stb/stb_image.h"
    "${PROJECT_SOURCE_DIR}/third_party/glad/include/glad/glad.h"
    "${PROJECT_SOURCE_DIR}/third_party/glad/include/KHR/khrplatform.h"
    DESTINATION licenses/embedded-notices COMPONENT Runtime)

# The MSVC runtime is required on clean Windows machines; install its redistributable files beside the EXE.
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION bin)
set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT Runtime)
include(InstallRequiredSystemLibraries)
set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME hpRenderer)
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "hpRenderer-windows-x64")
set(CPACK_COMPONENTS_ALL Runtime)
# ZIP is monolithic by default; explicitly choose Runtime instead of installing ALL components.
set(CPACK_INSTALL_CMAKE_PROJECTS "${PROJECT_BINARY_DIR};${PROJECT_NAME};Runtime;/")
include(CPack)
