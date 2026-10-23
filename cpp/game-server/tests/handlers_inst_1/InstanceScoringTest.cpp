// C++-dev only (owner, 2026-10-20): instance scoring (custom/instance/InstanceScoring) and its use in the Fire Temple Hard example
// (FireTempleInstance; docs/deviations/C2.md, "C++-dev only: instance scoring"). The score's clock is the test's; the rewards go to a test giver.

#include "InstanceHandlerTestSupport.h"

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

#include "aion/gameserver/configs/main/RuntimeConfig.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/custom/difficulty/DifficultyMenu.h"
#include "aion/gameserver/custom/instance/InstanceScoring.h"
#include "aion/gameserver/handlers/instance/FireTempleInstance.h"
#include "aion/gameserver/model/instance/InstanceProgressionType.h"
#include "aion/gameserver/model/instance/instancescore/DarkPoetaScore.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/network/aion/instanceinfo/DarkPoetaScoreWriter.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_SCORE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::instance::test {
namespace {

using custom::instance::InstanceScoring;
using model::gameobjects::Npc;
using network::aion::serverpackets::SM_QUEST_ACTION;

std::vector<std::tuple<int32_t, int32_t, int64_t>> given; // (player, item, count)

void testGiver(model::gameobjects::player::Player& player, int32_t itemId, int64_t count) {
	given.emplace_back(player.getObjectId(), itemId, count);
}

class InstanceScoringTest : public InstanceHandlerTest {
public:
	/** a Fire Temple instance through the handler factory (the test enter functions' getNextAvailableInstance) */
	world::WorldMapInstance& createFireTemple() { return create(FIRE_TEMPLE, FireTempleInstance_instanceFactory); }

	/** the player leaves his instance for `instance` (World.despawn, setPosition, spawn) */
	void moveInto(world::WorldMapInstance& instance) {
		world::World& world = world::World::getInstance();
		world.despawn(player());
		ASSERT_TRUE(world.setPosition(runtime::Ptr<model::gameobjects::VisibleObject>(&player()), instance.getMapId(), instance.getInstanceId(),
			127.0f, 176.0f, 99.7f, int8_t{0}));
		world.spawn(runtime::Ptr<model::gameobjects::VisibleObject>(&player()));
	}

protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		// the fixture's rows are all NORMAL: 214094 is ELITE and 214599 HERO here, as their kinds weigh differently
		std::string rows = handlerNpcTemplatesXml();
		for (const auto& [id, rating] : {std::pair<std::string, std::string>{"214094", "ELITE"}, {"214599", "HERO"}}) {
			const size_t row = rows.find("npc_id=\"" + id + "\"");
			const size_t at = rows.find("rating=\"NORMAL\"", row);
			rows.replace(at, std::string("rating=\"NORMAL\"").size(), "rating=\"" + rating + "\"");
		}
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(npcContext, rows));
		given.clear();
		InstanceScoring::setRewardGiverForTests(&testGiver);
		panel = configs::main::RuntimeConfig::INSTANCE_SCORING_DP_PANEL.exchange(false);
		scoringOn = configs::main::RuntimeConfig::INSTANCE_SCORING_ENABLE.exchange(false);
	}

	void TearDown() override {
		configs::main::RuntimeConfig::INSTANCE_SCORING_DP_PANEL.store(panel);
		configs::main::RuntimeConfig::INSTANCE_SCORING_ENABLE.store(scoringOn);
		InstanceScoring::setRewardGiverForTests(nullptr);
		custom::difficulty::DifficultyMenu::setEnterFunctionForTests(nullptr);
		InstanceHandlerTest::TearDown();
	}

	runtime::Ref<InstanceScoring> score(InstanceScoring::Settings settings) {
		return InstanceScoring::create(std::move(settings), [this] { return now; });
	}

	/** a Fire Temple instance (Normal) with the player inside, the sent packets cleared */
	world::WorldMapInstance& room() {
		world::WorldMapInstance& instance = create(FIRE_TEMPLE, FireTempleInstance_instanceFactory);
		enter(instance, 127.0f, 176.0f, 99.7f);
		(*client)->clearSent();
		return instance;
	}

	Npc& spawned(world::WorldMapInstance& instance, int32_t npcId) { return *spawnNpc(instance, npcId, 130.0f, 176.0f, 99.7f, int8_t{0}); }

	std::vector<uint8_t> chat(const std::string& text) {
		return serialized(network::aion::serverpackets::SM_MESSAGE(0, "", text, model::ChatType::GOLDEN_YELLOW));
	}

	xml::LoadContext npcContext; // the rated rows (the fixture's context holds its own ids)
	int64_t now = 1'700'000'000'000LL;
	bool panel = false;
	bool scoringOn = false;
};

/** points by kind: a listed boss, a HERO/LEGENDARY-less ELITE, a NORMAL npc */
TEST_F(InstanceScoringTest, PointsPerKillAreWeightedByTheNpcsKind) {
	world::WorldMapInstance& instance = room();
	InstanceScoring::Settings settings;
	settings.bossNpcIds = {212839};
	runtime::Ref<InstanceScoring> s = score(settings);
	EXPECT_EQ(s->pointsOf(spawned(instance, 212839)), 200) << "a listed boss (ELITE rating)";
	EXPECT_EQ(s->pointsOf(spawned(instance, 214094)), 30) << "ELITE";
	EXPECT_EQ(s->pointsOf(spawned(instance, 212790)), 10) << "NORMAL";
	EXPECT_EQ(s->pointsOf(spawned(instance, 214599)), 200) << "HERO";
	EXPECT_EQ(s->onKill(instance, spawned(instance, 212790)), 0) << "not started: nothing";
	EXPECT_TRUE(s->start(instance));
	Npc& boss = spawned(instance, 212839);
	(*client)->clearSent();
	EXPECT_EQ(s->onKill(instance, boss), 200);
	EXPECT_EQ(s->onKill(instance, spawned(instance, 214094)), 30);
	EXPECT_EQ(s->getPoints(), 230);
	EXPECT_EQ(countSent(serialized(network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_GET_SCORE(boss.getObjectTemplate()->getL10n(), 200))), 1)
		<< "announced as Dark Poeta does";
}

/** the countdown: SM_QUEST_ACTION with the time limit once, cleared at the end */
TEST_F(InstanceScoringTest, TheTimerBarShowsTheCountdownOnce) {
	world::WorldMapInstance& instance = room();
	runtime::Ref<InstanceScoring> s = score(InstanceScoring::Settings{});
	EXPECT_FALSE(s->isStarted());
	EXPECT_TRUE(s->start(instance));
	EXPECT_EQ(sent(), std::vector<std::vector<uint8_t>>{serialized(SM_QUEST_ACTION(0, 1200))}) << "the panel is off: only the bar";
	EXPECT_FALSE(s->start(instance)) << "once";
	now += 300'000;
	EXPECT_EQ(s->getTimeLeftMillis(), 900'000);
	(*client)->clearSent();
	s->finish(instance, false);
	EXPECT_EQ(countSent(serialized(SM_QUEST_ACTION(0, 0))), 1) << "the bar cleared";
	now += 300'000;
	EXPECT_EQ(s->getTimeLeftMillis(), 900'000) << "frozen at the end";
}

/** Dark Poeta's rank shape over a 20-minute limit: time fractions 1/2, 3/8, 1/4, 1/8 with the point thresholds; 5 in time; 8 otherwise */
TEST_F(InstanceScoringTest, TheRankThresholds) {
	runtime::Ref<InstanceScoring> s = score(InstanceScoring::Settings{});
	EXPECT_EQ(s->rankOf(600'001, 4000, true), 1);
	EXPECT_EQ(s->rankOf(600'000, 4000, true), 2) << "more than half the time is needed for 1";
	EXPECT_EQ(s->rankOf(600'001, 3999, true), 2);
	EXPECT_EQ(s->rankOf(450'001, 3200, true), 2);
	EXPECT_EQ(s->rankOf(450'000, 3200, true), 3);
	EXPECT_EQ(s->rankOf(300'001, 2400, true), 3);
	EXPECT_EQ(s->rankOf(300'001, 2399, true), 4);
	EXPECT_EQ(s->rankOf(150'001, 1600, true), 4);
	EXPECT_EQ(s->rankOf(150'000, 1600, true), 5);
	EXPECT_EQ(s->rankOf(150'001, 1599, true), 5);
	EXPECT_EQ(s->rankOf(1, 0, true), 5) << "the final boss in time";
	EXPECT_EQ(s->rankOf(0, 5000, true), 8) << "no time left";
	EXPECT_EQ(s->rankOf(1'000'000, 5000, false), 8) << "no final boss";
}

/** the final boss ends the run: rank, the result line, the rank's rewards to every player inside, once */
TEST_F(InstanceScoringTest, TheFinalBossEndsTheRunWithRankLineAndRewards) {
	world::WorldMapInstance& instance = room();
	InstanceScoring::Settings settings;
	settings.finalBossNpcIds = {212846};
	settings.pointThresholds = {400, 300, 200, 100};
	settings.rewards[1] = {{162000049, 5}, {162000005, 15}};
	runtime::Ref<InstanceScoring> s = score(settings);
	s->setTitle("Fire Temple - Hard");
	s->start(instance);
	now += 400'000;
	for (int i = 0; i < 2; i++)
		s->onKill(instance, spawned(instance, 214094)); // 60
	(*client)->clearSent();
	s->onKill(instance, spawned(instance, 212846)); // the final boss (NORMAL in these rows: 10)
	EXPECT_TRUE(s->isFinished());
	const int32_t expectedRank = s->rankOf(800'000, s->getPoints(), true);
	EXPECT_EQ(s->getRank(), expectedRank);
	EXPECT_EQ(countSent(chat(s->resultLine(expectedRank, s->getPoints(), 800'000))), 1);
	EXPECT_EQ(s->resultLine(2, 350, 800'000), "[Fire Temple - Hard] Rank 2 - 350 points, 13:20 left");
	const std::vector<InstanceScoring::Reward>& rewards = settings.rewards[static_cast<size_t>(expectedRank - 1)];
	ASSERT_EQ(given.size(), rewards.size());
	for (size_t i = 0; i < rewards.size(); i++)
		EXPECT_EQ(given[i], std::make_tuple(player().getObjectId(), rewards[i].first, rewards[i].second));
	EXPECT_EQ(s->finish(instance, true), 0) << "once";
	EXPECT_EQ(s->onKill(instance, spawned(instance, 214094)), 0) << "ended: no more points";
}

/** a rank with rewards: rank 2 gets its two items */
TEST_F(InstanceScoringTest, ARankGetsItsRewards) {
	world::WorldMapInstance& instance = room();
	InstanceScoring::Settings settings;
	settings.bossNpcIds = {212846};
	settings.finalBossNpcIds = {212846};
	settings.bossPoints = 3300;
	settings.rewards[1] = {{162000049, 5}, {162000005, 15}};
	runtime::Ref<InstanceScoring> s = score(settings);
	s->start(instance);
	now += 650'000; // 550 s left: more than 3/8 (450 s), not more than 1/2 (600 s)
	s->onKill(instance, spawned(instance, 212846));
	EXPECT_EQ(s->getRank(), 2);
	EXPECT_EQ(given, (std::vector<std::tuple<int32_t, int32_t, int64_t>>{{player().getObjectId(), 162000049, 5},
						 {player().getObjectId(), 162000005, 15}}));
}

/** the time out: rank 8, its line, no reward */
TEST_F(InstanceScoringTest, TheTimeoutEndsTheRunAtRankEight) {
	world::WorldMapInstance& instance = room();
	InstanceScoring::Settings settings;
	settings.rewards[7] = {};
	runtime::Ref<InstanceScoring> s = score(settings);
	s->start(instance);
	s->onKill(instance, spawned(instance, 214094));
	now += 1'200'000;
	(*client)->clearSent();
	EXPECT_EQ(s->finish(instance, false), 8);
	EXPECT_EQ(countSent(chat(s->resultLine(8, 30, 0))), 1);
	EXPECT_EQ(s->resultLine(8, 30, 0), "[Instance] Rank 8 - 30 points, 0:00 left");
	EXPECT_TRUE(given.empty());
}

/** the experimental panel: off, no SM_INSTANCE_SCORE; on, Dark Poeta's panel with its map id and the time left */
TEST_F(InstanceScoringTest, TheDarkPoetaPanelIsSentOnlyWhenItsSwitchIsOn) {
	world::WorldMapInstance& instance = room();
	runtime::Ref<InstanceScoring> off = score(InstanceScoring::Settings{});
	off->start(instance);
	EXPECT_EQ(sent().size(), 1u) << "the bar only";
	(*client)->clearSent();
	configs::main::RuntimeConfig::INSTANCE_SCORING_DP_PANEL.store(true);
	runtime::Ref<InstanceScoring> on = score(InstanceScoring::Settings{});
	on->start(instance);
	runtime::Ref<model::instance::instancescore::DarkPoetaScore> expected = model::instance::instancescore::DarkPoetaScore::create();
	expected->setInstanceProgressionType(model::instance::InstanceProgressionType::START_PROGRESS);
	runtime::Ref<network::aion::instanceinfo::DarkPoetaScoreWriter> writer = network::aion::instanceinfo::DarkPoetaScoreWriter::create(*expected);
	EXPECT_EQ(sent(), (std::vector<std::vector<uint8_t>>{serialized(SM_QUEST_ACTION(0, 1200)),
						  serialized(network::aion::serverpackets::SM_INSTANCE_SCORE(300040000, *writer, 1'200'000))}));
	Npc& victim = spawned(instance, 212790);
	(*client)->clearSent();
	on->onKill(instance, victim);
	EXPECT_EQ(sent().size(), 2u) << "the score message and the panel";
}

/** Fire Temple: Normal and Hard with scoring off have no score; Hard with scoring on counts from the first entry and ends at Kromede */
TEST_F(InstanceScoringTest, FireTempleHardIsScoredOnlyWithTheSwitch) {
	world::WorldMapInstance& normal = create(FIRE_TEMPLE, FireTempleInstance_instanceFactory);
	EXPECT_EQ(dynamic_cast<FireTempleInstance&>(*normal.getInstanceHandler()).getScoring(), nullptr) << "Normal";

	static world::WorldMapInstance* hard = nullptr;
	static InstanceScoringTest* self = nullptr;
	self = this;
	custom::difficulty::DifficultyMenu::setEnterFunctionForTests(
		[](const model::templates::portal::PortalPath*, model::gameobjects::player::Player&, Npc&, int8_t) {
			hard = &self->createFireTemple();
		});
	Npc& portal = *npcs(normal).at(0);
	runtime::Ref<custom::difficulty::DifficultyMenuOffer> offer = custom::difficulty::DifficultyMenuOffer::create(1, 1, portal, nullptr);
	enter(normal, 127.0f, 176.0f, 99.7f);
	custom::difficulty::DifficultyMenu::enter(player(), *offer, custom::difficulty::DifficultyMenu::HARD);
	EXPECT_EQ(dynamic_cast<FireTempleInstance&>(*hard->getInstanceHandler()).getScoring(), nullptr) << "Hard, scoring off";

	configs::main::RuntimeConfig::INSTANCE_SCORING_ENABLE.store(true);
	EXPECT_EQ(dynamic_cast<FireTempleInstance&>(*createFireTemple().getInstanceHandler()).getScoring(), nullptr) << "Normal, scoring on";
	custom::difficulty::DifficultyMenu::enter(player(), *offer, custom::difficulty::DifficultyMenu::HARD);
	auto& handler = dynamic_cast<FireTempleInstance&>(*hard->getInstanceHandler());
	runtime::Ptr<InstanceScoring> s = handler.getScoring();
	ASSERT_NE(s, nullptr);
	EXPECT_EQ(s->getSettings().timeLimitSeconds, 1200);
	EXPECT_FALSE(s->isStarted());
	moveInto(*hard);
	(*client)->clearSent();
	handler.onEnterInstance(player());
	EXPECT_TRUE(s->isStarted()) << "from the first entry";
	EXPECT_EQ(countSent(serialized(SM_QUEST_ACTION(0, 1200))), 1);
	Npc& elite = *spawnNpc(*hard, 214094, 130.0f, 176.0f, 99.7f, int8_t{0});
	handler.onDie(elite);
	EXPECT_EQ(s->getPoints(), 30);
	runtime::Ptr<Npc> kromede = npcs(*hard, 212846).empty() ? npcs(*hard, 214621).at(0) : npcs(*hard, 212846).at(0);
	handler.onDie(*kromede);
	EXPECT_TRUE(s->isFinished()) << "Kromede ends the run";
	EXPECT_EQ(s->getPoints(), 230);
	EXPECT_EQ(s->getRank(), 5) << "230 points, in time";
	EXPECT_EQ(given.size(), 1u) << "rank 5: two Greater Life Potions";
	EXPECT_EQ(given.at(0), std::make_tuple(player().getObjectId(), 162000005, int64_t{2}));
}

/** Fire Temple Hard's time out: the timer task ends the run at rank 8 after 20 minutes */
TEST_F(InstanceScoringTest, FireTempleHardTimesOutAfterTwentyMinutes) {
	configs::main::RuntimeConfig::INSTANCE_SCORING_ENABLE.store(true);
	static world::WorldMapInstance* hard = nullptr;
	static InstanceScoringTest* self = nullptr;
	self = this;
	custom::difficulty::DifficultyMenu::setEnterFunctionForTests(
		[](const model::templates::portal::PortalPath*, model::gameobjects::player::Player&, Npc&, int8_t) {
			hard = &self->createFireTemple();
		});
	world::WorldMapInstance& lobby = create(FIRE_TEMPLE, FireTempleInstance_instanceFactory);
	enter(lobby, 127.0f, 176.0f, 99.7f);
	runtime::Ref<custom::difficulty::DifficultyMenuOffer> offer = custom::difficulty::DifficultyMenuOffer::create(1, 1, *npcs(lobby).at(0), nullptr);
	custom::difficulty::DifficultyMenu::enter(player(), *offer, custom::difficulty::DifficultyMenu::HARD);
	auto& handler = dynamic_cast<FireTempleInstance&>(*hard->getInstanceHandler());
	handler.onEnterInstance(player());
	runtime::Ptr<InstanceScoring> s = handler.getScoring();
	executor().advance(std::chrono::milliseconds(1'199'000));
	EXPECT_FALSE(s->isFinished());
	executor().advance(std::chrono::milliseconds(1'000));
	EXPECT_TRUE(s->isFinished()) << "the timer task";
	EXPECT_EQ(s->getRank(), 8);
	EXPECT_TRUE(given.empty());
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
