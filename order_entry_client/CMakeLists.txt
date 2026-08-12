add_executable(order_entry_client
    main.cpp
)

target_link_libraries(order_entry_client
    PRIVATE
        csv_parser
        spdlog::spdlog
)

target_compile_definitions(order_entry_client
    PRIVATE
        SLIPSTREAM_CSV_PATH="${PROJECT_SOURCE_DIR}/Quotes_and_Trades.csv"
)
