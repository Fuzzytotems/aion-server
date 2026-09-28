// World chunk (P4-10): PlayerContainer with real players. updateCachedPlayerName is the D6 fix of Java's put inside the compute callback of
// the same map (runtime-architecture.md 4.4, lint L20): concurrent renames finish and leave each player under its current name only.

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
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
#include "aion/gameserver/world/container/PlayerContainer.h"
#include "aion/gameserver/world/exceptions/DuplicateAionObjectException.h"

namespace aion::gameserver::world::test {
namespace {

using model::gameobjects::player::Player;
using model::gameobjects::player::PlayerCommonData;
using runtime::Ptr;
using runtime::Ref;

std::vector<Ref<model::gameobjects::player::PetCommonData>> noPets(Player&) {
	return {};
}

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

class TestLifeStats final : public model::stats::container::CreatureLifeStats {
public:
	explicit TestLifeStats(model::gameobjects::Creature& owner) : CreatureLifeStats(owner, 1000, 500) {}
};

/** the real Player; postConstruct stops at the unported PlayerGameStats constructor (P5-01) and installs the doubles above */
class ContainerTestPlayer final : public Player {
	AION_MAKE_REF_FRIEND
public:
	ContainerTestPlayer(CreateKey key, model::account::PlayerAccountData& playerAccountData, model::account::Account& account)
		: Player(key, playerAccountData, account) {}

protected:
	~ContainerTestPlayer() override = default;

	void postConstruct() override {
		try {
			Player::postConstruct();
		} catch (const runtime::UnportedException&) {
		}
		setGameStats(std::make_unique<TestGameStats>(*this));
		setLifeStats(std::make_unique<TestLifeStats>(*this));
	}
};

struct PlayerFixture {
	Ref<model::account::Account> account;
	Ref<PlayerCommonData> commonData;
	Ref<ContainerTestPlayer> player;
};

PlayerFixture makePlayer(int32_t objectId, const std::string& name) {
	PlayerFixture f;
	f.account = model::account::Account::create(5000 + objectId);
	f.commonData = PlayerCommonData::create(objectId);
	f.commonData->setName(name);
	f.commonData->setRace(model::Race::ELYOS);
	Ref<model::gameobjects::player::PlayerAppearance> appearance = model::gameobjects::player::PlayerAppearance::create();
	f.account->addPlayerAccountData(std::make_unique<model::account::PlayerAccountData>(*f.account, *f.commonData, *appearance));
	f.account->setAccountWarehouse(
		std::make_unique<model::items::storage::PlayerStorage>(*f.account, model::items::storage::StorageType::ACCOUNT_WAREHOUSE));
	Ptr<model::account::PlayerAccountData> accountData = f.account->getPlayerAccountData(objectId);
	f.player = model::gameobjects::VisibleObject::create<ContainerTestPlayer>(*accountData, *f.account);
	return f;
}

class PlayerContainerTest : public ::testing::Test {
protected:
	void SetUp() override {
		utils::ThreadPoolManager::installBackend(nullptr);
		utils::ThreadPoolManager::installBackend(std::make_unique<runtime::DeterministicExecutor>(clock, 11));
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(&noPets);
	}

	void TearDown() override {
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
		runtime::Reclaimer::getInstance().drain();
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
	}

	runtime::ManualClock clock{0};
};

TEST_F(PlayerContainerTest, AddGetRemoveAndDuplicates) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Ref<container::PlayerContainer> players = container::PlayerContainer::create();
	PlayerFixture a = makePlayer(701, "Alpha");
	PlayerFixture b = makePlayer(702, "Beta");
	players->add(*a.player);
	players->add(*b.player);
	EXPECT_EQ(players->get(701).get(), a.player.get());
	EXPECT_EQ(players->get("Beta").get(), b.player.get());
	EXPECT_EQ(players->getAllPlayers().size(), 2u);
	EXPECT_THROW(players->add(*a.player), exceptions::DuplicateAionObjectException);
	players->remove(*a.player);
	EXPECT_FALSE(players->get(701));
	EXPECT_FALSE(players->get("Alpha"));
	EXPECT_EQ(players->get(702).get(), b.player.get());
}

TEST_F(PlayerContainerTest, UpdateCachedPlayerNameMovesTheNameEntry) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	Ref<container::PlayerContainer> players = container::PlayerContainer::create();
	PlayerFixture a = makePlayer(711, "Old");
	PlayerFixture other = makePlayer(712, "Other");
	players->add(*a.player);
	players->add(*other.player);

	a.commonData->setName("New");
	players->updateCachedPlayerName("Old", *a.player);
	EXPECT_EQ(players->get("New").get(), a.player.get());
	EXPECT_FALSE(players->get("Old"));

	// Java: the old name stays mapped when it belongs to another player (the compute callback returns p)
	players->updateCachedPlayerName("Other", *a.player);
	EXPECT_EQ(players->get("Other").get(), other.player.get());
	EXPECT_EQ(players->get("New").get(), a.player.get());
}

TEST_F(PlayerContainerTest, ConcurrentRenamesFinishAndKeepOnlyTheCurrentNames) {
	static constexpr int32_t THREADS = 4;
	static constexpr int32_t ROUNDS = 5000;
	Ref<container::PlayerContainer> players = container::PlayerContainer::create();
	std::vector<PlayerFixture> fixtures;
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		for (int32_t i = 0; i < THREADS; i++) {
			fixtures.push_back(makePlayer(720 + i, "P" + std::to_string(i) + "_0"));
			players->add(*fixtures.back().player);
		}
	}
	std::vector<std::thread> threads;
	for (int32_t t = 0; t < THREADS; t++) {
		threads.emplace_back([&players, &fixtures, t] {
			runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
			PlayerFixture& f = fixtures[static_cast<size_t>(t)];
			for (int32_t round = 1; round <= ROUNDS; round++) {
				std::string oldName = f.commonData->getName();
				f.commonData->setName("P" + std::to_string(t) + "_" + std::to_string(round % 7));
				players->updateCachedPlayerName(oldName, *f.player);
			}
		});
	}
	for (std::thread& thread : threads)
		thread.join();

	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	for (int32_t t = 0; t < THREADS; t++) {
		PlayerFixture& f = fixtures[static_cast<size_t>(t)];
		for (int32_t suffix = 0; suffix < 7; suffix++) {
			std::string name = "P" + std::to_string(t) + "_" + std::to_string(suffix);
			Ptr<Player> found = players->get(name);
			if (name == f.commonData->getName())
				EXPECT_EQ(found.get(), f.player.get()) << name;
			else
				EXPECT_FALSE(found) << name;
		}
	}
}

} // namespace
} // namespace aion::gameserver::world::test
