set(SLIPSTREAM_PROTO_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/proto/slipstream.proto"
)

set(SLIPSTREAM_PROTO_GENERATED_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

set(SLIPSTREAM_PROTO_SOURCES
    "${SLIPSTREAM_PROTO_GENERATED_DIR}/slipstream.pb.cc"
    "${SLIPSTREAM_PROTO_GENERATED_DIR}/slipstream.grpc.pb.cc"
)

set(SLIPSTREAM_PROTO_HEADERS
    "${SLIPSTREAM_PROTO_GENERATED_DIR}/slipstream.pb.h"
    "${SLIPSTREAM_PROTO_GENERATED_DIR}/slipstream.grpc.pb.h"
)

file(MAKE_DIRECTORY "${SLIPSTREAM_PROTO_GENERATED_DIR}")

get_target_property(
    SLIPSTREAM_PROTOBUF_INCLUDE_DIRS
    protobuf::libprotobuf
    INTERFACE_INCLUDE_DIRECTORIES
)

set(SLIPSTREAM_PROTO_PATH_ARGUMENTS
    "--proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto"
)

foreach(include_dir IN LISTS SLIPSTREAM_PROTOBUF_INCLUDE_DIRS)
    if(NOT include_dir MATCHES "^\\$<")
        list(APPEND SLIPSTREAM_PROTO_PATH_ARGUMENTS
            "--proto_path=${include_dir}"
        )
    endif()
endforeach()

add_custom_command(
    OUTPUT
        ${SLIPSTREAM_PROTO_SOURCES}
        ${SLIPSTREAM_PROTO_HEADERS}
    COMMAND
        $<TARGET_FILE:protobuf::protoc>
    ARGS
        ${SLIPSTREAM_PROTO_PATH_ARGUMENTS}
        "--cpp_out=${SLIPSTREAM_PROTO_GENERATED_DIR}"
        "--grpc_out=${SLIPSTREAM_PROTO_GENERATED_DIR}"
        "--plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
        "${SLIPSTREAM_PROTO_FILE}"
    DEPENDS
        "${SLIPSTREAM_PROTO_FILE}"
        protobuf::protoc
        gRPC::grpc_cpp_plugin
    VERBATIM
)

add_library(slipstream_grpc_protocol STATIC
    ${SLIPSTREAM_PROTO_SOURCES}
    ${SLIPSTREAM_PROTO_HEADERS}
)

target_include_directories(slipstream_grpc_protocol
    PUBLIC
        "${SLIPSTREAM_PROTO_GENERATED_DIR}"
)

target_link_libraries(slipstream_grpc_protocol
    PUBLIC
        protobuf::libprotobuf
        gRPC::grpc++
)

target_compile_features(slipstream_grpc_protocol
    PUBLIC
        cxx_std_23
)
