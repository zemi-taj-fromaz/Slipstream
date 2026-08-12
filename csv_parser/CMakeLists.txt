add_library(csv_parser STATIC
    src/csv_parser.cpp
    src/file_reader.cpp
    src/file_reader.h
    include/market_event.h
    include/parser.h
)

target_include_directories(csv_parser
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(csv_parser PUBLIC cxx_std_23)

target_compile_options(csv_parser
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)
