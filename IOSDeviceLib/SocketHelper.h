#pragma once

#include "ServiceInfo.h"
#include "SocketIncludes.h"
struct LengthEncodedMessage {
  char *message;
  size_t length;

  ~LengthEncodedMessage() {
    delete[] message;
    // free(message);
  }
};

struct Utf16Message {
  std::string message;
};

LengthEncodedMessage get_message_with_encoded_length(const char *message,
                                                     long long length = -1);
int send_message(const char *message, ServiceInfo info, long long length = -1);
int send_message(std::string message, ServiceInfo info, long long length = -1);
std::map<std::string, boost::any> receive_message(SOCKET socket,
                                                  int timeout = 5);
std::string receive_message_raw(ServiceInfo info, int size = 1000);

typedef std::function<void(SOCKET)> SocketClosedCallback;
void proxy_socket_io(SOCKET first, SOCKET second,
                     SocketClosedCallback first_socket_closed_callback,
                     SocketClosedCallback second_socket_closed_callback);
void close_socket(SOCKET socket);
