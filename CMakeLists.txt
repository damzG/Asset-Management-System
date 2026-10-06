cmake_minimum_required(VERSION 3.20)
project(asset_management_system C)

set(CMAKE_C_STANDARD 99)

include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

add_executable(${PROJECT_NAME} main.c)
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)

# Windows: no console window in release builds (optional)
# if (WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
#     target_link_options(${PROJECT_NAME} PRIVATE -mwindows)
# endif()