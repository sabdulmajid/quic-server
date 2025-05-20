#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include "msquic.h"
#include "teleop_generated.h"

// Modern msquic API expects const QUIC_API_TABLE*
class QuicServer {
private:
    const QUIC_API_TABLE* MsQuic;
    HQUIC Registration;
    HQUIC Listener;
    bool Running;

    // Listener callback function
    static QUIC_STATUS QUIC_API ListenerCallback(
        HQUIC Listener,
        void* Context,
        QUIC_LISTENER_EVENT* Event) {
        auto server = static_cast<QuicServer*>(Context);
        return server->HandleListenerEvent(Listener, Event);
    }

    // Connection callback function
    static QUIC_STATUS QUIC_API ServerCallback(
        HQUIC Connection,
        void* Context,
        QUIC_CONNECTION_EVENT* Event) {
        auto server = static_cast<QuicServer*>(Context);
        return server->HandleConnectionEvent(Connection, Event);
    }

    QUIC_STATUS HandleConnectionEvent(HQUIC Connection, QUIC_CONNECTION_EVENT* Event) {
        switch (Event->Type) {
            case QUIC_CONNECTION_EVENT_CONNECTED:
                std::cout << "Client connected" << std::endl;
                std::cout << "  ALPN: " << std::string((const char*)Event->CONNECTED.NegotiatedAlpn, 
                                                     Event->CONNECTED.NegotiatedAlpnLength) << std::endl;
                std::cout << "  Session resumed: " << (Event->CONNECTED.SessionResumed ? "yes" : "no") << std::endl;
                return QUIC_STATUS_SUCCESS;
                
            case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
                std::cout << "Transport shutdown with status: 0x" << std::hex 
                          << Event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status 
                          << ", error code: 0x" << Event->SHUTDOWN_INITIATED_BY_TRANSPORT.ErrorCode 
                          << std::dec << std::endl;
                return QUIC_STATUS_SUCCESS;
                
            case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
                std::cout << "Peer shutdown with error code: 0x" << std::hex 
                          << Event->SHUTDOWN_INITIATED_BY_PEER.ErrorCode 
                          << std::dec << std::endl;
                return QUIC_STATUS_SUCCESS;
                
            case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
                std::cout << "Connection shutdown complete" << std::endl;
                return QUIC_STATUS_SUCCESS;
                
            case QUIC_CONNECTION_EVENT_STREAMS_AVAILABLE:
                std::cout << "Streams available" << std::endl;
                return QUIC_STATUS_SUCCESS;
                
            case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
                std::cout << "Peer stream started" << std::endl;
                return QUIC_STATUS_SUCCESS;
                
            case QUIC_CONNECTION_EVENT_PEER_NEEDS_STREAMS:
                std::cout << "Peer needs streams" << std::endl;
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
    QuicServer() : MsQuic(nullptr), Registration(nullptr), Listener(nullptr), Running(false) {}

    bool Initialize() {
        if (QUIC_FAILED(MsQuicOpen2(&MsQuic))) {
            std::cerr << "Failed to open MsQuic" << std::endl;
            return false;
        }
        QUIC_REGISTRATION_CONFIG RegConfig = {
            "TeleopServer",
            QUIC_EXECUTION_PROFILE_LOW_LATENCY
        };
        if (QUIC_FAILED(MsQuic->RegistrationOpen(&RegConfig, &Registration))) {
            std::cerr << "Failed to open registration" << std::endl;
            return false;
        }
                
        return true;
    }

    bool Start() {
        std::cout << "Opening listener..." << std::endl;
        if (QUIC_FAILED(MsQuic->ListenerOpen(Registration, ListenerCallback, this, &Listener))) {
            std::cerr << "Failed to open listener" << std::endl;
            return false;
        }
        
        QUIC_ADDR address = {};
        QuicAddrSetFamily(&address, QUIC_ADDRESS_FAMILY_INET);
        QuicAddrSetPort(&address, 4433);

        // Setup ALPN buffer for negotiation
        const char* alpnStr = "teleop";
        QUIC_BUFFER alpn;
        alpn.Buffer = (uint8_t*)alpnStr;
        alpn.Length = (uint32_t)strlen(alpnStr);

        std::cout << "Starting listener on port 4433 with ALPN: " << alpnStr << std::endl;
        if (QUIC_FAILED(MsQuic->ListenerStart(Listener, &alpn, 1, &address))) {
            std::cerr << "ListenerStart failed" << std::endl;
            return false;
        }
        
        Running = true;
        std::cout << "Server started on port 4433" << std::endl;
        return true;
    }

    QUIC_STATUS HandleListenerEvent(HQUIC Listener, QUIC_LISTENER_EVENT* Event) {
        switch (Event->Type) {
            case QUIC_LISTENER_EVENT_NEW_CONNECTION: {
                std::cout << "New connection received" << std::endl;
                
                if (Event->NEW_CONNECTION.Info) {
                    std::cout << "  Remote address: IP:";
                    
                    // Get the address in a platform-compatible way
                    if (Event->NEW_CONNECTION.Info->RemoteAddress->Ip.sa_family == QUIC_ADDRESS_FAMILY_INET) {
                        // IPv4
                        std::cout << (int)Event->NEW_CONNECTION.Info->RemoteAddress->Ipv4.sin_addr.s_addr
                                  << ":" << ntohs(Event->NEW_CONNECTION.Info->RemoteAddress->Ipv4.sin_port);
                    } else {
                        // IPv6 or other
                        std::cout << "(IPv6 address)";
                    }
                    std::cout << std::endl;
                    
                    if (Event->NEW_CONNECTION.Info->ServerNameLength > 0) {
                        std::cout << "  Server name: " 
                                  << std::string(Event->NEW_CONNECTION.Info->ServerName, 
                                              Event->NEW_CONNECTION.Info->ServerNameLength) << std::endl;
                    }
                    
                    if (Event->NEW_CONNECTION.Info->NegotiatedAlpnLength > 0) {
                        std::cout << "  Negotiated ALPN: " 
                                  << std::string((const char*)Event->NEW_CONNECTION.Info->NegotiatedAlpn, 
                                              Event->NEW_CONNECTION.Info->NegotiatedAlpnLength) << std::endl;
                    }
                }
                
                // Accept the connection
                MsQuic->SetCallbackHandler(
                    Event->NEW_CONNECTION.Connection,
                    (void*)ServerCallback,
                    this);
                
                // Create configuration for the connection
                const char* alpnStr = "teleop";
                QUIC_BUFFER alpn;
                alpn.Buffer = (uint8_t*)alpnStr;
                alpn.Length = (uint32_t)strlen(alpnStr);
                
                HQUIC Configuration = nullptr;
                QUIC_SETTINGS Settings = {0};
                Settings.IsSet.SendBufferingEnabled = 1;
                Settings.SendBufferingEnabled = 1;
                
                // Customize other settings for better logging
                Settings.IsSet.KeepAliveIntervalMs = 1;
                Settings.KeepAliveIntervalMs = 1000; // Send keep-alive every second
                
                Settings.IsSet.DisconnectTimeoutMs = 1;
                Settings.DisconnectTimeoutMs = 30000; // 30 seconds disconnect timeout
                
                std::cout << "Creating configuration with ALPN: " << alpnStr << std::endl;
                if (QUIC_FAILED(MsQuic->ConfigurationOpen(Registration, &alpn, 1, 
                                                         &Settings, sizeof(Settings), 
                                                         nullptr, &Configuration))) {
                    std::cerr << "Failed to open configuration for connection" << std::endl;
                    return QUIC_STATUS_INTERNAL_ERROR;
                }
                
                // Use a simple credential setup for testing
                QUIC_CREDENTIAL_CONFIG CredConfig;
                memset(&CredConfig, 0, sizeof(CredConfig));
                CredConfig.Type = QUIC_CREDENTIAL_TYPE_NONE;
                CredConfig.Flags = QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;
                
                std::cout << "Loading credentials" << std::endl;
                if (QUIC_FAILED(MsQuic->ConfigurationLoadCredential(Configuration, &CredConfig))) {
                    std::cerr << "Failed to load credentials for connection" << std::endl;
                    MsQuic->ConfigurationClose(Configuration);
                    return QUIC_STATUS_INTERNAL_ERROR;
                }
                
                // Set the configuration on the connection
                std::cout << "Setting connection configuration" << std::endl;
                if (QUIC_FAILED(MsQuic->ConnectionSetConfiguration(
                    Event->NEW_CONNECTION.Connection, Configuration))) {
                    std::cerr << "Failed to set configuration on connection" << std::endl;
                    MsQuic->ConfigurationClose(Configuration);
                    return QUIC_STATUS_INTERNAL_ERROR;
                }
                
                MsQuic->ConfigurationClose(Configuration);
                return QUIC_STATUS_SUCCESS;
            }
            default:
                std::cout << "Unknown listener event: " << Event->Type << std::endl;
                return QUIC_STATUS_SUCCESS;
        }
    }

    void Run() {
        while (Running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    ~QuicServer() {
        if (Listener) {
            MsQuic->ListenerClose(Listener);
        }
        if (Registration) {
            MsQuic->RegistrationClose(Registration);
        }
        if (MsQuic) {
            MsQuicClose(MsQuic);
        }
    }
};

int main() {
    QuicServer server;
    if (!server.Initialize()) {
        return 1;
    }
    if (!server.Start()) {
        return 1;
    }
    server.Run();
    return 0;
} 