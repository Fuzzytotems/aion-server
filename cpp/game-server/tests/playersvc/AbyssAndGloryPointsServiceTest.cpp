// M5d E-09 (m5d-plan.md §7, §18.3, P5-08): the abyss-point and glory-point bodies a quest reward calls - AbyssPointsService.addAp(Player, int),
// addAp(Player, int, IntFunction<SM_SYSTEM_MESSAGE>) and onRankChanged (AbyssPointsService.java:33-63), and GloryPointsService.addGp
// (GloryPointsService.java:18-35). QuestService.giveReward pays a quest's AP and GP through them (QuestService.java:236-244; 30 relic-reward
// quests pay AP and 12 kill-in-zone quests GP, m5d-plan.md T-04), TradeService takes an abyss vendor's AP through addAp.
// - addAp: the AP added to the rank (AbyssRank.addAp: the daily and weekly AP only for a gain, the AP cap), the message of the points actually
//   added (the gain message for a gain, STR_MSG_USE_ABYSSPOINT for a loss), then onRankChanged: SM_ABYSS_RANK when the points or the rank
//   moved, and a rank change broadcast as SM_ABYSS_RANK_UPDATE to the players who see him (broadcastPacket(VisibleObject, packet) walks the known
//   list, not the player himself, PacketSendUtility.java:98-100), followed by the rank-limited equipment check (an item the new rank may not
//   wear is unequipped, with STR_MSG_UNEQUIP_RANKITEM) and the abyss skills (AbyssSkillService.updateSkills: the race's abyss skills taken,
//   those of a rank at or above RankingConfig.XFORM_MIN_RANK given as temporary skills). A null player does nothing; a gain of 0 is a gain.
// - addGp: nothing for 0; an online player's GP (the daily and weekly GP only for a gain), the gain or loss message of the points actually
//   added, and SM_ABYSS_RANK only when they moved; an offline player's GP goes to AbyssRankDAO.addGp, which this executable reaches without a
//   database (the DAO logs its SQLException, which the case reads) and, with the test database of the DAO tests (tests/dao/DaoTestDatabase.h,
//   AION_TEST_GS_DATABASE_URL), writes: the day's and the week's GP only for a gain.
// Not driven: the legion member's arm of addAp (Legion.addContributionPoints stays unported, P5-11, and Legion's constructor too, so no test
// can build a legion member) and the kill variant addAp(Player, VisibleObject, int), which stays unported on purpose (m5d-plan.md §18.8 item 6).
//
// The rank-limited sword and the two abyss skills of the Elyos STAR5_OFFICER are the shipped rows, verbatim (file:line beside each), added
// to the fixture's rows by the cases that need them.
//
// SM_ABYSS_RANK's ranking position comes from AbyssRankingCache, whose constructor loads the ranking from the database: without one the DAO
// logs and answers no row (AbyssRankDAO.loadRankingListPlayers), so the position of every character is 0 here. The expected packets are Java's
// bytes (writeOP + the writeImpl fields: SM_ABYSS_RANK.java, SM_ABYSS_RANK_UPDATE.java; ServerPacketsOpcodes.java:154, :255); the system
// messages are compared with the server's own serialization of the message Java builds (their bytes are pinned by the sm tests).

#include "../cm_ak/ItemPacketTestSupport.h"
#include "../dao/DaoTestDatabase.h"
#include "../world/WorldTestSupport.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/RankingConfig.h"
#include "aion/gameserver/dataholders/SkillTreeData.bind.h"
#include "aion/gameserver/dataholders/SkillTreeData.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketLookups.h"
#include "aion/gameserver/services/abyss/AbyssPointsService.h"
#include "aion/gameserver/services/abyss/GloryPointsService.h"
#include "aion/gameserver/utils/stats/AbyssRankEnum.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::items {
namespace {

using model::gameobjects::player::AbyssRank;
using serverpackets::SM_SYSTEM_MESSAGE;
using services::abyss::AbyssPointsService;
using services::abyss::GloryPointsService;
using utils::stats::AbyssRankEnum;

// ServerPacketsOpcodes.java:154, :255
constexpr int32_t SM_ABYSS_RANK_UPDATE_OPCODE = 136;
constexpr int32_t SM_ABYSS_RANK_OPCODE = 237;

const char* ABYSS_RANK_DAO_LOGGER = "com.aionemu.gameserver.dao.AbyssRankDAO";

// ServerPacketsOpcodes.java:62-63
constexpr int32_t SM_SKILL_LIST_OPCODE = 44;
constexpr int32_t SM_SKILL_REMOVE_OPCODE = 45;

constexpr int32_t RANK_LIMIT_TEST_SWORD = 100001319; // <uselimits rank_min="5" rank_max="9"/>: GRADE5_SOLDIER to GRADE1_SOLDIER
constexpr int64_t MAIN_HAND = 1;                     // ItemSlot.MAIN_HAND.getSlotIdMask()

// AbyssSkillService.java: AbyssSkills.STAR5_OFFICER (Race.ELYOS, AbyssRankEnum.STAR5_OFFICER, 11885, 11895)
constexpr int32_t GUARDIAN_GENERAL_TRANSFORMATION = 11885;
constexpr int32_t ABYSSAL_FURY = 11895;

/** items/item_templates.xml, verbatim row */
constexpr std::string_view RANK_LIMIT_TEST_SWORD_ROW = R"xml(
	<!-- :10245 -->
	<item_template id="100001319" name="Abyss Rank Limit Test Sword_01" level="50" cName="ranktest_sword_a_u0_50_01" mask="138316" item_group="SWORD" quality="UNIQUE" price="1163100" restrict="50 50 50 50 50 50 50 50 50 50 50 50 50 50 50 50 50" desc="793785" attack_type="PHYSICAL" max_enchant="15" m_slots="4">
		<weapon_stats hit_count="2" attack_range="1500" magical_accuracy="244" parry="823" physical_accuracy="764" critical="50" attack_speed="1400" max_damage="178" min_damage="144"/>
		<disposition id="188950002" count="6"/>
		<uselimits rank_min="5" rank_max="9" purchable_rank_min="5"/>
		<idian burn_attack="29" burn_defend="12"/>
	</item_template>
)xml";

/** skills/skill_templates.xml, verbatim rows */
constexpr std::string_view ABYSS_SKILL_ROWS = R"xml(
	<!-- :112384 -->
	<skill_template skill_id="11885" name="Transformation: Guardian General I" nameId="288673" stack="ABYSS_RANKERSKILL_LIGHT_AVATAR" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF" tslot="BUFF" tslot_level="2" dispel_category="BUFF" req_dispel_level="3" req_dispel_count="100" activation="ACTIVE" cooldown="72000" duration="6000" avatar="true" apply_magical_skill_boost_bonus="true">
		<properties first_target="ME" first_target_range="2" target_relation="FRIEND" target_type="ONLYONE" target_maxcount="1" />
		<useconditions>
			<move_casting allow="false" />
		</useconditions>
		<effects>
			<skilllauncher skill_id="11912" value="1" e="1" noresist="true" />
			<shapechange model="202502" type="AVATAR" duration2="600000" effectid="175" e="2" basiclvl="200" noresist="true" preeffect="1" />
			<statup maxstat="true" duration2="600000" effectid="197373" e="3" basiclvl="200" noresist="true" preeffect="1 2">
				<change stat="SPEED" func="ADD" value="1000" />
				<change stat="FLY_SPEED" func="ADD" value="1000" />
			</statup>
		</effects>
		<actions>
			<itemuse itemid="169300012" count="42" />
		</actions>
		<motion name="phburst" />
	</skill_template>
	<!-- :112564 -->
	<skill_template skill_id="11895" name="Abyssal Fury I" nameId="288694" stack="ABYSS_RANKERSKILL_ABYSSIANSTROM" lvl="1" skilltype="MAGICAL" skillsubtype="ATTACK" tslot="NONE" activation="ACTIVE" cooldown="200" duration="0" apply_magical_skill_boost_bonus="true">
		<properties first_target="ME" first_target_range="2" target_relation="ENEMY" target_type="AREA" target_maxcount="6" effective_altitude="4" effective_range="20" />
		<startconditions>
			<form value="AVATAR" />
		</startconditions>
		<endconditions>
			<mp value="10200" delta="0" />
		</endconditions>
		<effects>
			<skillatk value="1000" e="1" noresist="true" accmod2="0" />
		</effects>
		<motion name="asdpatk" delay="200" />
	</skill_template>
)xml";

/** `base` with `rows` inserted before its closing tag `closing` */
std::string withRows(std::string_view base, std::string_view closing, std::string_view rows) {
	std::string xml(base);
	xml.insert(xml.rfind(closing), rows);
	return xml;
}

runtime::Ptr<model::house::House> noHouse(model::gameobjects::player::Player&) {
	return nullptr;
}

runtime::Ptr<services::conquerorAndProtectorSystem::CPInfo> noCpInfo(model::gameobjects::player::Player&) {
	return nullptr;
}

/** The rank values SM_ABYSS_RANK writes, of a character without kills and without a ranking position */
struct RankValues {
	int64_t ap = 0;
	int32_t gp = 0;
	int32_t rankId = 1;
	int32_t maxRank = 1;
	int64_t dailyAp = 0;
	int32_t dailyGp = 0;
	int64_t weeklyAp = 0;
	int32_t weeklyGp = 0;
	int32_t position = 0;
};

/**
 * SM_ABYSS_RANK.writeImpl: Q ap, D gp, D rank id, D ranking position, D 0, D all kills, D max rank, D daily kills, Q daily ap, D daily gp,
 * D weekly kills, Q weekly ap, D weekly gp, D last kills, Q last ap, D last gp, C 0
 */
std::vector<uint8_t> abyssRank(const RankValues& r) {
	return javaPacket(SM_ABYSS_RANK_OPCODE, PacketWriter()
												.Q(r.ap)
												.D(r.gp)
												.D(r.rankId)
												.D(r.position)
												.D(0)
												.D(0)
												.D(r.maxRank)
												.D(0)
												.Q(r.dailyAp)
												.D(r.dailyGp)
												.D(0)
												.Q(r.weeklyAp)
												.D(r.weeklyGp)
												.D(0)
												.Q(0)
												.D(0)
												.C(0));
}

/** SM_ABYSS_RANK_UPDATE(0, player).writeImpl: C action 0, D object id, D rank id */
std::vector<uint8_t> abyssRankUpdate(int32_t objectId, int32_t rankId) {
	return javaPacket(SM_ABYSS_RANK_UPDATE_OPCODE, PacketWriter().C(0).D(objectId).D(rankId));
}

class AbyssAndGloryPointsServiceTest : public ItemPacketTest {
protected:
	void SetUp() override {
		// the world holders are published once per process, P4-10's test set first (the fixture of ClassChangeServiceTest)
		ASSERT_TRUE(world::test::publishTestStaticData()) << "this process published the real static data";
		ItemPacketTest::SetUp();
		// the reads of SM_PLAYER_INFO (the watcher's see) that need services this test has not got
		lookups.activeHouseOfPlayer = &noHouse;
		lookups.cpInfoForCurrentMap = &noCpInfo;
		serverpackets::detail::setPacketLookupsForTests(&lookups);
		// Java's default (RankingConfig.java:28, gameserver.topranking.xform.min_rank = STAR5_OFFICER): no soldier learns an abyss skill
		savedXformMinRank = configs::main::RankingConfig::XFORM_MIN_RANK.exchange(configs::detail::AbyssRankEnum::STAR5_OFFICER);
	}

	void TearDown() override {
		configs::main::RankingConfig::XFORM_MIN_RANK.store(savedXformMinRank);
		configs::main::CustomConfig::ENABLE_AP_CAP.store(false);
		configs::main::CustomConfig::AP_CAP_VALUE.store(0);
		if (watcher.player)
			watcher.player->setClientConnection(nullptr);
		watcherClient.reset();
		watcher = {};
		if (inWorld)
			world::World::getInstance().removeObject(*f.player);
		ItemPacketTest::TearDown();
		serverpackets::detail::setPacketLookupsForTests(nullptr);
		if (skillTreePublished)
			dataholders::DataManager::SKILL_TREE_DATA.resetForTests();
	}

	/** ItemPacketTest's item rows again, with the rank-limited sword */
	void addRankLimitedSword() {
		xml::LoadContext context;
		dataholders::DataManager::ITEM_DATA.resetForTests();
		dataholders::DataManager::ITEM_DATA.publish(
			xml::bindString<dataholders::ItemData>(context, withRows(ITEM_TEMPLATES_XML, "</item_templates>", RANK_LIMIT_TEST_SWORD_ROW)));
	}

	/** ItemPacketTest's skill rows again, with the two abyss skills; PlayerSkillList.addSkill asks the skill tree (no rows needed) */
	void addAbyssSkills() {
		xml::LoadContext context;
		dataholders::DataManager::SKILL_DATA.resetForTests();
		dataholders::DataManager::SKILL_DATA.publish(
			xml::bindString<dataholders::SkillData>(context, withRows(SKILL_TEMPLATES_XML, "</skill_data>", ABYSS_SKILL_ROWS)));
		if (!dataholders::DataManager::SKILL_TREE_DATA) {
			dataholders::DataManager::SKILL_TREE_DATA.publish(xml::bindString<dataholders::SkillTreeData>(context, "<skill_tree/>"));
			skillTreePublished = true;
		}
	}

	bool hasSkill(int32_t skillId) { return player().getSkillList()->isSkillPresent(skillId); }

	/** The abyss rank as AbyssRankDAO.loadAbyssRank builds it: AbyssRank(dailyAP, weeklyAP, ap, rank, ..., maxRank, ..., gp, lastGp) */
	AbyssRank& holdRank(int32_t ap, int32_t rankId, int32_t gp = 0) {
		player().setAbyssRank(AbyssRank::create(0, 0, ap, rankId, 0, 0, 0, rankId, 0, 0, 0, 0, 0, gp, 0));
		return *player().getAbyssRank();
	}

	/** The player stored in the World, where GloryPointsService finds him by his object id (World.getPlayer) */
	void storeInWorld() {
		world::World::getInstance().storeObject(*f.player);
		inWorld = true;
	}

	/** A second character 3 m from the player, in his known list, with a connection of his own (ClassChangeServiceTest's watch()) */
	void watch() {
		watcher = makePlayer(710102, 9902, "Watcher");
		watcher.player->setMotions(std::make_unique<model::gameobjects::player::motion::MotionList>(*watcher.player));
		watcher.player->setPosition(
			world::WorldPosition::create(210010000, 103.0f, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(103.0f, 100.0f, 50.0f)));
		watcher.player->getPosition()->setIsSpawned(true);
		watcherClient = std::make_unique<TestClient>();
		watcherClient->enterWorld(watcher);
		ASSERT_TRUE(f.knownList().addForTest(*watcher.player));
		ASSERT_EQ(knownSeeNotifiesFailed(), 0u) << "the player's see of the watcher ran to its end";
		clearSent();
		(*watcherClient)->clearSent();
	}

	std::vector<std::vector<uint8_t>> watcherSent() { return (*watcherClient)->sentBytes(); }

	std::vector<uint8_t> message(SM_SYSTEM_MESSAGE&& packet) { return serializedFor(std::move(packet)); }

	serverpackets::detail::PacketLookupsForTests lookups{};
	configs::detail::AbyssRankEnum savedXformMinRank{};
	PlayerFixture watcher;
	std::unique_ptr<TestClient> watcherClient;
	bool inWorld = false;
	bool skillTreePublished = false;
};

// ---- AbyssPointsService.addAp ---------------------------------------------------------------------------------------------------------------

TEST_F(AbyssAndGloryPointsServiceTest, AnApGainIsAddedToTheRankAndReportedWithTheRank) {
	AbyssRank& rank = holdRank(0, 1);
	watch();

	AbyssPointsService::addAp(player(), 400);

	// AbyssRank.addAp: 400 AP, 400 of the day and the week; still GRADE9_SOLDIER (GRADE8 wants 1,200)
	EXPECT_EQ(rank.getAp(), 400);
	EXPECT_EQ(rank.getDailyAP(), 400);
	EXPECT_EQ(rank.getWeeklyAP(), 400);
	EXPECT_EQ(rank.getRank(), AbyssRankEnum::GRADE9_SOLDIER);
	// :46-48: STR_MSG_COMBAT_MY_ABYSS_POINT_GAIN(added), then SM_ABYSS_RANK (the points moved, the rank did not)
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_COMBAT_MY_ABYSS_POINT_GAIN(400)),
						  abyssRank({.ap = 400, .dailyAp = 400, .weeklyAp = 400})}));
	EXPECT_TRUE(watcherSent().empty()) << "no rank change, nothing broadcast";
}

TEST_F(AbyssAndGloryPointsServiceTest, CrossingARankThresholdBroadcastsTheNewRank) {
	AbyssRank& rank = holdRank(1000, 1);
	watch();

	AbyssPointsService::addAp(player(), 300);

	// 1,300 AP reach GRADE8_SOLDIER (1,200, AbyssRankEnum.java:16), id 2, which is also the new maximum
	EXPECT_EQ(rank.getAp(), 1300);
	EXPECT_EQ(rank.getRank(), AbyssRankEnum::GRADE8_SOLDIER);
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_COMBAT_MY_ABYSS_POINT_GAIN(300)),
						  abyssRank({.ap = 1300, .rankId = 2, .maxRank = 2, .dailyAp = 300, .weeklyAp = 300})}));
	// :58-59: SM_ABYSS_RANK_UPDATE(0, player) to the players who see him
	EXPECT_EQ(watcherSent(), exactly({abyssRankUpdate(player().getObjectId(), 2)}));
}

TEST_F(AbyssAndGloryPointsServiceTest, SpendingApSendsTheUseMessageAndCanLowerTheRank) {
	// the TradeService case: an abyss vendor takes 70,350 AP (TradeService.java:130-131). 70,350 AP were GRADE3_SOLDIER (69,700, id 7)
	AbyssRank& rank = holdRank(70350, 7);
	watch();

	AbyssPointsService::addAp(player(), -70350);

	// AbyssRank.addAp adds a loss to neither the day nor the week; 0 AP are GRADE9_SOLDIER again, the maximum stays 7
	EXPECT_EQ(rank.getAp(), 0);
	EXPECT_EQ(rank.getDailyAP(), 0);
	EXPECT_EQ(rank.getRank(), AbyssRankEnum::GRADE9_SOLDIER);
	// :46: amount < 0 -> STR_MSG_USE_ABYSSPOINT(-added)
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_USE_ABYSSPOINT(70350)), abyssRank({.rankId = 1, .maxRank = 7})}));
	EXPECT_EQ(watcherSent(), exactly({abyssRankUpdate(player().getObjectId(), 1)}));
}

TEST_F(AbyssAndGloryPointsServiceTest, TheMessageNamesThePointsActuallyAdded) {
	// the AP cap (CustomConfig gameserver.ap.cap.enable / value): 90 AP with a cap of 100 take 10 of a gain of 50
	configs::main::CustomConfig::ENABLE_AP_CAP.store(true);
	configs::main::CustomConfig::AP_CAP_VALUE.store(100);
	AbyssRank& rank = holdRank(90, 1);

	AbyssPointsService::addAp(player(), 50);

	EXPECT_EQ(rank.getAp(), 100);
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_COMBAT_MY_ABYSS_POINT_GAIN(10)),
						  abyssRank({.ap = 100, .dailyAp = 50, .weeklyAp = 50})}))
		<< ":44: added = the rank's AP after - before, not the amount";

	// at the cap nothing is added: the gain message says 0 and, since nothing moved, no SM_ABYSS_RANK (:48, :56)
	clearSent();
	AbyssPointsService::addAp(player(), 50);
	EXPECT_EQ(rank.getAp(), 100);
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_COMBAT_MY_ABYSS_POINT_GAIN(0))}));
}

TEST_F(AbyssAndGloryPointsServiceTest, AnAmountOfZeroIsAGainOfNothing) {
	AbyssRank& rank = holdRank(500, 1);

	AbyssPointsService::addAp(player(), 0);

	// :46: amount >= 0, so the gain message (of the 0 points added), not STR_MSG_USE_ABYSSPOINT; nothing moved, so no SM_ABYSS_RANK (:48, :56)
	EXPECT_EQ(rank.getAp(), 500);
	EXPECT_EQ(rank.getDailyAP(), 0);
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_COMBAT_MY_ABYSS_POINT_GAIN(0))}));
}

TEST_F(AbyssAndGloryPointsServiceTest, TheThreeArgumentAddApSendsTheCallersGainMessageAndIgnoresANullPlayer) {
	AbyssRank& rank = holdRank(0, 1);
	std::vector<int32_t> asked;
	auto gainMessage = [&asked](int32_t added) {
		asked.push_back(added);
		return SM_SYSTEM_MESSAGE::STR_MSG_GLORY_POINT_GAIN(added); // any message of one number: the caller's choice
	};

	AbyssPointsService::addAp(runtime::Ptr<model::gameobjects::player::Player>(player()), 25, gainMessage);

	EXPECT_EQ(rank.getAp(), 25);
	EXPECT_EQ(asked, (std::vector<int32_t>{25}));
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_GLORY_POINT_GAIN(25)), abyssRank({.ap = 25, .dailyAp = 25, .weeklyAp = 25})}));

	// :38-39
	clearSent();
	EXPECT_NO_THROW(AbyssPointsService::addAp(nullptr, 25, gainMessage));
	EXPECT_EQ(asked.size(), 1u);
	EXPECT_EQ(rank.getAp(), 25);
	EXPECT_TRUE(sent().empty());
}

TEST_F(AbyssAndGloryPointsServiceTest, OnRankChangedSendsTheRankForANewRankingPositionAlone) {
	holdRank(500, 1);
	watch();

	// :56-57: a ranking position alone sends SM_ABYSS_RANK with it; nothing is broadcast
	AbyssPointsService::onRankChanged(player(), false, false, 12);
	EXPECT_EQ(sent(), exactly({abyssRank({.ap = 500, .position = 12})}));

	// nothing changed: nothing sent
	clearSent();
	AbyssPointsService::onRankChanged(player(), false, false, std::nullopt);
	EXPECT_TRUE(sent().empty());
	EXPECT_TRUE(watcherSent().empty());
}

TEST_F(AbyssAndGloryPointsServiceTest, ARankChangeUnequipsWhatTheNewRankMayNotWear) {
	addRankLimitedSword();
	AbyssRank& rank = holdRank(70350, 7); // GRADE3_SOLDIER
	Item& sword = equipped(720101, RANK_LIMIT_TEST_SWORD, MAIN_HAND);
	ASSERT_TRUE(sword.isEquipped());
	const std::vector<uint8_t> unequipMessage = message(SM_SYSTEM_MESSAGE::STR_MSG_UNEQUIP_RANKITEM(sword.getL10n()));

	// up to GRADE2_SOLDIER (105,600 AP, id 8): still within the sword's ranks 5 to 9 (Equipment.verifyRankLimits, ItemUseLimits.verifyRank)
	AbyssPointsService::addAp(player(), 35250);
	EXPECT_EQ(rank.getRank(), AbyssRankEnum::GRADE2_SOLDIER);
	EXPECT_TRUE(sword.isEquipped());
	EXPECT_EQ(std::ranges::count(sent(), unequipMessage), 0);

	// down to GRADE9_SOLDIER (id 1): :60, Equipment.checkRankLimitItems takes the sword off into the cube (unEquipItem(id, false)) and says
	// so; :61 has no abyss skill to take from or give to a soldier, so the message is the last packet
	clearSent();
	AbyssPointsService::addAp(player(), -105600);
	EXPECT_EQ(rank.getRank(), AbyssRankEnum::GRADE9_SOLDIER);
	EXPECT_FALSE(sword.isEquipped());
	EXPECT_EQ(storage(StorageType::CUBE).getItemByObjId(720101).get(), &sword);
	const std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_GE(packets.size(), 3u) << ::testing::PrintToString(opcodesOf(packets));
	EXPECT_EQ(packets[0], message(SM_SYSTEM_MESSAGE::STR_MSG_USE_ABYSSPOINT(105600)));
	EXPECT_EQ(packets[1], abyssRank({.rankId = 1, .maxRank = 8, .dailyAp = 35250, .weeklyAp = 35250}));
	EXPECT_EQ(packets.back(), unequipMessage);
	EXPECT_EQ(std::ranges::count(packets, unequipMessage), 1);
}

TEST_F(AbyssAndGloryPointsServiceTest, AnOfficersAbyssSkillsComeAndGoWithTheRank) {
	addAbyssSkills();
	// an Elyos STAR5_OFFICER (id 14) whose glory points are not an officer's: GP 0
	AbyssRank& rank = holdRank(160000, 14);
	ASSERT_FALSE(hasSkill(GUARDIAN_GENERAL_TRANSFORMATION));
	ASSERT_FALSE(hasSkill(ABYSSAL_FURY));

	// :58-61 for a new rank at RankingConfig.XFORM_MIN_RANK (STAR5_OFFICER, Java's default, the fixture's): AbyssSkillService.updateSkills
	// takes the race's abyss skills (none held) and gives the rank's two as temporary skills (SkillLearnService.learnTemporarySkill: a new
	// skill, SM_SKILL_LIST each)
	AbyssPointsService::onRankChanged(player(), false, true, std::nullopt);
	EXPECT_TRUE(hasSkill(GUARDIAN_GENERAL_TRANSFORMATION));
	EXPECT_TRUE(hasSkill(ABYSSAL_FURY));
	EXPECT_EQ(packetsOf(sent(), SM_SKILL_LIST_OPCODE).size(), 2u) << ::testing::PrintToString(opcodesOf(sent()));
	EXPECT_TRUE(packetsOf(sent(), SM_SKILL_REMOVE_OPCODE).empty());

	// any AP change sets the soldier rank of his AP, since he has not the GP of an officer's rank (AbyssRank.addAp: getRankForPoints(159,999,
	// 0) is GRADE1_SOLDIER, id 9, which wants no GP): the rank changed, and updateSkills takes both skills and gives none below the minimum
	clearSent();
	AbyssPointsService::addAp(player(), -1);
	EXPECT_EQ(rank.getRank(), AbyssRankEnum::GRADE1_SOLDIER);
	EXPECT_FALSE(hasSkill(GUARDIAN_GENERAL_TRANSFORMATION));
	EXPECT_FALSE(hasSkill(ABYSSAL_FURY));
	EXPECT_EQ(packetsOf(sent(), SM_SKILL_REMOVE_OPCODE).size(), 2u) << ::testing::PrintToString(opcodesOf(sent()));
	EXPECT_TRUE(packetsOf(sent(), SM_SKILL_LIST_OPCODE).empty());
}

// ---- GloryPointsService.addGp ---------------------------------------------------------------------------------------------------------------

TEST_F(AbyssAndGloryPointsServiceTest, AGpGainOfAnOnlinePlayerIsAddedAndReported) {
	AbyssRank& rank = holdRank(0, 1);
	storeInWorld();

	GloryPointsService::addGp(player().getObjectId(), 100);

	// AbyssRank.addGp(100, true): 100 GP, 100 of the day and the week; :30-33
	EXPECT_EQ(rank.getCurrentGP(), 100);
	EXPECT_EQ(rank.getDailyGP(), 100);
	EXPECT_EQ(rank.getWeeklyGP(), 100);
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_GLORY_POINT_GAIN(100)), abyssRank({.gp = 100, .dailyGp = 100, .weeklyGp = 100})}));
}

TEST_F(AbyssAndGloryPointsServiceTest, AGpLossIsReportedAsTheLossActuallyTaken) {
	AbyssRank& rank = holdRank(0, 1, 100);
	storeInWorld();

	GloryPointsService::addGp(player().getObjectId(), -30);

	// addToStats is false for a loss (:22): the day and the week keep 0; STR_MSG_GLORY_POINT_LOSE(-added)
	EXPECT_EQ(rank.getCurrentGP(), 70);
	EXPECT_EQ(rank.getDailyGP(), 0);
	EXPECT_EQ(rank.getWeeklyGP(), 0);
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_GLORY_POINT_LOSE(30)), abyssRank({.gp = 70})}));

	// a loss larger than the GP: AbyssRank.addGp stops at 0, so 70 are lost; then a loss of nothing reports 0 and sends no rank (:32)
	clearSent();
	GloryPointsService::addGp(player().getObjectId(), -500);
	EXPECT_EQ(rank.getCurrentGP(), 0);
	clearSent();
	GloryPointsService::addGp(player().getObjectId(), -5);
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_GLORY_POINT_LOSE(0))}));
}

TEST_F(AbyssAndGloryPointsServiceTest, NoGpChangesNothingAndAnOfflinePlayersGpGoesToTheDatabase) {
	AbyssRank& rank = holdRank(0, 1, 40);
	storeInWorld();
	network::test::LogCapture capture({ABYSS_RANK_DAO_LOGGER});

	// :19-20
	GloryPointsService::addGp(player().getObjectId(), 0);
	EXPECT_EQ(rank.getCurrentGP(), 40);
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(capture.count("GP for player"), 0) << capture.dump();

	// :23-24: World.getPlayer answers null for a character that is not online -> AbyssRankDAO.addGp(id, amount, amount > 0), which has no
	// database here and logs it
	GloryPointsService::addGp(777777, 25);
	EXPECT_TRUE(capture.contains("Couldn't increase 25 GP for player 777777")) << capture.dump();
	EXPECT_EQ(rank.getCurrentGP(), 40);
	EXPECT_TRUE(sent().empty());
}

// The last case of the file: it initializes DatabaseFactory for its process (ctest runs every case in its own; a run of the whole executable
// reaches it after the case above, which wants no database)
TEST_F(AbyssAndGloryPointsServiceTest, AnOfflinePlayersGpCountsForTheDayAndTheWeekOnlyWhenItIsAGain) {
	if (!dao::test::isEnabled())
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL to run the database tests";
	dao::test::setUpDatabaseOnce();
	dao::test::clearTables();
	constexpr int32_t OFFLINE_ID = 777777;
	dao::test::insertPlayer(OFFLINE_ID, "Offline", 9977);
	// the columns AbyssRankDAO.storeAbyssRank writes for a new rank (aion_gs.sql: every NOT NULL column without a default), 40 GP
	dao::test::execute("INSERT INTO abyss_rank (player_id, daily_ap, weekly_ap, ap, daily_kill, weekly_kill, last_kill, last_ap, last_update, gp) "
					   "VALUES (777777, 0, 0, 0, 0, 0, 0, 0, 0, 40)");
	const std::string where = " FROM abyss_rank WHERE player_id = " + std::to_string(OFFLINE_ID);
	auto column = [&where](std::string_view name) { return dao::test::queryLong("SELECT " + std::string(name) + where); };

	// :21-24: not online, so AbyssRankDAO.addGp(id, amount, addToStats = amount > 0): a gain also counts for the day and the week
	// (INCREASE_GP_QUERY_WITH_STATS)
	GloryPointsService::addGp(OFFLINE_ID, 25);
	EXPECT_EQ(column("gp"), 65);
	EXPECT_EQ(column("daily_gp"), 25);
	EXPECT_EQ(column("weekly_gp"), 25);

	// a loss does not (INCREASE_GP_QUERY, which also stops at 0)
	GloryPointsService::addGp(OFFLINE_ID, -10);
	EXPECT_EQ(column("gp"), 55);
	EXPECT_EQ(column("daily_gp"), 25);
	EXPECT_EQ(column("weekly_gp"), 25);
	GloryPointsService::addGp(OFFLINE_ID, -100);
	EXPECT_EQ(column("gp"), 0);
	EXPECT_EQ(column("daily_gp"), 25);
	EXPECT_TRUE(sent().empty()) << "no one online to tell";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::items
