cmake_minimum_required(VERSION 3.20)

# Ensure MinGW GCC is used on Windows to prevent MSVC SDK library errors from standalone LLVM Clang
if(WIN32)
    find_program(MINGW_GXX g++)
    if(MINGW_GXX)
        set(CMAKE_CXX_COMPILER "${MINGW_GXX}" CACHE FILEPATH "C++ compiler" FORCE)
        find_program(MINGW_GCC gcc)
        if(MINGW_GCC)
            set(CMAKE_C_COMPILER "${MINGW_GCC}" CACHE FILEPATH "C compiler" FORCE)
        endif()
    endif()
endif()

project(EnergySavior VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
    add_executable(EnergySavior WIN32
        main.cpp
        ${CORE_SOURCES}
        ${GUI_SOURCES}
    )

    target_include_directories(EnergySavior PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

    target_link_libraries(EnergySavior PRIVATE
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
        pdh
        dxgi
    )
endif()

# Automated Unit Tests Target
add_executable(EnergySavior_Tests
    tests/TestMain.cpp
    ${CORE_SOURCES}
    $<$<BOOL:${WIN32}>:src/gui/TrayManager.cpp>
)

target_include_directories(EnergySavior_Tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/tests)

if(WIN32)
    target_link_libraries(EnergySavior_Tests PRIVATE
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
        pdh
        dxgi
    )
endif()

enable_testing()
add_test(NAME UnitTests COMMAND EnergySavior_Tests)
