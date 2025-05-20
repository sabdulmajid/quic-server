# Find flatc compiler
find_program(FLATC flatc)
if(NOT FLATC)
    message(FATAL_ERROR "flatc compiler not found")
endif()

# Find FlatBuffers package
find_package(FlatBuffers REQUIRED)
include_directories(/opt/homebrew/include)

# Generate FlatBuffers code
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/teleop_generated.h
    COMMAND flatc --cpp -o ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/teleop.fbs
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/teleop.fbs
    COMMENT "Generating FlatBuffers code"
)

# Add executables
add_executable(quic_server server.cpp ${CMAKE_CURRENT_BINARY_DIR}/teleop_generated.h)
add_executable(quic_client client.cpp ${CMAKE_CURRENT_BINARY_DIR}/teleop_generated.h)
add_executable(test_flatbuffers test.cpp ${CMAKE_CURRENT_BINARY_DIR}/teleop_generated.h)

# Link against msquic library
target_link_libraries(quic_server msquic ${FLATBUFFERS_LIBRARIES})
target_link_libraries(quic_client msquic ${FLATBUFFERS_LIBRARIES})
target_link_libraries(test_flatbuffers ${FLATBUFFERS_LIBRARIES})

# Add include directories
target_include_directories(quic_server PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR} 
    ${CMAKE_CURRENT_BINARY_DIR}
    /opt/homebrew/include
    ${CMAKE_SOURCE_DIR}/msquic/src/inc
)
target_include_directories(quic_client PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR} 
    ${CMAKE_CURRENT_BINARY_DIR}
    /opt/homebrew/include
    ${CMAKE_SOURCE_DIR}/msquic/src/inc
)
target_include_directories(test_flatbuffers PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR} 
    ${CMAKE_CURRENT_BINARY_DIR}
    /opt/homebrew/include
)

# Include directories
target_include_directories(quic_server PRIVATE 
    ${CMAKE_SOURCE_DIR}/msquic/src/inc
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/msquic/msquic/include
    /opt/homebrew/include
)
target_include_directories(quic_client PRIVATE 
    ${CMAKE_SOURCE_DIR}/msquic/src/inc
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/msquic/msquic/include
    /opt/homebrew/include
) 