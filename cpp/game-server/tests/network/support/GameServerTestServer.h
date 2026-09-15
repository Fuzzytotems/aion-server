#pragma once

// An in-process game client listener for the network tests: a commons NioServer on 127.0.0.1:0 creating (test-derived) AionConnections, test
// client packets registered in AionClientPacketFactory under real class names, and a recorder of the packets they ran.

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "aion/commons/network/NioServer.h"
#include "aion/gameserver/configs/main/ThreadConfig.h"
#include "aion/gameserver/configs/network/NetworkConfig.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/network/aion/AionClientPacket.h"
#include "aion/gameserver/network/aion/AionClientPacketFactory.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/StateSet.h"
#include "NetworkTestSupport.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::test {

/** A server packet with a chosen opcode and raw writeImpl data (C++ test double) */
class TestServerPacket : public aion::AionServerPacket {
public:
	TestServerPacket(int32_t opCode, std::vector<uint8_t> data, bool perRecipient = false)
		: AionServerPacket(opCode), data(std::move(data)), perRecipient(perRecipient) {}

	Recipients recipients() const noexcept override { return perRecipient ? Recipients::PER_RECIPIENT : Recipients::SHARED; }

	/** the connection passed to the last writeImpl */
	aion::AionConnection* lastConnection = nullptr;

protected:
	void writeImpl(aion::AionConnection* con) override {
		lastConnection = con;
		writeB(data);
	}

private:
	std::vector<uint8_t> data;
	bool perRecipient;
};

/** The client packets the test packets ran */
struct PacketRecorder {
	struct Record {
		int32_t opcode;
		std::vector<uint8_t> data;
	};

	std::mutex mutex;
	std::condition_variable changed;
	std::vector<Record> records;

	void add(int32_t opcode, std::vector<uint8_t> data) {
		{
			std::lock_guard lock(mutex);
			records.push_back({opcode, std::move(data)});
		}
		changed.notify_all();
	}

	size_t size() {
		std::lock_guard lock(mutex);
		return records.size();
	}

	std::vector<Record> snapshot() {
		std::lock_guard lock(mutex);
		return records;
	}

	void clear() {
		std::lock_guard lock(mutex);
		records.clear();
	}

	static PacketRecorder& instance() {
		static PacketRecorder recorder;
		return recorder;
	}
};

/**
 * A client packet that reads all its data and records it when run. Opcode 44 (CM_PING) additionally echoes the data back as a test server
 * packet with opcode 99 (the packet's connection sends it, AionClientPacket::sendPacket).
 */
class RecordingClientPacket : public aion::AionClientPacket {
public:
	static constexpr int32_t ECHO_REQUEST_OPCODE = 44;
	static constexpr int32_t ECHO_RESPONSE_OPCODE = 99;

	RecordingClientPacket(int32_t opcode, const aion::StateSet& validStates) : AionClientPacket(opcode, validStates) {}

protected:
	void readImpl() override { data = readB(getRemainingBytes()); }

	void runImpl() override {
		if (getOpCode() == ECHO_REQUEST_OPCODE)
			sendPacket(TestServerPacket(ECHO_RESPONSE_OPCODE, data));
		PacketRecorder::instance().add(getOpCode(), data);
	}

private:
	std::vector<uint8_t> data;
};

inline std::unique_ptr<aion::AionClientPacket> createRecordingPacket(int32_t opcode, const aion::StateSet& validStates) {
	return std::make_unique<RecordingClientPacket>(opcode, validStates);
}

/** AION_CLIENT_PACKET registry entries for every class name of ClientPacketInfo.gen.inc, all creating RecordingClientPackets */
inline std::span<const handlers::ClientPacketEntry> recordingPacketEntries() {
	static const std::vector<handlers::ClientPacketEntry> entries = [] {
		std::vector<handlers::ClientPacketEntry> list;
#define AION_CLIENT_PACKET_INFO(opcode, wireOpcode, Class, clientName, ...) list.push_back({#Class, &createRecordingPacket, "tests/network"});
#include "aion/gameserver/network/aion/ClientPacketInfo.gen.inc"
#undef AION_CLIENT_PACKET_INFO
		std::ranges::sort(list, {}, &handlers::ClientPacketEntry::name);
		return list;
	}();
	return entries;
}

/** An AionConnection whose initialized() can skip the key exchange (to test packets that arrive before SM_KEY) */
class TestAionConnection : public aion::AionConnection {
public:
	TestAionConnection(asio::ip::tcp::socket socket, commons::network::NioServer& server, bool sendKey)
		: AionConnection(std::move(socket), server), sendKey(sendKey) {}

	using AionConnection::getSendMsgQueue;
	using AionConnection::guard;

protected:
	void initialized() override {
		if (sendKey)
			AionConnection::initialized();
	}

private:
	const bool sendKey;
};

/** Sets the network and thread configs the connection reads (the static PacketProcessor is created on first use with them) */
inline void configureNetworkForTests() {
	using configs::network::NetworkConfig;
	NetworkConfig::PACKET_PROCESSOR_MIN_THREADS = 2;
	NetworkConfig::PACKET_PROCESSOR_MAX_THREADS = 4;
	NetworkConfig::PACKET_PROCESSOR_THREAD_SPAWN_THRESHOLD = 50;
	NetworkConfig::PACKET_PROCESSOR_THREAD_KILL_THRESHOLD = 3;
	NetworkConfig::LOG_UNKNOWN_PACKETS = true;
	NetworkConfig::LOG_IGNORED_PACKETS = true;
	configs::main::ThreadConfig::MAXIMUM_RUNTIME_IN_MILLISEC_WITHOUT_WARNING = 5000;
}

/** A NioServer accepting game clients on 127.0.0.1 (ephemeral port) */
class GameServerTestServer {
public:
	explicit GameServerTestServer(bool sendKey = true) {
		configureNetworkForTests();
		aion::AionClientPacketFactory::setEntries(recordingPacketEntries());
		commons::network::ServerCfg cfg{{"127.0.0.1", 0}, "Aion game clients",
			[this, sendKey](asio::ip::tcp::socket socket, commons::network::NioServer& nioServer) -> std::shared_ptr<commons::network::AConnectionBase> {
				auto connection = std::make_shared<TestAionConnection>(std::move(socket), nioServer, sendKey);
				{
					std::lock_guard lock(mutex);
					connections.push_back(connection);
				}
				return connection;
			}};
		server = std::make_unique<commons::network::NioServer>(1, std::vector{cfg});
		server->connect();
		port = server->getBoundAddresses().at(0).port;
	}

	~GameServerTestServer() { server->shutdown(std::chrono::seconds(2)); }

	GameServerTestServer(const GameServerTestServer&) = delete;
	GameServerTestServer& operator=(const GameServerTestServer&) = delete;

	/** @return the server side of the n-th accepted connection, waiting for it */
	std::shared_ptr<TestAionConnection> connection(size_t index = 0) {
		std::shared_ptr<TestAionConnection> result;
		waitUntil([&] {
			std::lock_guard lock(mutex);
			if (connections.size() <= index)
				return false;
			result = connections[index];
			return true;
		});
		return result;
	}

	uint16_t port = 0;
	std::unique_ptr<commons::network::NioServer> server;

private:
	std::mutex mutex;
	std::vector<std::shared_ptr<TestAionConnection>> connections;
};

} // namespace aion::gameserver::network::test
