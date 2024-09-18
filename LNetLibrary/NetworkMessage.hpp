#ifndef L_NETWORK_MSG_HPP
#define L_NETWORK_MSG_HPP

#include <asio.hpp>

#include <iostream>

#include <vector>
#include <cstdint>
#include <memory>
#include <iomanip>


namespace lnet
{
	using LNetByte = uint8_t;
	using LNet4Byte = uint32_t;

#define LNET_TYPE_SIZE 4
#define LNET_SIZE_SIZE 4
#define LNET_HEADER_SIZE 8

	class Message
	{
	private:
		alignas(4) LNet4Byte type;  // Align type on a 4-byte boundary
		alignas(4) LNet4Byte size;  // Align size on a 4-byte boundary
		std::vector<LNetByte> payload;  // Payload follows after
		std::vector<LNetByte> messageData; // All values together

		void updateMessageData()
		{
			messageData.resize(LNET_HEADER_SIZE + payload.size());
			std::memcpy(messageData.data(), &type, LNET_HEADER_SIZE);
			std::memcpy(messageData.data() + LNET_HEADER_SIZE, payload.data(), payload.size());
		}

	public:
		// include type and size both 4 bytes
		Message() : type(0), size(8)
		{
			payload.clear();
		}
		Message(LNet4Byte type) : type(type), size(8)
		{
			payload.clear();
		}

		Message(asio::mutable_buffer buffer)
		{
			// Include type and size both 4 bytes + payload size
			size = buffer.size();
			LNetByte* ptr = static_cast<LNetByte*>(buffer.data());

			payload = std::vector<LNetByte>(size - LNET_HEADER_SIZE);
			std::memcpy(&type, ptr, LNET_TYPE_SIZE);
			std::memcpy(&size, ptr + LNET_TYPE_SIZE, LNET_SIZE_SIZE);
			std::memcpy(payload.data(), ptr + LNET_HEADER_SIZE, payload.size());

			updateMessageData();
		}
		Message(asio::const_buffer buffer)
		{
			// Include type and size both 4 bytes + payload size
			size = buffer.size();
			const LNetByte* ptr = static_cast<const LNetByte*>(buffer.data());

			payload = std::vector<LNetByte>(size - LNET_HEADER_SIZE);
			std::memcpy(&type, ptr, LNET_TYPE_SIZE);
			std::memcpy(&size, ptr + LNET_TYPE_SIZE, LNET_SIZE_SIZE);
			std::memcpy(payload.data(), ptr + LNET_HEADER_SIZE, payload.size());

			updateMessageData();
		}

		void setMsgType(LNet4Byte value)
		{
			type = value;
		}
		void setMsgSize(LNet4Byte value)
		{
			size = value;
			int payloadSize = size - LNET_HEADER_SIZE;
			payload.resize(payloadSize);
			updateMessageData();
		}
		LNet4Byte getMsgType() const
		{
			return type;

		}
		LNet4Byte getMsgSize() const
		{
			return size;
		}

		LNetByte* getHeaderPtr() const
		{
			// return first value of the alligned header
			return (LNetByte*)(&type);
		}
		LNetByte* getPayloadPtr() const
		{
			// return first value of the alligned header
			return (LNetByte*)payload.data();
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

			size += sizeof(T);

			updateMessageData();

			return *this;
		}

		// Input string
		Message& operator <<(const std::string& value)
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
		Message& operator >>(T& value)
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
		Message& operator >>(std::string& value)
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


		// Turn into asio const buffer
		asio::const_buffer constBuffer()
		{
			updateMessageData();

			return asio::buffer(static_cast<const void*>(messageData.data()), messageData.size());
		}

		// Turn into asio mutable buffer
		asio::mutable_buffer mutableBuffer()
		{
			updateMessageData();

			return asio::buffer(messageData);
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

			for (size_t i = 0; i < msg->getMsgSize() - LNET_HEADER_SIZE; i++) {
				os << std::hex << std::setw(2) << std::setfill('0') << (int)(*(msg->getPayloadPtr() + i)) << " ";
			}

			os << std::dec;
			return os;
		}

		void clear()
		{
			type = 0;
			size = 0;
			payload.clear();
		}

	};


	void asyncWriteMessage(std::shared_ptr<asio::ip::tcp::socket> socket,
		std::shared_ptr<Message> msg,
		std::function<void(std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<Message>, const asio::error_code&)> callback = nullptr)
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
		std::function<void(std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<Message>, const asio::error_code&)> callback = nullptr)
	{
		auto msg = std::make_shared<Message>();

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

	void syncWriteMessage(std::shared_ptr<asio::ip::tcp::socket> socket, std::shared_ptr<Message> sendMsg,
		std::function<void(std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<Message>, const asio::error_code&)> callback = nullptr)
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
		std::function<void(std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<Message>, const asio::error_code&)> callback = nullptr)
	{
		asio::error_code ec;

		std::shared_ptr<Message> msg = std::make_shared<Message>();

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