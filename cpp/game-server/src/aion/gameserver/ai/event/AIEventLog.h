#pragma once

#include <cstdint>
#include <iterator>

#include "aion/gameserver/runtime/collections/ArrayDeque.h"
#include "aion/gameserver/runtime/collections/Iterators.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/ai/event/fwd.h"

namespace aion::gameserver::ai::event {

/**
 * The last events an AI handled, newest first (a bounded deque that drops its oldest event when full).
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `AbstractAI::eventLog`), created with create(). Java
 * extends `LinkedBlockingDeque<AIEventType>`; C++ holds the deque and its capacity as members and declares the inherited operations its users
 * call (AbstractAI.addFirst, the `//ai` command's isEmpty and iteration, offerFirst's remainingCapacity/removeLast).
 *
 * @author ATracer
 */
class AIEventLog : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	static constexpr int64_t serialVersionUID = -7234174243343636729LL;

	/** C++ only: the elements of Java's LinkedBlockingDeque base (head = newest) */
	runtime::ArrayDeque<AIEventType> events{AION_LOCK_CLASS(AIEventLog::events)};
	/** C++ only: the capacity of Java's LinkedBlockingDeque base (Integer.MAX_VALUE for the default constructor) */
	const int32_t capacity;

protected:
	AIEventLog();
	explicit AIEventLog(int32_t capacity);
	~AIEventLog() override;

public:
	/** Java: new AIEventLog() */
	static runtime::Ref<AIEventLog> create();

	/** Java: new AIEventLog(capacity) */
	static runtime::Ref<AIEventLog> create(int32_t capacity);

	/** Java: @Override of LinkedBlockingDeque.offerFirst; synchronized */
	bool offerFirst(AIEventType e);

	/** Java: LinkedBlockingDeque.addFirst (calls offerFirst) */
	void addFirst(AIEventType e);

	/** Java: LinkedBlockingDeque.remainingCapacity */
	int32_t remainingCapacity();

	/** Java: LinkedBlockingDeque.removeLast */
	AIEventType removeLast();

	/** Java: LinkedBlockingDeque.isEmpty */
	bool isEmpty();

	/** Java: LinkedBlockingDeque.size */
	int32_t size();

	/** Java: LinkedBlockingDeque.iterator (head to tail, a snapshot, §7.2) */
	runtime::JavaIterator<AIEventType> iterator();

	/** C++ only: range-for over a snapshot (§7.2) */
	runtime::SnapshotIterator<AIEventType> begin();

	std::default_sentinel_t end() const noexcept { return {}; }
};

} // namespace aion::gameserver::ai::event
