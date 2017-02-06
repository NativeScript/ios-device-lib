#pragma once

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

#endif

#ifndef _WIN32
#include <sys/socket.h>
#endif

#include <map>
#include "PlistCpp/include/boost/any.hpp"
#include "Declarations.h"

struct LengthEncodedMessage {
	char *message;
	size_t length;

	~LengthEncodedMessage()
	{
		free(message);
	}
};

LengthEncodedMessage get_message_with_encoded_length(const char* message, long long length = -1);
int send_message(const char* message, SOCKET socket, long long length = -1);
int send_message(std::string message, SOCKET socket, long long length = -1);
std::map<std::string, boost::any> receive_message(SOCKET socket, int timeout);
std::map<std::string, boost::any> receive_message(SOCKET socket);
std::string receive_message_raw(SOCKET socket, int size = 1000);
