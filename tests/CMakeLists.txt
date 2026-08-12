include(GoogleTest)

add_executable(csv_parser_tests
    csv_parser_tests.cpp
)

target_link_libraries(csv_parser_tests
    PRIVATE
        UTILS
        GTest::gtest_main
)

target_compile_options(csv_parser_tests
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

gtest_discover_tests(csv_parser_tests)

add_executable(message_processor_tests
    message_processor_tests.cpp
)

target_link_libraries(message_processor_tests
    PRIVATE
        UTILS
        GTest::gtest_main
)

target_compile_options(message_processor_tests
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

gtest_discover_tests(message_processor_tests)
