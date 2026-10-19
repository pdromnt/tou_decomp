cmake_minimum_required(VERSION 3.24)
project(TunnelsOfUnderworld LANGUAGES C CXX)

# Keep the public build entry point stable; runtime data remains beside the game.
set(TOU_REPOSITORY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
add_subdirectory(source)
