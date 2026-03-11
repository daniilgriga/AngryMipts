cmake_minimum_required(VERSION 3.20)
project(AngryMipts LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependencies
include(FetchContent)

find_package(SFML 3 COMPONENTS Graphics Window System Audio REQUIRED)

set(BOX2D_UNIT_TESTS OFF CACHE BOOL "Disable Box2D unit tests" FORCE)
set(BOX2D_SAMPLES OFF CACHE BOOL "Disable Box2D samples" FORCE)
set(BOX2D_BENCHMARKS OFF CACHE BOOL "Disable Box2D benchmarks" FORCE)
set(BOX2D_DOCS OFF CACHE BOOL "Disable Box2D docs" FORCE)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/box2d/CMakeLists.txt")
    message(STATUS "Using vendored external/box2d (expected API: v3.0.0)")
    add_subdirectory(external/box2d)
else()
    message(STATUS "Fetching box2d v3.0.0 from GitHub")
    FetchContent_Declare(
        box2d
        GIT_REPOSITORY https://github.com/erincatto/box2d.git
        GIT_TAG v3.0.0
    )
    FetchContent_MakeAvailable(box2d)
endif()

if(TARGET box2d::box2d)
    set(ANGRY_BOX2D_TARGET box2d::box2d)
elseif(TARGET box2d)
    set(ANGRY_BOX2D_TARGET box2d)
else()
    message(FATAL_ERROR "Failed to configure Box2D v3.0.0 target")
endif()

find_package(nlohmann_json QUIET)
if(nlohmann_json_FOUND)
    message(STATUS "Found nlohmann_json package")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/nlohmann_json/single_include/nlohmann/json.hpp")
    add_library(nlohmann_json INTERFACE)
    target_include_directories(nlohmann_json INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/external/nlohmann_json/single_include"
    )
    add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
    message(STATUS "Using vendored nlohmann_json from external/")
else()
    message(FATAL_ERROR
        "nlohmann_json not found. Install nlohmann-json3-dev or add external/nlohmann_json."
    )
endif()

# Data library (C)
add_library(angry_data
    src/data/level_loader.cpp
    src/data/logger.cpp
    src/data/score_saver.cpp
)
target_include_directories(angry_data PUBLIC src)
if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(angry_data PUBLIC nlohmann_json::nlohmann_json)
endif()

# Physics library (A)
add_library(angry_physics
    src/core/score_system.cpp
    src/physics/physics_engine.cpp
    src/physics/physics_thread.cpp
    src/physics/physics_runtime.cpp
)
target_include_directories(angry_physics PUBLIC src)
target_link_libraries(angry_physics PUBLIC ${ANGRY_BOX2D_TARGET})

# Main executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/ui/scene_manager.cpp
    src/ui/menu_scene.cpp
    src/ui/game_scene.cpp
    src/ui/slingshot.cpp
    src/ui/result_scene.cpp
    src/ui/level_select_scene.cpp
    src/render/renderer.cpp
    src/render/texture_manager.cpp
    src/render/particles.cpp
    src/render/sfx_player.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_libraries(${PROJECT_NAME} PRIVATE
    angry_data
    angry_physics
    SFML::Graphics
    SFML::Window
    SFML::System
    SFML::Audio
)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    ANGRY_MIPTS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

# Smoke test (C)
add_executable(angry_data_smoke
    src/data/level_loader_smoke.cpp
)
target_include_directories(angry_data_smoke PRIVATE src)
target_link_libraries(angry_data_smoke PRIVATE angry_data)
target_compile_definitions(angry_data_smoke PRIVATE
    ANGRY_MIPTS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)
