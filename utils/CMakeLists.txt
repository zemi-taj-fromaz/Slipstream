add_library(UTILS STATIC
    csv_parser/src/csv_parser.cpp
    csv_parser/src/file_reader.cpp
    csv_parser/src/file_reader.h
    csv_parser/include/market_event.h
    csv_parser/include/parser.h
    message_processor/include/message_processor.h
    message_processor/src/message_processor.cpp
    replay/include/process_rows.h
    replay/include/replay_start.h
    replay/src/replay_start.cpp
    socket/include/socket.h
    transport/include/client_transport.h
    concurrent_queues/spsc_queue.h
)

target_include_directories(UTILS
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/message_processor/include
        ${CMAKE_CURRENT_SOURCE_DIR}/csv_parser/include
        ${CMAKE_CURRENT_SOURCE_DIR}/replay/include
        ${CMAKE_CURRENT_SOURCE_DIR}/socket/include
        ${CMAKE_CURRENT_SOURCE_DIR}/transport/include
        ${CMAKE_CURRENT_SOURCE_DIR}/concurrent_queues
)

target_compile_features(UTILS PUBLIC cxx_std_23)
