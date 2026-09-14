#include "aion/gameserver/ai/event/AIEventLog.h"

#include <limits>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/ai/event/AIEventType.h"

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
	AION_UNPORTED();
}

void AIEventLog::addFirst(AIEventType e) {
	AION_UNPORTED();
}

int32_t AIEventLog::remainingCapacity() {
	AION_UNPORTED();
}

AIEventType AIEventLog::removeLast() {
	AION_UNPORTED();
}

bool AIEventLog::isEmpty() {
	AION_UNPORTED();
}

int32_t AIEventLog::size() {
	AION_UNPORTED();
}

runtime::JavaIterator<AIEventType> AIEventLog::iterator() {
	AION_UNPORTED();
}

runtime::SnapshotIterator<AIEventType> AIEventLog::begin() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::ai::event
