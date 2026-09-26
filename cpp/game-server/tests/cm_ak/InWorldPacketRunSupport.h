#pragma once

// Shared fixture of the stage-3 wave B in-world client packet tests (m5a-plan.md §11 "Unported client packets", m5a-client-session.md F-2): the
// runImpl of a CM_* class driven against real Players, a real KnownList and a real AionConnection.
//
// - InWorldPacketTest: a DeterministicExecutor on a ManualClock, the IDFactory and the empty NPC skill data like the controller and packet tests;
//   an empty SKILL_DATA so GMService (AuditLogger's staff loop) constructs. One TEST TaskScope spans SetUp, the body and the derived TearDown
//   (as in tests/sm_ak/SmAkTestSupport.h): a Ref the fixture holds is read and released inside that scope, never after it.
// - makePlayer: Java PlayerService.getPlayer (account, common data, appearance, account data, account warehouse, create<Player>, setKnownlist),
//   with the real PlayerGameStats and PlayerLifeStats (see TestPlayer) and the Player parts the packets read.
// - TestKnownList: KnownList with Java's protected add(VisibleObject) exposed, so a test can make an object known (and, through the target's
//   visual state, seen or not seen) without the World and the spawn machinery. The know and see sets are updated before the notification
//   (KnownList.cpp:187-191), which is what these packets read; the notification itself (PlayerController::see -> SM_PLAYER_INFO ->
//   getActiveHouse -> HousingService -> PlayerDAO::getUsedIDs) needs a database, so it throws here, and KnownList::notifySee logs it and counts
//   it (KnownList.cpp:224-231). knownSeeNotifiesFailed() below pins that, so a test never mistakes a half-done see for a complete one; the
//   complete see is the scenario gate's business, not a chunk test's.
// - TestClient: a real AionConnection on a loopback socket whose IO was never started, so AConnectionBase::requestWrite returns early
//   (AConnection.cpp:101-106) and every sendPacket stays in the send queue as a SerializedBody the test can read back.
//
// tests/cm_lz includes this header by relative path: cm_ak and cm_lz are two chunks with two test executables, and their only shared test
// support directory (tests/support) belongs to another chunk. See the wave B report's manifest note.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <asio/ip/tcp.hpp>

#include "aion/commons/network/NioServer.h"
#include "aion/commons/utils/InetSocketAddress.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.bind.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/controllers/FlyController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/BlockList.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"
#include "aion/gameserver/model/gameobjects/player/emotion/EmotionList.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/AionClientPacket.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/SerializedBody.h"
#include "aion/gameserver/network/aion/StateSet.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "../support/NetworkTestSupport.h" // LogCapture and the little endian PacketWriter

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {

/** The first 16 levels of player_experience_table.xml, as tests/support/LoginSliceTestSupport.h:283-285 spells them (that file is another
 *  chunk's support and pulls the whole login slice in, so the literal is repeated rather than included) */
inline constexpr std::string_view PLAYER_EXPERIENCE_TABLE_XML =
	"<player_experience_table><exp>0</exp><exp>400</exp><exp>1433</exp><exp>3820</exp><exp>9054</exp>"
	"<exp>17655</exp><exp>30978</exp><exp>52010</exp><exp>82982</exp><exp>126069</exp><exp>182252</exp><exp>260622</exp><exp>360825</exp>"
	"<exp>490331</exp><exp>649169</exp><exp>844378</exp></player_experience_table>";

/**
 * Life stats that start at 0 current HP, so Creature::isDead() is true (CreatureLifeStats.cpp:54-56) without running any part of the death path.
 * Installed with setLifeStats by a test that needs the dead-player guard of a runImpl.
 */
class DeadLifeStats final : public model::stats::container::CreatureLifeStats {
public:
	explicit DeadLifeStats(model::gameobjects::Creature& owner) : CreatureLifeStats(owner, 0, 0) {}
};

/**
 * The same for a body that reads `player.getLifeStats()` rather than `creature.getLifeStats()`: Player narrows the part with a cast
 * (Player.cpp:336-338, Java's covariant CreatureLifeStats<Player>), which a plain DeadLifeStats fails. onHpChanged is overridden away so that
 * setCurrentHp(0) only writes the field instead of running the whole PlayerController::onDie path (CreatureLifeStats.cpp:303-307).
 */
class DeadPlayerLifeStats final : public model::stats::container::PlayerLifeStats {
public:
	explicit DeadPlayerLifeStats(model::gameobjects::player::Player& owner) : PlayerLifeStats(owner) { setCurrentHp(0); }

protected:
	void onHpChanged(int32_t, int32_t, runtime::Ptr<model::gameobjects::Creature>) override {}
};

/**
 * The real Player, with the real PlayerGameStats and PlayerLifeStats that Player::postConstruct installs (Player.cpp:216-224).
 * <p>
 * The other chunks' player fixtures still put CreatureGameStats doubles in their place and catch an UnportedException around postConstruct, from
 * the days when the P5-01 stat calculation was missing. Both stat containers are fully ported now (no AION_UNPORTED in PlayerGameStats.cpp, and
 * PlayerLifeStats' only one is sendGroupPacketUpdate behind isInTeam()), and a double is not an option here anyway: Player::getGameStats narrows
 * to PlayerGameStats (Player.cpp:341), so SM_EMOTION's `player.getGameStats()->getMovementSpeedFloat()` throws a cast error on anything else.
 */
class TestPlayer final : public model::gameobjects::player::Player {
	AION_MAKE_REF_FRIEND
public:
	TestPlayer(CreateKey key, model::account::PlayerAccountData& playerAccountData, model::account::Account& account)
		: Player(key, playerAccountData, account) {}

protected:
	~TestPlayer() override = default;
};

/** KnownList with Java's add(VisibleObject) exposed (protected since S0b; the server's own sites use addPair) */
class TestKnownList final : public world::knownlist::KnownList {
public:
	explicit TestKnownList(model::gameobjects::VisibleObject& owner) : KnownList(owner) {}

	/** Java KnownList.add: insert, then the visibility update, which asks the owner's canSee */
	bool addForTest(model::gameobjects::VisibleObject& object) { return add(object); }
};

/** How many see/not-see notifications KnownList::notifySee has swallowed since SetUp (see the header comment) */
inline uint64_t knownSeeNotifiesFailed() {
	return world::knownlist::KnownList::notifyFailureCount();
}

struct PlayerFixture {
	runtime::Ref<model::account::Account> account;
	runtime::Ref<model::gameobjects::player::PlayerCommonData> commonData;
	runtime::Ref<model::gameobjects::player::PlayerAppearance> appearance;
	runtime::Ref<TestPlayer> player;

	TestKnownList& knownList() const { return static_cast<TestKnownList&>(player->getKnownList()); }
};

/**
 * Java PlayerService.getPlayer, with the parts these packets read built empty in memory instead of loaded by a DAO: account, common data,
 * appearance, account data, the account warehouse, create<Player>, then the Player parts PlayerService.cpp:194-205 assigns. Each part below is
 * one that a CM_* runImpl of this wave (or a packet it sends) dereferences without a null check, exactly as Java does, so leaving it out would
 * make a test fail on the fixture rather than on the packet:
 *   knownlist          PlayerService.cpp:195   CM_TARGET_SELECT: sees/knows/getObject
 *   friendList         PlayerService.cpp:196   CM_FRIEND_STATUS: getFriendList().setStatus
 *   blockList          PlayerService.cpp:197   CM_SHOW_BLOCKLIST: SM_BLOCK_LIST reads it at serialization
 *   playerSettings     PlayerService.cpp:199   PlayerController::see -> SM_PLAYER_INFO (the known list notifies on add)
 *   abyssRank          PlayerService.cpp:200   the same SM_PLAYER_INFO; the rank of a character with no abyss_rank row (AbyssRankDAO.cpp:188)
 *   effectController   PlayerService.cpp:204   CM_EMOTION: isInAnyAbnormalState / isUnderFear / isConfused
 *   flyController      PlayerService.cpp:205   CM_EMOTION: FLY and LAND
 *   emotions           PlayerEmotionListDAO    CM_EMOTION: getEmotions()->canUse before the broadcast
 */
inline PlayerFixture makePlayer(int32_t objectId, int32_t accountId, std::string_view name, model::Race race = model::Race::ELYOS) {
	PlayerFixture f;
	f.account = model::account::Account::create(accountId);
	f.commonData = model::gameobjects::player::PlayerCommonData::create(objectId);
	f.commonData->setName(name);
	f.commonData->setRace(race);
	// a fresh level-1 character, as the real-client session created one: PlayerGameStats interns the stats template of this class and level
	f.commonData->setPlayerClass(model::PlayerClass::WARRIOR);
	f.commonData->setLevel(1);
	f.appearance = model::gameobjects::player::PlayerAppearance::create();
	f.account->addPlayerAccountData(std::make_unique<model::account::PlayerAccountData>(*f.account, *f.commonData, *f.appearance));
	f.account->setAccountWarehouse(
		std::make_unique<model::items::storage::PlayerStorage>(*f.account, model::items::storage::StorageType::ACCOUNT_WAREHOUSE));
	f.player = model::gameobjects::VisibleObject::create<TestPlayer>(*f.account->getPlayerAccountData(objectId), *f.account);
	f.player->setKnownlist(std::make_unique<TestKnownList>(*f.player)); // PlayerService.cpp:195
	f.player->setFriendList(std::make_unique<model::gameobjects::player::FriendList>(*f.player,
		std::vector<runtime::Ptr<model::gameobjects::player::Friend>>{}));
	f.player->setBlockList(model::gameobjects::player::BlockList::create());
	f.player->setPlayerSettings(model::gameobjects::player::PlayerSettings::create());
	f.player->setAbyssRank(model::gameobjects::player::AbyssRank::create(0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0));
	f.player->setEffectController(std::make_unique<controllers::effect::PlayerEffectController>(*f.player));
	f.player->setFlyController(std::make_unique<controllers::FlyController>(*f.player));
	f.player->setEmotions(std::make_unique<model::gameobjects::player::emotion::EmotionList>(*f.player));
	f.player->setPosition(world::WorldPosition::create(210010000, 100.0f, 100.0f, 50.0f, int8_t{0}));
	return f;
}

/** An AionConnection that hands out its send queue (nothing is ever written: the IO of this connection was never started) */
class RecordingAionConnection : public AionConnection {
public:
	RecordingAionConnection(asio::ip::tcp::socket socket, commons::network::NioServer& server) : AionConnection(std::move(socket), server) {}

	/** Everything sendPacket queued since the last clearSent(), in order */
	std::vector<SerializedBody> sent() {
		std::lock_guard lock(guard);
		std::vector<SerializedBody> bodies;
		// AionConnection::sendMsgQueue is a std::deque<SerializedBody> of its own (AionConnection.h:87), not AConnection's shared_ptr queue
		for (const SerializedBody& body : getSendMsgQueue())
			bodies.push_back(body);
		return bodies;
	}

	/** The queued packets' bytes, for comparison against a freshly serialized expected packet */
	std::vector<std::vector<uint8_t>> sentBytes() {
		std::vector<std::vector<uint8_t>> bytes;
		for (const SerializedBody& body : sent())
			bytes.push_back(*body.bytes);
		return bytes;
	}

	void clearSent() {
		std::lock_guard lock(guard);
		getSendMsgQueue().clear();
	}
};

/** A real AionConnection on a loopback socket, with no IO started (as in the P4-17 packet tests) */
class TestClient {
public:
	TestClient() : acceptor(io, asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0)) {
		server.connect();
		commons::utils::InetSocketAddress address{"127.0.0.1", acceptor.local_endpoint().port()};
		connection = std::make_shared<RecordingAionConnection>(server.openSocket(address), server);
	}

	~TestClient() {
		if (connection) {
			connection->clearSent();
			connection->setActivePlayer(nullptr);
		}
		connection.reset();
		server.shutdown(std::chrono::seconds(2));
	}

	TestClient(const TestClient&) = delete;
	TestClient& operator=(const TestClient&) = delete;

	/**
	 * Java PlayerEnterWorldService: the connection knows the player (State.IN_GAME) and the player knows the connection. The account is set
	 * because setActivePlayer moves the connection to IN_GAME, where AionConnection::sendPacketInfo asks
	 * canReceivePacketInfoInChat() -> getAccount()->getMembership() on every sent packet (AionConnection.cpp:251-258); Java has the account
	 * from login, so an unset one would be an NPE there too.
	 */
	void enterWorld(model::gameobjects::player::Player& player, model::account::Account& account) {
		connection->setAccount(account);
		connection->setActivePlayer(runtime::Ptr<model::gameobjects::player::Player>(player));
		player.setClientConnection(connection);
	}

	/** The account of the player's own PlayerAccountData, as Java's login already put on the connection */
	void enterWorld(const PlayerFixture& fixture) { enterWorld(*fixture.player, *fixture.account); }

	const std::shared_ptr<RecordingAionConnection>& get() const { return connection; }

	/** The raw connection a PER_RECIPIENT packet is serialized for (AionConnection::sendPacket passes `this`) */
	AionConnection* con() const { return connection.get(); }

	RecordingAionConnection& operator*() const { return *connection; }
	RecordingAionConnection* operator->() const { return connection.get(); }

private:
	asio::io_context io;
	asio::ip::tcp::acceptor acceptor;
	commons::network::NioServer server{1, {}};
	std::shared_ptr<RecordingAionConnection> connection;
};

/** A client packet whose protected readImpl/runImpl a test can drive directly (the PacketProcessor calls read() and run()) */
template <class P>
class Driver final : public P {
public:
	explicit Driver(int32_t opcode) : P(opcode, StateSet{AionConnection_State::IN_GAME}) {}

	/** Reads the body and runs it, as AionConnection::processData and the PacketProcessor do */
	void readAndRun(const std::vector<uint8_t>& body, const std::shared_ptr<AionConnection>& connection) {
		std::vector<uint8_t> copy = body;
		this->setBuffer(commons::utils::ByteBuffer::wrap(copy));
		this->setConnection(connection);
		ASSERT_TRUE(this->read());
		this->runImpl();
	}

	void runNow() { this->runImpl(); }
};

/** One serialization of an expected server packet, to compare against what runImpl queued */
inline std::vector<uint8_t> serialized(AionServerPacket&& packet, AionConnection* con = nullptr) {
	return *packet.serialize(con).bytes;
}

inline std::vector<uint8_t> serialized(AionServerPacket& packet, AionConnection* con = nullptr) {
	return *packet.serialize(con).bytes;
}

/**
 * The expected value of `sentBytes()` when runImpl must queue exactly these packets and nothing else. Spelled out because `std::vector{bytes}`
 * deduces `std::vector<uint8_t>` through the copy deduction guide, not the one-element `std::vector<std::vector<uint8_t>>` a test means.
 */
inline std::vector<std::vector<uint8_t>> exactly(std::initializer_list<std::vector<uint8_t>> bodies) {
	return {bodies.begin(), bodies.end()};
}

/**
 * The writeImpl data of one queued packet, i.e. its bytes after the 5 byte header (AionServerPacket::writeOP:
 * [H (opcode + 207) ^ 0xDF][C 0x44][H ~wire opcode]), so a test can decode the body with test::PacketReader instead of comparing it to another
 * serialization of the server's own packet (m5a-plan.md D9).
 */
inline std::vector<uint8_t> bodyOf(const std::vector<uint8_t>& serializedPacket) {
	EXPECT_GE(serializedPacket.size(), 5u);
	if (serializedPacket.size() < 5)
		return {};
	return std::vector<uint8_t>(serializedPacket.begin() + 5, serializedPacket.end());
}

class InWorldPacketTest : public ::testing::Test {
protected:
	void SetUp() override {
		utils::ThreadPoolManager::installBackend(nullptr);
		utils::ThreadPoolManager::installBackend(std::make_unique<runtime::DeterministicExecutor>(clock, 17));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		dataholders::DataManager::NPC_SKILL_DATA.publish(std::make_unique<dataholders::NpcSkillData>());
		// GMService's constructor (AuditLogger's staff member loop) reads SKILL_DATA; an empty holder is enough, it finds no GM skill
		xml::LoadContext context;
		dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(context, "<skill_data></skill_data>"));
		// PlayerCommonData::setLevel goes through setExp, which reads the table (PlayerCommonData.cpp:216-218); the first 16 levels are enough
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.publish(
			xml::bindString<dataholders::PlayerExperienceTable>(context, PLAYER_EXPERIENCE_TABLE_XML));
		world::knownlist::KnownList::resetNotifyFailureCountForTests();
		// one scope for SetUp, the body and every derived TearDown: Refs the fixture holds are created, read and released inside it
		scope = std::make_unique<runtime::TaskScope>(AION_TASK_INFO(runtime::TaskKind::TEST));
	}

	void TearDown() override {
		scope.reset();
		runtime::Reclaimer::getInstance().drain();
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
		dataholders::DataManager::NPC_SKILL_DATA.resetForTests();
		dataholders::DataManager::SKILL_DATA.resetForTests();
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.resetForTests();
	}

	runtime::ManualClock clock{0};
	std::unique_ptr<runtime::TaskScope> scope;
};

} // namespace aion::gameserver::network::aion::clientpackets::testing
