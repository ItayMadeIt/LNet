#include "LNetServer.hpp"
#include "LNetClient.hpp"
#include <iostream>
#include <cstring>
#include <thread>

using namespace lnet;

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

int main()
{
    unsigned short port = 12345;

    std::atomic<bool> isRunning(true);

    MessageIdentifier helloMessage(0, 0);

    std::thread serverThread(
        [&]() 
        {
            Server server(1000, 254);

            server.init(port);


            server.setMessageCallback(helloMessage,
                [&](const LNet4Byte& id, Message& message)
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
            Client client(254);
         
            client.connect(port, "127.0.0.1");

            int i = 0;

            while (client.isConnected())
            {
                if (i == 12)
                {
                    std::cout << "CLIENT SENT MESSAGE\n";
                    client.sendUnreliable(helloMessage, "Hi");
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