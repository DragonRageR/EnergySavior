cmake_minimum_required(VERSION 3.20)
project(EnergyControl VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler optimization flags (similar to cargo release)
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR NOT CMAKE_BUILD_TYPE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-O3 -flto -march=x86-64-v2 -DNDEBUG -Wall -Wno-missing-field-initializers -Wno-unused-parameter)
        add_link_options(-flto -s)
    endif()
endif()

# Core sources
file(GLOB_RECURSE CORE_SOURCES
    "src/core/*.cpp"
    "src/hardware/*.cpp"
)

# Main Application Target (Windows GUI)
if(WIN32)
    file(GLOB_RECURSE GUI_SOURCES "src/gui/*.cpp")
    add_executable(EnergyControl WIN32
        main.cpp
        ${CORE_SOURCES}
        ${GUI_SOURCES}
    )

    target_include_directories(EnergyControl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

    target_link_libraries(EnergyControl PRIVATE
        setupapi
        powrprof
        gdiplus
        gdi32
        user32
        shell32
        ole32
        crypt32
        dbghelp
        winhttp
        urlmon
    )
endif()

# Automated Unit Tests Target
add_executable(EnergyControl_Tests
    tests/TestMain.cpp
    ${CORE_SOURCES}
    $<$<BOOL:${WIN32}>:src/gui/TrayManager.cpp>
)

target_include_directories(EnergyControl_Tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/tests)

if(WIN32)
    target_link_libraries(EnergyControl_Tests PRIVATE
        setupapi
        powrprof
        gdiplus
        gdi32
        user32
        shell32
        ole32
        crypt32
        dbghelp
        winhttp
        urlmon
    )
endif()

enable_testing()
add_test(NAME UnitTests COMMAND EnergyControl_Tests)
