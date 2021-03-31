#pragma once

#include "Declarations.h"
#include "PlistCpp/Plist.hpp"
#include "PlistCpp/PlistDate.hpp"
#include "PlistCpp/include/boost/any.hpp"
#include <map>

AFCConnectionRef start_house_arrest(std::string device_identifier,
                                    const char *application_identifier,
                                    std::string method_id);
inline bool has_complete_status(std::map<std::string, boost::any> &dict);
ServiceInfo start_secure_service(std::string device_identifier,
                                 const char *service_name,
                                 std::string method_id,
                                 bool should_log_error = true,
                                 bool skip_cache = false);
bool mount_image(std::string &device_identifier, std::string &image_path,
                 std::string &method_id);
std::string get_device_property_value(std::string &device_identifier,
                                      const char *property_name);
int start_session(std::string &device_identifier);
void stop_session(std::string &device_identifier);
CFStringRef create_CFString(const char *str);
