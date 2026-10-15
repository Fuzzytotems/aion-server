#pragma once

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>

#include "aion/gameserver/runtime/collections/Collections.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"

// Synthetic game classes and fixtures of the collection shim tests.

namespace aion::gameserver::runtime::testcollections {

/** Live instances of the synthetic RefCounted test classes (destructors decrement it). */
inline std::atomic<int64_t> liveObjects{0};

/** AionObject-like class: Java equals/hashCode on objectId (relogin creates a second instance with the same id), compareTo on objectId. */
class Player final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<Player> create(int32_t objectId, std::string name = {}) { return makeRef<Player>(objectId, std::move(name)); }

	int32_t getObjectId() const noexcept { return objectId; }
	const std::string& getName() const noexcept { return name; }
	bool equals(const Player& other) const noexcept { return objectId == other.objectId; }
	int32_t hashCode() const noexcept { return objectId; }
	int32_t compareTo(const Player& other) const noexcept { return objectId < other.objectId ? -1 : (objectId > other.objectId ? 1 : 0); }

protected:
	Player(int32_t objectId, std::string name) : objectId(objectId), name(std::move(name)) { ++liveObjects; }
	~Player() override { --liveObjects; }

private:
	const int32_t objectId;
	const std::string name;
};

/** Class without Java equals: identity semantics in every shim. */
class Npc final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<Npc> create(int32_t objectId) { return makeRef<Npc>(objectId); }
	int32_t getObjectId() const noexcept { return objectId; }

protected:
	explicit Npc(int32_t objectId) : objectId(objectId) { ++liveObjects; }
	~Npc() override { --liveObjects; }

private:
	const int32_t objectId;
};

/** Opens a task scope per test and drains the Reclaimer afterwards, so every test starts and ends with nothing retired. */
class CollectionsTest : public ::testing::Test {
protected:
	void SetUp() override {
		Reclaimer::getInstance().drain();
		scope.emplace(AION_TASK_INFO(TaskKind::TEST));
	}
	void TearDown() override {
		scope.reset();
		Reclaimer::getInstance().drain();
	}

	/** Ends the test's scope, reclaims everything and reopens a scope (objects released so far are destroyed). */
	void reclaimAll() {
		scope.reset();
		Reclaimer::getInstance().drain();
		scope.emplace(AION_TASK_INFO(TaskKind::TEST));
	}

	std::optional<TaskScope> scope;
};

} // namespace aion::gameserver::runtime::testcollections
