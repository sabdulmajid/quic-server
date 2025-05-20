#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include "msquic.h"
#include "teleop_generated.h"

class QuicClient {
private:
    const QUIC_API_TABLE* MsQuic;
    HQUIC Registration;
    HQUIC Connection;
    bool Running;

    static QUIC_STATUS QUIC_API ClientCallback(
        HQUIC Connection,
        void* Context,
        QUIC_CONNECTION_EVENT* Event) {
        auto client = static_cast<QuicClient*>(Context);
        return client->HandleConnectionEvent(Connection, Event);
    }

    QUIC_STATUS HandleConnectionEvent(HQUIC Connection, QUIC_CONNECTION_EVENT* Event) {
        switch (Event->Type) {
            case QUIC_CONNECTION_EVENT_CONNECTED:
                std::cout << "Connected to server" << std::endl;
                std::cout << "  ALPN: " << std::string((const char*)Event->CONNECTED.NegotiatedAlpn, 
                                                     Event->CONNECTED.NegotiatedAlpnLength) << std::endl;
                std::cout << "  Session resumed: " << (Event->CONNECTED.SessionResumed ? "yes" : "no") << std::endl;
                return QUIC_STATUS_SUCCESS;

            case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
                std::cout << "Transport shutdown with status: 0x" << std::hex << Event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status 
                          << ", error code: 0x" << Event->SHUTDOWN_INITIATED_BY_TRANSPORT.ErrorCode << std::dec << std::endl;
                return QUIC_STATUS_SUCCESS;

            case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
                std::cout << "Peer shutdown with error code: 0x" << std::hex 
                          << Event->SHUTDOWN_INITIATED_BY_PEER.ErrorCode << std::dec << std::endl;
                return QUIC_STATUS_SUCCESS;

            case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
                std::cout << "Connection shutdown complete" << std::endl;
                Running = false;
                return QUIC_STATUS_SUCCESS;

            case QUIC_CONNECTION_EVENT_STREAMS_AVAILABLE:
                std::cout << "Streams available" << std::endl;
                return QUIC_STATUS_SUCCESS;

            case QUIC_CONNECTION_EVENT_PEER_NEEDS_STREAMS:
                std::cout << "Peer needs streams" << std::endl;
                return QUIC_STATUS_SUCCESS;
                
            case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
                std::cout << "Peer stream started" << std::endl;
                return QUIC_STATUS_SUCCESS;
                
            case QUIC_CONNECTION_EVENT_DATAGRAM_STATE_CHANGED:
                std::cout << "Datagram state changed" << std::endl;
                return QUIC_STATUS_SUCCESS;
                
            case QUIC_CONNECTION_EVENT_RESUMED:
                std::cout << "Connection resumed" << std::endl;
                return QUIC_STATUS_SUCCESS;
                
            default:
                std::cout << "Unknown event: " << Event->Type << std::endl;
                return QUIC_STATUS_SUCCESS;
        }
    }

public:
    QuicClient() : Running(false) {
        MsQuic = nullptr;
        Registration = nullptr;
        Connection = nullptr;
    }

    bool Initialize() {
        if (QUIC_FAILED(MsQuicOpen2(&MsQuic))) {
            std::cerr << "Failed to open MsQuic" << std::endl;
            return false;
        }

        QUIC_REGISTRATION_CONFIG RegConfig = {
            "TeleopClient",
            QUIC_EXECUTION_PROFILE_LOW_LATENCY
        };

        if (QUIC_FAILED(MsQuic->RegistrationOpen(&RegConfig, &Registration))) {
            std::cerr << "Failed to open registration" << std::endl;
            return false;
        }

        return true;
    }

    bool Connect(const char* ServerName) {
        if (QUIC_FAILED(MsQuic->ConnectionOpen(Registration, ClientCallback, this, &Connection))) {
            std::cerr << "Failed to open connection" << std::endl;
            return false;
        }

        // Setup ALPN buffer
        const char* alpnStr = "teleop";
        QUIC_BUFFER alpn;
        alpn.Buffer = (uint8_t*)alpnStr;
        alpn.Length = (uint32_t)strlen(alpnStr);

        // Create a configuration for the connection
        HQUIC Configuration = nullptr;
        QUIC_SETTINGS Settings = {0};
        
        // Set SendBufferingEnabled flag in IsSetFlags and set it to true
        Settings.IsSet.SendBufferingEnabled = 1;
        Settings.SendBufferingEnabled = 1;
        
        // Customize other settings for better logging
        Settings.IsSet.KeepAliveIntervalMs = 1;
        Settings.KeepAliveIntervalMs = 1000; // Send keep-alive every second
        
        Settings.IsSet.DisconnectTimeoutMs = 1;
        Settings.DisconnectTimeoutMs = 30000; // 30 seconds disconnect timeout
        
        std::cout << "Creating configuration with ALPN: " << alpnStr << std::endl;
        if (QUIC_FAILED(MsQuic->ConfigurationOpen(Registration, &alpn, 1, &Settings, sizeof(Settings), nullptr, &Configuration))) {
            std::cerr << "Failed to open configuration" << std::endl;
            return false;
        }

        // Disable certificate validation for testing
        QUIC_CREDENTIAL_CONFIG CredConfig;
        memset(&CredConfig, 0, sizeof(CredConfig));
        CredConfig.Type = QUIC_CREDENTIAL_TYPE_NONE;
        CredConfig.Flags = QUIC_CREDENTIAL_FLAG_CLIENT | QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;

        std::cout << "Loading credentials" << std::endl;
        if (QUIC_FAILED(MsQuic->ConfigurationLoadCredential(Configuration, &CredConfig))) {
            std::cerr << "Failed to load credentials" << std::endl;
            MsQuic->ConfigurationClose(Configuration);
            return false;
        }
        
        std::cout << "Connecting to server: " << ServerName << std::endl;
        if (QUIC_FAILED(MsQuic->ConnectionStart(Connection, Configuration, QUIC_ADDRESS_FAMILY_INET, ServerName, 4433))) {
            std::cerr << "Failed to start connection" << std::endl;
            MsQuic->ConfigurationClose(Configuration);
            return false;
        }

        // Close the configuration as we don't need it anymore
        MsQuic->ConfigurationClose(Configuration);

        Running = true;
        return true;
    }

    void SendControlCommand(float linear_velocity, float angular_velocity) {
        flatbuffers::FlatBufferBuilder builder;
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        auto command = Teleop::CreateControlCommand(
            builder,
            linear_velocity,
            angular_velocity,
            timestamp
        );
        builder.Finish(command);

        // TODO: Send the serialized command over QUIC
        // This would involve creating a stream and sending the data
    }

    void Run() {
        while (Running) {
            // Example: Send a control command every 100ms
            SendControlCommand(0.5f, 0.0f); // Move forward
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    ~QuicClient() {
        if (Connection) {
            MsQuic->ConnectionClose(Connection);
        }
        if (Registration) {
            MsQuic->RegistrationClose(Registration);
        }
        if (MsQuic) {
            MsQuicClose(MsQuic);
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <server_name>" << std::endl;
        return 1;
    }

    QuicClient client;
    if (!client.Initialize()) {
        return 1;
    }

    if (!client.Connect(argv[1])) {
        return 1;
    }

    client.Run();
    return 0;
} 