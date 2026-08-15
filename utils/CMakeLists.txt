add_library(UTILS STATIC
    csv_parser/src/csv_parser.cpp
    csv_parser/src/file_reader.cpp
    csv_parser/src/file_reader.h
    csv_parser/include/market_event.h
    csv_parser/include/parser.h
    message_processor/include/message_processor.h
    message_processor/src/message_processor.cpp
)

target_include_directories(UTILS
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/message_processor/include
        ${CMAKE_CURRENT_SOURCE_DIR}/csv_parser/include
)

target_compile_features(UTILS PUBLIC cxx_std_23)

target_compile_options(UTILS
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)
