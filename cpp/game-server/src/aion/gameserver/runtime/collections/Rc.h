#pragma once

#include <utility>

#include "aion/gameserver/runtime/collections/ArrayDeque.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/ConcurrentLinkedQueue.h"
#include "aion/gameserver/runtime/collections/CopyOnWriteArrayList.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/HashSet.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::runtime {

/**
 * RefCounted variant of a collection shim (design §3.3 "Every shim has a RefCounted variant"): a collection that is itself a heap object, so it
 * can be a map value (`ConcurrentHashMap<int32_t, Ref<RcArrayList<Ref<Npc>>>>`), an Array element or the target of a non-final collection field
 * (`Field<Ref<RcArrayList<int32_t>>>`, design §3.2).
 *
 * `SYNCHRONIZED(*list)` and monitor() use the collection's own Monitor (the one guarding its operations), not a second object monitor.
 * Create with `RcArrayList<Ref<Npc>>::create()` or `create(AION_LOCK_CLASS(Owner::field))`.
 */
template <class Collection>
class Rc final : public RefCounted, public Collection {
	AION_MAKE_REF_FRIEND

public:
	template <class... A>
	static Ref<Rc> create(A&&... args) {
		return makeRef<Rc>(std::forward<A>(args)...);
	}

	Monitor& monitor() const noexcept { return Collection::monitor(); }

protected:
	template <class... A>
	explicit Rc(A&&... args) : Collection(std::forward<A>(args)...) {}
	~Rc() override = default;
};

template <class T>
using RcArrayList = Rc<ArrayList<T>>;
template <class T>
using RcLinkedList = Rc<LinkedList<T>>;
template <class T>
using RcArrayDeque = Rc<ArrayDeque<T>>;
template <class K, class V>
using RcHashMap = Rc<HashMap<K, V>>;
template <class K, class V>
using RcLinkedHashMap = Rc<LinkedHashMap<K, V>>;
template <class K, class V>
using RcTreeMap = Rc<TreeMap<K, V>>;
template <class T>
using RcHashSet = Rc<HashSet<T>>;
template <class T>
using RcLinkedHashSet = Rc<LinkedHashSet<T>>;
template <class T>
using RcTreeSet = Rc<TreeSet<T>>;
template <class K, class V>
using RcConcurrentHashMap = Rc<ConcurrentHashMap<K, V>>;
template <class T>
using RcConcurrentKeySet = Rc<ConcurrentKeySet<T>>;
template <class T>
using RcCopyOnWriteArrayList = Rc<CopyOnWriteArrayList<T>>;
template <class T>
using RcConcurrentLinkedQueue = Rc<ConcurrentLinkedQueue<T>>;

} // namespace aion::gameserver::runtime
