set(CMAKE_AUTOMOC ON)

add_executable(test_extractors
    test_extractors.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/model/spritedocument.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/packer/atlaspacker.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/extractor.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/extractor/extractorregistry.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/extractor/spriteextractor.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/extractor/gifextractor.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/extractor/jsonextractor.cpp
    ${CMAKE_SOURCE_DIR}/SpriteStudio/src/extractor/godotextractor.cpp

    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/model/spritedocument.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/packer/atlaspacker.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/extractor/extractor.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/extractor/extractorregistry.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/extractor/spriteextractor.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/extractor/gifextractor.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/extractor/jsonextractor.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/extractor/godotextractor.h
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include/extractor/export.h
)

target_include_directories(test_extractors PRIVATE
    ${CMAKE_SOURCE_DIR}/SpriteStudio/include
    ${CMAKE_BINARY_DIR}/SpriteStudio
)

target_compile_definitions(test_extractors PRIVATE
    SAMPLE_DIR="${CMAKE_SOURCE_DIR}/sample"
)

target_link_libraries(test_extractors PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

set_target_properties(test_extractors PROPERTIES
    WIN32_EXECUTABLE FALSE
)

if (MINGW)
    target_link_options(test_extractors PRIVATE -mconsole)
endif()

add_test(NAME test_extractors COMMAND test_extractors)
