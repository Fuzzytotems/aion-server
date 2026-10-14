// P6I-3 (docs/design/p6-instances-plan.md §4): the Dredgions of chunk I1 - DredgionInstance and BaranathDredgionInstance,
// ChantraDredgionInstance, TerathDredgionInstance (game-server/data/handlers/instance/dredgion/*.java) - on real instances of their maps,
// created through the AION_INSTANCE_HANDLER factories, with the fixture's DeterministicExecutor driving the start (2 minutes) and the scheduled
// spawns. The reward paths (AP, items, quests) are left to a gate: no case lets a player in the instance get rewarded.

#include "InstanceHandlerTestSupport.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/handlers/instance/dredgion/BaranathDredgionInstance.h"
#include "aion/gameserver/handlers/instance/dredgion/ChantraDredgionInstance.h"
#include "aion/gameserver/handlers/instance/dredgion/TerathDredgionInstance.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/instance/InstanceProgressionType.h"
#include "aion/gameserver/model/instance/instancescore/PvpInstanceScore.h"
#include "aion/gameserver/model/instance/playerreward/PvpInstancePlayerReward.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::instance::test {
namespace {

using model::gameobjects::Npc;
using model::instance::InstanceProgressionType;
using model::instance::instancescore::PvpInstanceScore;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

class DredgionTest : public InstanceHandlerTest {
protected:
	static PvpInstanceScore& scoreOf(world::WorldMapInstance& instance) {
		return dynamic_cast<PvpInstanceScore&>(*instance.getInstanceHandler()->getInstanceScore());
	}

	/** an npc of the instance hit by the player, then dead through the handler */
	runtime::Ptr<Npc> killed(world::WorldMapInstance& instance, int32_t npcId) {
		runtime::Ptr<Npc> npc = spawnNpc(instance, npcId, 480.0f, 400.0f, 400.0f, int8_t{0});
		hit(*npc);
		instance.getInstanceHandler()->onDie(*npc);
		return npc;
	}

	/** the room states: the last 14 bytes of the last SM_INSTANCE_SCORE sent (DredgionScoreWriter writes them last) */
	std::vector<uint8_t> roomStates() {
		std::vector<std::vector<uint8_t>> bytes = sent();
		for (auto it = bytes.rbegin(); it != bytes.rend(); ++it) {
			if (it->size() > 88u * 12)
				return std::vector<uint8_t>(it->end() - 14, it->end());
		}
		return {};
	}
};

/** The three subclasses are the Java classes of their maps; DredgionInstance itself has no @InstanceID */
TEST_F(DredgionTest, EachDredgionIsTheJavaClassOfItsMap) {
	const std::map<std::string, int32_t> expected{{"BaranathDredgionInstance", BARANATH_DREDGION}, {"ChantraDredgionInstance", CHANTRA_DREDGION},
		{"TerathDredgionInstance", TERATH_DREDGION}};
	const std::filesystem::path javaDir = std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/handlers/instance/dredgion";
	const std::regex instanceId(R"(@InstanceID\((\d+)\))");
	for (const auto& [javaClass, mapId] : expected) {
		std::ifstream in(javaDir / (javaClass + ".java"));
		ASSERT_TRUE(in) << javaClass;
		std::stringstream text;
		text << in.rdbuf();
		std::string source = text.str();
		std::smatch match;
		ASSERT_TRUE(std::regex_search(source, match, instanceId)) << javaClass;
		EXPECT_EQ(std::stoi(match[1].str()), mapId) << javaClass;
	}
	std::ifstream base(javaDir / "DredgionInstance.java");
	std::stringstream baseText;
	baseText << base.rdbuf();
	EXPECT_EQ(baseText.str().find("@InstanceID"), std::string::npos);
	using namespace dredgion;
	EXPECT_NE(dynamic_cast<BaranathDredgionInstance*>(create(BARANATH_DREDGION, BaranathDredgionInstance_instanceFactory).getInstanceHandler().get()),
		nullptr);
	EXPECT_NE(dynamic_cast<ChantraDredgionInstance*>(create(CHANTRA_DREDGION, ChantraDredgionInstance_instanceFactory).getInstanceHandler().get()), nullptr);
	EXPECT_NE(dynamic_cast<TerathDredgionInstance*>(create(TERATH_DREDGION, TerathDredgionInstance_instanceFactory).getInstanceHandler().get()), nullptr);
}

/** DredgionInstance.java:79-107, Baranath :23-34, :120-122: the score at creation, the first entry's random guard and the start 2 minutes on */
TEST_F(DredgionTest, BaranathTheFirstEntryStartsTheRun) {
	world::WorldMapInstance& first = create(BARANATH_DREDGION, dredgion::BaranathDredgionInstance_instanceFactory);
	enter(first, 480.0f, 400.0f, 400.0f);
	PvpInstanceScore& score = scoreOf(first);
	EXPECT_EQ(score.getWinnerApReward(), 3000);
	EXPECT_EQ(score.getLoserApReward(), 1500);
	EXPECT_EQ(score.getDrawApReward(), 2250);
	EXPECT_EQ(score.getInstanceProgressionType(), InstanceProgressionType::PREPARING);

	// the guard: 51 % at one of two places, decided at the first entry only
	const std::set<std::tuple<float, float>> places{{416.3429f, 282.32785f}, {552.07446f, 289.058f}};
	std::map<std::string, int> outcomes;
	for (uint64_t seed = 1; seed <= 30; ++seed) {
		commons::utils::Rnd::seedCurrentThreadForTests(seed);
		world::WorldMapInstance& instance = create(BARANATH_DREDGION, dredgion::BaranathDredgionInstance_instanceFactory);
		instance.getInstanceHandler()->onEnterInstance(player());
		instance.getInstanceHandler()->onEnterInstance(player());
		std::vector<runtime::Ptr<Npc>> guards = npcs(instance, 215093);
		ASSERT_LE(guards.size(), 1u) << "seed " << seed;
		if (guards.empty()) {
			++outcomes["none"];
		} else {
			ASSERT_TRUE(places.contains({guards.front()->getX(), guards.front()->getY()})) << "seed " << seed;
			EXPECT_EQ(guards.front()->getHeading(), 80);
			++outcomes[std::to_string(guards.front()->getX())];
		}
		EXPECT_TRUE(scoreOf(instance).containsPlayer(player().getObjectId()));
	}
	EXPECT_EQ(outcomes.size(), 3u) << "none and both places";

	first.getInstanceHandler()->onEnterInstance(player());
	EXPECT_TRUE(score.containsPlayer(player().getObjectId()));
	executor().advance(std::chrono::milliseconds(119999));
	EXPECT_EQ(score.getInstanceProgressionType(), InstanceProgressionType::PREPARING);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(score.getInstanceProgressionType(), InstanceProgressionType::START_PROGRESS);
	EXPECT_EQ(roomStates(), std::vector<uint8_t>(14, 0xFF)) << "14 rooms, none captured";
}

/** Baranath :36-48, :50-117: before the start a death counts nothing; a Surkana's death captures its room for the race of the player who
 * did the most damage, scores, and goes; the fifth brings the captain */
TEST_F(DredgionTest, BaranathSurkanasCaptureRoomsAndTheFifthBringsTheCaptain) {
	world::WorldMapInstance& instance = create(BARANATH_DREDGION, dredgion::BaranathDredgionInstance_instanceFactory);
	enter(instance, 480.0f, 400.0f, 400.0f);
	instance.getInstanceHandler()->onEnterInstance(player());
	PvpInstanceScore& score = scoreOf(instance);
	runtime::Ptr<Npc> early = killed(instance, 700485);
	EXPECT_TRUE(early->isSpawned()) << "PREPARING: no arm";
	EXPECT_EQ(score.getElyosPoints(), 0);

	executor().advance(std::chrono::milliseconds(120000));
	ASSERT_EQ(score.getInstanceProgressionType(), InstanceProgressionType::START_PROGRESS);
	(*client)->clearSent();
	runtime::Ptr<Npc> surkana = killed(instance, 700498); // room 700498 + 14 - 700498 = 14
	EXPECT_FALSE(surkana->isSpawned());
	EXPECT_EQ(score.getElyosPoints(), 700);
	runtime::Ptr<model::instance::playerreward::PvpInstancePlayerReward> reward =
		runtime::cast<model::instance::playerreward::PvpInstancePlayerReward>(score.getPlayerReward(player().getObjectId()));
	EXPECT_EQ(reward->getCapturedZones(), 1);
	EXPECT_EQ(reward->getPoints(), 700);
	std::vector<uint8_t> rooms(14, 0xFF);
	rooms[13] = 0x00; // captured by an Elyos
	EXPECT_EQ(roomStates(), rooms);

	for (int32_t id : {700485, 700486, 700495}) // 500 each; rooms 1, 2, 11
		killed(instance, id);
	EXPECT_EQ(score.getElyosPoints(), 700 + 3 * 500);
	EXPECT_TRUE(npcs(instance, 214823).empty()) << "four Surkanas";
	killed(instance, 700487); // 900, room 3
	ASSERT_EQ(npcs(instance, 214823).size(), 1u) << "the fifth";
	EXPECT_FLOAT_EQ(npcs(instance, 214823).front()->getX(), 485.423f);
	EXPECT_EQ(reward->getCapturedZones(), 5);
	EXPECT_EQ(score.getElyosPoints(), 700 + 3 * 500 + 900);
	rooms[0] = rooms[1] = rooms[2] = rooms[10] = 0x00;
	EXPECT_EQ(roomStates(), rooms);
}

/** Baranath :50-117 for the hatches, DredgionInstance.java:249-261 for the rest (hp gauge 0: 12 points) */
TEST_F(DredgionTest, BaranathTheHatchesOpenForTheRaceAndOtherNpcsScoreByHpGauge) {
	world::WorldMapInstance& instance = create(BARANATH_DREDGION, dredgion::BaranathDredgionInstance_instanceFactory);
	enter(instance, 480.0f, 400.0f, 400.0f);
	instance.getInstanceHandler()->onEnterInstance(player());
	executor().advance(std::chrono::milliseconds(120000));
	killed(instance, 700508);
	ASSERT_EQ(npcs(instance, 700502).size(), 1u);
	EXPECT_FLOAT_EQ(npcs(instance, 700502).front()->getX(), 520.88f) << "the Elyos side";
	killed(instance, 700506);
	ASSERT_EQ(npcs(instance, 730214).size(), 1u);
	EXPECT_FLOAT_EQ(npcs(instance, 730214).front()->getX(), 567.59f);
	int32_t before = scoreOf(instance).getElyosPoints();
	runtime::Ptr<Npc> other = killed(instance, 216889);
	EXPECT_EQ(scoreOf(instance).getElyosPoints(), before + 12);
	EXPECT_TRUE(other->isSpawned()) << "the base onDie deletes nothing";
}

/** DredgionInstance.java:109-117 through Baranath's captain (:83-90): the end, the player's reward, every npc gone */
TEST_F(DredgionTest, BaranathTheCaptainsDeathEndsTheRun) {
	world::WorldMapInstance& instance = create(BARANATH_DREDGION, dredgion::BaranathDredgionInstance_instanceFactory);
	enter(instance, 480.0f, 400.0f, 400.0f);
	instance.getInstanceHandler()->onEnterInstance(player());
	executor().advance(std::chrono::milliseconds(120000));
	runtime::Ptr<Npc> captain = spawnNpc(instance, 214823, 485.423f, 808.826f, 416.868f, int8_t{30});
	hit(*captain);
	runtime::Ptr<Npc> bystander = spawnNpc(instance, 216889, 480.0f, 400.0f, 400.0f, int8_t{0});
	instance.getInstanceHandler()->onDie(*captain);
	EXPECT_EQ(scoreOf(instance).getInstanceProgressionType(), InstanceProgressionType::END_PROGRESS);
	EXPECT_EQ(scoreOf(instance).getElyosPoints(), 1000);
	EXPECT_FALSE(bystander->isSpawned()) << "stopInstance deletes every npc";
	EXPECT_FALSE(captain->isSpawned());
	runtime::Ptr<model::instance::playerreward::PvpInstancePlayerReward> reward =
		runtime::cast<model::instance::playerreward::PvpInstancePlayerReward>(scoreOf(instance).getPlayerReward(player().getObjectId()));
	EXPECT_EQ(reward->getBaseAp(), 3000) << "the winners' AP";
	EXPECT_EQ(reward->getBonusAp(), 2 * 1000 / 6) << "2 * the race's points / 6";
}

/** DredgionInstance.java:163-180: a death without an enemy killer costs the victim (and the victim's race, down to 0) 60 points */
TEST_F(DredgionTest, ADeathCostsSixtyPoints) {
	world::WorldMapInstance& instance = create(BARANATH_DREDGION, dredgion::BaranathDredgionInstance_instanceFactory);
	enter(instance, 480.0f, 400.0f, 400.0f);
	instance.getInstanceHandler()->onEnterInstance(player());
	EXPECT_TRUE(instance.getInstanceHandler()->onDie(player(), player()));
	EXPECT_EQ(scoreOf(instance).getElyosPoints(), 0) << "a race's points do not go below 0 (PvpInstanceScore.addPointsByRace)";
	EXPECT_EQ(scoreOf(instance).getPlayerReward(player().getObjectId())->getPoints(), -60);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_GET_SCORE(player().getName(), -60))), 1);
}

/** Chantra :69-90: before the start nothing; the escape hatch opens with its static id and goes */
TEST_F(DredgionTest, ChantraTheEscapeHatch) {
	world::WorldMapInstance& instance = create(CHANTRA_DREDGION, dredgion::ChantraDredgionInstance_instanceFactory);
	enter(instance, 480.0f, 400.0f, 400.0f);
	PvpInstanceScore& score = scoreOf(instance);
	EXPECT_EQ(score.getWinnerApReward(), 4500) << "DredgionInstance's onInstanceCreate";
	instance.getInstanceHandler()->onEnterInstance(player());
	runtime::Ptr<Npc> early = spawnNpc(instance, 730349, 480.0f, 400.0f, 400.0f, int8_t{0});
	instance.getInstanceHandler()->onDie(*early);
	EXPECT_TRUE(early->isSpawned());
	executor().advance(std::chrono::milliseconds(120000));
	runtime::Ptr<Npc> hatch = spawnNpc(instance, 730349, 480.0f, 400.0f, 400.0f, int8_t{0});
	instance.getInstanceHandler()->onDie(*hatch);
	EXPECT_FALSE(hatch->isSpawned());
	ASSERT_EQ(npcs(instance, 730314).size(), 1u);
	EXPECT_EQ(npcs(instance, 730314).front()->getSpawn()->getStaticId(), 42);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE(1400631, std::vector<std::string>{}))), 1);
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
