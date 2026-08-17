add_executable(slipstream
    main.cpp
    NetworkManager.cpp
    NetworkManager.h
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
)
