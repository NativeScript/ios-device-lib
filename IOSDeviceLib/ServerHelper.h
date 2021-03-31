#pragma once

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#endif // _WIN32

#include "SocketHelper.h"

struct DeviceServerData {
  SOCKET server_socket;
  SOCKET device_socket;
  struct sockaddr_in server_address;

  ~DeviceServerData() {
    close_socket(server_socket);
    close_socket(device_socket);
  }
};

struct sockaddr_in bind_socket(SOCKET socket, const char *host);
DeviceServerData *create_server(SOCKET device_socket, const char *host);
struct DeviceServerData;