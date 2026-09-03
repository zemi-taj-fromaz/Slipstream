add_executable(slipstream
    main.cpp
    Engine.cpp
    Engine.h
    ExecutionReport.cpp
    ExecutionReport.h
    Queues.h
    TradeManager.cpp
    TradeManager.h
    VwapWindow.cpp
    VwapWindow.h
    transport/GrpcServerTransport.cpp
    transport/GrpcServerTransport.h
    transport/IServerTransport.h
    transport/TcpPollServerTransport.cpp
    transport/TcpPollServerTransport.h
)

target_include_directories(slipstream
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(slipstream
    PRIVATE
        slipstream_codec
        slipstream_grpc_protocol
        UTILS
        spdlog::spdlog
)

target_compile_definitions(slipstream
    PRIVATE
        SLIPSTREAM_VERIFICATION_DIR="${PROJECT_BINARY_DIR}/verification"
        SLIPSTREAM_EXECUTION_REPORT_PATH="${PROJECT_BINARY_DIR}/slipstream_execution_report.log"
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(slipstream
        PRIVATE
            -Wno-interference-size
    )
endif()
