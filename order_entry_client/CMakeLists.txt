add_executable(order_entry_client
    main.cpp
)

target_link_libraries(order_entry_client
    PRIVATE
        csv_parser
        spdlog::spdlog
)
