if(NOT DEFINED SDL_SOURCE_DIR)
    message(FATAL_ERROR "SDL_SOURCE_DIR is required")
endif()

set(SDL_CMAKE_FILE "${SDL_SOURCE_DIR}/CMakeLists.txt")
set(SDL_TIMER_FILE "${SDL_SOURCE_DIR}/src/timer/SDL_timer.c")

file(READ "${SDL_CMAKE_FILE}" SDL_CMAKE_CONTENT)
set(SDL_WINMM_LINK
    "sdl_link_dependency(base LIBS kernel32 user32 gdi32 winmm imm32 ole32 oleaut32 version uuid advapi32 setupapi shell32)")
set(SDL_PORTABLE_LINK
    "sdl_link_dependency(base LIBS kernel32 user32 gdi32 imm32 ole32 oleaut32 version uuid advapi32 setupapi shell32)")
string(FIND "${SDL_CMAKE_CONTENT}" "${SDL_WINMM_LINK}" SDL_WINMM_POSITION)
string(FIND "${SDL_CMAKE_CONTENT}" "${SDL_PORTABLE_LINK}" SDL_PORTABLE_POSITION)
if(NOT SDL_WINMM_POSITION EQUAL -1)
    string(REPLACE "${SDL_WINMM_LINK}" "${SDL_PORTABLE_LINK}"
        SDL_CMAKE_CONTENT "${SDL_CMAKE_CONTENT}")
    file(WRITE "${SDL_CMAKE_FILE}" "${SDL_CMAKE_CONTENT}")
elseif(SDL_PORTABLE_POSITION EQUAL -1)
    message(FATAL_ERROR "Pinned SDL WinMM link declaration changed")
endif()

file(READ "${SDL_TIMER_FILE}" SDL_TIMER_CONTENT)
set(SDL_TIMER_GUARD
    "#if defined(SDL_TIMER_WINDOWS) && !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)")
set(TOU_TIMER_GUARD
    "#if 0 && defined(SDL_TIMER_WINDOWS) /* TOU: use waitable timers without WinMM's global timer-period API */")
string(FIND "${SDL_TIMER_CONTENT}" "${SDL_TIMER_GUARD}" SDL_TIMER_POSITION)
string(FIND "${SDL_TIMER_CONTENT}" "${TOU_TIMER_GUARD}" TOU_TIMER_POSITION)
if(NOT SDL_TIMER_POSITION EQUAL -1)
    string(REPLACE "${SDL_TIMER_GUARD}" "${TOU_TIMER_GUARD}"
        SDL_TIMER_CONTENT "${SDL_TIMER_CONTENT}")
    file(WRITE "${SDL_TIMER_FILE}" "${SDL_TIMER_CONTENT}")
elseif(TOU_TIMER_POSITION EQUAL -1)
    message(FATAL_ERROR "Pinned SDL timer-resolution guard changed")
endif()
