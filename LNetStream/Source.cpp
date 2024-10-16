#include <enet/enet.h>
#include <iostream>
#include <cstring>

int main(int argc, char** argv) {
    if (enet_initialize() != 0) {
        std::cerr << "An error occurred while initializing ENet.\n";
        return EXIT_FAILURE;
    }
    atexit(enet_deinitialize);

    ENetHost* client = enet_host_create(nullptr, 1, 2, 0, 0);
    if (client == nullptr) {
        std::cerr << "An error occurred while trying to create an ENet client host.\n";
        return EXIT_FAILURE;
    }

    ENetAddress address;
    ENetPeer* peer;
    enet_address_set_host(&address, "127.0.0.1");
    address.port = 12345;

    peer = enet_host_connect(client, &address, 2, 0);
    if (peer == nullptr) {
        std::cerr << "No available peers for initiating an ENet connection.\n";
        return EXIT_FAILURE;
    }

    if (enet_host_service(client, nullptr, 5000) > 0 && peer->state == ENET_PEER_STATE_CONNECTED) {
        std::cout << "Connected to server.\n";

        int32_t int2 = 1;
        
        const char* message = "Hello from the client!";

        size_t messageSize = 2 * sizeof(int32_t) + strlen(message) + 1;
        ENetPacket* packet = enet_packet_create(nullptr, messageSize, ENET_PACKET_FLAG_RELIABLE);

        memcpy(packet->data, &int1, sizeof(int32_t));
        memcpy(packet->data + sizeof(int32_t), &int2, sizeof(int32_t));
        memcpy(packet->data + 2 * sizeof(int32_t), message, strlen(message) + 1);

        enet_peer_send(peer, 0, packet);
        enet_host_flush(client);
    }
    else {
        std::cerr << "Connection to server failed.\n";
    }

    enet_peer_disconnect(peer, 0);
    enet_host_flush(client);
    enet_host_destroy(client);
    return 0;
}
