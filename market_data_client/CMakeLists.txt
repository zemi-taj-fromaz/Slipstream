add_executable(market_data_client
    main.cpp
)

target_link_libraries(market_data_client
    PRIVATE
        UTILS
        spdlog::spdlog
)

target_compile_definitions(market_data_client
    PRIVATE
        SLIPSTREAM_CSV_PATH="${PROJECT_SOURCE_DIR}/Quotes_and_Trades.csv"
)
