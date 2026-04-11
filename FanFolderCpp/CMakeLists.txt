cmake_minimum_required(VERSION 3.20)
project(FanFolderCpp CXX)
set(CMAKE_CXX_STANDARD 20)

add_executable(FanFolderCpp WIN32
    src/main.cpp
    src/Config.cpp
    src/FileService.cpp
    src/FanWindow.cpp
    src/MainWindow.cpp
    src/ShellDrag.cpp
    resources/app.rc
)

target_include_directories(FanFolderCpp PRIVATE src resources)
target_precompile_headers(FanFolderCpp PRIVATE src/pch.h)

target_link_libraries(FanFolderCpp PRIVATE
    shell32 user32 gdi32 ole32 oleaut32 comctl32 dwmapi shlwapi
    uxtheme comdlg32 gdiplus
)

target_compile_definitions(FanFolderCpp PRIVATE
    WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
)

set_target_properties(FanFolderCpp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
)
