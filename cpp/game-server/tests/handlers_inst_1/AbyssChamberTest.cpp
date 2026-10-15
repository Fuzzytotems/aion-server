// P6I-4b (docs/design/p6-instances-plan.md §4): the five abyss chambers with a start ring and a race gold chest - AsteriaInstance,
// RoahInstance, RightWingChamberInstance, LeftWingChamberInstance, SulfurTreeNestInstance (game-server/data/handlers/instance/abyss) - one
// table of their Java data, each case run over the five through their AION_INSTANCE_HANDLER factories.

#include "InstanceHandlerTestSupport.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/handlers/instance/abyss/AsteriaInstance.h"
#include "aion/gameserver/handlers/instance/abyss/LeftWingChamberInstance.h"
#include "aion/gameserver/handlers/instance/abyss/RightWingChamberInstance.h"
#include "aion/gameserver/handlers/instance/abyss/RoahInstance.h"
#include "aion/gameserver/handlers/instance/abyss/SulfurTreeNestInstance.h"
#include "aion/gameserver/model/flyring/FlyRing.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::instance::test {
namespace {

using model::gameobjects::Npc;
using network::aion::serverpackets::SM_QUEST_ACTION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

/** One chamber's Java data */
struct Chamber {
	const char* javaClass;
	int32_t mapId;
	::aion::gameserver::handlers::InstanceFactory* factory;
	const char* ring;
	bool startMessage;
	std::vector<int32_t> takenAtTheEnd;
	int32_t elyosChest;
	int32_t asmodianChest;
	float chestX;
	int32_t chestHeading;
};

const std::vector<Chamber>& chambers() {
	static const std::vector<Chamber> table{
		{"AsteriaInstance", 300050000, &abyss::AsteriaInstance_instanceFactory, "ASTERIA_WING_1", true, {700475, 700476, 700477, 701483, 701488},
		 701483, 701488, 512.8f, 60},
		{"RoahInstance", 300070000, &abyss::RoahInstance_instanceFactory, "ROAH_WING_1", true, {700472, 700473, 700474, 701489, 701484}, 701484,
		 701489, 504.44f, 60},
		{"RightWingChamberInstance", 300090000, &abyss::RightWingChamberInstance_instanceFactory, "RIGHT_WING_1", true, {700471, 701482, 701487},
		 701482, 701487, 261.69f, 30},
		{"LeftWingChamberInstance", 300080000, &abyss::LeftWingChamberInstance_instanceFactory, "LEFT_WING_1", false, {700466, 701481, 701486},
		 701481, 701486, 496.87f, 90},
		{"SulfurTreeNestInstance", 300060000, &abyss::SulfurTreeNestInstance_instanceFactory, "SULFUR_1", true,
		 {214804, 700463, 700462, 700464, 701485, 701480}, 701480, 701485, 482.87f, 90},
	};
	return table;
}

class AbyssChamberTest : public InstanceHandlerTest {};

TEST_F(AbyssChamberTest, EachChamberIsTheJavaClassOfItsMapWithItsRing) {
	const std::filesystem::path javaDir = std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/handlers/instance/abyss";
	const std::regex instanceId(R"(@InstanceID\((\d+)\))");
	for (const Chamber& c : chambers()) {
		std::ifstream in(javaDir / (std::string(c.javaClass) + ".java"));
		ASSERT_TRUE(in) << c.javaClass;
		std::stringstream text;
		text << in.rdbuf();
		std::string source = text.str();
		std::smatch match;
		ASSERT_TRUE(std::regex_search(source, match, instanceId)) << c.javaClass;
		EXPECT_EQ(std::stoi(match[1].str()), c.mapId) << c.javaClass;
		world::WorldMapInstance& instance = create(c.mapId, *c.factory);
		std::vector<std::string> rings;
		for (runtime::Ptr<model::gameobjects::VisibleObject> object : instance)
			if (runtime::Ptr<model::flyring::FlyRing> ring = runtime::as<model::flyring::FlyRing>(object))
				rings.push_back(ring->getName());
		EXPECT_EQ(rings, std::vector<std::string>{c.ring}) << c.javaClass;
	}
}

/** onPassFlyingRing: the first pass of the ring starts the 15 minutes once (the start message but in the left wing, the timer), after which
 * the treasures go; another ring does nothing */
TEST_F(AbyssChamberTest, TheRingStartsTheFifteenMinutes) {
	for (const Chamber& c : chambers()) {
		SCOPED_TRACE(c.javaClass);
		world::WorldMapInstance& instance = create(c.mapId, *c.factory);
		enter(instance, 500.0f, 500.0f, 200.0f);
		std::vector<runtime::Ptr<Npc>> treasures;
		for (int32_t id : c.takenAtTheEnd)
			treasures.push_back(spawnNpc(instance, id, 400.0f, 400.0f, 200.0f, int8_t{0}));
		runtime::Ptr<Npc> other = spawnNpc(instance, 215424, 400.0f, 400.0f, 200.0f, int8_t{0});
		(*client)->clearSent();
		EXPECT_FALSE(instance.getInstanceHandler()->onPassFlyingRing(player(), "OTHER_RING"));
		EXPECT_TRUE(sent().empty());
		EXPECT_FALSE(instance.getInstanceHandler()->onPassFlyingRing(player(), c.ring)) << "Java returns false";
		EXPECT_FALSE(instance.getInstanceHandler()->onPassFlyingRing(player(), c.ring));
		EXPECT_EQ(countSent(serialized(SM_QUEST_ACTION(0, 900))), 1);
		EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_START_IDABRE())), c.startMessage ? 1 : 0);
		executor().advance(std::chrono::milliseconds(899999));
		EXPECT_TRUE(treasures.front()->isSpawned());
		executor().advance(std::chrono::milliseconds(1));
		for (const runtime::Ptr<Npc>& treasure : treasures)
			EXPECT_FALSE(treasure->isSpawned()) << treasure->getNpcId();
		EXPECT_TRUE(other->isSpawned());
		leave();
	}
}

/** onEnterInstance: the first entry's race chooses the gold chest, once; after the start an entry gets the time left */
TEST_F(AbyssChamberTest, TheFirstRaceChoosesTheGoldChest) {
	for (const Chamber& c : chambers()) {
		SCOPED_TRACE(c.javaClass);
		world::WorldMapInstance& instance = create(c.mapId, *c.factory);
		enter(instance, 500.0f, 500.0f, 200.0f);
		instance.getInstanceHandler()->onEnterInstance(player());
		ASSERT_EQ(npcs(instance, c.elyosChest).size(), 1u);
		EXPECT_FLOAT_EQ(npcs(instance, c.elyosChest).front()->getX(), c.chestX);
		EXPECT_EQ(npcs(instance, c.elyosChest).front()->getHeading(), c.chestHeading);
		EXPECT_TRUE(npcs(instance, c.asmodianChest).empty());
		(*client)->clearSent();
		instance.getInstanceHandler()->onEnterInstance(player());
		EXPECT_EQ(npcs(instance, c.elyosChest).size(), 1u) << "once";
		EXPECT_TRUE(sent().empty()) << "before the start no timer";
		instance.getInstanceHandler()->onPassFlyingRing(player(), c.ring);
		(*client)->clearSent();
		instance.getInstanceHandler()->onEnterInstance(player());
		EXPECT_EQ(countSent(serialized(SM_QUEST_ACTION(0, 900))) + countSent(serialized(SM_QUEST_ACTION(0, 899))), 1) << "the time left";
		leave();
	}
}

TEST_F(AbyssChamberTest, AnAsmodianGetsTheOtherChest) {
	const Chamber& c = chambers()[1];
	world::WorldMapInstance& instance = create(c.mapId, *c.factory);
	enter(instance, 500.0f, 500.0f, 200.0f);
	actor.commonData->setRace(model::Race::ASMODIANS);
	instance.getInstanceHandler()->onEnterInstance(player());
	EXPECT_EQ(npcs(instance, c.asmodianChest).size(), 1u);
	EXPECT_TRUE(npcs(instance, c.elyosChest).empty());
}

/** LeftWingChamberInstance.onInstanceCreate: Treasurer Nabatma at one of two places */
TEST_F(AbyssChamberTest, TheLeftWingsTreasurerIsAtOneOfTwoPlaces) {
	std::set<float> places;
	for (uint64_t seed = 1; seed <= 16; ++seed) {
		commons::utils::Rnd::seedCurrentThreadForTests(seed);
		world::WorldMapInstance& instance = create(300080000, abyss::LeftWingChamberInstance_instanceFactory);
		ASSERT_EQ(npcs(instance, 215424).size(), 1u);
		places.insert(npcs(instance, 215424).front()->getX());
	}
	EXPECT_EQ(places, (std::set<float>{502.1326f, 508.71814f}));
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
