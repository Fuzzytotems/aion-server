#pragma once

// Shared fixture of the player-events lane tests (m5a-plan.md E1-01..E1-07: tests/itemsvc, playersvc, team, siege, worldevents).
//
// ctest runs every test case in its own process (gtest_discover_tests), so the singletons a test constructs (SiegeService, EventService,
// AutoGroupService, ...) see exactly the configuration and static data that test prepared.
//
// Test doubles, each standing in for a body of a later chunk:
// - the stat containers (P5-01): TestPlayer runs the real Player::postConstruct, which may stop at the unported PlayerGameStats constructor, and
//   then installs game and life stats doubles (like tests/dao/DaoTestSupport.h).
// - bodies of other chunks the M5a paths call (PlayerSkillList, CreatureLifeStats, PeriodicInstanceManager, ...): SKIP_IF_UNPORTED skips a test
//   until they are ported, so the test starts to run with the merge of that chunk.

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "aion/commons/configuration/ConfigValue.h"
#include "aion/gameserver/dataholders/loadingutils/HolderRef.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
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
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

/** Runs the statement; an unported body of another chunk skips the test (it starts to run once that body is ported) */
#define SKIP_IF_UNPORTED(statement)                                                                                                            \
	try {                                                                                                                                      \
		statement;                                                                                                                             \
	} catch (const ::aion::gameserver::runtime::UnportedException& unported) {                                                                  \
		GTEST_SKIP() << unported.what();                                                                                                       \
	}

namespace aion::gameserver::playerevents::test {

/** Binds a static data class from XML text */
template <class T>
std::unique_ptr<T> bindXml(std::string_view text) {
	xml::LoadContext context;
	return xml::bindString<T>(context, text);
}

/** Publishes a holder into DataManager for one test and forgets it again, also when an assertion ends the test early */
template <class Ref, class H>
class PublishedHolder {
public:
	PublishedHolder(Ref& holderRef, std::unique_ptr<H> holder) : ref(holderRef) { ref.publish(std::move(holder)); }
	~PublishedHolder() { ref.resetForTests(); }
	PublishedHolder(const PublishedHolder&) = delete;
	PublishedHolder& operator=(const PublishedHolder&) = delete;

private:
	Ref& ref;
};

/** Sets an atomic configuration field for the scope and restores the previous value */
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

/** Sets a ConfigValue for the scope and restores the previous value */
template <class T>
class ConfigValueScope {
public:
	ConfigValueScope(commons::configuration::ConfigValue<T>& configValue, T value) : config(configValue), previous(configValue.get()) {
		config.set(std::move(value));
	}
	~ConfigValueScope() { config.set(previous ? *previous : T{}); }
	ConfigValueScope(const ConfigValueScope&) = delete;
	ConfigValueScope& operator=(const ConfigValueScope&) = delete;

private:
	commons::configuration::ConfigValue<T>& config;
	const std::shared_ptr<const T> previous;
};

/** CreatureGameStats double (the stat calculation belongs to P5-01) */
class TestGameStats final : public model::stats::container::CreatureGameStats {
public:
	explicit TestGameStats(model::gameobjects::Creature& owner) : CreatureGameStats(owner) {}
	const model::templates::stats::StatsTemplate* getStatsTemplate() override { return nullptr; }
	int32_t getBaseAttackSpeed() override { return 0; }
	std::unique_ptr<model::stats::calc::Stat2> getMovementSpeed() override { return nullptr; }
	std::unique_ptr<model::stats::calc::Stat2> getAttackRange() override { return nullptr; }
	std::unique_ptr<model::stats::calc::Stat2> getHpRegenRate() override { return nullptr; }
	std::unique_ptr<model::stats::calc::Stat2> getMpRegenRate() override { return nullptr; }
};

/** CreatureLifeStats double (PlayerLifeStats reads PlayerGameStats) */
class TestLifeStats final : public model::stats::container::CreatureLifeStats {
public:
	explicit TestLifeStats(model::gameobjects::Creature& owner) : CreatureLifeStats(owner, 1000, 500) {}
};

inline std::atomic<int32_t> destroyedTestPlayers{0};

/** The real Player with the stat containers of the doubles above */
class TestPlayer final : public model::gameobjects::player::Player {
	AION_MAKE_REF_FRIEND
public:
	TestPlayer(CreateKey key, model::account::PlayerAccountData& playerAccountData, model::account::Account& account)
		: Player(key, playerAccountData, account) {}

protected:
	~TestPlayer() override { destroyedTestPlayers.fetch_add(1); }

	void postConstruct() override {
		try {
			Player::postConstruct();
		} catch (const runtime::UnportedException&) {
			// PlayerGameStats(Player&) is P5-01: everything before it ran
		}
		setGameStats(std::make_unique<TestGameStats>(*this));
		setLifeStats(std::make_unique<TestLifeStats>(*this));
	}
};

inline std::vector<runtime::Ref<model::gameobjects::player::PetCommonData>> noPets(model::gameobjects::player::Player&) {
	return {};
}

/** Fixture: a deterministic thread pool on a manual clock, fresh object ids, the pets loader double and a task scope */
class PlayerEventsTest : public testing::Test {
protected:
	void SetUp() override {
		utils::ThreadPoolManager::installBackend(nullptr);
		executor = new runtime::DeterministicExecutor(clock, 13);
		utils::ThreadPoolManager::installBackend(std::unique_ptr<runtime::DeterministicExecutor>(executor));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(&noPets);
		destroyedTestPlayers = 0;
		scope.emplace(AION_TASK_INFO(runtime::TaskKind::TEST));
	}

	void TearDown() override {
		scope.reset();
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
		runtime::Reclaimer::getInstance().drain();
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
	}

	/** Java PlayerService.getPlayer: account, common data, appearance, account data and the account warehouse, then create<Player> */
	struct PlayerFixture {
		runtime::Ref<model::account::Account> account;
		runtime::Ref<model::gameobjects::player::PlayerCommonData> commonData;
		runtime::Ref<TestPlayer> player;
	};

	static PlayerFixture makePlayer(int32_t objectId, int32_t accountId, model::Race race = model::Race::ELYOS) {
		PlayerFixture f;
		f.account = model::account::Account::create(accountId);
		f.commonData = model::gameobjects::player::PlayerCommonData::create(objectId);
		f.commonData->setName("Tester");
		f.commonData->setRace(race);
		runtime::Ref<model::gameobjects::player::PlayerAppearance> appearance = model::gameobjects::player::PlayerAppearance::create();
		f.account->addPlayerAccountData(std::make_unique<model::account::PlayerAccountData>(*f.account, *f.commonData, *appearance));
		f.account->setAccountWarehouse(
			std::make_unique<model::items::storage::PlayerStorage>(*f.account, model::items::storage::StorageType::ACCOUNT_WAREHOUSE));
		f.player = model::gameobjects::VisibleObject::create<TestPlayer>(*f.account->getPlayerAccountData(objectId), *f.account);
		return f;
	}

	runtime::ManualClock clock{0};
	runtime::DeterministicExecutor* executor = nullptr; // owned by ThreadPoolManager
	std::optional<runtime::TaskScope> scope;
};

} // namespace aion::gameserver::playerevents::test
