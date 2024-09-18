#include "NetworkMessage.hpp"
#include "NetworkClient.hpp"
#include "NetworkServer.hpp"
#include <iostream>
#include <fstream>
#include <string>

// Logger function to write to log file
void logMessage(const std::string& message)
{
    std::ofstream logFile("server_log.txt", std::ios::app);
    if (logFile.is_open()) {
        logFile << message << std::endl;
        logFile.close();
    }
    else {
        std::cerr << "Failed to open log file." << std::endl;
    }
}

// Message types
enum class MessageType : uint32_t
{
    Type1 = 1,
    Type2 = 2,
    Type3 = 3,
};

// Server-side function to handle client messages
void handleServerRead(lnet::NetworkServer<10>* server, std::shared_ptr<asio::ip::tcp::socket> client, std::shared_ptr<lnet::Message> msg, const asio::error_code& ec)
{
    if (ec) {
        std::cerr << "Error reading from client: " << ec.message() << std::endl;
        return;
    }

    // Extract message type
    uint32_t type;
    msg >> type;

    // Extract string payload
    std::string clientMessage;
    msg >> clientMessage;

    // Log the interaction
    std::string logEntry = "Received message from client: Type: " + std::to_string(type) + " Message: " + clientMessage;
    std::cout << logEntry << std::endl;
    logMessage(logEntry);

    // Respond with a formatted message
    auto response = std::make_shared<lnet::Message>();
    response << (std::string("Type: " + std::to_string(type) + " Sentence: " + clientMessage + "\n"));
    server->sendAllClients(response);
}

// Client-side function to interact with the server
void clientInteraction(lnet::NetworkClient& client)
{
    std::cout << "Enter message type (1, 2, or 3): ";
    uint32_t msgType;
    std::cin >> msgType;

    std::cin.ignore(); // Clear the newline from the input buffer

    std::cout << "Enter your message: ";
    std::string userMessage;
    std::getline(std::cin, userMessage);

    // Create and send the message
    auto msg = std::make_shared<lnet::Message>();
    msg << msgType << userMessage;
    client.send(msg);
}

int main()
{
    std::string mode;
    std::cout << "Start as (server/client): ";
    std::cin >> mode;

    if (mode == "server") {
        // Server code
        lnet::NetworkServer<10> server(12345, 4,
            [](lnet::NetworkServer<10>* server, std::shared_ptr<asio::ip::tcp::socket> client, const asio::error_code& ec) {
                if (!ec) {
                    std::cout << "New client connected!" << std::endl;
                }
            },
            handleServerRead,
            nullptr);

        server.startServer();
        std::cout << "Server started on port 12345." << std::endl;

        // Keep the server running
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

    }
    else if (mode == "client") {
        // Client code
        lnet::NetworkClient client("127.0.0.1", 12345, 2,
            nullptr,
            [](lnet::NetworkClient* client, std::shared_ptr<lnet::Message> msg, const asio::error_code ec)
            {
                std::string msgString;
                msg >> msgString;

                std::cout << msgString << "\n";
            });

        client.connect();

        // Allow the client to send multiple messages
        while (true) {
            clientInteraction(client);
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    }

    return 0;
}
