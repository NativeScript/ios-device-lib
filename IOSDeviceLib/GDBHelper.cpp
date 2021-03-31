#include "GDBHelper.h"
#include "Constants.h"
#include "StringHelper.h"
#include <iomanip>
#include <sstream>

#include "Printing.h"
#include "json.hpp"
using json = nlohmann::json;

const std::string kGDBErrorPrefix("$E");
const std::string kGDBOk("$OK#");
// const int kRetryCount = 3;

std::string get_gdb_message(std::string message) {
  size_t sum = 0;
  std::stringstream result;
  for (char &c : message)
    sum += c;

  sum &= 0xff;

  result << "$" << message << "#" << std::hex << sum;
  return result.str();
}

#define RETURN_IF_FALSE(expr)                                                  \
  ;                                                                            \
  if (!(expr)) {                                                               \
    return false;                                                              \
  }

int gdb_send_message(std::string message, ServiceInfo info, long long length) {
  return send_message(get_gdb_message(message), info, length);
}

bool await_response(std::string message, ServiceInfo info,
                    bool require_ok = false) {
  do {
    gdb_send_message(message, info);
    std::string answer = receive_message_raw(info);
    if (starts_with(answer, kGDBErrorPrefix)) {
      return false;
    }
    if (!require_ok || starts_with(answer, kGDBOk)) {
      break;
    }

    send_message("\x03", info);
  } while (true);

  return true;
}

bool init(std::string &executable, ServiceInfo info,
          std::string &application_identifier,
          std::map<std::string, ApplicationCache> &apps_cache,
          bool wait_for_debugger = false) {
  // Normally GDB requires + or - acknowledgments after every message
  // This however is only valuable if the communication channel is insecure
  // (packets are lost - e.g. UDP) In our case this would only cause overhead so
  // we disable it using QStartNoAckMode
  RETURN_IF_FALSE(await_response("QStartNoAckMode", info));
  send_message("+", info);
  // Set QEnvironmentHexEncoded because the arguments we pass around (e.g. path
  // to executable) may contain non-printable characters
  RETURN_IF_FALSE(await_response("QEnvironmentHexEncoded:", info));
  // This actually means QSetDisableASLR:TRUE and actually disables Address
  // space layout randomization on the next A request We need this because GDB
  // isn't connected to any inferior process (e.g. the application we want to
  // control)
  RETURN_IF_FALSE(await_response("QSetDisableASLR:1", info));
  std::stringstream result;
  std::string debugBrk = "--nativescript-debug-brk";
  // Initializes the argv in the form of arglen,argnum,arg
  // In our case:
  // arglen - twice the size of the original argument because we HEX encode it
  // and every symbol becomes two bytes argnum - 0 we only have one arg arg -
  // the actual arg, which is HEX encoded because we set the environment that
  // way
  result << "A" << executable.size() * 2 << ",0," << to_hex(executable);
  if (wait_for_debugger) {
    result << debugBrk.size() * 2 << ",1," << to_hex(debugBrk);
  }
  RETURN_IF_FALSE(await_response(result.str(), info));
  // After all of this is done we can actually call a method with these
  // arguments
  return true;
}

bool run_application(std::string &executable, ServiceInfo info,
                     std::string &application_identifier,
                     DeviceData *device_data, bool wait_for_debugger) {
  RETURN_IF_FALSE(init(executable, info, application_identifier,
                       device_data->apps_cache, wait_for_debugger));
  // Couldn't find official info on this but I'm guessing this is the method we
  // need to call
  RETURN_IF_FALSE(await_response("qLaunchSuccess", info));
  // vCont specifies a command to be run - c means continue
  await_response("vCont;c", info);
  // The answer is HEX encoded and looks somewhat like this when translated:
  /*
          KSCrash: App is running in a debugger. The following crash types have
     been disabled:
                  * KSCrashTypeMachException
                  * KSCrashTypeNSException
          2017-01-30 13:59:28.245 NativeScript220[240:20345] Logging crash
     types: 38 NativeScript loaded bundle
     file:///System/Library/Frameworks/UIKit.framework NativeScript loaded
     bundle file:///System/Library/Frameworks/Foundation.framework
  */
  // We have decided we do not need this at the moment, but it is vital that it
  // be read from the pipe Else it will remain there and may be read later on
  // and mistaken for a response to same other package

  await_response("D", info, true);
  detach_connection(info, device_data);

  return true;
}

bool stop_application(std::string &executable, ServiceInfo info,
                      std::string &application_identifier,
                      std::map<std::string, ApplicationCache> &apps_cache) {
  RETURN_IF_FALSE(init(executable, info, application_identifier, apps_cache));

  receive_message_raw(info);
  // Kill request
  await_response("\x03", info);
  await_response("k", info);
  return true;
}

void detach_connection(ServiceInfo info, DeviceData *device_data) {
  await_response("D", info);
  receive_message_raw(info);
  AMDServiceConnectionInvalidate(info.connection);
  device_data->services.erase(info.service_name);
#ifdef _WIN32
  closesocket(info.socket);
#else
  close((SOCKET)info.socket);
#endif
}
