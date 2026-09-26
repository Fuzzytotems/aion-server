// M5b-1 stage 2 (P5-13, m5b-plan.md §6.7 G-2): GeneralInstanceHandler::allowKiskRevive and allowInstanceRevive.
//
// They are two one-line Java bodies (GeneralInstanceHandler.java:269-276) and they sit on the character's death path:
// PlayerController::scheduleShowResurrectionOptions -> showResurrectionOptions -> SM_DIE(player), whose constructor asks the instance handler
// which revive buttons the client may show (SM_DIE.java). While they were AION_UNPORTED the scheduled task threw, no SM_DIE ever reached the
// client and a dead character could never revive - the gate's P1/P2/P3(a) could not run at all.
//
// The three terms of allowInstanceRevive are asserted one by one, because two of them are invisible on an open world map:
//   instance.getTemplate().isInstance() && getClass() != GeneralInstanceHandler.class || getWorldType() == WorldType.PANESTERRA
// Java's `getClass() != GeneralInstanceHandler.class` is an exact runtime-type test (not an instanceof), so the base class answers false even
// inside an instance and every ported instance script answers true; the port spells it `typeid(*this) != typeid(GeneralInstanceHandler)`.
// The Panesterra arm needs a map that is NOT an instance and still answers true, so this file binds its own world_maps.xml row for it: none of
// the shared test maps of tests/world has world_type="PANESTERRA" (the real file has five, all of them open world maps - world_maps.xml:140-144).

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>

#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldMapInstanceFactory.h"
#include "aion/gameserver/world/WorldType.h"

#include "../world/WorldTestSupport.h"

namespace aion::gameserver::instance::handlers {
namespace {

using world::test::DREDGION;
using world::test::POETA;

/** A registered instance script is a *subclass* of GeneralInstanceHandler, which is the whole point of Java's `getClass() != ...` test. */
class ScriptedInstanceHandler final : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
public:
	explicit ScriptedInstanceHandler(world::WorldMapInstance& instance) : GeneralInstanceHandler(instance) {}

	static runtime::Ref<ScriptedInstanceHandler> create(world::WorldMapInstance& instance) {
		return runtime::makeRef<ScriptedInstanceHandler>(instance);
	}

protected:
	~ScriptedInstanceHandler() override = default;
};

/**
 * The attribute set of a real Panesterra map (world_maps.xml:140, "Belus"): `world_type="PANESTERRA"` and **no** `instance="true"`. It keeps
 * the map id, cName and world_size of the shared POETA test map, because building a WorldMap runs ZoneService over the *published* world maps
 * (ZoneService.cpp:133 throws for an id that is not in them) - only the world type has to be the real one for this case.
 */
inline const char* const PANESTERRA_MAPS_XML = R"(<world_maps>)"
	R"(<map id="210010000" cName="LF1" name="Belus" name_id="404103" water_level="1" death_level="0" world_type="PANESTERRA")"
	R"( world_size="1024" drop_type="PANESTERRA" flags="GLIDE FLY RIDE PVP DUEL_SAME_RACE"/>)"
	R"(</world_maps>)";

class GeneralInstanceHandlerReviveTest : public ::testing::Test {
protected:
	void SetUp() override {
		if (!world::test::publishTestStaticData())
			GTEST_SKIP() << "this process published the real static data (run the test on its own)";
	}

	runtime::TaskScope scope{AION_TASK_INFO(runtime::TaskKind::TEST)};
};

TEST_F(GeneralInstanceHandlerReviveTest, KiskReviveIsAllowedEverywhereExceptInsideAnInstance) {
	world::World& world = world::World::getInstance();
	runtime::Ptr<world::WorldMapInstance> openWorld = world.getWorldMap(POETA)->getMainWorldMapInstance();
	runtime::Ref<world::WorldMapInstance> insideInstance = world::WorldMapInstanceFactory::createWorldMapInstance(*world.getWorldMap(DREDGION), 1);
	ASSERT_FALSE(openWorld->getTemplate()->isInstance());
	ASSERT_TRUE(insideInstance->getTemplate()->isInstance());

	// Java: return !instance.getTemplate().isInstance();
	EXPECT_TRUE(GeneralInstanceHandler::create(*openWorld)->allowKiskRevive()) << "a kisk works on an open world map";
	EXPECT_FALSE(GeneralInstanceHandler::create(*insideInstance)->allowKiskRevive()) << "a kisk never works inside an instance";
	// the term reads the map template, not the handler class: a scripted handler answers the same
	EXPECT_FALSE(ScriptedInstanceHandler::create(*insideInstance)->allowKiskRevive());
}

TEST_F(GeneralInstanceHandlerReviveTest, InstanceReviveNeedsAnInstanceMapAndAScriptedHandler) {
	world::World& world = world::World::getInstance();
	runtime::Ptr<world::WorldMapInstance> openWorld = world.getWorldMap(POETA)->getMainWorldMapInstance();
	runtime::Ref<world::WorldMapInstance> insideInstance = world::WorldMapInstanceFactory::createWorldMapInstance(*world.getWorldMap(DREDGION), 1);

	// both terms of the left-hand side, one at a time
	EXPECT_FALSE(GeneralInstanceHandler::create(*openWorld)->allowInstanceRevive()) << "not an instance map and not a scripted handler";
	EXPECT_FALSE(ScriptedInstanceHandler::create(*openWorld)->allowInstanceRevive()) << "a scripted handler on an open world map: isInstance is false";
	EXPECT_FALSE(GeneralInstanceHandler::create(*insideInstance)->allowInstanceRevive())
		<< "an instance map, but Java's getClass() != GeneralInstanceHandler.class is false for the base class itself";
	EXPECT_TRUE(ScriptedInstanceHandler::create(*insideInstance)->allowInstanceRevive()) << "an instance map with a scripted handler";
}

TEST_F(GeneralInstanceHandlerReviveTest, PanesterraAllowsInstanceReviveWithoutBeingAnInstance) {
	xml::LoadContext context;
	std::unique_ptr<dataholders::WorldMapsData> maps = xml::bindString<dataholders::WorldMapsData>(context, PANESTERRA_MAPS_XML);
	const model::templates::world::WorldMapTemplate* belus = maps->getTemplate(POETA);
	ASSERT_NE(belus, nullptr);
	ASSERT_EQ(belus->getWorldType(), world::WorldType::PANESTERRA);
	ASSERT_FALSE(belus->isInstance()) << "the real Panesterra maps are open world maps; the third term is the only one that can fire";

	runtime::Ref<world::WorldMap> map = world::WorldMap::create(belus);
	runtime::Ptr<world::WorldMapInstance> instance = map->getMainWorldMapInstance();
	ASSERT_TRUE(instance);

	// Java: ... || instance.getTemplate().getWorldType() == WorldType.PANESTERRA
	EXPECT_TRUE(GeneralInstanceHandler::create(*instance)->allowInstanceRevive()) << "Panesterra is the right-hand side of the ||";
	EXPECT_TRUE(GeneralInstanceHandler::create(*instance)->allowKiskRevive()) << "and it is not an instance, so a kisk works too";
}

} // namespace
} // namespace aion::gameserver::instance::handlers
