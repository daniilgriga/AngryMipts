cmake_minimum_required(VERSION 3.20)
project(AngryMipts LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependencies
find_package(SFML 3 COMPONENTS Graphics Window System REQUIRED)

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

# Main executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/ui/scene_manager.cpp
    src/ui/menu_scene.cpp
    src/ui/game_scene.cpp
    src/render/renderer.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_libraries(${PROJECT_NAME} PRIVATE
    angry_data
    SFML::Graphics
    SFML::Window
    SFML::System
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
