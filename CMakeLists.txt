cmake_minimum_required(VERSION 3.16)

project(mesen VERSION 0.1.0 LANGUAGES C CXX)

include(CheckPIESupported)
include(GNUInstallDirs)
check_pie_supported()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets)
find_package(SDL2 REQUIRED)
find_package(X11 REQUIRED)

file(GLOB_RECURSE MESEN_CORE_SOURCES CONFIGURE_DEPENDS
    "Core/*.cpp"
    "Linux/*.c"
    "Linux/*.cpp"
    "Lua/*.c"
    "SevenZip/*.c"
    "Utilities/*.c"
    "Utilities/*.cpp"
)

add_library(mesen_core STATIC
	${MESEN_CORE_SOURCES}
    Sdl/SdlSoundManager.cpp
)

set_target_properties(mesen_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(mesen_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Core
    ${CMAKE_CURRENT_SOURCE_DIR}/Linux
    ${CMAKE_CURRENT_SOURCE_DIR}/Sdl
    ${CMAKE_CURRENT_SOURCE_DIR}/Utilities
)

target_link_libraries(mesen_core PRIVATE
    SDL2::SDL2
    X11::X11
)

install(FILES Linux/appimage/Mesen.48x48.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps
    RENAME Mesen.png
)

add_subdirectory(src)

# Debug Build Only
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git/hooks/pre-commit)
        file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/pre-commit
            DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/.git/hooks
            FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
    endif()
endif()
