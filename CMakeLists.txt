cmake_minimum_required(VERSION 3.16)
project(MaxwellCatProject CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Wymagane pakiety (zainstaluj przez vcpkg, patrz README.md):
#   vcpkg install glfw3 glew glm imgui[glfw-binding,opengl3-binding] stb
find_package(glfw3 CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm CONFIG REQUIRED)
# Prefer building bundled ImGui for MinGW (vcpkg's prebuilt imgui may be MSVC-only).
# To use the bundled ImGui, place ImGui sources under external/imgui and backends in external/imgui/backends
if (EXISTS "${CMAKE_SOURCE_DIR}/external/imgui")
    message(STATUS "Building bundled ImGui from sources")
    file(GLOB IMGUI_SOURCES
        "${CMAKE_SOURCE_DIR}/external/imgui/*.cpp"
        "${CMAKE_SOURCE_DIR}/external/imgui/backends/imgui_impl_glfw.cpp"
        "${CMAKE_SOURCE_DIR}/external/imgui/backends/imgui_impl_opengl3.cpp"
    )
    add_library(imgui STATIC ${IMGUI_SOURCES})
    target_include_directories(imgui PUBLIC
        "${CMAKE_SOURCE_DIR}/external/imgui"
        "${CMAKE_SOURCE_DIR}/external/imgui/backends"
    )
    target_compile_definitions(imgui PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLEW)
    target_link_libraries(imgui PRIVATE glfw GLEW::GLEW)
else()
    find_package(imgui CONFIG REQUIRED)
endif()
find_package(Stb REQUIRED)

add_executable(MaxwellCat
    src/main.cpp
)

target_include_directories(MaxwellCat PRIVATE src ${Stb_INCLUDE_DIR})

target_link_libraries(MaxwellCat PRIVATE
    glfw
    GLEW::GLEW
    glm::glm
    # Link the built 'imgui' target when present (bundled sources), otherwise use the system/package imgui::imgui target
    $<$<TARGET_EXISTS:imgui>:imgui>
    $<$<NOT:$<TARGET_EXISTS:imgui>>:imgui::imgui>
    opengl32
)

# Kopiowanie shaderow i tekstur obok pliku wykonywalnego
add_custom_command(TARGET MaxwellCat POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:MaxwellCat>/shaders
)
add_custom_command(TARGET MaxwellCat POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/textures $<TARGET_FILE_DIR:MaxwellCat>/textures
)
