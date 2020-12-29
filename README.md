ios-device-lib
==============

JavaScript library, designed to facilitate communication with iOS devices. The library’s interface is Promise-based.

Quick overview
==

In order to use `ios-device-lib`, just add a reference to it in your package.json by executing

`$ npm install --save --save-exact ios-device-lib`

In your project. Now you are ready to use `ios-device-lib` in your project:

```JavaScript
const DeviceLib = require("ios-device-lib");
const dl = new DeviceLib.IOSDeviceLib(device => {
	console.log("Device FOUND!", device);
	dl.install("./app.ipa", [device.deviceId])
		.forEach(promise => {
			promise.then(response => {
				console.log("INSTALL PASSED", response);
			}).catch(err => {
				console.log("An error occurred ;(", err);
			});
		});
}, device => {
	console.log("Device UPDATED!", device);
}, device => {
	console.log("Device LOST!", device);
});
```

Building
==
The `ios-device-lib` package can be built on either Windows (requires Visual Studio) or macOS (requires Xcode). In order to build the application one should simply open the `.sln` or `.xcodeproj` file with the respective tool and click build. Whenever building in **release** configuration, the result binary will end up in `bin/<platform-name>/<architecture>`, relative to the root of this repository. For example `bin/win32/x64/` or `bin/darwin/x64/`. The JavaScript counterpart of the C++ code expects the binaries to be present in those exact locations, so one would have to build at least once prior to using the application.

Releasing
==
When you want to release a new version, you should build the binaries for macOS and Windows. To do this, please follow the steps below:
1. On Windows machine, open IOSDeviceLib.sln file with Visual Studio.
2. Select Release configuration and x64 architecture.
3. Build the application.
4. Now switch the architecture to x86.
5. Build the application again.
6. After that open the `<repo dir>\bin\win32` - you will find two dirs there - `ia32` and `x64`.
7. Open both of the directories and delete all files except the `ios-device-lib.exe` file.
8. Copy the win32 dir to your Mac machine.
9. Open the repository on macOS
10. With Xcode open the `IOSDeviceLib.xcodeproj`
11. Build the product in Release mode (Cmd + Shift + B on my side).
12. Open the `<repo dir>/bin/darwin` directory and check if you have `x64` dir there with a single binary file in it.
13. Inside `<repo dir>/bin/` put the `win32` directory that you've copied from your Windows machine.
14. At the root of the repository execute `npm pack` - this will produce a new .tgz file.
15. Publish the `.tgz` file in `npm`.

Inter-process Communication
==
This application consists of two separate layers - one `C++` and one `Node.js`. The `C++` layer implements the application's business logic, whereas the `Node.js` layer exists simply for alleviation. Whenever the `Node.js` part is used, it launches the `C++` binary and initiates communication with it. This communication consists of requests **to the binary** via the `stdin` pipe and responses **from the binary** via the `stdout` pipe. Currently the `stderr` pipe is only used for debugging purposes.

The nature of this way of communication imposes some quirks:
* All request messages are marked with an unique identifier which is also present in the response messages. This way the `Node.js` layer can distinguish between different messages and match a request with it's response counterpart
* All messages are printed in binary on the `stdout` (and the `stderr`) pipe. Each message is prefixed with a 4 byte header, containing the length of the succeeding message. These 4-byte prefixes appear as peculiar symbols in the console whenever one decides to launch the binary directly. This is necessary as the messages have variable length and in addition it alleviates printing from the `C++` code as the strings we want to print might contain `NULL` characters which would otherwise terminate printing.

API Reference
==
In order to use the application directly one can either launch the binary, either directly or from within an IDE, or use the JavaScript API. Whenever using this library **make sure that there is at least one actual iOS device attached to the system**. This library has no functionality whatsoever without actual devices.

## C++ Binary
Upon launching the binary it will report all devices currently attached in the following form:
```
{
  "deviceColor": "1",
  "deviceId": "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
  "deviceName": "iPhone",
  "event": "deviceFound",
  "productType": "iPhone9,4",
  "productVersion": "11.0.3",
  "status": "Connected"
}
```

After that the binary is ready to accept requests via its standart input. Currently each passed message **must be on one line**, ending with a new line (Enter key) in order for it to be parsed correctly. Each message contains the **name** of the method, which you'd like to invoke, an **identification string** and the **method's arguments**. Messages are processed asynchronously, hence multiple messages can be passed at once. Example messages for the different opperations can be found below:

### List applications
```
{
  "methods": [
    {
      "id": "1",
      "name": "apps",
      "args": [
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
        "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"
      ]
    }
  ]
}
```

### Install application
```
{
  "methods": [
    {
      "id": "2",
      "name": "install",
      "args": [
        "C:\\\apps\\\app.ipa",
        [
          "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
          "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"
        ]
      ]
    }
  ]
}
```

### Uninstall application
```
{
  "methods": [
    {
      "id": "3",
      "name": "uninstall",
      "args": [
        "com.sample.MyApp",
        [
          "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
          "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy"
        ]
      ]
    }
  ]
}
```

### List files in an application
```
{
  "methods": [
    {
      "id": "4",
      "name": "list",
      "args": [
        {
          "appId": "com.sample.MyApp",
          "deviceId": "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
          "path": "Library\\\Application Support
        }
      ]
    }
  ]
}
```

### Upload a file from the local file system to the device
```
{
  "methods": [
    {
      "id": "5",
      "name": "upload",
      "args": [
        {
          "appId": "com.sample.MyApp",
          "deviceId": "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
          "source": "D:\\\Project\\\app.js",
          "destination": "Library\\\Application Support\\\LiveSync\\\app\\\app.js"
        }
      ]
    }
  ]
}
```

### Delete a file from the device
```
{
  "methods": [
    {
      "id": "6",
      "name": "delete",
      "args": [
        {
          "appId": "com.sample.MyApp",
          "deviceId": "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
          "destination": "Library\\\Application Support\\\LiveSync\\\app\\\app.js"
        }
      ]
    }
  ]
}
```

### Retrieve the contents of a file from the device
```
{
  "methods": [
    {
      "id": "7",
      "name": "read",
      "args": [
        {
          "appId": "com.sample.MyApp",
          "deviceId": "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
          "path": "Library\\\Application Support\\\LiveSync\\\app\\\app.js"
        }
      ]
    }
  ]
}
```

### Download a file from the device to the local system
```
{
  "methods": [
    {
      "id": "8",
      "name": "download",
      "args": [
        {
          "appId": "com.sample.MyApp",
          "deviceId": "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
          "source": "Library\\\Application Support\\\LiveSync\\\app\\\app.js",
          "destination": "D:\\\Downloads\\\app.js"
        }
      ]
    }
  ]
}
```

### Start printing the device log for a device
```
{
  "methods": [
    {
      "id": "9",
      "name": "log",
      "args": [
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
      ]
    }
  ]
}
```

### Post a notification to a device. (As if it was dispatched via NSNotificationCenter). This call is non-blocking.
#### Post
```
{
  "methods": [
    {
      "id": "10",
      "name": "postNotification",
      "args": [
        {
          "deviceId": "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
          "commandType": "PostNotification",
          "notificationName": "com.sample.MyApp:NativeScript.Debug.AttachAvailabilityQuery"
        }
      ]
    }
  ]
}
```

#### Observe
```
{
  "methods": [
    {
      "id": "11",
      "name": "postNotification",
      "args": [
        {
          "deviceId": "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
          "commandType": "ObserveNotification",
          "notificationName": "com.sample.MyApp:NativeScript.Debug.AttachAvailable"
        }
      ]
    }
  ]
}
```

### Post a notification to a device and await its response. This call is blocking.
```
{
  "methods": [
    {
      "id": "12",
      "name": "awaitNotificationResponse",
      "args": [
        {
          "deviceId": "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
          "socket": 10,
          "timeout": 9,
          "responseCommandType": "RelayNotification",
          "responsePropertyName": "Name"
        }
      ]
    }
  ]
}
```

### Start an application.
```
{
  "methods": [
    {
      "id": "13",
      "name": "start",
      "args": [
        {
          "appId": "com.sample.MyApp",
          "deviceId": "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        }
      ]
    }
  ]
}
```

### Stop a running application.
```
{
  "methods": [
    {
      "id": "14",
      "name": "stop",
      "args": [
        {
          "appId": "com.sample.MyApp",
          "deviceId": "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        }
      ]
    }
  ]
}
```

## JavaScript interface
A detailed definition of all the methods can be found [here](https://github.com/telerik/ios-device-lib/blob/master/typings/interfaces.d.ts#L127)

Additional information
==
* Upon launching the `ios-device-lib` binary it will immediately start a detached thread with a run loop so that it can proactively detect attaching and detaching of devices.
* All requests to the binary are executed in separate threads
* **Error understanding:** Whenever an error is raised, for example:
```
{
  "deviceId": "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
  "error": {
    "code": -402653081,
    "deviceId": "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
    "message": "Could not install application"
  },
  "id": "1"
}
```
How does one go about deciphering the error code?

First off you'd need to convert the code to hex (this can easily be done with a calculator application for example). So `-402653081` becomes `FFFFFFFFE8000067` or `E8000067` if we disregard the sign. Using this hex code you can look the error up in [this](https://github.com/samdmarshall/SDMMobileDevice/blob/master/Framework/include/SDMMobileDevice/SDMMD_Error.h) header file. In this example we can see that this is a `kAMDAPIInternalError`, hence an internal error.