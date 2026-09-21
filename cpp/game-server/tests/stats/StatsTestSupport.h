// Shared fixture of the P5-01 stat tests: a real Player (Player::postConstruct creates the real PlayerGameStats and PlayerLifeStats) on the
// deterministic executor, with the pet loader of PlayerPetsDAO replaced by one that returns no pets. The class, level and attributes are those of a
// fresh character: PlayerCommonData without an experience table has level 0.
#pragma once

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
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
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::model::stats::test {

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

} // namespace aion::gameserver::model::stats::test
