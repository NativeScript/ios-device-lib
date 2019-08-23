import { Transform, TransformOptions } from "stream";

declare global {
	namespace IOSDeviceLib {
		interface IDeviceActionInfo {
			deviceId: string;
			event: string;
			deviceColor?: string;
			deviceName?: string;
			productType?: string;
			productVersion?: string;
			status?: string;
			isUSBConnected: number;
			isWiFiConnected: number;
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

		interface IFileData extends ISource, IDestination {
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

		interface IIOSApplicationData extends IDdiApplicationData {
			waitForDebugger?: string;
		}

		interface IPostNotificationData extends IDeviceId {
			commandType: string;
			notificationName: string;
		}

		interface IAwaitNotificatioNResponseData extends IDeviceId {
			responseCommandType: string;
			responsePropertyName: string;
			socket: number;
			timeout: number;
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

		interface IMessage {
			message: string;
		}

		interface IDeviceLogData extends IDeviceId, IMessage {
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

		interface ISocketData extends IDeviceId {
			socket: number;
		}

		interface ISendMessageToSocketData extends ISocketData, IMessage {
		}

		interface IReceiveMessagesFromSocketData extends ISocketData {
			callback: SocketMessageHandler;
			context: any;
		}

		interface ISocketMessage extends ISocketData, IMessage {
		}

		interface IConnectToPortResponse extends IDeviceId {
			host: string;
			port: number;
		}

		interface IOSDeviceLib extends NodeJS.EventEmitter {
			new(onDeviceFound: (found: IDeviceActionInfo) => void, onDeviceUpdated: (updated: IDeviceActionInfo) => void, onDeviceLost: (found: IDeviceActionInfo) => void): IOSDeviceLib;
			install(ipaPath: string, deviceIdentifiers: string[]): Promise<IDeviceResponse>[];
			uninstall(ipaPath: string, deviceIdentifiers: string[]): Promise<IDeviceResponse>[];
			list(listArray: IReadOperationData[]): Promise<IDeviceMultipleResponse>[];
			upload(uploadArray: IUploadFilesData[]): Promise<IDeviceResponse>[];
			download(downloadArray: IFileOperationData[]): Promise<IDeviceResponse>[];
			read(readArray: IReadOperationData[]): Promise<IDeviceResponse>[];
			delete(deleteArray: IDeleteFileData[]): Promise<IDeviceResponse>[];
			postNotification(postNotificationArray: IPostNotificationData[]): Promise<IDeviceResponse>[];
			awaitNotificationResponse(awaitNotificationResponseArray: IAwaitNotificatioNResponseData[]): Promise<IDeviceResponse>[];
			apps(deviceIdentifiers: string[]): Promise<IDeviceAppInfo>[];
			start(startArray: IIOSApplicationData[]): Promise<IDeviceResponse>[];
			stop(stopArray: IIOSApplicationData[]): Promise<IDeviceResponse>[];
			startDeviceLog(deviceIdentifiers: string[]): void;
			connectToPort(connectToPortArray: IConnectToPortData[]): Promise<IConnectToPortResponse>[];
			dispose(signal?: string): void;
			on(event: "deviceLogData", listener: (response: IDeviceLogData) => void): this;
		}

		type SocketMessageHandler = (message: ISocketMessage) => void;
		interface MessageUnpackStream extends Transform {
			new(opts?: TransformOptions): MessageUnpackStream;
		}
	}
}