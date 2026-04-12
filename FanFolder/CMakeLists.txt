cmake_minimum_required(VERSION 3.20)
project(FanFolder CXX)
set(CMAKE_CXX_STANDARD 20)

include(FetchContent)
FetchContent_Declare(
    lunasvg
    GIT_REPOSITORY https://github.com/sammycage/lunasvg.git
    GIT_TAG        v3.2.1
)
FetchContent_MakeAvailable(lunasvg)

add_executable(FanFolder WIN32
    src/main.cpp
    src/Config.cpp
    src/FileService.cpp
    src/FanWindow.cpp
    src/MainWindow.cpp
    src/ShellDrag.cpp
    src/Localization.cpp
    resources/app.rc
)

target_include_directories(FanFolder PRIVATE src resources)
target_precompile_headers(FanFolder PRIVATE src/pch.h)

target_link_libraries(FanFolder PRIVATE
    shell32 user32 gdi32 ole32 oleaut32 comctl32 dwmapi shlwapi
    uxtheme comdlg32 gdiplus lunasvg::lunasvg
)

target_compile_definitions(FanFolder PRIVATE
    WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
)

set_target_properties(FanFolder PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
)
