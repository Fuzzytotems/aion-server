#pragma once

// Umbrella header of the collection shims (design §3.3). Java collection FIELDS of shared classes use these; locals stay std::.
//
// | Java                                      | C++ shim                              | Synchronization                                 |
// |-------------------------------------------|---------------------------------------|-------------------------------------------------|
// | ArrayList, Collections.synchronizedList   | ArrayList<T>                          | collection Monitor, snapshot iteration          |
// | LinkedList                                | LinkedList<T>                         | collection Monitor, snapshot iteration          |
// | ArrayDeque                                | ArrayDeque<T>                         | collection Monitor, snapshot iteration          |
// | PriorityQueue                             | PriorityQueue<T>                      | collection Monitor, snapshot iteration          |
// | HashMap / LinkedHashMap / EnumMap         | HashMap / LinkedHashMap / EnumMap     | collection Monitor, snapshot views              |
// | TreeMap, Collections.synchronizedSortedMap| TreeMap<K, V>                         | collection Monitor, snapshot views              |
// | HashSet / LinkedHashSet / TreeSet         | HashSet / LinkedHashSet / TreeSet     | collection Monitor, snapshot iteration          |
// | ConcurrentHashMap                         | ConcurrentHashMap<K, V>               | 16 stripe Monitors, lock-free EBR reads/views   |
// | ConcurrentHashMap.newKeySet()             | ConcurrentKeySet<K>                   | as ConcurrentHashMap                            |
// | CopyOnWriteArrayList / CopyOnWriteArraySet| CopyOnWriteArrayList / ...ArraySet    | writer Monitor, lock-free EBR arrays            |
// | ConcurrentLinkedQueue / ...Deque          | ConcurrentLinkedQueue / ...Deque      | queue Monitor (see ConcurrentLinkedQueue.h)     |
// | (collection as a heap object)             | Rc<Shim> (RcArrayList<T>, ...)        | the shim's own Monitor                          |
//
// Common rules: Java equals/hashCode/compareTo for elements whose class declares them (JavaEquals.h), identity otherwise; JavaIterator::remove
// by identity; no ConcurrentModificationException (deviation 16); user callbacks (compute, removeIf, comparators) run under the collection's
// or stripe's reentrant Monitor; a write of the key whose compute callback is running throws IllegalStateException("Recursive update").
// Elements that are game objects are stored as Ref<X> and handed out as Ptr<X> (read barrier: TaskScope::ensurePublished()).
//
// Not provided (unused or replaced by the design): ConcurrentSkipListMap/Set, blocking queues, WeakHashMap (InstanceScaler: id-keyed map,
// design §5.1 "Weak refs"), EnumSet fields (locals only; std::bitset).

#include "aion/gameserver/runtime/collections/ArrayDeque.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/ConcurrentLinkedQueue.h"
#include "aion/gameserver/runtime/collections/CopyOnWriteArrayList.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/HashSet.h"
#include "aion/gameserver/runtime/collections/Iterators.h"
#include "aion/gameserver/runtime/collections/JavaEquals.h"
#include "aion/gameserver/runtime/collections/Rc.h"
