cmake_minimum_required(VERSION 3.10)
project(ChatroomEpoll VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

set(PLATFORM_SOURCES
    src/platform.cpp
)

set(SERVER_SOURCES
    src/main.cpp
    src/ChatServer.cpp
    src/command_parser.cpp
    ${PLATFORM_SOURCES}
)

add_executable(chat_server ${SERVER_SOURCES})
target_include_directories(chat_server PRIVATE include)

if(WIN32)
    target_link_libraries(chat_server ws2_32)
    target_compile_definitions(chat_server PRIVATE _WIN32_WINNT=0x0600)
endif()

if(UNIX AND NOT APPLE)
    target_compile_definitions(chat_server PRIVATE PLATFORM_LINUX=1)
elseif(APPLE)
    target_compile_definitions(chat_server PRIVATE PLATFORM_MACOS=1)
endif()

add_subdirectory(client)

option(BUILD_TESTS "Build tests with Google Test" ON)
if(BUILD_TESTS)
    enable_testing()
    find_package(GTest QUIET)
    if(GTest_FOUND)
        add_subdirectory(tests)
    else()
        message(STATUS "GTest not found, skipping tests. Install with: sudo apt install libgtest-dev")
    endif()
endif()