// P4-11a bodies of Creature and Npc against real Npcs created through VisibleObject::create<T>: state, visual and see state bit rules, canSee,
// skill cooldowns (with the D6 fix for the lazily created cooldown map), nested zone types, the template accessors of Npc, the lazily created
// TransformModel part and destruction with an empty LeakCensus. Expectations are derived by hand from Creature.java and Npc.java.
//
// Test doubles (standing in for bodies of later chunks): the npc templates are bound from XML text, NPC_SKILL_DATA is an empty holder, and the
// test Npc overrides the virtual setupStatContainers with a CreatureLifeStats part of fixed HP (NpcLifeStats reads the stat calculation of
// P5-01), as SpinePrototypeTest does.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/TribeClass.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/NpcObjectType.h"
#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/model/gameobjects/state/CreatureSeeState.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/templates/item/ItemAttackType.h"
#include "aion/gameserver/model/templates/npc/NpcRating.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.bind.h"
#include "aion/gameserver/model/templates/npc/NpcTemplateType.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/zone/ZoneType.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/runtime/services/LeakCensus.h"
#include "aion/gameserver/skillengine/effect/SummonOwner.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

namespace aion::gameserver::model::gameobjects {
namespace {

using runtime::Ptr;
using runtime::Ref;
using state::CreatureSeeState;
using state::CreatureState;
using state::CreatureVisualState;
using templates::zone::ZoneType;

#define TEST_SCOPE runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST))

std::atomic<int32_t> destroyedNpcs{0};

/** Life stats with fixed HP (NpcLifeStats reads the stat calculation of P5-01) */
class FixedLifeStats final : public stats::container::CreatureLifeStats {
public:
	explicit FixedLifeStats(Creature& owner) : CreatureLifeStats(owner, 1000, 100) {}
};

/** An Npc with the real constructor and postConstruct chain; only the stat containers are doubles. */
class BodiesTestNpc final : public Npc {
	AION_MAKE_REF_FRIEND
public:
	BodiesTestNpc(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate,
		const templates::npc::NpcTemplate* objectTemplate)
		: Npc(key, std::move(controller), spawnTemplate, objectTemplate) {}

protected:
	~BodiesTestNpc() override { destroyedNpcs.fetch_add(1); }

	void setupStatContainers() override {
		setGameStats(std::make_unique<stats::container::NpcGameStats>(*this));
		setLifeStats(std::make_unique<FixedLifeStats>(*this));
	}
};

class BodiesTestSpawnTemplate final : public templates::spawns::SpawnTemplate {
public:
	explicit BodiesTestSpawnTemplate(templates::spawns::SpawnGroup& group) : SpawnTemplate(group, 10.0f, 20.0f, 30.0f, int8_t{0}, 0, std::nullopt, 0, 0,
																			 std::nullopt) {}
};

/** Npc templates are immortal static data: kept for the process like the DataManager holder keeps them. */
const templates::npc::NpcTemplate* npcTemplate(std::string_view attributes) {
	xml::LoadContext context;
	return xml::bindString<templates::npc::NpcTemplate>(context, "<npc_template name_id=\"1\" " + std::string(attributes) + "/>").release();
}

class CreatureBodiesTest : public testing::Test {
protected:
	void SetUp() override {
		destroyedNpcs = 0;
		utils::ThreadPoolManager::installBackend(nullptr);
		auto backend = std::make_unique<runtime::DeterministicExecutor>(clock, 7);
		utils::ThreadPoolManager::installBackend(std::move(backend));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		runtime::LeakCensus::getInstance().install();
		dataholders::DataManager::NPC_SKILL_DATA.publish(std::make_unique<dataholders::NpcSkillData>());
		group = templates::spawns::SpawnGroup::create(210010000, 700000, 0, nullptr);
		spawnTemplate = Ref<BodiesTestSpawnTemplate>(
			static_cast<BodiesTestSpawnTemplate&>(group->addSpawnTemplate(std::make_unique<BodiesTestSpawnTemplate>(*group))));
	}

	void TearDown() override {
		spawnTemplate.reset();
		group.reset();
		runtime::Reclaimer::getInstance().drain();
		runtime::LeakCensus::getInstance().uninstall();
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
		dataholders::DataManager::NPC_SKILL_DATA.resetForTests();
	}

	Ref<BodiesTestNpc> createNpc(const templates::npc::NpcTemplate* objectTemplate) {
		return VisibleObject::create<BodiesTestNpc>(std::make_unique<controllers::NpcController>(), *spawnTemplate, objectTemplate);
	}

	runtime::ManualClock clock{0};
	Ref<templates::spawns::SpawnGroup> group;
	Ref<BodiesTestSpawnTemplate> spawnTemplate;
	static inline const templates::npc::NpcTemplate* normalNpc =
		npcTemplate(R"(npc_id="210001" level="12" name="guard" rating="NORMAL" rank="VETERAN" srange="5" cancel_level="40" tribe="GUARD")");
	static inline const templates::npc::NpcTemplate* eliteNpc = npcTemplate(R"(npc_id="210002" level="30" rating="ELITE" srange="20")");
	static inline const templates::npc::NpcTemplate* heroFlag = npcTemplate(R"(npc_id="210003" level="40" rating="HERO" type="FLAG")");
	static inline const templates::npc::NpcTemplate* sandworm =
		npcTemplate(R"(npc_id="210004" level="65" rating="LEGENDARY" tribe="WORLDRAID_MONSTER_SANDWORMSUM")");
	static inline const templates::npc::NpcTemplate* raidSandworm =
		npcTemplate(R"(npc_id="210005" level="65" rating="LEGENDARY" type="RAID_MONSTER" tribe="WORLDRAID_MONSTER_SANDWORMSUM")");
	static inline const templates::npc::NpcTemplate* worldRaid = npcTemplate(R"(npc_id="210006" level="65" rating="JUNK" tribe="WORLDRAID_MONSTER")");
};

TEST_F(CreatureBodiesTest, StateBitsFollowJavasMatchRules) {
	TEST_SCOPE;
	Ref<BodiesTestNpc> npc = createNpc(normalNpc);
	// Creature.java:48-50 field initializers
	EXPECT_EQ(npc->getState(), 1);
	EXPECT_EQ(npc->getVisualState(), 0);
	EXPECT_TRUE(npc->isInState(CreatureState::ACTIVE));

	npc->setState(CreatureState::FLYING);
	EXPECT_EQ(npc->getState(), 3);
	EXPECT_TRUE(npc->isFlying());
	EXPECT_TRUE(npc->isInFlyingState());

	npc->setState(CreatureState::RESTING); // 1 | 2 | 4
	EXPECT_EQ(npc->getState(), 7);
	EXPECT_TRUE(npc->isInState(CreatureState::DEAD)) << "DEAD (1 + 2 + 4) matches every set bit, not exactly";
	EXPECT_FALSE(npc->isInState(CreatureState::CHAIR)) << "CHAIR (2 + 4) must match exactly";
	EXPECT_FALSE(npc->isFlying()) << "flying but resting";
	EXPECT_FALSE(npc->isInFlyingState());

	npc->unsetState(CreatureState::ACTIVE);
	EXPECT_EQ(npc->getState(), 6);
	EXPECT_TRUE(npc->isInState(CreatureState::CHAIR));

	npc->setState(CreatureState::GLIDING, true);
	EXPECT_EQ(npc->getState(), 512);
	EXPECT_TRUE(npc->isFlying()) << "gliding counts as flying";

	npc->setState(CreatureState::PRIVATE_SHOP, true);
	EXPECT_TRUE(npc->isInState(CreatureState::PRIVATE_SHOP));
	npc->setState(CreatureState::POWERSHARD);
	EXPECT_FALSE(npc->isInState(CreatureState::PRIVATE_SHOP)) << "an extra bit breaks the exact match";
	EXPECT_TRUE(npc->isInState(CreatureState::FLOATING_CORPSE));
	npc->setState(int32_t{5});
	EXPECT_EQ(npc->getState(), 5);
}

TEST_F(CreatureBodiesTest, VisualAndSeeStatesDecideCanSee) {
	TEST_SCOPE;
	Ref<BodiesTestNpc> watcher = createNpc(eliteNpc); // congenital see state SEARCH1 (1)
	Ref<BodiesTestNpc> hidden = createNpc(normalNpc);

	EXPECT_EQ(watcher->getSeeState(), 1) << "Npc: max(skill see state 0, ELITE's SEARCH1)";
	EXPECT_EQ(hidden->getSeeState(), 0);
	EXPECT_TRUE(watcher->canSee(hidden));
	EXPECT_FALSE(watcher->canSee(nullptr)) << "VisibleObject.canSee: object != null";

	hidden->setVisualState(CreatureVisualState::HIDE2);
	EXPECT_TRUE(hidden->isInAnyHide());
	EXPECT_TRUE(hidden->isInVisualState(CreatureVisualState::HIDE2));
	EXPECT_FALSE(watcher->canSee(hidden)) << "visual state 2 > see state 1, and the watcher is not the master";
	EXPECT_TRUE(hidden->canSee(hidden)) << "the master (itself) always sees it";

	watcher->setSeeState(CreatureSeeState::SEARCH2);
	EXPECT_TRUE(watcher->isInSeeState(CreatureSeeState::SEARCH2));
	EXPECT_EQ(watcher->getSeeState(), 2);
	EXPECT_TRUE(watcher->canSee(hidden));
	watcher->unsetSeeState(CreatureSeeState::SEARCH2);
	EXPECT_FALSE(watcher->isInSeeState(CreatureSeeState::SEARCH2));

	hidden->unsetVisualState(CreatureVisualState::HIDE2);
	hidden->setVisualState(CreatureVisualState::BLINKING);
	EXPECT_FALSE(hidden->isInAnyHide()) << "blinking is no hide";
	EXPECT_TRUE(watcher->canSee(hidden)) << "blinking is excluded from the visual state compared";
}

TEST_F(CreatureBodiesTest, SkillCooldownsAndZoneTypes) {
	TEST_SCOPE;
	Ref<BodiesTestNpc> npc = createNpc(normalNpc);
	EXPECT_FALSE(npc->getSkillCoolDowns()) << "created by the first setSkillCoolDown";
	EXPECT_EQ(npc->getSkillCoolDown(5), 0);
	npc->setSkillCoolDown(0, 12345);
	EXPECT_FALSE(npc->getSkillCoolDowns()) << "cooldown id 0 is ignored";
	npc->removeSkillCoolDown(5);
	npc->setSkillCoolDown(5, 12345);
	ASSERT_TRUE(npc->getSkillCoolDowns());
	EXPECT_EQ(npc->getSkillCoolDown(5), 12345);
	npc->removeSkillCoolDown(5);
	EXPECT_EQ(npc->getSkillCoolDown(5), 0);

	// Creature.java:476-513: nested zones are counted; a PvP counter of 0 or 2 (not 1) counts as a PvP zone, any siege zone does
	EXPECT_FALSE(npc->isInsideZoneType(ZoneType::PVP));
	EXPECT_TRUE(npc->isInsidePvPZone()) << "pvp counter 0";
	npc->setInsideZoneType(ZoneType::PVP);
	EXPECT_TRUE(npc->isInsideZoneType(ZoneType::PVP));
	EXPECT_FALSE(npc->isInsidePvPZone()) << "pvp counter 1";
	npc->setInsideZoneType(ZoneType::PVP);
	EXPECT_TRUE(npc->isInsidePvPZone()) << "pvp counter 2";
	npc->unsetInsideZoneType(ZoneType::PVP);
	npc->setInsideZoneType(ZoneType::SIEGE);
	EXPECT_TRUE(npc->isInsidePvPZone()) << "siege";
}

/**
 * Widens the window between a lazy getter's null check and its publication: while installed, every thread sleeps 2 ms right before it stores a
 * Field<Ref> (exchange, the store of an unguarded `if (!f.get()) f.set(create())`) or compare-and-sets it. The concurrent first callers of a
 * Java-style port then all see null and publish different objects, so the D6 regression tests fail reliably instead of rarely. The yield points
 * exist only in AION_PCT builds (Debug and RelWithDebInfo); elsewhere the tests are smoke tests.
 */
class RaceWindowWidener {
public:
	RaceWindowWidener() { runtime::pct::installHooks(&hooks); }
	~RaceWindowWidener() { runtime::pct::installHooks(nullptr); }
	RaceWindowWidener(const RaceWindowWidener&) = delete;
	RaceWindowWidener& operator=(const RaceWindowWidener&) = delete;

private:
	static void sleepBeforeStore(const char* site) noexcept {
		const std::string_view name(site);
		if (name == "Field<Ref>::exchange" || name == "Field<Ref>::compareAndSet")
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	static constexpr runtime::pct::PctHooks hooks{&sleepBeforeStore, nullptr, nullptr};
};

/**
 * Deviation (D6, docs/deviations/P4-11a.md): concurrent first setSkillCoolDown calls publish one map. Java's unguarded
 * `if (skillCoolDowns == null) skillCoolDowns = new ConcurrentHashMap<>()` can replace a map another thread has just written to.
 */
TEST_F(CreatureBodiesTest, ConcurrentFirstCooldownsAreNotLost) {
	constexpr int threads = 8;
	for (int round = 0; round < 20; ++round) {
		Ref<BodiesTestNpc> npc;
		{
			TEST_SCOPE;
			npc = createNpc(normalNpc);
		}
		std::atomic<bool> go{false};
		std::vector<std::thread> workers;
		std::optional<RaceWindowWidener> widener(std::in_place);
		for (int t = 0; t < threads; ++t) {
			workers.emplace_back([&npc, &go, t] {
				while (!go.load())
					std::this_thread::yield();
				TEST_SCOPE;
				npc->setSkillCoolDown(100 + t, 1000 + t);
			});
		}
		go = true;
		for (std::thread& worker : workers)
			worker.join();
		widener.reset();
		TEST_SCOPE;
		for (int t = 0; t < threads; ++t)
			EXPECT_EQ(npc->getSkillCoolDown(100 + t), 1000 + t) << "round " << round << ", thread " << t;
	}
}

TEST_F(CreatureBodiesTest, NpcTemplateAccessors) {
	TEST_SCOPE;
	Ref<BodiesTestNpc> guard = createNpc(normalNpc);
	EXPECT_EQ(guard->getLevel(), 12);
	EXPECT_EQ(guard->getNpcId(), 210001);
	EXPECT_EQ(guard->getRating(), templates::npc::NpcRating::NORMAL);
	EXPECT_EQ(guard->getRank(), templates::npc::NpcRank::VETERAN);
	EXPECT_EQ(guard->getCancelLevel(), 40);
	EXPECT_FALSE(guard->isBoss());
	EXPECT_FALSE(guard->isFlag());
	EXPECT_EQ(guard->getNpcTemplateType(), templates::npc::NpcTemplateType::NONE);
	EXPECT_EQ(guard->getAggroRange(), 5) << "DummyAI keeps the template value";
	EXPECT_EQ(guard->getShortAggroRange(), 2) << "aggro range below 8: half";
	EXPECT_EQ(guard->getAggroAngle(), 360);
	EXPECT_EQ(guard->getTribe(), TribeClass::GUARD) << "no creator, not transformed: the template tribe";
	EXPECT_THROW(static_cast<void>(guard->getBaseTribe()), runtime::NullPointerException)
		<< "Java: DataManager.TRIBE_RELATIONS_DATA.getBaseTribe on the unpublished static field";
	EXPECT_EQ(guard->getAttackType(), templates::item::ItemAttackType::PHYSICAL);
	EXPECT_EQ(guard->getNpcObjectType(), NpcObjectType::NORMAL);
	EXPECT_EQ(guard->getMaster(), guard);
	EXPECT_EQ(guard->getActingCreature(), guard);
	EXPECT_FALSE(guard->getCreator()) << "creator id 0";
	EXPECT_FALSE(guard->getOverrideEquipment());
	EXPECT_FALSE(guard->isRandomWalker());
	EXPECT_FALSE(guard->isPathWalker());
	EXPECT_FALSE(guard->hasStatic());
	EXPECT_EQ(guard->getMasterName(), std::nullopt);
	guard->setMasterName("Owner");
	EXPECT_EQ(guard->getMasterName(), "Owner");
	guard->setSummonOwner(skillengine::effect::SummonOwner::PRIVATE);
	EXPECT_EQ(guard->getSummonOwner(), skillengine::effect::SummonOwner::PRIVATE);
	EXPECT_TRUE(guard->isNewSpawn());
	EXPECT_GE(guard->getMillisSinceSpawn(), 0);

	Ref<BodiesTestNpc> elite = createNpc(eliteNpc);
	EXPECT_EQ(elite->getShortAggroRange(), 4);
	EXPECT_THROW(static_cast<void>(elite->getRank()), runtime::NullPointerException) << "no rank attribute";
	EXPECT_EQ(elite->getTribe(), std::nullopt) << "Java null: no tribe attribute";

	Ref<BodiesTestNpc> flag = createNpc(heroFlag);
	EXPECT_TRUE(flag->isFlag());
	EXPECT_TRUE(flag->isBoss());
	EXPECT_EQ(flag->getSeeState(), 2) << "HERO: SEARCH2";

	// Creature.isWorldRaidMonster: `tribe == WORLDRAID_MONSTER || tribe == WORLDRAID_MONSTER_SANDWORMSUM && isRaidMonster()`
	EXPECT_FALSE(createNpc(sandworm)->isWorldRaidMonster());
	EXPECT_TRUE(createNpc(raidSandworm)->isWorldRaidMonster());
	EXPECT_TRUE(createNpc(raidSandworm)->isRaidMonster());
	EXPECT_TRUE(createNpc(worldRaid)->isWorldRaidMonster()) << "no raid monster needed for WORLDRAID_MONSTER";
}

TEST_F(CreatureBodiesTest, TransformModelIsALazilyCreatedPart) {
	TEST_SCOPE;
	Ref<BodiesTestNpc> npc = createNpc(normalNpc);
	EXPECT_FALSE(npc->isTransformed());
	TransformModel& model = npc->getTransformModel();
	EXPECT_EQ(&npc->getTransformModel(), &model) << "created once";
	EXPECT_EQ(&model.partOwner(), static_cast<const runtime::RefCounted*>(npc.get()));
	// TransformModel.getModelId, TransformModel.java:103-115: not active and no event model: the template id
	EXPECT_FALSE(model.isActive());
	EXPECT_TRUE(model.isUnrestricted());
	EXPECT_EQ(model.getModelId(), 210001);
	model.setEventModelId(300);
	EXPECT_EQ(model.getModelId(), 300);
	EXPECT_FALSE(npc->isTransformed());
}

TEST_F(CreatureBodiesTest, DestroyedCreaturesLeaveNoCensusEntry) {
	int32_t objectId = 0;
	{
		TEST_SCOPE;
		Ref<BodiesTestNpc> npc = createNpc(normalNpc);
		objectId = npc->getObjectId();
		npc->setSkillCoolDown(7, 1);
		static_cast<void>(npc->getTransformModel());
		npc->setInsideZoneType(ZoneType::PVP);
		runtime::LeakCensus::getInstance().onRemovedFromWorld(*npc, "Npc", objectId);
	}
	runtime::Reclaimer::getInstance().drain();
	EXPECT_GT(objectId, 0);
	EXPECT_EQ(destroyedNpcs.load(), 1) << "the cooldown map, zone counters and transform part hold no reference to the npc";
	EXPECT_EQ(runtime::LeakCensus::getInstance().trackedCount(), 0u);
	EXPECT_TRUE(runtime::LeakCensus::getInstance().getLeaks().empty());
}

} // namespace
} // namespace aion::gameserver::model::gameobjects
