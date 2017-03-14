"use strict";

const EventEmitter = require("events");
const path = require("path");
const os = require("os");
const spawn = require("child_process").spawn;

const bufferpack = require("bufferpack");
const Constants = require("./constants");

const HeaderSize = 4;

class IOSDeviceLibStdioHandler extends EventEmitter {
	constructor(options) {
		super();
		this._unfinishedMessage = new Buffer(0);
		this._showDebugInformation = options.debug;
	}

	startReadingData() {
		this._chProc = spawn(path.join(__dirname, "bin", os.platform(), os.arch(), "ios-device-lib").replace("app.asar", "app.asar.unpacked"));
		this._chProc.stdout.on("data", (data) => {
			this._unpackMessages(data);
		});

		if (this._showDebugInformation) {
			this._chProc.stderr.on("data", (data) => {
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

	_unpackMessages(data) {
		if (!this._unfinishedMessage.length && data.length >= HeaderSize) {
			// Get the message length header.
			const messageSizeBuffer = data.slice(0, HeaderSize);
			const messageLength = bufferpack.unpack(">i", messageSizeBuffer)[0];
			const dataLengthWithoutHeader = data.length - HeaderSize;

			if (messageLength > dataLengthWithoutHeader) {
				// Less than one message in the buffer.
				// Store the unfinished message untill the next call of the function.
				this._unfinishedMessage = data;
			} else if (dataLengthWithoutHeader > messageLength) {
				// More than one message in the buffer.
				const messageBuffer = this._getMessageFromBuffer(data, messageLength);

				// Get the rest of the message here.
				// Since our messages are not separated by whitespace or newlie
				// we do not need to add somethig when we slice the original buffer.
				const slicedBuffer = data.slice(messageBuffer.length + HeaderSize);
				this._distributeMessage(this._parseMessageFromBuffer(messageBuffer));
				this._unpackMessages(slicedBuffer);
			} else {
				// One message in the buffer.
				const messageBuffer = this._getMessageFromBuffer(data, messageLength);
				this._distributeMessage(this._parseMessageFromBuffer(messageBuffer));
			}
		} else if (this._unfinishedMessage.length && data.length >= HeaderSize) {
			// Append the new data to the unfinished message and try to unpack again.
			const concatenatedMessage = Buffer.concat([this._unfinishedMessage, data]);

			// Clear the unfinished message buffer.
			this._unfinishedMessage = new Buffer(0);
			this._unpackMessages(concatenatedMessage);
		} else {
			// While debugging the data here contains one character - \0, null or 0.
			// I think we get here when the Node inner buffer for the data of the data event
			// is filled with data and the last symbol is a part of the length header of the next message.
			// That's why we need this concat.
			// This code is tested with 10 000 000 messages in while loop.
			this._unfinishedMessage = Buffer.concat([this._unfinishedMessage, data]);
		}
	}

	_getMessageFromBuffer(buffer, messageLength) {
		return buffer.slice(HeaderSize, messageLength + HeaderSize);
	}

	_parseMessageFromBuffer(buffer) {
		return JSON.parse(buffer.toString().trim());
	}
}

exports.IOSDeviceLibStdioHandler = IOSDeviceLibStdioHandler;
