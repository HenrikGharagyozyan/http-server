cmake_minimum_required(VERSION 3.20)
project(http-server VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(WARNING_FLAGS "-Wall" "-Wextra" "-Wpedantic" "-Werror")
endif()

# Enable sanitizers
add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all)
add_link_options(-fsanitize=address,undefined)

include(FetchContent)

# Fetch MemCore
FetchContent_Declare(
    MemCore
    GIT_REPOSITORY https://github.com/HenrikGharagyozyan/MemCore.git
    GIT_TAG main
)
FetchContent_MakeAvailable(MemCore)

# Fetch nlohmann_json
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(json)

# Fetch spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
)
FetchContent_MakeAvailable(spdlog)

# Fetch GoogleTest
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
FetchContent_MakeAvailable(googletest)

find_package(Threads REQUIRED)

# 1. CORE LIBRARY: All server logic EXCEPT main.cpp
add_library(http_server_core 
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

# Use PUBLIC so anything linking to this library inherits these dependencies and includes
target_link_libraries(http_server_core PUBLIC 
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    MemCore::MemCore
    Threads::Threads
)
target_include_directories(http_server_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(http_server_core PRIVATE ${WARNING_FLAGS})

# 2. MAIN EXECUTABLE: Just main.cpp linked to the core library
add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE http_server_core)

# 3. TESTS: Enable CTest and add the tests subdirectory
enable_testing()
add_subdirectory(tests)