// P6-I2 Crucible Challenge (game-server/data/handlers/instance/crucible/CrucibleChallengeInstance.java and its base CrucibleInstance.java) on
// a real instance of its map. The test world has no zones, so the zone branches (the stadium checks of onDie(Player), the chests' arenas)
// take their "outside every stadium" arms; the revive's teleports and leaveInstance destroy or move through the instance service and are
// left out, as is the hidden boss's quest check (no quest rows).

#include "I2TestSupport.h"
#include "CrucibleTestSupport.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <functional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/handlers/instance/crucible/CrucibleChallengeInstance.h"
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
using DropSet = runtime::RcHashSet<runtime::Ref<model::drop::DropItem>>;

class CrucibleChallengeTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		publishCrucibleData(context);
		instance = &create(CRUCIBLE_CHALLENGE, CrucibleChallengeInstance_instanceFactory);
		enter(*instance, 380.0f, 1663.0f, 97.6f);
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

	runtime::Ptr<Npc> place(int32_t npcId) { return spawnNpc(*instance, npcId, 345.0f, 1663.0f, 95.3f, int8_t{0}); }

	/** an npc of the instance dies, the player's damage on it when `byPlayer`: its hp to 0, and NpcController.onDie calls the handler's onDie */
	runtime::Ptr<Npc> dies(runtime::Ptr<Npc> npc, bool byPlayer = true) {
		if (byPlayer)
			hit(*npc);
		npc->getLifeStats()->setCurrentHp(0);
		return npc;
	}

	runtime::Ptr<Npc> dies(int32_t npcId, bool byPlayer = true) { return dies(place(npcId), byPlayer); }

	/** the handler's onDie alone, on a living npc the player hit: what the handler itself deletes and when */
	runtime::Ptr<Npc> handlerDies(runtime::Ptr<Npc> npc) {
		hit(*npc);
		handler().onDie(*npc);
		return npc;
	}

	void after(int64_t millis) { executor().advance(std::chrono::milliseconds(millis)); }

	std::vector<uint8_t> stageInfo(StageType type) {
		return serialized(SM_INSTANCE_STAGE_INFO(2, model::instance::getId(type), model::instance::getType(type)));
	}

	std::vector<uint8_t> scorePacket() {
		runtime::Ref<CrucibleScoreWriter> writer = CrucibleScoreWriter::create(*handler().getInstanceScore());
		return serialized(SM_INSTANCE_SCORE(CRUCIBLE_CHALLENGE, *writer));
	}

	/** the first seed whose first Rnd draws satisfy `draws`, installed again so the handler sees the same draws */
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

	runtime::Ref<DropSet> dropSetOf(Npc& npc) {
		runtime::Ref<DropSet> set = DropSet::create(AION_LOCK_CLASS(DropRegistrationService::currentDropMap#dropItems));
		set->add(model::drop::DropItem::create(model::drop::Drop(182006429, 1, 1, 100.0f))); // a drop of the npc's own
		services::drop::DropRegistrationService::getInstance().getCurrentDropMap().put(npc.getObjectId(), set);
		return set;
	}

	static std::vector<std::pair<int32_t, int64_t>> itemsOf(DropSet& set) {
		std::vector<std::pair<int32_t, int64_t>> items;
		for (const auto& item : set)
			items.emplace_back(item->getDropTemplate()->getItemId(), item->getCount());
		std::sort(items.begin(), items.end()); // a hash set's order
		return items;
	}

	world::WorldMapInstance* instance = nullptr;
};

TEST_F(CrucibleChallengeTest, TheHandlerIsTheJavaClassOfItsMap) {
	std::ifstream in(std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/handlers/instance/crucible/CrucibleChallengeInstance.java");
	std::stringstream text;
	text << in.rdbuf();
	const std::string source = text.str();
	std::smatch match;
	ASSERT_TRUE(std::regex_search(source, match, std::regex(R"(@InstanceID\((\d+)\))")));
	EXPECT_EQ(std::stoi(match[1].str()), CRUCIBLE_CHALLENGE);
	EXPECT_NE(dynamic_cast<crucible::CrucibleChallengeInstance*>(&handler()), nullptr);
}

/** CrucibleChallengeInstance.java:49-54, CrucibleInstance.java:36-39: the first record keeper and arbiter, the score; no self revive */
TEST_F(CrucibleChallengeTest, CreationSpawnsTheFirstRecordKeeperAndArbiter) {
	after(1);
	EXPECT_EQ(count(205668), 1u);
	EXPECT_EQ(count(205682), 1u);
	ASSERT_NE(handler().getInstanceScore(), nullptr);
	EXPECT_FALSE(handler().allowSelfReviveBySkill());
	EXPECT_FALSE(handler().allowSelfReviveByItem());
	EXPECT_EQ(handler().getStage(), StageType::DEFAULT);
}

/** :56-74, CrucibleInstance.java:29-43: the reward is added once; the score and the stage info are sent */
TEST_F(CrucibleChallengeTest, EnteringAddsTheRewardAndSendsTheScoreAndTheStage) {
	ASSERT_NE(reward(), nullptr);
	EXPECT_FALSE(reward()->isRewarded());
	reward()->addPoints(120);
	runtime::Ptr<CruciblePlayerReward> first = reward();
	(*client)->clearSent();
	handler().onEnterInstance(player());
	EXPECT_EQ(handler().getInstanceScore()->getPlayerRewards().size(), 1u) << "the second entry adds no reward";
	EXPECT_EQ(reward(), first) << "the reward of the first entry stays";
	EXPECT_EQ(reward()->getPoints(), 120);
	EXPECT_EQ(countSent(scorePacket()), 1);
	EXPECT_EQ(countSent(stageInfo(StageType::DEFAULT)), 1);
}

/** :259-328: a kill by the player adds the npc's points and announces them; nobody's kill adds nothing */
TEST_F(CrucibleChallengeTest, KillsAddTheirPoints) {
	(*client)->clearSent();
	runtime::Ptr<Npc> boss = dies(217783);
	EXPECT_EQ(reward()->getPoints(), 1600);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_GET_SCORE(boss->getObjectTemplate()->getL10n(), 1600))), 1);
	EXPECT_EQ(countSent(scorePacket()), 1);
	dies(217784, false);
	EXPECT_EQ(reward()->getPoints(), 1600) << "no player damage: no points";
	const std::vector<std::pair<int32_t, int32_t>> table{{217784, 120}, {217786, 200}, {217797, 1100}, {217788, 1300}, {217843, 1400},
		{217800, 1650}, {217807, 2500}, {217847, 4600}, {217794, 5000}, {217806, 5800}, {217819, 7200}, {217820, 0}};
	for (const auto& [npcId, points] : table) {
		const int32_t before = reward()->getPoints();
		dies(npcId);
		EXPECT_EQ(reward()->getPoints() - before, points) << npcId;
	}
}

/** :76-85, 472-498, 580-593: stage 1 - the three Elyos fighters, two more after the second kill, the boss when all are dead */
TEST_F(CrucibleChallengeTest, StageOne) {
	(*client)->clearSent();
	handler().onChangeStage(StageType::START_STAGE_1_ROUND_1);
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_1_ROUND_1);
	after(1999);
	EXPECT_EQ(countSent(stageInfo(StageType::START_STAGE_1_ROUND_1)), 0);
	after(1);
	EXPECT_EQ(countSent(stageInfo(StageType::START_STAGE_1_ROUND_1)), 1) << "changeStage: the stage info after 2 s";
	after(1999);
	EXPECT_EQ(count(217784), 0u);
	after(1);
	ASSERT_EQ(count(217784), 3u) << "the Elyos player's fighters after 4 s";
	handler().onChangeStage(StageType::START_STAGE_1_ROUND_1);
	after(4000);
	EXPECT_EQ(count(217784), 3u) << "the same stage again changes nothing";

	std::vector<runtime::Ptr<Npc>> fighters = npcs(*instance, 217784);
	dies(fighters[0]);
	EXPECT_FALSE(fighters[0]->isSpawned()) << "onDie deletes the fighter";
	after(1);
	EXPECT_EQ(count(217784), 2u);
	dies(fighters[1]);
	after(1);
	ASSERT_EQ(count(217784), 3u) << "the second kill: two more fighters";
	std::vector<runtime::Ptr<Npc>> rest = npcs(*instance, 217784);
	dies(rest[0]);
	dies(rest[1]);
	after(4000);
	EXPECT_EQ(count(217783), 0u) << "one fighter is still alive";
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_1_ROUND_1);
	dies(rest[2]);
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_1_ROUND_2);
	after(3999);
	EXPECT_EQ(count(217783), 0u);
	after(1);
	ASSERT_EQ(count(217783), 1u) << "all dead: the boss after 4 s";
	dies(npcs(*instance, 217783)[0]);
	EXPECT_EQ(handler().getStage(), StageType::PASS_STAGE_1);
	after(1);
	EXPECT_EQ(count(217758), 1u) << "the boss: the chest";
}

/** :499-511, 76-85 (217800/217801), 368-417, 76-85 (217803): stage 2 and its bonus stage */
TEST_F(CrucibleChallengeTest, StageTwoAndPoppy) {
	seedFor([] { return commons::utils::Rnd::get(1, 2) == 1; });
	handler().onChangeStage(StageType::START_STAGE_2_ROUND_1);
	after(4000);
	ASSERT_EQ(count(217800), 1u);
	ASSERT_EQ(count(217801), 1u);
	dies(npcs(*instance, 217800)[0]);
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_2_ROUND_1) << "217801 lives";
	(*client)->clearSent();
	dies(npcs(*instance, 217801)[0]);
	EXPECT_EQ(handler().getStage(), StageType::PASS_STAGE_2);
	after(2000);
	EXPECT_EQ(handler().getStage(), StageType::START_BONUS_STAGE_2) << "the bonus stage after 2 s";
	after(3999);
	EXPECT_EQ(count(217802), 0u);
	after(1);
	EXPECT_EQ(count(217802), 1u) << "Poppy after 6 s";
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDArena_Solo_SB1_START_BROADCAST())), 1);
	after(3999);
	EXPECT_EQ(count(217803), 0u);
	after(1);
	EXPECT_EQ(count(217803), 1u) << "the first spawn at 10 s";
	after(4000 * 8);
	EXPECT_EQ(count(217803), 9u);
	after(4000);
	EXPECT_EQ(count(217803), 10u);

	(*client)->clearSent();
	std::vector<runtime::Ptr<Npc>> sparks = npcs(*instance, 217803);
	for (size_t i = 0; i < 4; i++)
		dies(sparks[i]);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDArena_Solo_SB1_5Dead_BROADCAST())), 0);
	dies(sparks[4]);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDArena_Solo_SB1_5Dead_BROADCAST())), 1) << "the fifth";
	for (size_t i = 5; i < 9; i++)
		dies(sparks[i]);
	after(1);
	EXPECT_EQ(count(205675), 0u);
	runtime::Ptr<Npc> poppy = npcs(*instance, 217802)[0];
	dies(sparks[9]);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDArena_Solo_SB1_Succ_BROADCAST())), 1);
	EXPECT_FALSE(poppy->isSpawned()) << "Poppy is replaced by her chest";
	after(1);
	EXPECT_EQ(count(205675), 1u) << "passStage2: the record keeper";
	EXPECT_EQ(count(218571), 1u);
	after(40000);
	EXPECT_EQ(count(217803), 0u) << "the timer stopped with the tenth: the success needs it cancelled before the next tick";
}

/** :186-191: Poppy caught - the sparks go, the timer stops and the record keeper comes (once) */
TEST_F(CrucibleChallengeTest, PoppyCaught) {
	seedFor([] { return commons::utils::Rnd::get(1, 2) == 2; });
	handler().onChangeStage(StageType::START_STAGE_2_ROUND_1);
	after(4000);
	ASSERT_EQ(count(217797), 1u);
	dies(npcs(*instance, 217797)[0]);
	dies(npcs(*instance, 217798)[0]);
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_2_ROUND_1);
	dies(npcs(*instance, 217799)[0]);
	EXPECT_EQ(handler().getStage(), StageType::PASS_STAGE_2);
	after(14000);
	ASSERT_EQ(count(217803), 2u);
	dies(npcs(*instance, 217802)[0]);
	EXPECT_EQ(count(217803), 0u);
	after(1);
	EXPECT_EQ(count(205675), 1u);
	after(60000);
	EXPECT_EQ(count(217803), 0u) << "the timer is cancelled";
	dies(217802);
	after(1);
	EXPECT_EQ(count(205675), 1u) << "passStage2 spawns the record keeper only once";
}

/** :512-531, 87-108, 340-350, 216-221: stage 3 - the barrels, the Dracuni and Gomju */
TEST_F(CrucibleChallengeTest, StageThree) {
	seedFor([] { return commons::utils::Rnd::get(1, 2) == 1; });
	(*client)->clearSent();
	handler().onChangeStage(StageType::START_STAGE_3_ROUND_1);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_CHAT_IDArena_Solo_S4_System2())), 1);
	after(2000);
	EXPECT_EQ(count(218560), 1u);
	after(2000);
	ASSERT_EQ(count(217844), 5u);
	std::vector<runtime::Ptr<Npc>> dracuni = npcs(*instance, 217844);
	dies(dracuni[0]);
	dies(dracuni[1]);
	dies(dracuni[2]);
	after(5000);
	EXPECT_EQ(count(217845), 0u) << "getNpcs counts the dying npc too: three kills leave two, no round yet";
	dies(dracuni[3]);
	EXPECT_EQ(count(217844), 1u);
	after(4999);
	EXPECT_EQ(count(217845), 0u);
	after(1);
	EXPECT_EQ(count(217844), 0u) << "startStage3Round1_1: the Dracuni, the barrels go";
	EXPECT_EQ(count(218560), 0u);
	after(2000);
	ASSERT_EQ(count(217845), 1u) << "the boss 2 s later";

	dies(npcs(*instance, 217845)[0]);
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_3_ROUND_2);
	after(3000);
	EXPECT_EQ(count(217848), 6u);
	EXPECT_EQ(count(217847), 1u);
	dies(npcs(*instance, 217847)[0]);
	after(1);
	EXPECT_EQ(count(205676), 1u) << "Gomju: the record keeper";
}

/** :221-238 (217841/218561): a new barrel at one of two spots */
TEST_F(CrucibleChallengeTest, ABarrelReplacement) {
	dies(217841);
	dies(218561);
	after(1);
	EXPECT_EQ(count(217840), 1u);
	EXPECT_EQ(count(218560), 1u);
}

/** :532-547, 109-130: stage 4, the round 2 boss and the bonus chest */
TEST_F(CrucibleChallengeTest, StageFour) {
	seedFor([] { return commons::utils::Rnd::get(1, 3) == 2; });
	handler().onChangeStage(StageType::START_STAGE_4_ROUND_1);
	after(4000);
	EXPECT_EQ(count(217786), 3u);
	ASSERT_EQ(count(217789), 1u);
	dies(npcs(*instance, 217789)[0]);
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_4_ROUND_2);
	after(6000);
	ASSERT_EQ(count(217794), 1u);
	dies(npcs(*instance, 217794)[0]);
	EXPECT_EQ(handler().getStage(), StageType::START_BONUS_STAGE_4);
	after(4000);
	EXPECT_EQ(count(217820), 1u);
	dies(217795);
	after(4000);
	EXPECT_EQ(count(217820), 1u) << "already in the bonus stage: no second chest";
	dies(217786);
	EXPECT_EQ(count(217786), 3u) << "217786 is just deleted";
}

/** :548-561, 117-122: the alternative stage 4 */
TEST_F(CrucibleChallengeTest, AlternativeStageFour) {
	seedFor([] { return commons::utils::Rnd::get(1, 3) == 3; });
	handler().onChangeStage(StageType::START_ALTERNATIVE_STAGE_4_ROUND_1);
	after(4000);
	EXPECT_EQ(count(217787), 3u);
	ASSERT_EQ(count(217793), 1u);
	dies(npcs(*instance, 217793)[0]);
	after(6000);
	EXPECT_EQ(count(217795), 1u);
}

/** :562-575, 140-160: stage 5 for an Elyos, the round 2 boss by Rnd, the record keeper */
TEST_F(CrucibleChallengeTest, StageFive) {
	seedFor([] { return commons::utils::Rnd::get(1, 5) == 5; });
	handler().onChangeStage(StageType::START_STAGE_5_ROUND_1);
	after(4000);
	ASSERT_EQ(count(217815), 1u);
	seedFor([] { return commons::utils::Rnd::get(1, 3) == 2; });
	dies(npcs(*instance, 217815)[0]);
	EXPECT_EQ(handler().getStage(), StageType::START_STAGE_5_ROUND_2);
	after(4000);
	ASSERT_EQ(count(218562), 1u);
	dies(npcs(*instance, 218562)[0]);
	EXPECT_EQ(handler().getStage(), StageType::PASS_STAGE_5);
	after(1);
	EXPECT_EQ(count(205678), 1u);
}

/** :576-578, 161-180: stage 6, the boss's delayed despawn and the record keeper (no quest: not the rift) */
TEST_F(CrucibleChallengeTest, StageSix) {
	handler().onChangeStage(StageType::START_STAGE_6_ROUND_1);
	after(4000);
	ASSERT_EQ(count(217819), 1u);
	runtime::Ptr<Npc> boss = npcs(*instance, 217819)[0];
	handlerDies(boss);
	EXPECT_EQ(handler().getStage(), StageType::PASS_STAGE_6);
	EXPECT_TRUE(boss->isSpawned()) << "the despawn is delayed";
	after(3999);
	EXPECT_TRUE(boss->isSpawned());
	after(1);
	EXPECT_FALSE(boss->isSpawned());
	after(1999);
	EXPECT_EQ(count(205679), 0u);
	after(1);
	EXPECT_EQ(count(205679), 1u);
	EXPECT_EQ(count(730459), 0u);
}

/** :239-244: the hidden boss opens the way back */
TEST_F(CrucibleChallengeTest, TheHiddenBoss) {
	runtime::Ptr<Npc> guard = place(218190);
	dies(218185);
	EXPECT_FALSE(guard->isSpawned());
	after(1);
	EXPECT_EQ(count(730460), 1u);
}

/** :245-246: other npcs are not deleted */
TEST_F(CrucibleChallengeTest, OtherNpcsStay) {
	runtime::Ptr<Npc> npc = handlerDies(place(205668));
	EXPECT_TRUE(npc->isSpawned());
	EXPECT_FALSE(handlerDies(place(217783))->isSpawned()) << "a stage npc is deleted by the handler";
}

/** :352-366: the insignias once, by the points; the score each time */
TEST_F(CrucibleChallengeTest, TheReward) {
	dies(217783); // 1600 points
	(*client)->clearSent();
	handler().doReward(player());
	EXPECT_TRUE(reward()->isRewarded());
	EXPECT_EQ(reward()->getInsignia(), 136) << "0.01 * 1600 + 120";
	EXPECT_EQ(player().getInventory().getItemCountByItemId(186000130), 136);
	EXPECT_EQ(handler().getInstanceScore()->getInstanceProgressionType(), InstanceProgressionType::END_PROGRESS);
	EXPECT_EQ(countSent(scorePacket()), 1);
	handler().onStopTraining(player());
	EXPECT_EQ(player().getInventory().getItemCountByItemId(186000130), 136) << "rewarded once";
	EXPECT_EQ(countSent(scorePacket()), 2);
	(*client)->clearSent();
	handler().onEnterInstance(player());
	EXPECT_EQ(countSent(scorePacket()), 1) << "a rewarded player who enters gets the score from doReward";
}

/** :419-440: a death outside every stadium keeps place 0 */
TEST_F(CrucibleChallengeTest, ADeathOutsideTheStadiums) {
	reward()->setSpawnPosition(3);
	EXPECT_TRUE(handler().onDie(player(), player()));
	EXPECT_EQ(reward()->getSpawnPosition(), 0);
}

/** :595-602: the chest of stage 1 used: the next record keeper */
TEST_F(CrucibleChallengeTest, TheStageOneChestUsed) {
	runtime::Ptr<Npc> chest = place(217758);
	handler().handleUseItemFinish(runtime::Ptr<model::gameobjects::player::Player>(player()), *chest);
	EXPECT_EQ(count(205674), 1u);
	handler().handleUseItemFinish(runtime::Ptr<model::gameobjects::player::Player>(player()), *place(217820));
	EXPECT_EQ(count(205674), 1u);
}

/** :604-620: the drops of the stage 1 chest, Poppy's chest and the hidden boss */
TEST_F(CrucibleChallengeTest, TheFixedDrops) {
	runtime::Ptr<Npc> chest = place(217758);
	runtime::Ref<DropSet> chestDrops = dropSetOf(*chest);
	handler().onDropRegistered(*chest, player().getObjectId());
	EXPECT_EQ(itemsOf(*chestDrops), (std::vector<std::pair<int32_t, int64_t>>{{182006429, 0}, {186000134, 1}}));

	runtime::Ptr<Npc> poppysChest = place(218571);
	runtime::Ref<DropSet> poppysDrops = dropSetOf(*poppysChest);
	seedFor([] { return commons::utils::Rnd::get(1, 3) == 2; });
	handler().onDropRegistered(*poppysChest, player().getObjectId());
	EXPECT_EQ(itemsOf(*poppysDrops), (std::vector<std::pair<int32_t, int64_t>>{{182006430, 1}}));

	runtime::Ptr<Npc> boss = place(218185);
	runtime::Ref<DropSet> bossDrops = dropSetOf(*boss);
	handler().onDropRegistered(*boss, player().getObjectId());
	EXPECT_EQ(itemsOf(*bossDrops), (std::vector<std::pair<int32_t, int64_t>>{{188051349, 1}}));

	runtime::Ptr<Npc> other = place(217783);
	runtime::Ref<DropSet> otherDrops = dropSetOf(*other);
	handler().onDropRegistered(*other, player().getObjectId());
	EXPECT_EQ(itemsOf(*otherDrops).size(), 1u) << "other npcs keep their drops";
}

/** :621-676: the three bonus chests - an empty first chest calls its guardian, the second and third pay by the round */
TEST_F(CrucibleChallengeTest, TheBonusChests) {
	runtime::Ptr<Npc> first = place(217827);
	runtime::Ptr<Npc> second = place(217828);
	runtime::Ptr<Npc> third = place(217829);
	dropSetOf(*first);
	seedFor([] { return commons::utils::Rnd::get(1, 3) == 3; });
	handler().onDropRegistered(*first, player().getObjectId());
	EXPECT_FALSE(first->isSpawned()) << "empty: the chest goes";
	EXPECT_FALSE(second->isSpawned()) << "the other two chests go";
	EXPECT_FALSE(third->isSpawned());
	after(1);
	EXPECT_EQ(count(217837), 1u) << "round 0: the guardian 217837";

	runtime::Ptr<Npc> next = place(217828);
	runtime::Ref<DropSet> nextDrops = dropSetOf(*next);
	seedFor([] { return commons::utils::Rnd::get(1, 3) == 2; });
	handler().onDropRegistered(*next, player().getObjectId());
	EXPECT_TRUE(next->isSpawned()) << "a paying chest stays";
	EXPECT_EQ(itemsOf(*nextDrops), (std::vector<std::pair<int32_t, int64_t>>{{186000130, 18}})) << "round 1, draw 2";

	runtime::Ptr<Npc> last = place(217829);
	runtime::Ref<DropSet> lastDrops = dropSetOf(*last);
	seedFor([] { return commons::utils::Rnd::get(1, 3) == 3; });
	handler().onDropRegistered(*last, player().getObjectId());
	EXPECT_EQ(itemsOf(*lastDrops), (std::vector<std::pair<int32_t, int64_t>>{{186000130, 250}})) << "round 2, draw 3";
}

/** :658-664: an empty chest in round 1 calls 217838 */
TEST_F(CrucibleChallengeTest, AnEmptySecondChest) {
	runtime::Ptr<Npc> first = place(217827);
	dropSetOf(*first);
	seedFor([] { return commons::utils::Rnd::get(1, 3) == 1; });
	handler().onDropRegistered(*first, player().getObjectId()); // round 0: 1 insignia
	runtime::Ptr<Npc> second = place(217829);
	dropSetOf(*second);
	seedFor([] { return commons::utils::Rnd::get(1, 3) == 3; });
	handler().onDropRegistered(*second, player().getObjectId());
	EXPECT_FALSE(second->isSpawned());
	after(1);
	EXPECT_EQ(count(217838), 1u);
	EXPECT_EQ(count(217837), 0u);
}

/** CrucibleInstance.java:54-59, 81-85; :406-412: a destroyed instance spawns nothing more */
TEST_F(CrucibleChallengeTest, DestroyedSpawnsNothing) {
	handler().onChangeStage(StageType::START_STAGE_6_ROUND_1);
	handler().onInstanceDestroy();
	after(4000);
	EXPECT_EQ(count(217819), 0u);
	EXPECT_TRUE(handler().getInstanceScore()->getPlayerRewards().empty()) << "the score is cleared";
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
