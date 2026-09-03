add_executable(order_entry_client
    main.cpp
    transport/OeTcpClientTransport.cpp
    transport/OeTcpClientTransport.h
)

target_link_libraries(order_entry_client
    PRIVATE
        UTILS
        slipstream_codec
        slipstream_grpc_protocol
        spdlog::spdlog
)

target_compile_definitions(order_entry_client
    PRIVATE
        SLIPSTREAM_CSV_PATH="${PROJECT_SOURCE_DIR}/Quotes_and_Trades.csv"
        SLIPSTREAM_EVENT_LOG_PATH="${PROJECT_BINARY_DIR}/order_entry_events.csv"
        SLIPSTREAM_VERIFICATION_DIR="${PROJECT_BINARY_DIR}/verification"
)
