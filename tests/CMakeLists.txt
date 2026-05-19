cmake_minimum_required(VERSION 3.10)
project(ChatroomTests CXX)

include(GoogleTest)

include_directories(${CMAKE_SOURCE_DIR}/include)

set(TEST_PLATFORM_SOURCES ${CMAKE_SOURCE_DIR}/src/platform.cpp)

add_executable(command_parser_test
    command_parser_test.cpp
    ${CMAKE_SOURCE_DIR}/src/command_parser.cpp
)
target_link_libraries(command_parser_test GTest::gtest GTest::gtest_main pthread)

add_executable(chat_server_test
    chat_server_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ChatServer.cpp
    ${CMAKE_SOURCE_DIR}/src/command_parser.cpp
    ${TEST_PLATFORM_SOURCES}
)
target_include_directories(chat_server_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(chat_server_test GTest::gtest GTest::gtest_main pthread)
if(WIN32)
    target_link_libraries(chat_server_test ws2_32)
endif()

add_executable(integration_test
    integration_test.cpp
    ${TEST_PLATFORM_SOURCES}
)
target_include_directories(integration_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(integration_test GTest::gtest GTest::gtest_main pthread)
if(WIN32)
    target_link_libraries(integration_test ws2_32)
endif()

add_executable(client_display_test
    client_display_test.cpp
)
target_link_libraries(client_display_test GTest::gtest GTest::gtest_main)

add_executable(stress_test
    stress_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ChatServer.cpp
    ${CMAKE_SOURCE_DIR}/src/command_parser.cpp
    ${TEST_PLATFORM_SOURCES}
)
target_include_directories(stress_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(stress_test GTest::gtest GTest::gtest_main pthread)
if(WIN32)
    target_link_libraries(stress_test ws2_32)
endif()

gtest_discover_tests(command_parser_test)
gtest_discover_tests(chat_server_test)
gtest_discover_tests(integration_test)
gtest_discover_tests(client_display_test)
gtest_discover_tests(stress_test DISCOVERY_TIMEOUT 30)