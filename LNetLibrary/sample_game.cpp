#include "LNetMessage.hpp"
#include "LNetClient.hpp"
#include "LNetServer.hpp"
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

int main()
{
    std::string mode;
    std::cout << "Start as (server/client): ";
    std::cin >> mode;

    if (mode == "server") {
        // Server code
        lnet::LNetServer<10> server(12345, 4);

        server.startServer();
        std::cout << "Server started on port 12345." << std::endl;

        server.addMsgListener(1,
            [](lnet::LNetServer<10>* server, std::shared_ptr < asio::ip::tcp::socket > client, std::shared_ptr<lnet::LNetMessage> msg)
            {
                msg->setMsgType(2);
                server->sendAllClientsExcept(client, msg);
            }
        );

        // Keep the server running
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

    }
    else if (mode == "client") {
        // Client code
        lnet::LNetClient client("127.0.0.1", 12345, 2);
        client.connect();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (!client.getIsConnected())
        {
            std::cout << "FAILED TO ESTABLISH CONNECTION\n";
            return 0;
        }

        client.addMsgListener(2,
            [](lnet::LNetClient* client, std::shared_ptr<lnet::LNetMessage> msg)
            {
                std::string v;
                msg >> v;
                std::cout << "Client: " << v << std::endl;
            }
        );
        // Allow the client to send multiple messages
        while (true) 
        {
            std::string userMessage;
            std::getline(std::cin, userMessage);

            // Create and send the message
            client.send(1, userMessage);

            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    }
    return 0;
}
