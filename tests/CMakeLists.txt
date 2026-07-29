# Create the test executable
add_executable(http-server-tests
    ParserTest.cpp
)

# Link our server core and GTest (gtest_main provides main() for tests)
target_link_libraries(http-server-tests PRIVATE
    http_server_core
    GTest::gtest_main
)

# Integrate with CTest
include(GoogleTest)
gtest_discover_tests(http-server-tests)