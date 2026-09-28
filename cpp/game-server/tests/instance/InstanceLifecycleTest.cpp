// The instance core of the ascension route (P5-13, m5f-plan.md §15, items N-01, N-02, N-03, N-04, N-07): InstanceService creates Karamatis B
// and Ataxiar B from their shipped spawns, registers the player, starts the empty-instance checker, answers the leave messages, and destroys
// an instance with its C++ cycle breakers and D13's scaling erase; WorldMapInstance::detachInstanceHandler installs the no-op handler.
//
// Java: InstanceService.java:39-114 (create, destroy, getOrRegisterInstance), :167-198 (EmptyInstanceCheckerTask), :210-224 (onLeaveInstance);
// GeneralInstanceHandler.java:64-68, 87-292. The fixture (AscensionTestSupport.h) builds a World of the real map rows and publishes the real
// spawn, npc, cooltime and exit rows (AscensionTestData.h, verbatim).
//
// Not covered here, said in place: the checker's team arm (isRegisteredTeamDisbanded) needs a GeneralTeam, which only P5-10's group services
// create (M5g, as N-05 already says); `getOrCreatePersonalInstance`'s creation needs a personal map id with a WorldMapType constant
// (720010000, a studio: M5h) in this World; the leak census after a destroy needs the fixture's one TaskScope to end first, so the
// breakers are asserted edge by edge instead (the old handler's reference count, the cancelled checker, startPos, the scalings entry); the
// supplier arm's event spawns need an active event with spawns (EventService), which no fixture of this lane starts; the PANESTERRA clause of
// getNextAvailableInstance is unreachable on shipped data (no PANESTERRA map is an instance map).

#include "AscensionTestSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <typeinfo>
#include <vector>

#include "aion/gameserver/configs/main/AutoGroupConfig.h"
#include "aion/gameserver/configs/main/InstanceConfig.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/ItemRestrictionCleanupData.h"
#include "aion/gameserver/dataholders/WalkerData.bind.h"
#include "aion/gameserver/dataholders/WalkerData.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/model/instance/StageType.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/walker/WalkerTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/HashSet.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/services/instance/InstanceScaler.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/services/item/ItemFactory.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/spawnengine/ClusteredNpc.h"
#include "aion/gameserver/spawnengine/InstanceWalkerFormations.h"
#include "aion/gameserver/spawnengine/TemporarySpawnEngine.h"
#include "aion/gameserver/spawnengine/WalkerFormationsCache.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldMapInstanceFactory.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::instance::test {
namespace {

using handlers::GeneralInstanceHandler;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using services::instance::InstanceScaler;
using services::instance::InstanceService;
using services::teleport::TeleportService;

// InstanceScaler::scalings is private and InstanceScaler.h is frozen: the standard's explicit-instantiation rule reaches it
// (tests/effects_al/EffectTemplateTest.cpp:114-129 has the pattern)
struct ScalingsTag {
	using Type = runtime::HashMap<runtime::Ref<world::WorldMapInstance>, runtime::Ref<InstanceScaler::Scaling>>*;
	friend Type scalingsOf(ScalingsTag);
};
template <class Tag, typename Tag::Type Member>
struct PrivateAccess {
	friend typename Tag::Type scalingsOf(Tag) { return Member; }
};
template struct PrivateAccess<ScalingsTag, &InstanceScaler::scalings>;

// The same rule for TemporarySpawnEngine::spawnedObjects (private, no getter; TemporarySpawnEngine.h is frozen)
struct SpawnedObjectsTag {
	using Type = runtime::HashSet<runtime::Ref<model::gameobjects::VisibleObject>>*;
	friend Type spawnedObjectsOf(SpawnedObjectsTag);
};
template <class Tag, typename Tag::Type Member>
struct SpawnedObjectsAccess {
	friend typename Tag::Type spawnedObjectsOf(Tag) { return Member; }
};
template struct SpawnedObjectsAccess<SpawnedObjectsTag, &spawnengine::TemporarySpawnEngine::spawnedObjects>;

// ... and for InstanceWalkerFormations::groupedSpawnObjects, the walker candidates of one instance (private, no getter)
struct GroupedSpawnObjectsTag {
	using Type = runtime::HashMap<std::string, runtime::Ref<runtime::RcArrayList<runtime::Ref<spawnengine::ClusteredNpc>>>>
		spawnengine::InstanceWalkerFormations::*;
	friend Type groupedSpawnObjectsOf(GroupedSpawnObjectsTag);
};
template <class Tag, typename Tag::Type Member>
struct GroupedSpawnObjectsAccess {
	friend typename Tag::Type groupedSpawnObjectsOf(Tag) { return Member; }
};
template struct GroupedSpawnObjectsAccess<GroupedSpawnObjectsTag, &spawnengine::InstanceWalkerFormations::groupedSpawnObjects>;

/** GeneralInstanceHandler's protected helpers, opened for the tests (the phase-6 instance scripts are their callers) */
class HelperInstanceHandler final : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
public:
	explicit HelperInstanceHandler(world::WorldMapInstance& instance) : GeneralInstanceHandler(instance) {}

	static runtime::Ref<HelperInstanceHandler> create(world::WorldMapInstance& instance) { return runtime::makeRef<HelperInstanceHandler>(instance); }

	using GeneralInstanceHandler::deleteAliveNpcs;
	using GeneralInstanceHandler::getNpc;
	using GeneralInstanceHandler::sendMsg;
	using GeneralInstanceHandler::spawn;
	using GeneralInstanceHandler::spawnAndSetRespawn;

protected:
	~HelperInstanceHandler() override = default;
};

/** A registered instance script records the calls the tests watch (the supplier arm of getNextAvailableInstance hands it over) */
class RecordingInstanceHandler final : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
public:
	explicit RecordingInstanceHandler(world::WorldMapInstance& instance) : GeneralInstanceHandler(instance) {}

	static runtime::Ref<RecordingInstanceHandler> create(world::WorldMapInstance& instance) {
		return runtime::makeRef<RecordingInstanceHandler>(instance);
	}

	void onInstanceCreate() override { ++created; }
	void onInstanceDestroy() override { ++destroyed; }
	void onLeaveInstance(model::gameobjects::player::Player& player) override {
		++left;
		GeneralInstanceHandler::onLeaveInstance(player);
	}

	int created = 0;
	int destroyed = 0;
	int left = 0;

protected:
	~RecordingInstanceHandler() override = default;
};

/**
 * item_templates.xml:823434-823439 (160001286 Taloc Fruit, ownership_worlds="300190000", not stackable), :831526-831531 (162000132 Steelskin
 * Elixir, ownership_worlds="301120000", max_stack_count 1000) and :848720 (169000001, no ownership_worlds), verbatim
 */
const char* const ITEM_TEMPLATES_XML = R"x(<item_templates>
	<item_template id="160001286" name="Taloc Fruit" level="50" cName="food_kaspa_shapechange_light" casting_delay="1000" mask="4161" quality="LEGEND" price="5" desc="751618" activate_target="STANDALONE" activate_count="1000">
		<actions>
			<skilluse level="1" skillid="10251"/>
		</actions>
		<uselimits usedelay="1200000" usedelayid="33" usearea="IDELIM_ITEMUSE" ownership_worlds="300190000"/>
	</item_template>
	<item_template id="169000001" name="+10 Attack Power Shard" level="1" cName="test_battery_01" mask="12414" max_stack_count="1000" item_group="POWER_SHARDS" quality="COMMON" price="1" desc="700226" weapon_boost="10"/>
	<item_template id="162000132" name="Steelskin Elixir" level="60" cName="idkamar_potion_invincible_01a" mask="12364" max_stack_count="1000" quality="RARE" price="2500" restrict="60 60 60 60 60 60 60 60 60 60 60 60 60 60 60 60 60" desc="813472" activate_target="STANDALONE" activate_count="1" temp_exchange_time="60">
		<actions>
			<skilluse level="2" skillid="10794"/>
		</actions>
		<uselimits usedelay="30000" usedelayid="12" ownership_worlds="301120000"/>
	</item_template>
</item_templates>)x";

/** Karamatis B's first archon legionary spot (spawns/Instances/310020000_Karamatis.xml:6), a place inside the map */
constexpr float INSIDE_X = 127.0f;
constexpr float INSIDE_Y = 203.0f;
constexpr float INSIDE_Z = 225.0f;

class InstanceLifecycleTest : public AscensionWorldTest {
protected:
	void SetUp() override {
		AscensionWorldTest::SetUp();
		if (IsSkipped())
			return;
		soloDelay = configs::main::InstanceConfig::SOLO_INSTANCE_DESTROY_DELAY_SECONDS.load();
		groupDelay = configs::main::InstanceConfig::INSTANCE_DESTROY_DELAY_SECONDS.load();
		configs::main::InstanceConfig::SOLO_INSTANCE_DESTROY_DELAY_SECONDS.store(600); // instance.properties:22
		configs::main::InstanceConfig::INSTANCE_DESTROY_DELAY_SECONDS.store(600);
		// 1006's beam spot on Poeta (_1006Ascension.java:95: 657, 1071, 99.375, h 72): where Pernos sends the Elyos into Karamatis B from
		spawnActor(POETA, 1, 657.0f, 1071.0f, 99.375f, int8_t{72});
	}

	void TearDown() override {
		configs::main::InstanceConfig::SOLO_INSTANCE_DESTROY_DELAY_SECONDS.store(soloDelay);
		configs::main::InstanceConfig::INSTANCE_DESTROY_DELAY_SECONDS.store(groupDelay);
		configs::main::AutoGroupConfig::AUTO_GROUP_ENABLE.store(false);
		AscensionWorldTest::TearDown();
	}

	/** Moves the actor into the instance, as _1006Ascension.java:98-99 does (the 5-argument WorldMapInstance overload, TeleportAnimation.NONE) */
	void enter(world::WorldMapInstance& instance) {
		TeleportService::teleportTo(*actor.player, instance, INSIDE_X, INSIDE_Y, INSIDE_Z);
		ASSERT_EQ(actor.player->getWorldMapInstance().get(), &instance);
		// the cross-map arm leaves the character despawned until CM_LEVEL_READY; the unit test spawns him as that packet would
		world::World::getInstance().spawn(runtime::Ptr<model::gameobjects::VisibleObject>(*actor.player));
		(*client)->clearSent();
	}

	/** Back to Poeta's beam spot: the cross-map arm of SpawnTask::run, which calls InstanceService::onLeaveInstance */
	void leaveToPoeta() { TeleportService::teleportTo(*actor.player, POETA, 657.0f, 1071.0f, 99.375f, int8_t{72}); }

	static size_t npcCount(world::WorldMapInstance& instance) { return instance.getNpcs().size(); }

	int32_t soloDelay = 0;
	int32_t groupDelay = 0;
};

// ---- N-01: creation --------------------------------------------------------------------------------------------------------------------

TEST_F(InstanceLifecycleTest, KaramatisBIsCreatedForAnElyosWithItsShippedSpawnsAndTheChecker) {
	network::test::LogCapture capture({"com.aionemu.gameserver.services.instance.InstanceService"}, spdlog::level::info);
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, *actor.player);

	ASSERT_TRUE(instance);
	EXPECT_EQ(instance->getMapId(), KARAMATIS_B);
	EXPECT_EQ(instance->getInstanceId(), 2) << "the map's own instance 1 exists from World creation, the new one takes the next id";
	EXPECT_TRUE(InstanceService::instanceExists(KARAMATIS_B, instance->getInstanceId()));
	EXPECT_EQ(instance->getMaxPlayers(), 1) << "instance_cooltimes.xml:302-303, max_member_light 1";
	EXPECT_EQ(instance->getOwnerId(), 0);
	EXPECT_TRUE(instance->isRegistered(actor.player->getObjectId())) << "InstanceService.java:71: instance.register(player.getObjectId())";
	EXPECT_EQ(instance->getRegisteredCount(), 1);
	EXPECT_EQ(npcCount(*instance), 51u) << "the 51 spots of spawns/Instances/310020000_Karamatis.xml (SpawnEngine.spawnInstance)";
	EXPECT_EQ(typeid(*instance->getInstanceHandler()), typeid(GeneralInstanceHandler)) << "no instance script is registered for 310020000";
	EXPECT_TRUE(capture.contains("Created new instance: 310020000 [2] owner:0 difficultyId:0")) << "InstanceService.java:60";
	runtime::Ptr<runtime::Future> checker = instance->getEmptyInstanceTask();
	ASSERT_TRUE(checker) << "autoDestroy is true for the (worldId, Player) overload";
	EXPECT_TRUE(checker->isPeriodic());
	EXPECT_EQ(checker->getDelay(), 60000) << "scheduleAtFixedRate(checker, 60000, 60000)";
}

TEST_F(InstanceLifecycleTest, AtaxiarBIsCreatedForAnAsmodianWithSixtySpawns) {
	world::World::getInstance().despawn(*actor.player);
	world::World::getInstance().removeObject(*actor.player);
	actor.player->setClientConnection(nullptr);
	client.reset();
	spawnActor(ISHALGEN, 1, 100.0f, 100.0f, 200.0f, int8_t{0}, model::Race::ASMODIANS);

	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(ATAXIAR_B, *actor.player);

	EXPECT_EQ(instance->getMapId(), ATAXIAR_B);
	EXPECT_EQ(instance->getMaxPlayers(), 1) << "instance_cooltimes.xml:333, max_member_dark 1";
	EXPECT_EQ(npcCount(*instance), 60u) << "the 60 spots of spawns/Instances/320020000_Ataxiar.xml";
	EXPECT_TRUE(instance->isRegistered(actor.player->getObjectId()));
}

TEST_F(InstanceLifecycleTest, AnOpenWorldMapIsNoInstanceMap) {
	try {
		static_cast<void>(InstanceService::getNextAvailableInstance(POETA, int8_t{0}, 0));
		FAIL() << "Poeta is no instance map";
	} catch (const runtime::UnsupportedOperationException& e) {
		EXPECT_EQ(std::string(e.what()), "Invalid call for next available instance  of 210010000") << "InstanceService.java:43, two spaces";
	}
}

TEST_F(InstanceLifecycleTest, TheSupplierArmUsesItsHandlerAndSpawnsNoMapSpawns) {
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, 0, int8_t{0},
		[](world::WorldMapInstance& created) -> runtime::Ref<handlers::InstanceHandler> { return RecordingInstanceHandler::create(created); },
		1, true);

	auto* handler = dynamic_cast<RecordingInstanceHandler*>(instance->getInstanceHandler().get());
	ASSERT_NE(handler, nullptr) << "the supplier's handler, not InstanceEngine's";
	EXPECT_EQ(handler->created, 1) << "onInstanceCreate once, after the spawns (InstanceService.java:54)";
	EXPECT_EQ(npcCount(*instance), 0u) << "the supplier arm spawns only the active events' spawns, and no event is active";
}

TEST_F(InstanceLifecycleTest, WithoutAutoDestroyNoCheckerIsStarted) {
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, 0, int8_t{0}, 1, false);

	EXPECT_FALSE(instance->getEmptyInstanceTask());
	EXPECT_EQ(npcCount(*instance), 51u) << "the null-supplier arm spawns the map";
}

// ---- N-01: the empty-instance checker ------------------------------------------------------------------------------------------------------

TEST_F(InstanceLifecycleTest, TheCheckerRunsFirstAfterSixtySecondsAndDestroysAnEmptyInstancePastItsDelay) {
	configs::main::InstanceConfig::SOLO_INSTANCE_DESTROY_DELAY_SECONDS.store(1);
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, *actor.player);
	const int32_t id = instance->getInstanceId();
	std::this_thread::sleep_for(std::chrono::milliseconds(5)); // the wall clock the checker reads: past taskStartTime + 1000 - 1000

	executor().advance(std::chrono::milliseconds(59999));
	EXPECT_TRUE(InstanceService::instanceExists(KARAMATIS_B, id)) << "the first run is due at 60 s";

	executor().advance(std::chrono::milliseconds(1));
	EXPECT_FALSE(InstanceService::instanceExists(KARAMATIS_B, id)) << "empty, and the destroy time (start + 1 s) - 1 s has passed";
}

TEST_F(InstanceLifecycleTest, TheCheckerKeepsAnInstanceWithinItsDelay) {
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, *actor.player);
	const int32_t id = instance->getInstanceId();

	executor().advance(std::chrono::milliseconds(60000));
	EXPECT_TRUE(InstanceService::instanceExists(KARAMATIS_B, id)) << "600 s have not passed on the wall clock";
}

TEST_F(InstanceLifecycleTest, TheCheckerNeverDestroysAnInstanceWithAPlayerInside) {
	configs::main::InstanceConfig::SOLO_INSTANCE_DESTROY_DELAY_SECONDS.store(1);
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, *actor.player);
	const int32_t id = instance->getInstanceId();
	enter(*instance);
	std::this_thread::sleep_for(std::chrono::milliseconds(5));

	executor().advance(std::chrono::milliseconds(60000));
	EXPECT_TRUE(InstanceService::instanceExists(KARAMATIS_B, id)) << "InstanceService.java:179: players inside";

	leaveToPoeta();
	std::this_thread::sleep_for(std::chrono::milliseconds(5));
	executor().advance(std::chrono::milliseconds(60000));
	EXPECT_FALSE(InstanceService::instanceExists(KARAMATIS_B, id)) << "the next run after the player left";
}

TEST_F(InstanceLifecycleTest, APersonalInstanceGoesAtTheCheckersFirstRun) {
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, 424242, int8_t{0}, 1, true);
	ASSERT_TRUE(instance->isPersonal());
	const int32_t id = instance->getInstanceId();

	executor().advance(std::chrono::milliseconds(60000));
	EXPECT_FALSE(InstanceService::instanceExists(KARAMATIS_B, id)) << "InstanceService.java:181: personal, whatever the delay (600 s)";
}

// ---- N-03: getOrRegisterInstance and onLeaveInstance -----------------------------------------------------------------------------------

TEST_F(InstanceLifecycleTest, GetOrRegisterInstanceFindsTheRegisteredInstanceOrCreatesOne) {
	runtime::Ptr<world::WorldMapInstance> first = InstanceService::getOrRegisterInstance(KARAMATIS_B, *actor.player);
	ASSERT_TRUE(first);
	EXPECT_TRUE(first->isRegistered(actor.player->getObjectId()));
	EXPECT_EQ(InstanceService::getOrRegisterInstance(KARAMATIS_B, *actor.player).get(), first.get()) << "registered: the same instance";

	first->getRegisteredObjects().remove(actor.player->getObjectId());
	runtime::Ptr<world::WorldMapInstance> second = InstanceService::getOrRegisterInstance(KARAMATIS_B, *actor.player);
	EXPECT_NE(second.get(), first.get()) << "not registered anywhere: a new instance";
	EXPECT_TRUE(second->isRegistered(actor.player->getObjectId()));
}

TEST_F(InstanceLifecycleTest, LeavingASoloInstanceAnnouncesItsDestroyDelayInMinutes) {
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, *actor.player);
	enter(*instance);

	leaveToPoeta();

	// _1006Ascension.java:185's exit, without the beam: onLeaveInstance -> STR_MSG_LEAVE_INSTANCE(600 / 60)
	EXPECT_TRUE(wasSent(cptest::serialized(SM_SYSTEM_MESSAGE::STR_MSG_LEAVE_INSTANCE(10), client->con())));
	EXPECT_EQ(actor.player->getWorldId(), POETA);
}

TEST_F(InstanceLifecycleTest, ASoloDelayUnderAMinuteIsAnnouncedAsZero) {
	configs::main::InstanceConfig::SOLO_INSTANCE_DESTROY_DELAY_SECONDS.store(59);
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, *actor.player);
	enter(*instance);

	leaveToPoeta();

	EXPECT_TRUE(wasSent(cptest::serialized(SM_SYSTEM_MESSAGE::STR_MSG_LEAVE_INSTANCE(0), client->con()))) << "59 / 60 in int arithmetic";
}

TEST_F(InstanceLifecycleTest, LeavingAnOpenWorldMapSendsNoLeaveMessage) {
	TeleportService::teleportTo(*actor.player, VERTERON, 100.0f, 100.0f, 100.0f, int8_t{0});

	// Poeta's main instance has no registered object (InstanceService.java:213), so none of the three messages, whatever its delay
	EXPECT_EQ(actor.player->getWorldId(), VERTERON);
	for (int32_t minutes : {0, 10}) {
		EXPECT_FALSE(wasSent(cptest::serialized(SM_SYSTEM_MESSAGE::STR_MSG_LEAVE_INSTANCE(minutes), client->con())));
		EXPECT_FALSE(wasSent(cptest::serialized(SM_SYSTEM_MESSAGE::STR_MSG_LEAVE_INSTANCE_PARTY(minutes), client->con())));
	}
}

TEST_F(InstanceLifecycleTest, LeavingAGroupInstanceAloneAnnouncesThePartyDelay) {
	configs::main::InstanceConfig::INSTANCE_DESTROY_DELAY_SECONDS.store(1200);
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(TALOCS_HOLLOW, int8_t{0}, 6);
	instance->register_(actor.player->getObjectId());
	TeleportService::teleportTo(*actor.player, *instance, 500.0f, 500.0f, 100.0f);
	world::World::getInstance().spawn(runtime::Ptr<model::gameobjects::VisibleObject>(*actor.player));
	(*client)->clearSent();

	leaveToPoeta();

	// maxPlayers 6, no registered team, at most one player inside: STR_MSG_LEAVE_INSTANCE_PARTY(1200 / 60), InstanceService.java:218-219
	EXPECT_TRUE(wasSent(cptest::serialized(SM_SYSTEM_MESSAGE::STR_MSG_LEAVE_INSTANCE_PARTY(20), client->con())));
	EXPECT_FALSE(wasSent(cptest::serialized(SM_SYSTEM_MESSAGE::STR_MSG_LEAVE_INSTANCE(20), client->con()))) << "not the solo message";
}

TEST_F(InstanceLifecycleTest, TheInstanceHandlerIsToldAboutTheLeave) {
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, 0, int8_t{0},
		[](world::WorldMapInstance& created) -> runtime::Ref<handlers::InstanceHandler> { return RecordingInstanceHandler::create(created); },
		1, false);
	enter(*instance);

	leaveToPoeta();

	EXPECT_EQ(dynamic_cast<RecordingInstanceHandler&>(*instance->getInstanceHandler()).left, 1)
		<< "InstanceService.java:212: instance.getInstanceHandler().onLeaveInstance(player)";
}

/** Publishes the two item rows for one test and forgets them afterwards */
class ItemRows {
public:
	ItemRows() {
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(context, ITEM_TEMPLATES_XML));
		// the item info blob of the add/delete packets asks it (hasAccountOrLegionWhStorabilityDisabled); no cleanup entries
		dataholders::DataManager::ITEM_CLEAN_UP.publish(std::make_unique<dataholders::ItemRestrictionCleanupData>());
	}
	~ItemRows() {
		dataholders::DataManager::ITEM_DATA.resetForTests();
		dataholders::DataManager::ITEM_CLEAN_UP.resetForTests();
	}
	ItemRows(const ItemRows&) = delete;
	ItemRows& operator=(const ItemRows&) = delete;

private:
	xml::LoadContext context;
};

constexpr int32_t TALOC_FRUIT = 160001286;
constexpr int32_t POWER_SHARD = 169000001;
constexpr int32_t STEELSKIN_ELIXIR = 162000132;

TEST_F(InstanceLifecycleTest, LeavingTalocsHollowTakesItsFruitFromTheCubeAndThePetBag) {
	ItemRows items;
	runtime::Ptr<world::WorldMapInstance> hollow = InstanceService::getNextAvailableInstance(TALOCS_HOLLOW, int8_t{0}, 6);
	enter(*hollow);
	model::items::storage::Storage& cube = actor.player->getInventory();
	runtime::Ptr<model::items::storage::Storage> petBag = actor.player->getStorage(model::items::storage::PET_BAG_MIN);
	ASSERT_TRUE(petBag);
	ASSERT_TRUE(cube.add(*services::item::ItemFactory::newItem(TALOC_FRUIT, 2), actor.player));
	ASSERT_TRUE(cube.add(*services::item::ItemFactory::newItem(POWER_SHARD, 5), actor.player));
	ASSERT_TRUE(petBag->add(*services::item::ItemFactory::newItem(TALOC_FRUIT, 3), actor.player));

	leaveToPoeta();

	// GeneralInstanceHandler.java:282-292: every item restricted to the map left goes, whole, from the cube and from every pet bag
	EXPECT_FALSE(cube.getFirstItemByItemId(TALOC_FRUIT)) << "ownership_worlds=\"300190000\"";
	EXPECT_FALSE(petBag->getFirstItemByItemId(TALOC_FRUIT));
	runtime::Ptr<model::gameobjects::Item> shard = cube.getFirstItemByItemId(POWER_SHARD);
	ASSERT_TRUE(shard) << "an item without ownership_worlds stays";
	EXPECT_EQ(shard->getItemCount(), 5);
}

TEST_F(InstanceLifecycleTest, LeavingAnotherInstanceKeepsTheFruit) {
	ItemRows items;
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, *actor.player);
	enter(*instance);
	model::items::storage::Storage& cube = actor.player->getInventory();
	ASSERT_TRUE(cube.add(*services::item::ItemFactory::newItem(TALOC_FRUIT, 2), actor.player));

	leaveToPoeta();

	EXPECT_TRUE(cube.getFirstItemByItemId(TALOC_FRUIT)) << "restricted to 300190000, not to 310020000";
}

TEST_F(InstanceLifecycleTest, LeavingKamarTakesAWholeStackOfItsElixir) {
	ItemRows items;
	runtime::Ptr<world::WorldMapInstance> kamar = InstanceService::getNextAvailableInstance(KAMAR_BATTLEFIELD, int8_t{0}, 6);
	enter(*kamar);
	model::items::storage::Storage& cube = actor.player->getInventory();
	runtime::Ptr<model::items::storage::Storage> petBag = actor.player->getStorage(model::items::storage::PET_BAG_MIN);
	ASSERT_TRUE(cube.add(*services::item::ItemFactory::newItem(STEELSKIN_ELIXIR, 5), actor.player));
	ASSERT_TRUE(petBag->add(*services::item::ItemFactory::newItem(STEELSKIN_ELIXIR, 3), actor.player));
	ASSERT_EQ(cube.getFirstItemByItemId(STEELSKIN_ELIXIR)->getItemCount(), 5);

	leaveToPoeta();

	// decreaseByObjectId(item.getObjectId(), item.getItemCount()): the whole stack, not one of it
	EXPECT_FALSE(cube.getFirstItemByItemId(STEELSKIN_ELIXIR));
	EXPECT_FALSE(petBag->getFirstItemByItemId(STEELSKIN_ELIXIR));
}

TEST_F(InstanceLifecycleTest, OnlyAnEnabledAutoGroupReachesAutoGroupService) {
	runtime::resetUnportedHitsForTests();
	EXPECT_NO_THROW(InstanceService::onLeaveInstance(*actor.player)) << "AUTO_GROUP_ENABLE is false (W-08)";
	configs::main::AutoGroupConfig::AUTO_GROUP_ENABLE.store(true);
	EXPECT_THROW(InstanceService::onLeaveInstance(*actor.player), runtime::UnportedException)
		<< "InstanceService.java:222-223: AutoGroupService.onLeaveInstance (P5-10, not ported)";
}

// ---- N-02 and N-07: destroyInstance ------------------------------------------------------------------------------------------------------

TEST_F(InstanceLifecycleTest, DestroyInstanceRemovesTheInstanceAndItsNpcsAndCancelsTheChecker) {
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, *actor.player);
	const int32_t id = instance->getInstanceId();
	std::vector<int32_t> npcIds;
	for (const runtime::Ptr<model::gameobjects::Npc>& npc : instance->getNpcs())
		npcIds.push_back(npc->getObjectId());
	ASSERT_EQ(npcIds.size(), 51u);
	runtime::Ptr<runtime::Future> checker = instance->getEmptyInstanceTask();

	InstanceService::destroyInstance(*instance);

	EXPECT_TRUE(checker->isCancelled()) << "InstanceService.java:83-84";
	EXPECT_FALSE(InstanceService::instanceExists(KARAMATIS_B, id)) << "WorldMap.removeWorldMapInstance";
	EXPECT_EQ(npcCount(*instance), 0u) << "every npc deleted (obj.getController().delete())";
	for (int32_t npcId : npcIds)
		EXPECT_FALSE(world::World::getInstance().findVisibleObject(npcId)) << "npc " << npcId << " is still in the World";
}

TEST_F(InstanceLifecycleTest, DestroyInstanceCutsTheCppEdges) {
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, 0, int8_t{0},
		[](world::WorldMapInstance& created) -> runtime::Ref<handlers::InstanceHandler> { return RecordingInstanceHandler::create(created); },
		1, true);
	runtime::Ref<handlers::InstanceHandler> oldHandler(*instance->getInstanceHandler());
	auto* recording = dynamic_cast<RecordingInstanceHandler*>(oldHandler.get());
	ASSERT_NE(recording, nullptr);
	const uint32_t handlerRefs = recording->refCount();
	instance->setStartPos(world::WorldPosition::create(KARAMATIS_B, INSIDE_X, INSIDE_Y, INSIDE_Z, int8_t{0}));

	InstanceService::destroyInstance(*instance);

	EXPECT_EQ(recording->destroyed, 1) << "onInstanceDestroy (InstanceService.java:105) runs on the real handler, before the detach";
	runtime::Ptr<handlers::InstanceHandler> detached = instance->getInstanceHandler();
	ASSERT_TRUE(detached) << "getInstanceHandler() is never null (WorldMapInstance.h)";
	EXPECT_NE(detached.get(), oldHandler.get()) << "WorldMapInstance::detachInstanceHandler";
	EXPECT_EQ(recording->refCount(), handlerRefs - 1) << "the instance no longer retains its handler (cycles.toml WorldMapInstance.instanceHandler)";
	EXPECT_FALSE(instance->getStartPos()) << "setStartPos(nullptr), cycles.toml WorldMapInstance.startPos";
	EXPECT_FALSE(instance->getRegisteredTeam());

	runtime::Ptr<world::WorldMapInstance> other = InstanceService::getNextAvailableInstance(KARAMATIS_B, 0, int8_t{0}, 1, false);
	InstanceService::destroyInstance(*other);
	EXPECT_EQ(other->getInstanceHandler().get(), detached.get()) << "one no-op handler for every destroyed instance";
}

TEST_F(InstanceLifecycleTest, DestroyInstanceForgetsTheInstancesScaling) {
	const bool scaling = configs::main::InstanceConfig::INSTANCE_SCALING_ENABLE.load();
	configs::main::InstanceConfig::INSTANCE_SCALING_ENABLE.store(true);
	configs::main::InstanceConfig::INSTANCE_SCALING_EXCLUDED_MAPS.set({});
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(TALOCS_HOLLOW, int8_t{0}, 6);
	TeleportService::teleportTo(*actor.player, *instance, 500.0f, 500.0f, 100.0f);
	InstanceScaler::onEnterInstance(*actor.player); // InstanceService::onEnterInstance's third call; the first two need a spawned instance
	const runtime::Ref<world::WorldMapInstance> key(*instance);
	ASSERT_TRUE(scalingsOf(ScalingsTag{})->containsKey(key)) << "canScale: scaling on, maxPlayers 6, an instance map";
	actor.player->getController().cancelAllTasks();
	TeleportService::teleportTo(*actor.player, POETA, 657.0f, 1071.0f, 99.375f, int8_t{72});

	InstanceService::destroyInstance(*instance);

	EXPECT_FALSE(scalingsOf(ScalingsTag{})->containsKey(key)) << "D13: the strong-keyed map forgets the destroyed instance";
	configs::main::InstanceConfig::INSTANCE_SCALING_ENABLE.store(scaling);
}

TEST_F(InstanceLifecycleTest, DestroyingAnInstanceWithAPlayerInsideSendsHimOut) {
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, *actor.player);
	enter(*instance);

	InstanceService::destroyInstance(*instance);

	// InstanceService.java:96-98: STR_MSG_LEAVE_INSTANCE_FORCE(0), then moveToExitPoint. instance_exit.xml has no row for 310020000, so
	// moveToInstanceExit falls back to the bind location - here player_initial_data.xml:4, the Elyos spawn on Poeta (no bind point is set)
	EXPECT_TRUE(wasSent(cptest::serialized(SM_SYSTEM_MESSAGE::STR_MSG_LEAVE_INSTANCE_FORCE(0), client->con())));
	EXPECT_EQ(actor.player->getWorldId(), POETA);
	EXPECT_FLOAT_EQ(actor.player->getX(), ELYOS_SPAWN_X);
	EXPECT_FLOAT_EQ(actor.player->getY(), ELYOS_SPAWN_Y);
}

/** npc_walker/npc_walker.xml:57217-57220, verbatim: a two-step route, published for one case */
const char* const WALKER_ROUTE = "7CE714E1F9D64E6763F219770E0E4C7FA1E1FF6C";
const char* const WALKER_TEMPLATES_XML = R"x(<npc_walker>
	<walker_template route_id="7CE714E1F9D64E6763F219770E0E4C7FA1E1FF6C">
		<routestep x="1354.03" y="937.81" z="160.49"/>
		<routestep x="1339.84" y="946.04" z="160.49"/>
	</walker_template>
</npc_walker>)x";

TEST_F(InstanceLifecycleTest, DestroyInstanceLogsItAndForgetsTheInstancesTemporarySpawnsAndWalkers) {
	xml::LoadContext walkerContext;
	dataholders::DataManager::WALKER_DATA.publish(xml::bindString<dataholders::WalkerData>(walkerContext, WALKER_TEMPLATES_XML));
	struct Unpublish {
		~Unpublish() { dataholders::DataManager::WALKER_DATA.resetForTests(); }
	} unpublish;
	network::test::LogCapture capture({"com.aionemu.gameserver.services.instance.InstanceService"}, spdlog::level::info);
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, 0, int8_t{0}, 1, false);
	const int32_t id = instance->getInstanceId();
	// a temporary spawn of the instance, registered as SpawnEngine.spawnObject registers one
	runtime::Ptr<model::gameobjects::Npc> legionary = instance->getNpc(205009);
	ASSERT_TRUE(legionary);
	const runtime::Ref<model::gameobjects::VisibleObject> temporary(*legionary);
	spawnengine::TemporarySpawnEngine::registerSpawned(*legionary);
	ASSERT_TRUE(spawnedObjectsOf(SpawnedObjectsTag{})->contains(temporary));
	// a walker candidate of the instance, cached as WalkerFormator.processClusteredNpc caches one
	const model::templates::walker::WalkerTemplate* route = dataholders::DataManager::WALKER_DATA->getWalkerTemplate(WALKER_ROUTE);
	ASSERT_NE(route, nullptr);
	runtime::Ptr<model::gameobjects::Npc> walking = instance->getNpc(205010);
	ASSERT_TRUE(walking);
	runtime::Ref<spawnengine::ClusteredNpc> walker = spawnengine::ClusteredNpc::create(*walking, id, route);
	runtime::Ptr<spawnengine::InstanceWalkerFormations> formations = spawnengine::WalkerFormationsCache::getInstanceFormations(KARAMATIS_B, id);
	ASSERT_TRUE(formations->cacheWalkerCandidate(*walker));
	ASSERT_TRUE((formations.get()->*groupedSpawnObjectsOf(GroupedSpawnObjectsTag{})).containsKey(std::string(WALKER_ROUTE)));

	InstanceService::destroyInstance(*instance);

	EXPECT_TRUE(capture.contains("Destroying WorldMapInstance 310020000 [" + std::to_string(id) + "]")) << "InstanceService.java:94";
	EXPECT_FALSE(spawnedObjectsOf(SpawnedObjectsTag{})->contains(temporary)) << "TemporarySpawnEngine.onInstanceDestroy (InstanceService.java:96)";
	EXPECT_TRUE((formations.get()->*groupedSpawnObjectsOf(GroupedSpawnObjectsTag{})).isEmpty())
		<< "WalkerFormator.onInstanceDestroy (InstanceService.java:106) drops the instance's walker candidates";
}

TEST_F(InstanceLifecycleTest, TheDetachedHandlerAnswersLikeTheBaseHandlerInsideAnInstance) {
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, 0, int8_t{0}, 1, false);
	InstanceService::destroyInstance(*instance);
	runtime::Ptr<handlers::InstanceHandler> h = instance->getInstanceHandler();
	model::gameobjects::player::Player& player = *actor.player;

	EXPECT_FALSE(h->onReviveEvent(player));
	EXPECT_FALSE(h->onDie(player, player));
	EXPECT_EQ(h->getStage(), model::instance::StageType::DEFAULT);
	EXPECT_FALSE(h->getInstanceScore());
	EXPECT_FALSE(h->onPassFlyingRing(player, "ring"));
	EXPECT_TRUE(h->canEnter(player));
	EXPECT_FLOAT_EQ(h->getExpMultiplier(), 1.5f) << "GeneralInstanceHandler.java:249-251 on an instance map";
	EXPECT_FLOAT_EQ(h->getApMultiplier(), 1.0f);
	EXPECT_TRUE(h->allowSelfReviveBySkill());
	EXPECT_TRUE(h->allowSelfReviveByItem());
	EXPECT_FALSE(h->allowKiskRevive());
	EXPECT_FALSE(h->allowInstanceRevive());
	EXPECT_THROW(h->portToStartPosition(player), runtime::UnsupportedOperationException);
	EXPECT_NO_THROW(h->onPlayerLogin(player));
	EXPECT_NO_THROW(h->onLeaveInstance(player));
	EXPECT_NO_THROW(h->onPlayMovieEnd(player, 14));
}

TEST_F(InstanceLifecycleTest, DestroyInstanceOnAnOpenWorldMapOnlyCancelsTheChecker) {
	runtime::Ptr<world::WorldMapInstance> poeta = world::World::getInstance().getWorldMap(POETA)->getMainWorldMapInstance();
	runtime::Ptr<handlers::InstanceHandler> handler = poeta->getInstanceHandler();

	InstanceService::destroyInstance(*poeta);

	EXPECT_TRUE(InstanceService::instanceExists(POETA, 1)) << "InstanceService.java:88-89: not an instance map, return";
	EXPECT_EQ(poeta->getInstanceHandler().get(), handler.get()) << "and no breaker ran";
	EXPECT_TRUE(actor.player->isSpawned());
}

// ---- N-04: GeneralInstanceHandler --------------------------------------------------------------------------------------------------------

TEST_F(InstanceLifecycleTest, TheBaseHandlersPortToStartPositionThrowsAsJava) {
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, 0, int8_t{0}, 1, false);
	EXPECT_THROW(instance->getInstanceHandler()->portToStartPosition(*actor.player), runtime::UnsupportedOperationException)
		<< "GeneralInstanceHandler.java:239-241";
}

/** A Karamatis B instance whose script is HelperInstanceHandler (the supplier arm: no map spawns) */
runtime::Ptr<world::WorldMapInstance> helperInstance() {
	return InstanceService::getNextAvailableInstance(KARAMATIS_B, 0, int8_t{0},
		[](world::WorldMapInstance& created) -> runtime::Ref<handlers::InstanceHandler> { return HelperInstanceHandler::create(created); }, 1,
		false);
}

TEST_F(InstanceLifecycleTest, TheSpawnHelpersSpawnIntoTheInstanceAndFindAndDeleteByNpcId) {
	runtime::Ptr<world::WorldMapInstance> instance = helperInstance();
	auto& script = dynamic_cast<HelperInstanceHandler&>(*instance->getInstanceHandler());
	ASSERT_EQ(npcCount(*instance), 0u);

	// GeneralInstanceHandler.java:91-94: a single-time spawn of the map (SpawnEngine.newSingleTimeSpawn), into this instance
	runtime::Ptr<model::gameobjects::VisibleObject> single = script.spawn(205009, INSIDE_X, INSIDE_Y, INSIDE_Z, int8_t{38});
	ASSERT_TRUE(single);
	EXPECT_EQ(single->getWorldId(), KARAMATIS_B);
	EXPECT_EQ(single->getInstanceId(), instance->getInstanceId()) << "not the map's instance 1";
	EXPECT_EQ(single->getSpawn()->getStaticId(), 0);
	EXPECT_TRUE(single->getSpawn()->isNoRespawn());
	// :96-100: the same with a static id
	runtime::Ptr<model::gameobjects::VisibleObject> placed = script.spawn(205010, 109.0f, 208.0f, 213.0f, int8_t{99}, 4242);
	ASSERT_TRUE(placed);
	EXPECT_EQ(placed->getInstanceId(), instance->getInstanceId());
	EXPECT_EQ(placed->getSpawn()->getStaticId(), 4242) << "template.setStaticId(staticId)";
	// :102-105: SpawnEngine.newSpawn with a respawn time
	runtime::Ptr<model::gameobjects::VisibleObject> respawning = script.spawnAndSetRespawn(205009, 164.0f, 221.0f, 220.0f, int8_t{75}, 300);
	ASSERT_TRUE(respawning);
	EXPECT_EQ(respawning->getInstanceId(), instance->getInstanceId());
	EXPECT_EQ(respawning->getSpawn()->getRespawnTime(), 300);
	EXPECT_EQ(npcCount(*instance), 3u);

	// :107-109: instance.getNpc(npcId)
	runtime::Ptr<model::gameobjects::Npc> legionary = script.getNpc(205010);
	ASSERT_TRUE(legionary);
	EXPECT_EQ(legionary->getObjectId(), placed->getObjectId());
	EXPECT_FALSE(script.getNpc(205000)) << "none spawned";

	// :111-113: every alive npc of the ids is deleted, the others stay
	script.deleteAliveNpcs({205009});
	EXPECT_EQ(npcCount(*instance), 1u);
	EXPECT_FALSE(script.getNpc(205009));
	EXPECT_FALSE(world::World::getInstance().findVisibleObject(single->getObjectId()));
	EXPECT_FALSE(world::World::getInstance().findVisibleObject(respawning->getObjectId()));
	EXPECT_TRUE(script.getNpc(205010));
}

TEST_F(InstanceLifecycleTest, SendMsgReachesThePlayersInsideAtOnceOrAfterItsDelay) {
	runtime::Ptr<world::WorldMapInstance> instance = helperInstance();
	auto& script = dynamic_cast<HelperInstanceHandler&>(*instance->getInstanceHandler());
	enter(*instance);

	// GeneralInstanceHandler.java:118-120: sendMsg(msg, 0), PacketSendUtility.broadcastToMap(instance, msg, 0) runs at once
	script.sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_LEAVE_INSTANCE(1));
	EXPECT_TRUE(wasSent(cptest::serialized(SM_SYSTEM_MESSAGE::STR_MSG_LEAVE_INSTANCE(1), client->con())));

	// :125-127: after the delay in milliseconds (scheduleOrRun)
	script.sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_LEAVE_INSTANCE(2), 3000);
	const std::vector<uint8_t> delayed = cptest::serialized(SM_SYSTEM_MESSAGE::STR_MSG_LEAVE_INSTANCE(2), client->con());
	EXPECT_FALSE(wasSent(delayed));
	executor().advance(std::chrono::milliseconds(2999));
	EXPECT_FALSE(wasSent(delayed));
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_TRUE(wasSent(delayed));
}

TEST_F(InstanceLifecycleTest, TheCheckerCountsTheDelayFromTheLastLeaveOnTheWallClock) {
	// the checker reads the wall clock (System.currentTimeMillis), so this case waits: twice 1.1 s of real time
	configs::main::InstanceConfig::SOLO_INSTANCE_DESTROY_DELAY_SECONDS.store(2);
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(KARAMATIS_B, *actor.player);
	const int32_t id = instance->getInstanceId();
	enter(*instance);
	std::this_thread::sleep_for(std::chrono::milliseconds(1100));
	leaveToPoeta(); // lastPlayerLeaveTime: more than a second after the checker's taskStartTime

	executor().advance(std::chrono::milliseconds(60000));
	// InstanceService.java:189-191: max(taskStartTime, lastPlayerLeaveTime) + 2 * 1000; destroyed once now > that - 1000
	EXPECT_TRUE(InstanceService::instanceExists(KARAMATIS_B, id)) << "counted from the leave, not from the start, and in seconds * 1000";

	std::this_thread::sleep_for(std::chrono::milliseconds(1100));
	executor().advance(std::chrono::milliseconds(60000));
	EXPECT_FALSE(InstanceService::instanceExists(KARAMATIS_B, id)) << "more than a second after the leave";
}

TEST_F(InstanceLifecycleTest, TheDifficultyOverloadStartsTheChecker) {
	runtime::Ptr<world::WorldMapInstance> hollow = InstanceService::getNextAvailableInstance(TALOCS_HOLLOW, int8_t{0}, 6);

	// InstanceService.java:75-77: getNextAvailableInstance(worldId, 0, difficult, null, maxPlayers, true)
	EXPECT_TRUE(hollow->getEmptyInstanceTask()) << "autoDestroy true";
	EXPECT_EQ(hollow->getMaxPlayers(), 6);
}

/** A second character inside an instance for one case; out of the World again when the case ends */
class Companion {
public:
	Companion(world::WorldMapInstance& instance, float x, float y, float z) : fixture(cptest::makePlayer(420002, 9412, "Companion")) {
		fixture.player->setMotions(std::make_unique<model::gameobjects::player::motion::MotionList>(*fixture.player));
		world::World& world = world::World::getInstance();
		world.storeObject(*fixture.player);
		EXPECT_TRUE(world.setPosition(runtime::Ptr<model::gameobjects::VisibleObject>(*fixture.player), instance.getMapId(), instance.getInstanceId(),
			x, y, z, int8_t{0}));
		world.spawn(runtime::Ptr<model::gameobjects::VisibleObject>(*fixture.player));
	}

	~Companion() {
		world::World& world = world::World::getInstance();
		fixture.player->getController().cancelAllTasks();
		if (fixture.player->isSpawned())
			world.despawn(*fixture.player);
		world.removeObject(*fixture.player);
		fixture.player->setTarget(nullptr);
	}

	Companion(const Companion&) = delete;
	Companion& operator=(const Companion&) = delete;

private:
	cptest::PlayerFixture fixture;
};

TEST_F(InstanceLifecycleTest, LeavingAGroupInstanceWithOneOtherPlayerInsideStillAnnouncesThePartyDelay) {
	configs::main::InstanceConfig::INSTANCE_DESTROY_DELAY_SECONDS.store(1200);
	runtime::Ptr<world::WorldMapInstance> instance = InstanceService::getNextAvailableInstance(TALOCS_HOLLOW, int8_t{0}, 6);
	instance->register_(actor.player->getObjectId());
	Companion companion(*instance, 510.0f, 510.0f, 100.0f);
	TeleportService::teleportTo(*actor.player, *instance, 500.0f, 500.0f, 100.0f);
	world::World::getInstance().spawn(runtime::Ptr<model::gameobjects::VisibleObject>(*actor.player));
	ASSERT_EQ(instance->getPlayersInside().size(), 2u);
	(*client)->clearSent();

	leaveToPoeta();

	// World.despawn took the leaver out of the instance before SpawnTask's onLeaveInstance, so one player is still inside: `size() <= 1`
	// (InstanceService.java:218), STR_MSG_LEAVE_INSTANCE_PARTY(1200 / 60)
	EXPECT_EQ(instance->getPlayersInside().size(), 1u);
	EXPECT_TRUE(wasSent(cptest::serialized(SM_SYSTEM_MESSAGE::STR_MSG_LEAVE_INSTANCE_PARTY(20), client->con())));
}

TEST_F(InstanceLifecycleTest, AnAsmodianLoggingInOnHaramelWithoutAnInstanceLeavesForAltgard) {
	world::World::getInstance().despawn(*actor.player);
	world::World::getInstance().removeObject(*actor.player);
	actor.player->setClientConnection(nullptr);
	client.reset();
	spawnActor(HARAMEL, 1, 100.0f, 100.0f, 100.0f, int8_t{0}, model::Race::ASMODIANS);

	InstanceService::onPlayerLogin(*actor.player);

	// registered in no Haramel instance, on an instance map: moveToExitPoint -> moveToInstanceExit(player, 300200000, player.getRace()), the
	// Asmodian row (instance_exit.xml:17)
	EXPECT_EQ(actor.player->getWorldId(), ALTGARD);
	EXPECT_FLOAT_EQ(actor.player->getX(), 2907.624f);
	EXPECT_FLOAT_EQ(actor.player->getY(), 1464.1887f);
	EXPECT_FLOAT_EQ(actor.player->getZ(), 252.59264f);
}

} // namespace
} // namespace aion::gameserver::instance::test
