#pragma once
#include "Declarations.h"

bool devicectl_stop_application(std::string &executable, std::string &device_identifier);
bool devicectl_start_application(std::string &bundle_id, std::string &device_identifier, bool wait_for_debugger);
