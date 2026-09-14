// Pins of OwnedParts (runtime-kernel-status.md open item "Pin(&part) and Pin(Ref<Part>) do not retain the part"): a pinned part is counted
// like a Ref<Part>, so a part replaced in a PartSlot<RECLAIMER> while a task that pins it is pending stays alive until the task is gone.

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <utility>

#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/runtime/sched/Pin.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

using namespace aion::gameserver::runtime;
using aion::gameserver::utils::ThreadPoolManager;

namespace {

class Storage final : public OwnedPart {
public:
	explicit Storage(const RefCounted& owner, std::atomic<int32_t>& destroyed) : OwnedPart(owner), destroyed(destroyed) {}
	~Storage() override { destroyed.fetch_add(1); }
	std::atomic<int32_t>& destroyed;
};

class Player final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<Player> create() { return makeRef<Player>(); }
	PartSlot<Storage, RetireTo::RECLAIMER> warehouse{*this};
	PartSlot<Storage, RetireTo::RECLAIMER> inventory{*this};

protected:
	Player() = default;
	~Player() override = default;
};

TEST(PinPartTest, PartsAreCountedAndShareTheirOwnersSlot) {
	TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
	static std::atomic<int32_t> destroyed{0}; // the objects are destroyed by a later scan, after this test returned
	destroyed.store(0);
	Ref<Player> player = Player::create();
	player->warehouse.set(std::make_unique<Storage>(*player, destroyed));
	player->inventory.set(std::make_unique<Storage>(*player, destroyed));
	Storage* warehouse = player->warehouse.get();
	Storage* inventory = player->inventory.get();
	{
		Pin ownerFirst{player.get(), warehouse};
		EXPECT_EQ(ownerFirst.size(), 1u); // the owner is held through the part
		EXPECT_EQ(ownerFirst.part(0), warehouse);
		EXPECT_EQ(player->refCount(), 2u);
		EXPECT_EQ(warehouse->partRefCount(), 1u);

		Pin partFirst{warehouse, player.get()};
		EXPECT_EQ(partFirst.size(), 1u);
		EXPECT_EQ(player->refCount(), 3u);
		EXPECT_EQ(warehouse->partRefCount(), 2u);

		Pin twoParts{warehouse, inventory, Ref<Storage>(inventory)};
		EXPECT_EQ(twoParts.size(), 2u);
		EXPECT_TRUE(twoParts.pins(*player));
		EXPECT_EQ(twoParts.owner(1), player.get());
		EXPECT_EQ(inventory->partRefCount(), 1u);

		Pin copy = twoParts;
		EXPECT_EQ(warehouse->partRefCount(), 4u);
		Pin moved = std::move(copy);
		EXPECT_EQ(copy.size(), 0u); // NOLINT(bugprone-use-after-move): moved-from pins are empty
		moved.reset();
		EXPECT_EQ(warehouse->partRefCount(), 3u);
		EXPECT_EQ(inventory->partRefCount(), 1u);
	}
	EXPECT_EQ(player->refCount(), 1u);
	EXPECT_EQ(warehouse->partRefCount(), 0u);
	EXPECT_EQ(inventory->partRefCount(), 0u);
	EXPECT_EQ(destroyed.load(), 0);
}

TEST(PinPartTest, TooManySlotsThrowAndReleaseEverything) {
	TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
	static std::atomic<int32_t> destroyed{0}; // the objects are destroyed by a later scan, after this test returned
	destroyed.store(0);
	Ref<Player> a = Player::create();
	Ref<Player> b = Player::create();
	a->warehouse.set(std::make_unique<Storage>(*a, destroyed));
	a->inventory.set(std::make_unique<Storage>(*a, destroyed));
	b->warehouse.set(std::make_unique<Storage>(*b, destroyed));
	Ref<Player> c = Player::create();
	Ref<Player> d = Player::create();
	Storage* part = a->warehouse.get();
	EXPECT_NO_THROW((void)Pin({part, a->inventory.get(), b->warehouse.get(), b.get(), a.get(), c})); // b and a share their parts' slots
	EXPECT_THROW((void)Pin({part, a->inventory.get(), b->warehouse.get(), c, d}), IllegalArgumentException);
	EXPECT_EQ(part->partRefCount(), 0u);
	EXPECT_EQ(a->refCount(), 1u);
	EXPECT_EQ(b->refCount(), 1u);
	EXPECT_EQ(d->refCount(), 1u);
}

TEST(PinPartTest, ReplacedPartOutlivesThePendingTaskThatPinsIt) {
	ManualClock clock(0);
	auto backend = std::make_unique<DeterministicExecutor>(clock, 1);
	DeterministicExecutor* executor = backend.get();
	ThreadPoolManager::installBackend(std::move(backend));

	std::atomic<int32_t> destroyed{0};
	static std::atomic<int32_t> runs{0};
	runs.store(0);
	Ref<Player> player = Player::create();
	FutureRef task;
	{
		TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
		player->warehouse.set(std::make_unique<Storage>(*player, destroyed));
		Storage* warehouse = player->warehouse.get();
		task = ThreadPoolManager::getInstance().schedule(Pin(warehouse), [] { runs.fetch_add(1); }, 1000);
	}
	{
		TaskScope scope(AION_TASK_INFO(TaskKind::TEST));
		player->warehouse.set(std::make_unique<Storage>(*player, destroyed)); // Account.accountWarehouse replaced on re-entry
	}
	Reclaimer::getInstance().drain();
	EXPECT_EQ(destroyed.load(), 0) << "the pending task pins the replaced part";

	executor->advance(std::chrono::milliseconds(1000));
	EXPECT_EQ(runs.load(), 1);
	task.reset();
	Reclaimer::getInstance().drain();
	EXPECT_EQ(destroyed.load(), 1);

	ThreadPoolManager::installBackend(nullptr);
	player.reset();
	Reclaimer::getInstance().drain();
	EXPECT_EQ(destroyed.load(), 2);
}

} // namespace
