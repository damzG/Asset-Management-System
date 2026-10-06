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

# raygui is a single header; it has no CMake project, so we only download it
FetchContent_Declare(
        raygui
        GIT_REPOSITORY https://github.com/raysan5/raygui.git
        GIT_TAG 4.0
        GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(raylib raygui)

add_executable(${PROJECT_NAME}
        main.c
        ui.c
        store.c
        Ui.c
        Ui.c
        Ui.h
        Ui.h
        Store.c
        Store.c
        Store.h
        Models.h
)

target_include_directories(${PROJECT_NAME} PRIVATE ${raygui_SOURCE_DIR}/src)
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)