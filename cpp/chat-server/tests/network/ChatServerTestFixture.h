#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "aion/chatserver/configs/main/LoggingConfig.h"
#include "aion/chatserver/configs/network/NetworkConfig.h"
#include "aion/chatserver/network/netty/NettyServer.h"
#include "support/FakePeers.h"
#include "support/TestUtils.h"

namespace aion::chatserver::test {

/**
 * Starts the chat server's network in-process for each test, like ChatServer::startup does (without database and logging setup): NetworkConfig
 * set programmatically (both servers on 127.0.0.1 with ports chosen by the OS, the game server password "gspass", the connect address
 * 127.0.0.1:10241 that SM_GS_AUTH_RESPONSE announces), then a NettyServer. TearDown shuts it down, which disconnects every game server, so each
 * test can register its own.
 * <p>
 * The services keep static state across tests (like in the Java server), so tests use unique player ids (nextPlayerId) and channel names
 * (uniqueName).
 */
class ChatServerTestFixture : public ::testing::Test {
protected:
	static constexpr const char* GS_PASSWORD = "gspass";
	static constexpr int32_t GS_ID = 1;
	static constexpr uint16_t CONNECT_PORT = 10241;

	void SetUp() override {
		using configs::network::NetworkConfig;
		using configs::main::LoggingConfig;
		NetworkConfig::CLIENT_SOCKET_ADDRESS = {"127.0.0.1", 0};
		NetworkConfig::GAMESERVER_SOCKET_ADDRESS = {"127.0.0.1", 0};
		NetworkConfig::CLIENT_CONNECT_ADDRESS = {"127.0.0.1", CONNECT_PORT};
		NetworkConfig::GAMESERVER_PASSWORD = GS_PASSWORD;
		NetworkConfig::NIO_READ_WRITE_THREADS = 1;
		LoggingConfig::LOG_CHANNEL_REQUEST = false;
		LoggingConfig::LOG_CHANNEL_INVALID = false;
		LoggingConfig::LOG_CHAT = false;
		LoggingConfig::LOG_CHAT_TO_DB = false;
		startServer();
	}

	void TearDown() override { server.reset(); }

	/** (Re)starts the network. Tests that change a config value restart it afterwards, so no server thread reads a field while it is written. */
	void startServer() {
		server.reset();
		server = std::make_unique<network::netty::NettyServer>();
		auto addresses = server->getBoundAddresses();
		ASSERT_EQ(addresses.size(), 2u);
		clientPort = addresses[0].port;
		gsPort = addresses[1].port;
	}

	/** @return a fake game server that authenticated with the right password (checking the AUTHED response bytes) */
	std::unique_ptr<FakeGameServer> connectGameServer() {
		auto gs = std::make_unique<FakeGameServer>(gsPort);
		Bytes response = gs->authenticate(GS_ID, GS_PASSWORD);
		// Java SM_GS_AUTH_RESPONSE: size 11, opcode 0, AUTHED 0, C 4, 127.0.0.1, H 10241
		EXPECT_EQ(response, (Bytes{0x0B, 0x00, 0x00, 0x00, 0x04, 0x7F, 0x00, 0x00, 0x01, 0x01, 0x28})) << hex(response);
		return gs;
	}

	/** A registered player whose client logged in */
	struct Player {
		int32_t id;
		std::string name;
		std::string nameIdentifier;
		std::unique_ptr<FakeChatClient> client;
	};

	/** Registers the player at the game server and logs its client in. raceId 0 = ELYOS, 1 = ASMODIANS */
	Player loginPlayer(FakeGameServer& gs, std::string_view name, int32_t raceId, int32_t accessLevel = 0) {
		Player player{nextPlayerId(), std::string(name), std::string(name) + "@AION", nullptr};
		std::string account = "acc" + std::to_string(player.id);
		Bytes token = gs.registerPlayer(player.id, account, player.name, raceId, accessLevel);
		player.client = std::make_unique<FakeChatClient>(clientPort);
		player.client->login(player.id, player.nameIdentifier, account, token);
		return player;
	}

	/** @return a player id no other test used */
	static int32_t nextPlayerId() {
		static std::atomic<int32_t> nextId = 1000000;
		return nextId++;
	}

	/** @return prefix + a number no other test used, for channel metas */
	static std::string uniqueName(std::string_view prefix) {
		static std::atomic<int32_t> next = 1;
		return std::string(prefix) + std::to_string(next++);
	}

	std::unique_ptr<network::netty::NettyServer> server;
	uint16_t clientPort = 0;
	uint16_t gsPort = 0;
};

} // namespace aion::chatserver::test
