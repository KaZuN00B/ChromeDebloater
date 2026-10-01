cmake_minimum_required(VERSION 3.20)
project(ChromeDebloater LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(src)

add_executable(ChromeDebloater WIN32
    src/main.cpp
    src/ui/window.cpp
    src/ui/window.h
    src/ui/theme.h
    src/ui/render_utils.cpp
    src/ui/render_utils.h
    src/ui/navigation.h
    src/engine/browser_target.h
    src/engine/audit_engine.cpp
    src/engine/audit_engine.h
    src/engine/tweak_engine.cpp
    src/engine/tweak_engine.h
    src/engine/backup_engine.cpp
    src/engine/backup_engine.h
    src/engine/sqlite_cleaner.h
    src/app.rc
)

target_compile_definitions(ChromeDebloater PRIVATE UNICODE _UNICODE)

target_link_libraries(ChromeDebloater PRIVATE
    advapi32
    shell32
    user32
    gdi32
    gdiplus
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
