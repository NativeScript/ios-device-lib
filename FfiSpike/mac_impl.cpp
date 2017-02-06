#ifndef _WIN32
#include "Declarations.h"
#include "dirent.h"
#include "Constants.h"
#include "StringHelper.h"
#include "CommonFunctions.h"
#include <map>
#include <array>

extern std::map<std::string, DeviceData> devices;

std::string exec(const char* cmd)
{
	std::array<char, 128> buffer;
	std::string result;
	std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
	if (!pipe) return "";
	while (!feof(pipe.get()))
	{
		if (fgets(buffer.data(), 128, pipe.get()) != NULL)
		{
			result += buffer.data();
		}
	}

	return result;
}

std::string get_developer_disk_image_directory_path(std::string& device_identifier)
{
	if (!devices.count(device_identifier))
	{
		return "";
	}

	std::string dev_dir_path = exec("xcode-select -print-path");
	dev_dir_path = trim_end(dev_dir_path) + "Platforms/iPhoneOS.platform/DeviceSupport";
	std::string build_version = get_device_property_value(device_identifier, "BuildVersion");
	std::string product_version = get_device_property_value(device_identifier, kProductVersion);
	std::vector<std::string> product_version_parts = split(product_version, '.');
	std::string product_major_version = product_version_parts[0];
	std::string product_minor_version = product_version_parts[1];
	std::string result = "";

	DIR *dir;
	struct dirent *ent;
	if ((dir = opendir(dev_dir_path.c_str())) != NULL) {
		// The contents of this directory are named like this
		// <Major>.<Minor>[ (<Build>)]
		// 9.0
		// 9.1 (13B137)
		while ((ent = readdir(dir)) != NULL) {
			std::vector<std::string> parts = split(ent->d_name, ' ');
			std::vector<std::string> version_parts = split(parts[0], '.');
			std::string major_version = version_parts[0];
			std::string minor_version = version_parts[1];
			std::string build = parts.size() > 1 ? parts[1].substr(1, parts[1].size() - 2) : "";
			if (major_version == product_major_version)
			{
				if (!result.size())
				{
					result = dev_dir_path + kUnixPathSeparator + ent->d_name;
				}
				else
				{
					// is this better than the last match?
					if (minor_version == product_minor_version)
					{
						if (build == build_version)
						{
							// it won't get better than this
							return dev_dir_path + kUnixPathSeparator + ent->d_name;
						}
						else
						{
							// major and minor match - consider this better off than last time
							result = dev_dir_path + kUnixPathSeparator + ent->d_name;
						}
					}
				}
			}
		}
		closedir(dir);
	}

	return result;
}

bool mount_image(std::string& device_identifier, std::string& image_path, std::string& method_id)
{
	std::string developer_disk_image_directory_path = get_developer_disk_image_directory_path(device_identifier);
	std::string osx_image_path = developer_disk_image_directory_path + kUnixPathSeparator + "DeveloperDiskImage.dmg";
	std::string osx_image_signature_path = osx_image_path + ".signature";
	start_session(device_identifier);
	FILE* sig = fopen(osx_image_signature_path.c_str(), "rb");
	// Signature files ALWAYS have a size of exactly 128 bytes
	void *sig_buffer = malloc(128);
	bool managed_to_read_signature = fread(sig_buffer, 1, 128, sig) == 128;
	fclose(sig);

	if (!managed_to_read_signature)
	{
		return false;
	}

	CFDataRef sig_data = CFDataCreateWithBytesNoCopy(NULL, (const UInt8 *)sig_buffer, 128, NULL);
	CFStringRef cf_image_path = create_CFString(osx_image_path.c_str());

	const void *keys_arr[] = { CFSTR("ImageType"), CFSTR("ImageSignature") };
	const void *values_arr[] = { CFSTR("Developer"), sig_data };
	CFDictionaryRef options = CFDictionaryCreate(NULL, keys_arr, values_arr, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	// The second-to-last argument is actually a function that is invoked periodically as to update the status
	// Its first argument is a pointer to Plist object that when deserialized has a Status property which tells us how far down the road we've come
	// Currently we do not need progress indication of this operation so I'm passing nullptr here
	unsigned result = AMDeviceMountImage(devices[device_identifier].device_info, cf_image_path, options, nullptr, NULL);
	stop_session(device_identifier);
	CFRelease(sig_data);
	CFRelease(cf_image_path);
	CFRelease(options);
	return result == 0 || result == kIncompatibleSignature || result == kMountImageAlreadyMounted;
}
#endif // !_WIN32