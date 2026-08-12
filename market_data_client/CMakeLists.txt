add_executable(market_data_client
    main.cpp
)

target_link_libraries(market_data_client
    PRIVATE
        csv_parser
        spdlog::spdlog
)
