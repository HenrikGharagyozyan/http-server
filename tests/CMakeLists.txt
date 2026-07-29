# Создаем исполняемый файл для тестов
add_executable(http-server-tests
    ParserTest.cpp
)

# Линкуем ядро нашего сервера и GTest (gtest_main содержит функцию main() для тестов)
target_link_libraries(http-server-tests PRIVATE
    http_server_core
    GTest::gtest_main
)

# Интеграция с CTest
include(GoogleTest)
gtest_discover_tests(http-server-tests)