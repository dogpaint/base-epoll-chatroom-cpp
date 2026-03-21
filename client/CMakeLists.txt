add_executable(chat_client client.cpp)
target_include_directories(chat_client PRIVATE ${CMAKE_SOURCE_DIR}/include)
# 设置输出目录到build根目录
set_target_properties(chat_client PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

