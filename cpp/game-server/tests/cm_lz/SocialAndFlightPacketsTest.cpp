// Seven more small client packets of P5-16 (play-session wishes of 2026-10-02), whose paths were ported already, on the item packet fixture of
// tests/cm_ak (a player in a Verteron map instance on a recording connection) with, where it matters, a second player on its own connection:
// - CM_PLAYER_SEARCH (the social search panel, CM_PLAYER_SEARCH.java:41-92), CM_REPORT_PLAYER (/accuse, CM_REPORT_PLAYER.java:30-56),
//   CM_SHOW_RESTRICTIONS (/restriction, CM_SHOW_RESTRICTIONS.java:25-28), CM_POSITION_SELF (the answer to SM_POSITION_SELF: nothing);
// - CM_BONUS_TITLE (CM_BONUS_TITLE.java:21-33), CM_RECALLED_BY_OTHER_ANSWER (CM_RECALLED_BY_OTHER_ANSWER.java:25-37);
// - CM_WINDSTREAM (C_WIND_PATH, CM_WINDSTREAM.java:33-91): entering, the boosts, leaving, an unknown state.
// - CM_MOVE's glide correction (2026-10-03, C++ branch only, docs/deviations/P5-00.md): a landed glider who walks on to a clicked spot stops
//   gliding (and burning flight time).

#include "../cm_ak/ItemPacketTestSupport.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/PunishmentConfig.h"
#include "aion/gameserver/dataholders/TitleData.bind.h"
#include "aion/gameserver/dataholders/TitleData.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/gameobjects/player/title/TitleList.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/templates/flypath/FlightPath.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/clientpackets/CM_BONUS_TITLE.h"
#include "aion/gameserver/network/aion/clientpackets/CM_MOVE.h"
#include "aion/gameserver/network/aion/clientpackets/CM_PLAYER_SEARCH.h"
#include "aion/gameserver/network/aion/clientpackets/CM_POSITION_SELF.h"
#include "aion/gameserver/network/aion/clientpackets/CM_RECALLED_BY_OTHER_ANSWER.h"
#include "aion/gameserver/network/aion/clientpackets/CM_REPORT_PLAYER.h"
#include "aion/gameserver/network/aion/clientpackets/CM_SHOW_RESTRICTIONS.h"
#include "aion/gameserver/network/aion/clientpackets/CM_WINDSTREAM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_SEARCH.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_WINDSTREAM.h"
#include "aion/gameserver/runtime/base/Finally.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/RecallService.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::items {
namespace {

using model::EmotionType;
using model::gameobjects::player::FriendList;
using model::gameobjects::player::Player;
using model::gameobjects::state::CreatureState;
using model::templates::flypath::FlightPath;
using network::test::LogCapture;
using network::test::PacketWriter;
using serverpackets::SM_SYSTEM_MESSAGE;

// the decoded opcodes of ClientPacketInfo.gen.inc (Java AionClientPacketFactory: packets[n], State.IN_GAME)
constexpr int32_t CM_PLAYER_SEARCH_OPCODE = 159;            // :142
constexpr int32_t CM_BONUS_TITLE_OPCODE = 233;              // :193
constexpr int32_t CM_WINDSTREAM_OPCODE = 70;                // :77
constexpr int32_t CM_RECALLED_BY_OTHER_ANSWER_OPCODE = 195; // :169
constexpr int32_t CM_REPORT_PLAYER_OPCODE = 191;            // :166
constexpr int32_t CM_POSITION_SELF_OPCODE = 17;             // :33
constexpr int32_t CM_SHOW_RESTRICTIONS_OPCODE = 194;        // :168
constexpr int32_t CM_MOVE_OPCODE = 48;                      // :76

constexpr int32_t SM_TITLE_INFO_OPCODE = opcodeOf<serverpackets::SM_TITLE_INFO>;
const char* const AUDIT_LOGGER = "AUDIT_LOG"; // AuditLogger.cpp:22
const char* const TITLES_XML = R"(<player_titles><title id="4" nameId="1100903" desc="Tree Hugger" race="PC_ALL"><modifiers><add name="PHYSICAL_ACCURACY" value="4" bonus="true"/></modifiers></title></player_titles>)";

/** Sets an atomic configuration value for the scope and restores the previous one (the test process loads no properties) */
template <class T>
class ConfigScope {
public:
	ConfigScope(std::atomic<T>& config, T value) : config_(config), previous_(config.load()) { config.store(value); }
	~ConfigScope() { config_.store(previous_); }
	ConfigScope(const ConfigScope&) = delete;
	ConfigScope& operator=(const ConfigScope&) = delete;

private:
	std::atomic<T>& config_;
	const T previous_;
};

class SocialAndFlightPacketsTest : public ItemPacketTest {
protected:
	void TearDown() override {
		if (buddy.player) {
			world::World::getInstance().removeObject(*buddy.player);
			buddy.player->setClientConnection(nullptr);
		}
		buddyClient.reset();
		buddy = {};
		dataholders::DataManager::TITLE_DATA.resetForTests();
		ItemPacketTest::TearDown();
	}

	template <class P>
	void run(int32_t opcode, const std::vector<uint8_t>& body = {}) {
		Driver<P> packet(opcode);
		packet.readAndRun(body, client->get());
	}

	/** A second player on its own recording connection, in the World; `online`: his friend list status (Java sets it at login) */
	void addBuddy(bool online, model::Race race = model::Race::ELYOS) {
		buddy = makePlayer(710102, 9902, "Buddy", race);
		buddyClient = std::make_unique<TestClient>();
		buddyClient->enterWorld(buddy);
		world::World::getInstance().storeObject(*buddy.player);
		if (online)
			buddy.player->getFriendList().setStatus(FriendList::Status::ONLINE, *buddy.commonData);
		clearSent();
		(*buddyClient)->clearSent();
	}

	/** CM_PLAYER_SEARCH's body: S name (25 characters), D region, D class mask, C min level, C max level, C lfg only, C */
	static std::vector<uint8_t> search(std::string_view name, int32_t minLevel = 0xFF, int32_t maxLevel = 0xFF, int32_t lfgOnly = 0) {
		return PacketWriter().S(name).D(0).D(0).C(minLevel).C(maxLevel).C(lfgOnly).C(0).data;
	}

	std::vector<uint8_t> searchResult(const std::vector<runtime::Ptr<Player>>& players) { return serializedFor(serverpackets::SM_PLAYER_SEARCH(players)); }

	PlayerFixture buddy;
	std::unique_ptr<TestClient> buddyClient;
	xml::LoadContext titleContext;
};

// --------------------------------------------------------------------------------------------------------------------- CM_PLAYER_SEARCH

TEST_F(SocialAndFlightPacketsTest, TheSearchFindsOnlinePlayersOfTheRaceByNameAndLevel) {
	addBuddy(true);
	const std::vector<runtime::Ptr<Player>> buddyOnly{runtime::Ptr<Player>(*buddy.player)};

	run<CM_PLAYER_SEARCH>(CM_PLAYER_SEARCH_OPCODE, search("bud")); // CM_PLAYER_SEARCH.java:72: a part of the name, any case
	EXPECT_EQ(sent(), exactly({searchResult(buddyOnly)}));
	clearSent();

	run<CM_PLAYER_SEARCH>(CM_PLAYER_SEARCH_OPCODE, search("")); // no name: every match but the searcher himself (:82-83)
	EXPECT_EQ(sent(), exactly({searchResult(buddyOnly)}));
	clearSent();

	run<CM_PLAYER_SEARCH>(CM_PLAYER_SEARCH_OPCODE, search("Nobody"));
	EXPECT_EQ(sent(), exactly({searchResult({})}));
	clearSent();

	const int32_t level = buddy.player->getLevel();
	run<CM_PLAYER_SEARCH>(CM_PLAYER_SEARCH_OPCODE, search("", level + 1, 0xFF)); // :74-75: below the minimum
	EXPECT_EQ(sent(), exactly({searchResult({})}));
	clearSent();

	run<CM_PLAYER_SEARCH>(CM_PLAYER_SEARCH_OPCODE, search("", 0xFF, 0xFF, 1)); // :70-71: looking for group only
	EXPECT_EQ(sent(), exactly({searchResult({})}));
}

TEST_F(SocialAndFlightPacketsTest, TheSearchSkipsOfflinePlayersAndTheOtherRace) {
	addBuddy(false); // in the World, but his friend list status is OFFLINE (:65-66)
	run<CM_PLAYER_SEARCH>(CM_PLAYER_SEARCH_OPCODE, search("Buddy"));
	EXPECT_EQ(sent(), exactly({searchResult({})}));
	clearSent();

	buddy.player->getFriendList().setStatus(FriendList::Status::ONLINE, *buddy.commonData);
	buddy.commonData->setRace(model::Race::ASMODIANS); // :63-64, gameserver.search.factions_mode off by default
	run<CM_PLAYER_SEARCH>(CM_PLAYER_SEARCH_OPCODE, search("Buddy"));
	EXPECT_EQ(sent(), exactly({searchResult({})}));
}

TEST_F(SocialAndFlightPacketsTest, TheSearchNeedsTheConfiguredLevel) {
	ConfigScope<int32_t> levelToSearch(configs::main::CustomConfig::LEVEL_TO_SEARCH, player().getLevel() + 1);

	run<CM_PLAYER_SEARCH>(CM_PLAYER_SEARCH_OPCODE, search("Buddy"));

	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_CANT_WHO_LEVEL(player().getLevel() + 1))})) << "CM_PLAYER_SEARCH.java:55-58";
}

// --------------------------------------------------------------------------------------------------------------------- CM_REPORT_PLAYER

TEST_F(SocialAndFlightPacketsTest, AReportOfAPlayerOfTheRaceIsAuditedAndConfirmed) {
	configs::main::PunishmentConfig::PUNISHMENT_ENABLE.store(false); // AuditLogger must not reach AutoBan
	ConfigScope<bool> auditLog(configs::main::LoggingConfig::LOG_AUDIT, true);
	LogCapture audit({AUDIT_LOGGER});
	addBuddy(true);

	run<CM_REPORT_PLAYER>(CM_REPORT_PLAYER_OPCODE, PacketWriter().C(0).S("Buddy").data);

	// CM_REPORT_PLAYER.java:45-48: the report is audited and confirmed with an endless count
	EXPECT_EQ(audit.count("reported player Buddy"), 1) << audit.dump();
	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_ACCUSE_SUBMIT("Buddy", "∞"))}));
}

TEST_F(SocialAndFlightPacketsTest, AReportOfTheOtherRaceOrOfOneselfIsRefused) {
	addBuddy(true, model::Race::ASMODIANS);

	run<CM_REPORT_PLAYER>(CM_REPORT_PLAYER_OPCODE, PacketWriter().C(0).S("Buddy").data);
	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_DO_NOT_ACCUSE())})) << "CM_REPORT_PLAYER.java:41-42";
	clearSent();

	world::World::getInstance().storeObject(player()); // World.getPlayer finds the reporter himself
	auto unstore = runtime::finally([this]() noexcept { world::World::getInstance().removeObject(player()); });
	run<CM_REPORT_PLAYER>(CM_REPORT_PLAYER_OPCODE, PacketWriter().C(0).S("Holder").data);
	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_INVALID_TARGET())})) << ":43-44";
}

TEST_F(SocialAndFlightPacketsTest, TheReportCountAndAnUnknownReportType) {
	LogCapture log({"com.aionemu.gameserver.network.aion.clientpackets.CM_REPORT_PLAYER"});

	run<CM_REPORT_PLAYER>(CM_REPORT_PLAYER_OPCODE, PacketWriter().C(1).S("").data);
	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_ACCUSE_COUNT_INFO("∞"))})) << "CM_REPORT_PLAYER.java:50-52";
	clearSent();

	run<CM_REPORT_PLAYER>(CM_REPORT_PLAYER_OPCODE, PacketWriter().C(9).S("Buddy").data);
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(log.count("Unhandled report type 9 (reported player: Buddy)"), 1) << log.dump();
}

// ----------------------------------------------------------------------------------------------------- CM_SHOW_RESTRICTIONS / POSITION_SELF

TEST_F(SocialAndFlightPacketsTest, TheRestrictionLevelIsNormalAndThePositionAnswerNeedsNothing) {
	run<CM_SHOW_RESTRICTIONS>(CM_SHOW_RESTRICTIONS_OPCODE);
	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_ACCUSE_INFO_NORMAL())})) << "CM_SHOW_RESTRICTIONS.java:27";
	clearSent();

	run<CM_POSITION_SELF>(CM_POSITION_SELF_OPCODE);
	EXPECT_TRUE(sent().empty()) << "CM_POSITION_SELF.java:23-24";
}

// ----------------------------------------------------------------------------------------------------------------------- CM_BONUS_TITLE

TEST_F(SocialAndFlightPacketsTest, OnlyAnOwnedTitleOrNoneCanGiveTheBonus) {
	dataholders::DataManager::TITLE_DATA.publish(xml::bindString<dataholders::TitleData>(titleContext, TITLES_XML));
	player().getTitleList().setOwner(player()); // Java Player.setTitleList binds the owner at load; the fixture does not load

	run<CM_BONUS_TITLE>(CM_BONUS_TITLE_OPCODE, PacketWriter().H(4).data);
	EXPECT_TRUE(sent().empty()) << "CM_BONUS_TITLE.java:28-30: a title the player does not have - return";
	EXPECT_NE(player().getCommonData()->getBonusTitleId(), 4);

	player().getTitleList().addEntry(4, 0);
	run<CM_BONUS_TITLE>(CM_BONUS_TITLE_OPCODE, PacketWriter().H(4).data);
	EXPECT_EQ(player().getCommonData()->getBonusTitleId(), 4) << ":32: setBonusTitle";
	EXPECT_EQ(packetsOf(sent(), SM_TITLE_INFO_OPCODE).size(), 1u);
	clearSent();

	run<CM_BONUS_TITLE>(CM_BONUS_TITLE_OPCODE, PacketWriter().H(0xFFFF).data); // read unsigned: 65535, no bonus title
	EXPECT_EQ(player().getCommonData()->getBonusTitleId(), 0xFFFF) << ":28: 0xFFFF skips the ownership check";
}

// ------------------------------------------------------------------------------------------------------------ CM_RECALLED_BY_OTHER_ANSWER

TEST_F(SocialAndFlightPacketsTest, AnAnswerWithoutARecallDoesNothing) {
	// RecallService::requestSummon (the summon skill's question, SM_RECALLED_BY_OTHER) is still AION_UNPORTED, so no request can be pending
	// in play yet; accept and cancel (CM_RECALLED_BY_OTHER_ANSWER.java:33-35) find none and do nothing
	services::RecallService& recalls = services::RecallService::getInstance();
	ASSERT_FALSE(recalls.hasPendingRequest(player()));
	for (const int32_t answer : {0, 1, 2}) {
		SCOPED_TRACE("answer " + std::to_string(answer));
		EXPECT_NO_THROW(run<CM_RECALLED_BY_OTHER_ANSWER>(CM_RECALLED_BY_OTHER_ANSWER_OPCODE, PacketWriter().C(answer).data));
		EXPECT_TRUE(sent().empty());
		EXPECT_EQ(runtime::unportedHitCount(), 0u);
	}
}

// ------------------------------------------------------------------------------------------------------------------------ CM_WINDSTREAM

TEST_F(SocialAndFlightPacketsTest, EnteringBoostingAndLeavingAWindstream) {
	auto windstream = [this](int32_t state) {
		run<CM_WINDSTREAM>(CM_WINDSTREAM_OPCODE, PacketWriter().D(120).D(3000).D(state).data);
	};

	windstream(0); // CM_WINDSTREAM.java:43-54: a windstream flight path, flying
	EXPECT_TRUE(player().isUsingFlightPath(FlightPath::Type::WINDSTREAM));
	EXPECT_TRUE(player().isInState(CreatureState::FLYING));
	EXPECT_EQ(sent(), exactly({serializedFor(serverpackets::SM_WINDSTREAM(0, 1))})) << ":90";
	clearSent();

	windstream(0); // :44-45: already in a windstream - nothing
	EXPECT_TRUE(sent().empty());

	windstream(7); // :81-85: the boost emotion to everyone around and the player, then SM_WINDSTREAM
	EXPECT_EQ(sent(), exactly({serializedFor(serverpackets::SM_EMOTION(player(), EmotionType::WINDSTREAM_START_BOOST)),
						  serializedFor(serverpackets::SM_WINDSTREAM(7, 1))}));
	clearSent();

	windstream(3); // :62-78: leaving: active again, no flight path, the exit emotion, SM_WINDSTREAM
	EXPECT_FALSE(player().isUsingFlightPath(FlightPath::Type::WINDSTREAM));
	EXPECT_FALSE(player().isInState(CreatureState::FLYING));
	EXPECT_TRUE(player().isInState(CreatureState::ACTIVE));
	const std::vector<std::vector<uint8_t>> leaving = sent(); // updateStatsAndSpeedVisually sends its own speed emotion too
	EXPECT_EQ(std::count(leaving.begin(), leaving.end(), serializedFor(serverpackets::SM_EMOTION(player(), EmotionType::WINDSTREAM_EXIT))), 1);
	EXPECT_EQ(leaving.back(), serializedFor(serverpackets::SM_WINDSTREAM(3, 1)));
	clearSent();

	windstream(3); // :63-64: not in a windstream - nothing
	EXPECT_TRUE(sent().empty());
}

TEST_F(SocialAndFlightPacketsTest, AnUnknownWindstreamStateIsLogged) {
	LogCapture log({"com.aionemu.gameserver.network.aion.clientpackets.CM_WINDSTREAM"});

	run<CM_WINDSTREAM>(CM_WINDSTREAM_OPCODE, PacketWriter().D(120).D(3000).D(5).data);

	EXPECT_TRUE(sent().empty()) << "CM_WINDSTREAM.java:86-88: no SM_WINDSTREAM";
	EXPECT_EQ(log.count("Unknown Windstream state #5 was sent from " + player().getPosition()->toString()), 1) << log.dump();
}

// ------------------------------------------------------------------------------------------------------------------------------ CM_MOVE

/**
 * The owner's bug report of 2026-10-03: glide, click a spot on the ground, land - and the flight time kept burning while the character walked
 * on. Java ends a glide only on a FALL or an IMMEDIATE (arrived) packet. The packet types are the owner's traced session (10:31:46-10:31:50,
 * Fuzzytotem): in the air a click is 228 (POSITION|MANUAL|ABSOLUTE|GLIDE) and its continuation 164 (POSITION|ABSOLUTE|GLIDE); after landing
 * the walk on to the clicked spot is 160 (POSITION|ABSOLUTE) - no MANUAL, no GLIDE. The correction: a position packet without the GLIDE flag
 * ends the glide; a glide packet keeps it.
 */
TEST_F(SocialAndFlightPacketsTest, ALandedGliderWhoWalksOnToAClickedSpotStopsGliding) {
	LogCapture log({"com.aionemu.gameserver.network.aion.clientpackets.CM_MOVE"});
	player().getCommonData()->setDaeva(true); // FlyController.canGlide
	const float x = player().getX(), y = player().getY(), z = player().getZ();
	auto move = [&](int32_t type) {
		PacketWriter body;
		body.F(x).F(y).F(z).C(0).C(type);
		if ((type & 0xC0) == 0xC0) // POSITION|MANUAL (ABSOLUTE): the destination
			body.F(x + 5).F(y).F(z);
		if ((type & 0x04) != 0) // GLIDE: the glide flag (NONE)
			body.C(0);
		run<CM_MOVE>(CM_MOVE_OPCODE, body.data);
	};
	auto glide = [&] {
		move(0xC4); // POSITION|MANUAL|GLIDE: switchToGliding
		ASSERT_TRUE(player().isInGlidingState());
		move(0x84); // the traced in-air packets
		move(0xE4);
		move(0xA4);
		EXPECT_TRUE(player().isInGlidingState()) << "every glide packet keeps the glide";
	};

	glide();
	EXPECT_EQ(log.count("ended by a move without the glide flag"), 0) << log.dump();
	move(0xA0); // landed, the click-to-move walking on: POSITION|ABSOLUTE without GLIDE (the traced 160)
	EXPECT_FALSE(player().isInGlidingState()) << "FlyController.onStopGliding: the flight time is restored, not burnt";
	EXPECT_EQ(log.count("ended by a move without the glide flag (type=160)"), 1) << log.dump();

	player().setFlyReuseTime(0); // FlyController.switchToGliding from walking: the fly reuse time of the first glide
	glide();
	move(0xC0); // landed and moved on with the keys (the traced 192)
	EXPECT_FALSE(player().isInGlidingState());
	EXPECT_EQ(log.count("ended by a move without the glide flag (type=192)"), 1) << log.dump();
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::items
