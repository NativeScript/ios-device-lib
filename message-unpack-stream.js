"use strict";

const stream = require("stream");
const bufferpack = require("bufferpack");

const HeaderSize = 4;

class MessageUnpackStream extends stream.Transform {
	constructor(opts) {
		super(opts);
		this._unfinishedMessage = Buffer.from("");
	}

	_transform(data, encoding, done) {
		if (!this._unfinishedMessage.length && data.length >= HeaderSize) {
			// Get the message length header.
			const messageSizeBuffer = data.slice(0, HeaderSize);
			// > - big-endian
			// i - signed long
			const messageLength = bufferpack.unpack(">i", messageSizeBuffer)[0];
			const dataLengthWithoutHeader = data.length - HeaderSize;

			if (messageLength > dataLengthWithoutHeader) {
				// Less than one message in the buffer.
				// Store the unfinished message untill the next call of the function.
				this._unfinishedMessage = data;
				this._endUnpackingMessages(done);
			} else if (dataLengthWithoutHeader > messageLength) {
				// More than one message in the buffer.
				const messageBuffer = this._getMessageFromBuffer(data, messageLength);

				// Get the rest of the message here.
				// Since our messages are not separated by whitespace or newlie
				// we do not need to add somethig when we slice the original buffer.
				const slicedBuffer = data.slice(messageBuffer.length + HeaderSize);
				this.push(messageBuffer);
				this._transform(slicedBuffer);
				this._endUnpackingMessages(done);
			} else {
				// One message in the buffer.
				const messageBuffer = this._getMessageFromBuffer(data, messageLength);
				this.push(messageBuffer);
				this._endUnpackingMessages(done);
			}
		} else if (this._unfinishedMessage.length && data.length) {
			// Append the new data to the unfinished message and try to unpack again.
			const concatenatedMessage = Buffer.concat([this._unfinishedMessage, data]);

			// Clear the unfinished message buffer.
			this._unfinishedMessage = Buffer.from("");
			this._transform(concatenatedMessage);
			this._endUnpackingMessages(done);
		} else {
			this._unfinishedMessage = Buffer.from(data);
			this._endUnpackingMessages(done);
		}
	}

	_getMessageFromBuffer(buffer, messageLength) {
		return buffer.slice(HeaderSize, messageLength + HeaderSize);
	}

	_endUnpackingMessages(done) {
		if (done) {
			done();
		}
	}
}

exports.MessageUnpackStream = MessageUnpackStream;