// P6-I2 Empyrean Crucible (game-server/data/handlers/instance/crucible/EmpyreanCrucibleInstance.java) on a real instance of its map, with
// one player inside. Left out:
// - the teleports: the ready room of a new player or a revive after stage 0, the stage teleports of START_STAGE_5 to 9, the exit of a player
//   who left, and a second doReward (it teleports);
// - the drop of the stage 7 book box (no registered team here: Java's NullPointerException);
// - the stage 9 twins' skill (no skill data).

#include "I2TestSupport.h"
#include "CrucibleTestSupport.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <regex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/handlers/instance/crucible/EmpyreanCrucibleInstance.h"
#include "aion/gameserver/model/drop/Drop.h"
#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/instance/StageTypeInfo.h"
#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"
#include "aion/gameserver/model/instance/playerreward/CruciblePlayerReward.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/network/aion/instanceinfo/CrucibleScoreWriter.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_SCORE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_STAGE_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/services/drop/DropRegistrationService.h"

namespace aion::gameserver::handlers::instance::test {
namespace {

using model::gameobjects::Npc;
using model::instance::InstanceProgressionType;
using model::instance::StageType;
using model::instance::playerreward::CruciblePlayerReward;
using network::aion::instanceinfo::CrucibleScoreWriter;
using network::aion::serverpackets::SM_INSTANCE_SCORE;
using network::aion::serverpackets::SM_INSTANCE_STAGE_INFO;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using EmpyreanDropSet = runtime::RcHashSet<runtime::Ref<model::drop::DropItem>>;

class EmpyreanCrucibleTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		publishCrucibleData(context);
		instance = &create(EMPYREAN_CRUCIBLE, EmpyreanCrucibleInstance_instanceFactory);
		enter(*instance, 380.0f, 347.0f, 96.7f);
		player().setQuestStateList(model::gameobjects::player::QuestStateList::create()); // PlayerService.loadPlayer's (no quests)
		handler().onEnterInstance(player());
	}

	void TearDown() override {
		InstanceHandlerTest::TearDown();
		resetCrucibleData();
	}

	::aion::gameserver::instance::handlers::InstanceHandler& handler() { return *instance->getInstanceHandler(); }

	runtime::Ptr<CruciblePlayerReward> reward() {
		return runtime::cast<CruciblePlayerReward>(handler().getInstanceScore()->getPlayerReward(player().getObjectId()));
	}

	size_t count(int32_t npcId) { return npcs(*instance, npcId).size(); }

	runtime::Ptr<Npc> place(int32_t npcId) { return spawnNpc(*instance, npcId, 345.0f, 349.0f, 96.1f, int8_t{0}); }

	/** the npc dies: its hp to 0 (NpcController.onDie calls the handler's onDie) */
	runtime::Ptr<Npc> dies(runtime::Ptr<Npc> npc) {
		npc->getLifeStats()->setCurrentHp(0);
		return npc;
	}

	/** every npc of these ids dies */
	void allDie(std::initializer_list<int32_t> npcIds) {
		for (int32_t npcId : npcIds) {
			for (const runtime::Ptr<Npc>& npc : npcs(*instance, npcId))
				dies(npc);
		}
	}

	void after(int64_t millis) { executor().advance(std::chrono::milliseconds(millis)); }

	std::vector<uint8_t> stageInfo(StageType type) {
		return serialized(SM_INSTANCE_STAGE_INFO(2, model::instance::getId(type), model::instance::getType(type)));
	}

	std::vector<uint8_t> scorePacket() {
		runtime::Ref<CrucibleScoreWriter> writer = CrucibleScoreWriter::create(*handler().getInstanceScore());
		return serialized(SM_INSTANCE_SCORE(EMPYREAN_CRUCIBLE, *writer));
	}

	static void seedFor(const std::function<bool()>& draws) {
		for (uint64_t seed = 1; seed < 10000; seed++) {
			commons::utils::Rnd::seedCurrentThreadForTests(seed);
			if (draws()) {
				commons::utils::Rnd::seedCurrentThreadForTests(seed);
				return;
			}
		}
		FAIL() << "no seed";
	}

	runtime::Ref<EmpyreanDropSet> dropSetOf(Npc& npc) {
		runtime::Ref<EmpyreanDropSet> set = EmpyreanDropSet::create(AION_LOCK_CLASS(DropRegistrationService::currentDropMap#dropItems));
		set->add(model::drop::DropItem::create(model::drop::Drop(182006429, 1, 1, 100.0f))); // a drop of the npc's own
		services::drop::DropRegistrationService::getInstance().getCurrentDropMap().put(npc.getObjectId(), set);
		return set;
	}

	static std::vector<std::pair<int32_t, int64_t>> itemsOf(EmpyreanDropSet& set) {
		std::vector<std::pair<int32_t, int64_t>> items;
		for (const auto& item : set)
			items.emplace_back(item->getDropTemplate()->getItemId(), item->getCount());
		std::sort(items.begin(), items.end()); // a hash set's order
		return items;
	}

	/** the elevator of stage 1 and the two first fighters (9 s) */
	void startStageOne() {
		handler().onChangeStage(StageType::START_STAGE_1_ELEVATOR);
		after(9000);
	}

	world::WorldMapInstance* instance = nullptr;
};

TEST_F(EmpyreanCrucibleTest, TheHandlerIsTheJavaClassOfItsMap) {
	std::ifstream in(std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/handlers/instance/crucible/EmpyreanCrucibleInstance.java");
	std::stringstream text;
	text << in.rdbuf();
	const std::string source = text.str();
	std::smatch match;
	ASSERT_TRUE(std::regex_search(source, match, std::regex(R"(@InstanceID\((\d+)\))")));
	EXPECT_EQ(std::stoi(match[1].str()), EMPYREAN_CRUCIBLE);
	EXPECT_NE(dynamic_cast<crucible::EmpyreanCrucibleInstance*>(&handler()), nullptr);
}

/** EmpyreanCrucibleInstance.java:64-91: the record keeper and the arbiter at once; a player at stage 0 gets the score and the stage */
TEST_F(EmpyreanCrucibleTest, CreationAndEntry) {
	EXPECT_EQ(count(799567), 1u);
	EXPECT_EQ(count(799573), 1u);
	ASSERT_NE(reward(), nullptr);
	(*client)->clearSent();
	handler().onEnterInstance(player());
	EXPECT_EQ(countSent(scorePacket()), 1);
	EXPECT_EQ(countSent(stageInfo(StageType::DEFAULT)), 1);
	EXPECT_FALSE(handler().allowSelfReviveByItem());
}

/** :971-990, 1107-1149, 114-276: stage 1, its five rounds by kills; the points of each kill */
TEST_F(EmpyreanCrucibleTest, StageOneByKills) {
	(*client)->clearSent();
	handler().onChangeStage(StageType::START_STAGE_1_ELEVATOR);
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_1_ROUND_1) << "setStage sets the stage at once, only its packet waits";
	after(1);
	EXPECT_EQ(countSent(stageInfo(StageType::START_STAGE_1_ELEVATOR)), 1);
	after(4999);
	EXPECT_EQ(countSent(stageInfo(StageType::START_STAGE_1_ROUND_1)), 1) << "round 1 after 5 s";
	after(3999);
	EXPECT_EQ(count(217486), 0u);
	after(1);
	ASSERT_EQ(count(217486), 1u);
	ASSERT_EQ(count(217489), 1u);
	runtime::Ptr<Npc> first = npcs(*instance, 217486)[0];
	dies(first);
	EXPECT_EQ(reward()->getPoints(), 600);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_GET_SCORE(first->getObjectTemplate()->getL10n(), 600))), 1);
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_1_ROUND_1) << "217489 lives";
	dies(npcs(*instance, 217489)[0]);
	EXPECT_EQ(reward()->getPoints(), 1100);
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_1_ROUND_2);
	after(6000);
	ASSERT_EQ(count(217492), 1u);
	dies(npcs(*instance, 217492)[0]);
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_1_ROUND_3);
	after(6000);
	ASSERT_EQ(count(217487), 1u);
	dies(npcs(*instance, 217487)[0]);
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_1_ROUND_4);
	after(6000);
	ASSERT_EQ(count(217491), 1u);
	dies(npcs(*instance, 217491)[0]);
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_1_ROUND_5);
	after(7999);
	EXPECT_EQ(count(217493), 0u);
	after(1);
	ASSERT_EQ(count(217493), 1u) << "the boss after 8 s";
	dies(npcs(*instance, 217493)[0]);
	EXPECT_EQ(handler().getStage(), StageType::PASS_GROUP_STAGE_1);
	EXPECT_EQ(count(217756), 6u) << "the six chests at once";
	EXPECT_EQ(count(799568), 1u);
	EXPECT_EQ(reward()->getPoints(), 1100 + 1100 + 1200 + 1300 + 5000);
}

/** :978-986, 1107-1118: the rounds of stage 1 also go on after their time when their npcs live */
TEST_F(EmpyreanCrucibleTest, StageOneByTime) {
	startStageOne();
	after(62000 - 9000 - 1);
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_1_ROUND_1);
	after(1);
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_1_ROUND_2) << "62 s: the round's npcs live, the next round starts";
	after(59000 - 1);
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_1_ROUND_2);
	after(1);
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_1_ROUND_3);
}

/** :961-968: a round whose npcs are all dead does not start the next one by time */
TEST_F(EmpyreanCrucibleTest, ADeadRoundDoesNotRepeat) {
	startStageOne();
	allDie({217486, 217489});
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_1_ROUND_2);
	after(6000);
	allDie({217492});
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_1_ROUND_3);
	after(62000);
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_1_ROUND_3) << "the 62 s check of round 1 finds no npc of it";
}

/** :991-999, 278-346, 846-856, 884-887, 371-391: stage 2 through its rounds to the boss and the bonus stage */
TEST_F(EmpyreanCrucibleTest, StageTwo) {
	place(217756);
	handler().onChangeStage(StageType::START_STAGE_2_ELEVATOR);
	EXPECT_EQ(count(217756), 0u) << "the chests of stage 1 go";
	after(9000);
	ASSERT_EQ(count(217503) + count(217502) + count(217504), 3u);
	dies(npcs(*instance, 217503)[0]);
	dies(npcs(*instance, 217502)[0]);
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_2_ROUND_1);
	dies(npcs(*instance, 217504)[0]);
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_2_ROUND_2);
	after(6000);
	ASSERT_EQ(count(217502) + count(217508) + count(217507) + count(217504), 4u);
	allDie({217502, 217508, 217507});
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_2_ROUND_2);
	allDie({217504});
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_2_ROUND_3);
	after(6000);
	ASSERT_EQ(count(217509), 1u);
	dies(npcs(*instance, 217509)[0]);
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_2_ROUND_4);
	after(6000);
	ASSERT_EQ(count(217505), 1u);
	ASSERT_EQ(count(217508), 1u);
	allDie({217505});
	EXPECT_EQ(count(217508), 1u) << "217508 lives: no round 4_1 yet";
	allDie({217508});
	ASSERT_EQ(count(217508), 1u) << "startStage4Round4_1: one 217508 at once";
	after(5000);
	ASSERT_EQ(count(217506), 1u);
	ASSERT_EQ(count(217507), 1u);
	allDie({217508});
	EXPECT_EQ(count(217508), 0u) << "a 217508 of an EmpyreanStage only goes (no new round 4_1)";
	after(5000);
	EXPECT_EQ(count(217506), 1u);
	allDie({217506});
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_2_ROUND_4) << "217507 of the same round lives";
	seedFor([] { return commons::utils::Rnd::get(1, 2) == 2; });
	allDie({217507});
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_2_ROUND_5);
	after(6000);
	ASSERT_EQ(count(217501), 1u) << "the boss by Rnd";
	(*client)->clearSent();
	allDie({217501});
	EXPECT_EQ(handler().getStage(), StageType::PASS_GROUP_STAGE_2);
	after(8000);
	EXPECT_EQ(count(217737), 1u);
	EXPECT_EQ(handler().getStage(), StageType::START_BONUS_STAGE_2);
	after(30000);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_S2_Saam1_01())), 1);
	after(29999);
	EXPECT_EQ(count(217737), 1u);
	after(1);
	EXPECT_EQ(count(217737), 0u) << "Saam goes after a minute";
	EXPECT_EQ(count(799569), 1u);
}

/** :382-386: Saam killed in time brings the record keeper at once, the minute's end does nothing */
TEST_F(EmpyreanCrucibleTest, SaamKilled) {
	dies(place(217510));
	after(8000);
	ASSERT_EQ(count(217737), 1u);
	(*client)->clearSent();
	allDie({217737});
	EXPECT_EQ(count(799569), 1u);
	after(60000);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_S2_Saam1_01())), 0);
	EXPECT_EQ(count(799569), 1u);
}

/** :1000-1027, 387-466: stage 3, its rounds and the bonus stage's ticket box */
TEST_F(EmpyreanCrucibleTest, StageThree) {
	handler().onChangeStage(StageType::START_STAGE_3_ELEVATOR);
	after(9000);
	EXPECT_EQ(count(217511) + count(217512) + count(217513) + count(217514), 24u);
	allDie({217511, 217512, 217513});
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_3_ROUND_1);
	allDie({217514});
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_3_ROUND_2);
	after(6000);
	EXPECT_EQ(count(217515) + count(217516) + count(217517) + count(217518), 12u);
	allDie({217515, 217516, 217517, 217518});
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_3_ROUND_3);
	after(6000);
	EXPECT_EQ(count(217519) + count(217520) + count(217521) + count(217522), 6u);
	allDie({217519, 217520, 217521, 217522});
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_3_ROUND_4);
	after(6000);
	allDie({217523, 217524, 217525, 217526});
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_3_ROUND_5);
	after(6000);
	ASSERT_EQ(count(217527) + count(217528), 2u);
	allDie({217527});
	allDie({217528});
	EXPECT_EQ(handler().getStage(), StageType::START_BONUS_STAGE_3);
	after(7000);
	EXPECT_EQ(count(217744), 1u);
	after(32000);
	EXPECT_EQ(count(217740) + count(217741) + count(217742) + count(217743), 4u) << "startBonusStage3 at 39 s";
	(*client)->clearSent();
	after(30000);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDArena_S3_Bonus_01())), 1);
	after(20000);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDArena_S3_Bonus_02())), 1);
	after(5000);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDArena_S3_Bonus_03())), 1);
	after(5000);
	EXPECT_EQ(count(217740) + count(217744), 0u);
	EXPECT_EQ(count(205331), 1u);
	EXPECT_EQ(count(217735), 1u);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_S3_ResurBox1_01())), 1);
}

/** :1028-1035, 466-536: stage 4's first two rounds; round 2's late trio is one EmpyreanStage */
TEST_F(EmpyreanCrucibleTest, StageFourFirstRounds) {
	place(217735);
	handler().onChangeStage(StageType::START_STAGE_4_ELEVATOR);
	EXPECT_EQ(count(217735), 0u);
	after(9000);
	ASSERT_EQ(count(217557), 2u);
	ASSERT_EQ(count(217559), 1u);
	allDie({217557});
	EXPECT_EQ(count(217558), 0u);
	allDie({217559});
	EXPECT_EQ(count(217558), 2u) << "round 1's second wave at once";
	allDie({217558});
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_4_ROUND_2);
	after(6000);
	EXPECT_EQ(count(217559) + count(217562) + count(217560), 3u);
	allDie({217559, 217562, 217560});
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_4_ROUND_2) << "no round of an EmpyreanStage died yet";
	after(40999);
	EXPECT_EQ(count(217557) + count(217558) + count(217561), 0u);
	after(1);
	ASSERT_EQ(count(217557) + count(217558) + count(217561), 3u) << "the trio at 47 s";
	allDie({217557, 217558});
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_4_ROUND_2);
	allDie({217561});
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_4_ROUND_3) << "the whole trio dead";
	after(6000);
	EXPECT_EQ(count(217563) + count(217564), 2u);
	after(10000);
	EXPECT_EQ(count(217565), 1u);
	after(27000);
	ASSERT_EQ(count(217566), 1u);
	allDie({217563, 217564, 217565});
	allDie({217566});
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_4_ROUND_4);
	after(6000);
	EXPECT_EQ(count(217567), 1u);
}

/** :575-604, 801-844: stage 4's last round, its timed waves, then the bonus stage */
TEST_F(EmpyreanCrucibleTest, StageFourLastRound) {
	dies(place(217567));
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_4_ROUND_5);
	after(6000);
	ASSERT_EQ(count(217653), 1u);
	allDie({217653});
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_4_ROUND_5) << "not done before 174 s";
	after(168000);
	EXPECT_EQ(count(217651) + count(217652) + count(217653), 10u);
	(*client)->clearSent();
	allDie({217651, 217652, 217653});
	EXPECT_EQ(handler().getStage(), StageType::START_BONUS_STAGE_4) << "PASS_GROUP_STAGE_4, then the bonus stage at once";
	after(1);
	EXPECT_EQ(countSent(stageInfo(StageType::PASS_GROUP_STAGE_4)), 1);
	after(5999);
	EXPECT_EQ(count(217749), 1u);
	EXPECT_EQ(countSent(stageInfo(StageType::START_BONUS_STAGE_4)), 1);
	after(27000);
	EXPECT_EQ(count(217778), 1u) << "startBonusStage4 at 33 s";
	after(102000);
	EXPECT_EQ(count(217778) + count(217749) + count(217747), 0u);
	EXPECT_EQ(count(205338), 1u);
}

/** :1036-1041, 605-655: stage 5 */
TEST_F(EmpyreanCrucibleTest, StageFive) {
	handler().onChangeStage(StageType::START_STAGE_5_ROUND_1);
	after(6000);
	ASSERT_EQ(count(217547) + count(217548) + count(217549), 3u);
	allDie({217547, 217548, 217549});
	after(4000);
	ASSERT_EQ(count(217550) + count(217545), 2u);
	allDie({217550, 217545});
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_5_ROUND_2);
	after(6000);
	allDie({217552});
	EXPECT_EQ(count(700527) + count(700528), 2u);
	after(6000);
	allDie({217553});
	EXPECT_EQ(count(700527) + count(700528), 0u);
	EXPECT_EQ(count(281111) + count(281322), 2u);
	after(6000);
	allDie({217554});
	EXPECT_EQ(count(281111), 0u);
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_5_ROUND_5);
	after(6000);
	EXPECT_EQ(count(281191), 1u);
	allDie({217556});
	EXPECT_EQ(count(281191), 0u);
	EXPECT_EQ(handler().getStage(), StageType::PASS_GROUP_STAGE_5);
	EXPECT_EQ(count(205339), 1u);
}

/** :1042-1052, 656-697: stage 6 */
TEST_F(EmpyreanCrucibleTest, StageSix) {
	handler().onChangeStage(StageType::START_STAGE_6_ROUND_1);
	after(6000);
	EXPECT_EQ(count(217568), 3u);
	allDie({217568});
	EXPECT_EQ(count(205413), 3u) << "each dead 217568 leaves 205413";
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_6_ROUND_1) << "the round is not done before 12 s";
	after(6000);
	ASSERT_EQ(count(217568), 2u);
	allDie({217568});
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_6_ROUND_2);
	after(6000);
	ASSERT_EQ(count(217570), 1u);
	runtime::Ptr<Npc> commander = npcs(*instance, 217570)[0];
	dies(commander);
	EXPECT_EQ(count(211984), 5u) << "217570's five adds";
	allDie({217569});
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_6_ROUND_2) << "not done before 12 s";
	after(6000);
	EXPECT_EQ(count(217569), 2u);
	dies(npcs(*instance, 217569)[0]);
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_6_ROUND_2) << "the commander is still in the instance";
}

/** :687-698, 699-715: stage 6's end */
TEST_F(EmpyreanCrucibleTest, StageSixEnd) {
	dies(place(217572));
	EXPECT_EQ(count(217573), 1u);
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_6_ROUND_4);
	allDie({217573});
	EXPECT_EQ(handler().getStage(), StageType::START_BONUS_STAGE_6);
	after(7999);
	EXPECT_EQ(count(217750), 0u);
	after(1);
	EXPECT_EQ(count(217750), 1u);
	allDie({217750});
	EXPECT_EQ(count(205340), 1u);
}

/** :1053-1063, 716-773: stages 7 to 9 and the group reward at the end */
TEST_F(EmpyreanCrucibleTest, StagesSevenToNine) {
	handler().onChangeStage(StageType::START_STAGE_7_ROUND_1);
	after(6000);
	allDie({217578});
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_7_ROUND_2);
	after(6000);
	allDie({217579});
	after(6000);
	allDie({217580});
	after(6000);
	allDie({217581});
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_7_ROUND_5);
	after(6000);
	(*client)->clearSent();
	allDie({217586});
	EXPECT_EQ(handler().getStage(), StageType::PASS_GROUP_STAGE_7);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_S7_BookBox_01())), 1);
	after(2000);
	EXPECT_EQ(count(217753) + count(205341), 2u);

	handler().onChangeStage(StageType::START_STAGE_8_ROUND_1);
	after(6000);
	allDie({217588});
	after(6000);
	allDie({217590});
	after(6000);
	allDie({217591});
	after(6000);
	allDie({217592});
	EXPECT_EQ(count(280790), 1u);
	after(6000);
	allDie({217593});
	EXPECT_EQ(handler().getStage(), StageType::PASS_GROUP_STAGE_8);
	EXPECT_EQ(count(205342), 1u);

	handler().onChangeStage(StageType::START_STAGE_9_ROUND_1);
	after(6000);
	allDie({217594});
	after(6000);
	allDie({217595});
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_9_ROUND_3);
	after(6000);
	ASSERT_EQ(count(217596) + count(217597), 2u);
	// the first twin's death would cast 19624 on the other (no skill data here): it leaves without dying, the second dies alone
	npcs(*instance, 217597)[0]->getController().delete_();
	EXPECT_FALSE(reward()->isRewarded());
	dies(npcs(*instance, 217596)[0]);
	EXPECT_TRUE(reward()->isRewarded()) << "both twins gone: rewardGroup";
}

/** :895-911: the insignias by the points, the score */
TEST_F(EmpyreanCrucibleTest, TheReward) {
	reward()->addPoints(1000);
	(*client)->clearSent();
	handler().onStopTraining(player());
	EXPECT_TRUE(reward()->isRewarded());
	EXPECT_EQ(reward()->getInsignia(), 360) << "0.01 * 1000 + 350";
	EXPECT_EQ(player().getInventory().getItemCountByItemId(186000130), 360);
	EXPECT_EQ(handler().getInstanceScore()->getInstanceProgressionType(), InstanceProgressionType::END_PROGRESS);
	EXPECT_EQ(countSent(scorePacket()), 1);
}

/** :93-104: a rewarded player still hears of the kill but gets no points */
TEST_F(EmpyreanCrucibleTest, NoPointsAfterTheReward) {
	handler().onStopTraining(player());
	ASSERT_TRUE(reward()->isRewarded());
	(*client)->clearSent();
	runtime::Ptr<Npc> npc = dies(place(217486));
	EXPECT_EQ(reward()->getPoints(), 0);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_GET_SCORE(npc->getObjectTemplate()->getL10n(), 600))), 1);
	EXPECT_EQ(countSent(scorePacket()), 1);
}

/** :920-937: the last player defeated ends the crucible */
TEST_F(EmpyreanCrucibleTest, TheGroupDefeated) {
	EXPECT_TRUE(handler().onDie(player(), player()));
	EXPECT_TRUE(reward()->isPlayerDefeated());
	EXPECT_TRUE(reward()->isRewarded());
	EXPECT_EQ(player().getInventory().getItemCountByItemId(186000130), 350);
}

/** :1196-1203: leaving marks the reward */
TEST_F(EmpyreanCrucibleTest, Leaving) {
	handler().onLeaveInstance(player());
	EXPECT_TRUE(reward()->isPlayerLeave());
}

/** :1231-1240: a stage 1 chest gives a worthiness ticket and goes */
TEST_F(EmpyreanCrucibleTest, AChestUsed) {
	runtime::Ptr<Npc> chest = place(217756);
	(*client)->clearSent();
	handler().handleUseItemFinish(runtime::Ptr<model::gameobjects::player::Player>(player()), *chest);
	EXPECT_EQ(player().getInventory().getItemCountByItemId(186000124), 1);
	EXPECT_FALSE(chest->isSpawned());
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE(1401009, std::vector<std::string>{}))), 1);
	runtime::Ptr<Npc> other = place(217749);
	handler().handleUseItemFinish(runtime::Ptr<model::gameobjects::player::Player>(player()), *other);
	EXPECT_TRUE(other->isSpawned());
}

/** :1242-1278: the bonus boxes' potions */
TEST_F(EmpyreanCrucibleTest, TheBonusBoxDrops) {
	runtime::Ptr<Npc> box = place(217740);
	runtime::Ref<EmpyreanDropSet> boxDrops = dropSetOf(*box);
	seedFor([] { return commons::utils::Rnd::chance() >= 6; });
	handler().onDropRegistered(*box, player().getObjectId());
	EXPECT_EQ(itemsOf(*boxDrops), (std::vector<std::pair<int32_t, int64_t>>{{162000108, 1}}));

	runtime::Ptr<Npc> chest = place(217750);
	runtime::Ref<EmpyreanDropSet> chestDrops = dropSetOf(*chest);
	seedFor([] { return commons::utils::Rnd::chance() >= 6; });
	handler().onDropRegistered(*chest, player().getObjectId());
	EXPECT_EQ(itemsOf(*chestDrops), (std::vector<std::pair<int32_t, int64_t>>{{162000109, 1}}));

	runtime::Ptr<Npc> other = place(217493);
	runtime::Ref<EmpyreanDropSet> otherDrops = dropSetOf(*other);
	handler().onDropRegistered(*other, player().getObjectId());
	EXPECT_EQ(itemsOf(*otherDrops).size(), 1u) << "other npcs keep their drops";
}

/** :913-918, 1210-1229: a destroyed instance spawns nothing more */
TEST_F(EmpyreanCrucibleTest, DestroyedSpawnsNothing) {
	handler().onChangeStage(StageType::START_STAGE_1_ELEVATOR);
	handler().onInstanceDestroy();
	after(9000);
	EXPECT_EQ(count(217486), 0u);
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
