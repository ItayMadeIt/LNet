#include "LNetServer.hpp"
#include "LNetClient.hpp"
#include "LNetMessage.hpp"

#include <memory>
#include <asio.hpp>

using namespace lnet;

#define MAX_PLAYERS 25

int main()
{
	LNetServer<MAX_PLAYERS> server(12345, 10, nullptr, 
		[](LNetServer<MAX_PLAYERS>* server, std::shared_ptr<asio::ip::tcp::socket> client, std::shared_ptr<lnet::LNetMessage> msg, const asio::error_code& ec)
		{
			std::cout << "BRUHHHH" << '\n';
			std::cout << *msg << '\n';
		}
	);
	server.startServer();

	server.addMsgListener(1,
		[](LNetServer<MAX_PLAYERS>* server, std::shared_ptr<asio::ip::tcp::socket> client, std::shared_ptr<LNetMessage> msg)
		{
			std::string str;
			msg >> str;

			server->sendAllClients(2, str);
		}
	);

	while (true)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}
