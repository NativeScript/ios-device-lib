#pragma once

#include <string>

static const unsigned kArgumentError = 3;
static const unsigned kAFCCustomError = 4;
static const unsigned kApplicationsCustomError = 5;
static const unsigned kUnexpectedError = 13;

static const unsigned kADNCIMessageConnected = 1;
static const unsigned kADNCIMessageDisconnected = 2;
static const unsigned kADNCIMessageUnknown = 3;
static const unsigned kADNCIMessageTrusted = 4;
static const unsigned kDeviceLogBytesToRead = 1 << 10;
static const unsigned kDeviceFileBytesToRead = 200;
static const size_t kDeviceUploadFilesBatchSize = 100;
#ifdef _WIN32
static const unsigned kCFStringEncodingUTF8 = 0x08000100;
#endif
static const unsigned kAMDNotFoundError = 0xe8000008;
static const unsigned kAMDAPIInternalError = 0xe8000067;
static const int kUSBInterfaceType = 1;
static const unsigned kAppleServiceNotStartedErrorCode = 0xE8000063;
static const unsigned kMountImageAlreadyMounted = 0xE8000076; // Note: This error code is actually named kAMDMobileImageMounterImageMountFailed but AppBuilder CLI and other sources think that getting this exit code during a mount is okay
static const unsigned kIncompatibleSignature = 0xE8000033; // Note: This error code is actually named kAMDInvalidDiskImageError but CLI and other sources think that getting this exit code during a mount is okay
static const char *kAppleFileConnection = "com.apple.afc";
static const char *kInstallationProxy = "com.apple.mobile.installation_proxy";
static const char *kHouseArrest = "com.apple.mobile.house_arrest";
static const char *kNotificationProxy = "com.apple.mobile.notification_proxy";
static const char *kSyslog = "com.apple.syslog_relay";
static const char *kMobileImageMounter = "com.apple.mobile.mobile_image_mounter";
static const char *kDebugServer = "com.apple.debugserver";

static const char *kUnreachableStatus = "Unreachable";
static const char *kConnectedStatus = "Connected";

static const char *kDeviceFound = "deviceFound";
static const char *kDeviceLost = "deviceLost";
static const char *kDeviceTrusted = "deviceTrusted";
static const char *kDeviceUnknown = "deviceUnknown";

static const char *kId = "id";
static const char *kNullMessageId = "null";
static const char *kDeviceId = "deviceId";
static const char *kDeveloperDiskImage = "ddi";
static const char *kAppId = "appId";
static const char *kNotificationName = "notificationName";
static const char *kDestination = "destination";
static const char *kSource = "source";
static const char *kFiles = "files";
static const char *kPathLowerCase = "path";
static const char *kPathPascalCase = "Path";
static const char *kPathUpperCase = "PATH";
static const char *kError = "error";
static const char *kErrorKey = "Error";
static const char *kCommandKey = "Command";
static const char *kStatusKey = "Status";
static const char *kResponse = "response";
static const char *kMessage = "message";
static const char *kResponseCommandType = "responseCommandType";
static const char *kResponsePropertyName = "responsePropertyName";
static const char *kShouldWaitForResponse = "shouldWaitForResponse";
static const char *kCommandType = "commandType";
static const char *kTimeout = "timeout";
static const char *kPort = "port";
static const char *kComplete = "Complete";
static const char *kCode = "code";
static const char *kProductVersion = "ProductVersion";
static const int kAFCFileModeRead = 2;
static const int kAFCFileModeWrite = 3;
static const char *kPathSeparators = "/\\";
static const char kUnixPathSeparator = '/';
static const char *kEventString = "event";
static const char *kRefreshAppMessage = "com.telerik.app.refreshApp";

static const std::string kFilePrefix("file:///");
static const std::string kDocumentsFolder("/Documents/");
static const std::string kVendDocumentsCommandName("VendDocuments");
static const std::string kVendContainerCommandName("VendContainer");
