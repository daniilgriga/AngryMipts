cmake_minimum_required(VERSION 3.20)
project(AngryMipts LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT EMSCRIPTEN)
    enable_testing()
endif()

# Dependencies
include(FetchContent)

if(EMSCRIPTEN)
    # Web build: Raylib (replaces SFML)
    set(PLATFORM Web CACHE STRING "" FORCE)
    # Use OpenGL ES 3.0 / WebGL 2 — avoids glVertexAttribIPointer warnings that
    # appear with the ES2 backend when rlgl tries to bind integer vertex attributes
    # not present in Raylib's default 2D shader.
    set(OPENGL_VERSION "ES 3.0" CACHE STRING "" FORCE)
    FetchContent_Declare(
        raylib
        URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP OFF
    )
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(raylib)
else()
    # Native build: SFML + cpr
    find_package(SFML 3 COMPONENTS Graphics Window System Audio REQUIRED)

    FetchContent_Declare(
        cpr
        GIT_REPOSITORY https://github.com/libcpr/cpr.git
        GIT_TAG 1.10.4
    )
    FetchContent_MakeAvailable(cpr)
endif()

set(BOX2D_UNIT_TESTS OFF CACHE BOOL "Disable Box2D unit tests" FORCE)
set(BOX2D_SAMPLES OFF CACHE BOOL "Disable Box2D samples" FORCE)
set(BOX2D_BENCHMARKS OFF CACHE BOOL "Disable Box2D benchmarks" FORCE)
set(BOX2D_DOCS OFF CACHE BOOL "Disable Box2D docs" FORCE)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/box2d/CMakeLists.txt")
    message(STATUS "Using vendored external/box2d (expected API: v3.0.0)")
    add_subdirectory(external/box2d)
else()
    if(EMSCRIPTEN)
        message(STATUS "Fetching box2d v3.0.0 from release archive")
        FetchContent_Declare(
            box2d
            URL https://github.com/erincatto/box2d/archive/refs/tags/v3.0.0.tar.gz
            DOWNLOAD_EXTRACT_TIMESTAMP OFF
        )
    else()
        message(STATUS "Fetching box2d v3.0.0 from GitHub")
        FetchContent_Declare(
            box2d
            GIT_REPOSITORY https://github.com/erincatto/box2d.git
            GIT_TAG v3.0.0
        )
    endif()
    FetchContent_MakeAvailable(box2d)
endif()

if(TARGET box2d::box2d)
    set(ANGRY_BOX2D_TARGET box2d::box2d)
elseif(TARGET box2d)
    set(ANGRY_BOX2D_TARGET box2d)
else()
    message(FATAL_ERROR "Failed to configure Box2D v3.0.0 target")
endif()

if(EMSCRIPTEN)
    # Use target-owned JSON dependency for cross-builds (avoid host include paths).
    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP OFF
    )
    set(JSON_BuildTests OFF CACHE INTERNAL "")
    FetchContent_MakeAvailable(nlohmann_json)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/nlohmann_json/single_include/nlohmann/json.hpp")
    add_library(nlohmann_json INTERFACE)
    target_include_directories(nlohmann_json INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/external/nlohmann_json/single_include"
    )
    add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
    message(STATUS "Using vendored nlohmann_json from external/")
else()
    find_package(nlohmann_json REQUIRED)
    message(STATUS "Found nlohmann_json package")
endif()

if(NOT EMSCRIPTEN)
    find_package(GTest REQUIRED)
    include(GoogleTest)
endif()

# Data library (C)
add_library(angry_data
    src/data/account_service.cpp
    src/data/auth_client.cpp
    src/data/level_loader.cpp
    src/data/logger.cpp
    src/data/score_saver.cpp
    src/data/online_score_client.cpp
    src/data/session_manager.cpp
)
target_include_directories(angry_data PUBLIC src)
if(EMSCRIPTEN)
    if(TARGET nlohmann_json::nlohmann_json)
        target_link_libraries(angry_data PUBLIC nlohmann_json::nlohmann_json)
    endif()
elseif(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(angry_data PUBLIC nlohmann_json::nlohmann_json cpr::cpr)
else()
    target_link_libraries(angry_data PUBLIC cpr::cpr)
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
    src/ui/login_scene.cpp
    src/ui/menu_scene.cpp
    src/ui/game_scene.cpp
    src/ui/view_utils.cpp
    src/ui/slingshot.cpp
    src/ui/result_scene.cpp
    src/ui/level_select_scene.cpp
    src/render/renderer.cpp
    src/render/texture_manager.cpp
    src/render/particles.cpp
    src/render/sfx_player.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE src)

if(EMSCRIPTEN)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        angry_data
        angry_physics
        raylib
    )
    set(ASSETS_DIR "${CMAKE_SOURCE_DIR}/assets")
    set(LEVELS_DIR "${CMAKE_SOURCE_DIR}/levels")
    set(WEB_SHELL_FILE "${CMAKE_SOURCE_DIR}/web/shell.html")
    target_link_options(${PROJECT_NAME} PRIVATE
        -sUSE_GLFW=3
        -sFETCH=1
        -sASYNCIFY=1
        -sALLOW_MEMORY_GROWTH=1
        -sMAX_WEBGL_VERSION=2
        -sEXPORTED_RUNTIME_METHODS=cwrap
        -sFORCE_FILESYSTEM=1
        "SHELL:--preload-file ${ASSETS_DIR}@/assets"
        "SHELL:--preload-file ${LEVELS_DIR}@/levels"
        -sASSERTIONS=1
    )
    if(EXISTS "${WEB_SHELL_FILE}")
        target_link_options(${PROJECT_NAME} PRIVATE
            "SHELL:--shell-file ${WEB_SHELL_FILE}"
        )
    endif()
    set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".html")
else()
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
endif()

if(NOT EMSCRIPTEN)
    # Smoke test (C)
    add_executable(angry_data_smoke
        src/data/level_loader_smoke.cpp
    )
    target_include_directories(angry_data_smoke PRIVATE src)
    target_link_libraries(angry_data_smoke PRIVATE angry_data)
    target_compile_definitions(angry_data_smoke PRIVATE
        ANGRY_MIPTS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_test(NAME angry_data_smoke COMMAND angry_data_smoke)

    # UI/Render smoke tests (B)
    add_executable(angry_ui_render_tests
        tests/ui_render_tests.cpp
        src/render/particles.cpp
        src/ui/view_utils.cpp
    )
    target_include_directories(angry_ui_render_tests PRIVATE src)
    target_link_libraries(angry_ui_render_tests PRIVATE
        SFML::Graphics
        SFML::System
        GTest::gtest
        GTest::gtest_main
    )
    gtest_discover_tests(angry_ui_render_tests)

    # Data online client tests (no UI dependency)
    add_executable(angry_online_score_client_tests
        tests/online_score_client_tests.cpp
    )
    target_include_directories(angry_online_score_client_tests PRIVATE src)
    target_link_libraries(angry_online_score_client_tests PRIVATE
        angry_data
        GTest::gtest
        GTest::gtest_main
    )
    gtest_discover_tests(angry_online_score_client_tests)

    # Data session manager tests (no UI dependency)
    add_executable(angry_session_manager_tests
        tests/session_manager_tests.cpp
    )
    target_include_directories(angry_session_manager_tests PRIVATE src)
    target_link_libraries(angry_session_manager_tests PRIVATE
        angry_data
        GTest::gtest
        GTest::gtest_main
    )
    gtest_discover_tests(angry_session_manager_tests)

    # Data auth client tests (no UI dependency)
    add_executable(angry_auth_client_tests
        tests/auth_client_tests.cpp
    )
    target_include_directories(angry_auth_client_tests PRIVATE src)
    target_link_libraries(angry_auth_client_tests PRIVATE
        angry_data
        GTest::gtest
        GTest::gtest_main
    )
    gtest_discover_tests(angry_auth_client_tests)

    # Data account service tests (no UI dependency)
    add_executable(angry_account_service_tests
        tests/account_service_tests.cpp
    )
    target_include_directories(angry_account_service_tests PRIVATE src)
    target_link_libraries(angry_account_service_tests PRIVATE
        angry_data
        GTest::gtest
        GTest::gtest_main
    )
    gtest_discover_tests(angry_account_service_tests)

    # Physics engine behavior tests (A)
    add_executable(angry_physics_engine_tests
        tests/physics_engine_tests.cpp
    )
    target_include_directories(angry_physics_engine_tests PRIVATE src)
    target_link_libraries(angry_physics_engine_tests PRIVATE
        angry_physics
        GTest::gtest
        GTest::gtest_main
    )
    gtest_discover_tests(angry_physics_engine_tests)
endif()
