#pragma once

// Helpers for the chat server tests: polling, a log capture, little endian packet builders and readers, and a blocking TCP socket that reads
// [uint16 size][body] frames with timeouts. The builders are deliberately independent of the server code (no chat server header is included),
// so the expected bytes of a test are derived from the Java packets by hand, never from the port.

#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/write.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/base_sink.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::chatserver::test {

using namespace std::chrono_literals;

using Bytes = std::vector<uint8_t>;

/**
 * How long the helpers wait for an answer or a log line by default. The server answers on the loopback interface within milliseconds, so this only
 * bounds how long a failing test takes (and a mutation run with many of them).
 */
inline constexpr std::chrono::milliseconds DEFAULT_TIMEOUT = 2s;

/**
 * How long to wait for a log line that carries an exception: the logger symbolizes its stack trace, which takes seconds in a Debug build when
 * other test processes run at the same time.
 */
inline constexpr std::chrono::milliseconds EXCEPTION_LOG_TIMEOUT = 20s;

/** Polls pred until it returns true or the timeout passes. */
inline bool waitUntil(const std::function<bool()>& pred, std::chrono::milliseconds timeout = DEFAULT_TIMEOUT) {
	auto deadline = std::chrono::steady_clock::now() + timeout;
	while (!pred()) {
		if (std::chrono::steady_clock::now() > deadline)
			return pred();
		std::this_thread::sleep_for(5ms);
	}
	return true;
}

/** Captures the messages ("level|logger|message") of the given logger subtrees while it exists (they still reach the root sink). */
class LogCapture {
public:
	LogCapture(std::initializer_list<std::string_view> loggerNames, spdlog::level::level_enum level = spdlog::level::debug)
		: names(loggerNames.begin(), loggerNames.end()) {
		sink = std::make_shared<Sink>();
		sink->set_pattern("%l|%n|%v");
		for (const std::string& name : names)
			commons::logging::LoggerFactory::configure(name, {.level = level, .sinks = {sink}, .additive = true});
	}
	~LogCapture() {
		for (const std::string& name : names)
			commons::logging::LoggerFactory::removeConfig(name);
	}
	LogCapture(const LogCapture&) = delete;
	LogCapture& operator=(const LogCapture&) = delete;

	bool contains(std::string_view text) const { return count(text) > 0; }

	int count(std::string_view text) const {
		std::lock_guard lock(sink->linesMutex);
		int n = 0;
		for (const std::string& line : sink->lines)
			if (line.find(text) != std::string::npos)
				n++;
		return n;
	}

	/** Waits until a line contains the text. */
	bool waitFor(std::string_view text, std::chrono::milliseconds timeout = DEFAULT_TIMEOUT) const {
		return waitUntil([&] { return contains(text); }, timeout);
	}

	std::string dump() const {
		std::lock_guard lock(sink->linesMutex);
		std::string text;
		for (const std::string& line : sink->lines)
			text += line + "\n";
		return text;
	}

private:
	struct Sink : spdlog::sinks::base_sink<std::mutex> {
		mutable std::mutex linesMutex;
		std::vector<std::string> lines;

	protected:
		void sink_it_(const spdlog::details::log_msg& msg) override {
			spdlog::memory_buf_t formatted;
			formatter_->format(msg, formatted);
			std::string line(formatted.data(), formatted.size());
			while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
				line.pop_back();
			std::lock_guard lock(linesMutex);
			lines.push_back(std::move(line));
		}
		void flush_() override {}
	};

	std::vector<std::string> names;
	std::shared_ptr<Sink> sink;
};

/** UTF-16LE bytes of an ASCII text (checked), without a terminator: every character followed by a zero byte. */
inline Bytes ascii16(std::string_view text) {
	Bytes bytes;
	for (char c : text) {
		if (static_cast<unsigned char>(c) >= 0x80)
			throw commons::utils::IllegalArgumentException("ascii16 takes ASCII text only");
		bytes.push_back(static_cast<uint8_t>(c));
		bytes.push_back(0);
	}
	return bytes;
}

/** UTF-16LE bytes of a text given as UTF-16 code units (any text, e.g. u"치유성"), without a terminator. */
inline Bytes utf16le(std::u16string_view text) {
	Bytes bytes;
	for (char16_t c : text) {
		bytes.push_back(static_cast<uint8_t>(c));
		bytes.push_back(static_cast<uint8_t>(c >> 8));
	}
	return bytes;
}

/** Little endian packet builder (Java: writeC/H/D/Q/S/B). */
class PacketWriter {
public:
	PacketWriter& C(int32_t value) {
		data.push_back(static_cast<uint8_t>(value));
		return *this;
	}
	PacketWriter& H(int32_t value) {
		C(value);
		return C(value >> 8);
	}
	PacketWriter& D(int32_t value) {
		H(value);
		return H(value >> 16);
	}
	PacketWriter& Q(int64_t value) {
		D(static_cast<int32_t>(value));
		return D(static_cast<int32_t>(value >> 32));
	}
	/** ASCII text as UTF-16LE with a terminating 0 char (Java: writeS) */
	PacketWriter& S(std::string_view text) {
		B(ascii16(text));
		return H(0);
	}
	/** any text as UTF-16LE with a terminating 0 char (Java: writeS) */
	PacketWriter& S(std::u16string_view text) {
		B(utf16le(text));
		return H(0);
	}
	PacketWriter& B(std::span<const uint8_t> bytes) {
		data.insert(data.end(), bytes.begin(), bytes.end());
		return *this;
	}
	PacketWriter& B(std::initializer_list<uint8_t> bytes) {
		data.insert(data.end(), bytes.begin(), bytes.end());
		return *this;
	}
	PacketWriter& zeros(size_t count) {
		data.insert(data.end(), count, uint8_t{0});
		return *this;
	}

	/** @return the frame: [uint16 LE size including the two size bytes][data] */
	Bytes frame() const {
		Bytes frame;
		size_t size = data.size() + 2;
		frame.push_back(static_cast<uint8_t>(size));
		frame.push_back(static_cast<uint8_t>(size >> 8));
		frame.insert(frame.end(), data.begin(), data.end());
		return frame;
	}

	Bytes data;
};

/** Little endian packet reader; throws on underflow. */
class PacketReader {
public:
	explicit PacketReader(std::span<const uint8_t> data) : data(data.begin(), data.end()) {}

	uint8_t C() { return next(1)[0]; }
	int32_t H() {
		auto b = next(2);
		return b[0] | b[1] << 8;
	}
	int32_t D() {
		auto b = next(4);
		return static_cast<int32_t>(static_cast<uint32_t>(b[0]) | static_cast<uint32_t>(b[1]) << 8 | static_cast<uint32_t>(b[2]) << 16 |
			static_cast<uint32_t>(b[3]) << 24);
	}
	Bytes B(size_t count) { return next(count); }
	size_t remaining() const noexcept { return data.size() - pos; }

private:
	Bytes next(size_t count) {
		if (pos + count > data.size())
			throw commons::utils::IndexOutOfBoundsException("PacketReader underflow at " + std::to_string(pos) + " reading " + std::to_string(count));
		Bytes result(data.begin() + static_cast<ptrdiff_t>(pos), data.begin() + static_cast<ptrdiff_t>(pos + count));
		pos += count;
		return result;
	}

	Bytes data;
	size_t pos = 0;
};

/** Formats bytes as "0A 00 31 ..." for failure messages. */
inline std::string hex(std::span<const uint8_t> bytes) {
	static constexpr char digits[] = "0123456789ABCDEF";
	std::string text;
	for (uint8_t b : bytes) {
		if (!text.empty())
			text += ' ';
		text += digits[b >> 4];
		text += digits[b & 0xF];
	}
	return text;
}

/** @return a port on 127.0.0.1 that was free a moment ago (bound to port 0 and released) */
inline uint16_t probeFreePort() {
	asio::io_context io;
	asio::ip::tcp::acceptor acceptor(io, asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
	return acceptor.local_endpoint().port();
}

/** A blocking TCP client socket on 127.0.0.1 reading [uint16 size][body] frames with timeouts. Not thread safe. */
class TestSocket {
public:
	explicit TestSocket(uint16_t port) : socket(io) {
		socket.connect(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port));
		socket.set_option(asio::ip::tcp::no_delay(true));
	}

	void send(std::span<const uint8_t> bytes) { asio::write(socket, asio::buffer(bytes.data(), bytes.size())); }

	/** @return the next frame including its size header, std::nullopt on timeout or if the connection was closed (see isClosed()) */
	std::optional<Bytes> readFrame(std::chrono::milliseconds timeout = DEFAULT_TIMEOUT) {
		auto deadline = std::chrono::steady_clock::now() + timeout;
		if (!fill(2, deadline))
			return std::nullopt;
		size_t size = static_cast<size_t>(buffer[0]) | static_cast<size_t>(buffer[1]) << 8;
		if (size < 2)
			throw commons::utils::IllegalStateException("Invalid frame size " + std::to_string(size));
		if (!fill(size, deadline))
			return std::nullopt;
		Bytes frame(buffer.begin(), buffer.begin() + static_cast<ptrdiff_t>(size));
		buffer.erase(buffer.begin(), buffer.begin() + static_cast<ptrdiff_t>(size));
		return frame;
	}

	/** @return true if no frame arrives within the time (and the connection stays open) */
	bool expectSilence(std::chrono::milliseconds time = 300ms) {
		auto frame = readFrame(time);
		return !frame && !closed;
	}

	/** Reads (and discards) until the server closes the connection. @return true if it was closed within the timeout */
	bool waitClosed(std::chrono::milliseconds timeout = DEFAULT_TIMEOUT) {
		auto deadline = std::chrono::steady_clock::now() + timeout;
		while (!closed) {
			if (!fill(buffer.size() + 1, deadline) && !closed)
				return false;
			buffer.clear();
		}
		return true;
	}

	bool isClosed() const noexcept { return closed; }

	void close() {
		std::error_code ec;
		socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
		socket.close(ec);
	}

private:
	/** @return true if the buffer holds at least count bytes */
	bool fill(size_t count, std::chrono::steady_clock::time_point deadline) {
		while (buffer.size() < count) {
			if (closed)
				return false;
			auto now = std::chrono::steady_clock::now();
			if (now >= deadline)
				return false;
			std::array<uint8_t, 8192> chunk;
			bool done = false;
			std::error_code error;
			size_t received = 0;
			socket.async_read_some(asio::buffer(chunk), [&](const std::error_code& ec, size_t n) {
				done = true;
				error = ec;
				received = n;
			});
			io.restart();
			io.run_for(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now));
			if (!done) {
				socket.cancel();
				io.restart();
				io.run();
			}
			buffer.insert(buffer.end(), chunk.begin(), chunk.begin() + static_cast<ptrdiff_t>(received));
			if (error && error != asio::error::operation_aborted)
				closed = true;
		}
		return true;
	}

	asio::io_context io;
	asio::ip::tcp::socket socket;
	Bytes buffer;
	bool closed = false;
};

} // namespace aion::chatserver::test
