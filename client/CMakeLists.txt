set(CLIENT_SOURCES
    client.cpp
    ${CMAKE_SOURCE_DIR}/src/platform.cpp
)

add_executable(chat_client ${CLIENT_SOURCES})
target_include_directories(chat_client PRIVATE ${CMAKE_SOURCE_DIR}/include)
set_target_properties(chat_client PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if(WIN32)
    target_link_libraries(chat_client ws2_32)
endif()