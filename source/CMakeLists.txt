if(NOT DEFINED TOU_REPOSITORY_ROOT)
    message(FATAL_ERROR "Configure from the repository root, not source/: cmake -S . -B build")
endif()

include(FetchContent)

set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

if(MSVC)
    # Release archives are self-contained and must not require the VC runtime.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

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
    PATCH_COMMAND
        ${CMAKE_COMMAND}
        -DSDL_SOURCE_DIR=<SOURCE_DIR>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/patch-sdl-no-winmm.cmake
    COMMAND ${CMAKE_COMMAND}
        -DSDL_SOURCE_DIR=<SOURCE_DIR>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/patch-sdl-xbox-ble.cmake
)
FetchContent_MakeAvailable(SDL3)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SDLMIXER_VENDORED ON CACHE BOOL "" FORCE)
set(SDLMIXER_DEPS_SHARED OFF CACHE BOOL "" FORCE)
set(SDLMIXER_TESTS OFF CACHE BOOL "" FORCE)
set(SDLMIXER_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDLMIXER_AIFF OFF CACHE BOOL "" FORCE)
set(SDLMIXER_VOC OFF CACHE BOOL "" FORCE)
set(SDLMIXER_AU OFF CACHE BOOL "" FORCE)
set(SDLMIXER_FLAC OFF CACHE BOOL "" FORCE)
set(SDLMIXER_GME OFF CACHE BOOL "" FORCE)
set(SDLMIXER_MOD OFF CACHE BOOL "" FORCE)
set(SDLMIXER_MP3 ON CACHE BOOL "" FORCE)
set(SDLMIXER_MP3_DRMP3 ON CACHE BOOL "" FORCE)
set(SDLMIXER_MP3_MPG123 OFF CACHE BOOL "" FORCE)
set(SDLMIXER_MIDI OFF CACHE BOOL "" FORCE)
set(SDLMIXER_OPUS OFF CACHE BOOL "" FORCE)
set(SDLMIXER_VORBIS_STB ON CACHE BOOL "" FORCE)
set(SDLMIXER_VORBIS_VORBISFILE OFF CACHE BOOL "" FORCE)
set(SDLMIXER_VORBIS_TREMOR OFF CACHE BOOL "" FORCE)
set(SDLMIXER_WAVPACK OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL3_mixer
    URL https://github.com/libsdl-org/SDL_mixer/releases/download/release-3.2.4/SDL3_mixer-3.2.4.tar.gz
    URL_HASH SHA256=182a07c745375e113dc740d43964ff21b0be29f29f59876c4dbc4db3d32f6901
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(SDL3_mixer)

add_subdirectory(level-editor)

set(TOU_SOURCES
    game/main.cpp
    game/sound.cpp
    platform/audio_sdl.cpp
    game/assets.cpp
    game/gameloop.cpp
    game/memory.cpp
    game/init.cpp
    platform/input_sdl.cpp
    game/input_actions.cpp
    platform/gamepad.cpp
    game/simulation_state.cpp
    game/netplay.cpp
    game/replay.cpp
    game/graphics.cpp
    platform/platform_sdl.cpp
    platform/render_backend.cpp
    platform/gfx_sdl.cpp
    platform/gfx_shader.cpp
    game/shader_effects.cpp
    game/intro.cpp
    game/menu.cpp
    game/level.cpp
    game/math.cpp
    game/sim.cpp
    game/utils.cpp
    game/effects.cpp
    game/entity.cpp
    game/hud.cpp
    game/gg_gen.cpp
    game/original_semantics.cpp
    game/entity_callbacks.cpp
    game/settings.cpp
    game/localization.cpp
    third_party/stb/stb_image.c
)

function(tou_embed_shader path output)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/${path}" shader)
    string(LENGTH "${shader}" size)
    set(chunks "")
    set(offset 0)
    while(offset LESS size)
        # Separate array entries, not concatenated C++ literals: MSVC limits
        # individual strings even when the shader is only runtime source.
        string(SUBSTRING "${shader}" ${offset} 16000 chunk)
        string(APPEND chunks "R\"FSR_SOURCE(${chunk})FSR_SOURCE\",\n")
        math(EXPR offset "${offset} + 16000")
    endwhile()
    set(${output} "${chunks}" PARENT_SCOPE)
endfunction()
tou_embed_shader(third_party/fsr/ffx_a.h TOU_FSR_PORTABILITY)
tou_embed_shader(third_party/fsr/ffx_fsr1.h TOU_FSR_ALGORITHM)
tou_embed_shader(third_party/gaussian_blur/13.glsl TOU_GAUSSIAN_BLUR)
configure_file(cmake/fsr_sources.h.in generated/fsr_sources.h @ONLY)
include_directories("${CMAKE_CURRENT_BINARY_DIR}/generated")

if(APPLE)
    set_source_files_properties(resources/icon.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    list(APPEND TOU_SOURCES resources/icon.icns)
endif()

if(WIN32)
    enable_language(RC)
    list(APPEND TOU_SOURCES resources/tou.rc)
    add_executable(TOU WIN32 ${TOU_SOURCES})
elseif(APPLE)
    add_executable(TOU MACOSX_BUNDLE ${TOU_SOURCES})
else()
    add_executable(TOU ${TOU_SOURCES})
endif()
target_compile_features(TOU PRIVATE cxx_std_11)
target_include_directories(TOU PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/game"
    "${CMAKE_CURRENT_SOURCE_DIR}/platform"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources"
)
if(MSVC)
    target_compile_definitions(TOU PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(TOU PRIVATE /W4 /WX)
else()
    # The original MSVC build uses signed plain char. ARM targets commonly
    # default to unsigned char, which changes recovered byte-state semantics.
    target_compile_options(TOU PRIVATE
        -fsigned-char
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()
target_link_libraries(TOU PRIVATE
    SDL3::SDL3
    SDL3_mixer::SDL3_mixer-static
    nlohmann_json::nlohmann_json
)
if(WIN32)
    target_link_libraries(TOU PRIVATE ws2_32)
endif()

set_target_properties(TOU PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TOU_REPOSITORY_ROOT}"
)
foreach(TOU_CONFIG DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(TOU PROPERTIES
        "RUNTIME_OUTPUT_DIRECTORY_${TOU_CONFIG}" "${TOU_REPOSITORY_ROOT}"
    )
endforeach()

if(APPLE)
    set_target_properties(TOU PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "Tunnels of Underworld"
        MACOSX_BUNDLE_GUI_IDENTIFIER "fi.iobox.tou"
        MACOSX_BUNDLE_ICON_FILE "icon.icns"
        MACOSX_BUNDLE_BUNDLE_VERSION "0.9"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.9"
    )
endif()

install(TARGETS TOU
    RUNTIME DESTINATION .
    BUNDLE DESTINATION .
)
set(TOU_RUNTIME_DIRECTORIES)
foreach(directory data ggstuff help lang levels music sfx ships swap)
    list(APPEND TOU_RUNTIME_DIRECTORIES "${TOU_REPOSITORY_ROOT}/${directory}")
endforeach()
if(APPLE)
    install(DIRECTORY ${TOU_RUNTIME_DIRECTORIES}
        DESTINATION "TOU.app/Contents/Resources"
    )
    install(FILES third_party/fsr/LICENSE.txt
        DESTINATION "TOU.app/Contents/Resources" RENAME TOU-FSR-LICENSE.txt)
    install(FILES third_party/gaussian_blur/LICENSE.md
        DESTINATION "TOU.app/Contents/Resources" RENAME TOU-BLOOM-LICENSE.txt)
    install(FILES "${TOU_REPOSITORY_ROOT}/THIRD_PARTY_NOTICES.md" DESTINATION "TOU.app/Contents/Resources")
else()
    install(DIRECTORY ${TOU_RUNTIME_DIRECTORIES}
        DESTINATION .
    )
    install(FILES third_party/fsr/LICENSE.txt DESTINATION . RENAME TOU-FSR-LICENSE.txt)
    install(FILES third_party/gaussian_blur/LICENSE.md DESTINATION . RENAME TOU-BLOOM-LICENSE.txt)
    install(FILES "${TOU_REPOSITORY_ROOT}/THIRD_PARTY_NOTICES.md" DESTINATION .)
endif()
