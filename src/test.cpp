#include <iostream>
#include "teleop_generated.h"

int main() {
    // Create a FlatBufferBuilder to store our data
    flatbuffers::FlatBufferBuilder builder(1024);

    // Create a control command
    auto command = Teleop::CreateControlCommand(
        builder,
        1.0f,    // linear_velocity
        0.5f,    // angular_velocity
        123456   // timestamp
    );
    builder.Finish(command);

    // Get the buffer and verify it
    uint8_t* buf = builder.GetBufferPointer();
    int size = builder.GetSize();

    // Verify and read back the data
    auto verifier = flatbuffers::Verifier(buf, size);
    auto cmd = flatbuffers::GetRoot<Teleop::ControlCommand>(buf);
    if (!cmd->Verify(verifier)) {
        std::cerr << "Invalid buffer!" << std::endl;
        return 1;
    }

    // Read back the data
    std::cout << "Control Command:" << std::endl;
    std::cout << "Linear velocity: " << cmd->linear_velocity() << std::endl;
    std::cout << "Angular velocity: " << cmd->angular_velocity() << std::endl;
    std::cout << "Timestamp: " << cmd->timestamp() << std::endl;

    return 0;
} 