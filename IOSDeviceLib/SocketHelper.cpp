#include "SocketHelper.h"

#include "PlistCpp/Plist.hpp"
#include "PlistCpp/PlistDate.hpp"
#include "PlistCpp/include/boost/any.hpp"

int send_message(const char* message, SOCKET socket, long long length)
{
	LengthEncodedMessage length_encoded_message = get_message_with_encoded_length(message, length);
	return send(socket, length_encoded_message.message, length_encoded_message.length, 0);
}

int send_message(std::string message, SOCKET socket, long long length)
{
	return send_message(message.c_str(), socket, length);
}

bool should_read_from_socket(SOCKET socket, int timeout)
{
	// The code is stolen from here http://stackoverflow.com/questions/30395258/setting-timeout-to-recv-function
	fd_set set;
	struct timeval time_to_wait;
	FD_ZERO(&set); /* clear the set */
	FD_SET(socket, &set); /* add our file descriptor to the set */
	time_to_wait.tv_sec = timeout;
	// If we don't set tv_usec this method won't work.
	time_to_wait.tv_usec = 0;
	// We need the + 1 here because we add our socket to the set and the first param of the select (nfds)
	// is the maximum socket number to read from.
	// nfds is the highest-numbered file descriptor in any of the three sets, plus 1 - http://manpages.courier-mta.org/htmlman2/select.2.html
	int rv = select(socket + 1, &set, NULL, NULL, &time_to_wait);
	return rv;
}

std::map<std::string, boost::any> receive_message(SOCKET socket, int timeout)
{

	if (!should_read_from_socket(socket, timeout))
	{
		return std::map<std::string, boost::any>();
	}

	return receive_message(socket);
}

std::map<std::string, boost::any> receive_message(SOCKET socket)
{
	std::map<std::string, boost::any> dict;
	char *buffer = new char[4];
	int bytes_read = recv(socket, buffer, 4, 0);
	unsigned long res = ntohl(*((unsigned long*)buffer));
	delete[] buffer;
	buffer = new char[res];
	bytes_read = recv(socket, buffer, res, 0);
	Plist::readPlist(buffer, res, dict);
	delete[] buffer;
	return dict;
}

std::mutex receive_utf16_message_mutex;
Utf16Message* receive_utf16_message(SOCKET fd, int size = 1000)
{
	receive_utf16_message_mutex.lock();
	// We do not want to stay at recv forever.
	int should_read_from_socket_result = should_read_from_socket(fd, 1);
	if (should_read_from_socket_result == 0)
	{
		Utf16Message* empty_result = new Utf16Message();
		empty_result->message = new unsigned char[0];
		empty_result->length = 0;
		receive_utf16_message_mutex.unlock();
		return empty_result;
	}
	else if (should_read_from_socket_result == -1)
	{
		receive_utf16_message_mutex.unlock();
		return nullptr;
	}

	int bytes_read;
	long long final_length = 0;
	// We need to create new unsigned char[] here because if we don't
	// the memcpy will fail with EXC_BAD_ACCESS
	unsigned char *result = new unsigned char[0];

	do
	{
		unsigned char *buffer = new unsigned char[size];
		bytes_read = recv(fd, (char*)buffer, size, 0);

		if (bytes_read > 0)
		{
			size_t temp_size = final_length + bytes_read;
			unsigned char *temp = new unsigned char[temp_size + 1];

			memcpy(temp, result, final_length);
			memcpy(temp + final_length, buffer, bytes_read);

			// Delete the result because we change it's address to temp's address.
			delete[] result;
			result = temp;
			final_length += bytes_read;
		}
		else if (bytes_read < 0)
		{
			receive_utf16_message_mutex.unlock();
			return nullptr;
		}

		delete[] buffer;
	} while (bytes_read == size);

	Utf16Message* utf16_message = new Utf16Message();
	utf16_message->message = result;
	utf16_message->length = final_length;

	receive_utf16_message_mutex.unlock();

	return utf16_message;
}

void proxy_socket_messages(SOCKET first, SOCKET second)
{
	Utf16Message* message;

	while ((message = receive_utf16_message(first)) != nullptr)
	{
		if (message->length)
		{
#ifdef _WIN32
			char* message_buffer = (char*)message->message;
#else
			unsigned char* message_buffer = message->message;
#endif // _WIN32

			send(second, message_buffer, message->length, NULL);
		}

		// Delete the message to free the message->message memory.
		delete message;
	}
}

void proxy_socket_io(SOCKET first, SOCKET second, SocketClosedCallback first_socket_closed_callback, SocketClosedCallback second_socket_closed_callback)
{
	std::thread([=]() {
		// Send the messages received on the first socket to the second socket.
		proxy_socket_messages(first, second);
		first_socket_closed_callback(first);
	}).detach();

	std::thread([=]() {
		// Send the messages received on the second socket to the first socket.
		proxy_socket_messages(second, first);
		second_socket_closed_callback(second);
	}).detach();
}

std::string receive_message_raw(SOCKET socket, int size)
{
	int bytes_read;
	std::string result = "";
	do
	{
		char *buffer = new char[size];
		bytes_read = recv(socket, buffer, size, 0);
		if (bytes_read > 0)
			result += std::string(buffer, bytes_read);
		delete[] buffer;
	} while (bytes_read == size);

	return result;
}

LengthEncodedMessage get_message_with_encoded_length(const char* message, long long length)
{
	if (length < 0)
		length = strlen(message);

	unsigned long message_len = length + 4;
	char *length_encoded_message = new char[message_len];
	unsigned long packed_length = htonl(length);
	size_t packed_length_size = 4;
	memcpy(length_encoded_message, &packed_length, packed_length_size);
	memcpy(length_encoded_message + packed_length_size, message, length);
	return{ length_encoded_message, message_len };
}
