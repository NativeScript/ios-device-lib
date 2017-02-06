#pragma once

#include <map>
#include "PlistCpp/Plist.hpp"
#include "PlistCpp/PlistDate.hpp"
#include "PlistCpp/include/boost/any.hpp"
#include "Declarations.h"

inline bool has_complete_status(std::map<std::string, boost::any>& dict);
HANDLE start_service(std::string device_identifier, const char* service_name, std::string method_id, bool should_log_error = true);
bool mount_image(std::string& device_identifier, std::string& image_path, std::string& method_id);
std::string get_device_property_value(std::string& device_identifier, const char* property_name);
int start_session(std::string& device_identifier);
void stop_session(std::string& device_identifier);
CFStringRef create_CFString(const char* str);