#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#include "Declarations.h"
#endif // _WIN32

#include <algorithm>
#include <vector>

struct DeviceServerData {
	SOCKET server_socket;
	SOCKET device_socket;
	struct sockaddr_in server_address;

	~DeviceServerData()
	{
		// We need closesocket for Windows because the socket
		// is a handle to kernel object instead of *nix file descriptor
		// http://stackoverflow.com/questions/35441815/are-close-and-closesocket-interchangable
#ifdef _WIN32
		closesocket(server_socket);
		closesocket(device_socket);
#else
		close(server_socket);
		close(device_socket);
#endif // _WIN32
	}
};

struct sockaddr_in bind_socket(SOCKET socket, const char* host);
DeviceServerData create_server(SOCKET device_socket, const char* host);
