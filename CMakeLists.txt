cmake_minimum_required(VERSION 3.20)
project(ChromeDebloater LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ChromeDebloater WIN32
    src/main.cpp
    src/ui.cpp
    src/ui.h
    src/engine.cpp
    src/engine.h
    src/browser.h
    src/sqlite_helper.h
    src/app.rc
)

target_compile_definitions(ChromeDebloater PRIVATE UNICODE _UNICODE)

target_link_libraries(ChromeDebloater PRIVATE
    advapi32
    shell32
    user32
    gdi32
    comctl32
    uxtheme
    dwmapi
    version
    dnsapi
    ole32
)

set_target_properties(ChromeDebloater PROPERTIES
    LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\""
)
