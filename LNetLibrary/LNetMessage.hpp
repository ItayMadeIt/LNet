#ifndef LNET_MESSAGE_HPP
#define LNET_MESSAGE_HPP

#include <asio.hpp>

#include <iostream>

#include <vector>
#include <cstdint>
#include <memory>
#include <iomanip>
#include "LNetEndianHandler.hpp"


namespace lnet
{
	using LNetByte = uint8_t;
	using LNet4Byte = uint32_t;

	constexpr size_t LNET_TYPE_SIZE = 4;
	constexpr size_t LNET_SIZE_SIZE = 4;
	constexpr size_t LNET_HEADER_SIZE = LNET_TYPE_SIZE + LNET_SIZE_SIZE;

	// Define a packed header structure
	#pragma pack(push, 1)
	struct LNetHeader
	{
		LNet4Byte type;
		LNet4Byte size;
	};
	#pragma pack(pop)

	class LNetMessage
	{
	private:
		LNetHeader header;  // Combined header (type and size)
		std::vector<LNetByte> payload;  // Payload follows after the header
		size_t readPosition = 0; // To track the current read position in the payload

	public:
		LNetMessage() : header{ 0, LNET_HEADER_SIZE } {}  // Default constructor

		LNetMessage(LNet4Byte type) : header{ htonl(type), LNET_HEADER_SIZE } {}


		LNetMessage(asio::mutable_buffer buffer)
		{
			if (buffer.size() < LNET_HEADER_SIZE)
				throw std::runtime_error("Buffer too small to contain header.");

			const LNetByte* ptr = static_cast<const LNetByte*>(buffer.data());
			std::memcpy(&header, ptr, LNET_HEADER_SIZE);
			header.type = ntohl(header.type);
			header.size = ntohl(header.size);

			if (header.size > LNET_HEADER_SIZE)
			{
				payload.resize(header.size - LNET_HEADER_SIZE);
				std::memcpy(payload.data(), ptr + LNET_HEADER_SIZE, payload.size());
			}
		}
		LNetMessage(asio::const_buffer buffer)
		{
			if (buffer.size() < LNET_HEADER_SIZE)
				throw std::runtime_error("Buffer too small to contain header.");

			const LNetByte* ptr = static_cast<const LNetByte*>(buffer.data());
			std::memcpy(&header, ptr, LNET_HEADER_SIZE);
			header.type = ntohl(header.type);
			header.size = ntohl(header.size);

			if (header.size > LNET_HEADER_SIZE)
			{
				payload.resize(header.size - LNET_HEADER_SIZE);
				std::memcpy(payload.data(), ptr + LNET_HEADER_SIZE, payload.size());
			}
		}

		void setMsgType(LNet4Byte value)
		{
			header.type = value;
		}
		void setMsgSize(LNet4Byte value)
		{
			header.size = value;
			int payloadSize = value - LNET_HEADER_SIZE;
			payload.resize(payloadSize);
		}
		LNet4Byte getMsgType() const
		{
			return header.type;

		}
		LNet4Byte getMsgSize() const
		{
			return header.size;
		}

		const LNetByte* getHeaderPtr() const
		{
			return reinterpret_cast<const LNetByte*>(&header);
		}

		LNetByte* getPayloadPtr()
		{
			return payload.data();
		}

		// Input values
		template<typename T>
		LNetMessage& operator <<(const T& value)
		{
			// Verify value can be converted
			static_assert(std::is_trivial<T>::value && std::is_standard_layout<T>::value,
				"Only trivial types can be added to the payload");

			size_t sizeBefore = payload.size();
			payload.resize(sizeBefore + sizeof(T));

			std::memcpy(payload.data() + sizeBefore, &value, sizeof(T));

			size += sizeof(T);

			updateMessageData();

			return *this;
		}

		// Input string
		LNetMessage& operator <<(const std::string& value)
		{
			size_t sizeBefore = payload.size();
			payload.resize(sizeBefore + (value.length() + 1));

			std::memcpy(payload.data() + sizeBefore, value.c_str(), value.length() + 1);

			size += value.length() + 1;

			updateMessageData();

			return *this;
		}

		// Output values
		template<typename T>
		LNetMessage& operator >>(T& value)
		{
			// Verify value can be converted
			static_assert(std::is_trivial<T>::value && std::is_standard_layout<T>::value,
				"Only trivial types can be added to the payload");

			size_t sizeBefore = payload.size();
			// Verify it can be taken as input
			if (sizeof(T) > sizeBefore)
			{
				throw std::runtime_error("You can't take out more bytes than there are.");
			}

			std::memcpy(reinterpret_cast<void*>(&value), payload.data(), sizeof(T));

			payload.erase(payload.begin(), payload.begin() + sizeof(T));

			size -= sizeof(T);

			updateMessageData();

			return *this;
		}

		// Output string 
		LNetMessage& operator >>(std::string& value)
		{
			size_t sizeBefore = payload.size();
			auto nullTermPos = std::find(payload.begin(), payload.end(), '\0');

			if (nullTermPos == payload.end()) {
				throw std::runtime_error("No null terminator found, string is incomplete.");
			}

			size_t stringLength = nullTermPos - payload.begin();
			value.assign(reinterpret_cast<const char*>(payload.data()), stringLength);

			payload.erase(payload.begin(), nullTermPos + 1);

			size -= stringLength + 1;

			updateMessageData();

			return *this;
		}


		// Prepare data for network transmission (converts header to network byte order)
		std::vector<asio::const_buffer> toConstBuffers()
		{
			// Create a network order header using the EndiannessHandler
			LNetHeader netHeader;
			netHeader.type = LNetEndiannessHandler::toBigEndian(header.type);
			netHeader.size = LNetEndiannessHandler::toBigEndian(header.size);

			std::vector<asio::const_buffer> buffers;
			buffers.push_back(asio::buffer(&netHeader, sizeof(LNetHeader)));
			if (!payload.empty())
				buffers.push_back(asio::buffer(payload));

			return buffers;
		}

		// Prepare data for network transmission (converts header to network byte order)
		std::vector<asio::mutable_buffer> toMutableBuffers()
		{
			// Create a network order header using the EndiannessHandler
			LNetHeader netHeader;
			netHeader.type = LNetEndiannessHandler::toBigEndian(header.type);
			netHeader.size = LNetEndiannessHandler::toBigEndian(header.size);

			std::vector<asio::mutable_buffer> buffers;
			buffers.push_back(asio::buffer(&netHeader, sizeof(LNetHeader)));
			if (!payload.empty())
				buffers.push_back(asio::buffer(payload));

			return buffers;
		}

		// Print
		friend std::ostream& operator<<(std::ostream& os, const LNetMessage& msg)
		{
			os << "---------------------------------------------\n"\
				"Type: " << msg.getMsgType() << '\n' <<
				"Length: " << msg.getMsgSize() << '\n' <<
				"-----------------------------------------------\n"\
				"PAYLOAD: \n";

			for (const auto& byte : msg.payload) {
				os << std::hex << std::setw(2) << std::setfill('0') << (int)byte << " ";
			}

			os << std::dec;
			return os;
		}
		// Print
		friend std::ostream& operator<<(std::ostream& os, const std::shared_ptr<LNetMessage> msg)
		{
			os << "---------------------------------------------\n"\
				"Type: " << msg->getMsgType() << '\n' <<
				"Length: " << msg->getMsgSize() << '\n' <<
				"-----------------------------------------------\n"\
				"PAYLOAD: \n";

			for (size_t i = 0; i < msg->getMsgSize() - LNET_HEADER_SIZE; i++) {
				os << std::hex << std::setw(2) << std::setfill('0') << (int)(*(msg->getPayloadPtr() + i)) << " ";
			}

			os << std::dec;
			return os;
		}

		void clear()
		{
			header.type = 0;
			header.size = 0;
			payload.clear();
		}

	};


	void asyncWriteMessage(std::shared_ptr<asio::ip::tcp::socket> socket,
		std::shared_ptr<LNetMessage> msg,
		std::function<void(std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<LNetMessage>, const asio::error_code&)> callback = nullptr)
	{
		asio::const_buffer writeBuffer = msg->constBuffer();

		asio::async_write(*socket, writeBuffer,
			[callback, socket, msg](const asio::error_code& ec, std::size_t size)
			{
				if (ec)
				{
					//std::cout << "Failed to write message: \n[" << ec.value() << "] : \'" << ec.message() << "\'\n";
				}

				if (callback) callback(socket, msg, ec);
			}
		);
	}

	void asyncReadMessage(std::shared_ptr<asio::ip::tcp::socket> socket,
		std::function<void(std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<LNetMessage>, const asio::error_code&)> callback = nullptr)
	{
		auto msg = std::make_shared<LNetMessage>();

		// First catch the header 
		asio::mutable_buffer headerBuffer = asio::buffer(msg->getHeaderPtr(), LNET_HEADER_SIZE);

		asio::async_read(*socket, headerBuffer,
			[socket, msg, callback](const asio::error_code& ec, std::size_t size)
			{
				if (ec)
				{
					//std::cout << "Failed to read message header: \n[" << ec.value() << "] " << ec.message() << "\n";

					if (callback)
					{
						callback(socket, msg, ec);
					}
					return;
				}

				if (size != LNET_HEADER_SIZE)
				{
					throw std::runtime_error("Size of read was not enough to fit the header!\n");
				}

				// Get the payload
				msg->setMsgSize(msg->getMsgSize());

				std::size_t payloadSize = msg->getMsgSize() - LNET_HEADER_SIZE;
				asio::mutable_buffer payloadBuffer = asio::buffer(msg->getPayloadPtr(), payloadSize);

				asio::async_read(*socket, payloadBuffer,
					[socket, msg, callback](const asio::error_code& ec, std::size_t size)
					{
						if (ec)
						{
							std::cout << "Failed to read message payload: \n[" << ec.value() << "] : \'" << ec.message() << "\'\n";
							if (callback)
							{
								callback(socket, msg, ec);
							}
							return;
						}

						// Call callback
						if (callback)
						{
							callback(socket, msg, ec);
						}
					}
				);
			}
		);
	}

	void syncWriteMessage(std::shared_ptr<asio::ip::tcp::socket> socket, std::shared_ptr<LNetMessage> sendMsg,
		std::function<void(std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<LNetMessage>, const asio::error_code&)> callback = nullptr)
	{
		asio::error_code ec;

		asio::mutable_buffer writeBuffer = sendMsg->mutableBuffer();
		size_t bytes = asio::write(*socket, writeBuffer, ec);

		if (callback)
		{
			callback(socket, sendMsg, static_cast<const asio::error_code>(ec));
		}
	}

	void syncReadMessage(std::shared_ptr<asio::ip::tcp::socket> socket,
		std::function<void(std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<LNetMessage>, const asio::error_code&)> callback = nullptr)
	{
		asio::error_code ec;

		std::shared_ptr<LNetMessage> msg = std::make_shared<LNetMessage>();

		// First catch the header 
		asio::mutable_buffer headerBuffer = asio::buffer(msg->getHeaderPtr(), LNET_HEADER_SIZE);

		asio::read(*socket, headerBuffer, ec);

		if (ec)
		{
			//std::cout << "Failed to read message header: \n[" << ec.value() << "] " << ec.message() << "\n";

			if (callback)
			{
				callback(socket, msg, ec);
			}

			return;
		}

		// Get the payload
		msg->setMsgSize(msg->getMsgSize());

		asio::mutable_buffer payloadBuffer =
			asio::buffer(msg->getPayloadPtr(), msg->getMsgSize() - LNET_HEADER_SIZE);

		asio::read(*socket, payloadBuffer, ec);
		if (ec)
		{
			//std::cout << "Failed to read message payload: \n[" << ec.value() << "] : \'" << ec.message() << "\'\n";

			if (callback)
			{
				callback(socket, msg, ec);
			}

			return;
		}

		// Call callback
		if (callback)
		{
			callback(socket, msg, ec);
		}
	}


	template<typename T>
	std::shared_ptr<LNetMessage>& operator <<(std::shared_ptr<LNetMessage>& msgPtr, const T& value)
	{
		if (!msgPtr)
		{
			throw std::runtime_error("Null message pointer in << operator.");
		}

		*msgPtr << value;

		return msgPtr;
	}
	template<typename T>
	std::shared_ptr<LNetMessage>& operator >>(std::shared_ptr<LNetMessage>& msgPtr, T& value)
	{
		if (!msgPtr)
		{
			throw std::runtime_error("Null message pointer in >> operator.");
		}

		*msgPtr >> value;

		return msgPtr;
	}

}



#endif