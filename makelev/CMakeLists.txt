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

add_executable(tou-level
    cli/main.cpp
)
target_compile_features(tou-level PRIVATE cxx_std_17)
target_link_libraries(tou-level PRIVATE tou_level)

if(WIN32)
    add_executable(tou-level-editor WIN32
        editor/main.cpp
        ${CMAKE_SOURCE_DIR}/stb_image.c
    )
else()
    add_executable(tou-level-editor
        editor/main.cpp
        ${CMAKE_SOURCE_DIR}/stb_image.c
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
set_target_properties(tou-level tou-level-editor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TOU_LEVEL_TOOL_OUTPUT}"
)
foreach(TOU_CONFIG DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(tou-level tou-level-editor PROPERTIES
        "RUNTIME_OUTPUT_DIRECTORY_${TOU_CONFIG}" "${TOU_LEVEL_TOOL_OUTPUT}"
    )
endforeach()

install(TARGETS tou-level tou-level-editor
    RUNTIME DESTINATION level-editor
)
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
    target_compile_options(tou-level PRIVATE /W4 /WX)
    target_compile_options(tou-level-editor PRIVATE /W4 /WX)
else()
    target_compile_options(tou_level PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(tou-level PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(tou-level-editor PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(BUILD_TESTING)
    add_executable(tou-level-tests
        tests/level_tests.cpp
    )
    target_compile_features(tou-level-tests PRIVATE cxx_std_17)
    target_compile_definitions(tou-level-tests PRIVATE
        TOU_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )
    target_link_libraries(tou-level-tests PRIVATE tou_level)
    if(MSVC)
        target_compile_options(tou-level-tests PRIVATE /W4 /WX)
    else()
        target_compile_options(tou-level-tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME tou-level-golden COMMAND tou-level-tests)
endif()
