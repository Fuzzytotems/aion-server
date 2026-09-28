// End-to-end tests of the game server protocol (together with Aion clients) against the in-process login server and the test database.

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "LoginServerTestFixture.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/loginserver/GameServerInfo.h"
#include "aion/loginserver/controller/AccountController.h"
#include "aion/loginserver/dao/AccountTimeDAO.h"
#include "aion/loginserver/service/PlayerTransferService.h"

namespace aion::loginserver::test {
namespace {

using namespace std::chrono_literals;
using configs::Config;

/**
 * Holds "LOCK TABLES &lt;table&gt; WRITE" on a connection outside the DatabaseFactory pool while it exists, so server queries on the table block
 * (e.g. to keep a game server packet running).
 */
class TableLock {
public:
	explicit TableLock(const std::string& table) : con(database::openConnection()) { con->executeSimple("LOCK TABLES " + table + " WRITE"); }

	/** @return true once a query of another connection is waiting for a table lock */
	static bool waitForBlockedQuery() {
		return waitUntil([] {
			return database::queryLong("SELECT COUNT(*) FROM information_schema.PROCESSLIST WHERE STATE LIKE 'Waiting for table%'").value_or(0) > 0;
		});
	}

private:
	std::unique_ptr<commons::database::Connection> con; // closing it releases the lock
};

class GameServerFlowTest : public LoginServerTestFixture {
protected:
	struct Joined {
		int32_t accountId = 0;
		int32_t loginOk = 0;
		int32_t playOk1 = 0;
		int32_t playOk2 = 0;
		std::vector<uint8_t> authResponse;
	};

	/** Logs the client in, plays on the game server and lets the game server authenticate the account. */
	Joined joinGameServer(AionTestClient& client, GsTestClient& gs, const std::string& name, int8_t serverId) {
		Joined joined;
		auto login = client.loginOk(name, "pw");
		joined.accountId = login.accountId;
		joined.loginOk = login.loginOk;
		client.sendPacket(AionTestClient::buildCM_PLAY(login.accountId, login.loginOk, serverId));
		PacketReader play(client.expectPacket(0x07));
		play.C();
		joined.playOk1 = play.D();
		joined.playOk2 = play.D();
		EXPECT_EQ(play.C(), serverId);
		EXPECT_EQ(play.B(14), std::vector<uint8_t>(14));
		gs.send(PacketWriter().C(1).D(joined.accountId).D(joined.loginOk).D(joined.playOk1).D(joined.playOk2).data);
		joined.authResponse = gs.expectPacket(1);
		return joined;
	}

	/** Creates an account that has an account time row (like after its first login). @return its id */
	static int32_t createAccountWithTime(const std::string& name) {
		auto account = controller::AccountController::createAccount(name, "pw");
		EXPECT_TRUE(account);
		int32_t id = account->getId().value();
		EXPECT_TRUE(dao::AccountTimeDAO::updateAccountTime(id, model::AccountTime()));
		return id;
	}
};

TEST_F(GameServerFlowTest, GameServerAuthentication) {
	LogCapture logs({"com.aionemu.loginserver.GameServerTable", "com.aionemu.loginserver.network.gameserver.clientpackets.CM_GS_AUTH",
		"com.aionemu.loginserver.network.gameserver.GsConnection", "com.aionemu.loginserver.network.factories.GsPacketHandlerFactory"});
	{
		GsTestClient gs(gsPort);
		gs.send(GsTestClient::buildCM_GS_AUTH(50, GS1_PASSWORD));
		EXPECT_EQ(gs.readPacket(), PacketWriter().C(0).C(1).data); // NOT_AUTHED
		EXPECT_TRUE(gs.socket.waitClosed());
		EXPECT_TRUE(logs.waitFor("Gameserver 127.0.0.1 requestedID: 50 is not registered in LS database!")) << logs.dump();
	}
	{
		GsTestClient gs(gsPort);
		gs.send(GsTestClient::buildCM_GS_AUTH(1, "wrong"));
		EXPECT_EQ(gs.readPacket(), PacketWriter().C(0).C(1).data);
		EXPECT_TRUE(gs.socket.waitClosed());
		EXPECT_TRUE(logs.waitFor("Gameserver 127.0.0.1 requested ID: 1 has wrong IP or password!")) << logs.dump();
	}
	{
		GsTestClient gs(gsPort, "127.0.0.7"); // right password, but the mask of #1 is 127.0.0.1
		gs.send(GsTestClient::buildCM_GS_AUTH(1, GS1_PASSWORD));
		EXPECT_EQ(gs.readPacket(), PacketWriter().C(0).C(1).data);
		EXPECT_TRUE(gs.socket.waitClosed());
	}
	{
		GsTestClient gs(gsPort);
		gs.send(GsTestClient::buildCM_GS_AUTH(1, GS1_PASSWORD, {192, 168, 0, 10}, 7778, 1, 3000));
		EXPECT_EQ(gs.readPacket(), PacketWriter().C(0).C(0).C(2).data); // AUTHED, 2 registered game servers
		EXPECT_TRUE(logs.waitFor("Gameserver #1 is now online")) << logs.dump();
		auto gsi = GameServerTable::getGameServerInfo(1);
		ASSERT_TRUE(gsi);
		EXPECT_TRUE(waitUntil([&] { return gsi->isOnline(); }));
		EXPECT_EQ(gsi->getIp(), (std::vector<uint8_t>{192, 168, 0, 10}));
		EXPECT_EQ(gsi->getPort(), 7778);
		EXPECT_EQ(gsi->getMinAccessLevel(), 1);
		EXPECT_EQ(gsi->getMaxPlayers(), 3000);

		GsTestClient second(gsPort);
		second.send(GsTestClient::buildCM_GS_AUTH(1, GS1_PASSWORD));
		EXPECT_EQ(second.readPacket(), PacketWriter().C(0).C(2).data); // ALREADY_REGISTERED
		EXPECT_TRUE(second.socket.waitClosed());

		// CM_GS_AUTH is unknown once authenticated
		gs.send(GsTestClient::buildCM_GS_AUTH(1, GS1_PASSWORD));
		EXPECT_TRUE(logs.waitFor("Unknown packet received from Game Server: 0x00 state=AUTHED")) << logs.dump();

		gs.socket.close();
		EXPECT_TRUE(logs.waitFor("Gameserver #1 127.0.0.1 disconnected")) << logs.dump();
		EXPECT_TRUE(waitUntil([&] { return !gsi->isOnline() && !gsi->getConnection(); }));
	}
	// the id is free again, and #2 accepts any address
	GsTestClient again(gsPort);
	again.auth(1, GS1_PASSWORD);
	GsTestClient gs2(gsPort, "127.0.0.9");
	gs2.auth(2, GS2_PASSWORD);
}

TEST_F(GameServerFlowTest, PlayFlowWithCharacterCountsAccountAuthAndDisconnect) {
	Config::LOG_LOGINS = true;
	restartNetwork();
	GsTestClient gs(gsPort);
	gs.auth(1, GS1_PASSWORD, 100);

	const std::string name = newAccountName();
	AionTestClient client(clientPort);
	auto login = client.loginOk(name, "pw");

	client.sendPacket(AionTestClient::buildCM_SERVER_LIST(login.accountId, login.loginOk));
	EXPECT_EQ(gs.expectPacket(8), PacketWriter().C(8).D(login.accountId).data); // SM_GS_CHARACTER_RESPONSE
	EXPECT_FALSE(client.readPacket(300ms).has_value()); // waits for the character count of #1
	gs.send(PacketWriter().C(8).D(login.accountId).C(3).data);

	PacketReader list(client.expectPacket(0x04));
	list.C();
	EXPECT_EQ(list.C(), 2);
	list.C(); // last server
	// #1: online, registered address and limits
	EXPECT_EQ(list.C(), 1);
	EXPECT_EQ(list.B(4), (std::vector<uint8_t>{127, 0, 0, 1}));
	EXPECT_EQ(list.H(), 7777);
	list.B(4);
	EXPECT_EQ(list.H(), 0); // current players
	EXPECT_EQ(list.H(), 100);
	EXPECT_EQ(list.C(), 1); // online
	list.B(5);
	// #2: offline
	EXPECT_EQ(list.C(), 2);
	list.B(14);
	EXPECT_EQ(list.C(), 0);
	list.B(5);
	EXPECT_EQ(list.H(), 3);
	EXPECT_EQ(list.C(), 1);
	EXPECT_EQ(list.C(), 3); // characters on #1
	EXPECT_EQ(list.C(), 0); // characters on #2

	client.sendPacket(AionTestClient::buildCM_PLAY(login.accountId, login.loginOk, 1));
	PacketReader play(client.expectPacket(0x07));
	play.C();
	int32_t playOk1 = play.D();
	int32_t playOk2 = play.D();
	EXPECT_EQ(play.C(), 1);

	int64_t creationDateMillis = database::queryLong("SELECT UNIX_TIMESTAMP(creation_date) FROM account_data WHERE id = " + std::to_string(login.accountId)).value() * 1000;
	gs.send(PacketWriter().C(1).D(login.accountId).D(login.loginOk).D(playOk1).D(playOk2).data);
	PacketReader auth(gs.expectPacket(1));
	auth.C();
	EXPECT_EQ(auth.D(), login.accountId);
	EXPECT_EQ(auth.C(), 1);
	EXPECT_EQ(auth.S(), name);
	EXPECT_NEAR(static_cast<double>(auth.Q()), static_cast<double>(creationDateMillis), 2000.0);
	EXPECT_EQ(auth.Q(), 0); // accumulated online time
	int64_t restTime = auth.Q();
	EXPECT_GE(restTime, 0);
	EXPECT_LT(restTime, 60000);
	EXPECT_EQ(auth.C(), 0); // access level
	EXPECT_EQ(auth.C(), 0); // membership
	EXPECT_EQ(auth.S(), ""); // allowed HDD serial
	EXPECT_EQ(auth.remaining(), 0u);

	auto gsi = GameServerTable::getGameServerInfo(1);
	EXPECT_TRUE(gsi->isAccountOnGameServer(login.accountId));
	EXPECT_EQ(gsi->getCurrentPlayers(), 1);
	EXPECT_TRUE(waitUntil([&] { return database::queryLong("SELECT last_server FROM account_data WHERE id = " + std::to_string(login.accountId)) == 1; }));

	// the client leaves the login server; the key was used, a second check fails
	client.socket.close();
	gs.send(PacketWriter().C(1).D(login.accountId).D(login.loginOk).D(playOk1).D(playOk2).data);
	EXPECT_EQ(gs.expectPacket(1), PacketWriter().C(1).D(login.accountId).C(0).data);

	gs.send(PacketWriter().C(7).D(login.accountId).Q(1757700000123).S("10.1.1.1").S("aa-bb-cc-dd-ee-ff").S("SERIAL-1").data);
	EXPECT_TRUE(waitUntil([&] {
		return database::queryString("SELECT last_hdd_serial FROM account_data WHERE id = " + std::to_string(login.accountId)) == "SERIAL-1";
	}));
	EXPECT_EQ(database::queryString("SELECT last_mac FROM account_data WHERE id = " + std::to_string(login.accountId)), "aa-bb-cc-dd-ee-ff");
	EXPECT_TRUE(waitUntil([&] {
		return database::queryString("SELECT ip FROM account_login_history WHERE gameserver_id = 1 AND account_id = " + std::to_string(login.accountId)) ==
			"10.1.1.1";
	}));

	// double login while playing: the game server is asked to kick the account
	{
		AionTestClient second(clientPort);
		second.authGG();
		EXPECT_EQ(reasonOf(second.login(name, "pw", 0x01)), 7);
		EXPECT_EQ(gs.expectPacket(2), PacketWriter().C(2).D(login.accountId).C(1).data);
	}

	// the account leaves the game server
	gs.send(PacketWriter().C(3).D(login.accountId).data);
	EXPECT_TRUE(waitUntil([&] { return !gsi->isAccountOnGameServer(login.accountId); }));
	AionTestClient third(clientPort);
	third.loginOk(name, "pw");
}

TEST_F(GameServerFlowTest, PlayFailsForFullOrRestrictedServers) {
	GsTestClient gs1(gsPort);
	gs1.send(GsTestClient::buildCM_GS_AUTH(1, GS1_PASSWORD, {127, 0, 0, 1}, 7777, 5, 100)); // min access level 5
	EXPECT_EQ(gs1.expectPacket(0)[1], 0);
	GsTestClient gs2(gsPort);
	gs2.auth(2, GS2_PASSWORD, 0); // full

	AionTestClient client(clientPort);
	auto login = client.loginOk(newAccountName(), "pw");
	client.sendPacket(AionTestClient::buildCM_PLAY(login.accountId, login.loginOk, 1));
	EXPECT_EQ(reasonOf(client.expectPacket(0x06)), 16); // STR_L2AUTH_S_SEVER_CHECK
	client.sendPacket(AionTestClient::buildCM_PLAY(login.accountId, login.loginOk, 2));
	EXPECT_EQ(reasonOf(client.expectPacket(0x06)), 15); // STR_L2AUTH_S_LIMIT_EXCEED
}

TEST_F(GameServerFlowTest, FastReconnectWithCmUpdateSession) {
	LogCapture logs("com.aionemu.loginserver.network.gameserver.clientpackets.CM_ACCOUNT_RECONNECT_KEY");
	GsTestClient gs(gsPort);
	gs.auth(1, GS1_PASSWORD);
	const std::string name = newAccountName();
	AionTestClient client(clientPort);
	Joined joined = joinGameServer(client, gs, name, 1);
	client.socket.close();

	gs.send(PacketWriter().C(2).D(joined.accountId).data);
	PacketReader key(gs.expectPacket(3));
	key.C();
	EXPECT_EQ(key.D(), joined.accountId);
	int32_t reconnectKey = key.D();
	EXPECT_FALSE(GameServerTable::getGameServerInfo(1)->isAccountOnGameServer(joined.accountId));

	{
		// wrong key: closed, and the reconnecting account is consumed
		AionTestClient wrong(clientPort);
		wrong.readInit();
		wrong.sendPacket(AionTestClient::buildCM_UPDATE_SESSION(joined.accountId, joined.loginOk, reconnectKey + 1));
		EXPECT_TRUE(wrong.socket.waitClosed());
	}
	gs.send(PacketWriter().C(2).D(joined.accountId).data); // not on the game server anymore
	PacketReader key2(gs.expectPacket(3));
	key2.C();
	EXPECT_EQ(key2.D(), joined.accountId);
	EXPECT_TRUE(logs.waitFor("Gameserver #1 127.0.0.1 requested reconnection for account " + std::to_string(joined.accountId) +
		", but account is not registered on game server"))
		<< logs.dump();

	// join again to get a valid reconnect key
	AionTestClient client2(clientPort);
	joined = joinGameServer(client2, gs, name, 1);
	client2.socket.close();
	gs.send(PacketWriter().C(2).D(joined.accountId).data);
	PacketReader key3(gs.expectPacket(3));
	key3.C();
	key3.D();
	reconnectKey = key3.D();

	AionTestClient reconnecting(clientPort);
	reconnecting.readInit();
	reconnecting.sendPacket(AionTestClient::buildCM_UPDATE_SESSION(joined.accountId, joined.loginOk, reconnectKey));
	PacketReader update(reconnecting.expectPacket(0x0c));
	update.C();
	EXPECT_EQ(update.D(), joined.accountId);
	int32_t newLoginOk = update.D();
	EXPECT_EQ(update.C(), 0);

	reconnecting.sendPacket(AionTestClient::buildCM_SERVER_LIST(joined.accountId, newLoginOk));
	EXPECT_EQ(gs.expectPacket(8), PacketWriter().C(8).D(joined.accountId).data);
	gs.send(PacketWriter().C(8).D(joined.accountId).C(1).data);
	PacketReader list(reconnecting.expectPacket(0x04));
	list.C();
	list.C();
	EXPECT_EQ(list.C(), 1); // last server
}

TEST_F(GameServerFlowTest, PingTimeoutClosesSilentGameServersOnly) {
	LogCapture logs("com.aionemu.loginserver.PingPongTask");
	PingPongTask::PERIOD = 100ms;
	GsTestClient silent(gsPort);
	silent.auth(1, GS1_PASSWORD);
	GsTestClient answering(gsPort);
	answering.auth(2, GS2_PASSWORD);

	auto deadline = std::chrono::steady_clock::now() + 1500ms;
	int pings = 0;
	while (std::chrono::steady_clock::now() < deadline) {
		auto packet = answering.readPacket(200ms, true);
		if (packet && *packet == std::vector<uint8_t>{11}) {
			pings++;
			answering.send(std::vector<uint8_t>{12}); // CM_GS_PONG
		}
	}
	EXPECT_GE(pings, 5);
	EXPECT_TRUE(silent.socket.waitClosed(3s));
	EXPECT_TRUE(logs.waitFor("Gameserver #1 connection died, closing it.")) << logs.dump();
	EXPECT_FALSE(answering.socket.isClosed());
	EXPECT_TRUE(GameServerTable::getGameServerInfo(2)->isOnline());
	EXPECT_EQ(logs.count("connection died"), 1);
}

TEST_F(GameServerFlowTest, SlowPacketDoesNotDelayPongs) {
	LogCapture logs("com.aionemu.loginserver.PingPongTask");
	PingPongTask::PERIOD = 50ms;
	int32_t accountId = createAccountWithTime(newAccountName());
	GsTestClient gs(gsPort);
	gs.auth(1, GS1_PASSWORD);
	{
		TableLock lock("account_data"); // CM_ACCOUNT_LIST blocks in AccountDAO::getAccount
		gs.send(PacketWriter().C(4).D(1).D(accountId).data);
		EXPECT_TRUE(TableLock::waitForBlockedQuery());
		auto deadline = std::chrono::steady_clock::now() + 1000ms; // 20 periods
		int pings = 0;
		while (std::chrono::steady_clock::now() < deadline && !gs.socket.isClosed()) {
			auto packet = gs.readPacket(100ms, true);
			if (packet && *packet == std::vector<uint8_t>{11}) {
				pings++;
				gs.send(std::vector<uint8_t>{12}); // CM_GS_PONG
			} else if (packet) {
				ADD_FAILURE() << "unexpected packet " << int((*packet)[0]);
			}
		}
		EXPECT_GE(pings, 5);
		EXPECT_FALSE(gs.socket.isClosed());
		EXPECT_FALSE(logs.contains("connection died")) << logs.dump();
	}
	// the account list completes once the table is unlocked
	gs.expectPacket(9);
	gs.expectPacket(10);
	EXPECT_TRUE(GameServerTable::getGameServerInfo(1)->isOnline());
	EXPECT_TRUE(GameServerTable::getGameServerInfo(1)->isAccountOnGameServer(accountId));
}

TEST_F(GameServerFlowTest, AccountListOfADisconnectedGameServerRegistersNoAccount) {
	const std::string name = newAccountName();
	int32_t accountId = createAccountWithTime(name);
	auto gsi = GameServerTable::getGameServerInfo(1);
	{
		GsTestClient gs(gsPort);
		gs.auth(1, GS1_PASSWORD);
		TableLock lock("account_data"); // CM_ACCOUNT_LIST blocks in AccountController::loadAccount
		gs.send(PacketWriter().C(4).D(1).D(accountId).data);
		ASSERT_TRUE(TableLock::waitForBlockedQuery());
		gs.socket.close();
		ASSERT_TRUE(waitUntil([&] { return !gsi->getConnection(); })); // onDisconnect ran while the packet still runs
	}
	EXPECT_FALSE(waitUntil([&] { return gsi->isAccountOnGameServer(accountId); }, 1s));
	EXPECT_EQ(gsi->getCurrentPlayers(), 0);

	// the account is not stuck on the offline game server
	AionTestClient client(clientPort);
	client.loginOk(name, "pw");
}

TEST_F(GameServerFlowTest, CmBanOfAnAccountWaitingForItsFastReconnect) {
	GsTestClient gs(gsPort);
	gs.auth(1, GS1_PASSWORD);
	const std::string name = newAccountName();
	AionTestClient client(clientPort);
	Joined joined = joinGameServer(client, gs, name, 1);
	client.socket.close();
	gs.send(PacketWriter().C(2).D(joined.accountId).data);
	PacketReader key(gs.expectPacket(3));
	key.C();
	key.D();
	int32_t reconnectKey = key.D();
	EXPECT_TRUE(controller::AccountController::getReconnectingAccount(joined.accountId));

	gs.send(PacketWriter().C(6).C(1).D(joined.accountId).S("").D(0).D(99).data);
	EXPECT_EQ(gs.expectPacket(5), PacketWriter().C(5).C(1).D(joined.accountId).S("").D(0).D(99).C(1).data);
	EXPECT_FALSE(controller::AccountController::getReconnectingAccount(joined.accountId));

	// the banned player cannot reconnect, and the penalty stays
	AionTestClient reconnecting(clientPort);
	reconnecting.readInit();
	reconnecting.sendPacket(AionTestClient::buildCM_UPDATE_SESSION(joined.accountId, joined.loginOk, reconnectKey));
	EXPECT_TRUE(reconnecting.socket.waitClosed(2s));
	auto accountTime = dao::AccountTimeDAO::getAccountTime(joined.accountId);
	ASSERT_TRUE(accountTime && accountTime->getPenaltyEnd());
	EXPECT_EQ(accountTime->getPenaltyEnd()->time_since_epoch().count(), 1000);
	AionTestClient banned(clientPort);
	banned.authGG();
	banned.login(name, "pw", 0x09);
	EXPECT_TRUE(banned.socket.waitClosed());
}

TEST_F(GameServerFlowTest, CmBanAndCmLsControl) {
	GsTestClient gs(gsPort);
	gs.auth(1, GS1_PASSWORD);
	const std::string name = newAccountName();
	AionTestClient client(clientPort);
	auto login = client.loginOk(name, "pw");
	const std::string idText = std::to_string(login.accountId);

	gs.send(PacketWriter().C(5).C(1).C(3).D(login.accountId).D(77).data);
	EXPECT_EQ(gs.expectPacket(4), PacketWriter().C(4).C(1).C(3).D(login.accountId).D(77).C(1).data);
	EXPECT_EQ(database::queryLong("SELECT access_level FROM account_data WHERE id = " + idText), 3);
	gs.send(PacketWriter().C(5).C(2).C(2).D(login.accountId).D(77).data);
	EXPECT_EQ(gs.expectPacket(4), PacketWriter().C(4).C(2).C(2).D(login.accountId).D(77).C(1).data);
	EXPECT_EQ(database::queryLong("SELECT membership FROM account_data WHERE id = " + idText), 2);

	// account ban: the logged in client is kicked
	gs.send(PacketWriter().C(6).C(1).D(login.accountId).S("").D(0).D(99).data);
	EXPECT_EQ(gs.expectPacket(5), PacketWriter().C(5).C(1).D(login.accountId).S("").D(0).D(99).C(1).data);
	EXPECT_EQ(reasonOf(client.expectPacket(0x08)), 22); // SM_ACCOUNT_KICK(STR_L2AUTH_S_BLOCKED_IP)
	EXPECT_TRUE(client.socket.waitClosed());
	auto accountTime = dao::AccountTimeDAO::getAccountTime(login.accountId);
	ASSERT_TRUE(accountTime && accountTime->getPenaltyEnd());
	EXPECT_EQ(accountTime->getPenaltyEnd()->time_since_epoch().count(), 1000);
	{
		AionTestClient banned(clientPort);
		banned.authGG();
		banned.login(name, "pw", 0x09);
		EXPECT_TRUE(banned.socket.waitClosed());
	}

	// IP ban with time, then unban
	int64_t before = commons::utils::currentTimeMillis();
	gs.send(PacketWriter().C(6).C(2).D(0).S("10.20.30.40").D(10).D(99).data);
	EXPECT_EQ(gs.expectPacket(5), PacketWriter().C(5).C(2).D(0).S("10.20.30.40").D(10).D(99).C(1).data);
	EXPECT_TRUE(controller::BannedIpController::isBanned("10.20.30.40"));
	auto timeEnd = database::queryLong("SELECT UNIX_TIMESTAMP(time_end) FROM banned_ip WHERE mask = '10.20.30.40'");
	ASSERT_TRUE(timeEnd.has_value());
	EXPECT_NEAR(static_cast<double>(*timeEnd), (before + 600000) / 1000.0, 5.0);
	gs.send(PacketWriter().C(6).C(2).D(0).S("10.20.30.40").D(-1).D(99).data);
	EXPECT_EQ(gs.expectPacket(5), PacketWriter().C(5).C(2).D(0).S("10.20.30.40").D(-1).D(99).C(1).data);
	EXPECT_FALSE(controller::BannedIpController::isBanned("10.20.30.40"));
	EXPECT_EQ(database::queryLong("SELECT COUNT(*) FROM banned_ip WHERE mask = '10.20.30.40'"), 0);

	// a full ban of an account that is not logged in bans its last IP instead of the given one
	const std::string name2 = newAccountName();
	int32_t id2 = createAccountWithTime(name2);
	database::execute("UPDATE account_data SET last_ip = '10.9.9.9' WHERE id = " + std::to_string(id2));
	gs.send(PacketWriter().C(6).C(3).D(id2).S("1.1.1.1").D(0).D(5).data);
	EXPECT_EQ(gs.expectPacket(5), PacketWriter().C(5).C(3).D(id2).S("10.9.9.9").D(0).D(5).C(1).data);
	EXPECT_EQ(database::queryString("SELECT time_end FROM banned_ip WHERE mask = '10.9.9.9'"), std::nullopt);
	EXPECT_EQ(database::queryLong("SELECT COUNT(*) FROM banned_ip WHERE mask = '10.9.9.9'"), 1);
	controller::BannedIpController::unbanIp("10.9.9.9");

	// CM_LS_CONTROL for an unknown account: logged, no response
	LogCapture logs("com.aionemu.loginserver.network.gameserver.GsClientPacket");
	gs.send(PacketWriter().C(5).C(1).C(1).D(99999999).D(1).data);
	EXPECT_TRUE(logs.waitFor("error handling gs (127.0.0.1) message [000] CM_LS_CONTROL")) << logs.dump();
	EXPECT_FALSE(gs.readPacket(200ms).has_value());
}

TEST_F(GameServerFlowTest, BanListsAccountListAndAllowedHddSerial) {
	GsTestClient gs1(gsPort);
	gs1.auth(1, GS1_PASSWORD);
	GsTestClient gs2(gsPort);
	gs2.auth(2, GS2_PASSWORD);

	int64_t banEnd = commons::utils::currentTimeMillis() / 1000 * 1000 + 3600000;
	gs1.send(PacketWriter().C(9).C(1).S("00-11-22-33-44-55").S("cheater").Q(banEnd).data);
	gs1.send(PacketWriter().C(10).C(1).S("HDD-XYZ").Q(banEnd).data);
	EXPECT_TRUE(waitUntil([] { return database::queryLong("SELECT COUNT(*) FROM banned_mac WHERE address = '00-11-22-33-44-55'") == 1; }));
	EXPECT_TRUE(waitUntil([] { return database::queryLong("SELECT COUNT(*) FROM banned_hdd WHERE serial = 'HDD-XYZ'") == 1; }));

	// account A plays on #2, account B on no server
	const std::string nameA = newAccountName();
	AionTestClient client(clientPort);
	Joined a = joinGameServer(client, gs2, nameA, 2);
	int32_t idB = createAccountWithTime(newAccountName());

	gs1.send(PacketWriter().C(4).D(2).D(a.accountId).D(idB).data);
	EXPECT_EQ(gs1.expectPacket(2), PacketWriter().C(2).D(a.accountId).C(0).data); // A already plays on #2
	EXPECT_EQ(gs1.expectPacket(9), PacketWriter().C(9).D(1).S("00-11-22-33-44-55").Q(banEnd).S("cheater").data);
	EXPECT_EQ(gs1.expectPacket(10), PacketWriter().C(10).D(1).S("HDD-XYZ").Q(banEnd).data);
	EXPECT_TRUE(GameServerTable::getGameServerInfo(1)->isAccountOnGameServer(idB));
	EXPECT_FALSE(GameServerTable::getGameServerInfo(1)->isAccountOnGameServer(a.accountId));

	// unbans
	gs1.send(PacketWriter().C(9).C(0).S("00-11-22-33-44-55").S("").Q(0).data);
	gs1.send(PacketWriter().C(10).C(0).S("HDD-XYZ").Q(0).data);
	EXPECT_TRUE(waitUntil([] { return database::queryLong("SELECT COUNT(*) FROM banned_mac") == 0; }));
	EXPECT_TRUE(waitUntil([] { return database::queryLong("SELECT COUNT(*) FROM banned_hdd") == 0; }));
	gs1.send(PacketWriter().C(4).D(0).data);
	EXPECT_EQ(gs1.expectPacket(9), PacketWriter().C(9).D(0).data);
	EXPECT_EQ(gs1.expectPacket(10), PacketWriter().C(10).D(0).data);

	gs1.send(PacketWriter().C(11).D(idB).S("ALLOWED-1").data);
	EXPECT_TRUE(waitUntil([&] { return database::queryString("SELECT allowed_hdd_serial FROM account_data WHERE id = " + std::to_string(idB)) == "ALLOWED-1"; }));

	// a malformed account list is not executed
	LogCapture logs("com.aionemu.commons.network.packet.BaseClientPacket");
	gs1.send(PacketWriter().C(4).D(1000).D(1).data);
	EXPECT_TRUE(logs.waitFor("Reading failed for packet [000] CM_ACCOUNT_LIST")) << logs.dump();
	EXPECT_FALSE(gs1.readPacket(200ms).has_value());
}

TEST_F(GameServerFlowTest, GameServerDisconnectUpdatesTheServerListOfLoggedInClients) {
	LogCapture logs("com.aionemu.loginserver.network.gameserver.GsConnection");
	auto gs = std::make_unique<GsTestClient>(gsPort);
	gs->auth(1, GS1_PASSWORD);
	AionTestClient client(clientPort);
	auto login = client.loginOk(newAccountName(), "pw");
	client.sendPacket(AionTestClient::buildCM_SERVER_LIST(login.accountId, login.loginOk));
	gs->expectPacket(8);
	gs->send(PacketWriter().C(8).D(login.accountId).C(2).data);
	PacketReader online(client.expectPacket(0x04));
	online.B(18); // opcode, count, last server, id, ip, port, 0, 0, 0, current players, max players
	EXPECT_EQ(online.C(), 1);

	// account on the game server is removed when it disconnects
	AionTestClient player(clientPort);
	Joined joined = joinGameServer(player, *gs, newAccountName(), 1);
	auto gsi = GameServerTable::getGameServerInfo(1);
	EXPECT_TRUE(gsi->isAccountOnGameServer(joined.accountId));

	gs->socket.close();
	PacketReader offline(client.expectPacket(0x04));
	offline.B(18);
	EXPECT_EQ(offline.C(), 0);
	EXPECT_TRUE(logs.waitFor("Gameserver #1 127.0.0.1 disconnected")) << logs.dump();
	EXPECT_FALSE(gsi->isAccountOnGameServer(joined.accountId));
	EXPECT_EQ(gsi->getCurrentPlayers(), 0);
}

TEST_F(GameServerFlowTest, PlayerTransfer) {
	LogCapture logs({"com.aionemu.loginserver.service.PlayerTransferService", "com.aionemu.loginserver.network.gameserver.GsClientPacket"});
	auto& service = service::PlayerTransferService::getInstance();
	service.shutdown(); // only the explicit verifyNewTasks calls below may run
	GsTestClient gs1(gsPort);
	gs1.auth(1, GS1_PASSWORD);
	GsTestClient gs2(gsPort);
	gs2.auth(2, GS2_PASSWORD);

	const std::string sourceName = newAccountName(), targetName = newAccountName();
	int32_t source = createAccountWithTime(sourceName);
	int32_t target = createAccountWithTime(targetName);
	auto insertTask = [&](int targetServer) {
		database::execute("INSERT INTO player_transfers (source_server, target_server, source_account_id, target_account_id, player_id, status) VALUES (1, " +
			std::to_string(targetServer) + ", " + std::to_string(source) + ", " + std::to_string(target) + ", 777, 0)");
		return static_cast<int32_t>(database::queryLong("SELECT MAX(id) FROM player_transfers").value());
	};
	auto statusOf = [](int32_t taskId) { return database::queryLong("SELECT status FROM player_transfers WHERE id = " + std::to_string(taskId)); };

	// ok
	int32_t task = insertTask(2);
	int32_t offlineTargetTask = insertTask(3);
	service.verifyNewTasks();
	EXPECT_EQ(gs1.expectPacket(12), PacketWriter().C(12).D(23).C(1).C(2).D(source).D(target).D(777).D(task).data);
	EXPECT_EQ(statusOf(task), 1);
	EXPECT_EQ(statusOf(offlineTargetTask), 0);
	EXPECT_TRUE(logs.contains("cannot perform transfer task #" + std::to_string(offlineTargetTask) + " while target server is down #3")) << logs.dump();
	database::execute("DELETE FROM player_transfers WHERE id = " + std::to_string(offlineTargetTask));

	// note: the accounts are (de)activated in memory only, AccountDAO::updateAccount does not store the activated column (like Java)
	std::vector<uint8_t> db = {9, 8, 7, 6};
	gs1.send(PacketWriter().C(13).C(1).D(task).S("CharName").B(db).data);
	EXPECT_EQ(gs2.expectPacket(12), PacketWriter().C(12).D(20).D(target).D(task).S("CharName").S(targetName).D(4).B(db).data);
	gs1.send(PacketWriter().C(13).C(1).D(task).S("CharName").B(db).data);
	EXPECT_EQ(gs1.expectPacket(12), PacketWriter().C(12).D(22).D(task).S("transfer cant be performed while it is already active").data);
	gs2.send(PacketWriter().C(13).C(3).D(task).data);
	EXPECT_EQ(gs1.expectPacket(12), PacketWriter().C(12).D(21).D(task).data);
	EXPECT_EQ(statusOf(task), 2);
	EXPECT_EQ(database::queryString("SELECT comment FROM player_transfers WHERE id = " + std::to_string(task)), "task done");

	// error reported by the target server
	int32_t failing = insertTask(2);
	service.verifyNewTasks();
	gs1.expectPacket(12);
	gs1.send(PacketWriter().C(13).C(1).D(failing).S("CharName").B(db).data);
	gs2.expectPacket(12);
	gs2.send(PacketWriter().C(13).C(2).D(failing).S("clone failed").data);
	EXPECT_EQ(gs2.expectPacket(12), PacketWriter().C(12).D(22).D(failing).S("clone failed").data);
	EXPECT_EQ(statusOf(failing), 3);

	// stopped by the source server
	int32_t stopped = insertTask(2);
	service.verifyNewTasks();
	gs1.expectPacket(12);
	gs1.send(PacketWriter().C(13).C(4).D(stopped).S("stopped").data);
	EXPECT_TRUE(waitUntil([&] { return statusOf(stopped) == 3; }));
	EXPECT_EQ(database::queryString("SELECT comment FROM player_transfers WHERE id = " + std::to_string(stopped)), "stopped");

	// unknown task
	gs1.send(PacketWriter().C(13).C(3).D(987654).data);
	EXPECT_TRUE(logs.waitFor("error handling gs (127.0.0.1) message [000] CM_PTRANSFER_CONTROL")) << logs.dump();
}

} // namespace
} // namespace aion::loginserver::test
