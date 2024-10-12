#include "LNetClient.hpp"
#include "LNetServer.hpp"
#include "LNetMessage.hpp"

using namespace lnet;

const unsigned short PORT = 12345;
const size_t CLIENTS_AMOUNT = 10;
const size_t THREADS = 3;

int main()
{
	Server server = Server(PORT, THREADS);

	server.startServer();



	server.stopServer();
}



