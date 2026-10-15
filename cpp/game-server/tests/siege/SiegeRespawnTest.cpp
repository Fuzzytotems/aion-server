// P5-12a SiegeService::isRespawnAllowed (m5b-plan.md C-05/D8): the one siege body that sits on *every* npc death, through
// NpcAI.ask(AIQuestion.ALLOW_RESPAWN) -> NpcController.onDie -> RespawnService.scheduleRespawn. Until M5b-1 nothing ever killed an npc, so the
// body was AION_UNPORTED and a kill would have thrown inside NpcController::onDie's catch and lost the respawn.
//
// The decision table below is read straight off SiegeService.java:511-522:
//
//   not a SiegeNpc                                     -> true   (the common case: every world monster)
//   SiegeNpc, no fortress for its siege id             -> true
//   SiegeNpc, fortress vulnerable                      -> false  (no respawns while the fortress is under siege)
//   SiegeNpc, fortress nextState == STATE_VULNERABLE   -> spawn respawnTime < seconds until the next fortress state
//   SiegeNpc, fortress otherwise                       -> true
//
// getSecondsUntilNextFortressState() is 0 here because nextStateUpdateTime is null while the siege service is deactivated
// (SiegeService.cpp:123-129, pinned by SiegeServiceM5aTest), which is what makes the `<` of the fourth row observable.

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>

#include "../ai/AiTestSupport.h"
#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/siege/SiegeModType.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/siegespawns/SiegeSpawnTemplate.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/SiegeService.h"

namespace aion::gameserver::ai::testing {
namespace {

using model::siege::FortressLocation;
using model::siege::SiegeLocation;
using model::siege::SiegeModType;
using model::siege::SiegeRace;
using model::templates::spawns::siegespawns::SiegeSpawnTemplate;

constexpr int32_t SIEGE_LOCATION_ID = 1011;

/** A SiegeNpc with the real constructor chain; only the stat containers are doubles, as AiTestNpc has them. */
class RespawnTestSiegeNpc final : public model::gameobjects::siege::SiegeNpc {
	AION_MAKE_REF_FRIEND
public:
	RespawnTestSiegeNpc(CreateKey key, std::unique_ptr<controllers::NpcController> controller, SiegeSpawnTemplate& spawnTemplate,
		const model::templates::npc::NpcTemplate* objectTemplate)
		: SiegeNpc(key, std::move(controller), spawnTemplate, objectTemplate) {}

protected:
	~RespawnTestSiegeNpc() override = default;

	void setupStatContainers() override {
		setGameStats(std::make_unique<model::stats::container::NpcGameStats>(*this));
		setLifeStats(std::make_unique<FixedLifeStats>(*this));
	}
};

class SiegeRespawnTest : public AiTest {
protected:
	void SetUp() override {
		AiTest::SetUp();
		// the singleton is built once per test process (gtest_discover_tests); with sieges off its location maps start empty
		configs::main::SiegeConfig::SIEGE_ENABLED.store(false);
		service = &services::SiegeService::getInstance();
		ASSERT_EQ(service->getFortresses().size(), 0u);
	}

	void TearDown() override {
		service->getFortresses().clear();
		siegeSpawn.reset();
		siegeGroup.reset();
		AiTest::TearDown();
	}

	/** A SiegeNpc of `siegeId` whose spawn group respawns after `respawnTime` seconds */
	runtime::Ref<RespawnTestSiegeNpc> createSiegeNpc(int32_t siegeId, int32_t respawnTime) {
		siegeGroup = model::templates::spawns::SpawnGroup::create(400010000, 700001, respawnTime, nullptr);
		siegeSpawn = SiegeSpawnTemplate::create(siegeId, SiegeRace::ELYOS, SiegeModType::PEACE, *siegeGroup, 10.0f, 20.0f, 30.0f, int8_t{0}, 0,
			std::nullopt, 0);
		return model::gameobjects::VisibleObject::create<RespawnTestSiegeNpc>(std::make_unique<controllers::NpcController>(), *siegeSpawn,
			plainTemplate);
	}

	/** Puts a fortress into the service's map, as initSieges does from the static data */
	FortressLocation& addFortress(int32_t locationId) {
		runtime::Ref<FortressLocation> fortress = FortressLocation::create(nullptr);
		service->getFortresses().put(locationId, fortress);
		return *fortress;
	}

	services::SiegeService* service = nullptr;
	runtime::Ref<model::templates::spawns::SpawnGroup> siegeGroup;
	runtime::Ref<SiegeSpawnTemplate> siegeSpawn;
};

TEST_F(SiegeRespawnTest, APlainNpcAlwaysRespawns) {
	AI_TEST_SCOPE;
	runtime::Ref<AiTestNpc> npc = createNpc(plainTemplate);
	runtime::resetUnportedHitsForTests();

	// `npc instanceof SiegeNpc` is false, so Java returns true without ever looking at a fortress - the answer every world monster gets
	EXPECT_TRUE(service->isRespawnAllowed(*npc));
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(SiegeRespawnTest, APlainNpcRespawnsEvenWhileAFortressIsVulnerable) {
	AI_TEST_SCOPE;
	addFortress(SIEGE_LOCATION_ID).setVulnerable(true);
	runtime::Ref<AiTestNpc> npc = createNpc(plainTemplate);

	// the instanceof guard, not the fortress state, is what answers here: a vulnerable fortress must not stop world respawns
	EXPECT_TRUE(service->isRespawnAllowed(*npc));
}

TEST_F(SiegeRespawnTest, ASiegeNpcWithoutItsFortressRespawns) {
	AI_TEST_SCOPE;
	runtime::Ref<RespawnTestSiegeNpc> npc = createSiegeNpc(SIEGE_LOCATION_ID, 300);
	ASSERT_EQ(npc->getSiegeId(), SIEGE_LOCATION_ID);
	runtime::resetUnportedHitsForTests();

	// getFortress returns null (sieges disabled): Java falls through the null check to the final `return true`
	EXPECT_FALSE(service->getFortress(SIEGE_LOCATION_ID));
	EXPECT_TRUE(service->isRespawnAllowed(*npc));
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(SiegeRespawnTest, ASiegeNpcOfAVulnerableFortressDoesNotRespawn) {
	AI_TEST_SCOPE;
	FortressLocation& fortress = addFortress(SIEGE_LOCATION_ID);
	fortress.setVulnerable(true);
	fortress.setNextState(SiegeLocation::STATE_INVULNERABLE);
	runtime::Ref<RespawnTestSiegeNpc> npc = createSiegeNpc(SIEGE_LOCATION_ID, 300);

	EXPECT_FALSE(service->isRespawnAllowed(*npc)) << "SiegeService.java:515-516: no respawn while the fortress is vulnerable";
}

TEST_F(SiegeRespawnTest, ASiegeNpcOfAnInvulnerableFortressRespawns) {
	AI_TEST_SCOPE;
	FortressLocation& fortress = addFortress(SIEGE_LOCATION_ID);
	fortress.setVulnerable(false);
	fortress.setNextState(SiegeLocation::STATE_INVULNERABLE);
	runtime::Ref<RespawnTestSiegeNpc> npc = createSiegeNpc(SIEGE_LOCATION_ID, 300);

	// neither of the two if arms is taken, so the final `return true` answers
	EXPECT_TRUE(service->isRespawnAllowed(*npc));
}

TEST_F(SiegeRespawnTest, ASiegeNpcRespawnsBeforeTheNextVulnerableStateOnlyIfItIsFastEnough) {
	AI_TEST_SCOPE;
	FortressLocation& fortress = addFortress(SIEGE_LOCATION_ID);
	fortress.setVulnerable(false);
	fortress.setNextState(SiegeLocation::STATE_VULNERABLE);
	ASSERT_EQ(service->getSecondsUntilNextFortressState(), 0) << "nextStateUpdateTime is null while the siege service is deactivated";

	// Java: `return npc.getSpawn().getRespawnTime() < getSecondsUntilNextFortressState()`. With 0 seconds left, 0 is not < 0 and a spawn that
	// would come back after the fortress turned vulnerable is refused; a negative respawn time is the only value still below it.
	EXPECT_FALSE(service->isRespawnAllowed(*createSiegeNpc(SIEGE_LOCATION_ID, 0))) << "0 < 0 is false";
	EXPECT_FALSE(service->isRespawnAllowed(*createSiegeNpc(SIEGE_LOCATION_ID, 300)));
	EXPECT_TRUE(service->isRespawnAllowed(*createSiegeNpc(SIEGE_LOCATION_ID, -1))) << "-1 < 0 is true";
}

TEST_F(SiegeRespawnTest, ASiegeNpcOfAnotherFortressIsUnaffected) {
	AI_TEST_SCOPE;
	addFortress(SIEGE_LOCATION_ID).setVulnerable(true);
	runtime::Ref<RespawnTestSiegeNpc> npc = createSiegeNpc(SIEGE_LOCATION_ID + 1, 300);

	// getFortress(siegeId) keys on the npc's own siege id, not on any vulnerable fortress
	EXPECT_TRUE(service->isRespawnAllowed(*npc));
}

} // namespace
} // namespace aion::gameserver::ai::testing
