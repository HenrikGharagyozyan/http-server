cmake_minimum_required(VERSION 3.20)
project(cpp-http-server VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(WARNING_FLAGS "-Wall" "-Wextra" "-Wpedantic" "-Werror")
endif()

add_executable(${PROJECT_NAME} 
    src/main.cpp
    src/Socket.cpp
    src/TcpServer.cpp
    src/http/Response.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(${PROJECT_NAME} PRIVATE ${WARNING_FLAGS})