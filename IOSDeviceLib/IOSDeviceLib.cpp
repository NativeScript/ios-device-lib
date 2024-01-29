#include <algorithm>
#include <bitset>
#include <dlfcn.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <thread>

#include "CommonFunctions.h"
#include "DevicectlHelper.h"

#include "Constants.h"
#include "Declarations.h"
#include "FileHelper.h"
#include "GDBHelper.h"
#include "PlistCpp/Plist.hpp"
#include "PlistCpp/PlistDate.hpp"
#include "PlistCpp/include/boost/any.hpp"
#include "Printing.h"
#include "ServerHelper.h"
#include "SetTimeout.h"
#include "StringHelper.h"
#include "json.hpp"

#ifdef _WIN32
#pragma region Dll_Variable_Definitions

CFPropertyListFormat kCFPropertyListXMLFormat_v1_0 = (CFPropertyListFormat)100;
device_notification_subscribe_ptr __AMDeviceNotificationSubscribe;
HINSTANCE mobile_device_dll;
HINSTANCE core_foundation_dll;

device_copy_device_identifier __AMDeviceCopyDeviceIdentifier;
device_copy_value __AMDeviceCopyValue;
device_start_service __AMDeviceStartService;
device_uninstall_application __AMDeviceUninstallApplication;
device_secure_operation_with_bundle_id __AMDeviceSecureUninstallApplication;
device_secure_start_service_ptr __AMDeviceSecureStartService;
service_connection_invalidate_ptr __AMDServiceConnectionInvalidate;
service_connection_get_socket_ptr __AMDServiceConnectionGetSocket;
service_connection_receive_ptr __AMDServiceConnectionReceive;
service_connection_send_message_ptr __AMDServiceConnectionSendMessage;
device_create_house_arrest_service_ptr __AMDeviceCreateHouseArrestService;
device_connection_operation __AMDeviceStartSession;
device_connection_operation __AMDeviceStopSession;
device_connection_operation __AMDeviceConnect;
device_connection_operation __AMDeviceDisconnect;
device_connection_operation __AMDeviceIsPaired;
device_connection_operation __AMDevicePair;
device_connection_operation __AMDeviceValidatePairing;
device_secure_operation_with_path __AMDeviceSecureTransferPath;
device_secure_operation_with_path __AMDeviceSecureInstallApplication;
device_start_house_arrest __AMDeviceStartHouseArrestService;
device_lookup_applications __AMDeviceLookupApplications;
usb_mux_connect_by_port __USBMuxConnectByPort;
device_connection_operation __AMDeviceGetConnectionID;
device_connection_operation __AMDeviceGetInterfaceType;

ssl_free_void_void __SSL_free;

cfstring_get_c_string_ptr __CFStringGetCStringPtr;
cfstring_get_c_string __CFStringGetCString;
cf_get_type_id __CFGetTypeID;
cf_get_concrete_type_id __CFStringGetTypeID;
cf_get_concrete_type_id __CFDictionaryGetTypeID;
cfdictionary_get_count __CFDictionaryGetCount;
cfdictionary_get_keys_and_values __CFDictionaryGetKeysAndValues;
cfstring_create_with_cstring __CFStringCreateWithCString;
cfarray_create __CFArrayCreate;
cfurl_create_with_string __CFURLCreateWithString;
cfdictionary_create __CFDictionaryCreate;
cfrelease __CFRelease;

afc_connection_open __AFCConnectionOpen;
afc_connection_close __AFCConnectionClose;
afc_file_info_open __AFCFileInfoOpen;
afc_directory_read __AFCDirectoryRead;
afc_directory_open __AFCDirectoryOpen;
afc_directory_close __AFCDirectoryClose;
afc_directory_create __AFCDirectoryCreate;
afc_remove_path __AFCRemovePath;
afc_fileref_open __AFCFileRefOpen;
afc_fileref_read __AFCFileRefRead;
afc_get_device_info_key __AFCGetDeviceInfoKey;
afc_fileref_write __AFCFileRefWrite;
afc_fileref_close __AFCFileRefClose;

#pragma endregion Dll_Variable_Definitions
#endif // _WIN32

using json = nlohmann::json;

int __result;
int nextServiceConnectionId = 1;
std::map<std::string, DeviceData> devices;
std::map<int, ServiceConnRef> serviceConnections;

std::string get_dirname(std::string &path) {
  size_t found;
  found = path.find_last_of(kPathSeparators);
  return path.substr(0, found);
}

template <typename T> std::string windows_path_to_unix(T &path) {
  std::string path_str(path);
  replace_all(path_str, "\\", "/");
  return path_str;
}

std::string get_cstring_from_cfstring(CFStringRef cfstring) {
  const char *result_attempt =
      CFStringGetCStringPtr(cfstring, kCFStringEncodingUTF8);
  char cfstring_buffer[2000];
  if (result_attempt == NULL) {
    if (CFStringGetCString(cfstring, cfstring_buffer, 2000,
                           kCFStringEncodingUTF8)) {
      return std::string(cfstring_buffer);
    }
  }

  return std::string(result_attempt);
}

CFStringRef create_CFString(const char *str) {
  return CFStringCreateWithCString(NULL, str, kCFStringEncodingUTF8);
}

std::mutex start_session_mutex;
int start_session(std::string &device_identifier) {
  start_session_mutex.lock();
  if (devices[device_identifier].sessions < 1) {
    const DeviceInfo *device_info = devices[device_identifier].device_info;
    UNLOCK_MUTEX_AND_RETURN_IF_FAILED_RESULT(AMDeviceConnect(device_info),
                                             start_session_mutex);
    assert(AMDeviceIsPaired(device_info));
    UNLOCK_MUTEX_AND_RETURN_IF_FAILED_RESULT(
        AMDeviceValidatePairing(device_info), start_session_mutex);
    UNLOCK_MUTEX_AND_RETURN_IF_FAILED_RESULT(AMDeviceStartSession(device_info),
                                             start_session_mutex);
  }

  ++devices[device_identifier].sessions;
  start_session_mutex.unlock();
  return 0;
}

std::mutex stop_session_mutex;
void stop_session(std::string &device_identifier) {
  stop_session_mutex.lock();
  if (--devices[device_identifier].sessions < 1) {
    const DeviceInfo *device_info = devices[device_identifier].device_info;
    AMDeviceStopSession(device_info);
    AMDeviceDisconnect(device_info);
  }

  stop_session_mutex.unlock();
}

std::string get_device_property_value(std::string &device_identifier,
                                      const char *property_name) {
  const DeviceInfo *device_info = devices[device_identifier].device_info;
  std::string result;
  if (!start_session(device_identifier)) {
    CFStringRef cfstring = create_CFString(property_name);
    result = get_cstring_from_cfstring(
        AMDeviceCopyValue(device_info, NULL, cfstring));
    CFRelease(cfstring);
  }

  stop_session(device_identifier);
  return result;
}

const char *get_device_status(std::string device_identifier) {
  const char *result;
  if (start_session(device_identifier)) {
    result = kUnreachableStatus;
  } else {
    result = kConnectedStatus;
  }

  stop_session(device_identifier);
  return result;
}

void erase_safe(std::map<const char *, HANDLE> &m, const char *key) {
  if (m.count(key)) {
    m.erase(key);
  }
}

void cleanup_file_resources(const std::string &device_identifier,
                            const std::string &application_identifier) {
  if (!devices.count(device_identifier)) {
    return;
  }

  if (devices[device_identifier]
          .apps_cache[application_identifier]
          .afc_connection) {
    AFCConnectionRef afc_connection_to_close =
        devices[device_identifier]
            .apps_cache[application_identifier]
            .afc_connection;
    AFCConnectionClose(afc_connection_to_close);
    devices[device_identifier].apps_cache.erase(application_identifier);
  }
}

void cleanup_file_resources(const std::string &device_identifier) {
  if (!devices.count(device_identifier)) {
    return;
  }

  if (devices[device_identifier].apps_cache.size()) {
    std::map<std::string, ApplicationCache> apps_cache_clone =
        devices[device_identifier].apps_cache;
    for (auto const &key_value_pair : apps_cache_clone) {
      cleanup_file_resources(device_identifier, key_value_pair.first);
    }

    devices[device_identifier].apps_cache.clear();
  }
}

void get_device_properties(std::string device_identifier, json &result) {
  result["status"] = get_device_status(device_identifier);
  result["productType"] =
      get_device_property_value(device_identifier, "ProductType");
  result["deviceName"] =
      get_device_property_value(device_identifier, "DeviceName");
  result["productVersion"] =
      get_device_property_value(device_identifier, kProductVersion);
  result["deviceColor"] =
      get_device_property_value(device_identifier, "DeviceColor");
  result["deviceClass"] =
      get_device_property_value(device_identifier, "DeviceClass");
  //    available values:
  //    "BluetoothAddress","BoardId","CPUArchitecture","ChipID","DeviceClass",
  //    "DeviceColor","DeviceName","FirmwareVersion","HardwareModel",
  //    "ModelNumber","ProductType","ProductVersion","UniqueDeviceID","WiFiAddress"
}

inline bool has_complete_status(std::map<std::string, boost::any> &dict) {
  return boost::any_cast<std::string>(dict[kStatusKey]) == kComplete;
}

void update_device_result(std::string device_identifier, json &result) {
  result[kIsUSBConnected] = devices[device_identifier].isUSBConnected;
  result[kIsWiFiConnected] = devices[device_identifier].isWiFiConnected;
  get_device_properties(device_identifier, result);
}

void on_device_found(const DevicePointer *device_ptr,
                     std::string device_identifier, json &result) {
  /*
  Interface type can be one of the followings:
          -1 - invalid interface type
          0 - unknown interface type
          1 - usb interface type
          2 - wifi interface type
  */
  int interface_type = AMDeviceGetInterfaceType(device_ptr->device_info);
  if (interface_type == kUSBInterfaceType ||
      interface_type == kWIFIInterfaceType) {
    if (devices.count(device_identifier)) {
      devices[device_identifier].device_info = device_ptr->device_info;
      result[kEventString] = kDeviceUpdated;
    } else {
      devices[device_identifier] = {device_ptr->device_info, nullptr};
      result[kEventString] = kDeviceFound;
    }

    if (interface_type == kUSBInterfaceType) {
      devices[device_identifier].isUSBConnected = 1;
    } else {
      devices[device_identifier].isWiFiConnected = 1;
    }

    update_device_result(device_identifier, result);
    if (result["deviceClass"] == "AppleTV") {
      // We do not support AppleTV devices
      result = nullptr;
    }
  }
}

void device_notification_callback(const DevicePointer *device_ptr) {
  std::string device_identifier = get_cstring_from_cfstring(
      AMDeviceCopyDeviceIdentifier(device_ptr->device_info));
  json result = nullptr;
  result[kDeviceId] = device_identifier;
  switch (device_ptr->msg) {
  case kADNCIMessageConnected: {
    on_device_found(device_ptr, device_identifier, result);
    break;
  }
  case kADNCIMessageDisconnected: {
    if (devices.count(device_identifier)) {
      int interface_type = AMDeviceGetInterfaceType(device_ptr->device_info);
      if (interface_type == kUSBInterfaceType) {
        devices[device_identifier].isUSBConnected = 0;
      } else if (interface_type == kWIFIInterfaceType) {
        devices[device_identifier].isWiFiConnected = 0;
      }

      if (!devices[device_identifier].isUSBConnected &&
          !devices[device_identifier].isWiFiConnected) {
        if (devices[device_identifier].apps_cache.size()) {
          cleanup_file_resources(device_identifier);
        }

        devices.erase(device_identifier);
        result[kEventString] = kDeviceLost;
      } else {
        result[kEventString] = kDeviceUpdated;
        update_device_result(device_identifier, result);
      }
    }

    break;
  }
  case kADNCIMessageUnknown: {
    result[kEventString] = kDeviceUnknown;
    break;
  }
  case kADNCIMessageTrusted: {
    on_device_found(device_ptr, device_identifier, result);
    break;
  }
  }

  if (result != nullptr) {
    print(result);
  }
}

#ifdef _WIN32
void add_dll_paths_to_environment() {
  std::string str(std::getenv(kPathUpperCase));
  str = str.append(
      ";C:\\Program Files\\Common Files\\Apple\\Apple Application "
      "Support;C:\\Program Files\\Common Files\\Apple\\Mobile Device Support;");
  str = str.append(";C:\\Program Files (x86)\\Common Files\\Apple\\Apple "
                   "Application Support;C:\\Program Files (x86)\\Common "
                   "Files\\Apple\\Mobile Device Support;");

  SetEnvironmentVariable(kPathUpperCase, str.c_str());
}

int load_dlls() {
  add_dll_paths_to_environment();
  mobile_device_dll = LoadLibrary("MobileDevice.dll");
  if (!mobile_device_dll) {
    print_error("Could not load MobileDevice.dll", kNullMessageId,
                kNullMessageId);
    return 1;
  }

  core_foundation_dll = LoadLibrary("CoreFoundation.dll");
  if (!core_foundation_dll) {
    print_error("Could not load CoreFoundation.dll", kNullMessageId,
                kNullMessageId);
    return 1;
  }

  return 0;
}

#endif // _WIN32

int subscribe_for_notifications() {
  HANDLE notify_function = nullptr;
  int result = AMDeviceNotificationSubscribe(device_notification_callback, 0, 0,
                                             0, &notify_function);
  if (result) {
    print_error("Could not attach notification callback", kNullMessageId,
                kNullMessageId);
  }

  return result;
}

void start_run_loop() {
  int subscribe_for_notifications_result = subscribe_for_notifications();

  if (subscribe_for_notifications_result) {
    return;
  }

#ifdef _WIN32
  run_loop_ptr CFRunLoopRun =
      (run_loop_ptr)GetProcAddress(core_foundation_dll, "CFRunLoopRun");
#endif
  CFRunLoopRun();
}

void disable_ssl_for_connection(ServiceConnRef connection) {

#ifndef _WIN32
  static void (*SSL_free)(void *);
  if (!SSL_free) {
    SSL_free = (void (*)(void *))dlsym(RTLD_DEFAULT, "SSL_free");
  }
#endif

  if (connection->sslContext != NULL) {
    SSL_free(connection->sslContext);
    connection->sslContext = NULL;
  }
}

std::mutex start_service_mutex;

ServiceInfo start_secure_service(std::string device_identifier,
                                 const char *service_name,
                                 std::string method_id, bool should_log_error,
                                 bool skip_cache) {
  start_service_mutex.lock();
  ServiceInfo serviceInfoResult = {};
  if (!devices.count(device_identifier)) {
    if (should_log_error)
      print_error("Device not found", device_identifier, method_id,
                  kAMDNotFoundError);

    start_service_mutex.unlock();
    return serviceInfoResult;
  }

  // Track the Debug Service Name
  devices[device_identifier].debugServiceName = (char *)service_name;

  if (!skip_cache && devices[device_identifier].services.count(service_name)) {
    start_service_mutex.unlock();
    return devices[device_identifier].services[service_name];
  }

  PRINT_ERROR_AND_RETURN_VALUE_IF_FAILED_RESULT(
      start_session(device_identifier), "Could not start device session",
      device_identifier, method_id, serviceInfoResult);
  CFStringRef cf_service_name = create_CFString(service_name);

  ServiceConnRef connection;
  unsigned result =
      AMDeviceSecureStartService(devices[device_identifier].device_info,
                                 cf_service_name, NULL, &connection);

  if (connection && service_name && strcmp(service_name, kDebugServer) == 0) {
    // We have to disable SSL on old service name
    disable_ssl_for_connection(connection);
  }

  HANDLE socket = (void *)(size_t)AMDServiceConnectionGetSocket(connection);

  stop_session(device_identifier);
  CFRelease(cf_service_name);
  if (result) {
    std::string message("Could not start service ");
    message += service_name;
    if (should_log_error)
      print_error(message.c_str(), device_identifier, method_id, result);

    start_service_mutex.unlock();
    return serviceInfoResult;
  }

  serviceInfoResult.socket = socket;
  serviceInfoResult.connection = connection;
  serviceInfoResult.connection_id = nextServiceConnectionId;
  serviceInfoResult.service_name = service_name;
  serviceConnections[nextServiceConnectionId] = connection;
  nextServiceConnectionId++;

  if (!skip_cache) {
    devices[device_identifier].services[service_name] = serviceInfoResult;
  }

  start_service_mutex.unlock();
  return serviceInfoResult;
}

AFCConnectionRef start_house_arrest(std::string device_identifier,
                                    const char *application_identifier,
                                    std::string method_id) {
  if (!devices.count(device_identifier)) {
    print_error("Device not found", device_identifier, method_id,
                kAMDNotFoundError);
    return NULL;
  }

  AFCConnectionRef persistedHouseArrestService =
      devices[device_identifier]
          .apps_cache[application_identifier]
          .afc_connection;
  if (persistedHouseArrestService) {
    return persistedHouseArrestService;
  }

  AFCConnectionRef conn = NULL;
  start_session(device_identifier);
  CFStringRef cf_application_identifier =
      create_CFString(application_identifier);
  unsigned result =
      AMDeviceCreateHouseArrestService(devices[device_identifier].device_info,
                                       cf_application_identifier, 0, &conn);

  stop_session(device_identifier);
  CFRelease(cf_application_identifier);

  if (result) {
    std::string message("Could not start house arrest for application ");
    message += application_identifier;
    print_error(message.c_str(), device_identifier, method_id, result);
    return NULL;
  }

  devices[device_identifier].apps_cache[application_identifier].afc_connection =
      conn;

  return conn;
}

ServiceInfo start_debug_server(std::string device_identifier, std::string ddi,
                               std::string method_id) {

  // Try New Service
  ServiceInfo info = start_secure_service(device_identifier, kNewDebugServer,
                                          method_id, false, false);

#ifndef _WIN32

  // mount_image is not available on Windows
  if (!info.socket && mount_image(device_identifier, ddi, method_id)) {
    info = start_secure_service(device_identifier, kNewDebugServer, method_id,
                                false, false);
  }
#endif

  //  Try Old service
  if (!info.socket) {
    info = start_secure_service(device_identifier, kDebugServer, method_id,
                                false, false);
  }

  // TODO: We might not need to remount image, image might be being dismounted
  // somehow in start_secure_image, so...
#ifndef _WIN32
  if (!info.socket && mount_image(device_identifier, ddi, method_id)) {
    info = start_secure_service(device_identifier, kDebugServer, method_id,
                                true, false);
  }
#endif

  return info;
}

AFCConnectionRef get_afc_connection(std::string &device_identifier,
                                    const char *application_identifier,
                                    std::string &root_path,
                                    std::string &method_id) {
  if (devices.count(device_identifier) &&
      devices[device_identifier]
          .apps_cache[application_identifier]
          .afc_connection) {
    return devices[device_identifier]
        .apps_cache[application_identifier]
        .afc_connection;
  }

  AFCConnectionRef con_ref =
      start_house_arrest(device_identifier, application_identifier, method_id);
  if (!con_ref) {
    return NULL;
  }

  devices[device_identifier].apps_cache[application_identifier].afc_connection =
      con_ref;
  return con_ref;
}

void uninstall_application(std::string application_identifier,
                           std::string device_identifier,
                           std::string method_id) {
  if (!devices.count(device_identifier)) {
    print_error("Device not found", device_identifier, method_id,
                kAMDNotFoundError);
    return;
  }

  ServiceInfo serviceInfo = start_secure_service(
      device_identifier, kInstallationProxy, method_id, true, false);
  if (!serviceInfo.socket) {
    return;
  }

  CFStringRef appid_cfstring = create_CFString(application_identifier.c_str());
  DeviceInfo *deviceInfo = devices[device_identifier].device_info;
  CFDictionaryRef params = CFDictionaryCreate(NULL, {}, {}, 0, NULL, NULL);
  unsigned result = AMDeviceSecureUninstallApplication(
      serviceInfo.connection, deviceInfo, appid_cfstring, params, NULL);
  CFRelease(appid_cfstring);

  if (result) {
    print_error("Could not uninstall application", device_identifier, method_id,
                result);
  } else {
    print(json({{kResponse, "Successfully uninstalled application"},
                {kId, method_id},
                {kDeviceId, device_identifier}}));
    // AppleFileConnection and HouseArrest deal with the files on an application
    // so they have to be removed when uninstalling the application
    cleanup_file_resources(device_identifier, application_identifier);
  }
}

void install_application(std::string install_path,
                         std::string device_identifier, std::string method_id) {
  if (!devices.count(device_identifier)) {
    print_error("Device not found", device_identifier, method_id,
                kAMDNotFoundError);
    return;
  }

#ifdef _WIN32
  if (install_path.compare(0, kFilePrefix.size(), kFilePrefix)) {
    install_path = kFilePrefix + install_path;
  }
  install_path = windows_path_to_unix(install_path);
  install_path = url_encode_without_forward_slash_and_colon(install_path);
#endif
  int session_result = start_session(device_identifier);
  if (session_result) {
    print_error("Could not start device session", device_identifier, method_id,
                session_result);
    return;
  }
  CFStringRef path = create_CFString(install_path.c_str());
  CFURLRef local_app_url =
#ifdef _WIN32
      CFURLCreateWithString(NULL, path, NULL);
#else
      CFURLCreateWithFileSystemPath(NULL, path, kCFURLPOSIXPathStyle, true);
#endif
  CFRelease(path);
  if (!local_app_url) {
    stop_session(device_identifier);
    print_error("Could not parse application path", device_identifier,
                method_id, kAMDAPIInternalError);
    return;
  }

  CFStringRef cf_package_type = create_CFString("PackageType");
  CFStringRef cf_developer = create_CFString("Developer");
  const void *keys_arr[] = {cf_package_type};
  const void *values_arr[] = {cf_developer};
  CFDictionaryRef options =
      CFDictionaryCreate(NULL, keys_arr, values_arr, 1, NULL, NULL);

  unsigned transfer_result =
      AMDeviceSecureTransferPath(0, devices[device_identifier].device_info,
                                 local_app_url, options, NULL, 0);
  stop_session(device_identifier);
  if (transfer_result) {
    print_error("Could not transfer application", device_identifier, method_id,
                transfer_result);
    return;
  }

  session_result = start_session(device_identifier);
  if (session_result) {
    print_error("Could not start device session", device_identifier, method_id,
                session_result);
    return;
  }

  unsigned install_result = AMDeviceSecureInstallApplication(
      0, devices[device_identifier].device_info, local_app_url, options, NULL,
      0);
  CFRelease(cf_package_type);
  CFRelease(cf_developer);
  stop_session(device_identifier);

  if (install_result) {
    print_error("Could not install application", device_identifier, method_id,
                install_result);
    return;
  }

  print(json({{kResponse, "Successfully installed application"},
              {kId, method_id},
              {kDeviceId, device_identifier}}));
  // In case this is a REinstall we need to invalidate cached file resources
  cleanup_file_resources(device_identifier);
}

void perform_detached_operation(void (*operation)(std::string, std::string),
                                std::string arg, std::string method_id) {
  std::thread([operation, arg, method_id]() {
    operation(arg, method_id);
  }).detach();
}

void perform_detached_operation(void (*operation)(std::string, std::string,
                                                  std::string),
                                json method_args, std::string method_id) {
  std::string first_arg = method_args[0].get<std::string>();
  std::vector<std::string> device_identifiers =
      method_args[1].get<std::vector<std::string>>();
  for (std::string &device_identifier : device_identifiers)
    std::thread([operation, first_arg, device_identifier, method_id]() {
      operation(first_arg, device_identifier, method_id);
    }).detach();
}

std::mutex clean_con_resources_mutex;
void clean_con_resources(void *data) {
  clean_con_resources_mutex.lock();
  ConnectionMessageData *connectionMessageData =
      reinterpret_cast<ConnectionMessageData *>(data);
  ServiceConnRef conn = connectionMessageData->conn;
  devices[connectionMessageData->device_identifier].services.erase(
      connectionMessageData->service_name.c_str());
  AMDServiceConnectionInvalidate(conn);
  clean_con_resources_mutex.unlock();
}

std::mutex receive_con_message_mutex;
std::map<std::string, boost::any>
receive_con_message(ConnectionMessageData data) {
  receive_con_message_mutex.lock();

  ServiceConnRef conn = data.conn;
  TimeoutOutputData *timeoutOutputData =
      setTimeout(data.timeout, &data, clean_con_resources);

  std::map<std::string, boost::any> dict;
  char *buffer = new char[4];
  int bytes_read = AMDServiceConnectionReceive(conn, buffer, 4);
  if (bytes_read > 0) {
    unsigned long res = ntohl(*((unsigned long *)buffer));
    delete[] buffer;
    buffer = new char[res];
    bytes_read = AMDServiceConnectionReceive(conn, buffer, res);
    if (bytes_read > 0) {
      Plist::readPlist(buffer, res, dict);
    }
  }

  clearTimeout(timeoutOutputData);
  delete[] buffer;
  receive_con_message_mutex.unlock();
  return dict;
}

long send_con_message(ServiceConnRef serviceConnection,
                      CFDictionaryRef message) {
  return AMDServiceConnectionSendMessage(serviceConnection, message,
                                         kCFPropertyListXMLFormat_v1_0);
}

void read_dir(AFCConnectionRef afc_conn_p, const char *dir, json &files,
              std::stringstream &errors, std::string method_id,
              std::string device_identifier) {
  char *dir_ent;
  files.push_back(dir);

  afc_dictionary afc_dict;
  afc_dictionary *afc_dict_p = &afc_dict;
  unsigned afc_file_info_open_result =
      AFCFileInfoOpen(afc_conn_p, dir, &afc_dict_p);
  if (afc_file_info_open_result) {
    errors << "Could not open file info for file: ";
    errors << dir;
    errors << '\n';

    return;
  }

  afc_directory afc_dir;
  afc_directory *afc_dir_p = &afc_dir;
  unsigned err = AFCDirectoryOpen(afc_conn_p, dir, &afc_dir_p);

  if (err != 0) {
    // Couldn't open dir - was probably a file
    return;
  }

  while (true) {
    err = AFCDirectoryRead(afc_conn_p, afc_dir_p, &dir_ent);

    if (err != 0) {
      // Couldn't read dir
      return;
    }

    if (!dir_ent)
      break;

    if (strcmp(dir_ent, ".") == 0 || strcmp(dir_ent, "..") == 0)
      continue;

    char *dir_joined = (char *)malloc(strlen(dir) + strlen(dir_ent) + 2);
    strcpy(dir_joined, dir);
    if (dir_joined[strlen(dir) - 1] != '/')
      strcat(dir_joined, "/");
    strcat(dir_joined, dir_ent);
    read_dir(afc_conn_p, dir_joined, files, errors, method_id,
             device_identifier);
    free(dir_joined);
  }

  unsigned afc_directory_close_result =
      AFCDirectoryClose(afc_conn_p, afc_dir_p);
  if (afc_directory_close_result) {
    errors << "Could not close directory: ";
    errors << dir_ent;
    errors << '\n';
  }
}

std::mutex list_files_mutex;
void list_files(std::string device_identifier,
                const char *application_identifier, const char *device_path,
                std::string method_id) {
  list_files_mutex.lock();

  std::string device_root(device_path);
  AFCConnectionRef afc_conn_p = get_afc_connection(
      device_identifier, application_identifier, device_root, method_id);
  if (!afc_conn_p) {
    print_error("Could not establish AFC Connection", device_identifier,
                method_id);
    list_files_mutex.unlock();
    return;
  }

  json files;
  std::stringstream errors;

  std::string device_path_str = windows_path_to_unix(device_path);
  read_dir(afc_conn_p, device_path_str.c_str(), files, errors, method_id,
           device_identifier);
  if (!files.empty()) {
    print(json({{kResponse, files},
                {kId, method_id},
                {kDeviceId, device_identifier}}));
  }
  if (errors.rdbuf()->in_avail() != 0) {
    print_error(errors.str().c_str(), device_identifier, method_id,
                kAFCCustomError);
  }

  cleanup_file_resources(device_identifier, application_identifier);
  list_files_mutex.unlock();
}

bool ensure_device_path_exists(std::string &device_path,
                               AFCConnectionRef connection) {
  std::vector<std::string> directories = split(device_path, kUnixPathSeparator);
  std::string curent_device_path("");
  for (std::string &directory_path : directories) {
    if (!directory_path.empty()) {
      curent_device_path += kUnixPathSeparator;
      curent_device_path += directory_path;
      if (AFCDirectoryCreate(connection, curent_device_path.c_str()))
        return false;
    }
  }

  return true;
}

std::mutex upload_file_mutex;
void upload_file(std::string device_identifier,
                 const char *application_identifier,
                 const std::vector<FileUploadData> &files,
                 std::string method_id) {
  json success_json = json({{kResponse, "Successfully uploaded files"},
                            {kId, method_id},
                            {kDeviceId, device_identifier}});
  if (!files.size()) {
    print(success_json);
    return;
  }

  upload_file_mutex.lock();

  std::string afc_destination_str = windows_path_to_unix(files[0].destination);
  AFCConnectionRef afc_conn_p =
      get_afc_connection(device_identifier, application_identifier,
                         afc_destination_str, method_id);
  if (!afc_conn_p) {
    upload_file_mutex.unlock();
    // If there is no opened afc connection the get_afc_connection will print
    // the error for the operation.
    return;
  }

  // We need to set the size of errors here because we need to access the
  // elements by index. If we don't access them by index and use push_back from
  // multiple threads, some of them will try to push at the same memory. The
  // result of this will be an exception.
  std::vector<std::string> errors(files.size());
  std::vector<std::thread> file_upload_threads;
  // We're only ever going to insert kDeviceUploadFilesBatchSize elements
  file_upload_threads.reserve(kDeviceUploadFilesBatchSize);

  // Launching a separate thread for each file puts a strain on the OS' memory
  // That's why we launch batches of threads - kDeviceUploadFilesBatchSize each
  size_t batch_end = files.size() / kDeviceUploadFilesBatchSize + 1;
  for (size_t batch_index = 0; batch_index < batch_end; ++batch_index) {
    size_t start = batch_index * kDeviceUploadFilesBatchSize;
    size_t end = (std::min)((batch_index + 1) * kDeviceUploadFilesBatchSize,
                            files.size());

    for (size_t i = start; i < end; ++i) {
      FileUploadData current_file_data = files[i];
      file_upload_threads.emplace_back([=, &errors]() -> void {
        afc_file_ref file_ref;
        std::string source = current_file_data.source;
        std::string destination = current_file_data.destination;
        FileInfo file_info = get_file_info(source, true);

        if (file_info.size >= 0) {
          std::string dir_name = get_dirname(destination);
          if (ensure_device_path_exists(dir_name, afc_conn_p)) {
            std::stringstream error_message;
            AFCRemovePath(afc_conn_p, destination.c_str());
            error_message << "Could not open file " << destination
                          << " for writing";
            if (AFCFileRefOpen(afc_conn_p, destination.c_str(),
                               kAFCFileModeWrite, &file_ref)) {
              errors[i] = error_message.str();
              return;
            }

            error_message.str("");
            error_message << "Could not write to file: " << destination;

            if (AFCFileRefWrite(afc_conn_p, file_ref, &file_info.contents[0],
                                file_info.size)) {
              errors[i] = error_message.str();
              return;
            }

            error_message.str("");
            error_message << "Could not close file reference: " << destination;
            if (AFCFileRefClose(afc_conn_p, file_ref)) {
              errors[i] = error_message.str();
              return;
            }
          } else {
            std::string message("Could not create device path for file: ");
            message += source;
            errors.push_back(message);
          }
        } else {
          std::string message("Could not open file: ");
          message += source;
          errors.push_back(message);
        }
      });
    }

    for (std::thread &file_upload_thread : file_upload_threads) {
      file_upload_thread.join();
    }

    // After a batch is completed we need to empty file_upload_threads so that
    // the new batch may take the old one's place
    file_upload_threads.clear();
    cleanup_file_resources(device_identifier, application_identifier);
    upload_file_mutex.unlock();
  }

  std::vector<std::string> filtered_errors;
  std::copy_if(std::begin(errors), std::end(errors),
               std::back_inserter(filtered_errors),
               [](std::string e) { return e.size() != 0; });

  if (!filtered_errors.size()) {
    print(success_json);
  } else {
    print_errors(filtered_errors, device_identifier, method_id,
                 kAMDAPIInternalError);
  }
}

std::mutex delete_file_mutex;
void delete_file(std::string device_identifier,
                 const char *application_identifier, const char *destination,
                 std::string method_id) {
  delete_file_mutex.lock();

  std::string destination_str = windows_path_to_unix(destination);
  AFCConnectionRef afc_conn_p = get_afc_connection(
      device_identifier, application_identifier, destination_str, method_id);
  if (!afc_conn_p) {
    delete_file_mutex.unlock();
    return;
  }

  destination = destination_str.c_str();
  std::stringstream error_message;
  error_message << "Could not remove file " << destination;

  unsigned afcRemovePathResult = AFCRemovePath(afc_conn_p, destination);

  cleanup_file_resources(device_identifier, application_identifier);

  delete_file_mutex.unlock();

  PRINT_ERROR_AND_RETURN_IF_FAILED_RESULT(afcRemovePathResult,
                                          error_message.str().c_str(),
                                          device_identifier, method_id);
  print(json({{kResponse, "Successfully removed file"},
              {kId, method_id},
              {kDeviceId, device_identifier}}));
}

std::unique_ptr<afc_file> get_afc_file(std::string device_identifier,
                                       const char *application_identifier,
                                       const char *destination,
                                       std::string method_id) {
  afc_file_ref file_ref;
  std::string destination_str = windows_path_to_unix(destination);
  AFCConnectionRef afc_conn_p = get_afc_connection(
      device_identifier, application_identifier, destination_str, method_id);
  if (!afc_conn_p) {
    return NULL;
  }

  destination = destination_str.c_str();
  std::stringstream error_message;
  error_message << "Could not open file " << destination << " for reading";
  PRINT_ERROR_AND_RETURN_VALUE_IF_FAILED_RESULT(
      AFCFileRefOpen(afc_conn_p, destination, kAFCFileModeRead, &file_ref),
      error_message.str().c_str(), device_identifier, method_id, NULL);
  std::unique_ptr<afc_file> result(new afc_file{file_ref, afc_conn_p});
  return result;
}

std::mutex read_file_mutex;
void read_file(std::string device_identifier,
               const char *application_identifier, const char *path,
               std::string method_id, const char *destination = nullptr) {
  read_file_mutex.lock();

  std::unique_ptr<afc_file> file =
      get_afc_file(device_identifier, application_identifier, path, method_id);
  if (!file) {
    read_file_mutex.unlock();
    return;
  }

  size_t read_size = kDeviceFileBytesToRead;
  std::string result;
  char *buf;
  if (destination) {
    // Write the contents of the source file to the destination
    std::ofstream ostream;
    ostream.open(destination);
    do {
      buf = new char[kDeviceFileBytesToRead];
      AFCFileRefRead(file->afc_conn_p, file->file_ref, buf, &read_size);
      ostream.write(buf, read_size);

      delete[] buf;
      //      free(buf);

    } while (read_size == kDeviceFileBytesToRead);

    ostream.close();
    result = std::string("File written successfully!");
  } else {
    // Pipe the contents of the file to the stdout
    std::vector<char> file_contents;
    do {
      buf = new char[kDeviceFileBytesToRead];
      AFCFileRefRead(file->afc_conn_p, file->file_ref, buf, &read_size);
      file_contents.insert(file_contents.end(), buf, buf + read_size);
      delete[] buf;
      // free(buf);
    } while (read_size == kDeviceFileBytesToRead);

    result = std::string(file_contents.begin(), file_contents.end());
  }

  unsigned afcFileRefCloseResult =
      AFCFileRefClose(file->afc_conn_p, file->file_ref);
  cleanup_file_resources(device_identifier, application_identifier);

  read_file_mutex.unlock();

  PRINT_ERROR_AND_RETURN_IF_FAILED_RESULT(afcFileRefCloseResult,
                                          "Could not close file reference",
                                          device_identifier, method_id);
  print(json(
      {{kResponse, result}, {kId, method_id}, {kDeviceId, device_identifier}}));
}

void get_application_infos(std::string device_identifier,
                           std::string method_id) {
  ServiceInfo serviceInfo = start_secure_service(
      device_identifier, kInstallationProxy, method_id, true, false);
  if (!serviceInfo.socket) {
    return;
  }

  CFStringRef cf_bundle_id_key = create_CFString("CFBundleIdentifier");
  CFStringRef cf_config_key = create_CFString("configuration");
  const void *cf_return_attributes[] = {cf_bundle_id_key, cf_config_key};
  const CFArrayRef cf_return_attributes_array =
      CFArrayCreate(NULL, cf_return_attributes, 2, NULL);

  CFStringRef cf_app_type_key = create_CFString("ApplicationType");
  CFStringRef cf_return_attrs_key = create_CFString("ReturnAttributes");
  const void *client_opts_keys_arr[] = {cf_app_type_key, cf_return_attrs_key};

  CFStringRef cf_app_type_value = create_CFString("User");
  const void *client_opts_values_arr[] = {cf_app_type_value,
                                          cf_return_attributes_array};
  CFDictionaryRef client_opts_dict = CFDictionaryCreate(
      NULL, client_opts_keys_arr, client_opts_values_arr, 2, NULL, NULL);

  CFStringRef cf_command_key = create_CFString("Command");
  CFStringRef cf_client_options_key = create_CFString("ClientOptions");
  const void *keys_arr[] = {cf_command_key, cf_client_options_key};
  CFStringRef cf_command_value = create_CFString("Browse");
  const void *values_arr[] = {cf_command_value, client_opts_dict};
  CFDictionaryRef dict_command =
      CFDictionaryCreate(NULL, keys_arr, values_arr, 2, NULL, NULL);

  send_con_message(serviceInfo.connection, dict_command);
  CFRelease(cf_bundle_id_key);
  CFRelease(cf_config_key);
  CFRelease(cf_return_attributes_array);
  CFRelease(cf_app_type_key);
  CFRelease(cf_return_attrs_key);
  CFRelease(cf_app_type_value);
  CFRelease(client_opts_dict);
  CFRelease(cf_command_key);
  CFRelease(cf_client_options_key);
  CFRelease(cf_command_value);
  CFRelease(dict_command);

  std::vector<json> livesync_app_infos;
  while (true) {
    ConnectionMessageData connectionMessageData = {serviceInfo.connection,
                                                   device_identifier, method_id,
                                                   kInstallationProxy, 0};
    std::map<std::string, boost::any> dict =
        receive_con_message(connectionMessageData);
    PRINT_ERROR_AND_RETURN_IF_FAILED_RESULT(
        dict.count(kErrorKey),
        boost::any_cast<std::string>(dict[kErrorKey]).c_str(),
        device_identifier, method_id);
    if (dict.empty() || (dict.count(kStatusKey) && has_complete_status(dict))) {
      break;
    }

    // Due to a concurrency issue we may receive an answer that we're not
    // expecting here We actually receive and answer from installation_proxy
    // here and we expect it to contain information about currently installed
    // applications However if the installation_proxy service is being used on
    // the device by another process or thread regardless of whether by us or
    // not we may receive other answers too Then we may receive the messages
    // from said usage instead of the expected messages containing information
    // about installed applications. NB! These messages may be lost for the
    // other user of installation_proxy! Be advised.
    if (dict.count(kCurrentApplicationsKey)) {
      std::vector<boost::any> current_list =
          boost::any_cast<std::vector<boost::any>>(
              dict[kCurrentApplicationsKey]);
      for (boost::any &list : current_list) {
        std::map<std::string, boost::any> app_info =
            boost::any_cast<std::map<std::string, boost::any>>(list);
        json current_info = {
            {"CFBundleIdentifier",
             app_info.count("CFBundleIdentifier")
                 ? boost::any_cast<std::string>(app_info["CFBundleIdentifier"])
                 : ""},
            {"configuration",
             app_info.count("configuration")
                 ? boost::any_cast<std::string>(app_info["configuration"])
                 : ""},
        };
        livesync_app_infos.push_back(current_info);
      }
    }
  }

  print(json({{kResponse, livesync_app_infos},
              {kId, method_id},
              {kDeviceId, device_identifier}}));
}

std::map<std::string, std::string> parse_cfdictionary(CFDictionaryRef dict) {
  std::map<std::string, std::string> result;
  long count = CFDictionaryGetCount(dict);
  const void **keys = new const void *[count];
  const void **values = new const void *[count];
  CFDictionaryGetKeysAndValues(dict, keys, values);
  for (size_t index = 0; index < count; index++) {
    // The casts below are necessary - Xcode's compiler doesn't cope without
    // them
    CFStringRef value = (CFStringRef)values[index];
    CFStringRef key_str = (CFStringRef)keys[index];
    std::string key = get_cstring_from_cfstring(key_str);
    if (CFGetTypeID(value) == CFStringGetTypeID()) {
      result[key] = get_cstring_from_cfstring(value);
    }
  }

  delete[] keys;
  delete[] values;

  return result;
}

std::map<std::string, std::map<std::string, std::string>>
parse_lookup_cfdictionary(CFDictionaryRef dict) {
  // The dictionary may contain elements whose values are not strings
  // However those are filtered out
  // In the future if we ever need more information about applications this
  // would be a perfect place to look
  std::map<std::string, std::map<std::string, std::string>> result;
  long count = CFDictionaryGetCount(dict);
  const void **keys = new const void *[count];
  const void **values = new const void *[count];
  CFDictionaryGetKeysAndValues(dict, keys, values);
  for (size_t index = 0; index < count; index++) {
    CFStringRef key_str = (CFStringRef)keys[index];
    CFDictionaryRef value_dict = (CFDictionaryRef)values[index];
    std::string key = get_cstring_from_cfstring(key_str);
    result[key] = parse_cfdictionary(value_dict);
  }

  delete[] keys;
  delete[] values;

  return result;
}

bool get_all_apps(
    std::string device_identifier,
    std::map<std::string, std::map<std::string, std::string>> &map) {
  CFDictionaryRef result = nullptr;
  if (!start_session(device_identifier)) {
    if (AMDeviceLookupApplications(devices[device_identifier].device_info, NULL,
                                   &result)) {
      return false;
    } else {
      // The result from AMDeviceLookupApplications is actually a dictionary
      // Very much resembling a plist
      // It contains a lot of information about the app, but right now we only
      // care for the properties `CFBundleExecutable` and `Path`
      map = parse_lookup_cfdictionary(result);
    }
  }

  stop_session(device_identifier);
  return true;
}

void lookup_apps(std::string device_identifier, std::string method_id) {
  std::map<std::string, std::map<std::string, std::string>> map;
  if (get_all_apps(device_identifier, map)) {
    print(json(
        {{kResponse, map}, {kId, method_id}, {kDeviceId, device_identifier}}));
  } else {
    print_error("Lookup applications failed", device_identifier, method_id,
                kApplicationsCustomError);
  }
}

void device_log(std::string device_identifier, std::string method_id) {
  ServiceInfo serviceInfo =
      start_secure_service(device_identifier, kSyslog, method_id, true, false);
  if (!serviceInfo.socket) {
    return;
  }

  char *buffer = new char[kDeviceLogBytesToRead];
  int bytes_read;

  // inspired by:
  // https://github.com/DerekSelander/mobdevim/blob/a457f119f1576b85e9f8f89be8713f018ee97f59/mobdevim/console.temp_caseinsensitive_rename.m#L27
  while ((bytes_read = AMDServiceConnectionReceive(
              serviceInfo.connection, buffer, kDeviceLogBytesToRead)) > 0) {
    json message;
    message[kDeviceId] = device_identifier;
    message[kMessage] = std::string(buffer, bytes_read);
    message[kId] = method_id;
    print(message);
  }

  delete[] buffer;
}

void post_notification(std::string device_identifier,
                       PostNotificationInfo post_notification_info,
                       std::string method_id) {
  ServiceInfo info = start_secure_service(device_identifier, kNotificationProxy,
                                          method_id, true, true);
  if (!info.socket) {
    return;
  }

  CFStringRef cf_command_key = create_CFString("Command");
  CFStringRef cf_command_value =
      create_CFString(post_notification_info.command_type.c_str());
  CFStringRef cf_name_key = create_CFString("Name");
  CFStringRef cf_name_value =
      create_CFString(post_notification_info.notification_name.c_str());
  CFStringRef cf_client_options_key = create_CFString("ClientOptions");
  CFStringRef cf_client_options_value = create_CFString("");
  const void *keys_arr[] = {cf_command_key, cf_name_key, cf_client_options_key};
  const void *values_arr[] = {cf_command_value, cf_name_value,
                              cf_client_options_value};
  CFDictionaryRef dict_command =
      CFDictionaryCreate(NULL, keys_arr, values_arr, 3, NULL, NULL);

  send_con_message(info.connection, dict_command);
  CFRelease(cf_command_key);
  CFRelease(cf_command_value);
  CFRelease(cf_name_key);
  CFRelease(cf_name_value);
  CFRelease(cf_client_options_key);
  CFRelease(cf_client_options_value);
  CFRelease(dict_command);

  print(json({{kResponse, info.connection_id},
              {kId, method_id},
              {kDeviceId, device_identifier}}));
}

void await_notification_response(
    std::string device_identifier,
    AwaitNotificationResponseInfo await_notification_response_info,
    std::string method_id) {
  ServiceConnRef connection =
      serviceConnections[(int)await_notification_response_info.socket];
  std::string invalid_connection_error_message =
      "Invalid socket: " +
      std::to_string(await_notification_response_info.socket);
  PRINT_ERROR_AND_RETURN_IF_FAILED_RESULT(
      connection == nullptr, invalid_connection_error_message.c_str(),
      device_identifier, method_id);

  // ServiceInfo currentNotificationProxy =
  //     devices[device_identifier].services[kNotificationProxy];
  ConnectionMessageData connectionMessageData = {
      connection, device_identifier, method_id, kNotificationProxy,
      await_notification_response_info.timeout};
  std::map<std::string, boost::any> response =
      receive_con_message(connectionMessageData);
  if (response.size()) {
    PRINT_ERROR_AND_RETURN_IF_FAILED_RESULT(
        response.count(kErrorKey),
        boost::any_cast<std::string>(response[kErrorKey]).c_str(),
        device_identifier, method_id);

    std::string response_command_type =
        boost::any_cast<std::string>(response[kCommandKey]);
    std::string invalid_response_type_error_message =
        "Invalid notification response command type: " + response_command_type;
    PRINT_ERROR_AND_RETURN_IF_FAILED_RESULT(
        response_command_type !=
            await_notification_response_info.response_command_type,
        invalid_response_type_error_message.c_str(), device_identifier,
        method_id);

    std::string response_message = boost::any_cast<std::string>(
        response[await_notification_response_info.response_property_name]);
    print(json({{kResponse, response_message},
                {kId, method_id},
                {kDeviceId, device_identifier}}));
  } else {
    print_error("ObserveNotification timeout.", device_identifier, method_id);
  }
}

bool validate(const json &j, std::string key, std::string method_id,
              std::string device_identifier = kNullMessageId) {
  std::stringstream error;
  if (j.count(key) == 0) {
    error << "Argument " + key + " is missing";
    print_error(error.str().c_str(), device_identifier, method_id,
                kArgumentError);
    return false;
  }

  json::const_iterator entry = j.find(key);
  json value = entry.value();
  if (!value.is_string()) {
    error << "Argument " + key + " has incorrect value";
    print_error(error.str().c_str(), device_identifier, method_id,
                kArgumentError);
    return false;
  }
  return true;
}

bool validate(const json &j, std::vector<std::string> attributes,
              std::string method_id, std::string device_identifier) {
  for (std::string key : attributes)
    if (!validate(j, key, method_id, device_identifier))
      return false;

  return true;
}

bool validate_device_id_and_attrs(const json &j, std::string method_id,
                                  std::vector<std::string> attrs) {
  if (!validate(j, kDeviceId, method_id))
    return false;

  std::string device_identifier = j.value(kDeviceId, "");

  if (!validate(j, attrs, method_id, device_identifier))
    return false;

  return true;
}

bool check_xcode_major_version(int version, std::string device_identifier, std::string method_id){
#ifdef _WIN32
    print_error("Not supported on windows", device_identifier, method_id,
                kApplicationsCustomError);
    return false;
#else
    if(get_xcode_major_version()>=version){
        return true;
    } else {
        std::string message = "Xcode version of at least " + std::to_string(version) + " is required";
        print_error(message.c_str(), device_identifier, method_id,
                    kApplicationsCustomError);
        return false;
    }
#endif
}

void stop_app(std::string device_identifier, std::string application_identifier,
              std::string ddi, std::string method_id) {
  if (!devices.count(device_identifier)) {
    print_error("Device not found", device_identifier, method_id,
                kAMDNotFoundError);
    return;
  }

  std::map<std::string, std::map<std::string, std::string>> map;
    if (get_all_apps(device_identifier, map)) {
        if (map.count(application_identifier) == 0) {
            print_error("Application not installed", device_identifier, method_id,
                        kApplicationsCustomError);
            return;
        }
        if(get_product_version(device_identifier) < 17){
            long service_count = devices[device_identifier].services.size();
            ServiceInfo gdb = start_debug_server(device_identifier, ddi, method_id);
            if (!gdb.socket) {
                print_error("Unable to start gdb server", device_identifier, method_id,
                            kUnexpectedError);
                return;
            }
            
            std::string executable = map[application_identifier][kPathPascalCase];
            if (devices[device_identifier].services.size() == service_count ||
                stop_application(executable, gdb, application_identifier,
                                 devices[device_identifier].apps_cache))
                print(json({{kResponse, "Successfully stopped application"},
                    {kId, method_id},
                    {kDeviceId, device_identifier}}));
            else
                print_error("Could not stop application", device_identifier, method_id,
                            kApplicationsCustomError);
            
            detach_connection(gdb, &devices[device_identifier]);
        } else {
            if(check_xcode_major_version(15, device_identifier, method_id)){
                std::string executable = map[application_identifier][kPathPascalCase];
                std::string CFBundleExecutableString =map[application_identifier]["CFBundleExecutable"];
                
                std::string fullPidPath =executable + "/" + CFBundleExecutableString;
                if(devicectl_stop_application(fullPidPath, device_identifier)){
                    
                    print(json({{kResponse, "Successfully stopped application"},
                        {kId, method_id},
                        {kDeviceId, device_identifier}}));
                } else {
                    print_error("Could not stop application", device_identifier, method_id,
                                kApplicationsCustomError);
                }
            }
        }
    } else {
        print_error("Lookup applications failed", device_identifier, method_id,
                    kApplicationsCustomError);
    }
}

void start_app(std::string device_identifier,
               std::string application_identifier, std::string ddi,
               std::string method_id, bool wait_for_debugger) {
  if (!devices.count(device_identifier)) {
    print_error("Device not found", device_identifier, method_id,
                kAMDNotFoundError);
    return;
  }

  std::map<std::string, std::map<std::string, std::string>> map;
  if (get_all_apps(device_identifier, map)) {
    if (map.count(application_identifier) == 0) {
      print_error("Application not installed", device_identifier, method_id,
                  kApplicationsCustomError);
      return;
    }
      if(get_product_version(device_identifier) < 17){
          ServiceInfo gdb = start_debug_server(device_identifier, ddi, method_id);
          if (!gdb.socket) {
              return;
          }
          
          std::string executable = map[application_identifier][kPathPascalCase] +
          "/" +
          map[application_identifier]["CFBundleExecutable"];
          if (run_application(executable, gdb, application_identifier,
                              &devices[device_identifier], wait_for_debugger))
              print(json({{kResponse, "Successfully started application"},
                  {kId, method_id},
                  {kDeviceId, device_identifier}}));
          else
              print_error("Could not start application", device_identifier, method_id,
                          kApplicationsCustomError);
      } else {
          if(check_xcode_major_version(15, device_identifier, method_id)){
              if(devicectl_start_application(application_identifier, device_identifier, wait_for_debugger))
                  print(json({{kResponse, "Successfully started application"},
                      {kId, method_id},
                      {kDeviceId, device_identifier}}));
              else
                  print_error("Could not start application", device_identifier, method_id,
                              kApplicationsCustomError);
          }
      }
  } else {
    print_error("Lookup applications failed", device_identifier, method_id,
                kApplicationsCustomError);
  }
}

void connect_to_port(std::string device_identifier, int port,
                     std::string method_id) {
  if (!devices.count(device_identifier)) {
    print_error("Device not found.", device_identifier, method_id);
    return;
  }

  if (devices[device_identifier].device_server_data != nullptr) {
    // Dispose the old connection.
    devices[device_identifier].kill_device_server();
  }

  DeviceInfo *device_info = devices[device_identifier].device_info;

  int interface_type = AMDeviceGetInterfaceType(device_info);

  if (interface_type != kUSBInterfaceType) {
    print_error("The device is not connected with USB.", device_identifier,
                method_id);
    return;
  }

  int connection_id = AMDeviceGetConnectionID(device_info);
  long long device_socket;
  int usb_result =
      USBMuxConnectByPort(connection_id, htons(port), &device_socket);

  if (usb_result != 0) {
      print_error("Failed to perform mux connect on device.", device_identifier,
                  method_id, usb_result);
      return;
  }
    
  if (device_socket < 0) {
    print_error("USBMuxConnectByPort returned bad file descriptor",
                device_identifier, method_id, usb_result);
  } else {
    DeviceServerData *server_socket_data =
        create_server(device_socket, kLocalhostAddress);
    if (server_socket_data == nullptr) {
      print_error("Failed to start the proxy server between the Chrome Dev "
                  "Tools and the iOS device.",
                  device_identifier, method_id);
      return;
    }

    devices[device_identifier].device_server_data = server_socket_data;
    // We can use the device socket which is returned from USBMuxConnectByPort
    // only in the C++ code. That's why we need to create a server which will
    // serve to expose the socket. When we receive a client connection we will
    // create a client socket. Each message received on the client socket will
    // be sent to the device socket. Each message from the device socket will be
    // sent to the client socket.
    sockaddr_in server_address = server_socket_data->server_address;
    unsigned short port = ntohs(server_address.sin_port);
    socklen_t address_length = sizeof(server_address);

    // Return the host address and the port to the client.
    print(json({{kHost, kLocalhostAddress},
                {kPort, port},
                {kId, method_id},
                {kDeviceId, device_identifier}}));

    // Wait for the client to connect.
    SOCKET client_socket = accept(server_socket_data->server_socket,
                                  (sockaddr *)&server_address, &address_length);

    // Proxy the messages from the client socket to the device socket
    // and from the device socket to the client socket.
    proxy_socket_io(
        device_socket, client_socket,
        [](SOCKET client_fd) { close_socket(client_fd); },
        [=](SOCKET d_fd) { devices[device_identifier].kill_device_server(); });
  }
}


int main() {
#ifdef _WIN32
  _setmode(_fileno(stdout), _O_BINARY);
  _setmode(_fileno(stderr), _O_BINARY);

  if (load_dlls()) {
    return -1;
  }
#endif

  std::thread([]() { start_run_loop(); }).detach();

  for (std::string line; std::getline(std::cin, line);) {
    json message = json::parse(line.c_str());
    for (json method : message.value("methods", json::array())) {
      std::string method_name = method.value("name", "");
      std::string method_id = method.value("id", "");
      std::vector<json> method_args =
          method.value("args", json::array()).get<std::vector<json>>();

      if (method_name == "install") {
        perform_detached_operation(install_application, method_args, method_id);
      } else if (method_name == "uninstall") {
        perform_detached_operation(uninstall_application, method_args,
                                   method_id);
      } else if (method_name == "list") {
        for (json &arg : method_args) {
          if (!validate_device_id_and_attrs(arg, method_id,
                                            {kAppId, kPathLowerCase}))
            continue;

          std::string device_identifier = arg.value(kDeviceId, "");
          std::string application_identifier = arg.value(kAppId, "");
          std::string path = arg.value(kPathLowerCase, "");
          list_files(device_identifier, application_identifier.c_str(),
                     path.c_str(), method_id);
        }
      } else if (method_name == "upload") {
        for (json &arg : method_args) {
          std::string application_identifier = arg.value(kAppId, "");
          std::string device_identifier = arg.value(kDeviceId, "");
          std::vector<json> files =
              arg.value(kFiles, json::array()).get<std::vector<json>>();

          std::thread([=]() -> void {
            std::vector<FileUploadData> files_data;
            for (json file : files) {
              files_data.push_back(
                  {file.value(kSource, ""), file.value(kDestination, "")});
            }

            upload_file(device_identifier, application_identifier.c_str(),
                        files_data, method_id);
          }).detach();
        }
      } else if (method_name == "delete") {
        for (json &arg : method_args) {
          if (!validate_device_id_and_attrs(arg, method_id,
                                            {kAppId, kDestination}))
            continue;

          std::string application_identifier = arg.value(kAppId, "");
          std::string device_identifier = arg.value(kDeviceId, "");
          std::string destination = arg.value(kDestination, "");
          std::thread([=]() {
            delete_file(device_identifier, application_identifier.c_str(),
                        destination.c_str(), method_id);
          }).detach();
        }
      } else if (method_name == "read") {
        for (json &arg : method_args) {
          if (!validate_device_id_and_attrs(arg, method_id,
                                            {kAppId, kPathLowerCase}))
            continue;

          std::string application_identifier = arg.value(kAppId, "");
          std::string device_identifier = arg.value(kDeviceId, "");
          std::string path = arg.value(kPathLowerCase, "");
          read_file(device_identifier, application_identifier.c_str(),
                    path.c_str(), method_id);
        }
      } else if (method_name == "download") {
        for (json &arg : method_args) {
          if (!validate_device_id_and_attrs(arg, method_id,
                                            {kAppId, kDeviceId, kDestination}))
            continue;

          std::string application_identifier = arg.value(kAppId, "");
          std::string device_identifier = arg.value(kDeviceId, "");
          std::string source = arg.value(kSource, "");
          std::string destination = arg.value(kDestination, "");
          read_file(device_identifier, application_identifier.c_str(),
                    source.c_str(), method_id, destination.c_str());
        }
      } else if (method_name == "apps") {
        for (json &device_identifier_json : method_args) {
          std::string device_identifier =
              device_identifier_json.get<std::string>();
          perform_detached_operation(get_application_infos, device_identifier,
                                     method_id);
        }
      } else if (method_name == "log") {
        for (json &device_identifier_json : method_args) {
          std::string device_identifier =
              device_identifier_json.get<std::string>();
          perform_detached_operation(device_log, device_identifier, method_id);
        }
      } else if (method_name == "postNotification") {
        for (json &arg : method_args) {
          if (!validate_device_id_and_attrs(arg, method_id,
                                            {kCommandType, kNotificationName}))
            continue;

          std::string device_identifier = arg.value(kDeviceId, "");
          std::string command_type = arg.value(kCommandType, "");
          std::string notification_name = arg.value(kNotificationName, "");
          //          int timeout = arg.value(kTimeout, 0);

          PostNotificationInfo post_notification_info(
              {command_type, notification_name});
          std::thread([=]() {
            post_notification(device_identifier, post_notification_info,
                              method_id);
          }).detach();
        }
      } else if (method_name == "awaitNotificationResponse") {
        for (json &arg : method_args) {
          if (!validate_device_id_and_attrs(
                  arg, method_id,
                  {kResponseCommandType, kResponsePropertyName}))
            continue;

          std::string device_identifier = arg.value(kDeviceId, "");
          std::string response_command_type =
              arg.value(kResponseCommandType, "");
          std::string response_key_name = arg.value(kResponsePropertyName, "");
          SOCKET socket = arg.value(kSocket, 0);
          int timeout = arg.value(kTimeout, -1);

          AwaitNotificationResponseInfo await_notification_response_info(
              {response_command_type, response_key_name, socket, timeout});
          std::thread([=]() {
            await_notification_response(
                device_identifier, await_notification_response_info, method_id);
          }).detach();
        }
      } else if (method_name == "start") {
        for (json &arg : method_args) {
          if (!validate_device_id_and_attrs(arg, method_id,
                                            {kAppId, kDeviceId}))
            continue;

          std::string application_identifier = arg.value(kAppId, "");
          std::string device_identifier = arg.value(kDeviceId, "");
          std::string ddi = arg.value(kDeveloperDiskImage, "");
          std::string wait_for_debugger = arg.value(kWaitForDebugger, "");
          start_app(device_identifier, application_identifier, ddi, method_id,
                    wait_for_debugger == "true");
        }
      } else if (method_name == "stop") {
        for (json &arg : method_args) {
          if (!validate_device_id_and_attrs(arg, method_id,
                                            {kAppId, kDeviceId}))
            continue;

          std::string application_identifier = arg.value(kAppId, "");
          std::string device_identifier = arg.value(kDeviceId, "");
          std::string ddi = arg.value(kDeveloperDiskImage, "");
          stop_app(device_identifier, application_identifier, ddi, method_id);
        }
      } else if (method_name == "connectToPort") {
        for (json &arg : method_args) {
          if (!validate_device_id_and_attrs(arg, method_id, {kDeviceId}))
            continue;

          std::string device_identifier = arg.value(kDeviceId, "");
          int port = arg.value(kPort, -1);
          std::thread([=]() {
            connect_to_port(device_identifier, port, method_id);
          }).detach();
        }
      } else if (method_name == "exit") {
        return 0;
      }
    }
  }

  return 0;
}
