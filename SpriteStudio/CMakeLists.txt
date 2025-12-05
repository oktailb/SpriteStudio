cmake_minimum_required(VERSION 3.30)

# Créer un fichier d'en-tête avec les informations de version
configure_file(
    src/version.h.in
    generated/version.h
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Spécifiez les langues à supporter (code ISO 639-1 ou code Qt)
set(${PROJECT_NAME}_TS_FILES
    i18n/sprite_studio_fr_FR.ts # Français
    i18n/sprite_studio_en_US.ts # English
    i18n/sprite_studio_ja_JA.ts # Japanese
)

set (${PROJECT_NAME}_SRC
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow_events.cpp
    src/mainwindow_callbacks.cpp
    src/mainwindow_animation.cpp
    src/mainwindow_frames.cpp
    src/mainwindow_atlas.cpp
    src/extractor.cpp
    src/extractor/gifextractor.cpp
    src/extractor/spriteextractor.cpp
    src/extractor/jsonextractor.cpp
    src/extractor/jsonExtractordialog.cpp
    src/arrangementmodel.cpp
    src/aboutdialog.cpp
    src/version.h.in
    src/haiku.PackageInfo.in
    generated/version.h
)

set (${PROJECT_NAME}_UI
    src/mainwindow.ui
    src/extractor/jsonExtractordialog.ui
)

set (${PROJECT_NAME}_INCLUDE
    include/mainwindow.h
    include/extractor/extractor.h
    include/extractor/gifextractor.h
    include/extractor/spriteextractor.h
    include/extractor/jsonextractor.h
    include/extractor/jsonExtractordialog.h
    include/extractor/export.h
    include/arrangementmodel.h
    include/aboutdialog.h
)

qt_add_translations(${PROJECT_NAME}
    SOURCES
        ${${PROJECT_NAME}_SRC}
        ${${PROJECT_NAME}_INCLUDE}
    TS_FILES
        ${${PROJECT_NAME}_TS_FILES}
)

qt_add_executable(${PROJECT_NAME}
    ${${PROJECT_NAME}_SRC}
    ${${PROJECT_NAME}_UI}
    ${${PROJECT_NAME}_INCLUDE}
)

add_custom_target(translations ALL DEPENDS ${TS_FILES})

qt6_add_resources(${PROJECT_NAME} "images"
    PREFIX "drawer"
    FILES
    play.png
    pause.png
    stop.png
    plus.png
    minus.png
)

qt6_add_resources(${PROJECT_NAME} "images"
    PREFIX "i18n"
    FILES
    ${${PROJECT_NAME}_TS_FILES}
)

qt6_add_resources(${PROJECT_NAME} "text"
    PREFIX "text"
    FILES
    license.txt
    about.txt
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Multimedia
    Qt${QT_VERSION_MAJOR}::MultimediaWidgets
)
#message(FATAL_ERROR "TEST: ")
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ../lib/include
    ${CMAKE_BUILD_RPATH}/
)

if(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
    set(CPACK_GENERATOR "External")
    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND package create -i ${CMAKE_BINARY_DIR}/generated/haiku.PackageInfo -C ${PROJECT_NAME} ${PROJECT_NAME}-${PROJECT_VERSION}-1-x86_64.hpkg
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()
