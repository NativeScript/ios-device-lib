#include "SocketHelper.h"
#include "Printing.h"
#include "Constants.h"
#include "json.hpp"

void print(const char* str)
{
	LengthEncodedMessage length_encoded_message = get_message_with_encoded_length(str);
	char* buff = new char[length_encoded_message.length];
	std::setvbuf(stdout, buff, _IOFBF, length_encoded_message.length);
	fwrite(length_encoded_message.message, length_encoded_message.length, 1, stdout);
	fflush(stdout);
	delete[] buff;
}

void print(const nlohmann::json& message)
{
	std::string str = message.dump();
	print(str.c_str());
}

void print_error(const char *message, std::string device_identifier, std::string method_id, int code)
{
	nlohmann::json exception;
	exception[kError][kMessage] = message;
	exception[kError][kCode] = code;
	exception[kError][kDeviceId] = device_identifier;
	exception[kDeviceId] = device_identifier;
	exception[kId] = method_id;
	//Decided it's a better idea to print everything to stdout
	//Not a good practice, but the client process wouldn't have to monitor both streams
	//fprintf(stderr, "%s", exception.dump().c_str());
	print(exception);
}

void print_errors(std::vector<std::string>& messages, std::string device_identifier, std::string method_id, int code)
{
	std::ostringstream joined_error_message;
	std::copy(messages.begin(), messages.end(), std::ostream_iterator<std::string>(joined_error_message, "\n"));
	print_error(joined_error_message.str().c_str(), device_identifier, method_id, code);
}
