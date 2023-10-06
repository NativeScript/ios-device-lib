#include "DevicectlHelper.h"
#include "FileHelper.h"
#include "json.hpp"
#include "CommonFunctions.h"
#include <fstream>
#include "Constants.h"

using json = nlohmann::json;


json run_devicectl(std::string command){
    char templatea[] = "/tmp/mytempfile-XXXXXX";
    char* tempFileDir = mkdtemp(templatea);
    std::string outputFile = std::string(tempFileDir) + "tempFileName";
    std::string commandWithJson = command + " --quiet --json-output "+outputFile;
    std::string cmdResult = exec(commandWithJson.c_str());
    FileInfo result = get_file_info(outputFile, true);
    std::remove(outputFile.c_str());
    if(result.size > 0){
        std::string jsonString(result.contents.begin(), result.contents.end());
        json jsonResult = json::parse(jsonString);
        return jsonResult;
    }
    nlohmann::json exception;
    exception[kError][kCode] = -1;
    return exception;
}

bool json_ok(json message){
    if(message.find(kError) != message.end()){
        return false;
    }
    return true;
}
bool devicectl_signal_application(int signal, int pid, std::string &device_identifier) {
    
    std::string command = "xcrun devicectl device process signal --device " + device_identifier + " --pid " + std::to_string(pid) + " --signal " +std::to_string(signal);
    
    json result = run_devicectl(command);
    if(!json_ok(result)) {
        json error = result.value(kError, json());
        int errorValue = error.value(kCode, -1);
        if(errorValue != 3){
            return false;
        }
    }
    return true;
}
bool devicectl_stop_application(std::string &executable, std::string &device_identifier) {
    
    std::string command = "xcrun devicectl device info processes --device " + device_identifier + " --filter \"executable.path == '"+ executable + "'\"";
    
    json result = run_devicectl(command);
    if(json_ok(result)){
        json actualResult = result.value("result", json());
        json pids = actualResult.value("runningProcesses", json::array());
        if(pids.size()>0){
            json matchingApp = pids.at(0);
            int pid = matchingApp.value("processIdentifier", -1);
            if(!devicectl_signal_application(3, pid, device_identifier)){
                return false;
            }
        }
    }
    return true;
}


bool devicectl_start_application(std::string &bundle_id, std::string &device_identifier, bool wait_for_debugger) {
    
    std::string command = "xcrun devicectl device process launch --device " + device_identifier + " --terminate-existing " + bundle_id + (wait_for_debugger?" --start-stopped":"");
     
    json result = run_devicectl(command);
    if(json_ok(result)){
        return true;
    }
    return false;
}
