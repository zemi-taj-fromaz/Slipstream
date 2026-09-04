add_executable(market_data_client
    main.cpp
    transport/MdGrpcClientTransport.cpp
    transport/MdGrpcClientTransport.h
    transport/MdTcpClientTransport.cpp
    transport/MdTcpClientTransport.h
    transport/MdUdpMulticastClientTransport.cpp
    transport/MdUdpMulticastClientTransport.h
)

target_link_libraries(market_data_client
    PRIVATE
        UTILS
        slipstream_codec
        slipstream_grpc_protocol
        spdlog::spdlog
)

target_compile_definitions(market_data_client
    PRIVATE
        SLIPSTREAM_CSV_PATH="${PROJECT_SOURCE_DIR}/Quotes_and_Trades.csv"
        SLIPSTREAM_EVENT_LOG_PATH="${PROJECT_BINARY_DIR}/market_data_events.csv"
        SLIPSTREAM_VERIFICATION_DIR="${PROJECT_BINARY_DIR}/verification"
)
