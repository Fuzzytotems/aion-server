// Shared fixture of the P5-01 stat tests: a real Player (Player::postConstruct creates the real PlayerGameStats and PlayerLifeStats) on the
// deterministic executor, with the pet loader of PlayerPetsDAO replaced by one that returns no pets. The class, level and attributes are those of a
// fresh character: PlayerCommonData without an experience table has level 0.
#pragma once

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/MaterialData.bind.h"
#include "aion/gameserver/dataholders/MaterialData.h"
#include "aion/gameserver/dataholders/ShieldData.bind.h"
#include "aion/gameserver/dataholders/ShieldData.h"
#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/ZoneData.bind.h"
#include "aion/gameserver/dataholders/ZoneData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/stats/calc/StatOwner.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::model::stats::test {

/** world_maps.xml of Poeta (the real attribute set) - an open world map, so WorldMap.isInstanceType() is false */
inline constexpr const char* POETA_WORLD_MAPS_XML = R"(<world_maps>)"
												   R"(<map id="210010000" cName="LF1" name="Poeta" name_id="1" water_level="16" death_level="0")"
												   R"( world_type="ELYSEA" world_size="1024" flags="FLY GLIDE RECALL"/>)"
												   R"(</world_maps>)";

/**
 * The holders the map regions and the reward chain read. ZoneService and WorldMapInstance::regionSize() read their data once per process, so these
 * four are published once and never reset (the pattern of tests/world/WorldTestSupport.h, which this chunk may not include).
 * <p>
 * **It lives here, not in one test file, because a HolderRef can be published exactly once per process and every test file of this chunk shares
 * the process** (`dataholders/loadingutils/HolderRef.h:47` throws IllegalStateException on the second publish). A per-file copy has a per-file
 * `static bool`, so the second file to run its own copy throws - which is what a second combat test file did before this became shared.
 */
inline void publishMapStaticDataOnce() {
	static const bool published = [] {
		configs::main::WorldConfig::WORLD_REGION_SIZE.store(128);
		// the unit tests never load geo data; with gameserver.geodata.cansee.enable off GeoService::canSee answers true (GeoService.cpp:117-119),
		// which AggroList::streamValidTargetInfo asks for every candidate target
		configs::main::GeoDataConfig::CANSEE_ENABLE.store(false);
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		static std::deque<xml::LoadContext> contexts;
		dataholders::DataManager::WORLD_MAPS_DATA.publish(
			xml::bindString<dataholders::WorldMapsData>(contexts.emplace_back(), POETA_WORLD_MAPS_XML));
		dataholders::DataManager::ZONE_DATA.publish(xml::bindString<dataholders::ZoneData>(contexts.emplace_back(), "<zones/>"));
		dataholders::DataManager::SHIELD_DATA.publish(xml::bindString<dataholders::ShieldData>(contexts.emplace_back(), "<shields/>"));
		dataholders::DataManager::MATERIAL_DATA.publish(
			xml::bindString<dataholders::MaterialData>(contexts.emplace_back(), "<material_templates/>"));
		return true;
	}();
	static_cast<void>(published);
}

/** Sets an atomic configuration field for the scope and restores the previous value (tests share the process-wide configuration) */
template <class T>
class AtomicConfigScope {
public:
	AtomicConfigScope(std::atomic<T>& configValue, T value) : config(configValue), previous(configValue.load()) { config.store(value); }
	~AtomicConfigScope() { config.store(previous); }
	AtomicConfigScope(const AtomicConfigScope&) = delete;
	AtomicConfigScope& operator=(const AtomicConfigScope&) = delete;

private:
	std::atomic<T>& config;
	const T previous;
};

/** A run-time stat owner (Java: any object implementing StatOwner, e.g. an Effect or a RandomBonusEffect) */
class TestStatOwner final : public runtime::RefCounted, public calc::StatOwner {
	AION_MAKE_REF_FRIEND
public:
	static runtime::Ref<TestStatOwner> create() { return runtime::makeRef<TestStatOwner>(); }

	void retain() const noexcept override { runtime::RefCounted::retain(); }
	void release() const noexcept override { runtime::RefCounted::release(); }

protected:
	TestStatOwner() = default;
	~TestStatOwner() override = default;
};

inline std::vector<runtime::Ref<gameobjects::player::PetCommonData>> noPets(gameobjects::player::Player&) {
	return {};
}

class StatsPlayerTest : public testing::Test {
protected:
	void SetUp() override {
		utils::ThreadPoolManager::installBackend(nullptr);
		utils::ThreadPoolManager::installBackend(std::make_unique<runtime::DeterministicExecutor>(clock, 11));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		gameobjects::player::PetList::setPlayerPetsLoaderForTests(&noPets);
	}

	void TearDown() override {
		gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
		runtime::Reclaimer::getInstance().drain();
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
	}

	struct PlayerFixture {
		runtime::Ref<account::Account> account;
		runtime::Ref<gameobjects::player::PlayerCommonData> commonData;
		runtime::Ref<gameobjects::player::Player> player;
	};

	/** Java PlayerService.getPlayer: account, common data, appearance, account data and the account warehouse, then new Player(...) */
	static PlayerFixture makePlayer(int32_t objectId, PlayerClass playerClass) {
		PlayerFixture f;
		f.account = account::Account::create(5000 + objectId);
		f.commonData = gameobjects::player::PlayerCommonData::create(objectId);
		f.commonData->setName("Stats" + std::to_string(objectId));
		f.commonData->setRace(Race::ELYOS);
		f.commonData->setPlayerClass(playerClass);
		runtime::Ref<gameobjects::player::PlayerAppearance> appearance = gameobjects::player::PlayerAppearance::create();
		f.account->addPlayerAccountData(std::make_unique<account::PlayerAccountData>(*f.account, *f.commonData, *appearance));
		f.account->setAccountWarehouse(
			std::make_unique<items::storage::PlayerStorage>(*f.account, items::storage::StorageType::ACCOUNT_WAREHOUSE));
		f.player = gameobjects::VisibleObject::create<gameobjects::player::Player>(*f.account->getPlayerAccountData(objectId), *f.account);
		// Java PlayerService.getPlayer: the position (not spawned), the known list and the effect controller are set after construction
		f.player->setPosition(world::WorldPosition::create(210010000, 1212.94f, 1044.85f, 140.76f, int8_t{0}));
		f.player->setKnownlist(std::make_unique<world::knownlist::KnownList>(*f.player));
		f.player->setEffectController(std::make_unique<controllers::effect::PlayerEffectController>(*f.player));
		return f;
	}

	runtime::ManualClock clock{0};
};

/**
 * An npc AI that scales the damage its owner deals and takes, so a test can tell a port that calls Java's modifyOwnerDamage and
 * modifyDamage hooks from one that does not - AbstractAI answers the damage unchanged, so only an AI that answers something else can.
 * Used by both halves of the damage math: CombatDamageTest (physical) and MagicalCombatTest (magical).
 */
class ScalingNpcAI final : public ::aion::gameserver::ai::NpcAI {
public:
	ScalingNpcAI(gameobjects::Npc& owner, float ownerFactorValue, float attackedFactorValue)
		: NpcAI(owner), ownerFactor(ownerFactorValue), attackedFactor(attackedFactorValue) {}

	/** Java NpcAI.modifyOwnerDamage: the damage this npc deals */
	float modifyOwnerDamage(float damage, gameobjects::Creature& effected, runtime::Ptr<skillengine::model::Effect> effect) override {
		return damage * ownerFactor;
	}

	/** Java NpcAI.modifyDamage: the damage this npc takes */
	float modifyDamage(gameobjects::Creature& attacker, float damage, runtime::Ptr<skillengine::model::Effect> effect) override {
		return damage * attackedFactor;
	}

private:
	const float ownerFactor;
	const float attackedFactor;
};

} // namespace aion::gameserver::model::stats::test
