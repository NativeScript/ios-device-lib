#pragma once

#pragma region Macros
#define UNLOCK_MUTEX_AND_RETURN_IF_FAILED_RESULT(expr, mutex) ; if((__result = (expr))) { mutex.unlock(); return __result; }
#define PRINT_ERROR_AND_RETURN_VALUE_IF_FAILED_RESULT(expr, error_msg, device_identifier, method_id, value) ; if((__result = (expr))) { print_error(error_msg, device_identifier, method_id, __result); return value; }
#define PRINT_ERROR_AND_RETURN_IF_FAILED_RESULT(expr, error_msg, device_identifier, method_id) ; if((__result = (expr))) { print_error(error_msg, device_identifier, method_id, __result); return; }
#pragma endregion Macros

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>

#pragma comment(lib, "Ws2_32.lib")
#endif // _WIN32

#include <string>
#include <map>

#include "ServerHelper.h"

#pragma region Data_Structures_Definition

struct PostNotificationInfo
{
	std::string command_type;
	std::string notification_name;
};

struct AwaitNotificationResponseInfo
{
	std::string response_command_type;
	std::string response_property_name;
	SOCKET socket;
	int timeout;
};

struct DeviceApplication {
	std::string CFBundleExecutable;
	std::string Path;
};

struct DeviceInfo {
	unsigned char unknown0[16]; /* 0 - zero */
	unsigned int device_id;     /* 16 */
	unsigned int product_id;    /* 20 - set to AMD_IPHONE_PRODUCT_ID */
	char *serial;               /* 24 - set to AMD_IPHONE_SERIAL */
	unsigned int unknown1;      /* 28 */
	unsigned char unknown2[4];  /* 32 */
	unsigned int lockdown_conn; /* 36 */
	unsigned char unknown3[8];  /* 40 */
};

struct DevicePointer {
	DeviceInfo* device_info;
	unsigned msg;
};

struct LiveSyncApplicationInfo {
	bool is_livesync_supported;
	std::string application_identifier;
	std::string configuration;
};

typedef unsigned long long afc_file_ref;

struct afc_connection {
	unsigned int handle;            /* 0 */
	unsigned int unknown0;          /* 4 */
	unsigned char unknown1;         /* 8 */
	unsigned char padding[3];       /* 9 */
	unsigned int unknown2;          /* 12 */
	unsigned int unknown3;          /* 16 */
	unsigned int unknown4;          /* 20 */
	unsigned int fs_block_size;     /* 24 */
	unsigned int sock_block_size;   /* 28: always 0x3c */
	unsigned int io_timeout;        /* 32: from AFCConnectionOpen, usu. 0 */
	void *afc_lock;                 /* 36 */
	unsigned int context;           /* 40 */
};

typedef struct afc_connection * AFCConnectionRef;

struct afc_dictionary {
	unsigned char unknown[0];   /* size unknown */
};

struct afc_directory {
	unsigned char unknown[0];   /* size unknown */
};

struct afc_file {
	afc_file_ref file_ref;
	AFCConnectionRef afc_conn_p;
};

struct ApplicationCache {
	AFCConnectionRef afc_connection;
	bool has_initialized_gdb;
};

typedef HANDLE service_conn_t;
typedef service_conn_t * ServiceConnRef;
typedef struct DeviceInfo * AMDeviceRef;
struct ServiceInfo {
	HANDLE socket;
	ServiceConnRef connection;
	int connection_id;
};

struct DeviceData {
	DeviceInfo* device_info;
	struct DeviceServerData* device_server_data;
	std::map<const char*, ServiceInfo> services;
	int sessions;
	std::map<std::string, ApplicationCache> apps_cache;
	int isWiFiConnected;
	int isUSBConnected;

	void kill_device_server()
	{
		if (device_server_data != nullptr)
		{
			delete device_server_data;
			device_server_data = nullptr;
		}
	}
};

struct FileUploadData {
	std::string source;
	std::string destination;
};

#pragma endregion Data_Structures_Definition

#pragma region Dll_Type_Definitions


#ifdef _WIN32
typedef unsigned(__cdecl *device_notification_subscribe_ptr)(void(*f)(const DevicePointer*), long, long, long, HANDLE*);
typedef void(__cdecl *run_loop_ptr)();
typedef void* CFArrayRef;
typedef void* CFStringRef;
typedef void* CFDictionaryRef;
typedef void* CFURLRef;
typedef void* CFPropertyListFormat;
extern CFPropertyListFormat kCFPropertyListXMLFormat_v1_0;
typedef void* CFSocketNativeHandle;
typedef unsigned(__cdecl *device_secure_start_service_ptr)(AMDeviceRef device, CFStringRef service_name, unsigned int *unknown, ServiceConnRef * handle);
typedef CFSocketNativeHandle(__cdecl *service_connection_get_socket_ptr)(ServiceConnRef con);
typedef long(__cdecl *service_connection_receive_ptr)(ServiceConnRef, void *, long);
typedef void(__cdecl *service_connection_invalidate_ptr)(ServiceConnRef);
typedef long(__cdecl *service_connection_send_message_ptr)(ServiceConnRef serviceConnection, CFDictionaryRef message, CFPropertyListFormat format);

extern service_connection_send_message_ptr __AMDServiceConnectionSendMessage;
extern service_connection_receive_ptr __AMDServiceConnectionReceive;
extern service_connection_invalidate_ptr __AMDServiceConnectionInvalidate;

typedef unsigned(__cdecl *device_create_house_arrest_service_ptr)(const DeviceInfo*, CFStringRef identifier, void * unknown, AFCConnectionRef * handle);

typedef void*(__cdecl *device_copy_device_identifier)(const DeviceInfo*);
typedef void*(__cdecl *device_copy_value)(const DeviceInfo*, CFStringRef, CFStringRef);
typedef unsigned(__cdecl *device_uninstall_application)(HANDLE, CFStringRef, void*, void(*f)(), void*);
typedef unsigned(__cdecl *device_connection_operation)(const DeviceInfo*);
typedef unsigned(__cdecl *device_start_service)(const DeviceInfo*, CFStringRef, HANDLE*, void*);
typedef const char*(__cdecl *cfstring_get_c_string_ptr)(void*, unsigned);
typedef bool(__cdecl *cfstring_get_c_string)(void*, char*, unsigned, unsigned);
typedef unsigned long(__cdecl *cf_get_type_id)(void*);
typedef unsigned long(__cdecl *cf_get_concrete_type_id)();
typedef unsigned(__cdecl *cfdictionary_get_count)(CFDictionaryRef);
typedef void(__cdecl *cfdictionary_get_keys_and_values)(CFDictionaryRef, const void**, const void**);
typedef CFStringRef(__cdecl *cfstring_create_with_cstring)(void*, const char*, unsigned);
typedef CFArrayRef(__cdecl *cfarray_create)(void*, const void**, long, void**);
typedef unsigned(__cdecl *device_secure_operation_with_path)(int, const DeviceInfo*, CFURLRef, CFDictionaryRef, void(*f)(), int);
typedef unsigned(__cdecl *device_secure_operation_with_bundle_id)(ServiceConnRef connection, const DeviceInfo*, CFStringRef bundleIdentifier, CFDictionaryRef params, void (*installCallback)(CFDictionaryRef*, void *));
typedef void(__cdecl *cfrelease)(CFStringRef);

typedef CFDictionaryRef(__cdecl *cfdictionary_create)(void *, void*, void*, int, void*, void*);
typedef void*(__cdecl *cfurl_create_with_string)(void *, CFStringRef, void*);

typedef unsigned(__cdecl *afc_connection_open)(HANDLE, const char*, void*);
typedef unsigned(__cdecl *afc_connection_close)(AFCConnectionRef);
typedef unsigned(__cdecl *afc_file_info_open)(AFCConnectionRef, const char*, afc_dictionary**);
typedef unsigned(__cdecl *afc_directory_read)(AFCConnectionRef, afc_directory*, char**);
typedef unsigned(__cdecl *afc_directory_open)(AFCConnectionRef, const char*, afc_directory**);
typedef unsigned(__cdecl *afc_directory_close)(AFCConnectionRef, afc_directory*);
typedef unsigned(__cdecl *afc_directory_create)(AFCConnectionRef, const char *);
typedef unsigned(__cdecl *afc_remove_path)(AFCConnectionRef, const char *);
typedef unsigned(__cdecl *afc_fileref_open)(AFCConnectionRef, const char *, unsigned long long, afc_file_ref*);
typedef unsigned(__cdecl *afc_fileref_read)(AFCConnectionRef, afc_file_ref, void *, size_t*);
typedef unsigned(__cdecl *afc_get_device_info_key)(AFCConnectionRef, const char *, char**);
typedef unsigned(__cdecl *afc_fileref_write)(AFCConnectionRef, afc_file_ref, const void*, size_t);
typedef unsigned(__cdecl *afc_fileref_close)(AFCConnectionRef, afc_file_ref);
typedef unsigned(__cdecl *device_start_house_arrest)(const DeviceInfo*, CFStringRef, void*, HANDLE*, unsigned int*);
typedef unsigned(__cdecl *device_lookup_applications)(const DeviceInfo*, CFDictionaryRef, CFDictionaryRef*);
typedef int(__cdecl *usb_mux_connect_by_port)(int, int, long long*);

#endif // _WIN32
#pragma endregion Dll_Type_Definitions

#pragma region Dll_Method_Definitions

#ifdef _WIN32
extern HINSTANCE mobile_device_dll;
#define GET_IF_EXISTS(variable, type, dll, method_name) (variable ? variable : variable = (type)GetProcAddress(dll, method_name))

#define AMDeviceSecureStartService GET_IF_EXISTS(__AMDeviceSecureStartService, device_secure_start_service_ptr, mobile_device_dll, "AMDeviceSecureStartService")
#define AMDServiceConnectionGetSocket GET_IF_EXISTS(__AMDServiceConnectionGetSocket, service_connection_get_socket_ptr, mobile_device_dll, "AMDServiceConnectionGetSocket")
#define AMDServiceConnectionReceive GET_IF_EXISTS(__AMDServiceConnectionReceive, service_connection_receive_ptr, mobile_device_dll, "AMDServiceConnectionReceive")
#define AMDServiceConnectionSendMessage GET_IF_EXISTS(__AMDServiceConnectionSendMessage, service_connection_send_message_ptr, mobile_device_dll, "AMDServiceConnectionSendMessage")
#define AMDeviceCreateHouseArrestService GET_IF_EXISTS(__AMDeviceCreateHouseArrestService, device_create_house_arrest_service_ptr, mobile_device_dll, "AMDeviceCreateHouseArrestService")
#define AMDeviceNotificationSubscribe GET_IF_EXISTS(__AMDeviceNotificationSubscribe, device_notification_subscribe_ptr, mobile_device_dll, "AMDeviceNotificationSubscribe")
#define AMDeviceCopyDeviceIdentifier GET_IF_EXISTS(__AMDeviceCopyDeviceIdentifier, device_copy_device_identifier, mobile_device_dll, "AMDeviceCopyDeviceIdentifier")
#define AMDeviceCopyValue GET_IF_EXISTS(__AMDeviceCopyValue, device_copy_value, mobile_device_dll, "AMDeviceCopyValue")
#define AMDServiceConnectionInvalidate GET_IF_EXISTS(__AMDServiceConnectionInvalidate, service_connection_invalidate_ptr, mobile_device_dll, "AMDServiceConnectionInvalidate")
#define AMDeviceStartService GET_IF_EXISTS(__AMDeviceStartService, device_start_service, mobile_device_dll, "AMDeviceStartService")
#define AMDeviceUninstallApplication GET_IF_EXISTS(__AMDeviceUninstallApplication, device_uninstall_application, mobile_device_dll, "AMDeviceUninstallApplication")
#define AMDeviceSecureUninstallApplication GET_IF_EXISTS(__AMDeviceSecureUninstallApplication, device_secure_operation_with_bundle_id, mobile_device_dll, "AMDeviceSecureUninstallApplication")
#define AMDeviceStartSession GET_IF_EXISTS(__AMDeviceStartSession, device_connection_operation, mobile_device_dll, "AMDeviceStartSession")
#define AMDeviceStopSession GET_IF_EXISTS(__AMDeviceStopSession, device_connection_operation, mobile_device_dll, "AMDeviceStopSession")
#define AMDeviceConnect GET_IF_EXISTS(__AMDeviceConnect, device_connection_operation, mobile_device_dll, "AMDeviceConnect")
#define AMDeviceDisconnect GET_IF_EXISTS(__AMDeviceDisconnect, device_connection_operation, mobile_device_dll, "AMDeviceDisconnect")
#define AMDeviceIsPaired GET_IF_EXISTS(__AMDeviceIsPaired, device_connection_operation, mobile_device_dll, "AMDeviceIsPaired")
#define AMDevicePair GET_IF_EXISTS(__AMDevicePair, device_connection_operation, mobile_device_dll, "AMDevicePair")
#define AMDeviceValidatePairing GET_IF_EXISTS(__AMDeviceValidatePairing, device_connection_operation, mobile_device_dll, "AMDeviceValidatePairing")
#define AMDeviceSecureTransferPath GET_IF_EXISTS(__AMDeviceSecureTransferPath, device_secure_operation_with_path, mobile_device_dll, "AMDeviceSecureTransferPath")
#define AMDeviceSecureInstallApplication GET_IF_EXISTS(__AMDeviceSecureInstallApplication, device_secure_operation_with_path, mobile_device_dll, "AMDeviceSecureInstallApplication")
#define AMDeviceStartHouseArrestService GET_IF_EXISTS(__AMDeviceStartHouseArrestService, device_start_house_arrest, mobile_device_dll, "AMDeviceStartHouseArrestService")
#define AMDeviceLookupApplications GET_IF_EXISTS(__AMDeviceLookupApplications, device_lookup_applications, mobile_device_dll, "AMDeviceLookupApplications")

#define AMDeviceGetConnectionID GET_IF_EXISTS(__AMDeviceGetConnectionID, device_connection_operation, mobile_device_dll, "AMDeviceGetConnectionID")
#define AMDeviceGetInterfaceType GET_IF_EXISTS(__AMDeviceGetInterfaceType, device_connection_operation, mobile_device_dll, "AMDeviceGetInterfaceType")
#define USBMuxConnectByPort GET_IF_EXISTS(__USBMuxConnectByPort, usb_mux_connect_by_port, mobile_device_dll, "USBMuxConnectByPort")

#define CFStringGetCStringPtr GET_IF_EXISTS(__CFStringGetCStringPtr, cfstring_get_c_string_ptr, core_foundation_dll, "CFStringGetCStringPtr")
#define CFStringGetCString GET_IF_EXISTS(__CFStringGetCString, cfstring_get_c_string, core_foundation_dll, "CFStringGetCString")
#define CFGetTypeID GET_IF_EXISTS(__CFGetTypeID, cf_get_type_id, core_foundation_dll, "CFGetTypeID")
#define CFStringGetTypeID GET_IF_EXISTS(__CFStringGetTypeID, cf_get_concrete_type_id, core_foundation_dll, "CFStringGetTypeID")
#define CFDictionaryGetTypeID GET_IF_EXISTS(__CFDictionaryGetTypeID, cf_get_concrete_type_id, core_foundation_dll, "CFDictionaryGetTypeID")
#define CFDictionaryGetCount GET_IF_EXISTS(__CFDictionaryGetCount, cfdictionary_get_count, core_foundation_dll, "CFDictionaryGetCount")
#define CFDictionaryGetKeysAndValues GET_IF_EXISTS(__CFDictionaryGetKeysAndValues, cfdictionary_get_keys_and_values, core_foundation_dll, "CFDictionaryGetKeysAndValues")
#define CFStringCreateWithCString GET_IF_EXISTS(__CFStringCreateWithCString, cfstring_create_with_cstring, core_foundation_dll, "CFStringCreateWithCString")
#define CFArrayCreate GET_IF_EXISTS(__CFArrayCreate, cfarray_create, core_foundation_dll, "CFArrayCreate")
#define CFURLCreateWithString GET_IF_EXISTS(__CFURLCreateWithString, cfurl_create_with_string, core_foundation_dll, "CFURLCreateWithString")
#define CFDictionaryCreate GET_IF_EXISTS(__CFDictionaryCreate, cfdictionary_create, core_foundation_dll, "CFDictionaryCreate")
#define CFRelease GET_IF_EXISTS(__CFRelease, cfrelease, core_foundation_dll, "CFRelease")

#define AFCConnectionOpen GET_IF_EXISTS(__AFCConnectionOpen, afc_connection_open, mobile_device_dll, "AFCConnectionOpen")
#define AFCConnectionClose GET_IF_EXISTS(__AFCConnectionClose, afc_connection_close, mobile_device_dll, "AFCConnectionClose")
#define AFCRemovePath GET_IF_EXISTS(__AFCRemovePath, afc_remove_path, mobile_device_dll, "AFCRemovePath")
#define AFCFileInfoOpen GET_IF_EXISTS(__AFCFileInfoOpen, afc_file_info_open, mobile_device_dll, "AFCFileInfoOpen")
#define AFCDirectoryRead GET_IF_EXISTS(__AFCDirectoryRead, afc_directory_read, mobile_device_dll, "AFCDirectoryRead")
#define AFCDirectoryOpen GET_IF_EXISTS(__AFCDirectoryOpen, afc_directory_open, mobile_device_dll, "AFCDirectoryOpen")
#define AFCDirectoryClose GET_IF_EXISTS(__AFCDirectoryClose, afc_directory_close, mobile_device_dll, "AFCDirectoryClose")
#define AFCDirectoryCreate GET_IF_EXISTS(__AFCDirectoryCreate, afc_directory_create, mobile_device_dll, "AFCDirectoryCreate")
#define AFCFileRefOpen GET_IF_EXISTS(__AFCFileRefOpen, afc_fileref_open, mobile_device_dll, "AFCFileRefOpen")
#define AFCFileRefRead GET_IF_EXISTS(__AFCFileRefRead, afc_fileref_read, mobile_device_dll, "AFCFileRefRead")
#define AFCGetDeviceInfoKey GET_IF_EXISTS(__AFCGetDeviceInfoKey, afc_get_device_info_key, mobile_device_dll, "AFCGetDeviceInfoKey")
#define AFCFileRefWrite GET_IF_EXISTS(__AFCFileRefWrite, afc_fileref_write, mobile_device_dll, "AFCFileRefWrite")
#define AFCFileRefClose GET_IF_EXISTS(__AFCFileRefClose, afc_fileref_close, mobile_device_dll, "AFCFileRefClose")
#endif // _WIN32

#pragma endregion Dll_Method_Definitions

#ifndef _WIN32

#include <CoreFoundation/CoreFoundation.h>

extern "C"
{
	CFSocketNativeHandle  AMDServiceConnectionGetSocket(ServiceConnRef con);
	long AMDServiceConnectionReceive(ServiceConnRef, void *, long);
	void AMDServiceConnectionInvalidate(ServiceConnRef);
	long AMDServiceConnectionSendMessage(ServiceConnRef serviceConnection, CFDictionaryRef message, CFPropertyListFormat format);
	unsigned AMDeviceSecureStartService(AMDeviceRef device, CFStringRef service_name, unsigned int *unknown, ServiceConnRef * handle);
	unsigned AMDeviceNotificationSubscribe(void(*f)(const DevicePointer*), long, long, long, HANDLE*);
	CFStringRef AMDeviceCopyDeviceIdentifier(const DeviceInfo*);
	CFStringRef AMDeviceCopyValue(const DeviceInfo*, CFStringRef, CFStringRef);
	unsigned AMDeviceMountImage(const DeviceInfo*, CFStringRef, CFDictionaryRef, void(*f)(void*, int), void*);
	unsigned AMDeviceStartService(const DeviceInfo*, CFStringRef, HANDLE*, void*);
	unsigned AMDeviceLookupApplications(const DeviceInfo*, CFDictionaryRef, CFDictionaryRef*);
	unsigned AMDeviceCreateHouseArrestService(const DeviceInfo*, CFStringRef identifier, void * unknown, AFCConnectionRef * handle);
	int AMDeviceGetConnectionID(const DeviceInfo*);
	int AMDeviceGetInterfaceType(const DeviceInfo*);
	unsigned AMDeviceSecureUninstallApplication(ServiceConnRef connection, const DeviceInfo*, CFStringRef bundleIdentifier, CFDictionaryRef params, void (*installCallback)(CFDictionaryRef*, void *));
	unsigned AMDeviceUninstallApplication(HANDLE, CFStringRef, void*, void(*f)(), void*);
	unsigned AMDeviceStartSession(const DeviceInfo*);
	unsigned AMDeviceStopSession(const DeviceInfo*);
	unsigned AMDeviceConnect(const DeviceInfo*);
	unsigned AMDeviceDisconnect(const DeviceInfo*);
	unsigned AMDeviceIsPaired(const DeviceInfo*);
	unsigned AMDevicePair(const DeviceInfo*);
	unsigned AMDeviceValidatePairing(const DeviceInfo*);
	unsigned AMDeviceSecureTransferPath(int, const DeviceInfo*, CFURLRef, CFDictionaryRef, void(*f)(), int);
	unsigned AMDeviceSecureInstallApplication(int, const DeviceInfo*, CFURLRef, CFDictionaryRef, void(*f)(), int);
	unsigned AMDeviceStartHouseArrestService(const DeviceInfo*, CFStringRef, void*, HANDLE*, unsigned int*);
	unsigned AFCConnectionOpen(HANDLE, const char*, void*);
	unsigned AFCConnectionClose(AFCConnectionRef);
	unsigned AFCRemovePath(AFCConnectionRef, const char*);
	unsigned AFCFileInfoOpen(AFCConnectionRef, const char*, afc_dictionary**);
	unsigned AFCDirectoryRead(AFCConnectionRef, afc_directory*, char**);
	unsigned AFCDirectoryOpen(AFCConnectionRef, const char*, afc_directory**);
	unsigned AFCDirectoryClose(AFCConnectionRef, afc_directory*);
	unsigned AFCDirectoryCreate(AFCConnectionRef, const char*);
	unsigned AFCFileRefOpen(AFCConnectionRef, const char*, unsigned long long, afc_file_ref*);
	unsigned AFCFileRefRead(AFCConnectionRef, afc_file_ref, void*, size_t*);
	unsigned AFCFileRefWrite(AFCConnectionRef, afc_file_ref, const void*, size_t);
	unsigned AFCFileRefClose(AFCConnectionRef, afc_file_ref);
	unsigned USBMuxConnectByPort(int, int, long long*);
}

#endif // !_WIN32
