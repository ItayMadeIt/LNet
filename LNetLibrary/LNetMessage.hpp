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
	constexpr size_t LNET_TYPE_SIZE = 4;
	constexpr size_t LNET_SIZE_SIZE = 4;
	constexpr size_t LNET_HEADER_SIZE = LNET_TYPE_SIZE + LNET_SIZE_SIZE;

	enum class MessageSizes
	{
		Size1Byte = 1,
		Size2Byte = 2,
		Size4Byte = 4,
	};

	// Define a packed header structure
#pragma pack(push, 1)
	struct MessageHeader
	{
		LNet4Byte type;
		LNet4Byte size;
	};
#pragma pack(pop)

	class Message
	{
	private:
		MessageHeader header;  // Combined header (type and size)
		MessageHeader netHeader;  // Combined network orderer header (type and size)
		std::vector<LNetByte> payload;  // Payload follows after the header
		size_t readPosition = 0; // To track the current read position in the payload
		MessageSizes inputSize = MessageSizes::Size4Byte;
		MessageSizes outputSize = MessageSizes::Size4Byte;

	public:
		Message() : header{ 0, LNET_HEADER_SIZE } {}  // Default constructor

		Message(LNet4Byte type) : header{ type, LNET_HEADER_SIZE } {}

		Message(asio::mutable_buffer buffer)
		{
			if (buffer.size() < LNET_HEADER_SIZE)
				throw std::runtime_error("Buffer too small to contain header.");

			const LNetByte* ptr = static_cast<const LNetByte*>(buffer.data());
			std::memcpy(&header, ptr, LNET_HEADER_SIZE);
			header.type = LNetEndiannessHandler::fromNetworkEndian(header.type);
			header.size = LNetEndiannessHandler::fromNetworkEndian(header.size);

			if (header.size > LNET_HEADER_SIZE)
			{
				payload.resize(header.size - LNET_HEADER_SIZE);
				std::memcpy(payload.data(), ptr + LNET_HEADER_SIZE, payload.size());
			}
		}

		Message(asio::const_buffer buffer)
		{
			if (buffer.size() < LNET_HEADER_SIZE)
				throw std::runtime_error("Buffer too small to contain header.");

			const LNetByte* ptr = static_cast<const LNetByte*>(buffer.data());
			std::memcpy(&header, ptr, LNET_HEADER_SIZE);
			header.type = LNetEndiannessHandler::fromNetworkEndian(header.type);
			header.size = LNetEndiannessHandler::fromNetworkEndian(header.size);

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

		MessageHeader& getHeader()
		{
			return header;
		}

		std::vector<LNetByte>& getPayload()
		{
			return payload;
		}

		// Prepare data for network transmission (converts header to network byte order)
		std::vector<asio::const_buffer> toConstBuffers()
		{
			// Create a network order header using the EndiannessHandler
			netHeader.type = LNetEndiannessHandler::toNetworkEndian(header.type);
			netHeader.size = LNetEndiannessHandler::toNetworkEndian(header.size);

			std::vector<asio::const_buffer> buffers;
			buffers.push_back(asio::buffer(&netHeader, sizeof(MessageHeader)));

			if (readPosition < payload.size())
				buffers.push_back(asio::buffer(payload.data() + readPosition, payload.size() - readPosition));

			return buffers;
		}

		// Prepare data for network transmission (converts header to network byte order)
		std::vector<asio::mutable_buffer> toMutableBuffers()
		{
			// Create a network order header using the EndiannessHandler
			MessageHeader netHeader;
			netHeader.type = LNetEndiannessHandler::toNetworkEndian(header.type);
			netHeader.size = LNetEndiannessHandler::toNetworkEndian(header.size);

			std::vector<asio::mutable_buffer> buffers;
			buffers.push_back(asio::buffer(&netHeader, sizeof(MessageHeader)));

			if (!payload.empty())
			{
				buffers.push_back(asio::buffer(payload.data() + readPosition, payload.size() - readPosition));
			}

			return buffers;
		}


		// Input values
		template<typename T>
		Message& operator <<(const T& value)
		{
			// Verify value can be converted
			static_assert(std::is_trivial<T>::value && std::is_standard_layout<T>::value,
				"Only trivial types can be added to the payload");

			size_t sizeBefore = payload.size();
			payload.resize(sizeBefore + sizeof(T));

			std::memcpy(payload.data() + sizeBefore, &value, sizeof(T));

			header.size += sizeof(T);

			return *this;
		}

		// Input string
		Message& operator <<(const std::string& value)
		{
			size_t sizeBefore = payload.size();
			payload.resize(sizeBefore + (value.length() + 1));

			std::memcpy(payload.data() + sizeBefore, value.c_str(), value.length() + 1);

			header.size += value.length() + 1;

			return *this;
		}

		// Define input size
		Message& operator <<(const MessageSizes& size)
		{
			inputSize = size;
		}

		// Input list
		template<typename T>
		Message& operator <<(const std::vector<T>& list)
		{
			// put the size as input size:
			switch (inputSize)
			{
				case MessageSizes::Size1Byte:
				{
					LNetByte length = static_cast<LNetByte>(list.size());
					*this << length;
					break;
				}
				case MessageSizes::Size2Byte:
				{
					LNet2Byte length = static_cast<LNet2Byte>(list.size());
					*this << length;
					break;
				}
				case MessageSizes::Size4Byte:
				{
					LNet4Byte length = static_cast<LNet4Byte>(list.size());
					*this << length;
					break;
				}
				default:
				{
					throw std::runtime_error("Undefined Message List Size");
				}
			}

			// input every value in the list
			for (auto& v : list)
			{
				*this << v;
			}

			return *this;
		}

		// Input array
		template<typename T, size_t SIZE>
		Message& operator <<(const std::array<T, SIZE>& arr)
		{
			// Input every value in the list
			for (auto& v : arr)
			{
				*this << v;
			}

			return *this;
		}


		// Output values
		template<typename T>
		Message& operator >>(T& value)
		{
			// Verify value can be converted
			static_assert(std::is_trivial<T>::value && std::is_standard_layout<T>::value,
				"Only trivial types can be added to the payload");

			size_t sizeBefore = payload.size();
			// Verify it can be taken as input
			if (readPosition + sizeof(T) > payload.size())
			{
				throw std::runtime_error("Not enough data in payload to extract type.");
			}

			std::memcpy(reinterpret_cast<void*>(&value), payload.data(), sizeof(T));

			readPosition += sizeof(T);

			header.size -= sizeof(T);

			return *this;
		}

		// Output string 
		Message& operator >>(std::string& value)
		{
			size_t sizeBefore = payload.size();
			auto nullTermPos = std::find(payload.begin(), payload.end(), '\0');

			if (nullTermPos == payload.end()) {
				throw std::runtime_error("No null terminator found, string is incomplete.");
			}

			size_t stringLength = nullTermPos - payload.begin();
			value.assign(reinterpret_cast<const char*>(payload.data()), stringLength);

			readPosition += stringLength + 1;

			header.size -= stringLength + 1;

			return *this;
		}

		// Define output size
		Message& operator >>(const MessageSizes size)
		{
			outputSize = size;
		}

		// Output list 
		template<typename T>
		Message& operator >>(std::vector<T>& list)
		{
			// value to hold length
			size_t length = 0;

			// gather length
			switch (outputSize)
			{
			case lnet::MessageSizes::Size1Byte:
			{
				LNetByte value;
				*this >> value;
				length = value;
				break;
			}
			case lnet::MessageSizes::Size2Byte:
			{
				LNet2Byte value;
				*this >> value;
				length = value;
				break;
			}
			case lnet::MessageSizes::Size4Byte:
			{
				LNet4Byte value;
				*this >> value;
				length = value;
				break;
			}
			default:
			{
				throw std::runtime_error("Undefined Message List Size");
			}
			}

			// set list length
			list.resize(length);

			// add all values
			for (size_t i = 0; i < length; i++)
			{
				*this >> list[i];
			}
		}

		// Output array
		template<typename T, size_t SIZE>
		Message& operator >>(std::array<T, SIZE>& arr)
		{
			// add all values
			for (size_t i = 0; i < SIZE; i++)
			{
				*this >> arr[i];
			}
		}


		// Print
		friend std::ostream& operator<<(std::ostream& os, const Message& msg)
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
		friend std::ostream& operator<<(std::ostream& os, const std::shared_ptr<Message> msg)
		{
			os << "---------------------------------------------\n"\
				"Type: " << msg->getMsgType() << '\n' <<
				"Length: " << msg->getMsgSize() << '\n' <<
				"-----------------------------------------------\n"\
				"PAYLOAD: \n";

			for (size_t i = 0; i < msg->getMsgSize() - LNET_HEADER_SIZE; i++)
			{
				os << std::hex << std::setw(2) << std::setfill('0') << (int)(msg->getPayload()[i]) << " ";
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
		std::shared_ptr<Message> msg,
		std::function<void(std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<Message>, const asio::error_code&)> callback = nullptr)
	{
		std::vector<asio::const_buffer> writeBuffer = msg->toConstBuffers();

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
		std::function<void(std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<Message>, const asio::error_code&)> callback = nullptr)
	{
		auto msg = std::make_shared<Message>();

		// First catch the header 
		asio::mutable_buffer headerBuffer = asio::buffer(reinterpret_cast<LNetByte*>(&msg->getHeader()), LNET_HEADER_SIZE);

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

				// Get the header values from memory
				msg->setMsgType(LNetEndiannessHandler::fromNetworkEndian(msg->getMsgType()));
				msg->setMsgSize(LNetEndiannessHandler::fromNetworkEndian(msg->getMsgSize()));


				std::size_t payloadSize = msg->getMsgSize() - LNET_HEADER_SIZE;
				asio::mutable_buffer payloadBuffer = asio::buffer(msg->getPayload().data(), payloadSize);

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

	template<typename T>
	std::shared_ptr<Message>& operator <<(std::shared_ptr<Message>& msgPtr, const T& value)
	{
		if (!msgPtr)
		{
			throw std::runtime_error("Null message pointer in << operator.");
		}

		*msgPtr << value;

		return msgPtr;
	}
	template<typename T>
	std::shared_ptr<Message>& operator >>(std::shared_ptr<Message>& msgPtr, T& value)
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