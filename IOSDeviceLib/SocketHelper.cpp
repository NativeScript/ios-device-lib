#include <mutex>
#include <thread>

#include "Constants.h"
#include "Declarations.h"
#include "PlistCpp/Plist.hpp"
#include "PlistCpp/PlistDate.hpp"
#include "SocketHelper.h"

#include "Printing.h"
#include "StringHelper.h"
#include "json.hpp"
using json = nlohmann::json;

#define DEBUG_PACKETS 0

int send_message(const char *message, ServiceInfo info, long long length) {
  if (DEBUG_PACKETS) {
    print(json({{"send_message", message}}));
    print("\n");
  }

  LengthEncodedMessage length_encoded_message =
      get_message_with_encoded_length(message, length);

  std::string encmsg(length_encoded_message.message,
                     length_encoded_message.length);

  return AMDServiceConnectionSend(info.connection,
                                  length_encoded_message.message,
                                  length_encoded_message.length);
}

int send_message(std::string message, ServiceInfo info, long long length) {
  return send_message(message.c_str(), info, length);
}

int get_socket_state(SOCKET socket, int timeout) {
  if (timeout < 0) {
    return 1;
  }

  // The code is stolen from here
  // http://stackoverflow.com/questions/30395258/setting-timeout-to-recv-function
  fd_set set;
  struct timeval time_to_wait;
  FD_ZERO(&set);        /* clear the set */
  FD_SET(socket, &set); /* add our file descriptor to the set */
  time_to_wait.tv_sec = timeout;
  // If we don't set tv_usec this method won't work.
  time_to_wait.tv_usec = 0;
  // We need the + 1 here because we add our socket to the set and the first
  // param of the select (nfds) is the maximum socket number to read from. nfds
  // is the highest-numbered file descriptor in any of the three sets, plus 1 -
  // http://manpages.courier-mta.org/htmlman2/select.2.html
  int rv = select(socket + 1, &set, NULL, NULL, &time_to_wait);
  return rv;
}

std::mutex receive_message_mutex;
std::map<std::string, boost::any> receive_message_core(SOCKET socket) {
  receive_message_mutex.lock();
  std::map<std::string, boost::any> dict;
  char *buffer = new char[4];
  int bytes_read = recv(socket, buffer, 4, 0);
  if (bytes_read > 0) {
    unsigned long res = ntohl(*((unsigned long *)buffer));
    delete[] buffer;
    buffer = new char[res];
    bytes_read = recv(socket, buffer, res, 0);
    if (bytes_read > 0) {
      Plist::readPlist(buffer, res, dict);
    }
  }

  delete[] buffer;
  receive_message_mutex.unlock();
  return dict;
}

std::map<std::string, boost::any> receive_message(SOCKET socket, int timeout) {

  if (get_socket_state(socket, timeout) <= kSocketNoMessages) {
    return std::map<std::string, boost::any>();
  }

  return receive_message_core(socket);
}

Utf16Message *create_utf16_message(const std::string &message) {
  Utf16Message *result = new Utf16Message();
  result->message = message;

  return result;
}

Utf16Message *receive_utf16_message(SOCKET fd, int size = 1024 * 1024) {
  int bytes_read;

  // We need to create new unsigned char[] here because if we don't
  // the memcpy will fail with EXC_BAD_ACCESS
  std::string result;
  unsigned char *buffer = new unsigned char[size];

  do {
    // We do not want to stay at recv forever.
    // Also we need to check if there are messages before each recv because
    // the message can have length 2000 and we will try to call recv 3 times.
    // We will stay forever at the 3d one because there will be no message in
    // the socket.
    int socket_state = get_socket_state(fd, 1);

    // If the MSG_PEEK flag is set the recv function won't read the data from
    // the socket. It will just return the size of the message in the socket. If
    // the size is 0 this means that the socket is closed. This call is
    // BLOCKING.
    bytes_read = recv(fd, (char *)buffer, size, MSG_PEEK);

    if (socket_state <= kSocketClosed || bytes_read <= 0) {
      delete[] buffer;
      // Socket is closed.
      if (result.length() > 0) {
        // Return the last received message before the socket was closed.
        return create_utf16_message(result);
      } else {
        return nullptr;
      }
    }

    bytes_read = recv(fd, (char *)buffer, size, 0);

    if (bytes_read > 0) {
      result.append(buffer, buffer + bytes_read);
    } else if (bytes_read < 0) {
      delete[] buffer;
      return nullptr;
    }
  } while (bytes_read == size);

  delete[] buffer;
  return create_utf16_message(result);
}

void proxy_socket_messages(SOCKET first, SOCKET second) {
  Utf16Message *message;

  while ((message = receive_utf16_message(first)) != nullptr) {
    if (message->message.length()) {
#ifdef _WIN32
      char *message_buffer = (char *)message->message.c_str();
#else
      const char *message_buffer = message->message.c_str();
#endif // _WIN32
      send(second, message_buffer, message->message.length(), NULL);
    }

    // Delete the message to free the message->message memory.
    delete message;
  }
}

void proxy_socket_io(SOCKET first, SOCKET second,
                     SocketClosedCallback first_socket_closed_callback,
                     SocketClosedCallback second_socket_closed_callback) {
  std::thread([=]() {
    // Send the messages received on the first socket to the second socket.
    proxy_socket_messages(first, second);
    first_socket_closed_callback(second);
  }).detach();

  std::thread([=]() {
    // Send the messages received on the second socket to the first socket.
    proxy_socket_messages(second, first);
    second_socket_closed_callback(second);
  }).detach();
}

std::string receive_message_raw_inner(ServiceInfo info, int size) {
  int total_read = 0;
  int bytes_read;
  std::string result = "";
  if (get_socket_state((SOCKET)info.socket, 1) == kSocketHasMessages) {
    do {
      char *buffer = (char *)malloc(size + 1);
      bytes_read = AMDServiceConnectionReceive(info.connection, buffer, size);
      if (bytes_read > 0)
        result += std::string(buffer, bytes_read);
      free(buffer);
      total_read += bytes_read;
    } while (bytes_read == size);
  }

  if (DEBUG_PACKETS) {
    print(json({{"total_read", total_read, "receive_message_raw", result}}));
    print("\n");
  }

  return result;
}

std::string receive_message_raw(ServiceInfo info, int size) {
  do {
    std::string result = receive_message_raw_inner(info, size);

    if (result.length() < 10 || result.substr(result.length() - 3) != "#00") {
      return result;
    }
  } while (true);

  return "";
}

LengthEncodedMessage get_message_with_encoded_length(const char *message,
                                                     long long length) {
  if (length < 0)
    length = strlen(message);

  size_t packed_length_size = 4;
  unsigned long message_len = length + packed_length_size;
  char *length_encoded_message = new char[message_len];
  unsigned long packed_length = htonl(length);
  memcpy(length_encoded_message, &packed_length, packed_length_size);
  memcpy(length_encoded_message + packed_length_size, message, length);
  return {length_encoded_message, message_len};
}

void close_socket(SOCKET socket) {
  // We need closesocket for Windows because the socket
  // is a handle to kernel object instead of *nix file descriptor
  // http://stackoverflow.com/questions/35441815/are-close-and-closesocket-interchangable
#ifdef _WIN32
  closesocket(socket);
#else
  close(socket);
#endif // _WIN32
}
