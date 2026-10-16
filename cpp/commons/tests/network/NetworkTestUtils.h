#pragma once

// Shared helpers for the network tests: a log capturing sink, a test connection with a probe recording its callbacks, and a blocking test
// client with timeouts.

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include <asio/connect.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>
#include <spdlog/sinks/base_sink.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/network/AConnection.h"
#include "aion/commons/network/NioServer.h"
#include "aion/commons/network/ServerCfg.h"
#include "aion/commons/network/packet/BaseServerPacket.h"
#include "aion/commons/utils/concurrent/ThreadName.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace nettest {

using namespace std::chrono_literals;
using namespace aion::commons;

/** Polls pred until it returns true or the timeout passes. */
inline bool waitUntil(const std::function<bool()>& pred, std::chrono::milliseconds timeout = 5s) {
	auto deadline = std::chrono::steady_clock::now() + timeout;
	while (!pred()) {
		if (std::chrono::steady_clock::now() > deadline)
			return pred();
		std::this_thread::sleep_for(2ms);
	}
	return true;
}

/** Collects formatted log lines ("level|logger|message") of all loggers below com.aionemu.commons.network. */
class LogCapture : public spdlog::sinks::base_sink<std::mutex> {
public:
	static LogCapture& instance() {
		static std::shared_ptr<LogCapture> capture = [] {
			auto sink = std::make_shared<LogCapture>();
			sink->set_pattern("%l|%n|%v");
			logging::LoggerFactory::configure("com.aionemu.commons.network", {.level = spdlog::level::info, .sinks = {sink}, .additive = true});
			return sink;
		}();
		return *capture;
	}

	bool contains(std::string_view text) {
		std::lock_guard lock(mutex_);
		for (const auto& line : lines)
			if (line.find(text) != std::string::npos)
				return true;
		return false;
	}

	int count(std::string_view text) {
		std::lock_guard lock(mutex_);
		int n = 0;
		for (const auto& line : lines)
			if (line.find(text) != std::string::npos)
				n++;
		return n;
	}

	void clear() {
		std::lock_guard lock(mutex_);
		lines.clear();
	}

protected:
	void sink_it_(const spdlog::details::log_msg& msg) override {
		spdlog::memory_buf_t formatted;
		formatter_->format(msg, formatted);
		lines.emplace_back(formatted.data(), formatted.size());
	}
	void flush_() override {}

private:
	std::vector<std::string> lines;
};

/** A server packet with an arbitrary payload, framed with the uint16 length prefix. */
class TestServerPacket : public network::packet::BaseServerPacket {
public:
	explicit TestServerPacket(std::vector<uint8_t> payload) : payload(std::move(payload)) {}

	void write(utils::ByteBuffer& buf) const {
		writeH(buf, 0);
		writeB(buf, payload);
		buf.flip();
		buf.putShort(0, static_cast<int16_t>(buf.limit()));
	}

	const std::vector<uint8_t> payload;
};

inline std::shared_ptr<TestServerPacket> makePacket(std::vector<uint8_t> payload) {
	return std::make_shared<TestServerPacket>(std::move(payload));
}

class TestConnection;

/** Records the callbacks of TestConnections and configures their behaviour. */
struct Probe {
	enum class ServerCloseAction { CLOSE, CLOSE_WITH_PACKET, DO_NOTHING };

	std::mutex mutex;
	std::vector<std::vector<uint8_t>> received;
	std::atomic<int> initializedCount = 0;
	std::atomic<int> disconnectCount = 0;
	std::atomic<int> serverCloseCount = 0;
	std::atomic<bool> processDataBeforeInitialized = false;
	std::atomic<bool> onDisconnectOnIoThread = false;
	ServerCloseAction serverCloseAction = ServerCloseAction::CLOSE;
	/** optional: decides the return value of processData (may throw) */
	std::function<bool(TestConnection& connection, const std::vector<uint8_t>& payload)> onPacket;
	/** optional: called in initialized() */
	std::function<void(TestConnection& connection)> onInitialized;
	/** optional: called in every writeData() call */
	std::function<void(TestConnection& connection)> onWriteData;

	std::vector<std::vector<uint8_t>> receivedCopy() {
		std::lock_guard lock(mutex);
		return received;
	}
	size_t receivedCount() {
		std::lock_guard lock(mutex);
		return received.size();
	}
};

class TestConnection : public network::AConnection<TestServerPacket> {
public:
	TestConnection(asio::ip::tcp::socket socket, network::NioServer& server, std::shared_ptr<Probe> probe, int32_t rbSize = 1024, int32_t wbSize = 1024)
		: AConnection(std::move(socket), server, rbSize, wbSize), probe(std::move(probe)) {}

	std::string toString() const override { return "TestConnection " + getIP(); }

	std::atomic<bool> initializedDone = false;

protected:
	bool processData(utils::ByteBuffer& data) override {
		if (!initializedDone)
			probe->processDataBeforeInitialized = true;
		std::vector<uint8_t> payload(data.remainingSpan().begin(), data.remainingSpan().end());
		if (probe->onPacket)
			return probe->onPacket(*this, payload);
		std::lock_guard lock(probe->mutex);
		probe->received.push_back(std::move(payload));
		return true;
	}

	bool writeData(utils::ByteBuffer& data) override {
		std::lock_guard lock(guard); // already held by the caller, must be re-entrant
		if (probe->onWriteData)
			probe->onWriteData(*this);
		if (sendMsgQueue.empty())
			return false;
		auto packet = std::move(sendMsgQueue.front());
		sendMsgQueue.pop_front();
		packet->write(data);
		return true;
	}

	void initialized() override {
		probe->initializedCount++;
		if (probe->onInitialized)
			probe->onInitialized(*this);
		initializedDone = true;
	}

	void onDisconnect() override {
		std::string name = utils::concurrent::getCurrentThreadName();
		if (name.find("Dispatcher") != std::string::npos)
			probe->onDisconnectOnIoThread = true;
		probe->disconnectCount++;
	}

	void onServerClose() override {
		probe->serverCloseCount++;
		switch (probe->serverCloseAction) {
			case Probe::ServerCloseAction::CLOSE:
				close();
				break;
			case Probe::ServerCloseAction::CLOSE_WITH_PACKET:
				close(makePacket({'b', 'y', 'e'}));
				break;
			case Probe::ServerCloseAction::DO_NOTHING:
				break;
		}
	}

private:
	std::shared_ptr<Probe> probe;
};

/** A NioServer on 127.0.0.1 with an ephemeral port, creating TestConnections. */
struct TestServer {
	std::shared_ptr<Probe> probe = std::make_shared<Probe>();
	std::unique_ptr<network::NioServer> server;
	std::mutex mutex;
	std::vector<std::shared_ptr<TestConnection>> connections;
	uint16_t port = 0;
	int32_t rbSize = 1024;
	int32_t wbSize = 1024;
	/** if set, the factory returns nullptr */
	bool rejectConnections = false;
	/** number of upcoming factory calls that throw */
	std::atomic<int> factoryThrows = 0;
	/** optional: called at the start of every factory call */
	std::function<void()> onFactory;
	/** optional: executor for onDisconnect callbacks */
	network::NioServer::DisconnectExecutor dcExecutor;

	void start(int32_t threads = 2) {
		network::ServerCfg cfg{{"127.0.0.1", 0}, "test clients", [this](asio::ip::tcp::socket socket, network::NioServer& nioServer) -> std::shared_ptr<network::AConnectionBase> {
			if (onFactory)
				onFactory();
			if (factoryThrows > 0) {
				factoryThrows--;
				throw std::runtime_error("factory failure");
			}
			if (rejectConnections)
				return nullptr;
			auto connection = std::make_shared<TestConnection>(std::move(socket), nioServer, probe, rbSize, wbSize);
			std::lock_guard lock(mutex);
			connections.push_back(connection);
			return connection;
		}};
		server = std::make_unique<network::NioServer>(threads, std::vector{cfg});
		if (dcExecutor)
			server->connect(dcExecutor);
		else
			server->connect();
		port = server->getBoundAddresses().at(0).port;
	}

	std::shared_ptr<TestConnection> connection(size_t index, std::chrono::milliseconds timeout = 5s) {
		std::shared_ptr<TestConnection> result;
		waitUntil([&] {
			std::lock_guard lock(mutex);
			if (connections.size() > index)
				result = connections[index];
			return result != nullptr;
		}, timeout);
		return result;
	}
};

/** Encodes a frame: uint16 LE length (including itself) + payload. */
inline std::vector<uint8_t> frame(std::span<const uint8_t> payload) {
	size_t size = payload.size() + 2;
	std::vector<uint8_t> bytes{static_cast<uint8_t>(size & 0xFF), static_cast<uint8_t>((size >> 8) & 0xFF)};
	bytes.insert(bytes.end(), payload.begin(), payload.end());
	return bytes;
}

inline std::vector<uint8_t> frame(std::initializer_list<uint8_t> payload) {
	return frame(std::span<const uint8_t>(payload.begin(), payload.size()));
}

/** A blocking TCP client with timeouts on reads. */
class TestClient {
public:
	explicit TestClient(uint16_t port) {
		socket.connect({asio::ip::make_address_v4("127.0.0.1"), port});
		socket.set_option(asio::ip::tcp::no_delay(true));
	}

	void send(std::span<const uint8_t> bytes) { asio::write(socket, asio::buffer(bytes.data(), bytes.size())); }

	/** @return the next frame's payload, or nullopt on timeout, EOF or error */
	std::optional<std::vector<uint8_t>> readFrame(std::chrono::milliseconds timeout = 5s) {
		std::array<uint8_t, 2> header{};
		auto body = std::make_shared<std::vector<uint8_t>>();
		bool done = false;
		std::error_code result;
		asio::async_read(socket, asio::buffer(header), [&](const std::error_code& error, size_t) {
			if (error) {
				result = error;
				done = true;
				return;
			}
			size_t size = static_cast<size_t>(header[0] | (header[1] << 8));
			body->resize(size >= 2 ? size - 2 : 0);
			asio::async_read(socket, asio::buffer(*body), [&](const std::error_code& bodyError, size_t) {
				result = bodyError;
				done = true;
			});
		});
		if (!runFor(timeout, done))
			return std::nullopt;
		if (result)
			return std::nullopt;
		return *body;
	}

	/** Reads and discards data until the server closes the connection. @return true if it was closed within the timeout */
	bool waitForClose(std::chrono::milliseconds timeout = 5s) {
		std::array<uint8_t, 4096> buffer{};
		bool done = false;
		std::function<void()> readMore = [&] {
			socket.async_read_some(asio::buffer(buffer), [&](const std::error_code& error, size_t) {
				if (error)
					done = true;
				else
					readMore();
			});
		};
		readMore();
		return runFor(timeout, done);
	}

	void close() {
		std::error_code ignored;
		socket.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
		socket.close(ignored);
	}

	asio::ip::tcp::socket& getSocket() { return socket; }

private:
	/** Runs the client's io_context until done or the timeout; cancels the pending operation on timeout. */
	bool runFor(std::chrono::milliseconds timeout, bool& done) {
		io.restart();
		auto deadline = std::chrono::steady_clock::now() + timeout;
		while (!done && std::chrono::steady_clock::now() < deadline)
			io.run_one_for(deadline - std::chrono::steady_clock::now());
		if (!done) {
			std::error_code ignored;
			socket.cancel(ignored);
			io.restart();
			io.run();
			return false;
		}
		return true;
	}

	asio::io_context io;
	asio::ip::tcp::socket socket{io};
};

} // namespace nettest
