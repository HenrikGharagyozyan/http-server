cmake_minimum_required(VERSION 3.20)
project(http-server VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(WARNING_FLAGS "-Wall" "-Wextra" "-Wpedantic" "-Werror")
endif()

# Включаем санитайзеры для нашей ветки PMR/MemCore
add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all)
add_link_options(-fsanitize=address,undefined)


# Подключаем MemCore через FetchContent
include(FetchContent)
FetchContent_Declare(
    MemCore
    GIT_REPOSITORY https://github.com/HenrikGharagyozyan/MemCore.git
    GIT_TAG main
)
FetchContent_MakeAvailable(MemCore)

# Automatically fetch nlohmann_json
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(json)

# Automatically fetch spdlog via Git repository (immune to 404 release URL issues)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
)
FetchContent_MakeAvailable(spdlog)


add_executable(${PROJECT_NAME} 
    src/main.cpp
    src/server/Socket.cpp
    src/server/TcpServer.cpp
    src/server/HttpServer.cpp
    src/server/ThreadPool.cpp
    src/server/Router.cpp
    src/http/Response.cpp
    src/http/Parser.cpp
    src/utils/FileSystem.cpp
    src/handlers/UserHandler.cpp
    src/handlers/StaticHandler.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    MemCore::MemCore
)

find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(${PROJECT_NAME} PRIVATE ${WARNING_FLAGS})