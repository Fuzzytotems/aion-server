// End-to-end tests of the Aion client login flow against the in-process login server and the test database.

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "LoginServerTestFixture.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/loginserver/controller/AccountController.h"
#include "aion/loginserver/dao/AccountTimeDAO.h"
#include "aion/loginserver/utils/AccountUtils.h"

namespace aion::loginserver::test {
namespace {

using namespace std::chrono_literals;
using configs::Config;

class ClientFlowTest : public LoginServerTestFixture {};

/** Reads an SM_SERVER_LIST entry. */
struct ServerEntry {
	int id;
	std::vector<uint8_t> ip;
	int port;
	int currentPlayers;
	int maxPlayers;
	int online;
};

struct ServerList {
	int lastServer;
	std::vector<ServerEntry> servers;
	int maxIdWithCharsPlusOne;
	std::vector<int> characterCounts;
};

ServerList parseServerList(const std::vector<uint8_t>& body) {
	PacketReader r(body);
	EXPECT_EQ(r.C(), 0x04);
	ServerList list;
	int count = r.C();
	list.lastServer = static_cast<int8_t>(r.C());
	for (int i = 0; i < count; i++) {
		ServerEntry e;
		e.id = r.C();
		e.ip = r.B(4);
		e.port = static_cast<uint16_t>(r.H());
		EXPECT_EQ(r.H(), 0);
		EXPECT_EQ(r.C(), 0);
		EXPECT_EQ(r.C(), 0);
		e.currentPlayers = r.H();
		e.maxPlayers = r.H();
		e.online = r.C();
		EXPECT_EQ(r.C(), 1); // server type
		EXPECT_EQ(r.C(), 0);
		EXPECT_EQ(r.H(), 0);
		EXPECT_EQ(r.C(), 0);
		list.servers.push_back(e);
	}
	list.maxIdWithCharsPlusOne = r.H();
	EXPECT_EQ(r.C(), 1);
	for (int i = 1; i < list.maxIdWithCharsPlusOne; i++)
		list.characterCounts.push_back(r.C());
	return list;
}

TEST_F(ClientFlowTest, AutoCreatedAccountLogsInAndGetsTheServerListWithAllServersOffline) {
	AionTestClient client(clientPort);
	auto init = client.readInit();
	EXPECT_EQ(init.protocolRevision, 0xc621);
	client.sendPacket(AionTestClient::buildCM_AUTH_GG(init.sessionId));
	PacketReader gg(client.expectPacket(0x0b));
	gg.C();
	EXPECT_EQ(gg.D(), init.sessionId);

	const std::string name = newAccountName();
	PacketReader ok(client.login(name, "secret", 0x03));
	ok.C();
	int32_t accountId = ok.D();
	int32_t loginOk = ok.D();
	EXPECT_EQ(ok.D(), 0);
	EXPECT_EQ(ok.D(), 0);
	EXPECT_EQ(ok.D(), 0x3ea);

	EXPECT_EQ(accountIdOf(name), accountId);
	EXPECT_EQ(database::queryString("SELECT password FROM account_data WHERE id = " + std::to_string(accountId)), utils::AccountUtils::encodePassword("secret"));
	EXPECT_EQ(database::queryString("SELECT last_ip FROM account_data WHERE id = " + std::to_string(accountId)), "127.0.0.1");
	EXPECT_TRUE(database::queryLong("SELECT COUNT(*) FROM account_time WHERE account_id = " + std::to_string(accountId)).value_or(0) == 1);

	client.sendPacket(AionTestClient::buildCM_SERVER_LIST(accountId, loginOk));
	ServerList list = parseServerList(client.expectPacket(0x04));
	EXPECT_EQ(list.lastServer, 0); // Java: byte field default of the new account object, which insertAccount also stores
	ASSERT_EQ(list.servers.size(), 2u);
	EXPECT_EQ(list.servers[0].id, 1);
	EXPECT_EQ(list.servers[1].id, 2);
	for (const ServerEntry& server : list.servers) {
		EXPECT_EQ(server.ip, (std::vector<uint8_t>{0, 0, 0, 0}));
		EXPECT_EQ(server.port, 0);
		EXPECT_EQ(server.online, 0);
		EXPECT_EQ(server.currentPlayers, 0);
	}
	EXPECT_EQ(list.maxIdWithCharsPlusOne, 3);
	EXPECT_EQ(list.characterCounts, (std::vector<int>{0, 0}));

	client.sendPacket(AionTestClient::buildCM_PLAY(accountId, loginOk, 1));
	EXPECT_EQ(reasonOf(client.expectPacket(0x06)), 8); // STR_L2AUTH_S_SERVER_DOWN

	// a second login of the same account loads it from the database
	client.socket.close();
	std::this_thread::sleep_for(300ms); // onDisconnect removes the account from the login server
	AionTestClient client2(clientPort);
	auto second = client2.loginOk(name, "secret");
	EXPECT_EQ(second.accountId, accountId);
	client2.sendPacket(AionTestClient::buildCM_SERVER_LIST(accountId, second.loginOk));
	EXPECT_EQ(parseServerList(client2.expectPacket(0x04)).lastServer, 0);
}

TEST_F(ClientFlowTest, WrongPasswordAndWrongSessionIds) {
	const std::string name = newAccountName();
	ASSERT_TRUE(controller::AccountController::createAccount(name, "right"));

	AionTestClient client(clientPort);
	client.authGG();
	EXPECT_EQ(reasonOf(client.login(name, "wrong", 0x01)), 3); // STR_L2AUTH_S_INCORRECT_PWD, 127.0.0.1 is never banned
	EXPECT_EQ(reasonOf(client.login(name, "wrong", 0x01)), 3);

	client.sendPacket(client.crypto.buildCM_LOGIN(name, "right", false, -1, client.crypto.getSessionId() ^ 0x100));
	EXPECT_EQ(reasonOf(client.expectPacket(0x01)), 20); // STR_L2AUTH_S_SYSTEM_ERROR, connection stays open
	EXPECT_FALSE(client.readPacket(200ms).has_value());
	EXPECT_FALSE(client.socket.isClosed());

	client.login(name, "right", 0x03);

	AionTestClient wrongGG(clientPort);
	auto init = wrongGG.readInit();
	wrongGG.sendPacket(AionTestClient::buildCM_AUTH_GG(init.sessionId + 1));
	EXPECT_EQ(reasonOf(wrongGG.expectPacket(0x01)), 20);
	EXPECT_TRUE(wrongGG.socket.waitClosed());
}

TEST_F(ClientFlowTest, LoginExLayoutWithLongNameAndPassword) {
	const std::string name = newAccountName() + "_loginex_long"; // longer than the 14 characters of the normal layout
	const std::string password = "a-32-character-password-12345678";
	AionTestClient client(clientPort);
	client.authGG();
	client.sendPacket(client.crypto.buildCM_LOGIN(name, password, true));
	PacketReader ok(client.expectPacket(0x03));
	ok.C();
	EXPECT_EQ(ok.D(), accountIdOf(name));
	EXPECT_EQ(database::queryString("SELECT password FROM account_data WHERE name = '" + name + "'"), utils::AccountUtils::encodePassword(password));
}

TEST_F(ClientFlowTest, InactiveExpiredAndIpForcedAccounts) {
	const std::string inactive = newAccountName(), expired = newAccountName(), ipForced = newAccountName();
	ASSERT_TRUE(controller::AccountController::createAccount(inactive, "pw"));
	ASSERT_TRUE(controller::AccountController::createAccount(expired, "pw"));
	ASSERT_TRUE(controller::AccountController::createAccount(ipForced, "pw"));
	database::execute("UPDATE account_data SET activated = 0 WHERE name = '" + inactive + "'");
	database::execute("UPDATE account_data SET ip_force = '10.0.0.*' WHERE name = '" + ipForced + "'");
	// the account time rows are created by the first login
	for (const std::string& name : {expired, ipForced}) {
		model::AccountTime time;
		if (name == expired)
			time.setExpirationTime(model::currentTimestamp() - std::chrono::hours(1));
		ASSERT_TRUE(dao::AccountTimeDAO::updateAccountTime(accountIdOf(name), time));
	}
	ASSERT_TRUE(dao::AccountTimeDAO::updateAccountTime(accountIdOf(inactive), model::AccountTime()));

	AionTestClient client(clientPort);
	client.authGG();
	EXPECT_EQ(reasonOf(client.login(inactive, "pw", 0x01)), 37); // STR_L2AUTH_S_AGREE_GAME
	EXPECT_EQ(reasonOf(client.login(expired, "pw", 0x01)), 34); // STR_L2AUTH_S_TIME_EXHAUSTED
	EXPECT_EQ(reasonOf(client.login(ipForced, "pw", 0x01)), 22); // STR_L2AUTH_S_BLOCKED_IP
	EXPECT_EQ(reasonOf(client.login("", "pw", 0x01)), 4); // STR_L2AUTH_S_ACCOUNT_LOAD_FAIL: empty names are not created

	Config::ACCOUNT_AUTO_CREATION = false;
	restartNetwork();
	AionTestClient noAutoCreation(clientPort);
	noAutoCreation.authGG();
	EXPECT_EQ(reasonOf(noAutoCreation.login(newAccountName(), "pw", 0x01)), 4);
}

TEST_F(ClientFlowTest, BannedIpCannotLogIn) {
	const std::string ip = "127.0.0.5";
	ASSERT_TRUE(controller::BannedIpController::banIp(ip));
	EXPECT_FALSE(controller::BannedIpController::banIp(ip)); // already banned
	EXPECT_EQ(database::queryLong("SELECT COUNT(*) FROM banned_ip WHERE mask = '127.0.0.5'"), 1);
	{
		AionTestClient client(clientPort, ip);
		client.authGG();
		const std::string name = newAccountName();
		EXPECT_EQ(reasonOf(client.login(name, "pw", 0x01)), 22); // STR_L2AUTH_S_BLOCKED_IP
		EXPECT_EQ(accountIdOf(name), -1);
	}
	ASSERT_TRUE(controller::BannedIpController::unbanIp(ip));
	EXPECT_FALSE(controller::BannedIpController::unbanIp(ip));
	EXPECT_EQ(database::queryLong("SELECT COUNT(*) FROM banned_ip WHERE mask = '127.0.0.5'"), 0);
	AionTestClient client(clientPort, ip);
	client.loginOk(newAccountName(), "pw");
}

TEST_F(ClientFlowTest, BruteForceProtectionBansTheIp) {
	LogCapture audit("com.aionemu.loginserver.network.aion.clientpackets.CM_LOGIN");
	Config::LOGIN_TRY_BEFORE_BAN = 2;
	Config::WRONG_LOGIN_BAN_TIME = 15;
	restartNetwork();
	const std::string ip = "127.0.0.6";
	const std::string name = newAccountName();
	ASSERT_TRUE(controller::AccountController::createAccount(name, "right"));
	int64_t before = commons::utils::currentTimeMillis();
	{
		AionTestClient client(clientPort, ip);
		client.authGG();
		EXPECT_EQ(reasonOf(client.login(name, "wrong1", 0x01)), 3); // count 1
		EXPECT_EQ(reasonOf(client.login(name, "wrong2", 0x01)), 3); // count 2
		EXPECT_EQ(reasonOf(client.login(name, "wrong3", 0x01)), 22); // banned and closed
		EXPECT_TRUE(client.socket.waitClosed());
	}
	EXPECT_TRUE(audit.waitFor(name + " on 127.0.0.6 banned for 15 min. bruteforce")) << audit.dump();
	auto timeEnd = database::queryLong("SELECT UNIX_TIMESTAMP(time_end) FROM banned_ip WHERE mask = '127.0.0.6'");
	ASSERT_TRUE(timeEnd.has_value());
	EXPECT_NEAR(static_cast<double>(*timeEnd), (before + 15 * 60000) / 1000.0, 5.0);
	{
		AionTestClient client(clientPort, ip);
		client.authGG();
		EXPECT_EQ(reasonOf(client.login(name, "right", 0x01)), 22);
	}
	controller::BannedIpController::unbanIp(ip);
}

TEST_F(ClientFlowTest, DoubleLoginOnTheLoginServerKicksTheFirstClient) {
	const std::string name = newAccountName();
	AionTestClient first(clientPort);
	auto login = first.loginOk(name, "pw");

	AionTestClient second(clientPort);
	second.authGG();
	EXPECT_EQ(reasonOf(second.login(name, "pw", 0x01)), 7); // STR_L2AUTH_S_ALREADY_LOGIN
	EXPECT_EQ(reasonOf(first.expectPacket(0x08)), 13); // SM_ACCOUNT_KICK(STR_L2AUTH_S_KICKED_DOUBLE_LOGIN)
	EXPECT_TRUE(first.socket.waitClosed());

	PacketReader secondLogin(second.login(name, "pw", 0x03));
	secondLogin.C();
	EXPECT_EQ(secondLogin.D(), login.accountId);
	int32_t loginOk = secondLogin.D();
	std::this_thread::sleep_for(300ms); // the first connection's onDisconnect must not remove the second one (Java would)
	second.sendPacket(AionTestClient::buildCM_SERVER_LIST(login.accountId, loginOk));
	EXPECT_EQ(parseServerList(second.expectPacket(0x04)).servers.size(), 2u);
}

TEST_F(ClientFlowTest, ClientDisconnectingWhileCmLoginRunsIsLoggedOutWhenTheLoginFinishes) {
	const std::string name = newAccountName();
	ASSERT_TRUE(controller::AccountController::createAccount(name, "pw"));
	const int32_t accountId = accountIdOf(name);
	ASSERT_TRUE(dao::AccountTimeDAO::updateAccountTime(accountId, model::AccountTime()));
	{
		// blocks the account query of CM_LOGIN until the client disconnected and onDisconnect ran
		auto lockConnection = commons::database::DatabaseFactory::getConnection();
		lockConnection->executeSimple("LOCK TABLES account_data WRITE");
		struct Unlock {
			commons::database::PooledConnection& con;
			~Unlock() {
				try {
					con->executeSimple("UNLOCK TABLES");
				} catch (...) {
				}
			}
		} unlock{lockConnection};

		AionTestClient client(clientPort);
		client.authGG();
		client.sendPacket(client.crypto.buildCM_LOGIN(name, "pw"));
		std::this_thread::sleep_for(300ms); // the packet is waiting for the table lock
		client.socket.close();
		std::this_thread::sleep_for(300ms); // onDisconnect runs while the login is still in progress
	}
	// the login finishes after the disconnect (it updates the last IP at its end) and must not leave the closed connection logged in
	ASSERT_TRUE(waitUntil([&] { return database::queryString("SELECT last_ip FROM account_data WHERE id = " + std::to_string(accountId)) == "127.0.0.1"; }));
	EXPECT_TRUE(waitUntil([&] { return controller::AccountController::getAccountOnLS(accountId) == nullptr; }, 2s));

	AionTestClient second(clientPort);
	second.authGG();
	second.login(name, "pw", 0x03); // the first attempt succeeds (no STR_L2AUTH_S_ALREADY_LOGIN)
}

TEST_F(ClientFlowTest, PenaltyBannedAccountGetsSmAccountBanned2) {
	const std::string name = newAccountName();
	ASSERT_TRUE(controller::AccountController::createAccount(name, "pw"));
	model::AccountTime time;
	time.setPenaltyEnd(model::Timestamp(std::chrono::milliseconds(1000))); // infinite
	ASSERT_TRUE(dao::AccountTimeDAO::updateAccountTime(accountIdOf(name), time));
	AionTestClient client(clientPort);
	client.authGG();
	client.login(name, "pw", 0x09);
	EXPECT_TRUE(client.socket.waitClosed());
}

TEST_F(ClientFlowTest, ExternalAuthentication) {
	// a minimal HTTP server answering each request with the next configured response
	struct HttpResponse {
		int status;
		std::string body;
	};
	std::vector<HttpResponse> responses = {{200, R"({"accountId":"extuser1","aionAuthResponseId":0})"}, {200, R"({"accountId":"x","aionAuthResponseId":3})"},
		{500, "oops"}, {200, R"({"aionAuthResponseId":999})"}, {200, "not json"}};
	std::vector<std::string> requests;
	asio::io_context io;
	asio::ip::tcp::acceptor acceptor(io, asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
	uint16_t httpPort = acceptor.local_endpoint().port();
	std::thread httpThread([&] {
		for (const HttpResponse& response : responses) {
			std::error_code ec;
			asio::ip::tcp::socket socket(io);
			acceptor.accept(socket, ec);
			if (ec)
				return;
			std::string request;
			std::array<char, 4096> buffer;
			size_t headerEnd = std::string::npos;
			size_t contentLength = 0;
			while (true) {
				size_t n = socket.read_some(asio::buffer(buffer), ec);
				if (ec)
					break;
				request.append(buffer.data(), n);
				if (headerEnd == std::string::npos && (headerEnd = request.find("\r\n\r\n")) != std::string::npos) {
					std::string lower = commons::utils::StringUtils::toLowerCase(request.substr(0, headerEnd));
					size_t pos = lower.find("content-length:");
					if (pos != std::string::npos)
						contentLength = std::stoul(lower.substr(pos + 15));
				}
				if (headerEnd != std::string::npos && request.size() >= headerEnd + 4 + contentLength)
					break;
			}
			requests.push_back(request);
			std::string reply = "HTTP/1.1 " + std::to_string(response.status) + " X\r\nContent-Type: application/json\r\nContent-Length: " +
				std::to_string(response.body.size()) + "\r\nConnection: close\r\n\r\n" + response.body;
			asio::write(socket, asio::buffer(reply), ec);
			socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
		}
	});

	LogCapture logs("com.aionemu.loginserver.utils.ExternalAuth");
	Config::EXTERNAL_AUTH_URL = "http://127.0.0.1:" + std::to_string(httpPort) + "/auth";
	restartNetwork();
	{
		AionTestClient client(clientPort);
		client.authGG();
		PacketReader ok(client.login("gameName", "extPassword", 0x03));
		ok.C();
		int32_t accountId = ok.D();
		EXPECT_EQ(database::queryLong("SELECT id FROM account_data WHERE ext_auth_name = 'extuser1'"), accountId);
		EXPECT_EQ(database::queryString("SELECT password FROM account_data WHERE id = " + std::to_string(accountId)), "");
	}
	{
		AionTestClient client(clientPort);
		client.authGG();
		EXPECT_EQ(reasonOf(client.login("u", "p", 0x01)), 3);
		EXPECT_EQ(reasonOf(client.login("u", "p", 0x01)), 62); // STR_L2AUTH_S_ACCOUNTCACHESERVER_DOWN
		EXPECT_EQ(reasonOf(client.login("u", "p", 0x01)), 60); // unknown id: STR_L2AUTH_UNKNOWN4
		EXPECT_EQ(reasonOf(client.login("u", "p", 0x01)), 62); // invalid JSON
	}
	httpThread.join(); // TearDown clears the URL after stopping the network

	ASSERT_EQ(requests.size(), 5u);
	EXPECT_NE(requests[0].find("POST /auth HTTP/1.1"), std::string::npos) << requests[0];
	EXPECT_NE(requests[0].find("User-Agent: AionLS"), std::string::npos) << requests[0];
	EXPECT_NE(requests[0].find("Content-Type: application/json"), std::string::npos) << requests[0];
	EXPECT_NE(requests[0].find(R"({"password":"extPassword","user":"gameName"})"), std::string::npos) << requests[0];
	EXPECT_TRUE(logs.contains("Server returned status code 500: oops")) << logs.dump();
	EXPECT_TRUE(logs.contains("Could not login user u")) << logs.dump();
}

TEST_F(ClientFlowTest, UnknownPacketsInEachState) {
	LogCapture logs("com.aionemu.loginserver.network.factories.AionPacketHandlerFactory");
	AionTestClient client(clientPort);
	client.authGG();
	client.sendPacket(std::vector<uint8_t>{0x07});
	EXPECT_TRUE(logs.waitFor("opCode=0x07 state=AUTHED_GG")) << logs.dump();
	client.login(newAccountName(), "pw", 0x03);
	client.sendPacket(std::vector<uint8_t>{0x00});
	EXPECT_TRUE(logs.waitFor("opCode=0x00 state=AUTHED_LOGIN")) << logs.dump();
	EXPECT_FALSE(client.socket.isClosed());
}

} // namespace
} // namespace aion::loginserver::test
