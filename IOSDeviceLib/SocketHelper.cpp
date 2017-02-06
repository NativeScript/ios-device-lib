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
