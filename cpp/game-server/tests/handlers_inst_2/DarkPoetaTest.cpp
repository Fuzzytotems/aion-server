// P6-I2 Dark Poeta (game-server/data/handlers/instance/DarkPoetaInstance.java) on a real instance of its map. The score is observed through
// the rank: the bonus boss Anuhart's death spawns (215280 to 215284 for ranks 1 to 5), whose thresholds the cases' kills cross or miss by a
// few points. Left out: the time limits of the ranks (the real clock; every case is well inside two hours), the leader's rank bonus (no
// registered team), a summon's kill (no master here), the doors (none in the test world) and leaveInstance (it moves the player out).

#include "I2TestSupport.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/handlers/instance/DarkPoetaInstance.h"
#include "aion/gameserver/model/gameobjects/NpcObjectType.h"
#include "aion/gameserver/model/gameobjects/Servant.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/spawnengine/VisibleObjectSpawner.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::instance::test {
namespace {

using model::gameobjects::Npc;

class DarkPoetaTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		instance = &create(DARK_POETA, DarkPoetaInstance_instanceFactory);
		enter(*instance, 1180.0f, 1235.0f, 143.0f);
		handler().onEnterInstance(player());
	}

	::aion::gameserver::instance::handlers::InstanceHandler& handler() { return *instance->getInstanceHandler(); }

	size_t count(int32_t npcId) { return npcs(*instance, npcId).size(); }

	runtime::Ptr<Npc> place(int32_t npcId) { return spawnNpc(*instance, npcId, 1180.0f, 1236.0f, 143.0f, int8_t{0}); }

	/** npcs die (hp 0: NpcController.onDie calls the handler's onDie) */
	void kill(int32_t npcId, int32_t times = 1) {
		for (int32_t i = 0; i < times; i++)
			place(npcId)->getLifeStats()->setCurrentHp(0);
	}

	void after(int64_t millis) { executor().advance(std::chrono::milliseconds(millis)); }

	void start() { handler().onOpenDoor(33); }

	/** the rank boss Anuhart's death leaves (215280 for rank 1 ... 215284 for rank 5) */
	int32_t rankBoss() {
		for (int32_t id = 215280; id <= 215284; id++) {
			if (count(id) > 0)
				return id;
		}
		return 0;
	}

	world::WorldMapInstance* instance = nullptr;
};

TEST_F(DarkPoetaTest, TheHandlerIsTheJavaClassOfItsMap) {
	std::ifstream in(std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/handlers/instance/DarkPoetaInstance.java");
	std::stringstream text;
	text << in.rdbuf();
	const std::string source = text.str();
	std::smatch match;
	ASSERT_TRUE(std::regex_search(source, match, std::regex(R"(@InstanceID\((\d+)\))")));
	EXPECT_EQ(std::stoi(match[1].str()), DARK_POETA);
	EXPECT_NE(dynamic_cast<DarkPoetaInstance*>(&handler()), nullptr);
}

/** DarkPoetaInstance.java:95-116, 122-242: 4 x 1241 + Anuhart's 954 = 5918 is rank 5; the portal goes after three minutes */
TEST_F(DarkPoetaTest, RankFive) {
	start();
	kill(281116, 4);
	kill(214904);
	EXPECT_EQ(rankBoss(), 215284);
	EXPECT_EQ(count(700478), 1u);
	after(179999);
	EXPECT_EQ(count(700478), 1u);
	after(1);
	EXPECT_EQ(count(700478), 0u) << "the portal after 3 minutes";
}

/** :104-107: 5 x 1241 + 954 = 7159 > 6656 is rank 4 */
TEST_F(DarkPoetaTest, RankFour) {
	start();
	kill(281116, 5);
	kill(214904);
	EXPECT_EQ(rankBoss(), 215283);
}

/** :101-103: 9 x 1241 + 954 = 12123 > 10913 is rank 3 */
TEST_F(DarkPoetaTest, RankThree) {
	start();
	kill(281116, 9);
	kill(214904);
	EXPECT_EQ(rankBoss(), 215282);
}

/** :98-100: 12 x 1241 + 954 = 15846 >= 15219 is rank 2 */
TEST_F(DarkPoetaTest, RankTwo) {
	start();
	kill(281116, 12);
	kill(214904);
	EXPECT_EQ(rankBoss(), 215281);
}

/** :95-97: 14 x 1241 + 954 = 18328 >= 17817 is rank 1 */
TEST_F(DarkPoetaTest, RankOne) {
	start();
	kill(281116, 14);
	kill(214904);
	EXPECT_EQ(rankBoss(), 215280);
}

/** :45-57, 272-279: kills before the start count nothing */
TEST_F(DarkPoetaTest, KillsBeforeTheStartDoNotCount) {
	kill(281116, 14);
	start();
	kill(214904);
	EXPECT_EQ(rankBoss(), 215284) << "only Anuhart's 954";
}

/** :272-279, 244-250: two minutes after the creation the instance starts by itself */
TEST_F(DarkPoetaTest, TheTimerStartsIt) {
	after(120999);
	kill(281116, 14);
	after(1);
	kill(281116, 14);
	kill(214904);
	EXPECT_EQ(rankBoss(), 215280) << "the kills after 121 s count";
}

/** :257-264: only door 33 starts it */
TEST_F(DarkPoetaTest, OnlyDoor33StartsIt) {
	handler().onOpenDoor(32);
	kill(281116, 14);
	kill(214904);
	EXPECT_EQ(rankBoss(), 215284);
}

/** :50, 272-279: the boosters and 281178 count nothing; any other npc 11 points (4 x 1241 + 67 x 11 + 954 = 6655, then one more) */
TEST_F(DarkPoetaTest, ExcludedNpcsAndTheDefaultPoints) {
	start();
	kill(281116, 4);
	for (int32_t id : {700439, 700440, 700441, 700442, 700443, 700444, 700445, 700446, 700447, 281178})
		kill(id, 10);
	kill(215262, 67);
	kill(214904);
	EXPECT_EQ(rankBoss(), 215284) << "6655 is not > 6656";
}

TEST_F(DarkPoetaTest, OneMoreKillIsRankFour) {
	start();
	kill(281116, 4);
	kill(215262, 68);
	kill(214904);
	EXPECT_EQ(rankBoss(), 215283) << "6666 > 6656";
}

/** :187-242: the special npcs' points add up to 6758 > 6656: rank 4 */
TEST_F(DarkPoetaTest, SpecialPoints) {
	start();
	kill(281116, 4); // 4964
	kill(214843);    // Atmach 456: 5420
	kill(700517, 2); // two walls 312: 5732
	kill(214841);    // -209: 5523
	kill(215431);    // 208: 5731
	kill(214885);    // 21: 5752
	kill(700520);    // 52: 5804
	kill(214904);    // 954: 6758 > 6656
	EXPECT_EQ(rankBoss(), 215283);
}

/** :122-242 at the rank 2 boundary: 10 x 1241 + 156 + 456 + 113 x 11 + 954 = 15219 exactly, so one point less is rank 3 */
TEST_F(DarkPoetaTest, ExactlyRankTwo) {
	start();
	kill(281116, 10);
	kill(700517);
	kill(214843);
	kill(215262, 113);
	kill(214904);
	EXPECT_EQ(rankBoss(), 215281);
}

/** :155-157 at the rank 1 boundary: 12 x 1241 + 156 + 2 x 456 - 209 + 101 x 11 + 954 = 17816, one short of rank 1 */
TEST_F(DarkPoetaTest, NamedOneTakesPoints) {
	start();
	kill(281116, 12);
	kill(700517);
	kill(214843, 2);
	kill(214841);
	kill(215262, 101);
	kill(214904);
	EXPECT_EQ(rankBoss(), 215281) << "214841's -209 keeps it below 17817";
}

/** :46-48: a kill by a player's servant counts nothing */
TEST_F(DarkPoetaTest, AServantsKillCountsNothing) {
	start();
	for (int32_t i = 0; i < 14; i++) {
		runtime::Ref<model::templates::spawns::SpawnTemplate> spawn =
			spawnengine::SpawnEngine::newSingleTimeSpawn(DARK_POETA, 281116, 1180.0f, 1236.0f, 143.0f, int8_t{0});
		runtime::Ref<model::gameobjects::Servant> servant = spawnengine::VisibleObjectSpawner::spawnServant(*spawn, instance->getInstanceId(),
			player(), 30, model::gameobjects::NpcObjectType::SERVANT);
		ASSERT_EQ(servant->getMaster(), runtime::Ptr<model::gameobjects::Creature>(player()));
		handler().onDie(*servant);
	}
	kill(214904);
	EXPECT_EQ(rankBoss(), 215284) << "only Anuhart's 954";
}

/** :59-64: the three generators call Anuhart */
TEST_F(DarkPoetaTest, TheGenerators) {
	start();
	kill(214895);
	kill(214896);
	EXPECT_EQ(count(214904), 0u);
	kill(214897);
	EXPECT_EQ(count(214904), 1u);
	kill(214895);
	EXPECT_EQ(count(214904), 1u) << "only the third";
}

/** :70-76: a bonus boss's death opens the exit */
TEST_F(DarkPoetaTest, TheExit) {
	kill(215283);
	EXPECT_EQ(count(730211), 1u);
	kill(214843);
	EXPECT_EQ(count(730211), 1u);
}

/** :293-300: the kobold bomb destroys the camouflaged wall within 10 m */
TEST_F(DarkPoetaTest, TheKoboldBomb) {
	dataholders::DataManager::SKILL_DATA.resetForTests();
	xml::LoadContext skillContext;
	std::string skills = "<skill_data>";
	for (int32_t id : {18130, 18131})
		skills += R"(<skill_template skill_id=")" + std::to_string(id) +
			R"(" name="bomb" nameId="1" stack="BOMB" lvl="1" skilltype="MAGICAL" skillsubtype="NONE" tslot="SPEC" activation="ACTIVE")"
			R"( cooldown="0" duration="0"><properties first_target="ME" target_relation="ALL" target_type="ONLYONE" /></skill_template>)";
	dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext, skills + "</skill_data>"));
	runtime::Ptr<Npc> wall = place(700516);
	runtime::Ptr<Npc> kobold = spawnNpc(*instance, 215262, 1180.0f, 1245.0f, 143.0f, int8_t{0});
	runtime::Ptr<Npc> far = spawnNpc(*instance, 215262, 1180.0f, 1260.0f, 143.0f, int8_t{0});
	handler().onEndCastSkill(*skillengine::SkillEngine::getInstance().getSkill(*far, 18130, 1, runtime::Ptr<model::gameobjects::VisibleObject>(*far)));
	EXPECT_FALSE(wall->isDead()) << "24 m away";
	handler().onEndCastSkill(*skillengine::SkillEngine::getInstance().getSkill(*kobold, 18131, 1, runtime::Ptr<model::gameobjects::VisibleObject>(*kobold)));
	EXPECT_FALSE(wall->isDead()) << "another skill";
	handler().onEndCastSkill(*skillengine::SkillEngine::getInstance().getSkill(*kobold, 18130, 1, runtime::Ptr<model::gameobjects::VisibleObject>(*kobold)));
	EXPECT_TRUE(wall->isDead());
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
