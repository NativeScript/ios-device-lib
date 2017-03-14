#include <mutex>

#include "SocketHelper.h"
#include "Printing.h"
#include "Constants.h"
#include "json.hpp"

std::mutex print_mutex;
void print_core(const char* str, FILE *destinaton)
{
	// We need to lock the print method because in some cases we print different parts of messages
	// from different threads.
	print_mutex.lock();
	LengthEncodedMessage length_encoded_message = get_message_with_encoded_length(str);
	char* buff = new char[length_encoded_message.length];
	std::setvbuf(destinaton, buff, _IOFBF, length_encoded_message.length);
	fwrite(length_encoded_message.message, length_encoded_message.length, 1, destinaton);
	fflush(destinaton);
	delete[] buff;
	print_mutex.unlock();
}

void print(const char* str)
{
	print_core(str, stdout);
}

void trace(const char* str)
{
	print_core(str, stderr);
}

void trace(int num)
{
	std::stringstream str;
	str << num;
	trace(str.str().c_str());
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
