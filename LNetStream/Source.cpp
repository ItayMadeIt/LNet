#include "LNetServer.hpp"
#include "LNetClient.hpp"
#include <iostream>
#include <cstring>
#include <thread>

using namespace lnet;

/*
int main(int argc, char** argv)
{
    if (enet_initialize() != 0) {
        std::cerr << "An error occurred while initializing ENet.\n";
        return 0;
    }

    Server server(100, 16);
    server.init(12345);

    std::thread serverThread([&server]()
        {
            for (int i = 0; i < 400; i++)
            {
                server.tick();  // Process server events
                std::this_thread::sleep_for(std::chrono::milliseconds(10));  // Simulate delay
            }
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    Client client(16);
    client.connect(12345, "127.0.0.1");

    int i = 0;
    for (int i = 0; i < 700; i++)
    {
        client.tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));  // Simulate delay

    } 

    serverThread.join();
    return 0;
    // Loop to handle events until connected
    
    serverThread.join();
    return 0;
}
*/

static const auto startTime = std::chrono::steady_clock::now();

std::string millisecondsSinceStart()
{
    // Static function-local variable to track time elapsed since start
    static auto currentTime = std::chrono::steady_clock::now();
    currentTime = std::chrono::steady_clock::now(); // Update to current time

    // Calculate elapsed time in milliseconds
    auto elapsedMillis = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();

    return "Ms " + std::to_string(elapsedMillis) + " : ";
}

/*
std::atomic<bool> serverRunning(true);

void runServer()
{
    if (enet_initialize() != 0)
    {
        std::cerr << "An error occurred while initializing ENet (Server).\n";
        return;
    }

    ENetAddress address;
    ENetHost* server;
    address.host = ENET_HOST_ANY;
    address.port = 12345;

    server = enet_host_create(&address, 32, 2, 0, 0);
    if (server == nullptr)
    {
        std::cerr << "An error occurred while trying to create the server.\n";
        return;
    }

    std::cout << millisecondsSinceStart() << "Server started on port 12345...\n";

    ENetEvent event;
    while (serverRunning.load())
    {
        while (enet_host_service(server, &event, 0) > 0)
        {
            switch (event.type)
            {
            case ENET_EVENT_TYPE_CONNECT:
                std::cout << millisecondsSinceStart() << "A new client connected from "
                    << (event.peer->address.host & 0xFF) << "."
                    << ((event.peer->address.host >> 8) & 0xFF) << "."
                    << ((event.peer->address.host >> 16) & 0xFF) << "."
                    << ((event.peer->address.host >> 24) & 0xFF) << ":\n";
                break;

            case ENET_EVENT_TYPE_RECEIVE:
                std::cout << millisecondsSinceStart() << "Packet received from client.\n";
                enet_packet_destroy(event.packet);
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                std::cout << millisecondsSinceStart() << "Client disconnected.\n";
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::cout << millisecondsSinceStart() << "Server is shutting down...\n";
    for (ENetPeer* peer = server->peers; peer < &server->peers[server->peerCount]; ++peer)
    {
        if (peer->state == ENET_PEER_STATE_CONNECTED)
        {
            enet_peer_timeout(peer, 32, 500, 1000);  // Shorten timeout
            enet_peer_disconnect(peer, 0);  // Send disconnect message
        }
    }
    enet_host_flush(server);  // Ensure all packets are sent

    enet_host_destroy(server);  // Clean up
    std::cout << "Server shut down and clients disconnected.\n";

    enet_deinitialize();
}
void stopServer()
{
    serverRunning.store(false);  // Change the running flag to false
}


void runClient()
{
    if (enet_initialize() != 0)
    {
        std::cerr << "An error occurred while initializing ENet (Client).\n";
        return;
    }

    ENetHost* client;
    client = enet_host_create(nullptr, 1, 2, 0, 0);
    if (client == nullptr)
    {
        std::cerr << "An error occurred while trying to create the client.\n";
        return;
    }

    ENetAddress address;
    ENetPeer* peer;
    enet_address_set_host(&address, "127.0.0.1");
    address.port = 12345;

    peer = enet_host_connect(client, &address, 2, 0);
    if (peer == nullptr)
    {
        std::cerr << "No available peers for initiating an ENet connection.\n";
        return;
    }

    ENetEvent event;
    while (true)
    {
        while (enet_host_service(client, &event, 0) > 0)
        {
            switch (event.type)
            {
            case ENET_EVENT_TYPE_CONNECT:
                std::cout << millisecondsSinceStart() << "Connected to server.\n";
                break;

            case ENET_EVENT_TYPE_RECEIVE:
                std::cout << millisecondsSinceStart() << "Packet received from server.\n";
                enet_packet_destroy(event.packet);
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                std::cout << millisecondsSinceStart() << "Disconnected from server.\n";
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    enet_host_destroy(client);
    enet_deinitialize();
}

int main()
{

    // Start the server in a separate thread
    std::thread serverThread([]() {
        runServer();
        });

    // Wait for the server to be up and running
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Start the client
    std::thread clientThread([]() {
        runClient();
        });

    // Simulate the server running for a few seconds
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Now stop the server and let the client receive the disconnection event
    stopServer();

    serverThread.join();  // Wait for the server to terminate
    clientThread.join();  // Wait for the client to process disconnection

    return 0;
}

*/


int main()
{
    unsigned short port = 12345;

    std::atomic<bool> isRunning(true);

    std::thread serverThread(
        [&]() 
        {
            Server server(1000, 250);

            server.init(port);

            server.setMessageCallback(22616,
                [&](const LNet4Byte& channel, Message& message)
                {
                    std::string value;
                    message >> value;

                    std::cout << "Got String: " << value << "\n";
                }
            );

            while (isRunning.load())
            {
                server.tick();

                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    );

    // Wait for the server to be up and running
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Start the client
    std::thread clientThread(
        [&]() 
        {
            Client client(250);
         
            client.connect(port, "127.0.0.1");

            int i = 0;

            while (client.isConnected())
            {
                if (i == 12)
                {
                    std::cout << "CLIENT SENT MESSAGE\n\\";
                    client.sendUnreliable(88, 22616, "Hi");
                }
                client.tick();

                
                std::this_thread::sleep_for(std::chrono::milliseconds(10));

                i++;
            }
        }
    );

    // Simulate the server running for a few seconds
    std::this_thread::sleep_for(std::chrono::seconds(25));

    // Now stop the server and let the client receive the disconnection event
    isRunning.store(false);

    std::this_thread::sleep_for(std::chrono::seconds(2));

    serverThread.join();  // Wait for the server to terminate
    clientThread.join();  // Wait for the client to process disconnection

    return 0;
}