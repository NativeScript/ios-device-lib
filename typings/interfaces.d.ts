declare module IOSDeviceLib {
	interface IDeviceActionInfo {
		deviceId: string;
		event: string;
		deviceColor?: string;
		deviceName?: string;
		productType?: string;
		productVersion?: string;
		status?: string;
	}

	interface IDeviceId {
		deviceId: string;
	}

	interface IAppId {
		appId: string;
	}

	interface IAppDevice extends IDeviceId, IAppId {
	}

	interface IDestination {
		destination: string;
	}

	interface ISource {
		source: string;
	}

	interface IReadOperationData extends IAppDevice {
		path: string;
	}

	interface IFileData extends ISource, IDestination{
	}

	interface IFileOperationData extends IAppDevice, IFileData {
	}


	interface IUploadFilesData extends IAppDevice {
		files: IFileData[];
	}

	interface IDeleteFileData extends IAppDevice, IDestination {
	}

	interface IDdiApplicationData extends IAppDevice {
		ddi: string;
	}

	interface INotifyData extends IDeviceId {
		commandType: string;
		notificationName: string;
		shouldWaitForResponse: boolean;
		timeout?: number;
		responseCommandType?: string;
		responsePropertyName?: string;
	}

	interface IDeviceResponse extends IDeviceId {
		response: string;
	}

	interface IDeviceMultipleResponse extends IDeviceId {
		response: string[];
	}

	interface IApplicationInfo {
		CFBundleIdentifier: string;
		IceniumLiveSyncEnabled: boolean;
		configuration: string;
	}

	interface IDeviceAppInfo extends IDeviceId {
		response: IApplicationInfo[];
	}

	interface IDeviceLogData extends IDeviceId {
		message: string;
	}

	interface IDeviceApplication {
		CFBundleExecutable: string;
		Path: string;
	}

	interface IDeviceLookupInfo extends IDeviceId {
		response: { [key: string]: IDeviceApplication };
	}

	interface IDeviceError extends Error {
		deviceId: string;
	}

	interface IConnectToPortData extends IDeviceId {
		port: number;
	}

	interface IOSDeviceLib extends NodeJS.EventEmitter {
		new (onDeviceFound: (found: IDeviceActionInfo) => void, onDeviceLost: (found: IDeviceActionInfo) => void): IOSDeviceLib;
		install(ipaPath: string, deviceIdentifiers: string[]): Promise<IDeviceResponse>[];
		uninstall(ipaPath: string, deviceIdentifiers: string[]): Promise<IDeviceResponse>[];
		list(listArray: IReadOperationData[]): Promise<IDeviceMultipleResponse>[];
		upload(uploadArray: IUploadFilesData[]): Promise<IDeviceResponse>[];
		download(downloadArray: IFileOperationData[]): Promise<IDeviceResponse>[];
		read(readArray: IReadOperationData[]): Promise<IDeviceResponse>[];
		delete(deleteArray: IDeleteFileData[]): Promise<IDeviceResponse>[];
		notify(notifyArray: INotifyData[]): Promise<IDeviceResponse>[];
		apps(deviceIdentifiers: string[]): Promise<IDeviceAppInfo>[];
		start(startArray: IDdiApplicationData[]): Promise<IDeviceResponse>[];
		stop(stopArray: IDdiApplicationData[]): Promise<IDeviceResponse>[];
		startDeviceLog(deviceIdentifiers: string[]): void;
		connectToPort(connectToPortArray: IConnectToPortData[]): Promise<IDeviceResponse>[];
		dispose(signal?: string): void;
		on(event: "deviceLogData", listener: (response: IDeviceLogData) => void): this;
	}
}