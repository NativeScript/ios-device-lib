#include "ServerHelper.h";

std::vector<DeviceServerData*> servers;

struct sockaddr_in bind_socket(SOCKET socket, const char* host)
{
	struct sockaddr_in server_address;
	socklen_t address_length = sizeof(server_address);

	server_address.sin_family = AF_INET;

#ifdef _WIN32
	server_address.sin_addr.s_addr = inet_pton(server_address.sin_family, host, (void*)&server_address.sin_addr);
#else
	server_address.sin_addr.s_addr = inet_addr(host);
#endif // _WIN32

	// Find available port.
	int port = 1111;
	server_address.sin_port = htons(port);

	// If the port is available the bind() will return 0.
	while (bind(socket, (sockaddr*)&server_address, address_length) != 0)
	{
		++port;
		server_address.sin_port = htons(port);
	}

	return server_address;
}

DeviceServerData create_server(SOCKET device_socket, const char* host)
{
# ifdef _WIN32
	// Start WinSock2
	WSADATA wsa_data;
	WORD dll_version = MAKEWORD(2, 1);
	if (WSAStartup(dll_version, &wsa_data) != 0)
	{
		return { 0, 0, NULL };
	}
#endif // _WIN32

	SOCKET server_socket = socket(AF_INET, SOCK_STREAM, NULL);

	struct sockaddr_in server_address = bind_socket(server_socket, host);

	listen(server_socket, SOMAXCONN);

	DeviceServerData result = { server_socket, device_socket, server_address };

	servers.push_back(&result);

	return result;
}
