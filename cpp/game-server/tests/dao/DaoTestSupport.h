#pragma once

// Shared fixture of the DAO round-trip tests (P4-14): the test database (DaoTestDatabase.h), a task scope for the runtime references, a
// deterministic thread pool, and players created with create<Player> on stat container doubles.
//
// Test doubles, each standing in for a body of a later chunk:
// - the stat containers (P5-01): TestPlayer runs the real Player::postConstruct, which stops at the unported PlayerGameStats constructor, and
//   then installs game and life stats doubles (like PlayerCreationTest in tests/player).

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

#include "DaoTestDatabase.h"
#include "aion/gameserver/dataholders/loadingutils/HolderRef.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
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

namespace aion::gameserver::dao::test {

/** Binds a static data class from XML text */
template <class T>
std::unique_ptr<T> bindXml(std::string_view text) {
	xml::LoadContext context;
	return xml::bindString<T>(context, text);
}

/** Publishes a holder into DataManager for one test and forgets it again, also when an assertion ends the test early */
template <class H>
class PublishedHolder {
public:
	PublishedHolder(xml::HolderRef<H>& holderRef, std::unique_ptr<H> holder) : ref(holderRef) { ref.publish(std::move(holder)); }
	~PublishedHolder() { ref.resetForTests(); }
	PublishedHolder(const PublishedHolder&) = delete;
	PublishedHolder& operator=(const PublishedHolder&) = delete;

private:
	xml::HolderRef<H>& ref;
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

/** The real Player with the stat containers of the doubles above */
class TestPlayer final : public model::gameobjects::player::Player {
	AION_MAKE_REF_FRIEND
public:
	TestPlayer(CreateKey key, model::account::PlayerAccountData& playerAccountData, model::account::Account& account)
		: Player(key, playerAccountData, account) {}

protected:
	~TestPlayer() override = default;

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

/** Fixture: skips without the test database, empties the tables, opens a task scope and installs a deterministic thread pool */
class DaoTest : public testing::Test {
protected:
	void SetUp() override {
		if (!isEnabled())
			GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL to run the DAO tests";
		setUpDatabaseOnce();
		clearTables();
		utils::ThreadPoolManager::installBackend(nullptr);
		utils::ThreadPoolManager::installBackend(std::make_unique<runtime::DeterministicExecutor>(clock, 11));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		scope.emplace(AION_TASK_INFO(runtime::TaskKind::TEST));
	}

	void TearDown() override {
		if (!isEnabled())
			return;
		scope.reset();
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

	static PlayerFixture makePlayer(int32_t objectId, int32_t accountId, std::string_view name = "Tester") {
		PlayerFixture f;
		f.account = model::account::Account::create(accountId);
		f.commonData = model::gameobjects::player::PlayerCommonData::create(objectId);
		f.commonData->setName(name);
		f.commonData->setRace(model::Race::ELYOS);
		runtime::Ref<model::gameobjects::player::PlayerAppearance> appearance = model::gameobjects::player::PlayerAppearance::create();
		f.account->addPlayerAccountData(std::make_unique<model::account::PlayerAccountData>(*f.account, *f.commonData, *appearance));
		f.account->setAccountWarehouse(
			std::make_unique<model::items::storage::PlayerStorage>(*f.account, model::items::storage::StorageType::ACCOUNT_WAREHOUSE));
		f.player = model::gameobjects::VisibleObject::create<TestPlayer>(*f.account->getPlayerAccountData(objectId), *f.account);
		return f;
	}

	runtime::ManualClock clock{0};
	std::optional<runtime::TaskScope> scope;
};

} // namespace aion::gameserver::dao::test
