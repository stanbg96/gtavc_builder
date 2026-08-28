cmake_minimum_required(VERSION 3.22.1)

project(gtavc_engine LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra -ffast-math")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set(SOURCES
    src/main.cpp
    src/osm_parser.cpp
    src/sat_vision.cpp
    src/geometry_builder.cpp
    src/dff_builder.cpp
    src/col_builder.cpp
    src/txd_builder.cpp
)

add_library(
    gtavc_engine
    SHARED
    ${SOURCES}
)

find_library(
    log-lib
    log
)

target_link_libraries(
    gtavc_engine
    ${log-lib}
)
