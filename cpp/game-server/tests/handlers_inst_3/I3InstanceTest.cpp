// P6I-5 (docs/design/p6-instances-plan.md §4): OphidanBridgeInstance (+ _L), LinkgateFoundryInstance and KromedesTrialInstance
// (game-server/data/handlers/instance) on real instances of their maps. Teleports (they reach HousingService, which loads from the database),
// zone entries (no zones here) and the skill effects (no skill rows) are left to a gate.

#include "I3TestSupport.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/handlers/instance/KromedesTrialInstance.h"
#include "aion/gameserver/handlers/instance/LinkgateFoundryInstance.h"
#include "aion/gameserver/handlers/instance/OphidanBridgeInstance.h"
#include "aion/gameserver/handlers/instance/OphidanBridgeInstance_L.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::instance::test {
namespace {

using model::gameobjects::Npc;
using model::gameobjects::player::Player;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

class I3InstanceTest : public InstanceHandlerTest {
protected:
	world::WorldMapInstance& ophidan() { return create(OPHIDAN_BRIDGE, OphidanBridgeInstance_instanceFactory); }
	world::WorldMapInstance& linkgate() { return create(LINKGATE_FOUNDRY, LinkgateFoundryInstance_instanceFactory); }
	world::WorldMapInstance& kromede() { return create(KROMEDES_TRIAL, KromedesTrialInstance_instanceFactory); }

	runtime::Ptr<Npc> at(world::WorldMapInstance& instance, int32_t npcId, float x = 300.0f, float y = 300.0f) {
		return spawnNpc(instance, npcId, x, y, 300.0f, int8_t{0});
	}

	runtime::Ptr<Npc> dies(world::WorldMapInstance& instance, int32_t npcId) {
		runtime::Ptr<Npc> npc = at(instance, npcId);
		instance.getInstanceHandler()->onDie(*npc);
		return npc;
	}

	void use(world::WorldMapInstance& instance, Npc& npc) {
		instance.getInstanceHandler()->handleUseItemFinish(runtime::Ptr<Player>(player()), npc);
	}

	static size_t count(world::WorldMapInstance& instance, std::initializer_list<int32_t> ids) {
		size_t n = 0;
		for (int32_t id : ids)
			n += npcs(instance, id).size();
		return n;
	}

	static std::string javaSource(const char* javaClass) {
		std::ifstream in(std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/handlers/instance" / (std::string(javaClass) + ".java"));
		std::stringstream text;
		text << in.rdbuf();
		return text.str();
	}
};

TEST_F(I3InstanceTest, EachHandlerIsTheJavaClassOfItsMap) {
	for (const auto& [javaClass, mapId] : std::map<std::string, int32_t>{{"OphidanBridgeInstance", OPHIDAN_BRIDGE},
		     {"OphidanBridgeInstance_L", LUCKY_OPHIDAN_BRIDGE}, {"LinkgateFoundryInstance", LINKGATE_FOUNDRY}, {"KromedesTrialInstance", KROMEDES_TRIAL}}) {
		std::string source = javaSource(javaClass.c_str());
		std::smatch match;
		ASSERT_TRUE(std::regex_search(source, match, std::regex(R"(@InstanceID\((\d+)\))"))) << javaClass;
		EXPECT_EQ(std::stoi(match[1].str()), mapId) << javaClass;
	}
	EXPECT_NE(dynamic_cast<OphidanBridgeInstance*>(ophidan().getInstanceHandler().get()), nullptr);
	world::WorldMapInstance& lucky = create(LUCKY_OPHIDAN_BRIDGE, OphidanBridgeInstance_L_instanceFactory);
	EXPECT_NE(dynamic_cast<OphidanBridgeInstance_L*>(lucky.getInstanceHandler().get()), nullptr);
	EXPECT_NE(dynamic_cast<LinkgateFoundryInstance*>(linkgate().getInstanceHandler().get()), nullptr);
	EXPECT_NE(dynamic_cast<KromedesTrialInstance*>(kromede().getInstanceHandler().get()), nullptr);
}

/** OphidanBridgeInstance.java:51-67, 95-103: in hard mode a boss's death removes Escapee Asachin; the third brings the bridge control and one
 * of the three end boss pairs */
TEST_F(I3InstanceTest, OphidansHardModeBringsARandomBossPair) {
	world::WorldMapInstance& instance = ophidan();
	at(instance, 235764);
	dies(instance, 235775);
	EXPECT_TRUE(npcs(instance, 235764).empty()) << "hard mode: Asachin goes";
	dies(instance, 235774);
	EXPECT_EQ(count(instance, {701644}), 0u);
	dies(instance, 235773);
	EXPECT_EQ(count(instance, {701644}), 1u);
	EXPECT_EQ(count(instance, {235759, 235763, 235767}), 1u);
	EXPECT_EQ(count(instance, {235769}), count(instance, {235759}));
	EXPECT_EQ(count(instance, {235770}), count(instance, {235763}));
	EXPECT_EQ(count(instance, {235771}), count(instance, {235767}));
	EXPECT_EQ(count(instance, {235768}), 0u);
	dies(instance, 235772);
	EXPECT_EQ(count(instance, {701644}), 1u) << "only the third";
}

/** :95-103: each random end boss comes with its own Velkur (twelve hard mode bridges: all three pairs, most likely) */
TEST_F(I3InstanceTest, OphidansRandomBossesComeInPairs) {
	for (int32_t round = 0; round < 12; ++round) {
		world::WorldMapInstance& instance = ophidan();
		dies(instance, 235775);
		dies(instance, 235774);
		dies(instance, 235773);
		EXPECT_EQ(count(instance, {235769}), count(instance, {235759})) << round;
		EXPECT_EQ(count(instance, {235770}), count(instance, {235763})) << round;
		EXPECT_EQ(count(instance, {235771}), count(instance, {235767})) << round;
	}
}

/** :48-50, 61-65, 76-77: Asachin's death is the normal mode: Spirited Velkur alone, whose death opens the exit */
TEST_F(I3InstanceTest, OphidansNormalModeHasOneEndBoss) {
	world::WorldMapInstance& instance = ophidan();
	runtime::Ptr<Npc> asachin = dies(instance, 235764);
	dies(instance, 235775);
	EXPECT_TRUE(asachin->isSpawned()) << "normal mode: Asachin stays";
	dies(instance, 235774);
	dies(instance, 235773);
	EXPECT_EQ(count(instance, {235768}), 1u);
	EXPECT_EQ(count(instance, {235759, 235763, 235767, 235769, 235770, 235771}), 0u);
	dies(instance, 235768);
	EXPECT_EQ(count(instance, {730868}), 1u);
	executor().advance(std::chrono::milliseconds(10000));
	EXPECT_EQ(count(instance, {856059}), 0u);
}

/** :75-80, 88-93: in hard mode the first end boss's death starts the enrage, which comes if the second still lives 10 s later */
TEST_F(I3InstanceTest, OphidansEnrageComesWhenTheSecondEndBossLives) {
	world::WorldMapInstance& instance = ophidan();
	dies(instance, 235759);
	executor().advance(std::chrono::milliseconds(9999));
	EXPECT_EQ(count(instance, {856059}), 0u);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(count(instance, {856059}), 1u);
	EXPECT_EQ(count(instance, {730868}), 0u);
	dies(instance, 235769);
	EXPECT_EQ(count(instance, {730868}), 1u) << "the second end boss opens the exit";
}

/** :76-80, 88-93: both end bosses within 10 s: the exit, no enrage */
TEST_F(I3InstanceTest, OphidansEnrageStaysAwayWhenBothEndBossesDie) {
	world::WorldMapInstance& instance = ophidan();
	dies(instance, 235763);
	dies(instance, 235770);
	EXPECT_EQ(count(instance, {730868}), 1u);
	executor().advance(std::chrono::milliseconds(10000));
	EXPECT_EQ(count(instance, {856059}), 0u);
}

/** :37-42: the destroy cancels the enrage */
TEST_F(I3InstanceTest, OphidansDestroyCancelsTheEnrage) {
	world::WorldMapInstance& instance = ophidan();
	dies(instance, 235767);
	instance.getInstanceHandler()->onInstanceDestroy();
	executor().advance(std::chrono::milliseconds(10000));
	EXPECT_EQ(count(instance, {856059}), 0u);
}

/** :82-84, 108-113: the Steel Wall goes when it dies; the bridge control turns into the bridge once */
TEST_F(I3InstanceTest, OphidansBridgeControlAndSteelWall) {
	world::WorldMapInstance& instance = ophidan();
	enter(instance, 436.0f, 496.0f, 605.0f);
	runtime::Ptr<Npc> wall = dies(instance, 235786);
	EXPECT_FALSE(wall->isSpawned());
	runtime::Ptr<Npc> control = at(instance, 701644, 436.0f, 496.0f);
	use(instance, *control);
	EXPECT_FALSE(control->isSpawned());
	ASSERT_EQ(npcs(instance, 731544).size(), 1u);
	EXPECT_EQ(npcs(instance, 731544)[0]->getSpawn()->getStaticId(), 7);
	runtime::Ptr<Npc> second = at(instance, 701644, 436.0f, 496.0f);
	use(instance, *second);
	EXPECT_TRUE(second->isSpawned()) << "used once";
	EXPECT_EQ(npcs(instance, 731544).size(), 1u);
}

/** The Lucky Ophidan Bridge is the same handler */
TEST_F(I3InstanceTest, TheLuckyBridgeIsTheSameBridge) {
	world::WorldMapInstance& instance = create(LUCKY_OPHIDAN_BRIDGE, OphidanBridgeInstance_L_instanceFactory);
	dies(instance, 235764);
	dies(instance, 235775);
	dies(instance, 235774);
	dies(instance, 235773);
	EXPECT_EQ(count(instance, {235768}), 1u);
}

/** LinkgateFoundryInstance.java:33-46: the first player's race picks the entrance npc and Belsagos' reward */
TEST_F(I3InstanceTest, LinkgatesFirstPlayerPicksTheRace) {
	world::WorldMapInstance& instance = linkgate();
	enter(instance, 348.0f, 252.0f, 311.0f);
	instance.getInstanceHandler()->onEnterInstance(player()); // InstanceService.onInstanceEnter's call
	EXPECT_EQ(count(instance, {206361}), 1u) << "an Elyos";
	EXPECT_EQ(count(instance, {206362}), 0u);
	instance.getInstanceHandler()->onEnterInstance(player());
	EXPECT_EQ(count(instance, {206361}), 1u) << "only the first entry";
	dies(instance, 234990);
	EXPECT_EQ(count(instance, {702338}), 1u);
	EXPECT_EQ(count(instance, {702389}), 0u);
}

/** :51-58, 77-100: the door's use starts the 20 minutes; their messages; at the end everything but Belsagos, the teleporter and the secret lab
 * goes */
TEST_F(I3InstanceTest, LinkgatesTimerClearsTheFoundry) {
	world::WorldMapInstance& instance = linkgate();
	enter(instance, 348.0f, 252.0f, 311.0f);
	runtime::Ptr<Npc> door = at(instance, 804578);
	runtime::Ptr<Npc> plain = at(instance, 900200);
	runtime::Ptr<Npc> belsagos = at(instance, 233898);
	runtime::Ptr<Npc> chest = at(instance, 234992);
	use(instance, *door);
	EXPECT_TRUE(door->isDead());
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF4_Re_01_Time_01())), 1);
	executor().advance(std::chrono::milliseconds(0));
	executor().advance(std::chrono::minutes(5));
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF4_Re_01_Time_02())), 1);
	executor().advance(std::chrono::minutes(14));
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF4_Re_01_Time_06())), 1);
	EXPECT_TRUE(plain->isSpawned());
	executor().advance(std::chrono::minutes(1));
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF4_Re_01_Time_07())), 1);
	EXPECT_FALSE(plain->isSpawned());
	EXPECT_TRUE(belsagos->isSpawned());
	EXPECT_TRUE(chest->isSpawned());
	executor().advance(std::chrono::minutes(260)); // a running timer's byte would wrap and reach 5 again
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF4_Re_01_Time_07())), 1) << "the timer ended";
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF4_Re_01_Time_02())), 1) << "the timer ended";
}

/** :111-116: the destroy stops the timer */
TEST_F(I3InstanceTest, LinkgatesDestroyStopsTheTimer) {
	world::WorldMapInstance& instance = linkgate();
	enter(instance, 348.0f, 252.0f, 311.0f);
	use(instance, *at(instance, 804578));
	executor().advance(std::chrono::milliseconds(0));
	instance.getInstanceHandler()->onInstanceDestroy();
	executor().advance(std::chrono::minutes(5));
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDLDF4_Re_01_Time_02())), 0);
}

/** :56-58: the other door dies when used */
TEST_F(I3InstanceTest, LinkgatesOtherDoorDies) {
	world::WorldMapInstance& instance = linkgate();
	enter(instance, 348.0f, 252.0f, 311.0f);
	runtime::Ptr<Npc> door = at(instance, 234193);
	use(instance, *door);
	EXPECT_TRUE(door->isDead());
}

/** KromedesTrialInstance.java:94-103, 127-138: a relic comes back 10 s after its death, at its place with its static id, while Kaliga lives */
TEST_F(I3InstanceTest, KromedesRelicsComeBackWhileKaligaLives) {
	// removeKaligaBuff's EffectController.removeEffect asks the skill's template (the base fixture's holder is empty; its TearDown resets this)
	dataholders::DataManager::SKILL_DATA.resetForTests();
	xml::LoadContext skillContext;
	dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext,
		R"(<skill_data><skill_template skill_id="19248" name="kaliga" nameId="1" stack="KALIGA" lvl="1" skilltype="MAGICAL")"
		R"( skillsubtype="NONE" tslot="BUFF" activation="ACTIVE" cooldown="0" duration="0"><properties first_target="ME")"
		R"( target_relation="FRIEND" target_type="ONLYONE" /></skill_template></skill_data>)"));
	world::WorldMapInstance& instance = kromede();
	at(instance, 217006, 600.0f, 600.0f);
	runtime::Ptr<Npc> relic = spawnStaticNpc(instance, 282093, 310.0f, 320.0f, 300.0f, 42);
	instance.getInstanceHandler()->onDie(*relic);
	EXPECT_FALSE(relic->isSpawned());
	EXPECT_TRUE(npcs(instance, 282093).empty());
	executor().advance(std::chrono::milliseconds(10000));
	std::vector<runtime::Ptr<Npc>> back = npcs(instance, 282093);
	ASSERT_EQ(back.size(), 1u);
	EXPECT_FLOAT_EQ(back[0]->getX(), 310.0f);
	EXPECT_FLOAT_EQ(back[0]->getY(), 320.0f);
	EXPECT_EQ(back[0]->getSpawn()->getStaticId(), 42);
}

/** :127-138: without Kaliga no relic comes back */
TEST_F(I3InstanceTest, KromedesRelicsStayAwayWithoutKaliga) {
	world::WorldMapInstance& instance = kromede();
	runtime::Ptr<Npc> relic = spawnStaticNpc(instance, 282095, 310.0f, 320.0f, 300.0f, 43);
	instance.getInstanceHandler()->onDie(*relic);
	EXPECT_FALSE(relic->isSpawned());
	executor().advance(std::chrono::milliseconds(10000));
	EXPECT_TRUE(npcs(instance, 282095).empty());
}

/** :76-86: the potion movie's end without Magas' potion in range does nothing */
TEST_F(I3InstanceTest, KromedesPotionMovieNeedsThePotion) {
	world::WorldMapInstance& instance = kromede();
	enter(instance, 300.0f, 300.0f, 300.0f);
	at(instance, 730308, 400.0f, 300.0f); // 100 m away
	instance.getInstanceHandler()->onPlayMovieEnd(player(), 454);
	EXPECT_FLOAT_EQ(player().getX(), 300.0f);
	EXPECT_TRUE(player().isSpawned());
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
