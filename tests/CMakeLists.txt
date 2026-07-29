# Create the test executable
add_executable(http-server-tests
    ParserTest.cpp
    ResponseTest.cpp
    RouterTest.cpp
    FileSystemTest.cpp
    UserHandlerTest.cpp
    StaticHandlerTest.cpp
    ThreadPoolTest.cpp
    IntegrationTest.cpp
)

# Link our server core and GTest (gtest_main provides main() for tests)
target_link_libraries(http-server-tests PRIVATE
    http_server_core
    GTest::gtest_main
)

# Integrate with CTest
include(GoogleTest)
gtest_discover_tests(http-server-tests
    DISCOVERY_TIMEOUT 60
    PROPERTIES TIMEOUT 120
)
