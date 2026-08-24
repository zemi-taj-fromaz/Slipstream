add_executable(slipstream
    main.cpp
    Engine.cpp
    Engine.h
    ExecutionReport.cpp
    ExecutionReport.h
    NetworkManager.cpp
    NetworkManager.h
    TradeManager.cpp
    TradeManager.h
    VwapWindow.cpp
    VwapWindow.h
)

target_link_libraries(slipstream
    PRIVATE
        slipstream_codec
        UTILS
        SPSCQueue::SPSCQueue
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
