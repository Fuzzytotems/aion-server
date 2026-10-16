// Conformance of parts (design §2.3, §3.2.1): OwnedPart binding (C11), PartSlot OWNER/RECLAIMER, PartMap, PartList, SelfOrRef.

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include "LifetimeTestSupport.h"

using namespace aion::gameserver::runtime;
using namespace aion::gameserver::runtime::lifetimetest;

namespace {

/** Late-bound controller (pattern 3). */
class LateController final : public OwnedPart {
public:
	LateController() = default;
	void setOwner(const RefCounted& owner) { bindOwner(owner); }
};

class Npc final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<Npc> create() { return makeRef<Npc>(std::make_unique<LateController>()); }
	const std::unique_ptr<LateController> controller;

protected:
	explicit Npc(std::unique_ptr<LateController> controller) : controller(std::move(controller)) { this->controller->setOwner(*this); }
	~Npc() override = default;
};

/** A storage part whose actor is its owner or a foreign object (PlayerStorage.actor). */
class Storage final : public OwnedPart {
public:
	explicit Storage(const RefCounted& owner) : OwnedPart(owner) {}
	SelfOrRef<Tracked> actor{static_cast<const OwnedPart&>(*this)};
};

class Player final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<Player> create() { return makeRef<Player>(); }
	const std::unique_ptr<Storage> inventory = std::make_unique<Storage>(*this);

protected:
	Player() = default;
	~Player() override = default;
};

struct Item final : OwnedPart {
	explicit Item(const RefCounted& owner, int value) : OwnedPart(owner), value(value) {}
	const int value;
};

class Group final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<Group> create() { return makeRef<Group>(); }
	PartList<Item> items{*this};

protected:
	Group() = default;
	~Group() override = default;
};

} // namespace

TEST(PartsTest, LateBoundControllerSharesTheOwnersCount) {
	Ref<Npc> npc = Npc::create();
	EXPECT_TRUE(npc->controller->isOwnerBound());
	EXPECT_EQ(&npc->controller->partOwner(), npc.get());
	{
		Ref<LateController> controller(npc->controller.get());
		EXPECT_EQ(npc->refCount(), 2u);
	}
	EXPECT_EQ(npc->refCount(), 1u);
	EXPECT_NE(&npc->controller->monitor(), &npc->monitor()) << "a part has its own Java monitor";
}

TEST(PartsTest, PartSlotOwnerKeepsReplacedPartsUntilTheOwnerDies) {
	drainReclaimer();
	auto tracker = std::make_shared<Tracker>();
	Ref<PartOwner> owner = PartOwner::create();
	TaskScope scope(testTask());
	EXPECT_EQ(owner->ownerSlot.get(), nullptr);
	EXPECT_FALSE(owner->ownerSlot);
	EXPECT_THROW((void)owner->ownerSlot->id(), NullPointerException);
	owner->ownerSlot.set(std::make_unique<TrackedPart>(*owner, tracker));
	TrackedPart* first = owner->ownerSlot.get();
	owner->ownerSlot.set(std::make_unique<TrackedPart>(*owner, tracker));
	owner->ownerSlot.set(nullptr);
	EXPECT_FALSE(owner->ownerSlot);
	Reclaimer::getInstance().drain(4);
	EXPECT_EQ(tracker->destroyedCount(0), 0u) << "OWNER: replaced parts live as long as the owner (//ai set keeps retired AIs)";
	EXPECT_EQ(first->id(), 0u);
	EXPECT_EQ(owner->refCount(), 1u) << "OWNER retirement does not retain the owner";
}

TEST(PartsTest, PartSlotOwnerPartsAreDestroyedWithTheOwner) {
	drainReclaimer();
	auto tracker = std::make_shared<Tracker>();
	{
		Ref<PartOwner> owner = PartOwner::create();
		owner->ownerSlot.set(std::make_unique<TrackedPart>(*owner, tracker));
		owner->ownerSlot.set(std::make_unique<TrackedPart>(*owner, tracker));
	}
	drainReclaimer();
	EXPECT_NO_THROW(tracker->expectAllDestroyedOnce("owner slot"));
}

TEST(PartsTest, PartSlotReclaimerRetiresReplacedPartsByEpoch) {
	drainReclaimer();
	auto tracker = std::make_shared<Tracker>();
	Ref<PartOwner> owner = PartOwner::create();
	owner->reclaimerSlot.set(std::make_unique<TrackedPart>(*owner, tracker));
	{
		TaskScope scope(testTask());
		TrackedPart* borrowed = owner->reclaimerSlot.get();
		owner->reclaimerSlot.set(std::make_unique<TrackedPart>(*owner, tracker)); // Account.accountWarehouse replaced on re-entry
		EXPECT_EQ(owner->refCount(), 2u) << "the retired part retains its owner";
		Reclaimer::getInstance().reclaimNow();
		EXPECT_EQ(tracker->destroyedCount(0), 0u);
		EXPECT_NO_THROW(useTrackedPart(*tracker, borrowed));
		EXPECT_EQ(owner->reclaimerSlot->id(), 1u);
	}
	drainReclaimer();
	EXPECT_EQ(tracker->destroyedCount(0), 1u);
	EXPECT_EQ(tracker->destroyedCount(1), 0u);
	EXPECT_EQ(owner->refCount(), 1u);
	owner.reset();
	drainReclaimer();
	EXPECT_EQ(tracker->destroyedCount(1), 1u);
}

TEST(PartsTest, PartMapOperationsAndEpochRetirement) {
	drainReclaimer();
	auto tracker = std::make_shared<Tracker>();
	Ref<PartOwner> owner = PartOwner::create();
	PartMap<int32_t, TrackedPart>& map = owner->map;
	EXPECT_TRUE(map.isEmpty());
	map.put(2, std::make_unique<TrackedPart>(*owner, tracker)); // id 0
	map.put(1, std::make_unique<TrackedPart>(*owner, tracker)); // id 1
	EXPECT_THROW(map.put(3, nullptr), NullPointerException);
	EXPECT_EQ(map.size(), 2);
	EXPECT_TRUE(map.containsKey(1));
	EXPECT_FALSE(map.containsKey(3));
	{
		TaskScope scope(testTask());
		Ptr<TrackedPart> two = map.get(2);
		EXPECT_EQ(two->id(), 0u);
		EXPECT_FALSE(map.get(3));
		auto snapshot = map.snapshot();
		ASSERT_EQ(snapshot.size(), 2u);
		EXPECT_EQ(snapshot[0].first, 1);
		EXPECT_EQ(snapshot[0].second->id(), 1u);
		EXPECT_EQ(map.values().size(), 2u);

		map.put(2, std::make_unique<TrackedPart>(*owner, tracker)); // id 2 replaces id 0
		EXPECT_TRUE(map.remove(1));
		EXPECT_FALSE(map.remove(1));
		EXPECT_EQ(map.size(), 1);
		Reclaimer::getInstance().reclaimNow();
		EXPECT_EQ(tracker->destroyedCount(0), 0u);
		EXPECT_EQ(two->id(), 0u) << "a replaced part stays readable until the task ends";
		EXPECT_EQ(owner->refCount(), 3u);
	}
	drainReclaimer();
	EXPECT_EQ(tracker->destroyedCount(0), 1u);
	EXPECT_EQ(tracker->destroyedCount(1), 1u);
	EXPECT_EQ(tracker->destroyedCount(2), 0u);
	EXPECT_EQ(owner->refCount(), 1u);
	owner.reset();
	drainReclaimer();
	EXPECT_EQ(tracker->destroyedCount(2), 1u) << "remaining parts are destroyed with the owner";
}

// Review finding: a Ref to a part replaced in a RECLAIMER container (Player.playerAccountData, Account.accountWarehouse) keeps the part alive
// until the Ref is gone, and its release stamps the part so a borrow taken from the Ref stays valid until the task ends.
TEST(PartsTest, RefsToReplacedPartsKeepThePartAlive) {
	drainReclaimer();
	auto tracker = std::make_shared<Tracker>();
	Ref<PartOwner> owner = PartOwner::create();
	owner->reclaimerSlot.set(std::make_unique<TrackedPart>(*owner, tracker)); // id 0
	owner->map.put(7, std::make_unique<TrackedPart>(*owner, tracker));        // id 1
	Ref<TrackedPart> warehouse;
	Ref<TrackedPart> accountData;
	{
		TaskScope scope(testTask());
		warehouse = Ref<TrackedPart>(owner->reclaimerSlot.get());
		accountData = Ref<TrackedPart>(owner->map.get(7));
	}
	owner->reclaimerSlot.set(std::make_unique<TrackedPart>(*owner, tracker)); // id 2, retires id 0
	owner->map.put(7, std::make_unique<TrackedPart>(*owner, tracker));        // id 3, retires id 1
	for (int i = 0; i < 4; ++i)
		Reclaimer::getInstance().reclaimNow();
	EXPECT_EQ(Reclaimer::getInstance().stats().backlog, 2u) << "retired parts held by Refs stay in the backlog";
	EXPECT_EQ(tracker->destroyedCount(0), 0u) << "a Ref<Part> keeps a part replaced in a PartSlot<RECLAIMER>";
	EXPECT_EQ(tracker->destroyedCount(1), 0u) << "a Ref<Part> keeps a part replaced in a PartMap";
	EXPECT_NO_THROW(useTrackedPart(*tracker, warehouse.get()));
	EXPECT_NO_THROW(useTrackedPart(*tracker, accountData.get()));
	{
		TaskScope scope(testTask());
		Ptr<TrackedPart> borrowed = accountData;
		accountData.reset();
		warehouse.reset();
		Reclaimer::getInstance().reclaimNow();
		Reclaimer::getInstance().reclaimNow();
		EXPECT_EQ(tracker->destroyedCount(1), 0u) << "the last release stamped the part: the borrow keeps it until the scope ends";
		EXPECT_NO_THROW(useTrackedPart(*tracker, borrowed.rawPointer()));
	}
	drainReclaimer();
	EXPECT_EQ(tracker->destroyedCount(0), 1u);
	EXPECT_EQ(tracker->destroyedCount(1), 1u);
	EXPECT_EQ(owner->refCount(), 1u) << "destroyed retired parts released their owner";
	owner.reset();
	drainReclaimer();
	EXPECT_NO_THROW(tracker->expectAllDestroyedOnce("refs to replaced parts"));
}

TEST(PartsTest, PartListHasStableAddressesAndLockFreeReaders) {
	drainReclaimer();
	auto tracker = std::make_shared<Tracker>();
	Ref<PartOwner> owner = PartOwner::create();
	PartList<TrackedPart>& list = owner->list;
	EXPECT_TRUE(list.isEmpty());
	std::vector<TrackedPart*> addresses;
	for (int i = 0; i < 40; ++i)
		addresses.push_back(&list.add(std::make_unique<TrackedPart>(*owner, tracker)));
	EXPECT_EQ(list.size(), 40);
	TaskScope scope(testTask());
	for (int i = 0; i < 40; ++i)
		EXPECT_EQ(list.get(i).rawPointer(), addresses[static_cast<size_t>(i)]);
	EXPECT_THROW((void)list.get(40), IndexOutOfBoundsException);
	EXPECT_THROW((void)list.get(-1), IndexOutOfBoundsException);
	EXPECT_THROW((void)list.add(nullptr), NullPointerException);
	EXPECT_EQ(list.snapshot().size(), 40u);
}

TEST(PartsTest, PartListGrowsWhileReadersRead) {
	Ref<Group> group = Group::create();
	std::atomic<bool> stop{false};
	std::atomic<bool> mismatch{false};
	std::thread reader([&] {
		TaskScope scope(testTask());
		while (!stop.load()) {
			int size = group->items.size();
			for (int i = 0; i < size; i += 97)
				if (group->items.get(i)->value != i)
					mismatch = true;
		}
	});
	for (int i = 0; i < 5000; ++i)
		group->items.add(std::make_unique<Item>(*group, i));
	stop = true;
	reader.join();
	EXPECT_FALSE(mismatch.load());
	EXPECT_EQ(group->items.size(), 5000);
}

TEST(PartsTest, SelfOrRefStoresOwnerWithoutRetainingAndForeignObjectsRetained) {
	drainReclaimer();
	auto tracker = std::make_shared<Tracker>();
	Ref<Tracked> self = Tracked::create(tracker);
	Ref<Tracked> foreign = Tracked::create(tracker);
	TaskScope scope(testTask());
	EXPECT_FALSE(self->target);
	EXPECT_THROW((void)self->target->id(), NullPointerException);

	self->target = Ptr<Tracked>(*self);
	EXPECT_EQ(self->refCount(), 1u) << "self: non-retaining (no self cycle, RT-4)";
	EXPECT_EQ(self->target.get(), self);

	Ref<Tracked> previous = self->target.exchange(foreign);
	EXPECT_EQ(previous, self);
	EXPECT_EQ(self->refCount(), 2u) << "exchange hands out a new reference to the owner";
	previous.reset();
	EXPECT_EQ(foreign->refCount(), 2u) << "foreign: retaining";
	EXPECT_EQ(self->target->id(), 1u);

	EXPECT_FALSE(self->target.compareAndSet(Ptr<Tracked>(*self), nullptr));
	EXPECT_EQ(foreign->refCount(), 2u) << "a failed compareAndSet leaves counts unchanged";
	EXPECT_TRUE(self->target.compareAndSet(foreign, Ptr<Tracked>(*self)));
	EXPECT_EQ(foreign->refCount(), 1u) << "the replaced foreign value is released";
	EXPECT_EQ(self->refCount(), 1u);
	EXPECT_TRUE(self->target.compareAndSet(Ptr<Tracked>(*self), foreign));
	EXPECT_EQ(foreign->refCount(), 2u);
	self->target = nullptr;
	EXPECT_EQ(foreign->refCount(), 1u);
	EXPECT_TRUE(self->target.compareAndSet(nullptr, foreign));
	EXPECT_EQ(foreign->refCount(), 2u);
}

TEST(PartsTest, SelfOrRefDestructorReleasesForeignValue) {
	drainReclaimer();
	auto tracker = std::make_shared<Tracker>();
	Ref<Tracked> foreign = Tracked::create(tracker);
	{
		Ref<Tracked> holder = Tracked::create(tracker);
		TaskScope scope(testTask());
		holder->target = Ptr<Tracked>(foreign);
		EXPECT_EQ(foreign->refCount(), 2u);
	}
	drainReclaimer();
	EXPECT_EQ(tracker->destroyedCount(1), 1u);
	EXPECT_EQ(foreign->refCount(), 1u);
}

TEST(PartsTest, SelfOrRefOfAPartComparesAgainstThePartOwner) {
	auto tracker = std::make_shared<Tracker>();
	Ref<Player> player = Player::create();
	TaskScope scope(testTask());
	// the actor type differs from the owner type here, so every value is foreign; identity with the owner is exercised by Tracked::target
	Ref<Tracked> entering = Tracked::create(tracker);
	player->inventory->actor = Ptr<Tracked>(entering);
	EXPECT_EQ(entering->refCount(), 2u);
	player->inventory->actor = nullptr;
	EXPECT_EQ(entering->refCount(), 1u);
}

#if AION_CHECKED
TEST(PartsDeathTest, BindOwnerTwiceTerminates) {
	GTEST_FLAG_SET(death_test_style, "threadsafe");
	EXPECT_DEATH(
		{
			Ref<Npc> npc = Npc::create();
			npc->controller->setOwner(*npc);
		},
		"C11");
}

TEST(PartsDeathTest, BindOwnerAfterPublicationTerminates) {
	GTEST_FLAG_SET(death_test_style, "threadsafe");
	EXPECT_DEATH(
		{
			Ref<Npc> npc = Npc::create();
			Ref<Npc> published = npc;
			LateController controller;
			controller.setOwner(*npc);
		},
		"C11");
}

TEST(PartsDeathTest, RetainOfUnboundPartTerminates) {
	GTEST_FLAG_SET(death_test_style, "threadsafe");
	EXPECT_DEATH(
		{
			LateController controller;
			controller.retain();
		},
		"C11");
}

TEST(PartsDeathTest, StoringAPartOfAnotherOwnerTerminates) {
	GTEST_FLAG_SET(death_test_style, "threadsafe");
	EXPECT_DEATH(
		{
			Ref<PartOwner> owner = PartOwner::create();
			Ref<PartOwner> other = PartOwner::create();
			owner->reclaimerSlot.set(std::make_unique<TrackedPart>(*other, std::make_shared<Tracker>()));
		},
		"C11");
}
#endif
