// Q09 (route-hand lane, phase 6, 2026-09-29): the two solo instances of the starting-zone missions, played through the quest - 1002's
// Karamatis (310010000) for an Elyos and 2002's Ataxiar (320010000) for an Asmodian. Each case enters through the quest npc's dialog
// (InstanceService.getNextAvailableInstance and TeleportService.teleportTo, the M5f instance core), does the steps inside, leaves through the
// quest's own timed flight and finishes the mission outside - or leaves early and comes back, as the handlers' re-entry arms allow.
//
// The cross-map teleport leaves the character despawned until CM_LEVEL_READY (TeleportService.SpawnTask); the cases spawn him as that
// packet does and run QuestEngine.onEnterWorld, which CM_LEVEL_READY calls (InstanceLifecycleTest.cpp's enter()). They need a test database
// (SpawnEngine.spawnInstance reads the houses, AscensionTestSupport.h) and skip without one.

#include "ZoneQuestTestSupport.h"

#include "aion/gameserver/network/aion/serverpackets/SM_ASCENSION_MORPH.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/PositionUtil.h"

#include <cmath>

namespace aion::gameserver::handlers::quest::poeta {
// compiled into this executable by PoetaHandPortsTest.cpp
std::unique_ptr<::aion::gameserver::questEngine::handlers::AbstractQuestHandler> _1002RequestoftheElim_questFactory();
} // namespace aion::gameserver::handlers::quest::poeta
namespace aion::gameserver::handlers::quest::ishalgen {
std::unique_ptr<::aion::gameserver::questEngine::handlers::AbstractQuestHandler> _2002WheresRae_questFactory();
} // namespace aion::gameserver::handlers::quest::ishalgen

namespace aion::gameserver::questEngine::handlers::zones::test {

namespace DA = gameserver::model::DialogAction;
using network::aion::serverpackets::SM_ASCENSION_MORPH;

class SoloInstanceQuestTest : public ZoneQuestTest {
protected:
	void SetUp() override {
		ZoneQuestTest::SetUp();
		if (IsSkipped())
			return;
		if (!needDatabase())
			GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL to run the instance cases (SpawnEngine.spawnInstance reads the houses)";
	}

	bool talk(Npc& npc, int32_t questId, int32_t action) {
		clearSent();
		return dialog(Ptr<gameserver::model::gameobjects::VisibleObject>(npc), questId, action);
	}

	/** CM_LEVEL_READY after a cross-map teleport: the character is spawned and the quests hear about the map (onEnterWorld) */
	void levelReady() {
		if (!player().isSpawned())
			world::World::getInstance().spawn(Ptr<gameserver::model::gameobjects::VisibleObject>(player()));
		QuestEngine::getInstance().onEnterWorld(player());
	}
};

// ---- 1002: Karamatis --------------------------------------------------------------------------------------------------------------------

TEST_F(SoloInstanceQuestTest, TheElimMissionEntersKaramatisFliesBackToPoetaAndFinishes) {
	install(::aion::gameserver::handlers::quest::poeta::_1002RequestoftheElim_questFactory());
	spawnActor(gameserver::model::Race::ELYOS, 8, POETA, 1000.0f, 1000.0f, 100.0f);
	hold(1002, QuestStatus::START, 13);
	Npc& daminu = spawnNpc(730008);

	EXPECT_TRUE(talk(daminu, 1002, DA::SETPRO5));
	EXPECT_EQ(varOf(1002), 20) << "changeQuestStep(env, 13, 20)";
	EXPECT_TRUE(wasSent(dialogWindow(daminu.getObjectId(), 0, 0))) << "closeDialogWindow";
	ASSERT_EQ(player().getWorldId(), KARAMATIS) << "WorldMapType.KARAMATIS";
	Ptr<world::WorldMapInstance> karamatis = player().getWorldMapInstance();
	ASSERT_TRUE(karamatis);
	EXPECT_GT(karamatis->getInstanceId(), 1) << "a new instance, not the map's own instance 1";
	EXPECT_TRUE(karamatis->isRegistered(player().getObjectId()));
	EXPECT_FLOAT_EQ(player().getX(), 52.0f) << "teleportTo(player, newInstance, 52, 174, 229)";
	EXPECT_FLOAT_EQ(player().getY(), 174.0f);
	EXPECT_FLOAT_EQ(player().getZ(), 229.0f);

	clearSent();
	levelReady();
	EXPECT_TRUE(wasSent(serializedFor(SM_ASCENSION_MORPH(1)))) << "onEnterWorldEvent inside 310010000";
	EXPECT_EQ(varOf(1002), 20);

	// Belpartan of the shipped spawns (spawns/Instances/310010000_Karamatis.xml:21-23) flies the player out after 43 s
	Ptr<Npc> belpartan = karamatis->getNpc(205000);
	ASSERT_TRUE(belpartan) << "SpawnEngine.spawnInstance spawned the map's npcs";
	EXPECT_TRUE(talk(*belpartan, 1002, DA::QUEST_SELECT));
	EXPECT_TRUE(player().isInState(gameserver::model::gameobjects::state::CreatureState::FLYING));
	executor().advance(std::chrono::milliseconds(43000));
	EXPECT_EQ(varOf(1002), 14);
	EXPECT_EQ(player().getWorldId(), POETA);
	EXPECT_EQ(player().getInstanceId(), 1);
	EXPECT_FLOAT_EQ(player().getX(), 603.0f);
	EXPECT_FLOAT_EQ(player().getY(), 1537.0f);
	levelReady();
	EXPECT_EQ(varOf(1002), 14) << "back in Poeta at step 14: onEnterWorldEvent changes only step 20";

	// the rest of the mission outside: Daminu sets the reward, Kalio finishes it
	Npc& daminuAgain = spawnNpc(730008);
	EXPECT_TRUE(talk(daminuAgain, 1002, DA::SETPRO6));
	EXPECT_EQ(stateOf(1002)->getStatus(), QuestStatus::REWARD);
	Npc& kalio = spawnNpc(203067, 3.0f);
	talk(kalio, 1002, DA::SELECTED_QUEST_REWARD1);
	EXPECT_EQ(stateOf(1002)->getStatus(), QuestStatus::COMPLETE);
}

TEST_F(SoloInstanceQuestTest, LeavingKaramatisEarlyPutsTheMissionBackToDaminuWhoSendsThePlayerIntoANewInstance) {
	install(::aion::gameserver::handlers::quest::poeta::_1002RequestoftheElim_questFactory());
	spawnActor(gameserver::model::Race::ELYOS, 8, POETA, 1000.0f, 1000.0f, 100.0f);
	hold(1002, QuestStatus::START, 13);
	Npc& daminu = spawnNpc(730008);
	EXPECT_TRUE(talk(daminu, 1002, DA::SETPRO5));
	ASSERT_EQ(player().getWorldId(), KARAMATIS);
	Ptr<world::WorldMapInstance> firstInstance = player().getWorldMapInstance();
	levelReady();

	// the player leaves (a Return or a relog outside the map): step 20 falls back to 13 when he enters the world elsewhere
	services::teleport::TeleportService::teleportTo(player(), POETA, 1000.0f, 1000.0f, 100.0f);
	levelReady();
	EXPECT_EQ(player().getWorldId(), POETA);
	EXPECT_EQ(varOf(1002), 13) << "_1002RequestoftheElim.java:172-176";

	Npc& daminuAgain = spawnNpc(730008);
	EXPECT_TRUE(talk(daminuAgain, 1002, DA::QUEST_SELECT));
	EXPECT_TRUE(wasSent(dialogWindow(daminuAgain.getObjectId(), 2375, 1002)));
	// the first instance is still there with the player registered (its empty-instance checker has not run): Daminu's
	// InstanceService.getNextAvailableInstance opens another one all the same (_1002RequestoftheElim.java:124)
	ASSERT_TRUE(firstInstance->isRegistered(player().getObjectId()));
	EXPECT_TRUE(talk(daminuAgain, 1002, DA::SETPRO5));
	EXPECT_EQ(player().getWorldId(), KARAMATIS);
	EXPECT_EQ(varOf(1002), 20);
	EXPECT_NE(player().getInstanceId(), firstInstance->getInstanceId()) << "a new instance, not the one the player left";
	EXPECT_GT(player().getInstanceId(), 1);
	EXPECT_TRUE(player().getWorldMapInstance()->isRegistered(player().getObjectId()));
}

// ---- 2002: Ataxiar ----------------------------------------------------------------------------------------------------------------------

TEST_F(SoloInstanceQuestTest, RaesMissionEntersAtaxiarFliesBackToIshalgenAndFinishes) {
	install(::aion::gameserver::handlers::quest::ishalgen::_2002WheresRae_questFactory());
	spawnActor(gameserver::model::Race::ASMODIANS, 8, ISHALGEN, 1000.0f, 1000.0f, 200.0f);
	hold(2002, QuestStatus::START, 12);
	Npc& rae = spawnNpc(790002);

	EXPECT_TRUE(talk(rae, 2002, DA::SETPRO5));
	EXPECT_EQ(rawVarsOf(2002), 99) << "qs.setQuestVar(99) before the instance";
	EXPECT_EQ(varOf(2002), 35) << "slot 0 of 99 (6-bit slots, QuestVars.java:55-58)";
	EXPECT_TRUE(wasSent(dialogWindow(rae.getObjectId(), 0, 0)));
	ASSERT_EQ(player().getWorldId(), ATAXIAR) << "WorldMapType.ATAXIAR";
	Ptr<world::WorldMapInstance> ataxiar = player().getWorldMapInstance();
	ASSERT_TRUE(ataxiar);
	EXPECT_GT(ataxiar->getInstanceId(), 1);
	EXPECT_TRUE(ataxiar->isRegistered(player().getObjectId()));
	EXPECT_FLOAT_EQ(player().getX(), 457.65f);
	EXPECT_FLOAT_EQ(player().getY(), 426.8f);
	EXPECT_FLOAT_EQ(player().getZ(), 230.4f);
	levelReady();

	// Hagen inside (spawns/Instances/320010000_Ataxiar.xml:63-66): the flight back after 40 s sets step 13
	Ptr<Npc> hagen = ataxiar->getNpc(205020);
	ASSERT_TRUE(hagen);
	EXPECT_TRUE(talk(*hagen, 2002, DA::QUEST_SELECT));
	executor().advance(std::chrono::milliseconds(40000));
	EXPECT_EQ(varOf(2002), 13);
	EXPECT_EQ(player().getWorldId(), ISHALGEN);
	EXPECT_FLOAT_EQ(player().getX(), 940.15f);
	levelReady();

	// the rest outside: Rae, the rock, the Rae it spawns, and Ulgorn
	Npc& raeAgain = spawnNpc(790002);
	EXPECT_TRUE(talk(raeAgain, 2002, DA::SETPRO6));
	EXPECT_EQ(varOf(2002), 14);
	Npc& rock = spawnNpc(203538, 3.0f);
	EXPECT_TRUE(talk(rock, 2002, DA::USE_OBJECT));
	EXPECT_EQ(varOf(2002), 15);
	Ptr<Npc> spawnedRae = player().getWorldMapInstance()->getNpc(203553);
	ASSERT_TRUE(spawnedRae);
	spawned.push_back(Ref<gameserver::model::gameobjects::VisibleObject>(spawnedRae));
	EXPECT_TRUE(talk(*spawnedRae, 2002, DA::SETPRO7));
	EXPECT_EQ(stateOf(2002)->getStatus(), QuestStatus::REWARD);
	Npc& ulgorn = spawnNpc(203516, 4.0f);
	talk(ulgorn, 2002, DA::SELECTED_QUEST_REWARD1);
	EXPECT_EQ(stateOf(2002)->getStatus(), QuestStatus::COMPLETE);
}

// java-bug kept (_2002WheresRae.cpp, SETPRO5): the re-entry arm `var == 99` never holds, because slot 0 of 99 is 35; a player who leaves
// Ataxiar before talking to Hagen (205020) inside cannot enter it again through Rae (790002), as on the Java server
TEST_F(SoloInstanceQuestTest, LeavingAtaxiarEarlyLeavesTheMissionAt99AndRaeCannotSendThePlayerBackIn) {
	install(::aion::gameserver::handlers::quest::ishalgen::_2002WheresRae_questFactory());
	spawnActor(gameserver::model::Race::ASMODIANS, 8, ISHALGEN, 1000.0f, 1000.0f, 200.0f);
	hold(2002, QuestStatus::START, 12);
	Npc& rae = spawnNpc(790002);
	EXPECT_TRUE(talk(rae, 2002, DA::SETPRO5));
	ASSERT_EQ(player().getWorldId(), ATAXIAR);
	levelReady();

	services::teleport::TeleportService::teleportTo(player(), ISHALGEN, 1000.0f, 1000.0f, 200.0f);
	levelReady();
	EXPECT_EQ(rawVarsOf(2002), 99) << "2002 has no enter-world hook: the 99 stays";

	Npc& raeAgain = spawnNpc(790002);
	EXPECT_FALSE(talk(raeAgain, 2002, DA::QUEST_SELECT)) << "slot 0 is 35: no page";
	EXPECT_FALSE(talk(raeAgain, 2002, DA::SETPRO5)) << ":120: var == 12 || var == 99 with var 35";
	EXPECT_EQ(player().getWorldId(), ISHALGEN);
	EXPECT_EQ(rawVarsOf(2002), 99);
}

TEST_F(SoloInstanceQuestTest, RaeOpensANewAtaxiarEvenForAPlayerRegisteredWithOne) {
	install(::aion::gameserver::handlers::quest::ishalgen::_2002WheresRae_questFactory());
	spawnActor(gameserver::model::Race::ASMODIANS, 8, ISHALGEN, 1000.0f, 1000.0f, 200.0f);
	hold(2002, QuestStatus::START, 12);
	Ptr<world::WorldMapInstance> registered = services::instance::InstanceService::getNextAvailableInstance(ATAXIAR, player());
	ASSERT_TRUE(registered->isRegistered(player().getObjectId()));
	Npc& rae = spawnNpc(790002);
	EXPECT_TRUE(talk(rae, 2002, DA::SETPRO5));
	ASSERT_EQ(player().getWorldId(), ATAXIAR);
	EXPECT_NE(player().getInstanceId(), registered->getInstanceId())
		<< "InstanceService.getNextAvailableInstance (_2002WheresRae.java:125), not getOrRegisterInstance";
	EXPECT_TRUE(player().getWorldMapInstance()->isRegistered(player().getObjectId()));
}

// ---- TeleportService.teleportToNpc's instance arms (TeleportService.java:316-319) ------------------------------------------------------------

TEST_F(SoloInstanceQuestTest, TeleportToAnNpcOfThePlayersOwnInstanceKeepsHimInItAndTurnsAHeadingOver60Back) {
	spawnActor(gameserver::model::Race::ELYOS, 8, POETA, 1000.0f, 1000.0f, 100.0f);
	Ptr<world::WorldMapInstance> karamatis = services::instance::InstanceService::getNextAvailableInstance(KARAMATIS, player());
	services::teleport::TeleportService::teleportTo(player(), *karamatis, 52, 174, 229);
	levelReady();
	ASSERT_EQ(player().getInstanceId(), karamatis->getInstanceId());
	ASSERT_GT(karamatis->getInstanceId(), 1);

	// Belpartan (spawns/Instances/310010000_Karamatis.xml:22: 85, 191, 231.6508, h 66): the same map, so the player's own instance
	services::teleport::TeleportService::teleportToNpc(player(), 205000);
	float radius = dataholders::DataManager::NPC_DATA->getNpcTemplate(205000)->getBoundRadius()->getFront();
	double radian = utils::PositionUtil::convertHeadingToAngle(int8_t{66}) * 0.017453292519943295;
	EXPECT_EQ(player().getWorldId(), KARAMATIS);
	EXPECT_EQ(player().getInstanceId(), karamatis->getInstanceId()) << "player.getPosition().getWorldMapInstance(), not the map's instance 1";
	EXPECT_FLOAT_EQ(player().getX(), 85.0f + static_cast<float>(std::cos(radian)) * (1.0f + radius));
	EXPECT_FLOAT_EQ(player().getY(), 191.0f + static_cast<float>(std::sin(radian)) * (1.0f + radius));
	EXPECT_EQ(player().getHeading(), 6) << "(h & 0xFF) >= 60 ? h - 60 : h + 60, with h 66";
}

TEST_F(SoloInstanceQuestTest, TeleportToAnNpcOfAnInstanceMapReusesTheInstanceThePlayerIsRegisteredWith) {
	spawnActor(gameserver::model::Race::ASMODIANS, 8, ISHALGEN, 1000.0f, 1000.0f, 200.0f);
	Ptr<world::WorldMapInstance> registered = services::instance::InstanceService::getNextAvailableInstance(ATAXIAR, player());
	services::teleport::TeleportService::teleportToNpc(player(), 205020);
	ASSERT_EQ(player().getWorldId(), ATAXIAR);
	EXPECT_EQ(player().getInstanceId(), registered->getInstanceId()) << "InstanceService.getOrRegisterInstance: the registered one";
}


TEST_F(SoloInstanceQuestTest, TeleportToAnNpcOfAnInstanceMapRegistersThePlayerWithAnInstanceOfIt) {
	spawnActor(gameserver::model::Race::ASMODIANS, 8, ISHALGEN, 1000.0f, 1000.0f, 200.0f);
	// Hagen of Ataxiar has no spawn on Ishalgen: SpawnsData finds 320010000's (spawns/Instances/320010000_Ataxiar.xml:63-66, h 25)
	services::teleport::TeleportService::teleportToNpc(player(), 205020);
	ASSERT_EQ(player().getWorldId(), ATAXIAR);
	EXPECT_GT(player().getInstanceId(), 1) << "InstanceService.getOrRegisterInstance: a new instance, not the map's own";
	EXPECT_TRUE(player().getWorldMapInstance()->isRegistered(player().getObjectId()));
	float radius = dataholders::DataManager::NPC_DATA->getNpcTemplate(205020)->getBoundRadius()->getFront();
	double radian = utils::PositionUtil::convertHeadingToAngle(int8_t{25}) * 0.017453292519943295;
	EXPECT_FLOAT_EQ(player().getX(), 434.75f + static_cast<float>(std::cos(radian)) * (1.0f + radius));
	EXPECT_FLOAT_EQ(player().getY(), 399.5f + static_cast<float>(std::sin(radian)) * (1.0f + radius));
	EXPECT_EQ(player().getHeading(), 85);
}

} // namespace aion::gameserver::questEngine::handlers::zones::test
