#ifndef LNET_TYPES_HPP
#define LNET_TYPES_HPP

#include <cstdint>
#include <asio.hpp>

namespace lnet
{
	using LNetByte = uint8_t;
	using LNet2Byte = uint16_t;
	using LNet4Byte = uint32_t;

	using TCPSocket = asio::ip::tcp::socket;
	using TCPEndpoint = asio::ip::tcp::endpoint;

	using UDPSocket = asio::ip::udp::socket;
	using UDPEndpoint = asio::ip::udp::endpoint;

}

#endif