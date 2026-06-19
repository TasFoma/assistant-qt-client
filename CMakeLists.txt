cmake_minimum_required(VERSION 3.16)
project(QtClient)

set(CMAKE_PREFIX_PATH "C:/Qt/6.10.2/msvc2022_64")
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Quick Network)

set(QML_RESOURCES resources/qml.qrc)
set(SOURCES src/main.cpp src/llamaclient.cpp)
set(HEADERS src/llamaclient.h)

add_executable(qt_client ${SOURCES} ${HEADERS} ${QML_RESOURCES})

target_link_libraries(qt_client
    Qt6::Core
    Qt6::Quick
    Qt6::Network
)

if(MSVC)
    target_compile_options(qt_client PRIVATE /Zc:__cplusplus /permissive-)
endif()