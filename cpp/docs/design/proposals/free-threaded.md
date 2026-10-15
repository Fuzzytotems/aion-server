# Proposal: Faithful Free-Threading: a GC-shaped runtime for the C++ Aion game server (Ref + task-scoped borrows + Java-shaped pools)

> One of three competing proposals scored by the design panel (not the chosen design; see ../runtime-architecture.md).

Keep the Java server's threads, pools, monitors and lifetimes, and replace only the JVM guarantees that C++ lacks. Stored references are intrusive, atomically counted Ref<T>. Objects whose count reaches zero are not freed at once: an epoch-based Reclaimer (the C++ Cleaner) frees them only after every thread that could have borrowed a pointer has finished its task. This rule, "anything you reached during a task stays valid until the task ends", is the JVM stack-root guarantee. It makes raw locals, parameters and getters safe without refcount traffic and lets Ref<T>(ptr) resurrect an object just as storing a Java reference does. Object IDs are released in the destructor on the Reclaimer thread, after a short quarantine, so the Cleaner timing and ABA safety carry over. Memory-level data races are removed mechanically: a Java `final` field becomes `const`, a non-final field becomes Field<T> (atomic scalar, atomic Ref, or atomic pointer to an immutable box), Java collections in shared objects become concurrent wrappers with snapshot iteration, and `synchronized` becomes a compact reentrant Monitor. Every added lock is a leaf lock, so the deadlock graph is exactly Java's. Null dereferences through Ref/Ptr/Field throw NullPointerException and are logged at the task boundary, as Java does. The one intentional structural deviation is eager packet serialization: packets are serialized on the sending thread, so the IO strands only frame, encrypt and write bytes. Under these rules ~850 schedule sites and ~1,600 handler files keep their structure line for line. Each site gets a small, lintable edit (pin `this`, capture Refs by value, wrap fields), and the Java code stays checkable as the behavioural reference.

# Faithful Free-Threading: runtime architecture for the C++ game server

Status: design proposal for phase 4 (foundation). All Java line references were checked against `D:\aion-server\game-server`. Numbers marked *est.* are estimates, not measurements.

---

## 0. Position: why fidelity beats re-architecting

The research maps considered other options: a single game-logic executor, strands per map instance, or ID handles. They are technically viable (critic.md shows only 2 blocking sites and no interrupt use). I recommend against all of them for this port, for five reasons.

1. **The reference implementation stays checkable.** DEVIATIONS.md says any difference from Java not listed there is a bug. A single executor or per-instance strands changes the execution model under *every* one of the ~850 schedule sites, ~2,400 `sendPacket` calls, the 254 `synchronized` blocks and every DAO call. "Is this C++ line equivalent to that Java line?" then stops being a local question. With fidelity it stays local: same thread kinds, same monitors, same lifetimes.
2. **Blocking I/O stays where Java put it.** Enter-world runs ~30 sequential DAO calls, logout 17 DAOs plus extras, and there are periodic saves, broker saves and `LegionService` DAO calls (255 DAO calls in services, 33 inside model mutators). On one executor these stall the world. Moving them to a DB worker means restructuring model mutators. Java runs them inline on packet or scheduler threads, and so will we.
3. **Refcounting plus task-scoped borrows reproduces GC semantics; handles approximate them.** In Java the scheduler queue is a GC root, so a pending task keeps its captured Npc alive even after `delete()`. Handlers check `isDead()`/`isSpawned()` on that stale-but-readable object (222 `isDead()` and 22 `isSpawned()` checks in scheduling files; 342 idempotent `delete()` calls in handlers). Effects keep a deleted effector readable (NpcController.java:293-294). Teams keep a logged-out Player for 600 s. Handles or weak pointers turn each of these into a null check that Java never wrote. `Ref<T>` keeps them exactly as they are.
4. **Cross-instance logic does not decompose.** Teleport, teams, siege, chat, world raids and legions span map instances. Strands per instance would need a new cross-strand protocol in exactly the code that is hardest to verify.
5. **The target does not need the performance.** A local server with a few players has no throughput problem. The dominant risk is porting bugs, and the least risky port is the most literal one. The ported commons already mirrors Java threading (PacketProcessor with per-connection seriality, NioServer, ConfigValue for atomic reloads), so this design continues that direction.

The price is that UB freedom depends on wrapper types and porting rules rather than on "only one thread touches state". §7 and §10 are candid about that.

---

## 1. Threading model

### 1.1 Threads

| Thread(s) | Count (default) | Java counterpart | Runs |
|---|---|---|---|
| `main` | 1 | main | `GameServer::main` in Java order (GameServer.java:92-191), then waits for the shutdown coordinator and runs the ShutdownHook sequence |
| Asio IO (`NioServer`) | `gameserver.network.nio.threads` (default 1, >1 allowed) | NIO dispatcher | accept; read; decrypt; client factory and `readImpl` (pure, see critic.md); frame, encrypt and write **pre-serialized** bytes; SM_KEY |
| `PacketProcessor:n` | 4 (config min/max) | same | `CM_*::runImpl`, serial per connection (commons, already ported) |
| `ScheduledPool:n` | max(4, cores) | `ScheduledThreadPoolExecutor` | all `schedule`/`scheduleAtFixedRate` bodies: AI timers, effects, respawn/decay, periodic managers, regen, saves, `leaveWorldDelayed` |
| `InstantPool:n` | max(4, cores), queue 100,000 | instantPool + AionRejectedExecutionHandler | `execute()`: LS/CS packet `runImpl` (serialized per link, §1.3), short cron jobs, MapRegion activation, `onDisconnect`, FixPath |
| `LongRunning:n` | 0..∞, 60 s idle | cached pool | `executeLongRunning`/`submitLongRunning`: long cron jobs, AbstractCronTask start, custom-instance model training, startup loads |
| `Cron` | 1 | Quartz (threadCount=1) | computes fire times in `GSConfig::TIME_ZONE_ID` and hands jobs to the RunnableRunner (instant, long, or current thread) |
| `ForkJoin:n` | cores − 1 | `ForkJoinPool.commonPool` | `MoveTaskManager` `parallelForEach`; parallel startup phases (engine init, `World` maps, `spawnAll` per map, geo BIH build) |
| `Reclaimer` | 1 | `java.lang.ref.Cleaner` + GC | advances epochs, destroys unreferenced objects, releases auto-release IDs (§3) |
| `Watchdog` | 1 | `DeadLockDetector` | monitor wait-graph cycle detection; after 1 min: dump + exit RESTART (like Java); slow task report |
| logging/Discord | commons | logback | already ported |
| OS console handler | OS | JVM shutdown hook | posts to the shutdown coordinator (already in the login server) |

There are **no** dedicated AI, geo or DB threads, as in Java:
- **AI** events are dispatched synchronously on the calling thread (packet, scheduled, ForkJoin). AI timers run on ScheduledPool.
- **DB**: DAOs run inline on the calling thread against the commons pool (5 connections, 5 s timeout, as in Java).
- **Geo** queries run inline on the calling thread against immutable data. BIH trees are built eagerly at startup (a documented fix of Java's lazy-build race). Per-instance door, placeable and town state lives in `std::atomic<uint64_t>` bitset chunks.
- **Admin commands** (chat, console and player commands) run in `CM_CHAT` on the PacketProcessor, as in Java. `//reload` publishes new holders from there (§6).

### 1.2 Handing work between threads

There is no new message passing. Game objects are shared memory, as in Java, and work is handed over by capturing `Ref`s in closures:

- client packet → `PacketProcessor` queue (commons);
- anything → `ThreadPoolManager::schedule/execute/submit` → pool queue;
- cron → Cron thread → RunnableRunner → instant/long pool;
- game thread → `AionConnection::sendPacket` → per-connection byte queue → IO strand (§5);
- IO strand → `onDisconnect` → instant pool (commons `connect(DisconnectExecutor)` adapted to `ThreadPoolManager::execute`).

Every entry point that runs game code is a **task boundary** for the Reclaimer (§2.3). Boundaries are set by the pool worker loop, the PacketProcessor `Executor` decorator (commons already accepts one, so no commons change is needed), `parallelForEach`, and the startup phases on `main`.

### 1.3 Small, documented threading deviations

- **LS/CS links:** Java runs their packets unordered on the instant pool (LoginServerConnection.java:80). C++ runs them on the same instant pool but through a per-link `SerialExecutor`, so they stay ordered. The login server set this precedent in DEVIATIONS.md:137.
- **IO threads:** `nio.threads > 1` is allowed. It is safe because serialization is eager (§5). The two `readImpl` log reads (CM_BUY_ITEM.java:48, CM_ATREIAN_PASSPORT.java:33) move to `runImpl`.
- **Debug mode `gameserver.debug.single_executor=true`:** PacketProcessor, ScheduledPool, InstantPool and Cron all run on one thread, and `parallelForEach` is sequential. This is a reproduction aid, not the production model. FixPath then blocks that thread for up to 5 s per step, which is acceptable in debug.

### 1.4 The two blocking sites stay blocking

- **CM_TELEPORT_ANIMATION_DONE.java:36-41.** `task->run()` claims a not-yet-started task (PENDING→RUNNING compare-and-swap) and runs it inline. `task->get()` returns at once or rethrows the task's exception. Two cases are supported. (a) The unscheduled `Future::deferred(SpawnTask)` from TeleportService.java:191. (b) A real scheduled future stored under `TaskId::TELEPORT` (PvPZone.java:45, PlayerReviveService.java:251). If a ScheduledPool thread is running that task at the same moment, `run()` returns and `get()` waits on `std::atomic::wait`, exactly as `FutureTask.get()` does. There is no deadlock: the runner never waits for the packet thread.
- **FixPath.java:85,143-152.** It still runs on an instant pool thread. `waitTask->get(5, SECONDS)` blocks that one thread, as in Java. The task's own `cancel(true)` wakes `get()` with `CancellationException` (the state change notifies waiters), which is what FixPath relies on. The world keeps running because the pools are Java's.

---

## 2. Object ownership and references

### 2.1 Reference kinds

| Kind | Type | Storable? | Cost | Use |
|---|---|---|---|---|
| Owning | `Ref<T>` | yes | atomic inc/dec | fields, containers, lambda captures, packet fields |
| Borrowed | `Ptr<T>` / `T&` | **no** (valid until the current task ends) | none | parameters, locals, getter results |
| Java non-final field | `Field<T>` | n/a | lock-free atomic | every non-final field of a shared object |
| Part back-pointer | `T& owner` | n/a | none | controller/AI/KnownList/EffectController/AggroList/stats → owner |
| Template | `const T*` | yes | none | immortal static data (§6) |
| Connection | `std::shared_ptr<AionConnection>` | yes | shared_ptr | commons convention; `Player.clientConnection` is `AtomicSharedPtr` |
| Weak | *none in the foundation* | | | Java has 2 sites (DropNpc.java:28, InstanceScaler.java:25). They become ID lookup and explicit removal in `destroyInstance` |

`Ref`, `Ptr` and `Field<Ref>` **throw `NullPointerException` on null dereference** (an `[[unlikely]]` branch). That is Java's observable behaviour: an NPE in a task is logged by RunnableWrapper, and periodic tasks continue. It also makes the unported Java TOCTOU patterns safe (`if (endTask != null) endTask.cancel(false)` reads the field twice).

### 2.2 Core API sketch

```cpp
namespace aion::gameserver::lifetime {

/** Base of every shared heap object: AionObject, Effect, Skill, Future, AggroInfo, KnownObject, SpawnTemplate, InstanceHandler, teams ...
 *  The count is atomic. Reaching zero retires the object; the Reclaimer destroys it after a grace period (Java: GC + Cleaner). */
class RefCounted {
public:
	void retain() const noexcept;  // 0 -> 1 is legal: it resurrects a retired, not yet reclaimed object (like storing a Java ref)
	void release() const noexcept; // 1 -> 0 calls Reclaimer::retire(this); never destroys inline
	uint32_t refCount() const noexcept;
protected:
	RefCounted() noexcept = default;
	virtual ~RefCounted() = default; // runs on the Reclaimer thread; may only release Refs and take leaf locks
private:
	friend class Reclaimer;
	mutable std::atomic<uint32_t> count{0};
	mutable std::atomic<uint64_t> retireEpoch{0};
	mutable std::atomic<bool> queued{false};
};

/** Parts forward counting to their owner, so Ref<AbstractAI>(this) pins the Npc (Java: capturing AI 'this' keeps the Npc reachable). */
class OwnedPart {
public:
	void retain() const noexcept { owner_.retain(); }
	void release() const noexcept { owner_.release(); }
protected:
	explicit OwnedPart(const RefCounted& owner) noexcept : owner_(owner) {}
private:
	const RefCounted& owner_;
};

template <class T> class Ptr { // borrowed; debug builds stamp (thread, taskSerial) and assert on deref from another task
public:
	Ptr() noexcept = default;
	Ptr(std::nullptr_t) noexcept {}
	Ptr(T* p) noexcept;
	Ptr(T& r) noexcept;
	T* operator->() const; // NullPointerException
	T& operator*() const;
	T* get() const noexcept;
	explicit operator bool() const noexcept;
	friend bool operator==(Ptr, Ptr) = default; // identity, like Java ==
};

template <class T> class Ref {
public:
	Ref() noexcept = default;
	Ref(std::nullptr_t) noexcept {}
	Ref(T* p) noexcept;    // retain (resurrection-safe)
	Ref(Ptr<T> p) noexcept;
	template <std::derived_from<T> U> Ref(Ref<U> other) noexcept;
	T* operator->() const; // NullPointerException
	T& operator*() const;
	T* get() const noexcept;
	operator Ptr<T>() const noexcept;
	explicit operator bool() const noexcept;
	friend bool operator==(const Ref&, const Ref&) = default; // identity (Java ==); equality is equals()
};

template <class T, class... A> Ref<T> makeRef(A&&... args); // two-phase init hooks are called by the factories (VisibleObjectSpawner)
template <class To, class From> Ptr<To> cast(Ptr<From> p);  // Java (To) cast: throws ClassCastException, null passes through

/** Java non-final field. Picks a representation by T:
 *   trivially copyable (int, float, bool, enum, optional<int>)  -> std::atomic<T> (relaxed): get()/set(), no torn values, lost updates as in Java
 *   Ref<U>                                                      -> atomic intrusive pointer: get() -> Ptr<U>, set/exchange/compareAndSet
 *   anything else (std::string, vectors replaced wholesale)     -> atomic pointer to an immutable box: get() -> const T& valid for the task */
template <class T> class Field;

class Monitor { // Java monitor: reentrant, 16 bytes (owner thread id + depth), atomic::wait; registers waits with the Watchdog
public:
	void lock();
	void unlock();
	bool try_lock();
	bool isHeldByCurrentThread() const noexcept;
};
#define SYNCHRONIZED(obj) if (std::unique_lock _aion_sync_lock{::aion::gameserver::lifetime::monitorOf(obj)}; true)

} // namespace aion::gameserver::lifetime
```

Java-named atomics (`AtomicBoolean::compareAndSet`, `AtomicInteger::incrementAndGet`, `AtomicReference<T>` = `Field<Ref<T>>` plus `compareAndSet`) are provided, so the 311 handler uses port unchanged. Concurrent containers never expose iterators:

```cpp
template <class K, class V> class ConcurrentHashMap { // Java CHM; striped mutex + unordered_map. V is usually Ref<U>.
public:
	Ptr<U> get(const K&) const;                  // (optional<V> for value types)
	Ref<U> put(K, V);
	Ref<U> putIfAbsent(K, V);
	Ref<U> remove(const K&);
	bool remove(const K&, const V& expected);    // identity for Refs (CreatureController.cancelTaskIfPresent)
	void compute(K, std::invocable<Ptr<U>> auto&& remap); // under the stripe lock, like CHM: remap must not touch the map
	std::vector<Ptr<U>> values() const;          // snapshot of borrowed pointers: no refcount traffic, valid for the task
	void forEach(std::invocable<K, U&> auto&& fn) const; // snapshot, callbacks run outside the lock (weakly consistent)
	bool containsKey(const K&) const;
	int32_t size() const;
	bool isEmpty() const;
	void clear();
};
// also: CopyOnWriteArrayList<T>, ConcurrentLinkedQueue<T>, ConcurrentHashSet<T>, SynchronizedList<T> (for Java ArrayList fields of shared objects)
```

### 2.3 Task-scoped borrows: the Reclaimer

This is the load-bearing idea. `release()` never frees memory. It stamps the object with the global epoch and queues it. Each registered thread publishes its epoch when its outermost task begins and publishes "idle" when that task ends (nested `run()`, `CurrentThreadRunnableRunner` and `runNow` do not end the outer task). About every 50 ms the Reclaimer advances the epoch and destroys every queued object with `count == 0 && retireEpoch < min(active thread epochs)`.

What follows:
- **Borrowed pointers are safe for the whole task.** Any `Ptr<T>`/`T&` obtained during a task stays valid until the task ends, even if another thread removes the last `Ref` meanwhile. This is the JVM stack-root guarantee, so getters return `Ptr<T>` (no inc/dec) and `values()` snapshots are cheap.
- **Resurrection is legal.** `Ref<T>(ptr)` on an object whose count just reached 0 simply increments it. Java code that stores a reference it just read (`adds.add((Npc) spawn(...))`, `boss = getNpc(id)`) ports unchanged. Proof sketch: an object is destroyed only if no active thread started before its last retire. Any thread that started later cannot reach it, because no `Ref` to it exists (count 0) and parts are only reachable through their owner.
- **Destructors never run under someone else's lock.** They run on the Reclaimer thread, so dropping a `Ref` inside `compute()`, a Monitor or a container lock is always safe.
- **Nested parallelism is covered.** `parallelForEach` helpers use pointers borrowed by the blocked submitting thread, whose active epoch protects everything retired after it started.

**Unregistered threads must not touch game objects.** These are the IO strands (they only see bytes), the Cron thread (it hands off), and logging. The debug stamp in `Ptr` catches pointers that escape their task (e.g. a raw capture) deterministically in tests.

### 2.4 Ownership table by use case

| Use case | Java (evidence) | C++ | Rule / cycle breaker |
|---|---|---|---|
| World registry | `World.allObjects` CHM (World.java:48), PlayerContainer, WorldMapInstance/MapRegion CHMs | `ConcurrentHashMap<int32_t, Ref<VisibleObject>>` | owning. `removeObject` keeps the identity check `worldObject.get() == &object` under `SYNCHRONIZED(object)` (World.java:99-106) |
| Parts | final controller, AI (Creature.java:67), EffectController, AggroList, stats, MoveController, Player parts | `std::unique_ptr` member + `T& owner`, retain/release forwarded | two-phase construction in the factories (the Npc ctor's virtual `setupStatContainers` and AI creation happen post-construction) |
| KnownList | CHM<Integer, KnownObject(final VisibleObject)>; `update()` synchronized, `add()` on the other list unsynchronized (KnownList.java:44-48, 188-203) | `ConcurrentHashMap<int32_t, Ref<KnownObject>>`, `KnownObject{const Ref<VisibleObject> object; AtomicBoolean visible;}`, `update()`/`clear()` under the list's Monitor, `add()` lock-free via `putIfAbsent` | two-way strong refs; broken by `World.despawn → clearKnownlist` (invariant: despawned ⇒ empty list, both sides) |
| Target | `VisibleObject target` (VisibleObject.java:41) | `Field<Ref<VisibleObject>>` | cleared in `notSee`; a stale target stays readable |
| AggroList | CHM<Integer, AggroInfo(final Creature attacker)> | `ConcurrentHashMap<int32_t, Ref<AggroInfo>>`; hate counters `AtomicInteger`/`Field<int>` | cleared on despawn/revive (CreatureController.java:554-556) |
| Effect effector/effected | final fields (Effect.java:39-40); DoT from a despawned npc keeps ticking | `const Ref<Creature> effector, effected` | effector deletion does not end effects (Java). effected↔Effect cycle broken by effect end / `removeAllEffects` (NpcController.onDespawn, logout) |
| Summon / master | `Summon.master`, `SummonedObject.creator` final; `Player.summon` cleared by release | `const Ref<Player>` / `const Ref<Creature>`; `Player.summon` is `Field<Ref<Summon>>` | broken by `SummonsService.release` (`setSummon(null)`) |
| Team members | CHM<Integer, TM>, `PlayerTeamMember.player` final (PlayerTeamMember.java:8), 600 s offline (PlayerGroupService.java:210-222) | `ConcurrentHashMap<int32_t, Ref<PlayerGroupMember>>`, member holds `const Ref<Player>` | the offline Player stays readable. Relogin: `PlayerConnectedEvent` swaps the member Ref, and the old Player is reclaimed once unreferenced |
| Handler object fields | `private Npc boss` (SacrificialSoulAI.java:18), `List<Npc> adds` (TwinProtectorAI.java:23), `List<Npc> traps` (LowerUdasTempleInstance.java:41) | `Field<Ref<Npc>>`, `CopyOnWriteArrayList<Ref<Npc>>` | keep Java semantics. **Audit the ~41 handler object fields for back-references** (e.g. an add's AI holding its spawner). Instance handlers are detached at destroy (§2.6) |
| Task captures | lambdas capturing `this`, Npc, Player, QuestEnv | pin overload `schedule(this, [this]{...})`; objects captured as `Ref` by value | the callable is destroyed when a one-shot finishes or any task is cancelled |
| Controller tasks | CHM<Integer, Future> (CreatureController.java:67) | `ConcurrentHashMap<int32_t, FuturePtr>` | Npc→tasks→lambda→Npc cycle broken by run/cancel; `onDelete → cancelAllTasks` |
| Server packets | 63 hold objects, lazy | `Ref`/`Ptr` fields, serialized synchronously in `sendPacket` | nothing queued holds game objects (§5) |
| Params/locals | references | `Ptr<T>` / `T&` | valid until the task ends |
| WorldPosition | `mapRegion → getParent()` never nulled (WorldPosition.java:25) | `const Ref<WorldMapInstance> instance` + `Field<MapRegion*> mapRegion` + `Field<float> x,y,z` + `std::atomic<bool> isSpawned` | a stale object keeps its destroyed instance readable (Java). instance→objects→position cycle broken by despawn |
| Spawn template | `final SpawnTemplate spawnTemplate` (VisibleObject.java:46) | `const Ref<SpawnTemplate>` | mutable and refcounted (§6) |
| Items | `ItemStorage` CHM<Integer, Item> (ItemStorage.java:18), `Storage.deletedItems` CLQ | `ConcurrentHashMap<int32_t, Ref<Item>>`, `ConcurrentLinkedQueue<Ref<Item>>` | items move between storages, mail and broker by Ref. ID release is DB-driven (§3) |
| Connection ↔ Player | `Player.clientConnection`, `AionConnection.activePlayer` (AtomicReference) | `AtomicSharedPtr<AionConnection>` / `Field<Ref<Player>>` | cycle broken in `leaveWorld` (PlayerLeaveWorldService.java:65,152), as in Java |
| Delayed ID-based tasks | GeneralUpdateTask(playerId), DecayTask(objectId), `Npc.creatorId` | unchanged (bare `int32_t` + lookup) | ABA as in Java, mitigated by ID quarantine (§3) |

### 2.5 Identity, equality, relogin

- `a.equals(b)` (Java objectId equality, AionObject.java:52-63, dummy id 0 never equal) → `a->equals(*b)`. `hashCode` → `ObjectIdHash`. The rare object-keyed containers (Map/Set<Player> ×3, <Creature> ×2, <VisibleObject> ×1, <Item> ×5) use `ObjectIdHash`/`ObjectIdEqual` over `Ref`.
- Java `==` between objects (~14 sites, e.g. AttackUtil.java:510, TargetRangeProperty.java:50, World.removeObject, `RespawnTask.spawnTemplate == spawnTemplate`) → `Ref`/`Ptr` `operator==`, i.e. pointer identity. Semantics are identical, and the port is a literal copy.
- **Relogin:** `PlayerService.getPlayer` creates a new Player with the DB objectId. Old instances referenced by teams, effects or tasks stay alive and readable until those refs drop, and they compare `equals()` to the new one, exactly as in Java. Player IDs are never released by lifetime.

### 2.6 Cycles the GC would collect, and how they are handled

Refcounting leaks any cycle that becomes unreachable from roots. Pending tasks are roots in *both* runtimes, so task lifetimes match Java exactly. The remaining cases:

1. **Cycles broken by lifecycle hooks** (KnownList, target, AggroList, Effect↔effected, summon, controller tasks, team↔player). These are documented invariants, with debug assertions after `World::removeObject`: known list empty, aggro empty, tasks empty, own effects ended.
2. **Instance handler cycles** (handler fields → Npc → position → instance → handler). `InstanceService::destroyInstance` calls `onInstanceDestroy()` as in Java, then detaches the handler: `instance.handler` is set to a shared no-op handler. The detached handler lives as long as tasks pin it. *Deviation:* `getInstanceHandler()` on a destroyed instance returns the no-op handler.
3. **Handler↔handler back-references and aborted despawns** ("did not leave world cleanly", World.java:109). A **zombie registry** logs every AionObject that was removed from World and is still alive after 10 minutes, with class, name, id and refcount. A per-port audit checklist covers the ~41 handler object fields.

---

## 3. ID lifecycle

| Group | Objects (evidence) | Release point in C++ | Notes |
|---|---|---|---|
| (a) Cleaner | Npc (Npc.java:57), Gatherable (:17), StaticObject (:15), Summon (:41), HouseObject (flag), GeneralTeam/TemporaryPlayerTeam (flag), new PlayerGroups | `~AionObject()` on the Reclaimer thread after the grace period: `if (!RespawnService::setAutoReleaseId(id)) IDFactory::releaseId(id)` (AionObject.java:31-34). The respawn deferral hook (RespawnService.java:99-104, `setReleaseIdOnCompletion`) is kept verbatim | an ID is never reused while any Ref *or borrowed pointer* to the object exists |
| (b) Explicit, DB/service driven | Items: `InventoryDAO.store` after a successful delete (InventoryDAO.java:240-241); PlayerRegisteredItemsDAO:164,167; ExchangeService:274,301; ItemSplitService:90; PetAdoptionService:95; HTMLService:144; CM_CREATE_CHARACTER:68; CMT_CHARACTER_INFORMATION:153 | the same call sites, independent of memory lifetime | a `Ref<Item>` may outlive its ID (RepurchaseService.java:20,58), as in Java. Memory-safe, logically Java's quirk |
| (c) Never | FlyRing, Road, CuringObject, AssembledNpcPart, HTML message ids, `CustomInstanceService` static id | never | `autoRelease=false` |
| (d) DB-owned | Players, Legions, Mail, Houses, Guides, Pets | never by lifetime; `IDFactory` locks them at startup from 8 DAOs | |

**ABA safety.**
- Any holder of a `Ref`/`Ptr` sees a unique ID among live auto-release objects, because release happens only at destruction.
- Bare-int holders (pendingRespawns, DecayTask, `Npc.creatorId`, `WorldMapInstance.registeredObjects`, DropRegistrationService) can alias exactly as in Java. RespawnService.java:79-88 already logs the case.
- **Quarantine (documented deviation).** Java releases IDs only when the GC happens to collect a tenured Npc: minutes to hours in practice, which almost never exposes those int holders. C++ would release within ~100 ms and reuse the lowest ID immediately (IDFactory.java:178-185), so a stale `DecayTask(objectId)` could delete a *new* object. `IDFactory::releaseId` therefore keeps released IDs in a FIFO quarantine (`gameserver.idfactory.release_delay`, default 300 s) before they return to the BitSet. The lowest-free-first policy still applies once an ID leaves quarantine.
- `World::removeObject` keeps its identity check, and `RespawnTask` keeps its `spawnTemplate` identity check.

---

## 4. Scheduler, Future and cron

### 4.1 API

```cpp
namespace aion::gameserver::utils {

using Runnable = std::move_only_function<void()>;
enum class TimeUnit { NANOSECONDS, MICROSECONDS, MILLISECONDS, SECONDS, MINUTES, HOURS, DAYS };

/** Java: ScheduledFutureTask / FutureTask behind ScheduledFuture<?>, Future<?>, RunnableFuture. State machine on std::atomic<uint8_t>:
 *  PENDING -> RUNNING -> {PENDING (periodic reschedule) | DONE | FAILED};  PENDING|RUNNING -> CANCELLED (cancel never interrupts). */
class Future final : public lifetime::RefCounted {
public:
	bool cancel(bool mayInterruptIfRunning = false) noexcept; // true if it was PENDING/RUNNING; isCancelled()/isDone() true immediately;
	                                                          // the callable is destroyed now if not running, else right after the run returns
	bool isCancelled() const noexcept;
	bool isDone() const noexcept;
	bool isPeriodic() const noexcept;
	int64_t getDelay(TimeUnit unit = TimeUnit::MILLISECONDS) const noexcept; // due - now (negative if overdue); INT64_MAX for deferred
	void run();                                                // Java RunnableFuture.run(): claim PENDING->RUNNING and run inline,
	                                                           // else return at once; a queued heap entry is then skipped
	void get();                                                // wait (atomic::wait); CancellationException / ExecutionException(cause)
	void get(int64_t timeout, TimeUnit unit);                  // + TimeoutException
	static lifetime::Ref<Future> deferred(Runnable r);        // Java: new FutureTask<>(runnable, null), never scheduled (TeleportService.java:191)
};
using FuturePtr = lifetime::Ref<Future>;

/** Anything with retain()/release(): RefCounted, OwnedPart (AI, controller), InstanceHandler. Pins the owner until the task is gone. */
class Pin {
public:
	template <class T> Pin(const T* object) noexcept;
};

class ThreadPoolManager final {
public:
	static ThreadPoolManager& getInstance();

	FuturePtr schedule(Runnable r, int64_t delay, TimeUnit unit = TimeUnit::MILLISECONDS, std::source_location = std::source_location::current());
	FuturePtr schedule(Pin pin, Runnable r, int64_t delay, TimeUnit unit = TimeUnit::MILLISECONDS, std::source_location = std::source_location::current());
	FuturePtr scheduleAtFixedRate(Runnable r, int64_t delay, int64_t period, std::source_location = std::source_location::current());
	FuturePtr scheduleAtFixedRate(Pin pin, Runnable r, int64_t delay, int64_t period, std::source_location = std::source_location::current());
	void execute(Runnable r, std::source_location = std::source_location::current());            // instant pool, 100k queue, rejection policy
	void executeLongRunning(Runnable r, std::source_location = std::source_location::current());
	FuturePtr submit(Runnable r, std::source_location = std::source_location::current());         // exceptions go into the Future
	FuturePtr submitLongRunning(Runnable r, std::source_location = std::source_location::current());
	void shutdown(); // drops delayed tasks (Java: setExecuteExistingDelayedTasksAfterShutdownPolicy(false)), awaits 5 s
	std::vector<std::string> getStats() const;
};
}
```

### 4.2 Semantics

**Pools and timing**
- **ScheduledPool:** one `std::mutex` + binary heap of `(due, seq, FuturePtr)`, N workers. The earliest-due worker waits on a condvar until its deadline.
- **Fixed rate:** `next = prevDue + period`. A slow run catches up by running again at once. A periodic task never overlaps itself.
- **Scale (est.):** 50k pending timers cost O(log n) ≈ 16 comparisons per operation, microseconds at most.

**Exceptions and slow tasks**
- **Exception policy:** `schedule`, `scheduleAtFixedRate` and `execute` wrap the callable in the commons `RunnableWrapper(r, ThreadConfig::MAXIMUM_RUNTIME_IN_MILLISEC_WITHOUT_WARNING, true)`. Exceptions are logged and **periodic tasks keep running**, as in ThreadPoolManager.java:51-65. This intentionally differs from the login server's ScheduledExecutor.
- **`submit*`:** uses `catch=false`. The exception is stored and rethrown by `get()`.
- **Slow-task warnings** name the `source_location` of the schedule call, e.g. "ai/instance/abyssal_splinter/YamenessPortalSummonedAI.cpp:23 ran 5210 ms". `RunnableStatsManager` is keyed by call site, which is more useful than Java's lambda class names.

**Cancellation and lifetime**
- **Self-cancel:** `cancel()` from inside the running callable sets CANCELLED. The worker destroys the callable only *after* it returns, so there is no use-after-free of the lambda.
- **Eager capture release** (deviation, benign): Java keeps cancelled tasks, with their captures, in the queue until their delay expires, because `setRemoveOnCancelPolicy` is never called. C++ drops the captures on cancel; the heap entry becomes an empty shell. This releases objects and IDs earlier, and the quarantine covers ID reuse.
- **`cancel(true)`** (154 sites) is accepted and behaves as `cancel(false)`. No game code checks interruption (critic.md).

**Instant pool**
- **Rejection policy** (CONVENTIONS.md "Unported Java features"): when the instant queue is full, log a warning, then run in a new thread if the caller's priority is above normal, otherwise in the calling thread.

### 4.3 CreatureController tasks, deferred futures and Future holders

- `ConcurrentHashMap<int32_t, FuturePtr> tasks`. `addTask` uses `compute` and cancels the old task inside the remapping function (CreatureController.java:400-410). `cancel()` only flips state and releases captures, which queues objects to the Reclaimer, so it is safe under the stripe lock.
- `Future<?>` fields (46 in src, 120 in handlers) → `Field<FuturePtr>`.
- `Future<?>[]` / `AtomicReference<Future>` holders (Effect.java:48, AbstractMaterialSkillActor.java:28, HarlequinLordReshkaAI.java:20, TheHexwayInstance.java:51, FixPath.java:145) → `Ref<AtomicReference<Future>>` captured by value.

### 4.4 Cron

```cpp
class CronExpression { // Quartz subset actually used: seconds, '?', names, lists, ranges, increments, optional year '*'; L/W/# rejected
public:
	CronExpression(std::string_view expression, const std::chrono::time_zone* zone);
	std::optional<std::chrono::sys_time<std::chrono::milliseconds>> getTimeAfter(std::chrono::sys_time<std::chrono::milliseconds> after) const;
	const std::string& getCronExpression() const;
};

class CronService {
public:
	class Job; // RefCounted: runnable, expression, std::type_index of the runnable type, next fire time
	static void initSingleton(std::unique_ptr<RunnableRunner> runner, const std::chrono::time_zone* zone); // ThreadPoolManagerRunnableRunner
	static CronService& getInstance();
	template <class R> lifetime::Ref<Job> schedule(R runnable, const CronExpression& expression, bool longRunning = false);
	bool cancel(const lifetime::Ref<Job>& job);
	std::vector<lifetime::Ref<Job>> findJobs(std::type_index runnableType, bool withSubTypes = false) const;
	std::map<lifetime::Ref<Job>, std::chrono::sys_time<std::chrono::milliseconds>, JobOrder> findNextFireTimes(std::type_index type) const; // SiegeService.java:323
	void shutdown();
};
```

- The Cron thread keeps a heap of next fire times, computed in `GSConfig::TIME_ZONE_ID` via `std::chrono::zoned_time`. DST transitions follow Quartz, and CronServiceTest vectors are ported.
- `AbstractCronTask` keeps its semaphore, `executeLongRunning` start and `ServerVariablesDAO`-based run-on-start check.
- Java `cancel(Runnable)` uses identity; C++ cancels by job handle, or by the runnable object's address when it is a `Ref`.

---

## 5. Server packets

### 5.1 Decision: eager serialization on the sending thread

**Why eager.** Java writes lazily on the single NIO thread (AionConnection.java:204-213) and forbids `nio.threads > 1` (GameServer.java:197). Our wrappers would make lazy writes memory-safe, but they would still:
- make packets extend object lifetimes;
- run `writeImpl` (with its service reads and 6 state mutations) on IO threads, which are not Reclaimer-registered;
- put serialization errors on a stack far from the sender.

Serializing inside `sendPacket` fixes all three. The `writeImpl` bodies port *identically*; only the moment they run changes. That is typically microseconds earlier, since local queues are almost always empty.

```cpp
class AionServerPacket : public commons::network::packet::BaseServerPacket {
public:
	/** Java writeImpl(AionConnection con): con is null when the packet is serialized once for many recipients. */
	virtual void writeImpl(AionConnection* con, commons::utils::ByteBuffer& buf) = 0;
	/** The 25 packets reading con in writeImpl, plus those mutating their own state (SM_GROUP_MEMBER_INFO/SM_ALLIANCE_MEMBER_INFO): per recipient. */
	virtual bool isSerializedPerRecipient() const noexcept { return false; }
	/** Plaintext body [opcode header + body], exactly the bytes Java writes between the length and encryption. */
	SerializedBody serialize(AionConnection* con); // thread-local 32,768-byte scratch buffer -> shared_ptr<const vector<uint8_t>>
};

struct OutgoingPacket {               // what AConnection<OutgoingPacket> queues
	std::shared_ptr<const std::vector<uint8_t>> body;
	bool enablesCryptAfterWrite = false; // SM_KEY
	int32_t opCode;                      // stats, logging
};

class SerializedPacket { // shared by all recipients of one broadcast
public:
	std::shared_ptr<const std::vector<uint8_t>> getOrSerialize(AionServerPacket& packet);
};

class PacketSendUtility {
public:
	static void sendPacket(Player& player, AionServerPacket&& packet); // Java: sendPacket(player, new SM_X(...)) -> sendPacket(player, SM_X(...))
	static void sendPacket(Player& player, AionServerPacket& packet);  // cached/static packets (SM_FRIEND_RESPONSE constants, ArtifactAI fields)
	static void broadcastPacket(VisibleObject& object, AionServerPacket&& packet);
	static void broadcastPacket(Player& player, AionServerPacket&& packet, bool toSelf);
	static void broadcastPacket(VisibleObject& object, AionServerPacket&& packet, std::predicate<Player&> auto&& filter);
	static void broadcastToWorld(AionServerPacket&& packet);
	static void broadcastMessage(Npc& npc, int32_t msgId, int32_t delay, auto&&... params); // scheduleOrRun with Pin(&npc)
};
```

- Packets are stack temporaries, with no heap allocation per packet object.
- A packet's object fields may be `Ptr`/`T&` while it is a temporary, and must be `Ref` if the packet is stored (SplitList parts, cached packets). The lint enforces `Ref` for packet members to stay simple.

### 5.2 Broadcasts and per-recipient packets

- **Serialize once:** for connection-independent packets, `broadcastPacket` builds one `SerializedPacket` on the stack. The first recipient serializes, and the rest share the `shared_ptr<const vector>`.
- **Per recipient:** `isSerializedPerRecipient()` packets are serialized once per recipient with `con` set. This covers the 25 listed in network.md (SM_PLAYER_INFO race per viewer, SM_MESSAGE staff view, SM_DIALOG_WINDOW …) plus SM_GROUP_MEMBER_INFO and SM_ALLIANCE_MEMBER_INFO.
- **Misclassified packets fail loudly.** A packet that dereferences `con` without the flag gets `con == nullptr` and throws NPE with its packet name on the first test.
- **`sendPacket` TOCTOU fixed.** `sendPacket(Player&)` loads `player.getClientConnection()` **once** (atomic shared_ptr). This removes the Java `isOnline()`→`getClientConnection()` race (PacketSendUtility.java:74-77) without changing behaviour.

### 5.3 The 6 state-mutating writeImpls

| Packet | Java effect | C++ |
|---|---|---|
| SM_ATTACK:89, SM_CASTSPELL_RESULT:181 | `Player.setLastCounterSkill` at write time | runs during serialization at send time; `Field<AttackStatus>`. Idempotent, so it runs once instead of once per recipient |
| SM_PET:219-240 | pet mood/gift cooldowns | at send time; fields are `Field`s |
| SM_PLAY_MOVIE:28 | `con.getActivePlayer().setCustomState(WATCHING_CUTSCENE)` | per recipient at send time |
| SM_GROUP/ALLIANCE_MEMBER_INFO | reassign own `event` | per-recipient serialization, sequential on the sender, which reproduces Java's sequential single-thread writes |
| SM_KEY | enables the connection crypt | **the one strand-serialized packet**: its body is written in `writeData` on the strand, then `enableKey()`, keeping the "first encrypt only enables" quirk (Crypt.java:69-77) |

All six are listed in DEVIATIONS.md as "write-time side effects happen at send time".

### 5.4 Connection write path and encryption

```cpp
bool AionConnection::writeData(ByteBuffer& data) { // IO strand, guard held (commons contract)
	if (sendMsgQueue.empty())
		return false;
	std::shared_ptr<OutgoingPacket> packet = std::move(sendMsgQueue.front());
	sendMsgQueue.pop_front();
	data.putShort(0);
	data.put(*packet->body);                        // opcode header + body, serialized by the sender
	data.flip();
	data.putShort(static_cast<int16_t>(data.limit()));
	ByteBuffer payload = data.slice(2);
	crypt.encrypt(payload);                         // rolling XOR key, only ever touched on this strand (no mutex needed)
	data.position(0);
	if (packet->enablesCryptAfterWrite)
		crypt.enableKey();
	return true;
}
```

- **Encryption stays per connection on the strand.** Decryption happens in `processData` on the same strand, so the crypt state needs no lock (unlike the login server's `CryptEngine`).
- **Diagnostics move to send time:** the `membership == 10` packet-name echo (AionConnection.java:227-230) and the 8,192-byte client limit warning.
- **Ordering:** order per connection is enqueue order. See §10 for the stale/fresh inversion this allows between two concurrent senders.

---

## 6. Static data at runtime

1. **Immortal const templates.** Holders are loaded by generated binders before the world starts. Nothing ever frees them. Live objects hold `const ItemTemplate*`, `const NpcTemplate*`, `const SkillTemplate*`, and so on, with no refcount.
2. **Publishing and `//reload`.** Each of the 91 `DataManager` fields becomes a `Reloadable<Holder>`:

   ```cpp
   template <class H> class Reloadable {
   public:
   	const H* operator->() const noexcept { return current.load(std::memory_order_acquire); } // Java: DataManager.ITEM_DATA.getItemTemplate(id)
   	void publish(std::unique_ptr<H> fresh); // atomic store; the old holder moves to an immortal 'retired' list (logged with its size)
   private:
   	std::atomic<const H*> current{nullptr};
   };
   // DataManager::ITEM_DATA->getItemTemplate(id)  -- same token shape as Java
   ```

   Reload leaks the old holder on purpose: items created before the reload keep pointing into it, as the GC would keep it. `NPC_SKILL_DATA.setNpcSkillTemplates`, `XML_QUESTS.setData` and `EVENT_DATA.setEvents` mutate in place, so their inner collections are `Field<std::vector<...>>` (boxed snapshots). QuestEngine's 27 event maps are one `Reloadable<QuestRegistry>`. AI, instance, zone and command factory tables are immutable generated arrays, so `//reload ai|commands` only rebuilds data-derived tables.
3. **Mutable template fields.** The generator emits `mutable Field<T>` for an explicit allow-list of fields that Java mutates at runtime: `HostileUpEffect.tempHate` (HostileUpEffect.java:32,61), `GuideTemplate.activated`, walker `RouteStep.z` (FixPath.java:121-125), event flags. Everything else is `const` after load.
4. **The spawn family is refcounted and mutable.**
   - `SpawnGroup`, `SpawnTemplate` (with Siege/Rift/Vortex/Base/Town/AhserionsFlight variants) and JAXB `Spawn` derive from `RefCounted`. Their setters use `Field<T>`, and `SpawnGroup.poolUsedTemplates` sits under its Monitor (SpawnGroup.java:163-189).
   - `SpawnsData` maps are `ConcurrentHashMap` of concurrent inner lists. Events (Event.java:88-149) can remove groups while live Npcs hold `const Ref<SpawnTemplate>`, which is safe.
   - The 47 handler setter calls on shared instance templates (AlarmAI.java:47, CaptainXastaAI.java:75,89 …) keep Java's cross-instance leakage (a quirk), but are now memory-safe.
   - `SpawnEngine` creates `makeRef<SpawnTemplate>(makeRef<SpawnGroup>(...))` per runtime spawn (SpawnEngine.java:86). The ~15 runtime-constructed template types get ordinary constructors and setters from the generator.
5. **Write-back paths** (`SpawnsData.saveSpawn` under `SYNCHRONIZED(*this)`, `WalkerData.saveData`) are hand-written pugixml writers.

---

## 7. Concurrency safety guarantees

### 7.1 Impossible by construction (given the porting rules)

| UB class | Mechanism |
|---|---|
| Data race on a scalar field | `Field<T>` = `std::atomic<T>`; compound Java ops (`hp -= x`) become load-modify-store: lost updates as in Java, never torn values or optimizer surprises |
| Data race on a reference field | `Field<Ref<T>>`: atomic pointer exchange plus EBR, so readers never see freed memory |
| Data race on strings/aggregates | `Field<T>` boxes are immutable; `get()` returns `const T&` valid for the task |
| Container corruption / iterator invalidation | concurrent wrappers, no public iterators, snapshot `forEach` (Java CHM weak consistency) |
| Use-after-free of game objects | `Ref` for stored references, EBR for borrowed ones, parts die with their owner, destructors only on the Reclaimer |
| Use-after-free of templates | immortal holders |
| Null dereference | `Ref`/`Ptr`/`Field<Ref>` throw `NullPointerException` (caught and logged like Java) |
| IO strand vs game state | eager serialization: IO threads touch only bytes and the crypt |
| Config reload | existing `ConfigValue<T>` / `std::atomic<T>` rule (CONVENTIONS.md) |
| Exceptions escaping threads/destructors | every entry point wrapped (RunnableWrapper / PacketProcessor Executor); destructors `noexcept` and release-only |
| Geo | immutable after the eager BIH build; per-instance state in atomic bitsets |
| Static init order / virtual calls in constructors | function-local singletons, Java startup order in `main`, two-phase factories |

### 7.2 What still needs locks (and has them)

- **Java `synchronized` → `Monitor`, 1:1** (254 blocks/methods, reentrant). Every `AionObject` carries a 16-byte Monitor so `synchronized(object)` works (World.java:99, HousingService). Other classes with synchronized members get their own.
- **Java explicit locks:** `ReentrantLock teamLock` → Monitor; `StampedLock` (EffectController) → `std::shared_mutex`, with the optimistic-read path mapped to shared locks.
- **Fields that Java only touches under a monitor** may stay plain as `Guarded<T, Monitor>`, which in debug builds asserts `monitor.isHeldByCurrentThread()` on access.

### 7.3 Deadlocks

The lock graph equals Java's plus **leaf locks only**: container stripes, the scheduler heap, IDFactory, Reclaimer queues. Leaf locks never call out while held; dropping a `Ref` under them is allowed because destruction is deferred. So C++ can deadlock exactly where Java could, and nowhere else. The Watchdog records Monitor waits and runs cycle detection. That is the equivalent of `ThreadMXBean.findDeadlockedThreads` for monitors, so `DeadLockDetector` becomes portable after all.

### 7.4 Explicitly not guaranteed (faithful to Java)

- Lost updates, check-then-act races, and cross-field inconsistency (e.g. x updated before y) are all Java's.
- Known Java race bugs are preserved unless DEVIATIONS.md lists a fix. Examples: `TwinProtectorAI.adds` touched from 3 threads, shared spawn template setters, `InventoryDAO.store` marking items UPDATED after a failed batch.

### 7.5 Residual UB surface and its controls

| Risk | Control |
|---|---|
| Raw `this`/pointer captured in a closure that outlives the task | pin overload; CI lint (`schedule(`/`execute(` with `[this`/`[&` and no Pin); debug `Ptr` task stamps; ASan (`/fsanitize=address`) in test runs |
| A raw std container or `T*` sneaks into a shared class | lint over headers in model/, controllers/, ai/, skillengine/, services/, handlers/: non-const `std::vector/map/string` or `T*`/`Ptr<` members need a `// confined:` or `Guarded<>` annotation |
| A thread touching objects without Reclaimer registration | only pools, PacketProcessor (via Executor), ForkJoin and `main` touch objects; `Ptr` debug stamp asserts on unregistered threads |
| Wrapper bugs (EBR, atomic Ref) | small, heavily tested core; **ThreadSanitizer runs on a Linux clang build** (MSVC has no TSan, which is where "portable code" pays off) |

---

## 8. Porting mechanics: side by side

### Porting rules at a glance

1. `final` object field → `const Ref<T>`; `final` scalar → `const T`; non-final field → `Field<T>`; `Atomic*` → same-named wrapper.
2. Java collection field in a shared object → concurrent wrapper; locals stay `std::`.
3. Object parameters, locals and return values → `Ptr<T>`/`T&`; anything stored → `Ref<T>`.
4. Closures handed to the scheduler: pin `this`, capture objects as `Ref` by value.
5. `synchronized (x)` → `SYNCHRONIZED(x)`.
6. `new SM_X(...)` → `SM_X(...)`.
7. `a.equals(b)` → `a->equals(*b)`; `a == b` → `a == b`.

### (a) AI handler: delayed action capturing `this`, `isDead()` later

Java, `data/handlers/ai/instance/abyssal_splinter/YamenessPortalSummonedAI.java:21-38`:
```java
@Override
protected void handleSpawned() {
	super.handleSpawned();
	ThreadPoolManager.getInstance().schedule(this::spawnSummons, 12000);
}

private void spawnSummons() {
	if (isDead() || !getOwner().isSpawned()) // ensure npc is still alive and instance is not destroyed yet
		return;
	spawn(281903, getOwner().getX() + 3, getOwner().getY() - 3, getOwner().getZ(), (byte) 0);
	spawn(281904, getOwner().getX() - 3, getOwner().getY() + 3, getOwner().getZ(), (byte) 0);
	ThreadPoolManager.getInstance().schedule(() -> {
		if (!isDead() && getOwner().isSpawned()) {
			spawn(281903, getOwner().getX() + 3, getOwner().getY() - 3, getOwner().getZ(), (byte) 0);
			spawn(281904, getOwner().getX() - 3, getOwner().getY() + 3, getOwner().getZ(), (byte) 0);
		}
	}, 60000);
}
```
C++:
```cpp
AI_HANDLER("yamenessportal", YamenessPortalSummonedAI) // generated registration table, see handlers-registration.md

class YamenessPortalSummonedAI : public AggressiveNpcAI {
public:
	using AggressiveNpcAI::AggressiveNpcAI;
protected:
	void handleSpawned() override {
		AggressiveNpcAI::handleSpawned();
		ThreadPoolManager::getInstance().schedule(this, [this] { spawnSummons(); }, 12000); // Pin(this) keeps the Npc (and this AI) alive
	}
private:
	void spawnSummons() {
		if (isDead() || !getOwner().isSpawned()) // a deleted Npc is still readable here, exactly like Java
			return;
		spawn(281903, getOwner().getX() + 3, getOwner().getY() - 3, getOwner().getZ(), 0);
		spawn(281904, getOwner().getX() - 3, getOwner().getY() + 3, getOwner().getZ(), 0);
		ThreadPoolManager::getInstance().schedule(this, [this] {
			if (!isDead() && getOwner().isSpawned()) {
				spawn(281903, getOwner().getX() + 3, getOwner().getY() - 3, getOwner().getZ(), 0);
				spawn(281904, getOwner().getX() - 3, getOwner().getY() + 3, getOwner().getZ(), 0);
			}
		}, 60000);
	}
};
```
Per-site change: `this::m` → `this, [this] { m(); }`. If the Npc is deleted before 12 s, the pinned task still runs, sees `isSpawned() == false` and returns. It then drops the pin, and the Reclaimer destroys the Npc and releases its ID (after quarantine).

### (b) Handler keeping `Npc boss`, and an instance handler spawning adds

Instance handlers have no direct `Npc` fields; they use local `getNpc()` plus collection fields. So both real shapes are shown. Java, `ai/instance/beshmundirTemple/SacrificialSoulAI.java:18-51`:
```java
private Npc boss;

@Override
protected void handleSpawned() {
	super.handleSpawned();
	AIActions.useSkill(this, 18901);
	this.setStateIfNot(AIState.FOLLOWING);
	boss = getPosition().getWorldMapInstance().getNpc(216263);
	if (boss != null && !boss.isDead()) {
		AIActions.targetCreature(this, boss);
		getMoveController().moveToTargetObject();
	}
}

@Override
protected void handleMoveArrived() {           // runs later, on a ForkJoin thread (MoveTaskManager)
	if (boss != null && !boss.isDead()) {
		SkillEngine.getInstance().getSkill(getOwner(), 18960, 55, boss).useNoAnimationSkill();
		AIActions.deleteOwner(this);
	}
}
```
C++:
```cpp
class SacrificialSoulAI : public GeneralNpcAI {
	Field<Ref<Npc>> boss; // written on one thread, read on another: atomic ref; keeps a dead/deleted boss readable
protected:
	void handleSpawned() override {
		GeneralNpcAI::handleSpawned();
		AIActions::useSkill(*this, 18901);
		setStateIfNot(AIState::FOLLOWING);
		boss = getPosition().getWorldMapInstance().getNpc(216263); // Ptr<Npc> -> stored as Ref (resurrection-safe)
		if (boss && !boss->isDead()) {
			AIActions::targetCreature(*this, *boss);
			getMoveController().moveToTargetObject();
		}
	}
	void handleMoveArrived() override {
		if (boss && !boss->isDead()) { // a racing clear throws NPE instead of UB, same as Java
			SkillEngine::getInstance().getSkill(getOwner(), 18960, 55, *boss)->useNoAnimationSkill();
			AIActions::deleteOwner(*this);
		}
	}
};
```
Java, `instance/LowerUdasTempleInstance.java:41-72`:
```java
private List<Npc> traps = new ArrayList<>();
private AtomicBoolean wasSpawned = new AtomicBoolean();

@Override
public void onEnterInstance(Player player) {
	if (wasSpawned.compareAndSet(false, true)) {
		traps.add((Npc) spawn(216531, 744.7521f, 885.8238f, 152.7852f, (byte) 30)); // Zhanim The Librarian
		for (WorldPosition position : trap_positions)
			traps.add((Npc) spawn(216530, position.getX(), position.getY(), position.getZ(), (byte) 0)); // Ancient Trap
	}
}

@Override
public void handleUseItemFinish(Player player, Npc npc) {
	for (Npc trap : traps) {
		if (trap != null && trap.getNpcId() != 216531)
			trap.getController().delete();
	}
}
```
C++:
```cpp
INSTANCE_HANDLER(300160000, LowerUdasTempleInstance)

class LowerUdasTempleInstance : public GeneralInstanceHandler {
	CopyOnWriteArrayList<Ref<Npc>> traps; // Java ArrayList filled by one packet thread, iterated by another
	AtomicBoolean wasSpawned;
public:
	using GeneralInstanceHandler::GeneralInstanceHandler;
	void onEnterInstance(Player& player) override {
		if (wasSpawned.compareAndSet(false, true)) {
			traps.add(cast<Npc>(spawn(216531, 744.7521f, 885.8238f, 152.7852f, 30))); // Java cast -> ClassCastException on mismatch
			for (const WorldPosition& position : trap_positions)
				traps.add(cast<Npc>(spawn(216530, position.getX(), position.getY(), position.getZ(), 0)));
		}
	}
	void handleUseItemFinish(Player& player, Npc& npc) override {
		traps.forEach([](Npc& trap) {
			if (trap.getNpcId() != 216531)
				trap.getController().delete(); // idempotent; deleted traps stay readable
		});
	}
};
```
Traps hold `Ref<WorldMapInstance>` through their positions, and this handler holds the traps. At `destroyInstance` the handler is detached after `onInstanceDestroy()` (§2.6), so the instance is reclaimed once no task pins the handler.

### (c) Effect whose effector despawns mid-DoT

Java, Effect.java:39-40 (`private final Creature effector; private final Creature effected;`) and AbstractOverTimeEffect.java:50-56:
```java
long initialDelay = 300 + checktime;
Future<?> task = ThreadPoolManager.getInstance().scheduleAtFixedRate(() -> onPeriodicAction(effect), initialDelay, checktime);
effect.setPeriodicTask(task, position);
```
PoisonEffect.java:44-50:
```java
public void onPeriodicAction(Effect effect) {
	Creature effected = effect.getEffected();
	effected.getController().onAttack(effect, TYPE.DAMAGE, effect.getReserveds(position).getValue(), false, LOG.POISON, hopType,
		effect.isMagicalCritical(position));
	effected.getObserveController().notifyDotAttackedObservers(effect.getEffector(), effect);
}
```
C++:
```cpp
class Effect : public RefCounted, public StatOwner {
	const Ref<Creature> effector;       // final: a despawned/deleted Npc stays readable while the DoT ticks
	const Ref<Creature> effected;
	const SkillTemplate* skillTemplate; // immortal
	Field<FuturePtr> endTask;
	Field<std::vector<FuturePtr>> periodicTasks; // Java Future<?>[] replaced wholesale -> boxed snapshot
	Field<FuturePtr> periodicActionsTask;
	// ...
public:
	void stopTasks() {
		if (endTask) {
			endTask->cancel(false);
			endTask = nullptr;
		} // literal port; concurrent stopTasks -> NPE, not UB
		// ...
	}
};

void AbstractOverTimeEffect::startEffect(Effect& effect, std::optional<AbnormalState> abnormal) {
	// ...
	int64_t initialDelay = 300 + checktime;
	FuturePtr task = ThreadPoolManager::getInstance().scheduleAtFixedRate(
		[this, effect = Ref<Effect>(&effect)] { onPeriodicAction(*effect); }, // 'this' is an immortal template: no pin needed
		initialDelay, checktime);
	effect.setPeriodicTask(std::move(task), position);
}

void PoisonEffect::onPeriodicAction(Effect& effect) {
	Ptr<Creature> effected = effect.getEffected();
	effected->getController().onAttack(effect, TYPE::DAMAGE, effect.getReserveds(position)->getValue(), false, LOG::POISON, hopType,
		effect.isMagicalCritical(position));
	effected->getObserveController().notifyDotAttackedObservers(effect.getEffector(), effect);
}
```
Runtime story:
1. The effector Npc is deleted. `NpcController::onDespawn` ends only its *own* effects.
2. The poison keeps ticking. `NpcController::onAttack` sees `attacker.isSpawned() == false` and uses the acting creature (NpcController.java:293-297), as in Java.
3. When the effect ends, `stopTasks` cancels the periodic task, which drops its `Ref<Effect>`. The EffectController removes the effect, the Effect is reclaimed, and that drops `Ref<Creature> effector`.
4. The deleted Npc is reclaimed and its ID released: later than despawn, exactly as with the Java Cleaner.

### (d) Group keeping a logged-out player

Java: `PlayerTeamMember.java:8` (`final Player player;`), PlayerGroupService.java:210-222 (OfflinePlayerChecker every 30 s, removal after `GROUP_REMOVE_TIME`), PlayerConnectedEvent.handleEvent:
```java
group.removeMember(player.getObjectId());
group.addMember(new PlayerGroupMember(player));
if (player.equals(group.getLeader().getObject())) { ... }
```
C++:
```cpp
class PlayerTeamMember : public RefCounted, public TeamMember<Player> {
	const Ref<Player> player;          // logged-out Player stays alive and readable (name, level, position) for up to 600 s
	AtomicLong lastOnlineTime;
public:
	explicit PlayerTeamMember(Player& player) : player(&player) {}
	bool isOnline() const { return player->isOnline(); }
	// ...
};

void OfflinePlayerChecker::operator()() const {
	groups.forEach([](int32_t, PlayerGroup& group) {
		group.forEachTeamMember([&](PlayerGroupMember& member) {
			if (!member.isOnline() && TimeUtil::isExpired(member.getLastOnlineTime() + GroupConfig::GROUP_REMOVE_TIME * 1000))
				group.onEvent(PlayerGroupLeavedEvent(group, member.getObject(), LeaveReson::LEAVE_TIMEOUT));
		});
	});
}

void PlayerConnectedEvent::handleEvent() {                        // runs under teamLock (Monitor) via GeneralTeam::onEvent
	group.removeMember(player->getObjectId());                    // drops the Ref to the *old* Player instance (reclaimed later)
	group.addMember(makeRef<PlayerGroupMember>(*player));         // new instance, same objectId
	if (player->equals(*group.getLeader()->getObject())) { /* ... */ }
	// ...
}
```
The old Player's `playerGroup` still refs the group, but the group no longer refs the old Player, so there is no cycle. Relogin equality stays objectId-based.

### (e) `PacketSendUtility.broadcastPacket` of an SM_ packet

Java, `ai/ActionItemNpcAI.java:64` and PacketSendUtility.java:83-97:
```java
PacketSendUtility.broadcastPacket(player, new SM_EMOTION(player, EmotionType.START_QUESTLOOT, 0, getObjectId()), true);

public static void broadcastPacket(VisibleObject object, AionServerPacket packet) {
	object.getKnownList().forEachPlayer(player -> sendPacket(player, packet));
}
```
C++:
```cpp
PacketSendUtility::broadcastPacket(player, SM_EMOTION(player, EmotionType::START_QUESTLOOT, 0, getObjectId()), true);

void PacketSendUtility::broadcastPacket(Player& player, AionServerPacket&& packet, bool toSelf) {
	SerializedPacket once;                       // serialized at most once (unless per-recipient)
	if (toSelf)
		sendTo(player, packet, once);
	player.getKnownList().forEachPlayer([&](Player& other) { sendTo(other, packet, once); }); // snapshot iteration, callbacks outside the lock
}

void PacketSendUtility::sendTo(Player& player, AionServerPacket& packet, SerializedPacket& once) {
	std::shared_ptr<AionConnection> con = player.getClientConnection(); // one atomic load (fixes the Java TOCTOU)
	if (!con)
		return;                                                         // semi-offline player: silently skipped, as in Java
	auto body = packet.isSerializedPerRecipient() ? packet.serialize(con.get()) : once.getOrSerialize(packet);
	con->sendPacket(std::make_shared<OutgoingPacket>(std::move(body), false, packet.getOpCode()));
}
```
Porting change at call sites: drop `new`. `SM_EMOTION` stores `Ptr<Creature>` for its lifetime, which is safe because serialization finishes before `broadcastPacket` returns.

### (f) DAO save at logout and periodic save

Java, PlayerEnterWorldService.java:367-371 and :496-521:
```java
player.getController().addTask(TaskId.PLAYER_UPDATE, ThreadPoolManager.getInstance().scheduleAtFixedRate(
	new GeneralUpdateTask(player.getObjectId()), PeriodicSaveConfig.PLAYER_GENERAL * 1000, PeriodicSaveConfig.PLAYER_GENERAL * 1000));

public void run() {
	Player player = World.getInstance().getPlayer(playerId);
	if (player != null) {
		try {
			AbyssRankDAO.storeAbyssRank(player);
			PlayerSkillListDAO.storeSkills(player);
			PlayerQuestListDAO.store(player);
			PlayerDAO.storePlayer(player);
			for (House house : player.getHouses())
				house.save();
		} catch (Exception ex) {
			log.error("Exception during periodic saving of player " + player.getName(), ex);
		}
	}
}
```
C++:
```cpp
player.getController().addTask(TaskId::PLAYER_UPDATE, ThreadPoolManager::getInstance().scheduleAtFixedRate(
	GeneralUpdateTask(player.getObjectId()), PeriodicSaveConfig::PLAYER_GENERAL * 1000, PeriodicSaveConfig::PLAYER_GENERAL * 1000));

struct GeneralUpdateTask {                         // captures only the id, like Java: no Ref needed
	int32_t playerId;
	void operator()() const {
		Ptr<Player> player = World::getInstance().getPlayer(playerId);
		if (player) {
			try {
				AbyssRankDAO::storeAbyssRank(*player);
				PlayerSkillListDAO::storeSkills(*player);
				PlayerQuestListDAO::store(*player);
				PlayerDAO::storePlayer(*player);
				for (Ptr<House> house : player->getHouses())
					house->save();
			} catch (const std::exception& ex) {
				log.error("Exception during periodic saving of player " + player->getName(), ex);
			}
		}
	}
};
```
- **Inline DB, as in Java:** the save runs on a ScheduledPool thread with the commons JDBC-shaped API.
- **No UB during concurrent play:** DAOs read `Field`s and snapshot copies (`Storage::getItems()` already copies CHM values, ItemStorage.java:27-28). A packet thread may mutate the same Player meanwhile.
- **Logical races kept:** an item changed mid-save is marked UPDATED (InventoryDAO.java:236-238), a Java quirk.
- **Logout:** `PlayerLeaveWorldService::leaveWorld` (PlayerLeaveWorldService.java:55-155) ports line by line on the DESPAWN task thread or the CM_QUIT packet thread. `PlayerDAO::onlinePlayer(player, false)` remains the "fully saved, may re-enter" marker, serializing logout against relogin.

### (g) `CreatureController.addTask(TaskId, schedule(...))` and cancel on despawn

Java, PlayerLeaveWorldService.java:52-55 and CreatureController.java:400-427:
```java
Future<?> leaveWorldTask = ThreadPoolManager.getInstance().schedule(() -> leaveWorld(player), delayInMillis);
player.getController().addTask(TaskId.DESPAWN, leaveWorldTask);

public void addTask(TaskId taskId, Future<?> task) {
	tasks.compute(taskId.ordinal(), (k, oldTask) -> {
		if (oldTask != null) {
			oldTask.cancel(false);
			if (taskId == TaskId.DESPAWN) log.warn("Despawn task for " + getOwner() + " was cancelled and replaced ...");
		}
		return task;
	});
}
public void cancelAllTasks() {
	for (Entry<Integer, Future<?>> e : tasks.entrySet()) { Future<?> task = e.getValue(); if (task != null) task.cancel(false); }
	tasks.clear();
}
```
C++:
```cpp
void PlayerLeaveWorldService::leaveWorldDelayed(Player& player, int64_t delayInMillis) {
	FuturePtr leaveWorldTask = ThreadPoolManager::getInstance().schedule([player = Ref<Player>(&player)] { leaveWorld(*player); }, delayInMillis);
	player.getController().addTask(TaskId::DESPAWN, std::move(leaveWorldTask));
}

void CreatureController::addTask(TaskId taskId, FuturePtr task) {
	tasks.compute(std::to_underlying(taskId), [&](Ptr<Future> oldTask) -> FuturePtr {
		if (oldTask) {
			oldTask->cancel(false); // releases captured Refs; destruction is deferred, so safe under the stripe lock
			if (taskId == TaskId::DESPAWN)
				log.warn("Despawn task for {} was cancelled and replaced ...", getOwner());
		}
		return task;
	});
}

void CreatureController::cancelAllTasks() {
	tasks.forEach([](int32_t, Future& task) { task.cancel(false); });
	tasks.clear();
}
```
The Player→tasks→closure→Player cycle ends when the task runs (a one-shot callable is destroyed after the run) or when `onDelete → cancelAllTasks` cancels it. The teleport variant is `addTask(TaskId::TELEPORT, Future::deferred(SpawnTask(...)))`, and CM_TELEPORT_ANIMATION_DONE ports line by line with `task->run(); task->get();`.

### (h) MoveTaskManager periodic NPC movement

Java, MoveTaskManager.java:39-55:
```java
public void run() {
	movingCreatures.values().parallelStream().forEach(creature -> {
		if (!creature.isSpawned()) {
			if (removeCreature(creature))
				LoggerFactory.getLogger(MoveTaskManager.class).warn(creature + " was still in moving creatures list but already despawned");
			return;
		}
		creature.getMoveController().moveToDestination();
		if (creature.getAi().isDestinationReached()) {
			removeCreature(creature);
			creature.getAi().onGeneralEvent(AIEventType.MOVE_ARRIVED);
			ZoneUpdateService.getInstance().add(creature);
		} else {
			creature.getAi().onGeneralEvent(AIEventType.MOVE_VALIDATE);
		}
	});
}
```
C++:
```cpp
class MoveTaskManager final : public AbstractPeriodicTaskManager { // schedules itself at a fixed rate of 200 ms on first getInstance()
	ConcurrentHashMap<int32_t, Ref<Creature>> movingCreatures;
public:
	void run() override {
		std::vector<Ptr<Creature>> creatures = movingCreatures.values(); // borrowed snapshot: no refcount traffic
		ForkJoinPool::commonPool().parallelForEach(creatures, [this](Ptr<Creature> creature) {
			// helpers are covered by this (blocked) task's epoch; exceptions are logged per element like CollectionUtil
			if (!creature->isSpawned()) {
				if (removeCreature(*creature))
					log.warn("{} was still in moving creatures list but already despawned", *creature);
				return;
			}
			creature->getMoveController().moveToDestination();
			if (creature->getAi().isDestinationReached()) {
				removeCreature(*creature);
				creature->getAi().onGeneralEvent(AIEventType::MOVE_ARRIVED);
				ZoneUpdateService::getInstance().add(*creature);
			} else {
				creature->getAi().onGeneralEvent(AIEventType::MOVE_VALIDATE);
			}
		});
	}
};
```
`gameserver.debug.serial_movement=true` makes `parallelForEach` sequential for reproduction.

### How mechanical is it?

**~850 schedule sites + ~100 delayed broadcasts + 15 execute/submit.**
- *est.* 65-70% capture only `this` (AI, handler, controller, effect): add `this,` as the pin and turn `this::m` into `[this] { m(); }`. A regex-assisted rewrite handles it.
- *est.* 25% also capture locals (player, npc, env): add `= Ref(x)` per captured object (or capture a `Ref` local by value).
- ~5% need thought:
  - 106 anonymous `Runnable` classes with state become structs or `mutable` lambdas;
  - 19 Future holder arrays/AtomicReferences become `Ref<AtomicReference<Future>>`;
  - FixPath and teleport are covered above.
- `cancel(true|false)`, `isDone`, `isCancelled`, `getDelay` and `addTask` are unchanged.

**~1,600 handler files + ~150 commands.** Syntax translation is the bulk. The rules add:
- **quests** (1,035; 0 mutable object fields, 36 files schedule): ~0-1 rule edits per file, essentially pure syntax.
- **AI** (461; 41 object fields, 126 `Future` fields, 295 `Atomic*`, 461 schedule sites): ~3-6 edits per file.
- **instances** (78; collection fields, 244 schedule sites): ~8-20 edits per file.
- **commands:** mostly syntax, plus ~6 reflective ones (Configure, Ai, Debug, Dye, Send, Reload) that need real design.

Overall the ownership and threading rules add roughly 10-15% on top of plain syntax porting (*est.*). Every change is local and reviewable against the Java line.

---

## 9. Performance, debuggability, foundation effort

### 9.1 Realistic load (local play, a few players)

- **Objects** (*est.*): world maps spawn a large part of the 131,896 spots, say 60-90k objects. At ~1.5-3 KB per Npc (object, controller, AI, KnownList, EffectController, AggroList, stats, life stats, move controller, 16-byte Monitor), that is ~150-250 MB.
- **Total memory** (*est.*): static data ~0.3-0.5 GB, geo ~0.3 GB, so ~1 GB overall. Leaked reload holders add to this only when `//reload items|skills` is used.
- **Movement tick:** only creatures in active regions move (regions activate around players). With a few players that is hundreds to low thousands of movers. Per mover the cost is `moveToDestination` + `World::updatePosition` + a KnownList update over 9 neighbour regions. Refcounts are not the dominant cost: borrowed snapshots avoid inc/dec, `Field<float>` loads are plain moves, and region scans iterate borrowed pointer snapshots. *est.* a few ms per 200 ms tick on one core, divided across cores−1.
- **Primitive costs** (*est.*, x64): uncontended atomic inc/dec 2-5 ns; Monitor lock/unlock ~20 ns; task-boundary epoch publish ~5-10 ns; `values()` snapshot of 300 pointers < 1 µs; scheduler heap op < 1 µs at 50k timers; eager serialization is the same work Java does, just earlier.
- **Reclaimer:** a 50 ms epoch plus batch destruction on its own core. Memory is delayed by up to one epoch, or by the longest running task (a DB-heavy enter world takes a few hundred ms).
- **Startup:** parallel static data load (per-file pugixml), eager parallel BIH build (~5.5M triangles), `spawnAll` per map on ForkJoin, which mirrors Java's parallel phases.
- **Verdict:** the overhead against a single-threaded design is real (atomics, snapshots, epoch bookkeeping) but irrelevant at this scale. The design would scale worse than a strand design to thousands of players, which is not a goal.

### 9.2 Debuggability

- Exceptions carry `std::stacktrace` (commons). NPEs are real exceptions with the dereference site's stack.
- Slow-task warnings and RunnableStatsManager statistics are keyed by the schedule call's `source_location`.
- The Watchdog gives exact Monitor deadlock cycles with owners' current tasks, then exit RESTART (Java parity).
- Zombie registry, plus an admin `//debug refs` command listing live and retired counts per class. An `AION_REF_TRACKING` build records the sites that hold refs to a chosen objectId.
- Debug `Ptr` stamps catch escaped borrows deterministically. ASan works on MSVC; TSan runs on the Linux clang build.
- `single_executor` and `serial_movement` modes turn a heisenbug into a mostly deterministic reproduction.
- Everything keeps Java names, so `grep` works across both trees.

### 9.3 Foundation effort (*est.*, focused person-days for one experienced C++ developer)

| Component | C++ lines | Days |
|---|---|---|
| RefCounted/Ref/Ptr/Field/Monitor/atomics + Reclaimer (EBR, resurrection) + stress tests | ~1,500 | 8-12 |
| Concurrent containers (CHM, COW list, CLQ, sets, SynchronizedList) | ~1,000 | 5-7 |
| Future + ThreadPoolManager (3 pools, rejection policy, run/get/timeout, stats, shutdown) + Java-semantics tests | ~1,300 | 7-10 |
| CronExpression + CronService + CronServiceTest vectors | ~900 | 5-7 |
| ForkJoinPool `parallelForEach`, SerialExecutor, shutdown coordinator | ~400 | 2-3 |
| IDFactory (BitSet, quarantine) + zombie registry + Watchdog | ~600 | 4-5 |
| AionConnection eager path: Crypt, opcode codec, OutgoingPacket, PacketSendUtility core, broadcast helpers | ~1,500 | 7-10 |
| `Reloadable` holders + DataManager publishing + generator hooks for `mutable Field` / RefCounted spawn family | ~400 | 2-3 |
| Lints (Python over headers and schedule sites) + CI TSan job on Linux | ~400 | 3-4 |
| **Total** | **~8,000** | **~45-60** |

---

## 10. Risks, weaknesses, and what to prototype first

### 10.1 Honest weaknesses

1. **UB freedom is by discipline, not by proof.** It relies on wrapper types, a few porting rules, lints, debug stamps and sanitizers. A single `[this]` without a pin, or a raw `std::vector` member in a shared class, reintroduces UB that may only show under timing.
2. **Faithfulness includes Java's race bugs:** lost updates, TOCTOU, shared spawn template mutation. Field<T> makes them safe, not correct.
3. **Refcount cycles leak where the GC would collect:** handler back-references, aborted despawns. The zombie registry detects them; humans fix them.
4. **The Reclaimer is global infrastructure.** A bug there corrupts everything. A long or stuck task (FixPath waits, a deadlock) delays all reclamation. A future change that lets an unregistered thread touch objects is a silent hazard.
5. **Eager serialization is a visible deviation.** Two threads sending to one connection can now enqueue an older snapshot after a newer one (Java's lazy write would send the latest state twice). Write-time side effects move to send time.
6. **Verbosity and overhead:** `Field<T>` on every non-final field, NPE branches on every dereference, snapshot copies. Irrelevant for local play, but makes the code heavier than idiomatic C++.
7. **Hard to test with MSVC alone:** no TSan on Windows. A Linux build is required to get real race detection.
8. **Leak-on-reload** grows memory with every `//reload items|skills`.
9. **Detaching instance handlers at destroy and quarantining IDs** are behaviour deviations, even if benign.

### 10.2 Prototype first (in order)

1. **Lifetime core stress test.** Build Ref/Ptr/Field/Reclaimer and hammer them: N threads randomly store, drop, borrow and resurrect, with ASan on MSVC and TSan plus ASan on Linux clang. Measure the cost of inc/dec, epoch publish and snapshots.
2. **Scheduler conformance suite.** Cover:
   - cancel while running;
   - self-cancel;
   - `run()` on a pending scheduled task and on a deferred task;
   - `get(timeout)` woken by cancel (the FixPath case);
   - fixed-rate catch-up;
   - an exception in a periodic task;
   - `getDelay`;
   - shutdown dropping delayed tasks.

   Then load test with 50k timers.
3. **World vertical slice.** Load world_maps + npc/spawn templates, spawn a large map (e.g. 220080000) or all world maps, and port MapRegion/KnownList/MoveTaskManager/a stub AI. Walk 3-5 fake players through dense areas. Measure the 200 ms tick, memory per Npc, refcount traffic, reclamation latency, and zombie count after repeated spawn/despawn.
4. **Eager packet path with the real 4.8 client.** SM_KEY/version/auth/character list, then SM_PLAYER_INFO/SM_NPC_INFO/SM_MOVE broadcasts with a per-recipient packet. Confirm batching and the eager timing are invisible to the client.
5. **Twenty representative handlers under the rules:**
   - YamenessPortalSummonedAI, TwinProtectorAI, SacrificialSoulAI;
   - LowerUdasTempleInstance, one PvP arena;
   - a DoT effect;
   - a quest with an anonymous Runnable (_2208);
   - FixPath and CM_TELEPORT_ANIMATION_DONE.

   Measure per-file effort, lint false positives, MSVC compile time per TU, and whether rerunning an instance 50 times returns memory to baseline (validating the §2.6 handler detach).
6. **Deadlock watchdog and single-executor debug mode** on the slice: inject a lock-order inversion and check that the report names both tasks.


## Porting cost

These figures are estimates from source counts, not measurements.

**Foundation:** ~8k C++ lines, ~45-60 focused developer-days. It covers:
- lifetime core (Ref/Ptr/Field/Monitor/Reclaimer);
- concurrent containers;
- Future/ThreadPoolManager and CronService;
- ForkJoin `parallelForEach`;
- IDFactory with quarantine, zombie registry and watchdog;
- the eager AionConnection/PacketSendUtility path;
- Reloadable holders;
- lints and a Linux TSan CI job.

**Schedule sites** (~850 plus ~100 delayed broadcasts and 15 execute/submit):
- ~65-70% only need a pin added (`schedule(this, [this]{...})`);
- ~25% also capture objects as Ref;
- ~5% need manual work: 106 anonymous stateful Runnables, 19 Future holder arrays/AtomicReferences, FixPath and teleport.

**Handlers:** the rules add ~10-15% on top of plain syntax translation.
- Quests (1,035): ~0-1 rule edits per file.
- AI (461): ~3-6 edits per file.
- Instances (78): ~8-20 edits per file.
- Commands: mostly syntax, except ~6 reflective ones.

Rough wall time with tool/LLM-assisted batches and review: quests ~250 h, AI ~200 h, instances ~150 h, commands ~150 h, so ~750 h for handlers. No handler needs to change structure or add liveness checks Java did not have.

## Weaknesses

- UB freedom rests on discipline: wrapper types, porting rules, lints, debug Ptr stamps and sanitizers. It is not a compile-time proof. A single unpinned [this] capture or a raw std container member in a shared class brings back timing-dependent use-after-free or data races.
- Fidelity preserves Java's logical race bugs: lost updates, TOCTOU, TwinProtectorAI.adds used from 3 threads, cross-instance mutation of shared SpawnTemplates, InventoryDAO marking items UPDATED after a failed batch. Field<T> makes them memory-safe, not correct.
- Refcounting leaks cycles the GC would collect: handler fields with back-references, aborted despawns ('did not leave world cleanly'). The zombie registry only detects them; each needs a manual fix. Instance handler cycles need a deviation (detaching the handler at destroy).
- The epoch-based Reclaimer is global infrastructure. Any bug in it is catastrophic. A long or stuck task (FixPath's 5 s waits, a heavy enter-world, a deadlock) delays all reclamation. Code that later lets an unregistered thread (e.g. an IO strand) touch game objects is a silent hazard.
- Eager packet serialization is a deliberate deviation. The state is sampled at send time. Two threads sending to the same connection can enqueue an older snapshot after a newer one. The 6 write-time side effects fire at send time, and SM_KEY stays a special strand-serialized packet.
- Performance and verbosity: Field<T> on every non-final field, an NPE branch on every Ref/Ptr dereference, atomic refcounts, snapshot copies and epoch bookkeeping. Fine for a local server, but it scales worse than strand or single-executor designs and reads heavier than idiomatic C++.
- Free-threaded bugs stay nondeterministic, and MSVC has no ThreadSanitizer. Real race detection needs the Linux clang build. The single-executor debug mode only approximates production interleavings.
- Leak-on-reload: every //reload of items or skills permanently keeps the old holder (tens of MB each).
- The ID quarantine (release delayed ~5 min) and the no-op handler on destroyed instances are behaviour deviations, even if they approximate Java's practical GC timing.
- Deadlocks remain exactly as possible as in Java. The Monitor wait-graph watchdog detects them but does not prevent them.

## Prototype first

- Lifetime core: Ref/Ptr/Field/Monitor plus the epoch Reclaimer with resurrection, under a randomized multi-threaded store/drop/borrow/resurrect stress test with ASan (MSVC) and TSan+ASan (Linux clang). Measure inc/dec, epoch publish and snapshot costs.
- Scheduler conformance suite against Java semantics: cancel while running, self-cancel, run() on a pending scheduled task and on a deferred task, get(timeout) woken by cancel (FixPath), fixed-rate catch-up without overlap, exceptions not stopping periodic tasks, getDelay, shutdown dropping delayed tasks. Then a 50k-timer load test.
- World vertical slice: load world maps, npc and spawn templates, spawn one dense map (e.g. 220080000) or all world maps, port MapRegion/KnownList/MoveTaskManager with a stub AI, move 3-5 fake players. Measure the 200 ms tick, memory per Npc, refcount traffic, reclamation latency and zombies after repeated spawn/despawn.
- Eager serialization path against the real 4.8 client: SM_KEY/version check/auth/character list, then SM_PLAYER_INFO/SM_NPC_INFO/SM_MOVE broadcasts including a per-recipient packet. Verify the timing and write batching are invisible to the client.
- Port ~20 representative handlers under the rules (YamenessPortalSummonedAI, TwinProtectorAI, SacrificialSoulAI, LowerUdasTempleInstance, a PvP arena, a DoT effect, a quest with an anonymous Runnable, FixPath, CM_TELEPORT_ANIMATION_DONE). Measure per-file effort, lint false positives and MSVC build time per TU. Run an instance 50 times and check memory returns to baseline.
- Watchdog and debug single-executor mode on the slice: inject a lock-order inversion and confirm the report names both tasks and monitors. Confirm single_executor/serial_movement reproduce a known interleaving bug.
