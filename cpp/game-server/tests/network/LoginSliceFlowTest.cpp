// In-process client packet flows of the login slice (P5-00 packets over the P4-15 connection; m5a-plan.md S-01/S-03/S-05/S-09/S-10): a
// FakeGameClient talks to a GameServerTestServer whose packet factory holds the slice's AION_CLIENT_PACKET factories, so every request goes through
// decryption, the opcode table, readImpl, the packet processor and runImpl, and the answers are decrypted and checked byte by byte (the expected
// bodies follow the Java writeImpl). No GameServer::main and no login server: the connection's account and state are set directly.
// A flow that reaches a body of another chunk that is still AION_UNPORTED (the server logs and drops the packet) skips itself.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/configs/main/SecurityConfig.h"
#include "aion/gameserver/dao/PlayerAppearanceDAO.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/network/aion/AionClientPacketFactory.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Finally.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/services/AccountService.h"
#include "aion/gameserver/services/player/PlayerEnterWorldService.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/World.h"
#include "FakeGameClient.h"
#include "LoginSliceTestSupport.h"
#include "support/GameServerTestServer.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

// the AION_CLIENT_PACKET factories of the slice (defined by the markers in the clientpackets sources)
namespace aion::gameserver::network::aion::clientpackets {
#define AION_SLICE_PACKET_FACTORY(Class) std::unique_ptr<AionClientPacket> Class##_clientPacketFactory(int32_t opcode, const StateSet& validStates);
AION_SLICE_PACKET_FACTORY(CM_CHARACTER_LIST)
AION_SLICE_PACKET_FACTORY(CM_CHARACTER_PASSKEY)
AION_SLICE_PACKET_FACTORY(CM_CHECK_NICKNAME)
AION_SLICE_PACKET_FACTORY(CM_CREATE_CHARACTER)
AION_SLICE_PACKET_FACTORY(CM_DELETE_CHARACTER)
AION_SLICE_PACKET_FACTORY(CM_DISCONNECT)
AION_SLICE_PACKET_FACTORY(CM_ENTER_WORLD)
AION_SLICE_PACKET_FACTORY(CM_GAMEGUARD)
AION_SLICE_PACKET_FACTORY(CM_L2AUTH_LOGIN_CHECK)
AION_SLICE_PACKET_FACTORY(CM_LEVEL_READY)
AION_SLICE_PACKET_FACTORY(CM_MAC_ADDRESS)
AION_SLICE_PACKET_FACTORY(CM_MAY_LOGIN_INTO_GAME)
AION_SLICE_PACKET_FACTORY(CM_MOVE)
AION_SLICE_PACKET_FACTORY(CM_PING)
AION_SLICE_PACKET_FACTORY(CM_QUIT)
AION_SLICE_PACKET_FACTORY(CM_RECONNECT_AUTH)
AION_SLICE_PACKET_FACTORY(CM_RESTORE_CHARACTER)
AION_SLICE_PACKET_FACTORY(CM_SECURITY_TOKEN)
AION_SLICE_PACKET_FACTORY(CM_TIME_CHECK)
AION_SLICE_PACKET_FACTORY(CM_UI_SETTINGS)
AION_SLICE_PACKET_FACTORY(CM_VERSION_CHECK)
#undef AION_SLICE_PACKET_FACTORY
} // namespace aion::gameserver::network::aion::clientpackets

namespace aion::gameserver::network::test {
namespace {

using State = aion::AionConnection::State;
using loginslice::test::isDatabaseEnabled;
using runtime::Ref;

namespace sp = aion::serverpackets;

// client opcodes (ClientPacketInfo.gen.inc)
constexpr int32_t CM_VERSION_CHECK = 0;
constexpr int32_t CM_DISCONNECT = 2;
constexpr int32_t CM_QUIT = 3;
constexpr int32_t CM_ENTER_WORLD = 8;
constexpr int32_t CM_TIME_CHECK = 18;
constexpr int32_t CM_PING = 44;
constexpr int32_t CM_SECURITY_TOKEN = 92;
constexpr int32_t CM_L2AUTH_LOGIN_CHECK = 149;
constexpr int32_t CM_CHARACTER_LIST = 150;
constexpr int32_t CM_CREATE_CHARACTER = 151;
constexpr int32_t CM_DELETE_CHARACTER = 152;
constexpr int32_t CM_RESTORE_CHARACTER = 153;
constexpr int32_t CM_CHECK_NICKNAME = 177;
constexpr int32_t CM_MAY_LOGIN_INTO_GAME = 186;
constexpr int32_t CM_MAC_ADDRESS = 189;

/** The slice's registry entries, sorted by name */
std::span<const handlers::ClientPacketEntry> slicePacketEntries() {
	namespace cp = aion::clientpackets;
	static const std::vector<handlers::ClientPacketEntry> entries = [] {
		std::vector<handlers::ClientPacketEntry> list{
			{"CM_CHARACTER_LIST", &cp::CM_CHARACTER_LIST_clientPacketFactory, "tests"},
			{"CM_CHARACTER_PASSKEY", &cp::CM_CHARACTER_PASSKEY_clientPacketFactory, "tests"},
			{"CM_CHECK_NICKNAME", &cp::CM_CHECK_NICKNAME_clientPacketFactory, "tests"},
			{"CM_CREATE_CHARACTER", &cp::CM_CREATE_CHARACTER_clientPacketFactory, "tests"},
			{"CM_DELETE_CHARACTER", &cp::CM_DELETE_CHARACTER_clientPacketFactory, "tests"},
			{"CM_DISCONNECT", &cp::CM_DISCONNECT_clientPacketFactory, "tests"},
			{"CM_ENTER_WORLD", &cp::CM_ENTER_WORLD_clientPacketFactory, "tests"},
			{"CM_GAMEGUARD", &cp::CM_GAMEGUARD_clientPacketFactory, "tests"},
			{"CM_L2AUTH_LOGIN_CHECK", &cp::CM_L2AUTH_LOGIN_CHECK_clientPacketFactory, "tests"},
			{"CM_LEVEL_READY", &cp::CM_LEVEL_READY_clientPacketFactory, "tests"},
			{"CM_MAC_ADDRESS", &cp::CM_MAC_ADDRESS_clientPacketFactory, "tests"},
			{"CM_MAY_LOGIN_INTO_GAME", &cp::CM_MAY_LOGIN_INTO_GAME_clientPacketFactory, "tests"},
			{"CM_MOVE", &cp::CM_MOVE_clientPacketFactory, "tests"},
			{"CM_PING", &cp::CM_PING_clientPacketFactory, "tests"},
			{"CM_QUIT", &cp::CM_QUIT_clientPacketFactory, "tests"},
			{"CM_RECONNECT_AUTH", &cp::CM_RECONNECT_AUTH_clientPacketFactory, "tests"},
			{"CM_RESTORE_CHARACTER", &cp::CM_RESTORE_CHARACTER_clientPacketFactory, "tests"},
			{"CM_SECURITY_TOKEN", &cp::CM_SECURITY_TOKEN_clientPacketFactory, "tests"},
			{"CM_TIME_CHECK", &cp::CM_TIME_CHECK_clientPacketFactory, "tests"},
			{"CM_UI_SETTINGS", &cp::CM_UI_SETTINGS_clientPacketFactory, "tests"},
			{"CM_VERSION_CHECK", &cp::CM_VERSION_CHECK_clientPacketFactory, "tests"},
		};
		std::ranges::sort(list, {}, &handlers::ClientPacketEntry::name);
		return list;
	}();
	return entries;
}

/** Sets an atomic configuration field for the scope and restores the previous value */
template <class T>
class AtomicConfigScope {
public:
	AtomicConfigScope(std::atomic<T>& configValue, T value) : config(configValue), previous(configValue.load()) { config.store(value); }
	~AtomicConfigScope() { config.store(previous); }
	AtomicConfigScope(const AtomicConfigScope&) = delete;
	AtomicConfigScope& operator=(const AtomicConfigScope&) = delete;

private:
	std::atomic<T>& config;
	const T previous;
};

/** A server with the slice packets, a connected client that received SM_KEY and the server side of the connection */
class SliceSession {
public:
	SliceSession() : client(server.port) {
		aion::AionClientPacketFactory::setEntries(slicePacketEntries());
		client.readKey();
		con = server.connection();
	}

	/** Gives the connection an account (without characters unless loaded from the database) and the AUTHED state */
	void authenticate(Ref<model::account::Account> account) {
		{
			runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
			con->setAccount(*account);
		}
		con->setState(State::AUTHED);
		accountRef = std::move(account);
	}

	/** An account without characters but with its warehouse */
	static Ref<model::account::Account> emptyAccount(int32_t id) {
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		Ref<model::account::Account> account = model::account::Account::create(id);
		account->setName("account" + std::to_string(id));
		account->setAccountWarehouse(std::make_unique<model::items::storage::PlayerStorage>(*account, model::items::storage::StorageType::ACCOUNT_WAREHOUSE));
		return account;
	}

	/**
	 * @return the next server packet; std::nullopt if none arrived within the timeout. Sets `unported` if AION_UNPORTED was reached meanwhile
	 * (the server dropped the request at an unported body of another chunk).
	 */
	std::optional<FakeGameClient::ServerPacket> next(std::chrono::milliseconds timeout = 3s) {
		const uint64_t before = runtime::unportedHitCount();
		std::optional<FakeGameClient::ServerPacket> packet = client.readPacket(timeout);
		unported = !packet && runtime::unportedHitCount() > before;
		return packet;
	}

	/** the names of the unported sites reached so far (for skip messages) */
	static std::string unportedSites() {
		std::string sites;
		for (const runtime::UnportedHit& hit : runtime::unportedHits())
			sites += hit.function + " ";
		return sites;
	}

	GameServerTestServer server;
	FakeGameClient client;
	std::shared_ptr<TestAionConnection> con;
	Ref<model::account::Account> accountRef;
	bool unported = false;
};

/** Reads the next server packet into `var`; skips the test if an unported body swallowed the request, fails if nothing arrived */
#define NEXT_PACKET_OR_SKIP(session, var)                                                                                                          \
	std::optional<FakeGameClient::ServerPacket> var = (session).next();                                                                            \
	if (!var && (session).unported)                                                                                                                \
		GTEST_SKIP() << "another chunk is not ported yet: " << SliceSession::unportedSites();                                                     \
	ASSERT_TRUE(var) << "no server packet"

class LoginSliceFlowTest : public ::testing::Test {
protected:
	void SetUp() override {
		loginslice::test::applyConfigDefaultsOnce();
		loginslice::test::publishStaticDataOnce();
	}
};

/** The body of CM_CREATE_CHARACTER (AbstractCharacterEditPacket.readBasicInfo/readAppearance and the type byte) */
std::vector<uint8_t> createCharacterBody(std::string_view name, int32_t gender, int32_t race, int32_t playerClass, int32_t type) {
	PacketWriter w;
	w.D(5).S("account");
	w.S(name).zeros(static_cast<size_t>(25 - static_cast<int32_t>(name.size())) * 2);
	w.D(gender).D(race).D(playerClass);
	w.D(1).D(2).D(3).D(4).D(5);
	for (int i = 0; i < 6; i++)
		w.C(i);
	w.C(4);
	for (int i = 0; i < 37; i++)
		w.C(10 + i);
	w.C(0);
	for (int i = 0; i < 4; i++)
		w.C(60 + i);
	w.C(0).C(0).C(0);
	w.F(1.25f);
	w.C(type);
	return w.data;
}

TEST_F(LoginSliceFlowTest, TimeCheckAnswersInEveryState) {
	SliceSession session;
	session.client.sendPacket(CM_TIME_CHECK, PacketWriter().D(1234).data);
	session.client.expectPacket(aion::opcodeOf<sp::SM_AFTER_TIME_CHECK_4_7_5>);
	FakeGameClient::ServerPacket time = session.client.expectPacket(aion::opcodeOf<sp::SM_TIME_CHECK>);
	PacketReader reader(time.data);
	reader.D(); // server up time
	EXPECT_EQ(reader.D(), 1234);

	session.authenticate(SliceSession::emptyAccount(5));
	session.client.sendPacket(CM_TIME_CHECK, PacketWriter().D(99).data);
	session.client.expectPacket(aion::opcodeOf<sp::SM_AFTER_TIME_CHECK_4_7_5>);
	session.client.expectPacket(aion::opcodeOf<sp::SM_TIME_CHECK>);
}

TEST_F(LoginSliceFlowTest, VersionCheckIsAnswered) {
	SliceSession session;
	session.client.sendPacket(CM_VERSION_CHECK, PacketWriter().H(0x0C3A).H(1).D(949).D(10).D(0).C(2).data);
	NEXT_PACKET_OR_SKIP(session, version);
	EXPECT_EQ(version->opcode, aion::opcodeOf<sp::SM_VERSION_CHECK>);
}

TEST_F(LoginSliceFlowTest, MacAddressStoresTheSerialsAndClosesWithoutALoginServer) {
	SliceSession session;
	session.client.sendPacket(CM_L2AUTH_LOGIN_CHECK, PacketWriter().D(3).D(2).D(5).D(1).D(0).D(0).data);
	session.client.sendPacket(CM_MAC_ADDRESS, PacketWriter().C(0).H(1).D(0x0100007F).S("0A-1B-2C-3D-4E-5F").S("DWW-AC1V325476 8").D(0).data);
	// the login server link is down: LoginServer.authenticateClient closes with SM_L2AUTH_LOGIN_CHECK
	NEXT_PACKET_OR_SKIP(session, response);
	EXPECT_EQ(response->opcode, aion::opcodeOf<sp::SM_L2AUTH_LOGIN_CHECK>);
	EXPECT_TRUE(session.client.socket.waitClosed());
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST)); // the connection fields are runtime fields
	EXPECT_EQ(session.con->getMacAddress(), "0A-1B-2C-3D-4E-5F");
	EXPECT_EQ(session.con->getHddSerial(), "WD-WCAV12345678");
}

TEST_F(LoginSliceFlowTest, PingAnswersPongAndKicksATimerCheater) {
	SliceSession session;
	session.authenticate(SliceSession::emptyAccount(6));
	AtomicConfigScope kick(configs::main::SecurityConfig::PINGCHECK_KICK, true);
	// the first ping only stores the time; the next three come far earlier than CLIENT_PING_INTERVAL
	for (int i = 0; i < 4; i++) {
		session.client.sendPacket(CM_PING, PacketWriter().H(0).data);
		FakeGameClient::ServerPacket pong = session.client.expectPacket(aion::opcodeOf<sp::SM_PONG>);
		EXPECT_EQ(pong.data, (std::vector<uint8_t>{0, 0}));
	}
	EXPECT_TRUE(session.client.socket.waitClosed()) << "the third failed ping kicks (PINGCHECK_KICK)";
}

TEST_F(LoginSliceFlowTest, PingWithoutKickOnlyResetsTheFailCount) {
	SliceSession session;
	session.authenticate(SliceSession::emptyAccount(7));
	AtomicConfigScope kick(configs::main::SecurityConfig::PINGCHECK_KICK, false);
	// 1 + 6 early pings: the fail count reaches 3 twice and is reset both times (an audit line each), the connection stays open
	for (int i = 0; i < 7; i++) {
		session.client.sendPacket(CM_PING, PacketWriter().H(0).data);
		session.client.expectPacket(aion::opcodeOf<sp::SM_PONG>);
	}
	session.client.sendPacket(CM_TIME_CHECK, PacketWriter().D(1).data);
	session.client.expectPacket(aion::opcodeOf<sp::SM_AFTER_TIME_CHECK_4_7_5>);
	EXPECT_FALSE(session.client.socket.isClosed());
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	EXPECT_EQ(session.con->increaseAndGetPingFailCount(), 1) << "reset after the second third failure";
}

TEST_F(LoginSliceFlowTest, CharacterListMayLoginAndDisconnect) {
	SliceSession session;
	session.authenticate(SliceSession::emptyAccount(8));
	session.client.sendPacket(CM_CHARACTER_LIST, PacketWriter().D(0x01020304).data);
	session.client.expectPacket(aion::opcodeOf<sp::SM_ACCOUNT_PROPERTIES>);
	FakeGameClient::ServerPacket list = session.client.expectPacket(aion::opcodeOf<sp::SM_CHARACTER_LIST>);
	EXPECT_EQ(list.data, (PacketWriter().D(0x01020304).C(0).data)) << "playOk2 and no characters";

	session.client.sendPacket(CM_MAY_LOGIN_INTO_GAME);
	FakeGameClient::ServerPacket mayLogin = session.client.expectPacket(aion::opcodeOf<sp::SM_MAY_LOGIN_INTO_GAME>);
	EXPECT_EQ(mayLogin.data, (PacketWriter().D(0).data));

	// CM_DISCONNECT and CM_DELETE_CHARACTER of an unknown character answer nothing: the next packet belongs to the time check
	session.client.sendPacket(CM_DISCONNECT, PacketWriter().C(0).data);
	session.client.sendPacket(CM_DELETE_CHARACTER, PacketWriter().D(0).D(4711).data);
	session.client.sendPacket(CM_TIME_CHECK, PacketWriter().D(1).data);
	session.client.expectPacket(aion::opcodeOf<sp::SM_AFTER_TIME_CHECK_4_7_5>);
}

TEST_F(LoginSliceFlowTest, SecurityTokenIsSent) {
	SliceSession session;
	session.authenticate(SliceSession::emptyAccount(9));
	session.client.sendPacket(CM_SECURITY_TOKEN);
	NEXT_PACKET_OR_SKIP(session, token);
	EXPECT_EQ(token->opcode, aion::opcodeOf<sp::SM_SECURITY_TOKEN>);
	std::string generated;
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		generated = session.accountRef->getSecurityToken();
	}
	EXPECT_FALSE(generated.empty());
	// the second request sends the same token
	session.client.sendPacket(CM_SECURITY_TOKEN);
	FakeGameClient::ServerPacket again = session.client.expectPacket(aion::opcodeOf<sp::SM_SECURITY_TOKEN>);
	EXPECT_EQ(again.data, token->data);
}

TEST_F(LoginSliceFlowTest, CreateCharacterTypeOneOpensTheCreationWindow) {
	SliceSession session;
	session.authenticate(SliceSession::emptyAccount(10));
	session.client.sendPacket(CM_CREATE_CHARACTER, createCharacterBody("x", 1, 1, 77, 1));
	FakeGameClient::ServerPacket response = session.client.expectPacket(aion::opcodeOf<sp::SM_CREATE_CHARACTER>);
	EXPECT_EQ(response.data, (PacketWriter().D(22).data)) << "RESPONSE_OPEN_CREATION_WINDOW, no player info";
}

TEST_F(LoginSliceFlowTest, RestoreOfAnUnknownCharacterFails) {
	SliceSession session;
	session.authenticate(SliceSession::emptyAccount(11));
	session.client.sendPacket(CM_RESTORE_CHARACTER, PacketWriter().D(0).D(4711).data);
	FakeGameClient::ServerPacket response = session.client.expectPacket(aion::opcodeOf<sp::SM_RESTORE_CHARACTER>);
	EXPECT_EQ(response.data, (PacketWriter().D(0x10).D(4711).data));
}

TEST_F(LoginSliceFlowTest, QuitWithoutAnActivePlayer) {
	SliceSession session;
	session.authenticate(SliceSession::emptyAccount(12));
	session.client.sendPacket(CM_QUIT, PacketWriter().C(1).data);
	FakeGameClient::ServerPacket stay = session.client.expectPacket(aion::opcodeOf<sp::SM_QUIT_RESPONSE>);
	EXPECT_EQ(stay.data, (PacketWriter().D(1).C(0).D(-1).data));
	EXPECT_FALSE(session.client.socket.isClosed());

	session.client.sendPacket(CM_QUIT, PacketWriter().C(0).data);
	FakeGameClient::ServerPacket leave = session.client.expectPacket(aion::opcodeOf<sp::SM_QUIT_RESPONSE>);
	EXPECT_EQ(leave.data, (PacketWriter().D(1).C(0).D(-1).data));
	EXPECT_TRUE(session.client.socket.waitClosed()) << "CM_QUIT(0) closes after the response";
}

TEST_F(LoginSliceFlowTest, EnterWorldOfAnUnknownCharacterFails) {
	SliceSession session;
	session.authenticate(SliceSession::emptyAccount(13));
	LogCapture capture({"GAMECONNECTION_LOG"});
	session.client.sendPacket(CM_ENTER_WORLD, PacketWriter().D(77).data);
	FakeGameClient::ServerPacket response = session.client.expectPacket(aion::opcodeOf<sp::SM_ENTER_WORLD_CHECK>);
	EXPECT_EQ(response.data, (std::vector<uint8_t>{2, 0, 0})) << "CONNECTION_ERROR";
	EXPECT_TRUE(capture.waitFor("Player enterWorld fail: character obj ID 77 was not found on account ID 13.")) << capture.dump();
}

// ---------------------------------------------------------------------------------------------------------------------------- database flows

class LoginSliceDatabaseFlowTest : public LoginSliceFlowTest {
protected:
	void SetUp() override {
		if (!isDatabaseEnabled())
			GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL to run the login slice database flows";
		LoginSliceFlowTest::SetUp();
		loginslice::test::setUpDatabaseOnce();
		loginslice::test::clearTables();
		utils::idfactory::IDFactory::getInstance().resetForTests();
	}

	/** a stored character with an appearance row */
	static void storeCharacter(int32_t id, std::string_view name, int32_t accountId) {
		loginslice::test::insertPlayer(id, name, accountId);
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		Ref<model::gameobjects::player::PlayerAppearance> appearance = model::gameobjects::player::PlayerAppearance::create();
		ASSERT_TRUE(dao::PlayerAppearanceDAO::store(id, *appearance));
	}

	static Ref<model::account::Account> loadAccount(int32_t id) {
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		return services::AccountService::loadAccount(id);
	}
};

TEST_F(LoginSliceDatabaseFlowTest, EnterWorldRejectsACharacterThatIsStillSaving) {
	storeCharacter(4001, "Saving", 41);
	SliceSession session;
	session.authenticate(loadAccount(41));
	loginslice::test::execute("UPDATE players SET online = 1 WHERE id = 4001");
	session.client.sendPacket(CM_ENTER_WORLD, PacketWriter().D(4001).data);
	FakeGameClient::ServerPacket response = session.client.expectPacket(aion::opcodeOf<sp::SM_ENTER_WORLD_CHECK>);
	EXPECT_EQ(response.data, (std::vector<uint8_t>{6, 0, 0})) << "REENTRY_TIME";
}

TEST_F(LoginSliceDatabaseFlowTest, EnterWorldRejectsAnEarlyReentry) {
	storeCharacter(4011, "Early", 42);
	loginslice::test::execute("UPDATE players SET last_online = NOW() WHERE id = 4011");
	SliceSession session;
	session.authenticate(loadAccount(42));
	AtomicConfigScope reentry(configs::main::GSConfig::CHARACTER_REENTRY_TIME, 60);
	session.client.sendPacket(CM_ENTER_WORLD, PacketWriter().D(4011).data);
	FakeGameClient::ServerPacket response = session.client.expectPacket(aion::opcodeOf<sp::SM_ENTER_WORLD_CHECK>);
	EXPECT_EQ(response.data, (std::vector<uint8_t>{6, 0, 0})) << "REENTRY_TIME";
}

TEST_F(LoginSliceDatabaseFlowTest, EnterWorldRejectsABannedCharacter) {
	storeCharacter(4021, "Banned", 43);
	// a CHARBAN whose end (start + duration) lies in the future
	const int64_t nowSeconds = commons::utils::currentTimeMillis() / 1000;
	loginslice::test::execute("INSERT INTO player_punishments (player_id, punishment_type, start_time, duration, reason) VALUES (4021, 'CHARBAN', " +
		std::to_string(nowSeconds) + ", 3600, 'test')");
	SliceSession session;
	session.authenticate(loadAccount(43));
	session.client.sendPacket(CM_ENTER_WORLD, PacketWriter().D(4021).data);
	FakeGameClient::ServerPacket response = session.client.expectPacket(aion::opcodeOf<sp::SM_ENTER_WORLD_CHECK>);
	EXPECT_EQ(response.data, (std::vector<uint8_t>{2, 0, 0})) << "CONNECTION_ERROR";
}

TEST_F(LoginSliceDatabaseFlowTest, CheckNicknameOfAUsedName) {
	loginslice::test::insertPlayer(4031, "Occupied", 44);
	SliceSession session;
	session.authenticate(SliceSession::emptyAccount(45));
	AtomicConfigScope mode(configs::main::GSConfig::CHARACTER_CREATION_MODE, 0);
	session.client.sendPacket(CM_CHECK_NICKNAME, PacketWriter().S("Occupied").data);
	FakeGameClient::ServerPacket response = session.client.expectPacket(aion::opcodeOf<sp::SM_NICKNAME_CHECK_RESPONSE>);
	EXPECT_EQ(response.data, (std::vector<uint8_t>{10})) << "RESPONSE_NAME_ALREADY_USED";

	AtomicConfigScope reserved(configs::main::GSConfig::CHARACTER_CREATION_MODE, 2);
	session.client.sendPacket(CM_CHECK_NICKNAME, PacketWriter().S("Occupied").data);
	FakeGameClient::ServerPacket reservedResponse = session.client.expectPacket(aion::opcodeOf<sp::SM_NICKNAME_CHECK_RESPONSE>);
	EXPECT_EQ(reservedResponse.data, (std::vector<uint8_t>{11})) << "RESPONSE_NAME_RESERVED";
}

TEST_F(LoginSliceDatabaseFlowTest, CreateCharacterValidation) {
	loginslice::test::insertPlayer(4041, "Duplicate", 46, "ASMODIANS", "MAGE");
	storeCharacter(4042, "Elyos", 47);
	SliceSession session;
	session.authenticate(loadAccount(47));
	AtomicConfigScope mode(configs::main::GSConfig::CHARACTER_CREATION_MODE, 0);
	AtomicConfigScope limit(configs::main::GSConfig::CHARACTER_LIMIT_COUNT, 8);
	// a used name: 10 (removeDeletedCharacters runs first and finds nothing to delete)
	session.client.sendPacket(CM_CREATE_CHARACTER, createCharacterBody("Duplicate", 0, 0, 0, 0));
	FakeGameClient::ServerPacket used = session.client.expectPacket(aion::opcodeOf<sp::SM_CREATE_CHARACTER>);
	EXPECT_EQ(used.data, (PacketWriter().D(10).data));
	// an invalid class id: FAILED_TO_CREATE_THE_CHARACTER (1)
	session.client.sendPacket(CM_CREATE_CHARACTER, createCharacterBody("Classless", 0, 0, 77, 0));
	FakeGameClient::ServerPacket classless = session.client.expectPacket(aion::opcodeOf<sp::SM_CREATE_CHARACTER>);
	EXPECT_EQ(classless.data, (PacketWriter().D(1).data));
	// the limit: Java's off-by-one lets an account with limit characters create one more (size() > max)
	AtomicConfigScope zeroLimit(configs::main::GSConfig::CHARACTER_LIMIT_COUNT, 0);
	session.client.sendPacket(CM_CREATE_CHARACTER, createCharacterBody("Limited", 0, 0, 0, 0));
	FakeGameClient::ServerPacket limited = session.client.expectPacket(aion::opcodeOf<sp::SM_CREATE_CHARACTER>);
	EXPECT_EQ(limited.data, (PacketWriter().D(4).data)) << "RESPONSE_SERVER_LIMIT_EXCEEDED with 1 character and limit 0";
}

/**
 * The multi-client reject of enterWorld (m5a-plan.md S-05/S-10 case a): a first character is put into the world by hand with a connection of the
 * same MAC address (a full enter world needs the static data of the whole server, which the stage-2 scenario gate provides), then a second client
 * is rejected by MultiClientingService and the Player that getPlayer created must be released again.
 */
TEST_F(LoginSliceDatabaseFlowTest, EnterWorldRejectedByMultiClientingReleasesThePlayer) {
	loginslice::test::SlicePlayerFactoryScope factory;
	storeCharacter(4061, "First", 50);
	storeCharacter(4062, "Second", 51);
	SliceSession first;
	first.authenticate(loadAccount(50));
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		first.con->setMacAddress("0A-1B-2C-3D-4E-5F");
		Ref<model::gameobjects::player::Player> online;
		try {
			online = services::player::PlayerService::getPlayer(4061, first.accountRef);
		} catch (const runtime::UnportedException& unported) {
			GTEST_SKIP() << "another chunk is not ported yet: " << unported.what();
		}
		online->setClientConnection(first.con);
		ASSERT_TRUE(first.con->setActivePlayer(online));
		world::World::getInstance().storeObject(*online);
	}

	// a second client from the same IP with the same MAC address is rejected (SecurityConfig.MULTI_CLIENTING_RESTRICTION_MODE = FULL)
	AtomicConfigScope mode(configs::main::SecurityConfig::MULTI_CLIENTING_RESTRICTION_MODE,
		configs::main::SecurityConfig::MultiClientingRestrictionMode::FULL);
	const int32_t createdBefore = loginslice::test::createdSlicePlayers.load();
	const int32_t destroyedBefore = loginslice::test::destroyedSlicePlayers.load();
	SliceSession second;
	second.authenticate(loadAccount(51));
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		second.con->setMacAddress("0A-1B-2C-3D-4E-5F");
	}
	second.client.sendPacket(CM_ENTER_WORLD, PacketWriter().D(4062).data);
	FakeGameClient::ServerPacket rejected = second.client.expectPacket(aion::opcodeOf<sp::SM_ENTER_WORLD_CHECK>);
	EXPECT_EQ(rejected.data, (std::vector<uint8_t>{2, 0, 0})) << "CONNECTION_ERROR (FULL, not SAME_FACTION)";
	EXPECT_EQ(loginslice::test::createdSlicePlayers.load(), createdBefore + 1) << "getPlayer created the rejected player";
	// the rejected player is dropped: the S-05 guard runs the logout breakers, so nothing retains it
	EXPECT_TRUE(waitUntil(
		[&] {
			loginslice::test::drainReclaimer();
			return loginslice::test::destroyedSlicePlayers.load() > destroyedBefore;
		},
		5s))
		<< "the rejected player was not destroyed";
}

TEST_F(LoginSliceDatabaseFlowTest, EnterWorldOfACharacterThatIsAlreadyInTheWorldIsRejected) {
	loginslice::test::SlicePlayerFactoryScope factory;
	storeCharacter(4071, "Twice", 52);
	SliceSession session;
	session.authenticate(loadAccount(52));
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		Ref<model::gameobjects::player::Player> online;
		try {
			online = services::player::PlayerService::getPlayer(4071, session.accountRef);
		} catch (const runtime::UnportedException& unported) {
			GTEST_SKIP() << "another chunk is not ported yet: " << unported.what();
		}
		world::World::getInstance().storeObject(*online);
	}
	const int32_t createdBefore = loginslice::test::createdSlicePlayers.load();
	LogCapture capture({"GAMECONNECTION_LOG"});
	session.client.sendPacket(CM_ENTER_WORLD, PacketWriter().D(4071).data);
	FakeGameClient::ServerPacket rejected = session.client.expectPacket(aion::opcodeOf<sp::SM_ENTER_WORLD_CHECK>);
	EXPECT_EQ(rejected.data, (std::vector<uint8_t>{2, 0, 0})) << "CONNECTION_ERROR";
	EXPECT_TRUE(capture.waitFor("Duplicate character obj ID 4071 found in world.")) << capture.dump();
	EXPECT_EQ(loginslice::test::createdSlicePlayers.load(), createdBefore) << "the check runs before getPlayer, so no second player is created";
}

TEST_F(LoginSliceDatabaseFlowTest, CreateCharacterReleasesTheTransientPlayerOnSuccessAndOnDatabaseErrors) {
	loginslice::test::SlicePlayerFactoryScope factory;
	SliceSession session;
	session.authenticate(SliceSession::emptyAccount(48));
	AtomicConfigScope mode(configs::main::GSConfig::CHARACTER_CREATION_MODE, 1);
	AtomicConfigScope limit(configs::main::GSConfig::CHARACTER_LIMIT_COUNT, 8);

	// success: RESPONSE_OK with the player info, the character stored and added to the account
	session.client.sendPacket(CM_CREATE_CHARACTER, createCharacterBody("Creator", 1, 0, 0, 0));
	NEXT_PACKET_OR_SKIP(session, created);
	ASSERT_EQ(created->opcode, aion::opcodeOf<sp::SM_CREATE_CHARACTER>);
	ASSERT_GE(created->data.size(), 4u);
	EXPECT_EQ(PacketReader(created->data).D(), 0) << "RESPONSE_OK";
	EXPECT_EQ(loginslice::test::queryLong("SELECT COUNT(*) FROM players WHERE name = 'Creator'"), 1);
	EXPECT_TRUE(loginslice::test::queryString("SELECT creation_date FROM players WHERE name = 'Creator'"));

	// a database error after the players insert. The injected failure sits exactly where PlayerAppearanceDAO.store would fail (Java is not
	// transactional there, CM_CREATE_CHARACTER.java:66-68); an earlier version of this test renamed player_appearance away, which blocks on the
	// open table handles of the connection pool and mutates the shared schema (docs/deviations/P5-00.md).
	services::player::PlayerService::setStoreNewPlayerHookForTests([](model::gameobjects::player::Player&) { return false; });
	auto removeHook = runtime::finally([] { services::player::PlayerService::setStoreNewPlayerHookForTests(nullptr); });
	session.client.sendPacket(CM_CREATE_CHARACTER, createCharacterBody("Unlucky", 0, 0, 0, 0));
	std::optional<FakeGameClient::ServerPacket> failed = session.next(10s);
	ASSERT_TRUE(failed) << "no SM_CREATE_CHARACTER; socket closed " << session.client.socket.isClosed() << "; unported: " << session.unported << " "
						<< SliceSession::unportedSites();
	EXPECT_EQ(failed->opcode, aion::opcodeOf<sp::SM_CREATE_CHARACTER>);
	EXPECT_EQ(failed->data, (PacketWriter().D(2).data)) << "RESPONSE_DB_ERROR";
	EXPECT_EQ(loginslice::test::queryLong("SELECT COUNT(*) FROM players WHERE name = 'Unlucky'"), 1)
		<< "Java leaves the players row of the failed creation behind";

	// both transient players are released once the packet tasks ended
	ASSERT_TRUE(waitUntil(
		[] {
			loginslice::test::drainReclaimer();
			return loginslice::test::destroyedSlicePlayers.load() == loginslice::test::createdSlicePlayers.load();
		},
		5s));
	EXPECT_EQ(loginslice::test::createdSlicePlayers.load(), 2);
}

/**
 * The plan's S-10 destroy case (b): the duplicate enter of PlayerEnterWorldService.java:159. With the object id already in `enteringWorld`, the
 * `!contains && add` guard is false and the method falls through **without sending any packet** - the only path where the C++ finally guard of
 * S-05 fires after getPlayer without a preceding reject. The Player getPlayer created must still be released.
 */
TEST_F(LoginSliceDatabaseFlowTest, ADuplicateEnterSendsNoPacketAndStillReleasesThePlayer) {
	loginslice::test::SlicePlayerFactoryScope factory;
	storeCharacter(4091, "Doubled", 54);
	SliceSession session;
	session.authenticate(loadAccount(54));
	ASSERT_TRUE(services::player::PlayerEnterWorldService::addEnteringWorldForTests(4091));
	auto removeEntering = runtime::finally([] { services::player::PlayerEnterWorldService::removeEnteringWorldForTests(4091); });

	const int32_t createdBefore = loginslice::test::createdSlicePlayers.load();
	const int32_t destroyedBefore = loginslice::test::destroyedSlicePlayers.load();
	session.client.sendPacket(CM_ENTER_WORLD, PacketWriter().D(4091).data);

	// nothing is sent: the branch is skipped after getPlayer returned
	std::optional<FakeGameClient::ServerPacket> none = session.next(1s);
	if (!none && session.unported)
		GTEST_SKIP() << "another chunk is not ported yet: " << SliceSession::unportedSites();
	EXPECT_FALSE(none) << "the duplicate enter must send no packet (opcode " << (none ? none->opcode : -1) << ")";
	EXPECT_EQ(loginslice::test::createdSlicePlayers.load(), createdBefore + 1) << "getPlayer created the player before the guard";
	EXPECT_TRUE(waitUntil(
		[&] {
			loginslice::test::drainReclaimer();
			return loginslice::test::destroyedSlicePlayers.load() > destroyedBefore;
		},
		5s))
		<< "the player of the duplicate enter was not destroyed";
}

/**
 * The account warehouse is a part of the **Account** and shared by every character of it, so its actor can belong to a character that is online
 * on another connection of the same account. A rejected enter of a second character then runs the S-05 guard, whose breakers (L3) null that
 * shared actor; the guard must put the online character back (docs/deviations/P5-00.md). Java leaves the (wrong but non-null) rejected player
 * there, so only the C++ port can turn this into a null-actor dereference.
 * <p>
 * The trigger is a second connection of the same account, not a second CM_ENTER_WORLD on one connection: `setActivePlayer` moves a connection
 * to IN_GAME, and CM_ENTER_WORLD is only valid in AUTHED (ClientPacketInfo.gen.inc:25), so the second packet would never be instantiated.
 */
TEST_F(LoginSliceDatabaseFlowTest, AnEnterRejectedOnASecondConnectionKeepsTheAccountWarehouseActorOfTheOnlineCharacter) {
	loginslice::test::SlicePlayerFactoryScope factory;
	storeCharacter(4101, "Online", 55);
	storeCharacter(4102, "Rejected", 55);
	Ref<model::account::Account> account = loadAccount(55);
	SliceSession first;
	first.authenticate(account);
	SliceSession second;
	second.authenticate(account); // the same Account object: one account, two connections

	Ref<model::gameobjects::player::Player> online;
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		first.con->setMacAddress("0A-1B-2C-3D-4E-5F");
		second.con->setMacAddress("0A-1B-2C-3D-4E-5F");
		try {
			online = services::player::PlayerService::getPlayer(4101, first.accountRef);
		} catch (const runtime::UnportedException& unported) {
			GTEST_SKIP() << "another chunk is not ported yet: " << unported.what();
		}
		online->setClientConnection(first.con);
		ASSERT_TRUE(first.con->setActivePlayer(online));
		world::World::getInstance().storeObject(*online);
		ASSERT_EQ(services::player::PlayerService::accountWarehouseActor(*account).get(), online.get()) << "getPlayer made the first character the actor";
	}

	// the second connection is rejected by MultiClientingService after getPlayer made the second character the actor
	AtomicConfigScope mode(configs::main::SecurityConfig::MULTI_CLIENTING_RESTRICTION_MODE,
		configs::main::SecurityConfig::MultiClientingRestrictionMode::FULL);
	const int32_t createdBefore = loginslice::test::createdSlicePlayers.load();
	second.client.sendPacket(CM_ENTER_WORLD, PacketWriter().D(4102).data);
	NEXT_PACKET_OR_SKIP(second, rejected);
	ASSERT_EQ(rejected->opcode, aion::opcodeOf<sp::SM_ENTER_WORLD_CHECK>);
	EXPECT_EQ(rejected->data, (std::vector<uint8_t>{2, 0, 0})) << "CONNECTION_ERROR";
	EXPECT_EQ(loginslice::test::createdSlicePlayers.load(), createdBefore + 1) << "getPlayer created the rejected player";
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		EXPECT_EQ(services::player::PlayerService::accountWarehouseActor(*account).get(), online.get())
			<< "the breakers of the rejected player must not leave the online character without an account warehouse actor";
		world::World::getInstance().removeObject(*online);
	}
}

} // namespace
} // namespace aion::gameserver::network::test
