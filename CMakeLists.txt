cmake_minimum_required(VERSION 3.24)

project(TunnelsOfUnderworld LANGUAGES C CXX RC)

include(FetchContent)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL3
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.14/SDL3-3.4.14.tar.gz
    URL_HASH SHA256=30d4aa2b3037718142b32dffd4e72f917ebb6cc5227150e7bb9c45efb2153aeb
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(SDL3)

set(TOU_SOURCES
    winmain.cpp
    sound.cpp
    assets.cpp
    gameloop.cpp
    memory.cpp
    init.cpp
    input_sdl.cpp
    graphics.cpp
    platform_sdl.cpp
    render_backend.cpp
    gfx_ddraw.cpp
    gfx_sdl.cpp
    intro.cpp
    menu.cpp
    level.cpp
    math.cpp
    sim.cpp
    utils.cpp
    effects.cpp
    entity.cpp
    hud.cpp
    gg_gen.cpp
    binary_compat.cpp
    entity_callbacks.cpp
    stb_image.c
    fmod_loader.c
    tou.rc
)

add_executable(TOU WIN32 ${TOU_SOURCES})
target_compile_features(TOU PRIVATE cxx_std_11)
target_compile_definitions(TOU PRIVATE TOU_HAS_SDL=1 SDL_MAIN_HANDLED=1)
target_include_directories(TOU PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(TOU PRIVATE
    SDL3::SDL3
    ddraw
    winmm
)

set_target_properties(TOU PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
