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

find_package(Qt6 REQUIRED COMPONENTS Core Quick Network Sql Widgets Multimedia)

set(QML_RESOURCES resources/qml.qrc)
# qt_client — тонкий клиент: интерфейс + голос; данные на companion_server
set(SOURCES
    src/main.cpp
    src/musicplayer.cpp
    src/remotebackend.cpp
    src/voiceinput.cpp
)
set(HEADERS
    src/musicplayer.h
    src/remotebackend.h
    src/voiceinput.h
)

add_executable(qt_client ${SOURCES} ${HEADERS} ${QML_RESOURCES})

target_link_libraries(qt_client
    Qt6::Core
    Qt6::Quick
    Qt6::Network
    Qt6::Widgets
    Qt6::Multimedia
    winmm
)

if(MSVC)
    target_compile_options(qt_client PRIVATE /Zc:__cplusplus /permissive-)
endif()

# ── companion_server: безоконный «мозг» для серверной машины ─────────
# Те же менеджеры без GUI и голоса; клиенты (телефон/десктоп)
# подключаются к нему по TCP на порт 8765.
add_executable(companion_server
    src/main_server.cpp
    src/chatmanager.cpp
    src/commandparser.cpp
    src/databasemanager.cpp
    src/mentormanager.cpp
    src/navigationmanager.cpp
    src/plannermanager.cpp
    src/syncserver.cpp
    src/chatmanager.h
    src/commandparser.h
    src/databasemanager.h
    src/mentormanager.h
    src/navigationmanager.h
    src/plannermanager.h
    src/syncserver.h
)
target_link_libraries(companion_server
    Qt6::Core
    Qt6::Network
    Qt6::Sql
)
if(MSVC)
    target_compile_options(companion_server PRIVATE /Zc:__cplusplus /permissive-)
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
