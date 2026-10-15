// Hub header reference classes of S0b (docs/design/hub-headers.md): AionObject (Java equals/hashCode, auto-release ids through the CleanerQueue
// instead of Java's Cleaner) and Persistable (the static state predicates built during static initialization).

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "aion/gameserver/model/gameobjects/AionObject.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/services/CleanerQueue.h"

namespace aion::gameserver::model::gameobjects {
namespace {

class TestObject final : public AionObject {
	AION_MAKE_REF_FRIEND
protected:
	TestObject(int32_t objId, bool autoReleaseObjectId) : AionObject(objId, autoReleaseObjectId) {}
	~TestObject() override = default;

public:
	static runtime::Ref<TestObject> create(int32_t objId, bool autoReleaseObjectId = false) {
		return runtime::makeRef<TestObject>(objId, autoReleaseObjectId);
	}
	std::string getName() override { return "test"; }
};

class TestPersistable final : public Persistable {
public:
	explicit TestPersistable(PersistentState initial) : state(initial) {}
	PersistentState getPersistentState() override { return state; }
	void setPersistentState(PersistentState value) override { state = value; }

private:
	PersistentState state;
};

struct ReleasedIds {
	static inline std::mutex mutex;
	static inline std::vector<int32_t> ids;

	static void record(int32_t objectId, const char*) {
		std::scoped_lock lock(mutex);
		ids.push_back(objectId);
	}
	static std::vector<int32_t> take() {
		std::scoped_lock lock(mutex);
		return std::exchange(ids, {});
	}
};

TEST(AionObjectTest, EqualsComparesObjectIdsAndDummiesOnlyEqualThemselves) {
	runtime::Ref<TestObject> a = TestObject::create(42);
	runtime::Ref<TestObject> b = TestObject::create(42);
	runtime::Ref<TestObject> c = TestObject::create(43);
	runtime::Ref<TestObject> dummy = TestObject::create(0);
	runtime::Ref<TestObject> otherDummy = TestObject::create(0);
	EXPECT_TRUE(a->equals(*b));   // Java: relogin creates a new object with the same id, which compares equal
	EXPECT_FALSE(a->equals(*c));
	EXPECT_TRUE(dummy->equals(*dummy));
	EXPECT_FALSE(dummy->equals(*otherDummy));
	EXPECT_EQ(a->hashCode(), 42);
	EXPECT_EQ(a->getObjectId(), 42);
}

TEST(AionObjectTest, TheDestructorQueuesOnlyAutoReleaseIds) {
	runtime::Reclaimer::getInstance().drain(); // objects of earlier tests
	runtime::CleanerQueue::setCleanerAction(&ReleasedIds::record);
	(void)runtime::CleanerQueue::drainNow();
	(void)ReleasedIds::take();
	{
		runtime::Ref<TestObject> autoRelease = TestObject::create(7, true);
		runtime::Ref<TestObject> kept = TestObject::create(8, false);
		runtime::Ref<TestObject> dummy = TestObject::create(0, true); // Java registers nothing for objectId 0
	}
	runtime::Reclaimer::getInstance().drain();
	EXPECT_EQ(runtime::CleanerQueue::drainNow(), 1u);
	EXPECT_EQ(ReleasedIds::take(), std::vector<int32_t>{7});
	runtime::CleanerQueue::setCleanerAction(nullptr);
}

TEST(PersistableTest, StaticPredicatesMatchTheirState) {
	TestPersistable fresh(Persistable::PersistentState::NEW);
	TestPersistable changed(Persistable::PersistentState::UPDATE_REQUIRED);
	TestPersistable deleted(Persistable::PersistentState::DELETED);
	EXPECT_TRUE(Persistable::NEW(fresh));
	EXPECT_FALSE(Persistable::NEW(changed));
	EXPECT_TRUE(Persistable::CHANGED(changed));
	EXPECT_FALSE(Persistable::CHANGED(deleted));
	EXPECT_TRUE(Persistable::DELETED(deleted));
	TestPersistable untouched(Persistable::PersistentState::NOACTION);
	EXPECT_TRUE(Persistable::newPredicate(Persistable::PersistentState::NOACTION)(untouched));
	changed.setPersistentState(Persistable::PersistentState::UPDATED);
	EXPECT_FALSE(Persistable::CHANGED(changed));
}

} // namespace
} // namespace aion::gameserver::model::gameobjects
