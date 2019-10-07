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

struct TimeoutData {
    int timeout;
    void * data;
    void(*operation)(void *);
};

struct TimeoutOutputData {
    std::thread::native_handle_type darwinHandle;
    HANDLE windowsHandle;
    TimeoutData* timeoutData;
};

TimeoutOutputData* setTimeout(int timeout, void * data, void(*operation)(void*));
void clearTimeout(TimeoutOutputData* data);
