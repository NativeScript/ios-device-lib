#pragma once

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <ws2tcpip.h>
typedef void *CFDictionaryRef;

#pragma comment(lib, "Ws2_32.lib")
#else
typedef void *HANDLE;
typedef unsigned long long SOCKET;
#endif

#ifndef _WIN32

#include <CoreFoundation/CoreFoundation.h>
#include <stdlib.h>
#include <sys/socket.h>
#endif

#include "PlistCpp/include/boost/any.hpp"
#include <functional>
#include <map>
#include <string>
#include <thread>

struct TimeoutData {
  int timeout;
  void *data;
  void (*operation)(void *);
};

struct TimeoutOutputData {
  std::thread::native_handle_type darwinHandle;
  HANDLE windowsHandle;
  TimeoutData *timeoutData;
};

TimeoutOutputData *setTimeout(int timeout, void *data,
                              void (*operation)(void *));
void clearTimeout(TimeoutOutputData *data);
