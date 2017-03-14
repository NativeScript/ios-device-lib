"use strict";

const uuid = require('node-uuid');
const EventEmitter = require("events");

const Constants = require("./constants");

const IOSDeviceLibStdioHandler = require("./ios-device-lib-stdio-handler").IOSDeviceLibStdioHandler;

const MethodNames = {
	install: "install",
	uninstall: "uninstall",
	list: "list",
	log: "log",
	upload: "upload",
	download: "download",
	read: "read",
	delete: "delete",
	postNotification: "postNotification",
	awaitNotificationResponse: "awaitNotificationResponse",
	start: "start",
	stop: "stop",
	apps: "apps",
	connectToPort: "connectToPort"
};

const Events = {
	deviceLogData: "deviceLogData"
};

class IOSDeviceLib extends EventEmitter {
	constructor(onDeviceFound, onDeviceLost, options) {
		super();
		options = options || {};
		this._iosDeviceLibStdioHandler = new IOSDeviceLibStdioHandler(options);
		this._iosDeviceLibStdioHandler.startReadingData();
		this._iosDeviceLibStdioHandler.on(Constants.DeviceFoundEventName, onDeviceFound);
		this._iosDeviceLibStdioHandler.on(Constants.DeviceLostEventName, onDeviceLost);
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

	postNotification(postNotificationArray) {
		return postNotificationArray.map(notificationObject => this._getPromise(MethodNames.postNotification, [notificationObject]));
	}

	awaitNotificationResponse(awaitNotificationResponseArray) {
		return awaitNotificationResponseArray.map(awaitNotificationObject => this._getPromise(MethodNames.awaitNotificationResponse, [awaitNotificationObject]));
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

	connectToPort(connectToPortArray) {
		return connectToPortArray.map(connectToPortObject => this._getPromise(MethodNames.connectToPort, [connectToPortObject]));
	}

	dispose(signal) {
		this.removeAllListeners();
		this._iosDeviceLibStdioHandler.dispose(signal);
	}

	_getPromise(methodName, args, options) {
		return new Promise((resolve, reject) => {
			if (!args || !args.length) {
				return reject(new Error("No arguments provided"));
			}

			const id = uuid.v4();
			const eventHandler = (message) => {
				if (message && message.id === id) {
					delete message.id;
					if (options && options.shouldEmit) {
						this.emit(Events.deviceLogData, message);
					} else {
						message.error ? reject(message.error) : resolve(message);
						this._iosDeviceLibStdioHandler.removeListener(Constants.DataEventName, eventHandler);
					}
				}
			};

			this._iosDeviceLibStdioHandler.on(Constants.DataEventName, eventHandler);
			this._iosDeviceLibStdioHandler.writeData(this._getMessage(id, methodName, args));
		});
	}

	_getMessage(id, name, args) {
		return JSON.stringify({ methods: [{ id: id, name: name, args: args }] }) + '\n';
	}
}

exports.IOSDeviceLib = IOSDeviceLib;
