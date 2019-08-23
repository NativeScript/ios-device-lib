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
	constructor(onDeviceFound, onDeviceUpdated, onDeviceLost, options) {
		super();
		this._options = options || {};
		this._iosDeviceLibStdioHandler = new IOSDeviceLibStdioHandler(this._options);
		this._iosDeviceLibStdioHandler.startReadingData();
		this._iosDeviceLibStdioHandler.on(Constants.DeviceFoundEventName, onDeviceFound);
		this._iosDeviceLibStdioHandler.on(Constants.DeviceUpdatedEventName, onDeviceUpdated);
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
		this._getPromise(MethodNames.log, deviceIdentifiers, { shouldEmit: true, disregardTimeout: true, doNotFailOnDeviceLost: true });
	}

	connectToPort(connectToPortArray) {
		return connectToPortArray.map(connectToPortObject => this._getPromise(MethodNames.connectToPort, [connectToPortObject]));
	}

	dispose(signal) {
		this.removeAllListeners();
		this._iosDeviceLibStdioHandler.dispose(signal);
	}

	_getPromise(methodName, args, options = {}) {
		return new Promise((resolve, reject) => {
			if (!args || !args.length) {
				return reject(new Error("No arguments provided"));
			}

			let timer = null;
			let eventHandler = null;
			let deviceLostHandler = null;
			const id = uuid.v4();
			const removeListeners = () => {
				if (eventHandler) {
					this._iosDeviceLibStdioHandler.removeListener(Constants.DataEventName, eventHandler);
				}

				if (deviceLostHandler) {
					this._iosDeviceLibStdioHandler.removeListener(Constants.DeviceLostEventName, deviceLostHandler);
				}
			};

			// In case device is lost during waiting for operation to complete
			// or in case we do not execute operation in the specified timeout
			// remove all handlers and reject the promise.
			// NOTE: This is not applicable for device logs, where the Promise is not awaited
			// Rejecting it results in Unhandled Rejection
			const handleMessage = (message) => {
				removeListeners();
				message.error ? reject(message.error) : resolve(message);
			};

			deviceLostHandler = (device) => {
				let message = `Device ${device.deviceId} lost during operation ${methodName} for message ${id}`;

				if (!options.doNotFailOnDeviceLost) {
					message = { error: new Error(message) };
				}

				handleMessage(message);
			};

			eventHandler = (message) => {
				if (message && message.id === id) {
					if (timer) {
						clearTimeout(timer);
					}

					delete message.id;
					if (options && options.shouldEmit) {
						this.emit(Events.deviceLogData, message);
					} else {
						handleMessage(message);
					}
				}
			};

			if (this._options.timeout && !options.disregardTimeout) {
				// TODO: Check if we should clear the timers when dispose is called.
				timer = setTimeout(() => {
					handleMessage({ error: new Error(`Timeout waiting for ${methodName} response from ios-device-lib, message id: ${id}.`) });
				}, this._options.timeout);
			}

			this._iosDeviceLibStdioHandler.on(Constants.DataEventName, eventHandler);
			this._iosDeviceLibStdioHandler.on(Constants.DeviceLostEventName, deviceLostHandler);

			this._iosDeviceLibStdioHandler.writeData(this._getMessage(id, methodName, args));
		});
	}

	_getMessage(id, name, args) {
		return JSON.stringify({ methods: [{ id: id, name: name, args: args }] }) + '\n';
	}
}

exports.IOSDeviceLib = IOSDeviceLib;
