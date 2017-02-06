#ifdef _WIN32
#include "Declarations.h"
#include "Constants.h"
#include "FileHelper.h"
#include "Printing.h"
#include "SocketHelper.h"
#include "CommonFunctions.h"

#include <map>
#include <sstream>
extern int __result;

std::string get_signature_base64(std::string image_signature_path)
{
	FileInfo signature_file_info = get_file_info(image_signature_path, true);
	return base64_encode(&signature_file_info.contents[0], signature_file_info.size);
}

bool mount_image(std::string& device_identifier, std::string& image_path, std::string& method_id)
{
	PRINT_ERROR_AND_RETURN_VALUE_IF_FAILED_RESULT(!exists(image_path), "Could not find developer disk image", device_identifier, method_id, false);
	std::string image_signature_path = image_path + ".signature";
	PRINT_ERROR_AND_RETURN_VALUE_IF_FAILED_RESULT(!exists(image_signature_path), "Could not find developer disk image signature", device_identifier, method_id, false);

	HANDLE mountFd = start_service(device_identifier, kMobileImageMounter, method_id);
	if (!mountFd)
	{
		return false;
	}

	FileInfo image_file_info = get_file_info(image_path, false);
	std::string signature_base64 = get_signature_base64(image_signature_path);

	std::stringstream xml_command;
	int bytes_sent;
	std::map<std::string, boost::any> dict;
	xml_command << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"
		"<plist version=\"1.0\">"
		"<dict>"
		"<key>Command</key>"
		"<string>ReceiveBytes</string>"
		"<key>ImageSize</key>"
		"<integer>" + std::to_string(image_file_info.size) + "</integer>"
		"<key>ImageType</key>"
		"<string>Developer</string>"
		"<key>ImageSignature</key>"
		"<data>" + signature_base64 + "</data>"
		"</dict>"
		"</plist>";

	bytes_sent = send_message(xml_command.str().c_str(), (SOCKET)mountFd);
	dict = receive_message((SOCKET)mountFd);
	PRINT_ERROR_AND_RETURN_VALUE_IF_FAILED_RESULT(dict.count(kErrorKey), boost::any_cast<std::string>(dict[kErrorKey]).c_str(), device_identifier, method_id, false);
	if (boost::any_cast<std::string>(dict[kStatusKey]) == "ReceiveBytesAck")
	{
		image_file_info = get_file_info(image_path, true);
		bytes_sent = send((SOCKET)mountFd, &image_file_info.contents[0], image_file_info.size, 0);
		dict = receive_message((SOCKET)mountFd);
		xml_command.str("");
		xml_command << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
			"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"
			"<plist version=\"1.0\">"
			"<dict>"
			"<key>Command</key>"
			"<string>MountImage</string>"
			"<key>ImageType</key>"
			"<string>Developer</string>"
			"<key>ImageSignature</key>"
			"<data>" + signature_base64 + "</data>"
			"<key>ImagePath</key>"
			"<string>/var/mobile/Media/PublicStaging/staging.dimage</string>"
			"</dict>"
			"</plist>";
		bytes_sent = send_message(xml_command.str().c_str(), (SOCKET)mountFd);
		dict = receive_message((SOCKET)mountFd);
		PRINT_ERROR_AND_RETURN_VALUE_IF_FAILED_RESULT(dict.count(kErrorKey), boost::any_cast<std::string>(dict[kErrorKey]).c_str(), device_identifier, method_id, false);
		return dict.count(kStatusKey) && has_complete_status(dict);
	}
	else
	{
		print_error("Could not transfer disk image", device_identifier, method_id, kUnexpectedError);
		return false;
	}
}

#endif // _WIN32