#include "aion/gameserver/ai/event/AIEventLog.h"

#include <limits>

#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::ai::event {

AIEventLog::AIEventLog() : AIEventLog(std::numeric_limits<int32_t>::max()) {
}

AIEventLog::AIEventLog(int32_t capacityValue) : capacity(capacityValue) {
	// Java: LinkedBlockingDeque(capacity) throws IllegalArgumentException if capacity <= 0 (no caller passes one)
}

AIEventLog::~AIEventLog() = default;

runtime::Ref<AIEventLog> AIEventLog::create() {
	return runtime::makeRef<AIEventLog>();
}

runtime::Ref<AIEventLog> AIEventLog::create(int32_t capacityValue) {
	return runtime::makeRef<AIEventLog>(capacityValue);
}

bool AIEventLog::offerFirst(AIEventType e) {
	SYNCHRONIZED(*this) {
		if (remainingCapacity() == 0) {
			removeLast();
		}
		// Java: super.offerFirst(e) (returns false only when full, which the removal above rules out while this monitor is held)
		events.offerFirst(e);
	}
	return true;
}

void AIEventLog::addFirst(AIEventType e) {
	// Java: LinkedBlockingDeque.addFirst: if (!offerFirst(e)) throw new IllegalStateException("Deque full"); the override always returns true
	offerFirst(e);
}

int32_t AIEventLog::remainingCapacity() {
	return capacity - events.size();
}

AIEventType AIEventLog::removeLast() {
	return events.removeLast(); // Java: NoSuchElementException when empty (the shim throws the same)
}

bool AIEventLog::isEmpty() {
	return events.isEmpty();
}

int32_t AIEventLog::size() {
	return events.size();
}

runtime::JavaIterator<AIEventType> AIEventLog::iterator() {
	return events.iterator();
}

runtime::SnapshotIterator<AIEventType> AIEventLog::begin() {
	return events.begin();
}

} // namespace aion::gameserver::ai::event
