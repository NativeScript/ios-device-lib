"use strict";

const EventEmitter = require("events");
const path = require("path");
const os = require("os");
const spawn = require("child_process").spawn;

const Constants = require("./constants");
const MessageUnpackStream = require("./message-unpack-stream").MessageUnpackStream;

class IOSDeviceLibStdioHandler extends EventEmitter {
	constructor(options) {
		super();
		this._showDebugInformation = options.debug;
	}

	getBinaryPath() {
		if(os.platform() === "darwin") {
			// on mac ios-device-lib is a universal binary (x86_64 & arm64)
			return path.resolve(__dirname, "bin/darwin/ios-device-lib")
		}

		if (os.platform() === "win32" || os.platform() === "win64") {
			return path.resolve(__dirname,`bin/win32/${os.arch()}ios-device-lib`)
		}
	}

	startReadingData() {
		this._chProc = spawn(
			// replace app.asar to allow spawning from electron
			this.getBinaryPath().replace("app.asar", "app.asar.unpacked")
		);

		const stdinMessageUnpackStream = new MessageUnpackStream();
		this._chProc.stdout.pipe(stdinMessageUnpackStream);
		stdinMessageUnpackStream.on("data", (data) => {
			this._distributeMessage(this._parseMessageFromBuffer(data));
		});

		if (this._showDebugInformation) {
			const stderrMessageUnpackStream = new MessageUnpackStream();
			this._chProc.stderr.pipe(stderrMessageUnpackStream);
			stderrMessageUnpackStream.on("data", (data) => {
				console.log("TRACE: ", data && data.toString());
			});
		}
	}

	writeData(data) {
		this._chProc.stdin.write(data);
	}

	dispose(signal) {
		this.removeAllListeners();
		this._chProc.removeAllListeners();
		this._chProc.kill(signal);
	}

	_distributeMessage(message) {
		if (message.event) {
			switch (message.event) {
				case Constants.DeviceEventEnum.kDeviceFound:
					this.emit(Constants.DeviceFoundEventName, message);
					break;
				case Constants.DeviceEventEnum.kDeviceUpdated:
					this.emit(Constants.DeviceUpdatedEventName, message);
					break;
				case Constants.DeviceEventEnum.kDeviceLost:
					this.emit(Constants.DeviceLostEventName, message);
					break;
				case Constants.DeviceEventEnum.kDeviceTrusted:
					this.emit(Constants.DeviceLostEventName, message);
					this.emit(Constants.DeviceFoundEventName, message);
					break;
			}
		} else {
			this.emit(Constants.DataEventName, message);
		}
	}

	_parseMessageFromBuffer(buffer) {
		return JSON.parse(buffer.toString().trim());
	}
}

exports.IOSDeviceLibStdioHandler = IOSDeviceLibStdioHandler;
