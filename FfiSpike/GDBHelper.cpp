#include "GDBHelper.h"
#include "StringHelper.h"
#include <sstream>
#include <iomanip>

const std::string kGDBErrorPrefix("$E");
const std::string kGDBOk("$OK#");
const int kRetryCount = 3;

std::string get_gdb_message(std::string message)
{
	size_t sum = 0;
	std::stringstream result;
	for (char& c : message)
		sum += c;

	sum &= 0xff;

	result << "$" << message << "#" << std::hex << sum;
	return result.str();
}

#define RETURN_IF_FALSE(expr) ; if(!(expr)) { return false; }

int gdb_send_message(std::string message, SOCKET socket, long long length)
{
	return send_message(get_gdb_message(message), socket, length);
}

bool await_response(std::string message, SOCKET socket, std::string expected_response = kGDBOk)
{
	gdb_send_message(message, socket);
	std::string answer = receive_message_raw(socket);
	bool matches_expected_response = contains(answer, expected_response) || contains(answer, kGDBOk);
	return matches_expected_response && !starts_with(answer, kGDBErrorPrefix);
}

bool init(std::string& executable, SOCKET socket, std::string& application_identifier, std::map<std::string, ApplicationCache>& apps_cache)
{
	if (apps_cache[application_identifier].has_initialized_gdb)
		return true;
	// Noramlly GDB requires + or - acknowledgments after every message
	// This however is only valuable if the communication channel is insecure (packets are lost - e.g. UDP)
	// In our case this would only cause overhead so we disable it using QStartNoAckMode
	RETURN_IF_FALSE(await_response("QStartNoAckMode", socket, "+"));
	send_message("+", socket);
	// Set QEnvironmentHexEncoded because the arguments we pass around (e.g. path to executable) may contain non-printable characters
	RETURN_IF_FALSE(await_response("QEnvironmentHexEncoded:", socket));
	// This actually means QSetDisableASLR:TRUE and actually disables Address space layout randomization on the next A request
	// We need this because GDB isn't connected to any inferior process (e.g. the application we want to control)
	RETURN_IF_FALSE(await_response("QSetDisableASLR:1", socket));
	std::stringstream result;
	// Initializes the argv in the form of arglen,argnum,arg
	// In our case:
	// arglen - twice the size of the original argument because we HEX encode it and every symbol becomes two bytes
	// argnum - 0 we only have one arg
	// arg - the actual arg, which is HEX encoded because we set the environment that way
	result << "A" << executable.size() * 2 << ",0," << to_hex(executable);
	RETURN_IF_FALSE(await_response(result.str(), socket));
	// After all of this is done we can actually call a method with these arguments
	apps_cache[application_identifier].has_initialized_gdb = true;
	return true;
}

bool run_application(std::string& executable, SOCKET socket, std::string& application_identifier, std::map<std::string, ApplicationCache>& apps_cache)
{
	RETURN_IF_FALSE(init(executable, socket, application_identifier, apps_cache));
	// Couldn't find official info on this but I'm guessing this is the method we need to call
	RETURN_IF_FALSE(await_response("qLaunchSuccess", socket));
	// vCont specifies a command to be run - c means continue
	gdb_send_message("vCont;c", socket);
	// The answer is HEX encoded and looks somewhat like this when translated:
	/*
		KSCrash: App is running in a debugger. The following crash types have been disabled:
			* KSCrashTypeMachException
			* KSCrashTypeNSException
		2017-01-30 13:59:28.245 NativeScript220[240:20345] Logging crash types: 38
		NativeScript loaded bundle file:///System/Library/Frameworks/UIKit.framework
		NativeScript loaded bundle file:///System/Library/Frameworks/Foundation.framework
	*/
	// We have decided we do not need this at the moment, but it is vital that it be read from the pipe
	// Else it will remain there and may be read later on and mistaken for a response to same other package
	std::string answer = receive_message_raw(socket);
	return true;
}

bool stop_application(std::string & executable, SOCKET socket, std::string& application_identifier, std::map<std::string, ApplicationCache>& apps_cache)
{
	RETURN_IF_FALSE(init(executable, socket, application_identifier, apps_cache));
	// If we just send kill here it doesn't work
	// I believe it is due to some racing condition because when I send kill I sometimes receive an error and sometimes - nothing
	
	std::string answer;
	bool can_send_kill = false;
	for (size_t _ = 0; _ < kRetryCount; _++)
	{
		// The code that follows is translated from the AppBuilder CLI
		// I am not really sure what's going on here but I'll share what I know
		// The 3rd character of the ASCII table is ETX - end ot text.
		// After we send it we either get an error or some information about a thread - I presume the currently running thread
		send_message("\x03", socket);
		answer = receive_message_raw(socket);
		// If we get infromation about the thread then apparently it is now safe to send the kill command
		// Thread information contains data that I cannot decipher - looks like this:
		// $T11thread:42202;00:0540001000000000;01:0608000700000000;02:0000000000000000;03:000c000000000000;04:0321000000000000;05:ffffffff00000000;06:0000000000000000;07:0100000000000000;08:bffbffff00000000;09:0000000700000000;0a:0001000700000000;0b:c0bdf0ff00000000;0c:034e0d00004d0d00;0d:0000000000000000;0e:004e0d00004e0d00;0f:0000000000000000;10:e1ffffffffffffff;11:20a59e8201000000;12:0000000000000000;13:0000000000000000;14:ffffffff00000000;15:0321000000000000;16:000c000000000000;17:08a0d66f01000000;18:0608000700000000;19:0000000000000000;1a:0608000700000000;1b:000c000000000000;1c:0100000000000000;1d:009fd66f01000000;1e:dcffab8101000000;1f:b09ed66f01000000;20:6c01ac8101000000;21:00000060;metype:5;mecount:2;medata:10003;medata:11;memory:0x16fd69f00=609fd66f01000000ecdcab8201000000;memory:0x16fd69f60=70acd66f0100000008b9ab8201000000;#00
		if (contains(answer, "thread"))
		{
			can_send_kill = true;
			break;
		}
	}

	if (!can_send_kill)
		return false;

	// Kill request
	gdb_send_message("k", socket);
	// The answer is HEX encoded and looks somewhat like this when translated:
	// Terminated due to signal 9
	// We have decided we do not need this at the moment, but it is vital that it be read from the pipe
	// Else it will remain there and may be read later on and mistaken for a response to same other package
	answer = receive_message_raw(socket);
	return true;
}

void detach_connection(SOCKET socket)
{
	///*gdb_send_message("D", socket);
	//std::string answer = receive_message_raw(socket);*/
}
