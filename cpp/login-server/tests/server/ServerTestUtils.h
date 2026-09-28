#pragma once

// Helpers for the login server tests: blocking sockets with timeouts, packet builders/readers, a log capture, a fake Aion game client and a fake
// game server.

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <asio/connect.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/write.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/base_sink.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/StringUtils.h"
#include "support/AionLoginClientCrypto.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::loginserver::test {

using namespace std::chrono_literals;

/** Polls pred until it returns true or the timeout passes. */
inline bool waitUntil(const std::function<bool()>& pred, std::chrono::milliseconds timeout = 5s) {
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
	LogCapture(std::initializer_list<std::string_view> loggerNames, spdlog::level::level_enum level = spdlog::level::info)
		: names(loggerNames.begin(), loggerNames.end()) {
		sink = std::make_shared<Sink>();
		sink->set_pattern("%l|%n|%v");
		for (const std::string& name : names)
			commons::logging::LoggerFactory::configure(name, {.level = level, .sinks = {sink}, .additive = true});
	}
	explicit LogCapture(std::string_view loggerName, spdlog::level::level_enum level = spdlog::level::info) : LogCapture({loggerName}, level) {}
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
	bool waitFor(std::string_view text, std::chrono::milliseconds timeout = 5s) const {
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
	/** UTF-16LE with terminating 0 char */
	PacketWriter& S(std::string_view text) {
		for (char16_t c : commons::utils::StringUtils::toUtf16(text))
			H(c);
		return H(0);
	}
	PacketWriter& B(std::span<const uint8_t> bytes) {
		data.insert(data.end(), bytes.begin(), bytes.end());
		return *this;
	}
	PacketWriter& zeros(size_t count) {
		data.insert(data.end(), count, uint8_t{0});
		return *this;
	}

	std::vector<uint8_t> data;
};

/** Little endian packet reader; throws on underflow. */
class PacketReader {
public:
	explicit PacketReader(std::span<const uint8_t> data) : data(data.begin(), data.end()) {}

	uint8_t C() { return next(1)[0]; }
	int16_t H() {
		auto b = next(2);
		return static_cast<int16_t>(b[0] | b[1] << 8);
	}
	int32_t D() {
		auto b = next(4);
		return static_cast<int32_t>(static_cast<uint32_t>(b[0]) | static_cast<uint32_t>(b[1]) << 8 | static_cast<uint32_t>(b[2]) << 16 | static_cast<uint32_t>(b[3]) << 24);
	}
	int64_t Q() {
		uint64_t low = static_cast<uint32_t>(D());
		uint64_t high = static_cast<uint32_t>(D());
		return static_cast<int64_t>(low | high << 32);
	}
	std::string S() {
		std::u16string text;
		for (char16_t c; (c = static_cast<char16_t>(H())) != 0;)
			text += c;
		return commons::utils::StringUtils::toUtf8(text);
	}
	std::vector<uint8_t> B(size_t count) { return next(count); }
	size_t remaining() const noexcept { return data.size() - pos; }
	size_t position() const noexcept { return pos; }

private:
	std::vector<uint8_t> next(size_t count) {
		if (pos + count > data.size())
			throw commons::utils::IndexOutOfBoundsException("PacketReader underflow at " + std::to_string(pos) + " reading " + std::to_string(count));
		std::vector<uint8_t> result(data.begin() + static_cast<ptrdiff_t>(pos), data.begin() + static_cast<ptrdiff_t>(pos + count));
		pos += count;
		return result;
	}

	std::vector<uint8_t> data;
	size_t pos = 0;
};

/** A blocking TCP client socket reading [uint16 size][body] frames with timeouts. Not thread safe. */
class TestSocket {
public:
	/**
	 * @param port server port on 127.0.0.1
	 * @param bindAddress local address to connect from (any 127.x.x.x address works on the loopback interface)
	 */
	explicit TestSocket(uint16_t port, std::string_view bindAddress = "127.0.0.1") : socket(io) {
		socket.open(asio::ip::tcp::v4());
		socket.bind(asio::ip::tcp::endpoint(asio::ip::make_address(bindAddress), 0));
		socket.connect(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port));
		socket.set_option(asio::ip::tcp::no_delay(true));
	}

	void send(std::span<const uint8_t> bytes) { asio::write(socket, asio::buffer(bytes.data(), bytes.size())); }

	/** @return the next frame including its size header, std::nullopt on timeout or if the connection was closed (see isClosed()) */
	std::optional<std::vector<uint8_t>> readFrame(std::chrono::milliseconds timeout = 5s) {
		auto deadline = std::chrono::steady_clock::now() + timeout;
		if (!fill(2, deadline))
			return std::nullopt;
		size_t size = static_cast<size_t>(buffer[0]) | static_cast<size_t>(buffer[1]) << 8;
		if (size < 2)
			throw commons::utils::IllegalStateException("Invalid frame size " + std::to_string(size));
		if (!fill(size, deadline))
			return std::nullopt;
		std::vector<uint8_t> frame(buffer.begin(), buffer.begin() + static_cast<ptrdiff_t>(size));
		buffer.erase(buffer.begin(), buffer.begin() + static_cast<ptrdiff_t>(size));
		return frame;
	}

	/** Reads (and discards) until the server closes the connection. @return true if it was closed within the timeout */
	bool waitClosed(std::chrono::milliseconds timeout = 5s) {
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
			std::array<uint8_t, 4096> chunk;
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
	std::vector<uint8_t> buffer;
	bool closed = false;
};

/** The client side of the Aion login protocol (a fake game client). */
class AionTestClient {
public:
	explicit AionTestClient(uint16_t port, std::string_view bindAddress = "127.0.0.1") : socket(port, bindAddress) {}

	/** Reads and decrypts SM_INIT. */
	AionLoginClientCrypto::InitPacket readInit() {
		auto frame = socket.readFrame();
		if (!frame)
			throw commons::utils::IllegalStateException("No SM_INIT received");
		return crypto.decryptInitPacket(*frame);
	}

	/** Encrypts and sends a packet (opcode + data). */
	void sendPacket(std::span<const uint8_t> payload) { socket.send(crypto.encryptClientPacket(payload)); }

	/** @return the next decrypted server packet body (opcode, data, padding, checksum), std::nullopt on timeout or close */
	std::optional<std::vector<uint8_t>> readPacket(std::chrono::milliseconds timeout = 5s) {
		auto frame = socket.readFrame(timeout);
		if (!frame)
			return std::nullopt;
		return crypto.decryptServerPacket(*frame);
	}

	/** @return the next server packet, failing the test if there is none */
	std::vector<uint8_t> expectPacket(uint8_t opcode, std::chrono::milliseconds timeout = 5s) {
		auto packet = readPacket(timeout);
		if (!packet) {
			ADD_FAILURE() << "expected server packet 0x" << std::hex << int(opcode) << " but got none (closed: " << socket.isClosed() << ")";
			return std::vector<uint8_t>(64);
		}
		EXPECT_EQ((*packet)[0], opcode) << "unexpected server packet";
		return *packet;
	}

	/** CM_AUTH_GG payload with the given session id */
	// The trailing fields that CM_*.readImpl skips as unknown/random are the client's padding, checksum (last-but-one word) and the ignored last
	// word, which encryptClientPacket adds. The builders therefore only write the leading fields, so the packets have exactly the size of real
	// client packets (and the server does not warn that they were not fully read).
	static std::vector<uint8_t> buildCM_AUTH_GG(int32_t sessionId) { return PacketWriter().C(0x07).D(sessionId).D(0).D(0).D(0).D(0).data; }

	/** CM_SERVER_LIST payload */
	static std::vector<uint8_t> buildCM_SERVER_LIST(int32_t accountId, int32_t loginOk) {
		return PacketWriter().C(0x05).D(accountId).D(loginOk).C(7).zeros(6).data;
	}

	/** CM_PLAY payload */
	static std::vector<uint8_t> buildCM_PLAY(int32_t accountId, int32_t loginOk, int8_t serverId) {
		return PacketWriter().C(0x02).D(accountId).D(loginOk).C(serverId).zeros(6).data;
	}

	/** CM_UPDATE_SESSION payload */
	static std::vector<uint8_t> buildCM_UPDATE_SESSION(int32_t accountId, int32_t loginOk, int32_t reconnectKey) {
		return PacketWriter().C(0x08).D(accountId).D(loginOk).D(reconnectKey).C(68).data;
	}

	/** SM_INIT + CM_AUTH_GG -> SM_AUTH_GG */
	void authGG() {
		readInit();
		sendPacket(buildCM_AUTH_GG(crypto.getSessionId()));
		expectPacket(0x0b);
	}

	/** sends CM_LOGIN and returns the response */
	std::vector<uint8_t> login(std::string_view user, std::string_view password, uint8_t expectedOpcode) {
		sendPacket(crypto.buildCM_LOGIN(user, password));
		return expectPacket(expectedOpcode);
	}

	struct LoginOk {
		int32_t accountId;
		int32_t loginOk;
	};

	/** authGG + CM_LOGIN -> SM_LOGIN_OK */
	LoginOk loginOk(std::string_view user, std::string_view password) {
		authGG();
		PacketReader reader(login(user, password, 0x03));
		reader.C();
		LoginOk result{};
		result.accountId = reader.D();
		result.loginOk = reader.D();
		return result;
	}

	TestSocket socket;
	AionLoginClientCrypto crypto;
};

/** A fake game server speaking the plain LS <-> GS protocol. */
class GsTestClient {
public:
	explicit GsTestClient(uint16_t port, std::string_view bindAddress = "127.0.0.1") : socket(port, bindAddress) {}

	void send(std::span<const uint8_t> payload) { socket.send(AionLoginClientCrypto::makeFrame(payload)); }

	/** @return the next packet body (opcode + data), skipping SM_PING (opcode 11) unless includePings; std::nullopt on timeout or close */
	std::optional<std::vector<uint8_t>> readPacket(std::chrono::milliseconds timeout = 5s, bool includePings = false) {
		auto deadline = std::chrono::steady_clock::now() + timeout;
		while (true) {
			auto now = std::chrono::steady_clock::now();
			auto frame = socket.readFrame(std::chrono::duration_cast<std::chrono::milliseconds>(now < deadline ? deadline - now : 0ms));
			if (!frame)
				return std::nullopt;
			std::vector<uint8_t> body(frame->begin() + 2, frame->end());
			if (!includePings && body.size() == 1 && body[0] == 11)
				continue;
			return body;
		}
	}

	std::vector<uint8_t> expectPacket(uint8_t opcode, std::chrono::milliseconds timeout = 5s) {
		auto packet = readPacket(timeout);
		if (!packet) {
			ADD_FAILURE() << "expected game server packet " << int(opcode) << " but got none (closed: " << socket.isClosed() << ")";
			return std::vector<uint8_t>(64);
		}
		EXPECT_EQ((*packet)[0], opcode) << "unexpected game server packet";
		return *packet;
	}

	static std::vector<uint8_t> buildCM_GS_AUTH(int8_t id, std::string_view password, std::vector<uint8_t> ip = {127, 0, 0, 1}, int32_t port = 7777,
		int8_t minAccessLevel = 0, int32_t maxPlayers = 100) {
		return PacketWriter().C(0).C(id).S(password).C(static_cast<int32_t>(ip.size())).B(ip).H(port).C(minAccessLevel).D(maxPlayers).data;
	}

	/** CM_GS_AUTH -> SM_GS_AUTH_RESPONSE AUTHED */
	void auth(int8_t id, std::string_view password, int32_t maxPlayers = 100) {
		send(buildCM_GS_AUTH(id, password, {127, 0, 0, 1}, 7777, 0, maxPlayers));
		auto response = expectPacket(0);
		EXPECT_GE(response.size(), 2u);
		EXPECT_EQ(response[1], 0) << "game server authentication failed";
	}

	TestSocket socket;
};

} // namespace aion::loginserver::test
