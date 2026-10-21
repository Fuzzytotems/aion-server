// P6-I2 Stonespear Reach (game-server/data/handlers/instance/StonespearReachInstance.java) on a real instance of its map. The item and GP
// grants of the ranks above F (no item rows here: the F cases stay at or under 1000 points, which grants nothing), the teleports (they reach
// HousingService, which loads from the database) and LegionDominionService.onFinishInstance with a legion (LegionDominionServiceTest's) are
// left out.

#include "I2TestSupport.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "aion/gameserver/handlers/instance/StonespearReachInstance.h"
#include "aion/gameserver/model/instance/instancescore/LegionDominionScore.h"
#include "aion/gameserver/network/aion/instanceinfo/LegionDominionScoreWriter.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_SCORE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::instance::test {
namespace {

using model::gameobjects::Npc;
using network::aion::serverpackets::SM_INSTANCE_SCORE;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

constexpr int32_t PREPARING = 1048576, START_PROGRESS = 2097152, END_PROGRESS = 3145728; // InstanceProgressionType.java:8-10

/** SM_INSTANCE_SCORE with LegionDominionScoreWriter: mapId, time, progression, then the writer's 14 values */
struct Score {
	int32_t progression = 0;
	int32_t points = 0;
	int32_t gp = 0;
	int32_t ap = 0;
	int32_t item1 = 0, count1 = 0, item2 = 0, count2 = 0, item3 = 0, count3 = 0;
};

class StonespearReachTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		instance = &create(STONESPEAR_REACH, StonespearReachInstance_instanceFactory);
		enter(*instance, 231.0f, 264.0f, 96.5f);
		handler().onEnterInstance(player());
	}

	::aion::gameserver::instance::handlers::InstanceHandler& handler() { return *instance->getInstanceHandler(); }

	size_t count(int32_t npcId) { return npcs(*instance, npcId).size(); }

	size_t countAny(const std::vector<int32_t>& ids) {
		size_t n = 0;
		for (int32_t id : ids)
			n += count(id);
		return n;
	}

	/** an npc of the instance dies: the handler's onDie, as NpcController.onDie calls it */
	void dies(int32_t npcId) {
		runtime::Ptr<Npc> npc = spawnNpc(*instance, npcId, 240.0f, 264.0f, 96.5f, int8_t{0});
		handler().onDie(*npc);
		EXPECT_FALSE(npc->isSpawned()) << npcId << ": onDie deletes the npc (StonespearReachInstance.java:205)";
	}

	void start() {
		executor().advance(std::chrono::milliseconds(180000));
	}

	/** the SM_INSTANCE_SCORE packets sent since the last clearSent, decoded */
	std::vector<Score> scores() {
		runtime::Ref<LegionDominionScore> probe = LegionDominionScore::create();
		runtime::Ref<LegionDominionScoreWriter> writer = LegionDominionScoreWriter::create(*probe);
		const std::vector<uint8_t> shape = serialized(SM_INSTANCE_SCORE(STONESPEAR_REACH, *writer, 0));
		const size_t header = shape.size() - 17 * 4;
		std::vector<Score> result;
		for (const std::vector<uint8_t>& bytes : sent()) {
			if (bytes.size() != shape.size() || !std::equal(shape.begin(), shape.begin() + static_cast<std::ptrdiff_t>(header) + 4, bytes.begin()))
				continue;
			const auto d = [&](size_t index) {
				int32_t value = 0;
				std::memcpy(&value, bytes.data() + header + 4 * index, 4);
				return value;
			};
			result.push_back(Score{d(2), d(3), d(7), d(8), d(9), d(10), d(11), d(12), d(13), d(14)});
		}
		return result;
	}

	world::WorldMapInstance* instance = nullptr;
};

TEST_F(StonespearReachTest, TheHandlerIsTheJavaClassOfItsMap) {
	std::ifstream in(std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/handlers/instance/StonespearReachInstance.java");
	std::stringstream text;
	text << in.rdbuf();
	const std::string source = text.str();
	std::smatch match;
	ASSERT_TRUE(std::regex_search(source, match, std::regex(R"(@InstanceID\((\d+)\))")));
	EXPECT_EQ(std::stoi(match[1].str()), STONESPEAR_REACH);
	EXPECT_NE(dynamic_cast<StonespearReachInstance*>(&handler()), nullptr);
}

/** :57-73, 75-98, 285-302: the preparation's npc and score, the start after three minutes, the first stone and wave */
TEST_F(StonespearReachTest, PreparationThenTheFirstStageAfterThreeMinutes) {
	EXPECT_EQ(count(833284), 1u);
	std::vector<Score> entered = scores();
	ASSERT_EQ(entered.size(), 1u) << "onEnterInstance: the score while not rewarded";
	EXPECT_EQ(entered[0].progression, PREPARING);
	EXPECT_EQ(entered[0].points, 0);
	(*client)->clearSent();
	executor().advance(std::chrono::milliseconds(179999));
	EXPECT_EQ(count(833284), 1u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count(833284), 0u) << "startInstance: deleteAliveNpcs(833284)";
	std::vector<Score> started = scores();
	ASSERT_EQ(started.size(), 1u);
	EXPECT_EQ(started[0].progression, START_PROGRESS);
	EXPECT_EQ(count(855763), 1u) << "an Elyos legion's first stone";
	EXPECT_EQ(count(856466), 0u);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_OBJ_Start())), 1);
	EXPECT_EQ(countAny(stonespearTrash()), 0u);
	executor().advance(std::chrono::milliseconds(50));
	std::set<int32_t> wave;
	for (int32_t id : stonespearTrash())
		if (count(id) > 0)
			wave.insert(id);
	ASSERT_EQ(wave.size(), 1u) << "one npc id 855765 + Rnd.get(0, 8) at every point";
	EXPECT_GE(*wave.begin(), 855765);
	EXPECT_LE(*wave.begin(), 855773);
	EXPECT_EQ(count(*wave.begin()), 8u) << "the eight world points";
}

/** :669-675: no points while preparing */
TEST_F(StonespearReachTest, NoPointsWhilePreparing) {
	(*client)->clearSent();
	dies(855765);
	EXPECT_TRUE(scores().empty());
}

/** :100-206, 669-681: each kill adds its points and broadcasts the score and STR_MSG_GET_SCORE */
TEST_F(StonespearReachTest, KillsAddTheirPoints) {
	start();
	(*client)->clearSent();
	for (const auto& [npcId, points] : std::vector<std::pair<int32_t, int32_t>>{{855765, 100}, {855796, 200}, {855811, 300}, {855842, 400},
		     {855764, 500}, {855787, 1000}, {856303, 1500}, {855810, 1500}, {855833, 2000}, {856305, 12000}, {855799, 21000}, {855822, 30000}}) {
		const int32_t before = scores().empty() ? 0 : scores().back().points;
		dies(npcId);
		ASSERT_FALSE(scores().empty()) << npcId;
		EXPECT_EQ(scores().back().points - before, points) << npcId;
	}
	EXPECT_EQ(scores().back().progression, START_PROGRESS) << "no kill above ends the run";
	(*client)->clearSent();
	runtime::Ptr<Npc> npc = spawnNpc(*instance, 855770, 240.0f, 264.0f, 96.5f, int8_t{0});
	const std::string l10n(npc->getObjectTemplate()->getL10n());
	handler().onDie(*npc);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_GET_SCORE(l10n, 100))), 1) << "the npc's l10n name and its points";
}

/** :190-202, 208-268, 794-796, 851-856: a stone's death ranks the run (F at 100 points: nothing granted), clears the map, closes the door */
TEST_F(StonespearReachTest, AStonesDeathEndsTheRun) {
	start();
	dies(855765);
	(*client)->clearSent();
	dies(855763);
	std::vector<Score> ended = scores();
	ASSERT_EQ(ended.size(), 1u);
	EXPECT_EQ(ended[0].progression, END_PROGRESS);
	EXPECT_EQ(ended[0].points, 100);
	EXPECT_EQ(ended[0].item1, 0) << "F rank at 1000 points or less grants nothing";
	EXPECT_EQ(ended[0].gp, 0);
	EXPECT_TRUE(npcs(*instance).empty()) << "despawnAll";
	(*client)->clearSent();
	dies(855765);
	EXPECT_TRUE(scores().empty()) << "no points after the end";
	(*client)->clearSent();
	EXPECT_FALSE(handler().canEnter(player()));
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE(1402947, std::vector<std::string>{}))), 1);
	(*client)->clearSent();
	dies(856466);
	EXPECT_TRUE(scores().empty()) << "a second end is not rewarded again (reward.isRewarded())";
}

/** :208-257: no points at all is rank 8 (the score's items stay 0) */
TEST_F(StonespearReachTest, NoPointsAtTheEnd) {
	start();
	(*client)->clearSent();
	dies(856469);
	std::vector<Score> ended = scores();
	ASSERT_EQ(ended.size(), 1u);
	EXPECT_EQ(ended[0].progression, END_PROGRESS);
	EXPECT_EQ(ended[0].points, 0);
}

/** :320-340, 914-924: the four aetheric fields of stage 1 must all die within two minutes, or the run ends */
TEST_F(StonespearReachTest, TheAethericFieldsTimeOut) {
	start();
	executor().advance(std::chrono::milliseconds(119999));
	EXPECT_EQ(count(855764), 0u);
	(*client)->clearSent();
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count(855764), 4u) << "stage 1_3, two minutes after the start";
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_OBJ_Bomb())), 1);
	handler().onDie(*npcs(*instance, 855764).front()); // 500 points: 1 of 4
	(*client)->clearSent();
	executor().advance(std::chrono::milliseconds(119999));
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_OBJ_Bomb_Die())), 0);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_OBJ_Bomb_Die())), 1);
	ASSERT_FALSE(scores().empty());
	EXPECT_EQ(scores().back().progression, END_PROGRESS);
	EXPECT_EQ(scores().back().points, 500);
}

/** :914-924: three of the four fields are not enough (the map is cleared at the timeout; the player is out of the map, so the F rank's items
 * - 1500 points - are granted to no one) */
TEST_F(StonespearReachTest, ThreeFieldsOfFourAreNotEnough) {
	start();
	executor().advance(std::chrono::milliseconds(120000));
	std::vector<runtime::Ptr<Npc>> fields = npcs(*instance, 855764);
	ASSERT_EQ(fields.size(), 4u);
	for (size_t i = 0; i < 3; i++)
		handler().onDie(*fields[i]);
	world::World::getInstance().despawn(player());
	executor().advance(std::chrono::milliseconds(119999));
	EXPECT_FALSE(npcs(*instance).empty());
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_TRUE(npcs(*instance).empty()) << "checkRank's despawnAll";
}

/** :914-924: all four dead in time, the run goes on */
TEST_F(StonespearReachTest, TheAethericFieldsKilledInTime) {
	start();
	executor().advance(std::chrono::milliseconds(120000));
	for (runtime::Ptr<Npc>& field : npcs(*instance, 855764))
		handler().onDie(*field);
	(*client)->clearSent();
	executor().advance(std::chrono::milliseconds(120000));
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_OBJ_Bomb_Die())), 0);
	for (const Score& score : scores())
		EXPECT_EQ(score.progression, START_PROGRESS);
	EXPECT_EQ(count(855776), 1u) << "stage 1_4: Kaliga, three minutes after the start";
}

/** :851-881: before the end, a player without a legion is refused without a message while the instance has no legion */
/** :558-596 with the correction of 2026-10-19 (was J-I2-2): stage 4-3 starts 18 minutes into the run; its second wave's 855835 at point 1
 * comes at 9 s, with its neighbours (Java: 90 s). The aetheric fields of stages 1 to 3 are killed as they come, so the run goes on */
TEST_F(StonespearReachTest, TheStage43SecondWaveComesTogether) {
	start();
	(*client)->clearSent();
	for (int minute = 1; minute <= 18; minute++) {
		executor().advance(std::chrono::milliseconds(60000));
		for (int32_t fieldId : {855764, 855787, 855810, 855833})
			for (runtime::Ptr<Npc>& field : npcs(*instance, fieldId))
				handler().onDie(*field);
	}
	for (const Score& score : scores())
		ASSERT_EQ(score.progression, START_PROGRESS);
	executor().advance(std::chrono::milliseconds(8999));
	const size_t before = count(855835);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count(855835), before + 1) << "the 9 s spawn at point 1 (Java: 90 s)";
}

TEST_F(StonespearReachTest, NoLegionNoEntry) {
	(*client)->clearSent();
	EXPECT_FALSE(handler().canEnter(player()));
	EXPECT_TRUE(sent().empty());
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
