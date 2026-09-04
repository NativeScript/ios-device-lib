const { assert } = require("chai");
const EventEmitter = require("events");
const Constants = require("../constants");
const { IOSDeviceLib } = require("../ios-device-lib");

describe("ios-device-lib", () => {
	describe("device lost during an operation", () => {
		const deviceA = "00008132-0016111C0E05001C";
		const deviceB = "00008150-0019290136E2401C";

		// Builds an instance whose stdio handler is a bare emitter, so no binary
		// is spawned and device events can be raised directly.
		const createLib = (options) => {
			const lib = Object.create(IOSDeviceLib.prototype);
			EventEmitter.call(lib);
			lib._options = options || {};
			lib._iosDeviceLibStdioHandler = new EventEmitter();
			lib._iosDeviceLibStdioHandler.written = [];
			lib._iosDeviceLibStdioHandler.writeData = (data) => {
				lib._iosDeviceLibStdioHandler.written.push(JSON.parse(data));
			};
			return lib;
		};

		const lastMessageId = (lib) => {
			const written = lib._iosDeviceLibStdioHandler.written;
			return written[written.length - 1].methods[0].id;
		};

		const emitDeviceLost = (lib, deviceId) => {
			lib._iosDeviceLibStdioHandler.emit(Constants.DeviceLostEventName, {
				event: Constants.DeviceEventEnum.kDeviceLost,
				deviceId
			});
		};

		const emitResponse = (lib, id, response) => {
			lib._iosDeviceLibStdioHandler.emit(Constants.DataEventName, Object.assign({ id }, response));
		};

		// Resolves with { state, value } after the current microtask queue drains,
		// so a promise that must stay pending can be asserted on.
		const settle = (promise) => {
			let result = { state: "pending" };
			promise.then(
				(value) => { result = { state: "resolved", value }; },
				(error) => { result = { state: "rejected", error }; }
			);
			return new Promise((resolve) => setImmediate(() => resolve(result)));
		};

		it("rejects when the device the operation targets is lost", async () => {
			const lib = createLib();
			const [promise] = lib.upload([{ deviceId: deviceA, appId: "org.example.app", files: [] }]);

			emitDeviceLost(lib, deviceA);

			const result = await settle(promise);
			assert.equal(result.state, "rejected");
			assert.include(result.error.message, `Device ${deviceA} lost during operation upload`);
			assert.equal(result.error.deviceId, deviceA);
		});

		it("ignores a lost event for a device the operation does not target", async () => {
			const lib = createLib();
			const [promise] = lib.upload([{ deviceId: deviceA, appId: "org.example.app", files: [] }]);

			emitDeviceLost(lib, deviceB);
			assert.equal((await settle(promise)).state, "pending");

			emitResponse(lib, lastMessageId(lib), { deviceId: deviceA, response: "ok" });
			const result = await settle(promise);
			assert.equal(result.state, "resolved");
			assert.deepEqual(result.value, { deviceId: deviceA, response: "ok" });
		});

		it("keeps listening for the target device after another device is lost", async () => {
			const lib = createLib();
			const [promise] = lib.upload([{ deviceId: deviceA, appId: "org.example.app", files: [] }]);

			emitDeviceLost(lib, deviceB);
			emitDeviceLost(lib, deviceA);

			const result = await settle(promise);
			assert.equal(result.state, "rejected");
			assert.equal(result.error.deviceId, deviceA);
		});

		it("matches the device identifier nested in install arguments", async () => {
			const lib = createLib();
			const [promise] = lib.install("/tmp/app.ipa", [deviceA]);

			emitDeviceLost(lib, deviceB);
			assert.equal((await settle(promise)).state, "pending");

			emitDeviceLost(lib, deviceA);
			const result = await settle(promise);
			assert.equal(result.state, "rejected");
			assert.equal(result.error.deviceId, deviceA);
		});

		it("matches the bare device identifier of apps arguments", async () => {
			const lib = createLib();
			const [promise] = lib.apps([deviceA]);

			emitDeviceLost(lib, deviceB);
			assert.equal((await settle(promise)).state, "pending");

			emitDeviceLost(lib, deviceA);
			assert.equal((await settle(promise)).state, "rejected");
		});

		it("rejects on any lost device when the operation names no device", async () => {
			const lib = createLib();
			const [promise] = lib.awaitNotificationResponse([{ socket: 3, timeout: 1000 }]);

			emitDeviceLost(lib, deviceB);

			const result = await settle(promise);
			assert.equal(result.state, "rejected");
			assert.equal(result.error.deviceId, deviceB);
		});

		it("does not fail device log streaming for another device", async () => {
			const lib = createLib();
			lib.startDeviceLog([deviceA]);
			const handler = lib._iosDeviceLibStdioHandler;

			emitDeviceLost(lib, deviceB);
			assert.equal(handler.listenerCount(Constants.DeviceLostEventName), 1);
			assert.equal(handler.listenerCount(Constants.DataEventName), 1);

			emitDeviceLost(lib, deviceA);
			assert.equal(handler.listenerCount(Constants.DeviceLostEventName), 0);
			assert.equal(handler.listenerCount(Constants.DataEventName), 0);
		});
	});
});
