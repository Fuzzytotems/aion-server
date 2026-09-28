// P5-06b, M5d H-05 (m5d-plan.md §7): the spawn helpers of AbstractQuestHandler (AbstractQuestHandler.java:1238-1289) over the ported
// SpawnEngine - a single-time spawn of the npc template brought into the World's map instance of the same id, placed in front of a reference
// position (1.5 m or the distance given, along the reference's heading, facing back at it), and the temporary spawns deleted after their
// minutes (deleteIfAliveOrCancelRespawn on the ManualClock).
//
// The World holds the Poeta row the item fixture publishes (ItemPacketTestSupport.h), so the npcs land in its instance 1 - the id of the fixture's
// own instance, where the reference player stands. The unit tests load no geo data: GeoService.init builds empty geo maps, whose getZ is NaN,
// so the reference's z is kept (AbstractQuestHandler.java:1266-1268). Positions are the Java float arithmetic: heading * 3 degrees
// (PositionUtil.convertHeadingToAngle), Math.toRadians, cos/sin times the distance cast to float.

#include "QuestHandlerTestSupport.h"

#include <chrono>
#include <cstdint>
#include <vector>

#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/geo/GeoService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::handlers::test {
namespace {

using gameserver::model::gameobjects::Npc;
using gameserver::model::gameobjects::VisibleObject;

class AbstractQuestHandlerSpawnTest : public QuestHandlerTest {
protected:
	void SetUp() override {
		QuestHandlerTest::SetUp();
		// empty geo maps for the published world maps (GeoDataConfig.GEO_ENABLE is false): getZ answers NaN
		static const bool geoInitialised = [] {
			world::geo::GeoService::getInstance().init();
			return true;
		}();
		static_cast<void>(geoInitialised);
	}

	void TearDown() override {
		for (const Ref<VisibleObject>& object : spawned) {
			if (object->isSpawned())
				object->getController().delete_();
		}
		spawned.clear();
		QuestHandlerTest::TearDown();
	}

	static world::WorldMapInstance& poeta() { return *world::World::getInstance().getWorldMap(210010000)->getMainWorldMapInstance(); }

	/** The spawned npc of `templateId`, kept for the TearDown */
	Npc& spawnedNpc(Ptr<VisibleObject> object, int32_t templateId) {
		EXPECT_TRUE(object);
		Ptr<Npc> npc = runtime::as<Npc>(object);
		EXPECT_TRUE(npc);
		spawned.emplace_back(*object);
		EXPECT_EQ(npc->getNpcId(), templateId);
		return *npc;
	}

	static void expectAt(VisibleObject& object, float x, float y, float z, int8_t heading) {
		Ptr<world::WorldPosition> position = object.getPosition();
		EXPECT_FLOAT_EQ(position->getX(), x);
		EXPECT_FLOAT_EQ(position->getY(), y);
		EXPECT_FLOAT_EQ(position->getZ(), z);
		EXPECT_EQ(position->getHeading(), heading);
		EXPECT_EQ(position->getMapId(), 210010000);
		EXPECT_EQ(position->getInstanceId(), 1);
	}

	std::vector<Ref<VisibleObject>> spawned;
};

// spawn (AbstractQuestHandler.java:1238-1245): SpawnEngine.newSingleTimeSpawn of the instance's map and spawnObject into its instance id - an
// npc of the template, spawned at the coordinates; the VisibleObject overload takes the instance of the object
TEST_F(AbstractQuestHandlerSpawnTest, SpawnBringsAnNpcOfTheTemplateIntoTheInstance) {
	Npc& mires = spawnedNpc(AbstractQuestHandler::spawn(MIRES, poeta(), 120.0f, 130.0f, 51.0f, int8_t{30}), MIRES);
	EXPECT_TRUE(mires.isSpawned());
	expectAt(mires, 120.0f, 130.0f, 51.0f, 30);
	EXPECT_EQ(mires.getSpawn()->getRespawnTime(), 0) << "a single-time spawn";

	Npc& kerub = spawnedNpc(AbstractQuestHandler::spawn(STRIPED_KERUB, player(), 110.0f, 111.0f, 52.0f, int8_t{-5}), STRIPED_KERUB);
	EXPECT_TRUE(kerub.isSpawned());
	expectAt(kerub, 110.0f, 111.0f, 52.0f, -5);
	executor->advance(std::chrono::minutes(10));
	EXPECT_TRUE(mires.isSpawned()) << "not temporary";
	EXPECT_TRUE(kerub.isSpawned());
}

// spawnInFrontOf and spawnForFiveMinutesInFrontOf (AbstractQuestHandler.java:1247-1253, 1259-1270): 1.5 m (or the distance) along the reference's
// heading, facing it (heading + 60 below 60, else - 60); the five-minute one is deleted after 300,000 ms
TEST_F(AbstractQuestHandlerSpawnTest, SpawnInFrontOfPlacesTheNpcAlongTheReferenceHeading) {
	player().getPosition()->setH(30); // 90 degrees: along +y
	Npc& ahead = spawnedNpc(AbstractQuestHandler::spawnInFrontOf(MIRES, player()), MIRES);
	expectAt(ahead, 100.0f, 101.5f, 50.0f, 90);

	player().getPosition()->setH(60); // 180 degrees: along -x
	Npc& temporary = spawnedNpc(AbstractQuestHandler::spawnForFiveMinutesInFrontOf(MIRES, player(), 3.0f), MIRES);
	expectAt(temporary, 97.0f, 100.0f, 50.0f, 0);

	player().getPosition()->setH(-30); // 270 degrees: along -y
	Npc& facing = spawnedNpc(AbstractQuestHandler::spawnForFiveMinutesInFront(MIRES, player(), int8_t{7}, 2.0f), MIRES);
	expectAt(facing, 100.0f, 98.0f, 50.0f, 7);

	executor->advance(std::chrono::milliseconds(299'999));
	EXPECT_TRUE(temporary.isSpawned());
	EXPECT_TRUE(facing.isSpawned());
	executor->advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(temporary.isSpawned());
	EXPECT_FALSE(facing.isSpawned());
	EXPECT_TRUE(ahead.isSpawned()) << "spawnInFrontOf is not temporary (0 minutes)";
}

// spawnForFiveMinutes x3 and spawnTemporarily (AbstractQuestHandler.java:1272-1289): at a position (with its heading or another) or at
// coordinates of an instance, deleted after 5 minutes - or after the minutes given; 0 minutes is a spawn that stays
TEST_F(AbstractQuestHandlerSpawnTest, TheTemporarySpawnsAreDeletedAfterTheirMinutes) {
	Ref<world::WorldPosition> position =
		world::WorldPosition::create(210010000, 105.0f, 106.0f, 49.0f, int8_t{12}, poeta().getRegion(105.0f, 106.0f, 49.0f));
	Npc& atPosition = spawnedNpc(AbstractQuestHandler::spawnForFiveMinutes(MIRES, *position), MIRES);
	expectAt(atPosition, 105.0f, 106.0f, 49.0f, 12);
	Npc& turned = spawnedNpc(AbstractQuestHandler::spawnForFiveMinutes(MIRES, *position, int8_t{40}), MIRES);
	expectAt(turned, 105.0f, 106.0f, 49.0f, 40);
	Npc& atCoordinates = spawnedNpc(AbstractQuestHandler::spawnForFiveMinutes(MIRES, poeta(), 107.0f, 108.0f, 48.0f, int8_t{3}), MIRES);
	expectAt(atCoordinates, 107.0f, 108.0f, 48.0f, 3);
	Npc& twoMinutes = spawnedNpc(AbstractQuestHandler::spawnTemporarily(MIRES, poeta(), 109.0f, 108.0f, 48.0f, int8_t{0}, 2), MIRES);
	Npc& staying = spawnedNpc(AbstractQuestHandler::spawnTemporarily(MIRES, poeta(), 111.0f, 108.0f, 48.0f, int8_t{0}, 0), MIRES);

	executor->advance(std::chrono::milliseconds(119'999));
	EXPECT_TRUE(twoMinutes.isSpawned());
	executor->advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(twoMinutes.isSpawned()) << "2 minutes";
	EXPECT_TRUE(atPosition.isSpawned());
	executor->advance(std::chrono::milliseconds(179'999));
	EXPECT_TRUE(atPosition.isSpawned());
	EXPECT_TRUE(turned.isSpawned());
	EXPECT_TRUE(atCoordinates.isSpawned());
	executor->advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(atPosition.isSpawned());
	EXPECT_FALSE(turned.isSpawned());
	EXPECT_FALSE(atCoordinates.isSpawned());
	executor->advance(std::chrono::minutes(10));
	EXPECT_TRUE(staying.isSpawned()) << "0 minutes: not deleted";
}

// useQuestObject with a dying object (AbstractQuestHandler.java:911-916): the player's target is the env's object, a quest object spawned here,
// which dies by the player's hand (NpcController.die) before the step moves on
TEST_F(AbstractQuestHandlerSpawnTest, UseQuestObjectKillsTheObjectItIsAimedAt) {
	PlainHandler handler(1103);
	Npc& sack = spawnedNpc(AbstractQuestHandler::spawn(GRAIN_SACK, poeta(), 101.0f, 101.0f, 50.0f, int8_t{0}), GRAIN_SACK);
	Ref<QuestState> qs = hold(*me, 1103, QuestStatus::START);
	player().setTarget(Ptr<VisibleObject>(sack));
	EXPECT_FALSE(sack.isDead());
	EXPECT_TRUE(handler.useQuestObject(*envOf(*me, 1103, gameserver::model::DialogAction::USE_OBJECT, Ptr<VisibleObject>(sack)), 0, 1, false, true));
	EXPECT_TRUE(sack.isDead());
	EXPECT_EQ(qs->getQuestVars()->getQuestVars(), 1);
	player().setTarget(nullptr);
}

// A template the npc data does not know spawns nothing (VisibleObjectSpawner.spawnNpc logs "No template for NPC" and answers null); Java's
// temporary spawn then schedules a lambda that dereferences the null object when it runs - the scheduler logs that NullPointerException
TEST_F(AbstractQuestHandlerSpawnTest, AnUnknownTemplateSpawnsNothing) {
	network::test::LogCapture log({"com.aionemu.commons.utils.concurrent.ExecuteWrapper"});
	EXPECT_FALSE(AbstractQuestHandler::spawnTemporarily(299999, poeta(), 109.0f, 108.0f, 48.0f, int8_t{0}, 1));
	executor->advance(std::chrono::milliseconds(59'999));
	EXPECT_FALSE(log.contains("NullPointerException"));
	EXPECT_NO_THROW(executor->advance(std::chrono::milliseconds(1)));
	EXPECT_EQ(log.count("NullPointerException"), 1) << "the task's exception, logged\n" << log.dump();
}

} // namespace
} // namespace aion::gameserver::questEngine::handlers::test
