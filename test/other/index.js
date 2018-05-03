const { execSync } = require("child_process");
const { existsSync, unlinkSync, readFileSync } = require("fs");
const _ = require("lodash");
const { resolve } = require("path");
const { IOSDeviceLib } = require("../../index");
const assert = require("chai").assert;
const testAppIdentifier = "org.nativescript.iosDeviceTestApp";

describe("IOSDeviceLib", () => {
	let deviceLib;
	let deviceId;
	beforeEach((done) => {
		const deviceFoundCallback = (deviceInfo) => {
			deviceId = deviceInfo.deviceId;
			done();
		};

		const deviceLostCallback = () => {
			throw new Error("Aborting test, device was disconnected!");
		};

		deviceLib = new IOSDeviceLib(deviceFoundCallback, deviceLostCallback);
		if (process.env.USE_DEVICE_MANAGEMENT && process.env.DEVICE_CONTROL_LOCATION) {
			execSync(`python ${process.env.DEVICE_CONTROL_LOCATION} on`);
		}
	})

	afterEach(() => {
		if (deviceLib) {
			deviceLib.dispose();
		}
	})


	describe("application management", () => {
		const deviceFileDestination = "/Library/Application Support/app.js";
		const testFileUploadLocation = resolve(`${__dirname}/../fixtures/app.js`);
		const testFileDownloadLocation = resolve(`${__dirname}/../fixtures/downloadedFile.js`);

		function getListPromise() {
			return deviceLib.list([{
				path: "/Library",
				deviceId,
				appId: testAppIdentifier
			}])[0]
		}

		function assertApplication(options) {
			return deviceLib.apps([deviceId])[0]
				.then(apps => {
					assert.deepEqual(apps.deviceId, deviceId);
					const testApp = _.find(apps.response, { CFBundleIdentifier: testAppIdentifier });
					return options.assertInstalled ? assert.isObject(testApp) : assert.isUndefined(testApp);
				});
		}

		it("install", () => {
			const uninstallPromises = deviceLib.uninstall(testAppIdentifier, [deviceId]);
			return uninstallPromises[0]
				.then(() => assertApplication({ assertInstalled: false }))
				.then(() => {
					const ipaPath = resolve(`${__dirname}/../fixtures/app.ipa`);
					const installPromises = deviceLib.install(ipaPath, [deviceId]);
					return assert.isFulfilled(installPromises[0]);
				})
				.then(() => assertApplication({ assertInstalled: true }));
		});

		it("list applications", () => {
			return deviceLib.apps([deviceId])[0].then((apps) => {
				assert.deepEqual(apps.deviceId, deviceId);
				const testApp = _.find(apps.response, { CFBundleIdentifier: testAppIdentifier });
				return assert.isObject(testApp);
			});
		});

		it("list files", () => {
			return getListPromise()
				.then(listResponse => {
					const expectedResponse = [
						"/Library",
						"/Library/Caches",
						"/Library/Caches/Snapshots",
						`/Library/Caches/Snapshots/${testAppIdentifier}`,
						"/Library/Preferences"
					];
					assert.isObject(listResponse);
					assert.isArray(listResponse.response);
					return assert.includeMembers(listResponse.response, expectedResponse);
				});
		});

		it("upload file", () => {
			return getListPromise()
				.then(listResponse => {
					assert.isObject(listResponse);
					assert.isArray(listResponse.response);
					return assert.notInclude(listResponse.response, deviceFileDestination);
				})
				.then(() => {
					return deviceLib.upload([{
						files: [{
							source: testFileUploadLocation,
							destination: deviceFileDestination
						}],
						deviceId,
						appId: testAppIdentifier
					}])[0]
				})
				.then(uploadResponse => {
					assert.isObject(uploadResponse);
					return assert.strictEqual(uploadResponse.response, "Successfully uploaded files");
				})
				.then(getListPromise)
				.then(listResponse => {
					assert.isObject(listResponse);
					assert.isArray(listResponse.response);
					return assert.include(listResponse.response, deviceFileDestination);
				});
		});

		it("download file", () => {
			const destination = testFileDownloadLocation;
			if (existsSync(destination)) {
				unlinkSync(destination);
			}

			assert.isFalse(existsSync(destination));
			return deviceLib.download([{
				deviceId,
				destination,
				source: deviceFileDestination,
				appId: testAppIdentifier
			}])[0].then(uploadResponse => {
				assert.isObject(uploadResponse);
				assert.isTrue(existsSync(destination));
				assert.strictEqual(uploadResponse.response, "File written successfully!");
				const originalFileContentsStrippedSpaces = readFileSync(testFileUploadLocation).toString().replace(/\s/g, "");
				const downloadedFileContentsStrippedSpaces = readFileSync(destination).toString().replace(/\s/g, "");
				assert.strictEqual(downloadedFileContentsStrippedSpaces, originalFileContentsStrippedSpaces);
				unlinkSync(destination);
			});
		});

		it("uninstall", () => {
			assertApplicationInstalled()
				.then(() => {
					const uninstallPromises = deviceLib.uninstall(testAppIdentifier, [deviceId]);
					return assert.isFulfilled(uninstallPromises[0]);
				});
		});
	});
});