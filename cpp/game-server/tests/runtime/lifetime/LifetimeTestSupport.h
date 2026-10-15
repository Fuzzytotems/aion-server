#pragma once

// Test objects and helpers of the lifetime core tests. Tracked objects record their destruction OUTSIDE their own memory (in a Tracker), so a
// reader can detect "this object was already destroyed" before it touches freed memory, which keeps mutation tests deterministic and
// independent of ASan.

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>

#include "aion/gameserver/runtime/base/YieldPoint.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"

namespace aion::gameserver::runtime::lifetimetest {

/** Thrown by test threads that observe a protocol violation (use after free, leak, double destruction). */
struct LifetimeViolation : std::runtime_error {
	using std::runtime_error::runtime_error;
};

/** Registry of tracked allocations: addresses and destruction counts, stored outside the tracked objects. */
struct Tracker {
	static constexpr size_t MAX = 64;
	std::array<std::atomic<const void*>, MAX> addresses{};
	std::array<std::atomic<uint32_t>, MAX> destroyed{};
	std::atomic<uint32_t> count{0};

	uint32_t registerObject(const void* address) {
		uint32_t id = count.fetch_add(1);
		if (id >= MAX)
			throw std::logic_error("Tracker: too many objects");
		addresses[id].store(address);
		return id;
	}
	/** id of the most recently registered object at `address` (the only one that can be alive there) */
	uint32_t find(const void* address) const {
		for (uint32_t id = count.load(); id-- > 0;)
			if (addresses[id].load() == address)
				return id;
		throw std::logic_error("Tracker: unknown address");
	}
	uint32_t destroyedCount(uint32_t id) const { return destroyed[id].load(); }

	/** Throws LifetimeViolation unless every registered object was destroyed exactly once. */
	void expectAllDestroyedOnce(const char* what) const {
		for (uint32_t id = 0; id < count.load(); ++id) {
			uint32_t times = destroyed[id].load();
			if (times != 1)
				throw LifetimeViolation(std::string(what) + ": object " + std::to_string(id) + " destroyed " + std::to_string(times) + " times");
		}
	}
};

inline constexpr uint64_t PAYLOAD_MAGIC = 0x7EA5'17ED'0B1E'C700;

/** A shared game object stand-in. */
class Tracked final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<Tracked> create(const std::shared_ptr<Tracker>& tracker, Ref<Tracked> child = nullptr) {
		return makeRef<Tracked>(tracker, std::move(child));
	}

	uint32_t id() const noexcept { return id_; }
	uint64_t payload() const noexcept { return payload_; }

	/** SelfOrRef field of the object itself (TargetField pattern). */
	SelfOrRef<Tracked> target{static_cast<const RefCounted&>(*this)};

protected:
	Tracked(const std::shared_ptr<Tracker>& tracker, Ref<Tracked> child)
		: tracker_(tracker), id_(tracker->registerObject(this)), payload_(PAYLOAD_MAGIC ^ id_), child_(std::move(child)) {}
	~Tracked() override {
		payload_ = 0;
		tracker_->destroyed[id_].fetch_add(1);
	}

private:
	const std::shared_ptr<Tracker> tracker_;
	const uint32_t id_;
	uint64_t payload_;
	Ref<Tracked> child_; // released by the destructor (cascade)
};

/**
 * Uses a borrowed Tracked object: first checks the tracker (without touching the object's memory), then reads the payload through the checked
 * Ptr. Throws LifetimeViolation if the object was destroyed. Contains no yield point, so under PCT the check and the read are atomic.
 */
inline void useTracked(const Tracker& tracker, Ptr<Tracked> object) {
	if (!object)
		return;
	uint32_t id = tracker.find(object.rawPointer());
	if (tracker.destroyedCount(id) != 0)
		throw LifetimeViolation("use after free: object " + std::to_string(id) + " was destroyed while borrowed");
	if (object->payload() != (PAYLOAD_MAGIC ^ id))
		throw LifetimeViolation("corrupted object " + std::to_string(id));
}

/** A part stand-in. */
class TrackedPart final : public OwnedPart {
public:
	TrackedPart(const RefCounted& owner, const std::shared_ptr<Tracker>& tracker)
		: OwnedPart(owner), tracker_(tracker), id_(tracker->registerObject(this)), payload_(PAYLOAD_MAGIC ^ id_) {}
	~TrackedPart() override {
		payload_ = 0;
		tracker_->destroyed[id_].fetch_add(1);
	}
	uint32_t id() const noexcept { return id_; }
	uint64_t payload() const noexcept { return payload_; }

private:
	const std::shared_ptr<Tracker> tracker_;
	const uint32_t id_;
	uint64_t payload_;
};

inline void useTrackedPart(const Tracker& tracker, const TrackedPart* part) {
	if (part == nullptr)
		return;
	uint32_t id = tracker.find(part);
	if (tracker.destroyedCount(id) != 0)
		throw LifetimeViolation("use after free: part " + std::to_string(id) + " was destroyed while borrowed");
	if (part->payload() != (PAYLOAD_MAGIC ^ id))
		throw LifetimeViolation("corrupted part " + std::to_string(id));
}

/** Owner of parts in all container flavours. */
class PartOwner final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	static Ref<PartOwner> create() { return makeRef<PartOwner>(); }

	PartSlot<TrackedPart> ownerSlot{*this};
	PartSlot<TrackedPart, RetireTo::RECLAIMER> reclaimerSlot{*this};
	PartMap<int32_t, TrackedPart> map{*this};
	PartList<TrackedPart> list{*this};

protected:
	PartOwner() = default;
	~PartOwner() override = default;
};

/** A retired node stand-in. */
class TrackedNode final : public RetiredNode {
public:
	explicit TrackedNode(const std::shared_ptr<Tracker>& tracker) : tracker_(tracker), id_(tracker->registerObject(this)), payload_(PAYLOAD_MAGIC ^ id_) {}
	~TrackedNode() override {
		payload_ = 0;
		tracker_->destroyed[id_].fetch_add(1);
	}
	size_t retiredBytes() const noexcept override { return 1000; }
	uint64_t payload() const noexcept { return payload_; }

private:
	const std::shared_ptr<Tracker> tracker_;
	const uint32_t id_;
	uint64_t payload_;
};

/**
 * A lock-free shared location holding a Ref (the essence of Field<Ref<T>>): load() is a pointer load with read barrier, exchange() unlinks.
 * Yield points "test:location:load" (after publication, before the load) and "test:location:exchange".
 */
template <class T>
class SharedLocation {
public:
	SharedLocation() = default;
	~SharedLocation() {
		if (T* stored = slot.load())
			stored->release();
	}
	SharedLocation(const SharedLocation&) = delete;
	SharedLocation& operator=(const SharedLocation&) = delete;

	Ptr<T> load() const {
		TaskScope::ensurePublished();
		pct::yieldPoint("test:location:load");
		return Ptr<T>(slot.load());
	}
	Ref<T> exchange(Ref<T> value) {
		pct::yieldPoint("test:location:exchange");
		return Ref<T>::adopt(slot.exchange(value.leak()));
	}
	/** Stores `value`; the previous value is released after the unlink. */
	void store(Ref<T> value) { (void)exchange(std::move(value)); }

private:
	std::atomic<T*> slot{nullptr};
};

/** Shared location of a retired node (the essence of a ConcurrentHashMap table or Field<std::string> box). */
class NodeLocation {
public:
	~NodeLocation() { delete slot.load(); }
	const TrackedNode* load() const {
		TaskScope::ensurePublished();
		pct::yieldPoint("test:node:load");
		return slot.load();
	}
	void replace(std::unique_ptr<TrackedNode> node) {
		pct::yieldPoint("test:node:exchange");
		Reclaimer::retireNode(std::unique_ptr<RetiredNode>(slot.exchange(node.release())));
	}

private:
	std::atomic<TrackedNode*> slot{nullptr};
};

inline void useTrackedNode(const Tracker& tracker, const TrackedNode* node) {
	if (node == nullptr)
		return;
	uint32_t id = tracker.find(node);
	if (tracker.destroyedCount(id) != 0)
		throw LifetimeViolation("use after free: node " + std::to_string(id) + " was destroyed while loaded");
	if (node->payload() != (PAYLOAD_MAGIC ^ id))
		throw LifetimeViolation("corrupted node " + std::to_string(id));
}

inline TaskInfo testTask() {
	return AION_TASK_INFO(TaskKind::TEST);
}

/** Drains the Reclaimer from a thread without a published epoch; fails the test if the backlog does not empty. */
inline void drainReclaimer() {
	ASSERT_TRUE(Reclaimer::getInstance().drain(128)) << "Reclaimer backlog did not drain: " << Reclaimer::getInstance().stats().backlog;
}

} // namespace aion::gameserver::runtime::lifetimetest
