cmake_minimum_required(VERSION 3.25)
project(Slipstream LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

include(CTest)
include(third_party)

#add_executable(Slipstream main.cpp
#slipstream/main.cpp)

add_subdirectory(utils)
add_subdirectory(codec)
add_subdirectory(market_data_client)
add_subdirectory(order_entry_client)
add_subdirectory(slipstream)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
