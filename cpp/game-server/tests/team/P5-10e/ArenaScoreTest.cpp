// M5j stage 4 CP3 (J13, m5j-plan.md §18.5): the PvP arena model - InstanceBuff, PvPArenaPlayerReward, HarmonyGroupReward, PvPArenaScore,
// HarmonyArenaScore - their score writers (ArenaScoreWriter, HarmonyScoreWriter) and the arms of AutoPvPFFAInstance and AutoHarmonyInstance
// they complete. The arena maps are map instances of instance_bonusattr-free test world templates (the ids of WorldMapType); the expected
// bytes are written from the Java writeMe methods.

#include "AutoGroupTestSupport.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "../../support/NetworkTestSupport.h"
#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/dataholders/InstanceBuffData.bind.h"
#include "aion/gameserver/dataholders/InstanceBuffData.h"
#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/instance/InstanceBuff.h"
#include "aion/gameserver/model/instance/InstanceScoreType.h"
#include "aion/gameserver/model/instance/instancescore/HarmonyArenaScore.h"
#include "aion/gameserver/model/instance/instancescore/PvPArenaScore.h"
#include "aion/gameserver/model/instance/playerreward/HarmonyGroupReward.h"
#include "aion/gameserver/model/instance/playerreward/PvPArenaPlayerReward.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/instance/instancescore/DarkPoetaScore.h"
#include "aion/gameserver/model/instance/instancescore/LegionDominionScore.h"
#include "aion/gameserver/model/instance/instancescore/NormalScore.h"
#include "aion/gameserver/model/instance/instancescore/PvpInstanceScore.h"
#include "aion/gameserver/model/instance/playerreward/CruciblePlayerReward.h"
#include "aion/gameserver/model/instance/playerreward/PvpInstancePlayerReward.h"
#include "aion/gameserver/model/templates/rewards/ArenaRewardItem.h"
#include "aion/gameserver/model/templates/rewards/RewardItem.h"
#include "aion/gameserver/network/aion/instanceinfo/ArenaScoreWriter.h"
#include "aion/gameserver/network/aion/instanceinfo/CrucibleScoreWriter.h"
#include "aion/gameserver/network/aion/instanceinfo/DarkPoetaScoreWriter.h"
#include "aion/gameserver/network/aion/instanceinfo/EternalBastionScoreWriter.h"
#include "aion/gameserver/network/aion/instanceinfo/HarmonyScoreWriter.h"
#include "aion/gameserver/network/aion/instanceinfo/LegionDominionScoreWriter.h"
#include "aion/gameserver/network/aion/instanceinfo/PvpInstanceScoreWriter.h"
#include "aion/gameserver/network/aion/instanceinfo/TheShugoEmperorsVaultScoreWriter.h"
#include "aion/gameserver/utils/stats/AbyssRankEnumInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABNORMAL_STATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_SCORE.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMap2DInstance.h"

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

namespace instancescore = model::instance::instancescore;
namespace playerreward = model::instance::playerreward;
using model::instance::InstanceScoreType;

constexpr int32_t ARENA_OF_CHAOS = 300350000;
constexpr int32_t ARENA_OF_DISCIPLINE = 300360000;
constexpr int32_t ARENA_OF_HARMONY = 300450000;

/** instance_bonusattr/instance_bonusattr.xml:40-51, verbatim */
constexpr const char* BONUS_ATTR_XML = R"(<instance_bonusattrs>
	<instance_bonusattr buff_id="7">
		<penalty_attr stat="PVP_DEFEND_RATIO" func="ADD" value="9999"/>
		<penalty_attr stat="SPEED" func="PERCENT" value="50"/>
		<penalty_attr stat="MAGICAL_RESIST" func="ADD" value="9999"/>
		<penalty_attr stat="ABNORMAL_RESISTANCE_ALL" func="ADD" value="9999"/>
	</instance_bonusattr>
	<instance_bonusattr buff_id="8">
		<penalty_attr stat="PVP_DEFEND_RATIO" func="ADD" value="9999"/>
		<penalty_attr stat="SPEED" func="PERCENT" value="50"/>
		<penalty_attr stat="MAGICAL_RESIST" func="ADD" value="9999"/>
		<penalty_attr stat="ABNORMAL_RESISTANCE_ALL" func="ADD" value="9999"/>
	</instance_bonusattr>
</instance_bonusattrs>)";

/** the shared Poeta row (ItemPacketTestSupport.h, world_maps.xml:11 reduced), character for character */
constexpr const char* POETA_ROW = R"(<map id="210010000" cName="LF1" name="Poeta" name_id="1" water_level="16" death_level="0")"
	R"( world_type="ELYSEA" world_size="1024" flags="FLY GLIDE RECALL"/>)";

/**
 * test world templates with the arena map ids (the attributes a map instance reads, as the Poeta row). A map instance reads its zones
 * through DataManager.WORLD_MAPS_DATA (ZoneService), so the fixture publishes them beside Poeta and restores the Poeta-only holder after.
 */
constexpr const char* ARENA_MAPS_XML = R"(<world_maps>)"
	R"(<map id="300350000" cName="IDArena" name="Arena of Chaos" name_id="1" water_level="16" death_level="0" world_type="ELYSEA" world_size="1024" flags="FLY"/>)"
	R"(<map id="300360000" cName="IDArena_Solo" name="Arena of Discipline" name_id="1" water_level="16" death_level="0" world_type="ELYSEA" world_size="1024" flags="FLY"/>)"
	R"(<map id="300450000" cName="IDArena_Team01" name="Arena of Harmony" name_id="1" water_level="16" death_level="0" world_type="ELYSEA" world_size="1024" flags="FLY"/>)"
	R"(</world_maps>)";

/** An instance script whose score the test sets (an arena's handler creates its score in onInstanceCreate) */
class ArenaTestHandler final : public ::aion::gameserver::instance::handlers::GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
public:
	explicit ArenaTestHandler(world::WorldMapInstance& instance) : GeneralInstanceHandler(instance) {}

	static runtime::Ref<ArenaTestHandler> create(world::WorldMapInstance& instance) { return runtime::makeRef<ArenaTestHandler>(instance); }

	runtime::Ptr<instancescore::InstanceScore> getInstanceScore() override { return score; }

	runtime::Ref<instancescore::InstanceScore> score;

protected:
	~ArenaTestHandler() override = default;
};

class ArenaScoreTest : public AutoGroupTest {
protected:
	void SetUp() override {
		AutoGroupTest::SetUp();
		dataholders::DataManager::INSTANCE_BUFF_DATA.publish(xml::bindString<dataholders::InstanceBuffData>(arenaContext, BONUS_ATTR_XML));
		std::string maps(ARENA_MAPS_XML);
		maps.insert(maps.rfind("</world_maps>"), POETA_ROW);
		dataholders::DataManager::WORLD_MAPS_DATA.resetForTests();
		dataholders::DataManager::WORLD_MAPS_DATA.publish(xml::bindString<dataholders::WorldMapsData>(arenaContext, maps));
	}

	void TearDown() override {
		arenaInstances.clear();
		arenaWorldMaps.clear();
		AutoGroupTest::TearDown();
		dataholders::DataManager::INSTANCE_BUFF_DATA.resetForTests();
		static auto* const poetaContexts = new std::deque<xml::LoadContext>(); // the restored holder outlives the fixture
		dataholders::DataManager::WORLD_MAPS_DATA.resetForTests();
		dataholders::DataManager::WORLD_MAPS_DATA.publish(
			xml::bindString<dataholders::WorldMapsData>(poetaContexts->emplace_back(), std::string("<world_maps>") + POETA_ROW + "</world_maps>"));
	}

	/** an instance of the arena map with the test handler */
	world::WorldMapInstance& arena(int32_t mapId) {
		runtime::Ref<world::WorldMap>& worldMap = arenaWorldMaps.emplace_back(world::WorldMap::create(dataholders::DataManager::WORLD_MAPS_DATA->getTemplate(mapId)));
		runtime::Ref<world::WorldMapInstance>& instance = arenaInstances.emplace_back(world::WorldMap2DInstance::create(*worldMap, 1, 0, 0,
			[](world::WorldMapInstance& i) { return runtime::Ref<::aion::gameserver::instance::handlers::InstanceHandler>(ArenaTestHandler::create(i)); }));
		return *instance;
	}

	static ArenaTestHandler& handlerOf(world::WorldMapInstance& instance) {
		return static_cast<ArenaTestHandler&>(*instance.getInstanceHandler());
	}

	static std::vector<uint8_t> written(instanceinfo::InstanceScoreWriter& writer) {
		commons::utils::ByteBuffer buf = commons::utils::ByteBuffer::allocate(8192);
		writer.writeMe(buf);
		buf.flip();
		return std::vector<uint8_t>(buf.remainingSpan().begin(), buf.remainingSpan().end());
	}

	/** writeS(buf, text, size): UTF-16LE characters, then zero bytes up to size */
	static void writeS(network::test::PacketWriter& w, std::string_view text, int32_t size) {
		std::u16string utf16 = commons::utils::StringUtils::toUtf16(text);
		for (char16_t c : utf16)
			w.H(c);
		w.zeros(static_cast<size_t>(size) - utf16.size() * 2);
	}

	xml::LoadContext arenaContext;
	std::vector<runtime::Ref<world::WorldMap>> arenaWorldMaps;
	std::vector<runtime::Ref<world::WorldMapInstance>> arenaInstances;
};

/** PvPArenaPlayerReward.java:31-164: the start values, the score caps, the time bonus, the boost morale buff */
TEST_F(ArenaScoreTest, PlayerReward) {
	Member& a = addMember("Arenaa");
	runtime::Ref<playerreward::PvPArenaPlayerReward> reward = playerreward::PvPArenaPlayerReward::create(a.player(), 12000, int8_t{7});
	EXPECT_EQ(reward->getOwnerId(), a.player().getObjectId());
	EXPECT_EQ(reward->getPoints(), 13000) << "super.setPoints(13000)";
	EXPECT_EQ(reward->getPlayerClass(), a.player().getPlayerClass());
	EXPECT_EQ(reward->getPlayerName(), "Arenaa");
	EXPECT_EQ(reward->getTimeBonus(), 12000);
	EXPECT_EQ(reward->getScorePoints(), 25000);
	EXPECT_FLOAT_EQ(reward->getParticipation(), 1.0f);
	EXPECT_EQ(reward->getAp()->itemId(), 0);
	EXPECT_EQ(reward->getCourageInsignia()->getTotalCount(), 0);
	EXPECT_EQ(reward->getRewardItem1(), nullptr);
	EXPECT_FALSE(reward->isRewarded());
	reward->setRewarded();
	EXPECT_TRUE(reward->isRewarded());
	EXPECT_FALSE(reward->hasBoostMorale());
	EXPECT_EQ(reward->getRemainingTime(), 0);

	runtime::Ref<instancescore::PvPArenaScore> score = instancescore::PvPArenaScore::create(arena(ARENA_OF_CHAOS));
	score->setLowerScoreCap(12500);
	score->setUpperScoreCap(14000);
	reward->addPoints(5000, *score);
	EXPECT_EQ(reward->getPoints(), 14000) << "the upper cap";
	reward->addPoints(-5000, *score);
	EXPECT_EQ(reward->getPoints(), 12500) << "the lower cap";
	reward->addPoints(700);
	EXPECT_EQ(reward->getPoints(), 13200) << "InstancePlayerReward.addPoints: no cap";

	runtime::Ref<playerreward::PvPArenaPlayerReward> groupEntry = playerreward::PvPArenaPlayerReward::create(4711, 12000, int8_t{7});
	EXPECT_EQ(groupEntry->getPlayerName(), "");
	EXPECT_THROW(groupEntry->getPlayerClass(), runtime::NullPointerException) << "Java: null, dereferenced by its readers";
	runtime::Ref<playerreward::PvPArenaPlayerReward> noBonus = playerreward::PvPArenaPlayerReward::create(4712, -50, int8_t{7});
	EXPECT_EQ(noBonus->getTimeBonus(), 0) << "Math.max(timeBonus, 0)";
	EXPECT_EQ(noBonus->getScorePoints(), 12950) << "the raw time bonus";
}

/** InstanceBuff.java:38-104: the bonus attribute's penalties as stat functions, ended by the task, the abnormal state sent */
TEST_F(ArenaScoreTest, InstanceBuff) {
	Member& a = addMember("Buffer");
	using model::stats::container::StatEnum;
	const int32_t defendBefore = a.player().getGameStats()->getStat(StatEnum::PVP_DEFEND_RATIO, 0)->getCurrent();
	runtime::Ref<model::instance::InstanceBuff> buff = model::instance::InstanceBuff::create(7);
	EXPECT_FALSE(buff->isActive());
	buff->applyEffect(a.player(), 5000);
	EXPECT_TRUE(buff->isActive());
	EXPECT_GT(buff->getRemainingTime(), 0);
	EXPECT_LE(buff->getRemainingTime(), 5000);
	EXPECT_EQ(a.player().getGameStats()->getStat(StatEnum::PVP_DEFEND_RATIO, 0)->getCurrent(), defendBefore + 9999);
	buff->applyEffect(a.player(), 5000);
	EXPECT_EQ(a.player().getGameStats()->getStat(StatEnum::PVP_DEFEND_RATIO, 0)->getCurrent(), defendBefore + 9999) << "active: no second time";
	a.clearSent();
	executor->advance(std::chrono::milliseconds(5000));
	EXPECT_FALSE(buff->isActive()) << "the task ended it";
	EXPECT_EQ(a.player().getGameStats()->getStat(StatEnum::PVP_DEFEND_RATIO, 0)->getCurrent(), defendBefore);
	EXPECT_EQ(a.count(opcodeOf<serverpackets::SM_ABNORMAL_STATE>), 1) << "notify: no arena score on Poeta, only the abnormal state";

	runtime::Ref<model::instance::InstanceBuff> unknown = model::instance::InstanceBuff::create(99);
	unknown->applyEffect(a.player(), 5000);
	EXPECT_FALSE(unknown->isActive()) << "no bonus attribute: nothing applied";
}

/** PvPArenaScore.java:32-134: the setup per arena, the player rewards and their random start positions */
TEST_F(ArenaScoreTest, ScoreSetupAndPositions) {
	std::vector<Member*> players;
	for (const char* name : {"Solo1", "Solo2", "Solo3", "Solo4", "Solo5"})
		players.push_back(&addMember(name));

	runtime::Ref<instancescore::PvPArenaScore> solo = instancescore::PvPArenaScore::create(arena(ARENA_OF_DISCIPLINE));
	EXPECT_EQ(solo->getBuffId(), 8) << "a solo arena";
	EXPECT_TRUE(solo->isSoloArena(world::WorldMapType::ARENA_OF_DISCIPLINE));
	EXPECT_FALSE(solo->isSoloArena(world::WorldMapType::ARENA_OF_CHAOS));
	EXPECT_TRUE(solo->canReward());
	EXPECT_TRUE(solo->regPlayerReward(players[0]->player()));
	EXPECT_FALSE(solo->regPlayerReward(players[0]->player())) << "registered once";
	EXPECT_EQ(solo->getPlayerReward(players[0]->player().getObjectId())->getTimeBonus(), 8100);
	std::vector<int32_t> taken;
	for (size_t i = 0; i < 4; i++) {
		solo->regPlayerReward(players[i]->player());
		solo->setRndPosition(players[i]->player().getObjectId());
		int32_t position = solo->getPlayerReward(players[i]->player().getObjectId())->getPosition();
		EXPECT_GE(position, 1);
		EXPECT_LE(position, 4);
		EXPECT_EQ(std::count(taken.begin(), taken.end(), position), 0) << "a free position";
		taken.push_back(position);
	}
	solo->regPlayerReward(players[4]->player());
	solo->setRndPosition(players[4]->player().getObjectId());
	EXPECT_EQ(solo->getPlayerReward(players[4]->player().getObjectId())->getPosition(), 0) << "four positions, all taken";
	int32_t first = taken[0];
	solo->setRndPosition(players[0]->player().getObjectId());
	EXPECT_EQ(solo->getPlayerReward(players[0]->player().getObjectId())->getPosition(), first) << "his own position was freed first";

	runtime::Ref<instancescore::PvPArenaScore> chaos = instancescore::PvPArenaScore::create(arena(ARENA_OF_CHAOS));
	EXPECT_EQ(chaos->getBuffId(), 7);
	chaos->regPlayerReward(players[0]->player());
	EXPECT_EQ(chaos->getPlayerReward(players[0]->player().getObjectId())->getTimeBonus(), 12000);

	runtime::Ref<instancescore::PvPArenaScore> poeta = instancescore::PvPArenaScore::create(*mapInstance);
	EXPECT_FALSE(poeta->canReward()) << "not an arena";
	EXPECT_EQ(poeta->getBuffId(), 7);
	poeta->regPlayerReward(players[0]->player());
	EXPECT_EQ(poeta->getPlayerReward(players[0]->player().getObjectId())->getTimeBonus(), 0) << "the default arm: bonusTime 0";
	EXPECT_THROW(poeta->portToPosition(players[0]->player()), runtime::NullPointerException) << "no zone, no position table";
}

/** PvPArenaScore.java:111-206, :244-248: rounds, ranks, totals, the score gap, the npc bonus skills, the time, clear */
TEST_F(ArenaScoreTest, ScoreValues) {
	runtime::Ref<instancescore::PvPArenaScore> score = instancescore::PvPArenaScore::create(arena(ARENA_OF_CHAOS));
	EXPECT_EQ(score->getRound(), 1);
	score->incrementRound();
	score->incrementRound();
	score->incrementRound();
	EXPECT_EQ(score->getRound(), 3) << "at most 3";

	score->setMaxScoreGap(1);
	EXPECT_TRUE(score->reachedScoreGap()) << "no rewards: Integer.MIN_VALUE - Integer.MAX_VALUE wraps to 1";
	score->setMaxScoreGap(2);
	EXPECT_FALSE(score->reachedScoreGap());

	runtime::Ref<playerreward::PvPArenaPlayerReward> low = playerreward::PvPArenaPlayerReward::create(1, 100, int8_t{7});
	runtime::Ref<playerreward::PvPArenaPlayerReward> mid = playerreward::PvPArenaPlayerReward::create(2, 200, int8_t{7});
	runtime::Ref<playerreward::PvPArenaPlayerReward> high = playerreward::PvPArenaPlayerReward::create(3, 300, int8_t{7});
	score->addPlayerReward(*low);
	score->addPlayerReward(*mid);
	score->addPlayerReward(*high);
	high->addPoints(500);
	EXPECT_EQ(score->getRank(*high), 0);
	EXPECT_EQ(score->getRank(*mid), 1);
	EXPECT_EQ(score->getRank(*low), 2);
	EXPECT_EQ(score->getTotalPoints(), 13100 + 13200 + 13800);
	score->setMaxScoreGap(500);
	EXPECT_TRUE(score->reachedScoreGap()) << "13500 - 13000";
	score->setMaxScoreGap(501);
	EXPECT_FALSE(score->reachedScoreGap());

	EXPECT_EQ(score->getNpcBonusSkill(701176), 0x4E5732);
	EXPECT_EQ(score->getNpcBonusSkill(701190), 0x4FDB3C);
	EXPECT_EQ(score->getNpcBonusSkill(701319), 0x4f853C);
	EXPECT_EQ(score->getNpcBonusSkill(701220), 0x4E5537);
	EXPECT_EQ(score->getNpcBonusSkill(207119), 0x50C101);
	EXPECT_EQ(score->getNpcBonusSkill(207100), 0x50BE01);
	EXPECT_EQ(score->getNpcBonusSkill(1), 0);

	score->setInstanceStartTime();
	EXPECT_GT(score->getTime(), 119000) << "the first 2 minutes";
	EXPECT_LE(score->getTime(), 120000);
	score->setInstanceProgressionType(model::instance::InstanceProgressionType::END_PROGRESS);
	EXPECT_EQ(score->getTime(), 0) << "rewarded";

	score->clear();
	EXPECT_TRUE(score->getPlayerRewards().empty());
}

/** HarmonyGroupReward.java, HarmonyArenaScore.java: the groups, their ranks and total over the group entries */
TEST_F(ArenaScoreTest, HarmonyScore) {
	Member& a = addMember("Harma");
	runtime::Ref<instancescore::HarmonyArenaScore> score = instancescore::HarmonyArenaScore::create(arena(ARENA_OF_HARMONY));
	runtime::Ref<playerreward::HarmonyGroupReward> red = playerreward::HarmonyGroupReward::create(1, 12000, int8_t{7}, 9001);
	runtime::Ref<playerreward::HarmonyGroupReward> blue = playerreward::HarmonyGroupReward::create(2, 12000, int8_t{7}, 9002);
	EXPECT_EQ(red->getCourageInsignia()->itemId(), 186000137);
	EXPECT_EQ(red->getGrpObjectId(), 9001);
	runtime::Ref<model::autogroup::AGPlayer> agp = model::autogroup::AGPlayer::create(a.player());
	red->addPlayer(*agp);
	EXPECT_TRUE(red->containsPlayer(a.player().getObjectId()));
	EXPECT_EQ(red->getAGPlayer(a.player().getObjectId()).get(), agp.get());
	EXPECT_EQ(red->getAGPlayer(4711), nullptr);
	score->addHarmonyGroup(*red);
	score->addHarmonyGroup(*blue);
	EXPECT_EQ(score->getGroupReward(a.player().getObjectId()).get(), red.get());
	EXPECT_EQ(score->getGroupReward(4711), nullptr);
	blue->addPoints(100);
	EXPECT_EQ(score->getRank(*blue), 0);
	EXPECT_EQ(score->getRank(*red), 1);
	EXPECT_EQ(score->getTotalPoints(), 25000 + 25100);
	EXPECT_TRUE(score->getHarmonyGroupInside().empty()) << "nobody inside the instance";
	EXPECT_EQ(score->getGroups().size(), 2u);
	score->clear();
	EXPECT_TRUE(score->getGroups().empty());
}

/** ArenaScoreWriter.java:25-87 */
TEST_F(ArenaScoreTest, ArenaScoreWriterBytes) {
	Member& a = addMember("Writer");
	runtime::Ref<instancescore::PvPArenaScore> score = instancescore::PvPArenaScore::create(arena(ARENA_OF_CHAOS));
	score->setUpperScoreCap(16000);
	score->regPlayerReward(a.player());
	runtime::Ref<instanceinfo::ArenaScoreWriter> writer = instanceinfo::ArenaScoreWriter::create(*score, a.player().getObjectId(), true);

	network::test::PacketWriter w;
	w.D(a.player().getObjectId()).D(0).D(13000).D(0).C(0).C(model::getClassId(a.player().getPlayerClass())).C(1).C(0).D(0).D(0).D(0).D(0).H(0);
	writeS(w, "Writer", 54);
	w.zeros(92 * 11); // spaces
	w.zeros(72); // not rewarded: no owner rewards
	w.D(0).D(0).D(1).D(16000).D(3).D(1);
	EXPECT_EQ(written(*writer), w.data);

	score->setInstanceProgressionType(model::instance::InstanceProgressionType::END_PROGRESS);
	runtime::Ptr<playerreward::PvPArenaPlayerReward> reward = score->getPlayerReward(a.player().getObjectId());
	reward->setAp(*model::templates::rewards::ArenaRewardItem::create(0, 100, 20, 30));
	reward->setCrucibleInsignia(*model::templates::rewards::ArenaRewardItem::create(186000130, 1, 2, 3));
	reward->setRewardItem1(model::templates::rewards::RewardItem::create(186000147, 2));
	network::test::PacketWriter r;
	r.D(a.player().getObjectId()).D(0).D(25000).D(0).C(0).C(model::getClassId(a.player().getPlayerClass())).C(1).C(0).D(0).D(12000).D(0).D(0).H(100);
	writeS(r, "Writer", 54);
	r.zeros(92 * 11);
	r.D(100).D(30).D(20); // ap: item id 0 is not written
	r.D(0).D(0).D(0); // gp
	r.D(186000130).D(1).D(3).D(2); // crucible insignia
	r.D(0).D(0).D(0); // courage insignia
	r.D(186000147).D(2).D(0).D(0); // the simple rewards
	r.D(0).D(0).D(1).D(16000).D(3).D(1);
	EXPECT_EQ(written(*writer), r.data) << "rewarded: the score points, the time bonus, participation 100, the owner rewards";

	EXPECT_NO_THROW(serverpackets::SM_INSTANCE_SCORE(ARENA_OF_CHAOS, *writer)) << "the arena constructor reads the score time";
}

/** HarmonyScoreWriter.java:33-145 */
TEST_F(ArenaScoreTest, HarmonyScoreWriterBytes) {
	Member& a = addMember("Harmw");
	runtime::Ref<instancescore::HarmonyArenaScore> score = instancescore::HarmonyArenaScore::create(arena(ARENA_OF_HARMONY));
	score->incrementRound();
	using instanceinfo::HarmonyScoreWriter;
	EXPECT_EQ(written(*HarmonyScoreWriter::create(*score, InstanceScoreType::UPDATE_INSTANCE_PROGRESS)), (network::test::PacketWriter().C(2).D(0).D(2).data));
	EXPECT_EQ(written(*HarmonyScoreWriter::create(*score, InstanceScoreType::UPDATE_RANK, a.player())),
		(network::test::PacketWriter().C(10).zeros(13).data)) << "no group";
	EXPECT_EQ(written(*HarmonyScoreWriter::create(*score, InstanceScoreType::UPDATE_PLAYER_BUFF_STATUS, a.player())),
		(network::test::PacketWriter().C(4).zeros(16).data)) << "no reward";
	EXPECT_EQ(written(*HarmonyScoreWriter::create(*score, InstanceScoreType::INIT_PLAYER, a.player())),
		(network::test::PacketWriter().C(3).zeros(64).data)) << "no group";
	EXPECT_EQ(written(*HarmonyScoreWriter::create(*score, InstanceScoreType::PLAYER_QUIT, a.player())), (network::test::PacketWriter().C(8).data))
		<< "a type without an arm";
	score->setLowerScoreCap(10000);
	score->setUpperScoreCap(20000);
	EXPECT_EQ(written(*HarmonyScoreWriter::create(*score, InstanceScoreType::UPDATE_INSTANCE_BUFFS_AND_SCORE)),
		(network::test::PacketWriter().C(6).D(10000).D(20000).D(3).D(1).D(0).D(0).D(0).D(2).C(0).data)) << "no group inside";

	runtime::Ref<playerreward::HarmonyGroupReward> group = playerreward::HarmonyGroupReward::create(77, 12000, int8_t{7}, 9001);
	group->addPlayer(*model::autogroup::AGPlayer::create(a.player()));
	group->addPoints(5);
	score->addHarmonyGroup(*group);
	network::test::PacketWriter init;
	init.C(3).D(77);
	writeS(init, "Harmw", 52);
	init.D(9001).D(a.player().getObjectId());
	EXPECT_EQ(written(*HarmonyScoreWriter::create(*score, InstanceScoreType::INIT_PLAYER, a.player())), init.data);
	EXPECT_EQ(written(*HarmonyScoreWriter::create(*score, InstanceScoreType::UPDATE_RANK, a.player())),
		(network::test::PacketWriter().C(10).C(0).D(0).D(13005).D(9001).data));

	score->regPlayerReward(a.player());
	EXPECT_EQ(written(*HarmonyScoreWriter::create(*score, InstanceScoreType::UPDATE_PLAYER_BUFF_STATUS, a.player())),
		(network::test::PacketWriter().C(4).D(0).D(0).D(0).D(a.player().getObjectId()).data));
	network::test::PacketWriter show;
	show.C(5).D(0).D(0).D(0).D(0).D(0).D(0); // ap, gp: base, score, ranking
	show.D(0).D(0).D(0).D(0).D(0).D(0).D(0).D(0); // courage and crucible insignia with their item ids
	show.D(0).D(0).D(0).D(0);
	EXPECT_EQ(written(*HarmonyScoreWriter::create(*score, InstanceScoreType::SHOW_REWARD, a.player())), show.data);
}

/** AutoPvPFFAInstance.java:19-24, AutoHarmonyInstance.java:29-34: the instance score takes the type's difficulty */
TEST_F(ArenaScoreTest, AutoInstancesSetTheDifficulty) {
	world::WorldMapInstance& chaos = arena(ARENA_OF_CHAOS);
	runtime::Ref<instancescore::PvPArenaScore> chaosScore = instancescore::PvPArenaScore::create(chaos);
	handlerOf(chaos).score = chaosScore;
	runtime::Ref<model::autogroup::AutoPvPFFAInstance> ffa = model::autogroup::AutoPvPFFAInstance::create(AutoGroupType::ARENA_OF_CHAOS_1);
	ffa->onInstanceCreate(chaos);
	EXPECT_EQ(chaosScore->getDifficultyId(), model::autogroup::getDifficultId(AutoGroupType::ARENA_OF_CHAOS_1));
	EXPECT_EQ(chaosScore->getDifficultyId(), 1);

	world::WorldMapInstance& harmony = arena(ARENA_OF_HARMONY);
	runtime::Ref<instancescore::HarmonyArenaScore> harmonyScore = instancescore::HarmonyArenaScore::create(harmony);
	handlerOf(harmony).score = harmonyScore;
	runtime::Ref<model::autogroup::AutoHarmonyInstance> team = model::autogroup::AutoHarmonyInstance::create(AutoGroupType::ARENA_OF_HARMONY_1);
	team->onInstanceCreate(harmony);
	EXPECT_EQ(harmonyScore->getDifficultyId(), 1);

	world::WorldMapInstance& wrong = arena(ARENA_OF_DISCIPLINE);
	handlerOf(wrong).score = instancescore::InstanceScore::create();
	runtime::Ref<model::autogroup::AutoPvPFFAInstance> other = model::autogroup::AutoPvPFFAInstance::create(AutoGroupType::ARENA_OF_CHAOS_1);
	EXPECT_THROW(other->onInstanceCreate(wrong), runtime::ClassCastException) << "Java: (PvPArenaScore) of another score";
}

/** DarkPoetaScore, NormalScore, LegionDominionScore and their writers (DarkPoetaScoreWriter.java, EternalBastionScoreWriter.java,
 * LegionDominionScoreWriter.java, TheShugoEmperorsVaultScoreWriter.java), CrucibleScoreWriter.java over CruciblePlayerRewards */
TEST_F(ArenaScoreTest, CounterScoreWriters) {
	using network::test::PacketWriter;
	runtime::Ref<instancescore::DarkPoetaScore> poeta = instancescore::DarkPoetaScore::create();
	EXPECT_EQ(poeta->getRank(), 7) << "none";
	poeta->addPoints(150);
	poeta->addPoints(-200);
	poeta->addNpcKill();
	poeta->addGather();
	poeta->addGather();
	poeta->setRank(2);
	EXPECT_EQ(written(*instanceinfo::DarkPoetaScoreWriter::create(*poeta)), (PacketWriter().D(-50).D(1).D(2).D(2).data)) << "no floor";

	runtime::Ref<instancescore::NormalScore> normal = instancescore::NormalScore::create();
	normal->addPoints(100);
	normal->addPoints(-300);
	EXPECT_EQ(normal->getPoints(), 0) << "NormalScore floors at 0";
	normal->setPoints(4200);
	normal->setRank(1);
	normal->setBasicAp(11);
	normal->setFinalAp(22);
	normal->setRewardItem1(101);
	normal->setRewardItem1Count(1);
	normal->setRewardItem2(102);
	normal->setRewardItem2Count(2);
	normal->setRewardItem3(103);
	normal->setRewardItem3Count(3);
	normal->setRewardItem4(104);
	normal->setRewardItem4Count(4);
	EXPECT_EQ(written(*instanceinfo::EternalBastionScoreWriter::create(*normal)),
		(PacketWriter().D(4200).Q(0).D(1).D(0).D(22).D(101).D(1).D(102).D(2).D(103).D(3).D(104).D(4).data));
	EXPECT_EQ(written(*instanceinfo::TheShugoEmperorsVaultScoreWriter::create(*normal)),
		(PacketWriter().D(4200).D(11).D(0).D(1).D(22).D(0).D(101).D(1).D(102).D(2).D(103).D(3).D(104).D(4).data));

	runtime::Ref<instancescore::LegionDominionScore> dominion = instancescore::LegionDominionScore::create();
	dominion->addPoints(-5);
	dominion->setFinalGP(33);
	dominion->setFinalAP(44);
	dominion->setRewardItem2(202);
	dominion->setRewardItem2Count(5);
	EXPECT_EQ(written(*instanceinfo::LegionDominionScoreWriter::create(*dominion)),
		(PacketWriter().D(-5).D(0).D(0).D(0).D(33).D(44).D(0).D(0).D(202).D(5).D(0).D(0).D(0).D(0).data));

	runtime::Ref<instancescore::InstanceScore> crucible = instancescore::InstanceScore::create();
	runtime::Ref<playerreward::CruciblePlayerReward> fighter = playerreward::CruciblePlayerReward::create(55);
	fighter->addPoints(300);
	fighter->setInsignia(4);
	crucible->addPlayerReward(*fighter);
	PacketWriter running;
	running.D(55).D(300).D(1).D(4).zeros(16 * 5);
	EXPECT_EQ(written(*instanceinfo::CrucibleScoreWriter::create(*crucible)), running.data);
	crucible->setInstanceProgressionType(model::instance::InstanceProgressionType::END_PROGRESS);
	PacketWriter ended;
	ended.D(55).D(300).D(3).D(4).zeros(16 * 5);
	EXPECT_EQ(written(*instanceinfo::CrucibleScoreWriter::create(*crucible)), ended.data) << "3 once the progression ended";
	crucible->addPlayerReward(*playerreward::InstancePlayerReward::create(56));
	EXPECT_THROW(written(*instanceinfo::CrucibleScoreWriter::create(*crucible)), runtime::ClassCastException)
		<< "Java InstanceScore<CruciblePlayerReward>: another reward type";
}

/** PvpInstancePlayerReward.java: the reward arrays (0 while null), AP/GP, captured zones, the mythic Kunax equipment */
TEST_F(ArenaScoreTest, PvpInstancePlayerReward) {
	Member& a = addMember("Kunax");
	runtime::Ref<playerreward::PvpInstancePlayerReward> reward = playerreward::PvpInstancePlayerReward::create(5, model::Race::ELYOS);
	EXPECT_EQ(reward->getRace(), model::Race::ELYOS);
	EXPECT_EQ(reward->getReward1ItemId(), 0);
	EXPECT_EQ(reward->getReward1BonusCount(), 0);
	EXPECT_EQ(reward->getBonusRewardCount(), 0);
	reward->setReward1(1, 2, 3);
	reward->setReward2(4, 5, 6);
	reward->setReward3(7, 8);
	reward->setReward4(9, 10);
	reward->setBonusReward(11, 12);
	EXPECT_EQ(reward->getReward1ItemId(), 1);
	EXPECT_EQ(reward->getReward1Count(), 2);
	EXPECT_EQ(reward->getReward1BonusCount(), 3);
	EXPECT_EQ(reward->getReward2BonusCount(), 6);
	EXPECT_EQ(reward->getReward3Count(), 8);
	EXPECT_EQ(reward->getReward4ItemId(), 9);
	EXPECT_EQ(reward->getBonusRewardCount(), 12);
	reward->incrementCapturedZones();
	reward->incrementCapturedZones();
	EXPECT_EQ(reward->getCapturedZones(), 2);
	ASSERT_EQ(a.player().getPlayerClass(), model::PlayerClass::WARRIOR);
	EXPECT_EQ(reward->getMythicKunaxEquipment(a.player()), 0) << "a base class: the default arm";
	a.f.commonData->setPlayerClass(model::PlayerClass::TEMPLAR);
	const std::vector<int32_t> templar{100901305, 100001682, 115001702, 110601549, 111601512, 112601494, 113601495, 114601502, 125003995};
	for (int32_t i = 0; i < 200; i++) { // every item of both lists comes up (weapons with chance 17.5)
		int32_t item = reward->getMythicKunaxEquipment(a.player());
		EXPECT_NE(std::find(templar.begin(), templar.end(), item), templar.end()) << item;
	}
}

/** PvpInstanceScoreWriter.java:49-150 */
TEST_F(ArenaScoreTest, PvpInstanceScoreWriterBytes) {
	using network::test::PacketWriter;
	using instanceinfo::PvpInstanceScoreWriter;
	Member& e = addMember("Elyos");
	Member& d = asmodian("Asmo");
	runtime::Ref<instancescore::PvpInstanceScore> score = instancescore::PvpInstanceScore::create(3000, 1500, 2000);
	EXPECT_EQ(written(*PvpInstanceScoreWriter::create(*score, InstanceScoreType::UPDATE_INSTANCE_PROGRESS)), (PacketWriter().C(2).D(0).data));
	score->setInstanceProgressionType(model::instance::InstanceProgressionType::PREPARING);
	EXPECT_EQ(written(*PvpInstanceScoreWriter::create(*score, InstanceScoreType::UPDATE_INSTANCE_PROGRESS)), (PacketWriter().C(2).D(2).data));
	EXPECT_EQ(written(*PvpInstanceScoreWriter::create(*score, InstanceScoreType::UPDATE_PLAYER_BUFF_STATUS, 77, 60)),
		(PacketWriter().C(4).D(0).D(60).D(77).data));
	EXPECT_EQ(written(*PvpInstanceScoreWriter::create(*score, InstanceScoreType::PLAYER_QUIT, 77, 0)), (PacketWriter().C(8).D(77).data));
	EXPECT_EQ(written(*PvpInstanceScoreWriter::create(*score, InstanceScoreType::UPDATE_RANK)), (PacketWriter().C(10).data)) << "no arm";
	EXPECT_THROW(written(*PvpInstanceScoreWriter::create(*score, InstanceScoreType::UPDATE_FACTION_SCORE)), runtime::NullPointerException)
		<< "Java: the null race";
	EXPECT_THROW(written(*PvpInstanceScoreWriter::create(*score, InstanceScoreType::INIT_PLAYER, 77, 0)), runtime::NullPointerException)
		<< "Java: getPlayerReward(77).getRace() of no reward";

	runtime::Ref<playerreward::PvpInstancePlayerReward> elyos = playerreward::PvpInstancePlayerReward::create(e.player().getObjectId(), model::Race::ELYOS);
	runtime::Ref<playerreward::PvpInstancePlayerReward> asmo =
		playerreward::PvpInstancePlayerReward::create(d.player().getObjectId(), model::Race::ASMODIANS);
	score->addPlayerReward(*elyos);
	score->addPlayerReward(*asmo);
	score->addPointsByRace(model::Race::ELYOS, 500);
	score->addPointsByRace(model::Race::ASMODIANS, 300);
	score->incrementKillsByRace(model::Race::ASMODIANS);
	EXPECT_EQ(written(*PvpInstanceScoreWriter::create(*score, InstanceScoreType::INIT_PLAYER, e.player().getObjectId(), 60)),
		(PacketWriter().C(3).D(0).D(60).D(e.player().getObjectId()).D(model::getRaceId(model::Race::ELYOS)).data));
	EXPECT_EQ(written(*PvpInstanceScoreWriter::create(*score, InstanceScoreType::UPDATE_FACTION_SCORE, model::Race::ASMODIANS)),
		(PacketWriter().C(11).C(0).D(1).D(300).D(model::getRaceId(model::Race::ASMODIANS)).D(model::getRaceId(model::Race::ELYOS)).data));

	elyos->setBaseAp(100);
	elyos->setBonusAp(10);
	elyos->setBaseGp(20);
	elyos->setBonusGp(2);
	elyos->setReward1(186000001, 3, 1);
	elyos->setBonusReward(186000002, 1);
	EXPECT_EQ(written(*PvpInstanceScoreWriter::create(*score, InstanceScoreType::SHOW_REWARD, e.player().getObjectId(), 0)),
		(PacketWriter().C(5).D(100).D(100).D(10).D(20).D(2).D(186000001).D(3).D(1).D(0).D(0).D(0).D(0).D(0).D(0).D(0).D(186000002).D(1).C(1).data));

	std::vector<runtime::Ref<model::gameobjects::player::Player>> participants{runtime::Ref<model::gameobjects::player::Player>(e.player()),
		runtime::Ref<model::gameobjects::player::Player>(d.player())};
	PacketWriter scores;
	scores.C(6).D(100);
	scores.D(0).D(0).D(e.player().getObjectId()).zeros(12 * 23);
	scores.D(0).D(0).D(d.player().getObjectId()).zeros(12 * 23);
	scores.C(0).D(0).D(500).D(model::getRaceId(model::Race::ELYOS)).D(0);
	scores.C(0).D(1).D(300).D(model::getRaceId(model::Race::ASMODIANS)).D(model::getRaceId(model::Race::ELYOS));
	EXPECT_EQ(written(*PvpInstanceScoreWriter::create(*score, InstanceScoreType::UPDATE_INSTANCE_BUFFS_AND_SCORE, participants)), scores.data);

	asmo->addPvPKill();
	asmo->addPoints(40);
	PacketWriter full;
	full.C(7);
	full.D(e.player().getObjectId()).C(model::getClassId(e.player().getPlayerClass())).C(utils::stats::id(e.player().getAbyssRank()->getRank()))
		.C(0).H(0).D(0).D(0);
	writeS(full, "Elyos", 52);
	full.zeros(69 * 23);
	full.D(d.player().getObjectId()).C(model::getClassId(d.player().getPlayerClass())).C(utils::stats::id(d.player().getAbyssRank()->getRank()))
		.C(0).H(0).D(1).D(40);
	writeS(full, "Asmo", 52);
	full.zeros(69 * 23);
	EXPECT_EQ(written(*PvpInstanceScoreWriter::create(*score, InstanceScoreType::UPDATE_ALL_PLAYER_INFO, participants)), full.data);
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
