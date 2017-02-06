ios-device-lib
==============

JavaScript library, designed to facilitate communication with iOS devices. The library’s interface is Promise-based.

Usage
==

In order to use `ios-device-lib`, just add a reference to it in your package.json:
```JSON
dependencies: {
	"ios-device-lib": "*"
}
```

After that execute `npm install` in the directory, where your `package.json` is located. This command will install all your dependencies in `node_modules` directory. Now you are ready to use `ios-device-lib` in your project:

```JavaScript
const DeviceLib = require("ios-device-lib");
const dl = new DeviceLib(d => {
	console.log("Device FOUND!", device);
}, device => {
	console.log("Device LOST!", device);
});

dl.install("./app.ipa", ["deviceId1", "deviceId2"])
	.forEach(promise => {
		promise.then(response => {
			console.log("INSTALL PASSED", response);
		}).catch(err => {
			console.log("An error occurred ;(", err);
		});
	});
```

API Reference
==
This section contains information about each public method.

