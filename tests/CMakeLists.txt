include(GoogleTest)

add_executable(csv_parser_tests
    csv_parser_tests.cpp
)

target_link_libraries(csv_parser_tests
    PRIVATE
        UTILS
        GTest::gtest_main
)

gtest_discover_tests(csv_parser_tests)

add_executable(vwap_window_tests
    vwap_window_tests.cpp
    ../slipstream/VwapWindow.cpp
)

target_include_directories(vwap_window_tests
    PRIVATE
        ../slipstream
)

target_link_libraries(vwap_window_tests
    PRIVATE
        GTest::gtest_main
)

gtest_discover_tests(vwap_window_tests)

add_executable(execution_report_tests
    execution_report_tests.cpp
    ../slipstream/ExecutionReport.cpp
)

target_include_directories(execution_report_tests
    PRIVATE
        ../slipstream
)

target_link_libraries(execution_report_tests
    PRIVATE
        GTest::gtest_main
)

gtest_discover_tests(execution_report_tests)

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

add_executable(spsc_queue_tests
    spsc_queue_tests.cpp
)

target_link_libraries(spsc_queue_tests
    PRIVATE
        UTILS
        GTest::gtest_main
)

gtest_discover_tests(spsc_queue_tests)

add_executable(trade_manager_tests
    trade_manager_tests.cpp
    ../slipstream/TradeManager.cpp
    ../slipstream/VwapWindow.cpp
)

target_include_directories(trade_manager_tests
    PRIVATE
        ../slipstream
)

target_link_libraries(trade_manager_tests
    PRIVATE
        UTILS
        GTest::gtest_main
)

gtest_discover_tests(trade_manager_tests)

add_executable(engine_tests
    engine_tests.cpp
    ../slipstream/Engine.cpp
    ../slipstream/TradeManager.cpp
    ../slipstream/VwapWindow.cpp
)

target_include_directories(engine_tests
    PRIVATE
        ../slipstream
)

target_link_libraries(engine_tests
    PRIVATE
        UTILS
        slipstream_codec
        GTest::gtest_main
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(spsc_queue_tests PRIVATE -Wno-interference-size)
    target_compile_options(engine_tests PRIVATE -Wno-interference-size)
endif()

gtest_discover_tests(engine_tests)
