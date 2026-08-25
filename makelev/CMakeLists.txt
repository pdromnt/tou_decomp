add_library(tou_level STATIC
    src/level.cpp
)
target_compile_features(tou_level PUBLIC cxx_std_17)
target_include_directories(tou_level PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(tou_level PUBLIC
    nlohmann_json::nlohmann_json
)

add_executable(tou-level-compiler
    cli/main.cpp
)
target_compile_features(tou-level-compiler PRIVATE cxx_std_17)
target_link_libraries(tou-level-compiler PRIVATE tou_level)

set(TOU_LEVEL_EDITOR_SOURCES
    editor/main.cpp
    ${CMAKE_SOURCE_DIR}/stb_image.c
)

if(WIN32)
    enable_language(RC)
    add_executable(tou-level-editor WIN32
        ${TOU_LEVEL_EDITOR_SOURCES}
        resources/tou-level-editor.rc
        resources/level-editor.ico
    )
elseif(APPLE)
    set_source_files_properties(
        resources/level-editor.icns
        resources/level-editor-icon.png
        PROPERTIES MACOSX_PACKAGE_LOCATION "Resources"
    )
    add_executable(tou-level-editor MACOSX_BUNDLE
        ${TOU_LEVEL_EDITOR_SOURCES}
        resources/level-editor.icns
        resources/level-editor-icon.png
    )
else()
    add_executable(tou-level-editor
        ${TOU_LEVEL_EDITOR_SOURCES}
    )
endif()
target_compile_features(tou-level-editor PRIVATE cxx_std_17)
target_compile_definitions(tou-level-editor PRIVATE
    TOU_ASSET_ROOT="${CMAKE_SOURCE_DIR}"
)
target_include_directories(tou-level-editor PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(tou-level-editor PRIVATE
    tou_level
    SDL3::SDL3
)

set(TOU_LEVEL_TOOL_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set_target_properties(tou-level-compiler tou-level-editor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TOU_LEVEL_TOOL_OUTPUT}"
)
if(APPLE)
    set_target_properties(tou-level-editor PROPERTIES
        OUTPUT_NAME "TOU Level Editor"
        MACOSX_BUNDLE_BUNDLE_NAME "TOU Level Editor"
        MACOSX_BUNDLE_GUI_IDENTIFIER "fi.iobox.tou.level-editor"
        MACOSX_BUNDLE_ICON_FILE "level-editor.icns"
        MACOSX_BUNDLE_BUNDLE_VERSION "0.7"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.7"
    )
endif()
foreach(TOU_CONFIG DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(tou-level-compiler tou-level-editor PROPERTIES
        "RUNTIME_OUTPUT_DIRECTORY_${TOU_CONFIG}" "${TOU_LEVEL_TOOL_OUTPUT}"
    )
endforeach()

install(TARGETS tou-level-compiler tou-level-editor
    RUNTIME DESTINATION level-editor
    BUNDLE DESTINATION level-editor
)
if(UNIX AND NOT APPLE)
    install(FILES resources/level-editor-icon.png
        DESTINATION level-editor
    )
    install(FILES resources/tou-level-editor.desktop
        DESTINATION level-editor
    )
endif()
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
    DESTINATION level-editor
)
install(FILES
    ${CMAKE_SOURCE_DIR}/docs/LEVEL_FORMAT.md
    ${CMAKE_SOURCE_DIR}/docs/LEVEL_PALETTE.json
    ${CMAKE_SOURCE_DIR}/docs/GG_LEVELS.md
    DESTINATION level-editor/docs
)

if(MSVC)
    target_compile_definitions(tou_level PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(tou_level PRIVATE /W4 /WX)
    target_compile_options(tou-level-compiler PRIVATE /W4 /WX)
    target_compile_options(tou-level-editor PRIVATE /W4 /WX)
else()
    target_compile_options(tou_level PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(tou-level-compiler PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(tou-level-editor PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
