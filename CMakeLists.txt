cmake_minimum_required(VERSION 3.10)
project(quic_teleop_example)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(FlatBuffers REQUIRED)
include_directories(${FLATBUFFERS_INCLUDE_DIR})

# Add msquic as a submodule
add_subdirectory(msquic)

# Add source directory
add_subdirectory(src) 