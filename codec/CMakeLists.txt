add_library(slipstream_codec STATIC
    include/slipstream_codec/market_data_codec.h
    src/market_data_codec.cpp
)

set_target_properties(slipstream_codec
    PROPERTIES
        OUTPUT_NAME slipstream_codec
)

target_include_directories(slipstream_codec
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(slipstream_codec
    PUBLIC
        UTILS
)

target_compile_features(slipstream_codec PUBLIC cxx_std_23)
