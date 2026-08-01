cmake_minimum_required(VERSION 3.20)
project(http-server VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Экспортируем команды компиляции для IDE (нужно для clangd)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Опции сборки (переключаются через CMake / Presets)
option(ENABLE_SANITIZERS "Enable Address and Undefined Behavior Sanitizers" OFF)
option(ENABLE_WERROR "Treat warnings as errors" OFF)

# Базовые варнинги
set(WARNING_FLAGS "-Wall" "-Wextra" "-Wpedantic")
if(ENABLE_WERROR)
    list(APPEND WARNING_FLAGS "-Werror")
endif()

# Применяем флаги компилятора
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(${WARNING_FLAGS})
endif()

if(ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all)
    add_link_options(-fsanitize=address,undefined)
endif()

include(FetchContent)

# Fetch MemCore (Рекомендуется заменить GIT_TAG main на конкретный хэш коммита для стабильности!)
FetchContent_Declare(
    MemCore
    GIT_REPOSITORY https://github.com/HenrikGharagyozyan/MemCore.git
    GIT_TAG v1.0
)
FetchContent_MakeAvailable(MemCore)

FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(json)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
FetchContent_MakeAvailable(googletest)

find_package(Threads REQUIRED)

# Обрати внимание: добавлен src/server/Config.cpp
add_library(http_server_core 
    src/server/Config.cpp
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

target_link_libraries(http_server_core PUBLIC 
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    MemCore::MemCore
    Threads::Threads
)
target_include_directories(http_server_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE http_server_core)

enable_testing()
add_subdirectory(tests)