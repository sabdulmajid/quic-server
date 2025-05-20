#include <iostream>
#include <memory>
#include <unordered_map>
#include <chrono>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include "msquic.h"
#include "teleop_generated.h"
#include <thread>

#define QUIC_STATUS_ACCESS_DENIED 0x8041000E

class QuicProxy {
private:
    const QUIC_API_TABLE* MsQuic;
    HQUIC Registration;
    HQUIC ClientConnection;
    HQUIC ServerConnection;
    bool Running;
    
    // Authentication state
    struct AuthState {
        std::string auth_token;
        std::chrono::system_clock::time_point expires_at;
        std::string client_id;
        std::string robot_id;
    };
    std::unordered_map<std::string, AuthState> auth_states;

    static QUIC_STATUS QUIC_API ClientCallback(
        HQUIC Connection,
        void* Context,
        QUIC_CONNECTION_EVENT* Event) {
        auto proxy = static_cast<QuicProxy*>(Context);
        return proxy->HandleClientEvent(Connection, Event);
    }

    static QUIC_STATUS QUIC_API ServerCallback(
        HQUIC Connection,
        void* Context,
        QUIC_CONNECTION_EVENT* Event) {
        auto proxy = static_cast<QuicProxy*>(Context);
        return proxy->HandleServerEvent(Connection, Event);
    }

    static QUIC_STATUS QUIC_API ListenerCallback(
        HQUIC Listener,
        void* Context,
        QUIC_LISTENER_EVENT* Event) {
        auto proxy = static_cast<QuicProxy*>(Context);
        return proxy->HandleListenerEvent(Listener, Event);
    }

    static QUIC_STATUS QUIC_API StreamCallback(
        HQUIC Stream,
        void* Context,
        QUIC_STREAM_EVENT* Event) {
        auto proxy = static_cast<QuicProxy*>(Context);
        return proxy->HandleStreamEvent(Stream, Event);
    }

    QUIC_STATUS HandleClientEvent(HQUIC Connection, QUIC_CONNECTION_EVENT* Event) {
        switch (Event->Type) {
            case QUIC_CONNECTION_EVENT_CONNECTED:
                std::cout << "Client connected" << std::endl;
                return QUIC_STATUS_SUCCESS;

            case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
                return HandleClientStream(Connection, Event->PEER_STREAM_STARTED.Stream);

            case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
                std::cout << "Client connection shutdown complete" << std::endl;
                return QUIC_STATUS_SUCCESS;

            default:
                return QUIC_STATUS_SUCCESS;
        }
    }

    QUIC_STATUS HandleServerEvent(HQUIC Connection, QUIC_CONNECTION_EVENT* Event) {
        switch (Event->Type) {
            case QUIC_CONNECTION_EVENT_CONNECTED:
                std::cout << "Server connected" << std::endl;
                return QUIC_STATUS_SUCCESS;

            case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
                return HandleServerStream(Connection, Event->PEER_STREAM_STARTED.Stream);

            case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
                std::cout << "Server connection shutdown complete" << std::endl;
                return QUIC_STATUS_SUCCESS;

            default:
                return QUIC_STATUS_SUCCESS;
        }
    }

    QUIC_STATUS HandleListenerEvent(HQUIC Listener, QUIC_LISTENER_EVENT* Event) {
        switch (Event->Type) {
            case QUIC_LISTENER_EVENT_NEW_CONNECTION:
                return HandleNewConnection(Listener, Event->NEW_CONNECTION.Connection);
            default:
                return QUIC_STATUS_SUCCESS;
        }
    }

    QUIC_STATUS HandleNewConnection(HQUIC Listener, HQUIC Connection) {
        if (QUIC_FAILED(MsQuic->SetCallbackHandler(Connection, (QUIC_CONNECTION_CALLBACK_HANDLER)ClientCallback, this))) {
            return QUIC_STATUS_INTERNAL_ERROR;
        }
        return QUIC_STATUS_SUCCESS;
    }

    QUIC_STATUS HandleStreamEvent(HQUIC Stream, QUIC_STREAM_EVENT* Event) {
        switch (Event->Type) {
            case QUIC_STREAM_EVENT_RECEIVE:
                // Process received data
                if (Event->RECEIVE.BufferCount > 0) {
                    const QUIC_BUFFER* buffer = &Event->RECEIVE.Buffers[0];
                    
                    // Verify the packet type and handle accordingly
                    auto packet_type = flatbuffers::GetRoot<Teleop::ControlCommand>(buffer->Buffer);
                    if (packet_type) {
                        return HandleControlCommand(packet_type, Stream);
                    }

                    auto auth_request = flatbuffers::GetRoot<Teleop::AuthRequest>(buffer->Buffer);
                    if (auth_request) {
                        return HandleAuthRequest(auth_request, Stream);
                    }

                    auto sensor_data = flatbuffers::GetRoot<Teleop::SensorData>(buffer->Buffer);
                    if (sensor_data) {
                        return ForwardSensorData(sensor_data, Stream);
                    }
                }
                break;

            case QUIC_STREAM_EVENT_SEND_COMPLETE:
                // Handle send completion
                break;

            case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
                // Handle peer shutdown
                break;

            case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
                // Handle stream shutdown
                break;
        }

        return QUIC_STATUS_SUCCESS;
    }

    QUIC_STATUS HandleClientStream(HQUIC Connection, HQUIC Stream) {
        // Set the stream callback handler
        QUIC_STREAM_CALLBACK_HANDLER handler = StreamCallback;
        if (QUIC_FAILED(MsQuic->SetCallbackHandler(Stream, handler, this))) {
            std::cerr << "Failed to set stream callback" << std::endl;
            return QUIC_STATUS_INTERNAL_ERROR;
        }

        // Start the stream
        if (QUIC_FAILED(MsQuic->StreamStart(Stream, QUIC_STREAM_START_FLAG_NONE))) {
            std::cerr << "Failed to start stream" << std::endl;
            return QUIC_STATUS_INTERNAL_ERROR;
        }

        // Enable receiving data
        if (QUIC_FAILED(MsQuic->StreamReceiveSetEnabled(Stream, TRUE))) {
            std::cerr << "Failed to enable stream receive" << std::endl;
            return QUIC_STATUS_INTERNAL_ERROR;
        }

        return QUIC_STATUS_SUCCESS;
    }

    QUIC_STATUS HandleServerStream(HQUIC Connection, HQUIC Stream) {
        // Set the stream callback handler
        QUIC_STREAM_CALLBACK_HANDLER handler = StreamCallback;
        if (QUIC_FAILED(MsQuic->SetCallbackHandler(Stream, handler, this))) {
            return QUIC_STATUS_INTERNAL_ERROR;
        }

        // Start the stream
        if (QUIC_FAILED(MsQuic->StreamStart(Stream, QUIC_STREAM_START_FLAG_NONE))) {
            return QUIC_STATUS_INTERNAL_ERROR;
        }

        // Enable receiving data
        if (QUIC_FAILED(MsQuic->StreamReceiveSetEnabled(Stream, TRUE))) {
            return QUIC_STATUS_INTERNAL_ERROR;
        }

        return QUIC_STATUS_SUCCESS;
    }

    QUIC_STATUS HandleControlCommand(const Teleop::ControlCommand* command, HQUIC Stream) {
        // Verify authentication
        auto it = auth_states.find(command->client_id()->str());
        if (it == auth_states.end() || 
            it->second.auth_token != command->auth_token()->str() ||
            std::chrono::system_clock::now() > it->second.expires_at) {
            return QUIC_STATUS_ACCESS_DENIED;
        }

        // Forward the command to the server
        return QUIC_STATUS_SUCCESS;
    }

    QUIC_STATUS HandleAuthRequest(const Teleop::AuthRequest* request, HQUIC Stream) {
        // Generate a new auth token
        std::string auth_token = GenerateAuthToken();
        
        // Store the auth state
        AuthState state;
        state.auth_token = auth_token;
        state.expires_at = std::chrono::system_clock::now() + std::chrono::hours(24);
        state.client_id = request->client_id()->str();
        state.robot_id = request->robot_id()->str();
        
        auth_states[state.client_id] = state;

        // Send auth response
        flatbuffers::FlatBufferBuilder builder;
        auto response = Teleop::CreateAuthResponse(
            builder,
            true,
            builder.CreateString(auth_token),
            std::chrono::duration_cast<std::chrono::seconds>(
                state.expires_at.time_since_epoch()).count(),
            builder.CreateString("")
        );
        builder.Finish(response);

        // Send the response
        QUIC_BUFFER sendBuffer;
        sendBuffer.Buffer = builder.GetBufferPointer();
        sendBuffer.Length = builder.GetSize();
        if (QUIC_FAILED(MsQuic->StreamSend(Stream, &sendBuffer, 1, QUIC_SEND_FLAG_FIN, nullptr))) {
            return QUIC_STATUS_INTERNAL_ERROR;
        }

        return QUIC_STATUS_SUCCESS;
    }

    std::string GenerateAuthToken() {
        // Generate a random token
        uint8_t random_bytes[32];
        RAND_bytes(random_bytes, sizeof(random_bytes));
        
        // Convert to hex string
        std::string token;
        char hex[3];
        for (size_t i = 0; i < sizeof(random_bytes); i++) {
            snprintf(hex, sizeof(hex), "%02x", random_bytes[i]);
            token += hex;
        }
        return token;
    }

    QUIC_STATUS ForwardSensorData(const Teleop::SensorData* sensor_data, HQUIC Stream) {
        // Forward sensor data to the client
        flatbuffers::FlatBufferBuilder builder;
        
        // Create Vector2D
        auto position = Teleop::CreateVector2D(
            builder,
            sensor_data->position()->x(),
            sensor_data->position()->y()
        );
        
        // Create Quaternion
        auto orientation = Teleop::CreateQuaternion(
            builder,
            sensor_data->orientation()->x(),
            sensor_data->orientation()->y(),
            sensor_data->orientation()->z(),
            sensor_data->orientation()->w()
        );
        
        auto data = Teleop::CreateSensorData(
            builder,
            sensor_data->sensor_type(),
            position,
            orientation,
            sensor_data->battery_level(),
            sensor_data->temperature(),
            sensor_data->error_code(),
            builder.CreateString(sensor_data->error_message()->str()),
            sensor_data->timestamp(),
            sensor_data->sequence_number(),
            builder.CreateString(sensor_data->robot_id()->str())
        );
        builder.Finish(data);

        QUIC_BUFFER buffer;
        buffer.Buffer = builder.GetBufferPointer();
        buffer.Length = builder.GetSize();

        return MsQuic->StreamSend(Stream, &buffer, 1, QUIC_SEND_FLAG_FIN, nullptr);
    }

public:
    QuicProxy() : Running(false) {
        MsQuic = nullptr;
        Registration = nullptr;
        ClientConnection = nullptr;
        ServerConnection = nullptr;
    }

    bool Initialize() {
        if (QUIC_FAILED(MsQuicOpen2(&MsQuic))) {
            std::cerr << "Failed to open MsQuic" << std::endl;
            return false;
        }

        QUIC_REGISTRATION_CONFIG RegConfig = {
            "TeleopProxy",
            QUIC_EXECUTION_PROFILE_LOW_LATENCY
        };

        if (QUIC_FAILED(MsQuic->RegistrationOpen(&RegConfig, &Registration))) {
            std::cerr << "Failed to open registration" << std::endl;
            return false;
        }

        return true;
    }

    bool Start(const char* ServerName, uint16_t ClientPort, uint16_t ServerPort) {
        // Setup ALPN buffer
        const char* alpnStr = "teleop";
        QUIC_BUFFER alpn;
        alpn.Buffer = (uint8_t*)alpnStr;
        alpn.Length = (uint32_t)strlen(alpnStr);

        // Create configurations for both client and server connections
        HQUIC ClientConfig = nullptr;
        HQUIC ServerConfig = nullptr;
        QUIC_SETTINGS Settings = {0};
        
        Settings.IsSet.SendBufferingEnabled = 1;
        Settings.SendBufferingEnabled = 1;
        Settings.IsSet.KeepAliveIntervalMs = 1;
        Settings.KeepAliveIntervalMs = 1000;
        
        // Create client configuration
        if (QUIC_FAILED(MsQuic->ConfigurationOpen(Registration, &alpn, 1, &Settings, 
                                                 sizeof(Settings), nullptr, &ClientConfig))) {
            std::cerr << "Failed to open client configuration" << std::endl;
            return false;
        }

        // Create server configuration
        if (QUIC_FAILED(MsQuic->ConfigurationOpen(Registration, &alpn, 1, &Settings, 
                                                 sizeof(Settings), nullptr, &ServerConfig))) {
            std::cerr << "Failed to open server configuration" << std::endl;
            MsQuic->ConfigurationClose(ClientConfig);
            return false;
        }

        // Disable certificate validation for testing
        QUIC_CREDENTIAL_CONFIG CredConfig;
        memset(&CredConfig, 0, sizeof(CredConfig));
        CredConfig.Type = QUIC_CREDENTIAL_TYPE_NONE;
        CredConfig.Flags = QUIC_CREDENTIAL_FLAG_CLIENT | QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;

        if (QUIC_FAILED(MsQuic->ConfigurationLoadCredential(ClientConfig, &CredConfig)) ||
            QUIC_FAILED(MsQuic->ConfigurationLoadCredential(ServerConfig, &CredConfig))) {
            std::cerr << "Failed to load credentials" << std::endl;
            MsQuic->ConfigurationClose(ClientConfig);
            MsQuic->ConfigurationClose(ServerConfig);
            return false;
        }

        // Start listening for client connections
        if (QUIC_FAILED(MsQuic->ListenerOpen(Registration, ListenerCallback, this, &ClientConnection))) {
            std::cerr << "Failed to open client listener" << std::endl;
            MsQuic->ConfigurationClose(ClientConfig);
            MsQuic->ConfigurationClose(ServerConfig);
            return false;
        }

        // Start the server connection
        if (QUIC_FAILED(MsQuic->ConnectionOpen(Registration, ServerCallback, this, &ServerConnection))) {
            std::cerr << "Failed to open server connection" << std::endl;
            MsQuic->ConfigurationClose(ClientConfig);
            MsQuic->ConfigurationClose(ServerConfig);
            return false;
        }

        if (QUIC_FAILED(MsQuic->ConnectionStart(ServerConnection, ServerConfig, 
                                               QUIC_ADDRESS_FAMILY_INET, ServerName, ServerPort))) {
            std::cerr << "Failed to start server connection" << std::endl;
            MsQuic->ConfigurationClose(ClientConfig);
            MsQuic->ConfigurationClose(ServerConfig);
            return false;
        }

        // Close configurations as we don't need them anymore
        MsQuic->ConfigurationClose(ClientConfig);
        MsQuic->ConfigurationClose(ServerConfig);

        Running = true;
        return true;
    }

    void Run() {
        while (Running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    ~QuicProxy() {
        if (ClientConnection) {
            MsQuic->ConnectionClose(ClientConnection);
        }
        if (ServerConnection) {
            MsQuic->ConnectionClose(ServerConnection);
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
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <server_name> <client_port> <server_port>" << std::endl;
        return 1;
    }

    QuicProxy proxy;
    if (!proxy.Initialize()) {
        return 1;
    }

    if (!proxy.Start(argv[1], static_cast<uint16_t>(std::atoi(argv[2])), 
                     static_cast<uint16_t>(std::atoi(argv[3])))) {
        return 1;
    }

    proxy.Run();
    return 0;
} 