const { execSync } = require("child_process");
const { IOSDeviceLib } = require("../../index");
const deviceManagementCommand = `python ${process.env.DEVICE_CONTROL_LOCATION} ios`
const assert = require("chai").assert;

describe("IOSDeviceLib", () => {
	describe("device detection", () => {
		it("attach", (done) => {
			execSync(`${deviceManagementCommand} off`);
			const deviceFoundCallback = (deviceInfo) => {
				deviceLib.dispose();
				done();
			};

			const deviceLib = new IOSDeviceLib(deviceFoundCallback, () => { });
			execSync(`${deviceManagementCommand} on`);
		});

		it("detach", (done) => {
			execSync(`${deviceManagementCommand} on`);
			const deviceLostCallback = (deviceInfo) => {
				deviceLib.dispose();
				done();
			};

			const deviceLib = new IOSDeviceLib(() => { }, deviceLostCallback);
			execSync(`${deviceManagementCommand} off`);
		});
	});
});