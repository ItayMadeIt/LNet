#include "LNetClient.hpp"
#include "LNetServer.hpp"
#include "LNetMessage.hpp"

using namespace lnet;

const unsigned short PORT = 12345;
const size_t THREADS = 3;

void runServer()
{
	Server server = Server(PORT, THREADS);

	server.startServer();

	bool isRunning = true;

	server.addMsgListener(1,
		[&](Server* server, std::shared_ptr<TCPSocket> socket, std::shared_ptr<lnet::Message> msg)
		{
			std::cout << "Got message!\n";

			std::string line;
			msg >> line;

			std::cout << ": " << line << '\n';

			if (line.find("TCP") != std::string::npos)
			{
				server->sendClient(socket, 1, "TCP recieved");
				std::cout << "sent TCP recieved\n";
			}


		}
	);

	server.addMsgListener(2,
		[&](Server* server, std::shared_ptr<TCPSocket> socket, std::shared_ptr<lnet::Message> msg)
		{
			isRunning = false;
		}
	);

	while (isRunning)
	{
		std::cout << "";
	}

	server.stopServer();
}

void runClient()
{
	Client client("127.0.0.1", PORT);
	client.connect();

	bool isRunning = true;

	client.addMsgListener(1,
		[&](Client* client, std::shared_ptr<Message> msg)
		{
			std::string response;
			msg >> response;
			
			std::cout << "Response: " << response << '\n';

		}
	);


	std::string input;

	while (isRunning)
	{
		getline(std::cin, input);

		if (input.find("stop") != std::string::npos)
		{
			client.sendTCP(2);
		}
		else
		{
			client.sendTCP(1, input);
		}

		input.clear();
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}
}


int main()
{
	runServer();

	return 0;
}



