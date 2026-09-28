#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "ServerTestUtils.h"
#include "aion/loginserver/GameServerTable.h"
#include "aion/loginserver/PingPongTask.h"
#include "aion/loginserver/configs/Config.h"
#include "aion/loginserver/controller/BannedHDDController.h"
#include "aion/loginserver/controller/BannedIpController.h"
#include "aion/loginserver/controller/BannedMacManager.h"
#include "aion/loginserver/network/NetConnector.h"
#include "aion/loginserver/network/ncrypt/KeyGen.h"
#include "support/LoginServerTestDatabase.h"

namespace aion::loginserver::test {

/**
 * Starts the login server components in-process for each test, like LoginServer::startup: Config values set programmatically, DatabaseFactory
 * on the test database (schema created once per process, with the game servers #1 (mask 127.0.0.1, password gs1pass) and #2 (mask *, password
 * gs2pass); the process then holds the database lock of database::lockForProcess until it exits), KeyGen, GameServerTable, BannedIpController, the MAC/HDD ban lists and NetConnector on ephemeral ports.
 * <p>
 * The controllers keep static state across tests (like in the Java server), so tests use unique account names and client IP addresses.
 */
class LoginServerTestFixture : public ::testing::Test {
protected:
	static constexpr const char* GS1_PASSWORD = "gs1pass";
	static constexpr const char* GS2_PASSWORD = "gs2pass";

	static void SetUpTestSuite() {
		if (!database::isEnabled())
			return;
		database::initDatabaseFactory();
		static bool schemaCreated = false;
		if (!schemaCreated) {
			database::recreateSchema();
			database::execute("INSERT INTO gameservers (id, mask, password) VALUES (1, '127.0.0.1', 'gs1pass'), (2, '*', 'gs2pass')");
			schemaCreated = true;
		}
		network::ncrypt::KeyGen::init();
	}

	void SetUp() override {
		if (!database::isEnabled())
			GTEST_SKIP() << "AION_TEST_LS_DATABASE_URL is not set";
		using configs::Config;
		Config::CLIENT_SOCKET_ADDRESS = {"127.0.0.1", 0};
		Config::GAMESERVER_SOCKET_ADDRESS = {"127.0.0.1", 0};
		Config::NIO_READ_WRITE_THREADS = 2;
		Config::ACCOUNT_AUTO_CREATION = true;
		Config::EXTERNAL_AUTH_URL.clear();
		Config::ENABLE_BRUTEFORCE_PROTECTION = true;
		Config::LOGIN_TRY_BEFORE_BAN = 5;
		Config::WRONG_LOGIN_BAN_TIME = 15;
		Config::LOG_LOGINS = false;
		PingPongTask::PERIOD = std::chrono::seconds(5);

		GameServerTable::load();
		controller::BannedIpController::start();
		controller::BannedMacManager::getInstance().reload();
		controller::BannedHDDController::getInstance().reload();
		network::NetConnector::connect();
		auto addresses = network::NetConnector::getBoundAddresses();
		ASSERT_EQ(addresses.size(), 2u);
		clientPort = addresses[0].port;
		gsPort = addresses[1].port;
	}

	/**
	 * Restarts the network (new ports, new packet threads). Tests that change Config values call it afterwards, so no server thread reads a field
	 * while the test thread writes it.
	 */
	void restartNetwork() {
		network::NetConnector::shutdown();
		network::NetConnector::connect();
		auto addresses = network::NetConnector::getBoundAddresses();
		ASSERT_EQ(addresses.size(), 2u);
		clientPort = addresses[0].port;
		gsPort = addresses[1].port;
	}

	void TearDown() override {
		network::NetConnector::shutdown();
		configs::Config::EXTERNAL_AUTH_URL.clear();
		configs::Config::LOG_LOGINS = false;
		PingPongTask::PERIOD = std::chrono::seconds(5);
	}

	/** @return a new account name (at most 14 characters, the limit of the normal client login layout) */
	static std::string newAccountName() {
		static std::atomic<int> counter = 0;
		return "flowacc" + std::to_string(++counter);
	}

	/** @return the id of the account with the name, -1 if it does not exist */
	static int32_t accountIdOf(const std::string& name) {
		return static_cast<int32_t>(database::queryLong("SELECT id FROM account_data WHERE name = '" + name + "'").value_or(-1));
	}

	/** @return the reason id of SM_LOGIN_FAIL / SM_PLAY_FAIL / SM_ACCOUNT_KICK (the last payload byte is overwritten by the checksum) */
	static int32_t reasonOf(const std::vector<uint8_t>& body) { return body[1] | body[2] << 8 | body[3] << 16; }

	uint16_t clientPort = 0;
	uint16_t gsPort = 0;
};

} // namespace aion::loginserver::test
