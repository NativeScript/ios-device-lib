"use strict";

const bufferpack = require("bufferpack");
const uuid = require('node-uuid');
const spawn = require("child_process").spawn;
const path = require("path");
const fs = require("fs");
const os = require("os");
const EventEmitter = require("events");

const DeviceEventEnum = {
	kDeviceFound: "deviceFound",
	kDeviceLost: "deviceLost",
	kDeviceTrusted: "deviceTrusted",
	kDeviceUnknown: "deviceUnknown"
};

const MethodNames = {
	install: "install",
	uninstall: "uninstall",
	list: "list",
	log: "log",
	upload: "upload",
	download: "download",
	read: "read",
	delete: "delete",
	notify: "notify",
	start: "start",
	stop: "stop",
	apps: "apps",
};

const Events = {
	deviceLogData: "deviceLogData"
};
const DataEventName = "data";

class IOSDeviceLib extends EventEmitter {
	constructor(onDeviceFound, onDeviceLost) {
		super();
		this._chProc = spawn(path.join(__dirname, "bin", os.platform(), os.arch(), "devicelib"));
		this._chProc.stdout.on(DataEventName, (data) => {
			const parsedMessage = this._read(data);
			switch (parsedMessage.event) {
				case DeviceEventEnum.kDeviceFound:
					onDeviceFound(parsedMessage);
					break;
				case DeviceEventEnum.kDeviceLost:
					onDeviceLost(parsedMessage);
					break;
				case DeviceEventEnum.kDeviceTrusted:
					onDeviceLost(parsedMessage);
					onDeviceFound(parsedMessage);
					break;
			}
		});
	}

	install(ipaPath, deviceIdentifiers) {
		return deviceIdentifiers.map(di => this._getPromise(MethodNames.install, [ipaPath, [di]]));
	}

	uninstall(appId, deviceIdentifiers) {
		return deviceIdentifiers.map(di => this._getPromise(MethodNames.uninstall, [appId, [di]]));
	}

	list(listArray) {
		return listArray.map(listObject => this._getPromise(MethodNames.list, [listObject]));
	}

	upload(uploadArray) {
		return uploadArray.map(uploadObject => this._getPromise(MethodNames.upload, [uploadObject]));
	}

	download(downloadArray) {
		return downloadArray.map(downloadObject => this._getPromise(MethodNames.download, [downloadObject]));
	}

	read(readArray) {
		return readArray.map(readObject => this._getPromise(MethodNames.read, [readObject]));
	}

	delete(deleteArray) {
		return deleteArray.map(deleteObject => this._getPromise(MethodNames.delete, [deleteObject]));
	}

	notify(notifyArray) {
		return notifyArray.map(notificationObject => this._getPromise(MethodNames.notify, [notificationObject]));
	}

	apps(deviceIdentifiers) {
		return deviceIdentifiers.map(di => this._getPromise(MethodNames.apps, [di]));
	}

	start(startArray) {
		return startArray.map(startObject => this._getPromise(MethodNames.start, [startObject]));
	}

	stop(stopArray) {
		return stopArray.map(stopObject => this._getPromise(MethodNames.stop, [stopObject]));
	}

	startDeviceLog(deviceIdentifiers) {
		this._getPromise(MethodNames.log, deviceIdentifiers, { shouldEmit: true });
	}

	dispose(signal) {
		this._chProc.removeAllListeners();
		this._chProc.kill(signal);
	}

	_getPromise(methodName, args, options) {
		return new Promise((resolve, reject) => {
			if (!args || !args.length) {
				return reject(new Error("No arguments provided"));
			}

			const id = uuid.v4();
			const eventHandler = (data) => {
				let response = this._read(data, id);
				if (response) {
					delete response.id;
					if (options && options.shouldEmit) {
						this.emit(Events.deviceLogData, response);
					} else {
						response.error ? reject(response.error) : resolve(response);
						this._chProc.stdout.removeListener(DataEventName, eventHandler);
					}
				}
			};

			this._chProc.stdout.on(DataEventName, eventHandler);
			this._chProc.stdin.write(this._getMessage(id, methodName, args));
		});
	}

	_getMessage(id, name, args) {
		return JSON.stringify({ methods: [{ id: id, name: name, args: args }] }) + '\n';
	}

	_read(data, id) {
		if (data.length) {
			const messageSizeBuffer = data.slice(0, 4);
			const bufferLength = bufferpack.unpack(">i", messageSizeBuffer)[0];
			const message = data.slice(messageSizeBuffer.length, bufferLength + messageSizeBuffer.length);
			let parsedMessage = {};

			try {
				parsedMessage = JSON.parse(message.toString());
			} catch (ex) {
				console.log("An ERROR OCCURRED");
				// What to do here?
			}

			if (!id || id === parsedMessage.id) {
				return parsedMessage;
			} else {
				return this._read(data.slice(bufferLength + messageSizeBuffer.length), id);
			}
		}
	}
}

exports.IOSDeviceLib = IOSDeviceLib;
