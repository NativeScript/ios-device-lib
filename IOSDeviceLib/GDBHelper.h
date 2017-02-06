#pragma once

#include "SocketHelper.h"
#include "Declarations.h"
#include <string>

std::string get_gdb_message(std::string message);
int gdb_send_message(std::string message, SOCKET socket, long long length = -1);
bool run_application(std::string& executable, SOCKET socket, std::string& application_identifier, std::map<std::string, ApplicationCache>& apps_cache);
bool stop_application(std::string& executable, SOCKET socket, std::string& application_identifier, std::map<std::string, ApplicationCache>& apps_cache);
void detach_connection(SOCKET socket);

