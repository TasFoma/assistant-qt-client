cmake_minimum_required(VERSION 3.16)
project(QtClient)

set(CMAKE_PREFIX_PATH "C:/Qt/6.10.2/msvc2022_64")
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RC_COMPILER "")
set(CMAKE_RC_COMPILER_WORKS FALSE)

find_package(Qt6 REQUIRED COMPONENTS Core Quick Network)

set(QML_RESOURCES resources/qml.qrc)
set(SOURCES src/main.cpp src/llamaclient.cpp src/voiceinput.cpp)
set(HEADERS src/llamaclient.h src/voiceinput.h)

add_executable(qt_client ${SOURCES} ${HEADERS} ${QML_RESOURCES})

target_link_libraries(qt_client
    Qt6::Core
    Qt6::Quick
    Qt6::Network
    winmm
)

if(MSVC)
    target_compile_options(qt_client PRIVATE /Zc:__cplusplus /permissive-)
endif()

# Copy Python scripts next to the executable so voice_listener.py and recognize.py are always found.
foreach(SCRIPT voice_listener.py voice_daemon.py recognize.py)
    add_custom_command(TARGET qt_client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/${SCRIPT}"
            "$<TARGET_FILE_DIR:qt_client>/${SCRIPT}"
        COMMENT "Copying ${SCRIPT} to output directory"
    )
endforeach()