#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/ArrayDeque.h"
#include "aion/gameserver/runtime/sync/LockClass.h"

namespace aion::gameserver::runtime {

/**
 * Java: java.util.concurrent.ConcurrentLinkedQueue.
 *
 * IMPLEMENTATION CHOICE (collections stage, reported as a design issue): the design lists this queue as lock-free (§3.3). A lock-free
 * Michael-Scott queue with epoch reclamation cannot support Java's remove(Object) of interior elements (used by PlayerEnterWorldService
 * .enteringWorld and TrapService) without either leaking logically deleted nodes or unlinking races that double-retire nodes. The five Java
 * fields are low-rate, so the shim is the Monitor-guarded deque of ArrayDeque: every operation takes the queue's Monitor (lock class
 * `Owner::field`), isEmpty()/size() are lock-free atomic reads, removeIf predicates run under the Monitor, iteration uses snapshots. The
 * Java-visible semantics are unchanged (operations are atomic instead of weakly consistent). Nulls are rejected (NullPointerException).
 * Thread-safety: every member is thread-safe.
 */
template <class T>
class ConcurrentLinkedQueue : public detail::MonitorDeque<T> {
public:
	ConcurrentLinkedQueue() : detail::MonitorDeque<T>(LockClass::named("ConcurrentLinkedQueue")) {}
	explicit ConcurrentLinkedQueue(const LockClass& lockClass) : detail::MonitorDeque<T>(lockClass) {}
};

/**
 * Java: java.util.concurrent.ConcurrentLinkedDeque (LootGroupRules.itemsToBeDistributed). Same model as ConcurrentLinkedQueue plus deque
 * operations; getFirst/getLast/removeFirst/removeLast/pop throw NoSuchElementException when empty.
 */
template <class T>
class ConcurrentLinkedDeque : public detail::MonitorDeque<T> {
public:
	ConcurrentLinkedDeque() : detail::MonitorDeque<T>(LockClass::named("ConcurrentLinkedDeque")) {}
	explicit ConcurrentLinkedDeque(const LockClass& lockClass) : detail::MonitorDeque<T>(lockClass) {}
};

} // namespace aion::gameserver::runtime
