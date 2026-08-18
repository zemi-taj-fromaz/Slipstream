include(GoogleTest)

add_executable(csv_parser_tests
    csv_parser_tests.cpp
        ../slipstream/SlipstreamConfig.h
        ../slipstream/TradeManager.cpp
        ../slipstream/TradeManager.h
)

target_link_libraries(csv_parser_tests
    PRIVATE
        UTILS
        GTest::gtest_main
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

gtest_discover_tests(message_processor_tests)

add_executable(market_data_codec_tests
    market_data_codec_tests.cpp
)

target_link_libraries(market_data_codec_tests
    PRIVATE
        slipstream_codec
        GTest::gtest_main
)

gtest_discover_tests(market_data_codec_tests)

add_executable(order_codec_tests
    order_codec_tests.cpp
)

target_link_libraries(order_codec_tests
    PRIVATE
        slipstream_codec
        GTest::gtest_main
)

gtest_discover_tests(order_codec_tests)
