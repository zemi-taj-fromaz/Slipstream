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
