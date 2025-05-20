# QUIC Teleop Client

A QUIC-based teleoperation client implementation using Microsoft's msquic library.

## Prerequisites

- Microsoft's msquic library (https://github.com/microsoft/msquic)
- C++ compiler with C++17 support
- CMake 3.10 or higher

## Building

1. Clone the msquic repository:
```bash
git clone https://github.com/microsoft/msquic.git
```

2. Build msquic following their instructions

3. Build this project:
```bash
mkdir build && cd build
cmake ..
make
```

## Usage

```bash
./client <server-name>
```

## Features

- QUIC-based communication
- Low-latency connection settings
- FlatBuffers serialization for control commands
- Automatic keep-alive and connection management 