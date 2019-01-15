const { assert } = require("chai");
const bufferpack = require("bufferpack");
const MessageUnpackStream = require("../message-unpack-stream").MessageUnpackStream;

describe("message-unpack-stream", () => {
	describe("_transform", () => {
		const createMessageToSend = (message) => {
			const header = bufferpack.pack(">i", [message.length]);
			const binaryMsg = Buffer.from(message);
			const dataToSend = Buffer.concat([header, binaryMsg]);
			return {
				binaryMsg,
				dataToSend
			};
		};

		it("parses correctly one message", (mochaDone) => {
			const { dataToSend, binaryMsg } = createMessageToSend("testMessage");
			const msgUnpackStreamInstance = new MessageUnpackStream();

			let pushedMessage = null;
			msgUnpackStreamInstance.push = (data, encoding) => {
				pushedMessage = data;
			};

			msgUnpackStreamInstance._transform(dataToSend, null, () => {
				assert.deepEqual(pushedMessage, binaryMsg);
				mochaDone();
			});
		});

		it("does not parse anything when only partial message is sent", (mochaDone) => {
			const { dataToSend, binaryMsg } = createMessageToSend("testMessage");
			const msgUnpackStreamInstance = new MessageUnpackStream();

			let pushedMessage = null;
			msgUnpackStreamInstance.push = (data, encoding) => {
				assert.fail("Push should not be called with partial message");
				pushedMessage = data;
			};

			msgUnpackStreamInstance._transform(dataToSend.slice(0, 6), null, () => {
				assert.deepEqual(pushedMessage, null);
				mochaDone();
			});
		});

		describe("parses correctly one message passed on two calls, when the first one", () => {
			const validateMessage = (endSplitIndex, mochaDone) => {
				const { dataToSend, binaryMsg } = createMessageToSend("testMessage");
				const msgUnpackStreamInstance = new MessageUnpackStream();

				let pushedMessage = null;
				msgUnpackStreamInstance.push = (data, encoding) => {
					pushedMessage = data;
				};

				let doneCallbackCounter = 0;
				const doneCallback = () => {
					doneCallbackCounter++;

					if (doneCallbackCounter === 2) {
						assert.deepEqual(pushedMessage, binaryMsg);
						mochaDone();
					}
				};
				const firstMessage = dataToSend.slice(0, endSplitIndex);
				msgUnpackStreamInstance._transform(firstMessage, null, doneCallback);
				msgUnpackStreamInstance._transform(dataToSend.slice(firstMessage.length), null, doneCallback);
			};

			it("does not contain full header", (mochaDone) => {
				validateMessage(1, mochaDone);
			});

			it("contains only header", (mochaDone) => {
				validateMessage(4, mochaDone);
			});

			it("contains header and part of the message", (mochaDone) => {
				validateMessage(7, mochaDone);
			});
		});

		it("parses correctly two messages passed on one call", (mochaDone) => {
			const { dataToSend: firstDataToSend, binaryMsg: firstBinaryMsg } = createMessageToSend("testMessage");
			const { dataToSend: secondDataToSend, binaryMsg: secondBinaryMsg } = createMessageToSend("secondTestMessage");

			const msgUnpackStreamInstance = new MessageUnpackStream();

			let pushedMessages = [];
			msgUnpackStreamInstance.push = (data, encoding) => {
				pushedMessages.push(data)
			};

			const doneCallback = () => {
				assert.deepEqual(pushedMessages, [firstBinaryMsg, secondBinaryMsg]);
				mochaDone();
			};

			const dataToSend = Buffer.concat([firstDataToSend, secondDataToSend]);
			msgUnpackStreamInstance._transform(dataToSend, null, doneCallback);
		});

		describe("parses correctly two messages passed on two calls, when", () => {
			const validateMessage = (endSplitIndex, mochaDone) => {
				// NOTE: Do NOT change the messages here as it will result in incorrect tests below
				const { dataToSend: firstDataToSend, binaryMsg: firstBinaryMsg } = createMessageToSend("abc");
				const { dataToSend: secondDataToSend, binaryMsg: secondBinaryMsg } = createMessageToSend("def");

				const msgUnpackStreamInstance = new MessageUnpackStream();

				let pushedMessages = [];
				msgUnpackStreamInstance.push = (data, encoding) => {
					pushedMessages.push(data)
				};

				let doneCallbackCounter = 0;
				const doneCallback = () => {
					doneCallbackCounter++;

					if (doneCallbackCounter === 2) {
						assert.deepEqual(pushedMessages, [firstBinaryMsg, secondBinaryMsg]);
						mochaDone();
					}
				};

				const dataToSend = Buffer.concat([firstDataToSend, secondDataToSend]);
				const firstMessage = dataToSend.slice(0, endSplitIndex);
				msgUnpackStreamInstance._transform(firstMessage, null, doneCallback);
				msgUnpackStreamInstance._transform(dataToSend.slice(firstMessage.length), null, doneCallback);
			};

			it("first call contains part of the header of the first message", (mochaDone) => {
				validateMessage(1, mochaDone);
			});

			it("first call contains only the header of the first message", (mochaDone) => {
				validateMessage(4, mochaDone);
			});

			it("first call contains the header and part of the first message", (mochaDone) => {
				validateMessage(6, mochaDone);
			});

			it("first call contains the first message and part of the header of the second message", (mochaDone) => {
				validateMessage(9, mochaDone);
			});

			it("first call contains the first message and the header of the second message", (mochaDone) => {
				validateMessage(11, mochaDone);
			});

			it("first call contains the first message, the header of the second message and part of the second message", (mochaDone) => {
				validateMessage(13, mochaDone);
			});
		});

	});
});
