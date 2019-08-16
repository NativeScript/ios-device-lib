#pragma once

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <ws2tcpip.h>
typedef void* CFDictionaryRef;

#pragma comment(lib, "Ws2_32.lib")
#else
typedef void* HANDLE;
typedef unsigned long long SOCKET;
#endif

#ifndef _WIN32

#include <sys/socket.h>
#include <stdlib.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <map>
#include <string>
#include <functional>
#include "PlistCpp/include/boost/any.hpp"
struct LengthEncodedMessage {
	char *message;
	size_t length;

	~LengthEncodedMessage()
	{
		free(message);
	}
};

struct Utf16Message {
	std::string message;
};

LengthEncodedMessage get_message_with_encoded_length(const char* message, long long length = -1);
int send_message(const char* message, SOCKET socket, long long length = -1);
int send_message(std::string message, SOCKET socket, long long length = -1);
long send_con_message(HANDLE* serviceConnection, CFDictionaryRef message);
std::map<std::string, boost::any> receive_con_message(HANDLE* con);
std::map<std::string, boost::any> receive_message(SOCKET socket, int timeout = 5);
std::string receive_message_raw(SOCKET socket, int size = 1000);

typedef std::function<void(SOCKET)> SocketClosedCallback;
void proxy_socket_io(SOCKET first, SOCKET second, SocketClosedCallback first_socket_closed_callback, SocketClosedCallback second_socket_closed_callback);
void close_socket(SOCKET socket);
