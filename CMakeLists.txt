cmake_minimum_required(VERSION 3.16)
project(jamoke_linux CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(SDL2IMAGE REQUIRED IMPORTED_TARGET SDL2_image)

add_library(jamoke_core STATIC
    src/Assets.cpp
    src/Assets.h
    src/Font.cpp
    src/Font.h
    src/Drinks.cpp
    src/Drinks.h
    src/Game.cpp
    src/Game.h
)

target_include_directories(jamoke_core PUBLIC
    src
)

target_link_libraries(jamoke_core PUBLIC
    PkgConfig::SDL2
    PkgConfig::SDL2IMAGE
)

target_compile_definitions(jamoke_core PUBLIC
    JAMOKE_DEFAULT_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/../jamoke/src/Data/Program_Executable_Files/DATA"
)

add_executable(jamoke
    src/main.cpp
)

target_link_libraries(jamoke PRIVATE
    jamoke_core
)

include(CTest)
if(BUILD_TESTING)
    find_package(GTest REQUIRED)
    add_executable(jamoke_tests
        tests/TestHelper.h
        tests/Main.cpp
        tests/TestDrinks.cpp
        tests/TestFont.cpp
        tests/TestAssets.cpp
        tests/TestGame.cpp
    )
    target_include_directories(jamoke_tests PRIVATE
        tests
        src
    )
    target_link_libraries(jamoke_tests PRIVATE
        jamoke_core
        GTest::gtest
    )
    add_test(NAME JamokeTests COMMAND jamoke_tests)
endif()