# QUIC Teleop Framework

A high-performance, secure, and low-latency teleoperation framework built on Microsoft's msquic protocol. Perfect for robotics, remote control systems, and real-time control applications.

![QUIC Teleop Framework](https://img.shields.io/badge/QUIC-Teleop-blue)
![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)

## Key Features

- **Ultra-Low Latency**: Built on QUIC protocol for minimal communication delay
- **Secure by Design**: Built-in authentication and encryption
- **Real-time Control**: Precise command transmission with sequence tracking
- **Robust Architecture**: Client-Server-Proxy architecture for scalable deployments
- **Rich Telemetry**: Comprehensive sensor data and state monitoring
- **Cross-Platform**: Works on Windows, Linux, and macOS

## Components

### Client
- Command transmission with authentication
- Real-time control interface
- Automatic connection management
- Command sequence tracking

### Server
- Robot control interface
- Sensor data aggregation
- State management
- Command validation

### Proxy
- Authentication and authorization
- Traffic management
- Load balancing
- Security enforcement

## Prerequisites

- Microsoft's msquic library (https://github.com/microsoft/msquic)
- C++17 compatible compiler
- CMake 3.10 or higher
- OpenSSL for cryptographic operations
- FlatBuffers for efficient serialization

## Building

1. Clone the repository:
```bash
git clone https://github.com/yourusername/quic-teleop.git
cd quic-teleop
```

2. Install msquic:
```bash
git clone https://github.com/microsoft/msquic.git
cd msquic
# Follow msquic build instructions
```

3. Build the project:
```bash
mkdir build && cd build
cmake ..
make
```

## Usage

### Starting the Proxy
```bash
./proxy <server_name> <client_port> <server_port>
```

### Running the Client
```bash
./client <server_name>
```

### Running the Server
```bash
./server <port>
```

## Security Features

- Token-based authentication
- Public key verification
- Session management
- Automatic token expiration
- Secure command validation

## Performance

- Sub-10ms command latency
- Automatic connection recovery
- Efficient binary serialization
- Optimized network utilization
- Built-in keep-alive mechanism

## Roadmap

- [ ] Web-based control interface
- [ ] Multiple robot support
- [ ] Advanced telemetry visualization
- [ ] Machine learning integration
- [ ] ROS2 integration
- [ ] Docker support
- [ ] Kubernetes deployment
- [ ] WebRTC fallback
- [ ] Advanced security features
- [ ] Performance monitoring dashboard

## Super Cool Features

1. **Command Logging** - The server logs every incoming command to `command_log.csv` for later analysis.
2. **Macro Recorder** - Record a sequence of commands and replay them to automate complex maneuvers.
3. **Latency Monitor** - The server calculates average latency from each command's timestamp.

### Demo

Start the server normally. It will run a short demo that records commands for 5 seconds and prints the running latency:

```bash
./quic_server
```

Check `command_log.csv` for the logged commands and see the console output for average latency. The recorded macro length is printed on shutdown.
