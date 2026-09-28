# Game server runtime architecture (threading, ownership, scheduling, packets)

> **Status: superseded.** This was the panel's synthesized single-logic-thread design (2026-09-13). The user chose the free-threaded model (D1); the final design is [../runtime-architecture.md](../runtime-architecture.md). Kept for its analysis and red team findings RT-1..RT-14, which the final design carries over.

## Decision summary

Game logic runs on one dedicated logic thread, which is the main thread once startup is done. It is a hand-written event loop, not an Asio strand. It runs client packet runImpl, all scheduled and cron tasks, AI, movement, geo queries, LS/CS packets, disconnect handling and DB calls. DB calls run inline by default, the way Java ran them on its pool threads. Asio IO threads (N is allowed) only decrypt, run the pure readImpl, post to the logic thread, and frame and encrypt bytes that are already serialized. A small BlockingPool accepts detached work only: reload parsing, pure-SQL jobs, the blocking connect for LS/CS. A coordinator thread handles shutdown, so nothing ever pumps jobs inside another job.

Game objects are shared through a non-atomic intrusive Ref<T>. Anything stored (fields, containers, captures, packets) is a Ref. Anything borrowed (parameters, locals, return values) is a Ptr<T>/T&; dereferencing a null one throws a Java NullPointerException, and in checked builds a Ptr carries a job-serial stamp. Nothing is freed during a job: when a count reaches zero the object goes on a zombie list. The list is drained after the job and after its packet flush. The objectId is released in ~AionObject, keeping the RespawnService deferral, and then waits in a 60 s quarantine before reuse. This keeps Java's GC-shaped behaviour. Stale effectors, team members held for 600 s after logout, handler Npc fields, and delete-then-use calls all port without new null checks. Tasks keep their pinned owners alive and are never silently dropped (the anchor approach was rejected).

Scheduling uses a Pin-first overload, `schedule(this, [this]{...}, delay)`. A task that captures anything without a pin must be captureless or built with bindTask; the compiler enforces this. Every task records its std::source_location, which feeds stats, the watchdog and a //tasks command. The Future type follows Java: cancel, isDone, isCancelled, getDelay (still valid after cancel), deferred, runNowIfPending, and self-cancel is safe.

Server packet writeImpl bodies stay the Java code. Packets are serialized on the logic thread when the job ends: once per broadcast, or once per recipient for the connection-dependent ones. close(packet) goes through the same outbox in order. Each packet serializes inside its own exception guard. An `immediate` switch exists for diagnosis.

synchronized, the Atomic* classes and the concurrent collections become shims with the same API. Collections that are iterated while callbacks can re-enter become StableMap/StableSet. Their values are Refs or scalars, callbacks receive them by value, iteration uses indexes, and removal leaves tombstones. This fixes the MoveTaskManager walker re-add use-after-free that I verified.

Instance handlers are RefCounted and hold a strong Ref to their instance. After onInstanceDestroy, destroyInstance swaps the handler for a shared no-op handler. WorldPosition keeps a strong Ref<MapRegion>, which pins the instance just as Java's strong reference does. The weak reference with a hollow shell is rejected. Replaced AIs go to retiredAis.

Static data follows decision B unchanged: immortal const templates, HolderRef that leaks old holders on reload. Spawn/walker/event data becomes logic-thread-only, so their mutexes are removed. SpawnTemplate refs forward counting to their owning SpawnGroup. The handler registry design is kept, with one change: InstanceFactory returns Ref<InstanceHandler>.

Thread-confinement and borrow checks are always on in the Debug and RelWithDebInfo builds used to run the server. Release keeps the boundary checks. Tests get a ManualClock, runReady/advance, a seeded Rnd, and a leak census.

# Runtime architecture of the C++ game server (decision A, final)

Base: the **single-logic-executor** proposal. It scored highest on two of the three lenses: fidelity 8 and hobby pragmatics 8.5, with safety a close second at 7.

Additions from the other two proposals:
- **handles-world-owned:** always-on confinement asserts, compile-time captureless unpinned tasks, a shutdown coordinator, a split between logic-only and shared config, scripted classification of schedule sites, and task introspection.
- **free-threaded:** the Pin-first schedule overload, detaching the instance handler at destroy, debug borrow stamps, and source_location-keyed statistics.
- **Judges' findings:** fixes for StableMap, nested reclamation, and close ordering in the outbox.

Java claims I re-checked for this synthesis:
- ThreadPoolManager.java:51-79: `RunnableWrapper(catch=true)` wraps schedule and execute; submit uses `catch=false`.
- CM_TELEPORT_ANIMATION_DONE.java:36-48: it catches only `ExecutionException`, so only a deferred FutureTask can rethrow.
- HostileUpEffect.java:32,46-49,61: the delayed task reads the template's `tempHate` when it runs.
- IncarnateAI.java:27-35: the dying boss schedules the claw's delete 5 minutes later.
- InstanceService.destroyInstance:92-106: remove from the map, delete objects while iterating, then `onInstanceDestroy`.
- WorldPosition.java:25,135: a strong `mapRegion` whose parent is the instance.
- WorldMapInstance.java:51: a final handler.
- Ai.java:86-90: reflective AI replacement between despawn and spawn.
- PlayerGroup.java:30-32: `onRemoveMember` calls `setPlayerGroup(null)`.
- NpcController.java:311 plus MoveTaskManager.java:42-48: the walker is re-added during iteration.
- Commons `AConnection::close(closePacket)` (AConnection.h:250-260) and Java: close clears the queue and then pushes the close packet.

---

## 0. Contradictions resolved

| # | Contradiction | Resolution | Why |
|---|---|---|---|
| 1 | Threads: Java pools (free-threaded) vs one logic thread | **One logic thread** | Data-race UB is ruled out by construction. Deterministic tests. No Field<T> or Monitor discipline over 390k lines. critic.md shows only 2 blocking sites and no reliance on interrupts |
| 2 | Ownership: intrusive Ref (GC-shaped) vs registry plus generational handles | **Non-atomic intrusive Ref with end-of-job reclamation** | Handles turn Java's stale-but-readable reads into null checks at 202 getEffector sites and more. Anchors drop tasks Java ran: IncarnateAI's claw delete is scheduled from a corpse that decays after 2 s. Ref keeps Java behaviour with one uniform edit per site |
| 3 | Task lifetime: capture `keepAlive` vs anchors vs pin | **Pin as the first argument** (`schedule(this, [this]{...})`). A task retains its pins until it has run or been cancelled. Unpinned callables must be captureless or `bindTask` (compile-time check). Anchors are kept only as an introspection index, never for cancellation | Hard to forget, easy to lint, never drops behaviour |
| 4 | Borrow safety: raw `T*` from `get()` vs checked Ptr | **`Ptr<T>` throws NullPointerException.** In checked builds it also stamps the job serial and asserts on dereference in a later job | A missed null check becomes a logged NPE for one job instead of a crash of the whole world. Escaped borrows are caught on MSVC without TSan |
| 5 | Thread asserts in debug only vs all builds | **Checked builds (Debug and RelWithDebInfo, the build used to run the server) assert on every refcount operation and Ptr dereference. Release asserts at boundaries**: `post` receipt, scheduler, World/registry mutators, Reclaimer push, outbox push | Cost is one thread-id compare. The per-operation checks run in the builds people actually use |
| 6 | Instance cycle: weak position→instance with a hollow shell vs detaching the handler | **Strong `Ref<MapRegion>` in WorldPosition, which pins the instance. `InstanceHandler` is RefCounted and holds `const Ref<WorldMapInstance>`. After `onInstanceDestroy()` the instance's handler is swapped for a shared no-op handler** | Keeps Java's strong reference and changes meaning in fewer places. It breaks instance→handler→Npc→position→instance |
| 7 | Packets: eager at `sendPacket` vs end-of-job outbox | **End-of-job outbox.** `close(packet)` is an ordered outbox entry. Each packet serializes inside its own try/catch. Broadcasts are deduplicated per broadcast group, not by packet address. There is a `packet_serialization=immediate` switch | Closest to Java's lazy write (it reads state after the sender finishes). The 6 mutating writeImpls stay unchanged and legal. Java close also clears the queue, so the ordering stays faithful |
| 8 | Buffer ownership in packets: commons "no buffer member" vs Java-shaped `writeD(x)` | **Game-server `AionServerPacket` has a transient `buf` pointer, set only during `serialize` on the logic thread. `writeD(v)` forwards to the commons `writeD(*buf, v)`** | Serialization is confined to one thread, so this is safe. The 3,890 writeImpl lines port verbatim and write-sequence parity stays textual |
| 9 | StableMap handed out `V&`/`V*` into a vector (use-after-free on walker re-add) | **Values must be `Ref<T>`, a handle or a scalar. Callbacks receive values by copy. Iteration is by index up to the entry count at start, re-reading each slot. Erase writes a tombstone. Compaction and the destruction of erased values happen at iteration depth 0** | Removes both UAFs the judges found. Java objects stored by value (AggroInfo, KnownObject) become RefCounted |
| 10 | Shutdown: `pumpUntil` nested jobs (reclaim at depth>0) vs coordinator | **A ShutdownCoordinator thread sequences the phases by posting jobs and waiting on promises. The logic loop is never re-entered.** The only inline nested execution is `Future::runNowIfPending`, which is not a job boundary | Removes nested reclamation and nested flush entirely |
| 11 | DB offload: build `unique_ptr<Player>` off-thread vs rows only | **Inline by default. Offloading is allowed only for pure SQL that returns rows/values; objects are built on logic. RefCounted objects are never created off the logic thread** | Refcounts are non-atomic, and DAO loaders reach World and caches |
| 12 | FixPath: coroutine vs continuation object | **Continuation object driven by a pinned fixed-rate task.** No coroutine infrastructure | One site does not justify a coroutine framework |
| 13 | Config: ConfigValue/atomic everywhere (CONVENTIONS) vs plain logic-only fields | **Classification per config class.** `LogicConfig` classes are plain `static inline` fields, rebound only on logic (asserted). `SharedConfig` classes (Network, Pff/Flood, Thread, Database, logging, LS/CS link) follow the existing ConfigValue/atomic rule | 420 properties are read verbatim in handlers. Classifying whole classes is a mechanical choice |
| 14 | `HostileUpEffect.tempHate`: mutable template field (runtime) vs per-effect (static data) | **Per-effect state (static data design wins)** | The Java delayed task reads the template field later, so another cast can change it. That is a bug, not intent |
| 15 | Spawn family ownership: independently refcounted vs group-owned | **SpawnGroup owns its SpawnTemplates; `Ref<SpawnTemplate>` forwards counting to the group (RefPart). SpawnsData holds `Ref<SpawnGroup>`.** Static data's `recursive_mutex` on SpawnsData and WalkerData is removed (logic-only, asserted). The `ZoneName` intern mutex stays (startup workers) | No template↔group cycle, no weak pointers |
| 16 | `runtime_mutable` atomics (static data) | **Generated as plain `mutable` fields with a logic-thread assert in the setter** | Only logic reads them |
| 17 | Handler registry `InstanceFactory` returns `unique_ptr` | **Returns `Ref<InstanceHandler>`**. AI stays `unique_ptr` (a part), and replaced AIs go to `retiredAis` | Required by #6 |
| 18 | ID quarantine 300 s vs 60 s | **60 s default** (`gameserver.idfactory.release_delay`) | Only bare-int holders are exposed; any Ref holder already prevents release |
| 19 | Movement: parallel (Java) vs sequential | **Sequential, in insertion order**, with a two-phase parallel tick as a documented fallback if measurements require it | Single thread |
| 20 | Java ArrayList/HashMap fields iterated re-entrantly: UB in C++ vs CME in Java | **Shims named `ArrayList<T>`/`HashMap<K,V>` for fields of game objects and handlers. Their iterators check a modCount and throw `ConcurrentModificationException`** | Keeps "exception, not UB". Locals stay `std::` |

---

## 1. Threading model

### 1.1 Threads

| Thread | Count | Runs | Touches game state |
|---|---|---|---|
| **Logic** (main after startup) | 1 | client packet `runImpl`; every `schedule*`, `execute`, cron callback and periodic manager (MoveTaskManager, MovementNotifyTask, ZoneUpdateService, TeamStatUpdater…); AI events; KnownList; skills and effects; geo queries; DAO calls (inline); outbox serialization; LS/CS packet `runImpl`; `onDisconnect` bodies; `//reload` publishes; Config rebinds; shutdown phases | yes, exclusively |
| Asio IO (`NioServer`) | `gameserver.network.nio.threads` (>1 allowed, deviation) | accept, read, `Crypt.decrypt`, client packet factory plus state check, PFF/flood, `readImpl`, `post`. Write side: frame, `Crypt.encrypt` and SM_KEY enabling on the connection strand | no (reads only `std::atomic<State>`) |
| BlockingPool | 2 | `executeLongRunning` jobs audited as detached; reload XML parse; NN training on copied data; blocking `openSocket` for LS/CS; pure-SQL `submitBlocking` jobs | no (captures are values only; asserted) |
| Startup workers | cores, before the loop only | parallel per-file XML parse (decision B), geo load plus eager BIH build | only immutable data; never RefCounted |
| ShutdownCoordinator | 1, created on demand | sequences the shutdown phases (§1.5) | no |
| Watchdog | 1 | reads atomic `jobStartedAt`/`jobSource`; warns after 5 s (`ThreadConfig::MAXIMUM_RUNTIME_IN_MILLISEC_WITHOUT_WARNING`); stall dump after `gameserver.watchdog.stall_seconds` | no |
| Console | 1 (optional) | Ctrl+C handler and stdin commands, both posted | no |
| Logging, Discord | existing commons | | no |

Removed Java threads and pools: PacketProcessor (4), scheduled pool, instant pool, Quartz, ForkJoin `parallelStream`, NetFlusher Timer, Cleaner.

### 1.2 GameExecutor

```cpp
namespace aion::gameserver::utils {
using Job = std::move_only_function<void()>;

/** Single game-logic executor. Replaces PacketProcessor, the ThreadPoolManager pools and the Quartz thread (Java has no equivalent). */
class GameExecutor {
public:
	static GameExecutor& getInstance();
	void post(Job job, std::source_location where = std::source_location::current()); // any thread; FIFO
	void run();   // called by GameServer::main after startup; returns after stop()
	void stop();  // any thread
	[[nodiscard]] bool isLogicThread() const noexcept;
	[[nodiscard]] uint64_t currentJobSerial() const noexcept; // Ptr stamps
	// tests
	void setClock(Clock& clock);                 // ManualClock
	void runReady();                             // run due timers + posted jobs, no waiting
	void advance(std::chrono::milliseconds dt);  // ManualClock + runReady after each due point
};

inline void assertLogicThread(std::source_location = std::source_location::current()); // logs stacktrace + std::terminate
}
```

**Loop iteration**
1. Run due timers in `(due, seq)` order with a 20 ms budget.
2. Swap the inbox and run the posted jobs with a 20 ms budget.
3. Wait on a condvar until `min(nextDue, inbox signal)`.

**Each job runs inside `JobScope`**
```
++jobSerial; watchdog.begin(where);
try { job(); } catch (...) { log.errorCurrentException(where); }  // Java RunnableWrapper
outbox.flush();          // serialize + enqueue bytes; per-packet try/catch
reclaimer.reclaimAll();  // iterative; destructors may release more Refs -> loop until empty
stats.record(where, dt); watchdog.end();
```

**Timing**
- Windows: `CREATE_WAITABLE_TIMER_HIGH_RESOLUTION` waits, so the 200 ms movement tick does not jitter by 15.6 ms.
- Scheduling uses `steady_clock`. Game code keeps `currentTimeMillis()`.

### 1.3 What runs where

| Java origin | C++ |
|---|---|
| `AionConnection.processData` → PacketProcessor (serial per connection) | strand: decrypt → factory → `readImpl` → `post`. Logic: `if (pck->isValid()) pck->run()`. Order per connection is preserved by one FIFO consumer. The 2 logging reads (CM_BUY_ITEM:48, CM_ATREIAN_PASSPORT:33) move to `runImpl` |
| `schedule`/`scheduleAtFixedRate` | logic timers (§4) |
| `execute`/`submit` | `post`: runs after the current job |
| `executeLongRunning`/`submitLongRunning` | posted to logic, unless the site is audited as detached (then BlockingPool) |
| Quartz | logic one-shot timers re-armed per fire (§4.4) |
| `parallelStream` movement | sequential loop (§8h) |
| LS/CS: NIO plus unordered instant pool | IO on Asio; packets posted in order (deviation). Reconnect: logic timer → BlockingPool `openSocket` → posted `registerConnection` |
| `onDisconnect` (disconnect executor) | `nioServer.connect([](auto t){ GameExecutor::getInstance().post(std::move(t)); })` |
| MapRegion activation `execute` | `post` |
| GeoService | inline; immutable after eager BIH; door/placeable state is logic-only |
| DAO calls | inline (§1.4) |
| DatabaseCleaningService "main thread" check | holds: logic == main |

### 1.4 DB policy

- **Default:** inline on logic, exactly where Java calls the DAOs (255 in services, 33 inside model mutators, 8 in client packets). The pool is pre-warmed at startup.
- **Offload API**, only for pure SQL whose result is applied on logic:
```cpp
template <std::invocable Work, class Then>
void ThreadPoolManager::submitBlocking(Pin pin, Work work, Then then, std::source_location = current());
// work: BlockingPool, captures DetachedArg only (ids, strings, row structs) - checked by concept + refcount thread assert
// then(std::expected<R, std::exception_ptr>): posted to logic, pins retained until then runs
```
- Rule: never construct RefCounted objects on the BlockingPool.
- If enter-world or character-list stalls are measured as noticeable, split those specific loaders: rows are loaded off-thread, objects are built on logic. That is a non-mechanical DAO change, applied only where measured.

### 1.5 Blocking sites and shutdown

- **CM_TELEPORT_ANIMATION_DONE:** `Future::runNowIfPending()` runs inline (§8g).
- **FixPath:** a `PathFixJob` continuation polled by `scheduleAtFixedRate(&admin, ..., 250)`. It advances when Z settles and aborts after 5 s. No waiting.
- **Shutdown:**
  1. The Ctrl+C or `//shutdown` countdown runs on logic timers.
  2. The final phase starts the ShutdownCoordinator.
  3. The coordinator calls `NioServer::shutdown()` on its own thread. Meanwhile the normal logic loop runs the posted `onDisconnect`/`leaveWorld` jobs.
  4. It then posts `PeriodicSaveService::onShutdown`, `saveGameTime` and `CronService/ThreadPoolManager::shutdown` as one job, and waits on a promise.
  5. It calls `GameExecutor::stop()`, flushes the logs and calls `quick_exit(code)`, skipping the teardown of ~100k objects.
- **Watchdog:**
  - Slow job warning at 5 s (Java parity).
  - Stall past `stall_seconds` (default 60) writes a stack dump or minidump plus the job's source location.
  - Exit with RESTART only if `gameserver.watchdog.restart_on_stall=true` (Java DeadLockDetector analogue).
  - Disabled while `IsDebuggerPresent()`. Debug config relaxes client ping timeouts.

---

## 2. Object ownership and references

### 2.1 Core types

```cpp
namespace aion::gameserver::model {

/** Base of every heap object whose Java reference escapes into fields, tasks, packets or other objects. Not in Java (GC). */
class RefCounted {
public:
	RefCounted(const RefCounted&) = delete;
protected:
	RefCounted() noexcept = default;
	virtual ~RefCounted();               // Reclaimer only, logic thread, noexcept: may release Refs and IDs; no packets, events, new Refs to this, no Ptr deref
private:
	mutable uint32_t refs = 0;
	mutable bool queued = false;
	friend class Reclaimer; friend void refAdd(const RefCounted*) noexcept; friend void refRelease(const RefCounted*) noexcept;
};
inline void refRelease(const RefCounted* o) noexcept {
	AION_CHECKED_ASSERT_LOGIC_THREAD();
	if (--o->refs == 0 && !o->queued) { o->queued = true; Reclaimer::push(o); } // never inline delete
}
// Reclaimer::reclaimAll(): while (!list.empty()) { swap; for o: o->queued=false; if (o->refs == 0) { census.onDestroy(o); delete o; } }  (resurrected zombies skipped)

/** Parts whose lifetime equals their owner's (AI, controllers, KnownList, EffectController, AggroList, stats, MapRegion, SpawnTemplate-in-group).
 *  Ref<Part> retains the owner. */
class RefPart { protected: explicit RefPart(const RefCounted& owner) noexcept; public: const RefCounted& refOwner() const noexcept; };

/** Marker for immortal objects whose `this` may be captured without retention: templates, quest handlers, commands, services. */
struct Immortal {};

template <class T> class Ref {                        // stored reference (Java reference-typed field / capture)
public:
	Ref() noexcept = default; Ref(std::nullptr_t) noexcept {}
	Ref(T* p) noexcept; Ref(T& r) noexcept; Ref(Ptr<T> p) noexcept;          // implicit: `boss = instance.getNpc(216263);`
	template <class U> requires std::convertible_to<U*, T*> Ref(Ref<U> other) noexcept;
	T* operator->() const;  T& operator*() const;       // NullPointerException("Ref<Npc>") when null
	T* get() const noexcept; operator Ptr<T>() const noexcept; explicit operator bool() const noexcept;
	friend bool operator==(const Ref&, const Ref&) = default;                 // Java == (identity)
};

template <class T> class Ptr {                        // borrowed: params, locals, returns; never stored (lint)
public:
	Ptr() noexcept = default; Ptr(std::nullptr_t) noexcept {}; Ptr(T* p) noexcept; Ptr(T& r) noexcept;
	T* operator->() const;  T& operator*() const;       // NPE; checked builds: assert(job == GameExecutor::currentJobSerial())
	T* get() const noexcept; explicit operator bool() const noexcept;
	friend bool operator==(Ptr, Ptr) noexcept = default;
private:
	T* p = nullptr;
#if AION_CHECKED
	uint64_t job = GameExecutor::getInstance().currentJobSerial();
#endif
};

template <class T, class... A> Ref<T> makeRef(A&&... a);    // only VisibleObject::create<T> (handlers design) and service factories call it
template <class To, class From> Ptr<To> cast(Ptr<From> p);  // Java (To) cast: ClassCastException; null passes
template <class To, class From> Ptr<To> as(Ptr<From> p) noexcept; // Java instanceof pattern: nullptr on mismatch
}
```

Ptr stamps: startup code runs with serial 0, and loop jobs start at 1. A Ptr captured into a scheduled lambda fails its assert the first time the task runs in a test.

### 2.2 What is RefCounted and what is a part

- **RefCounted:**
  - AionObject and all subclasses: Npc, Summon, Pet, Player, Gatherable, StaticObject, Kisk, HouseObject, Item, teams, Legion.
  - Effect, Skill, Future, CronJob.
  - WorldMapInstance, InstanceHandler, ZoneHandler, SpawnGroup.
  - DropNpc, RespawnTask, ActionObserver/AttackCalcObserver, QuestState, AggroInfo, KnownObject.
- **RefPart:**
  - AbstractAI, VisibleObjectController and subclasses, KnownList, EffectController, ObserveController, AggroList, Creature stats, move controllers, Player parts (Inventory, Equipment, lists…).
  - MapRegion (part of WorldMapInstance) and SpawnTemplate (part of SpawnGroup).
- **Values:** QuestEnv (copyable, holds Ref fields), WorldPosition (held by value in VisibleObject), stat modifiers.
- **Immortal:** templates (`const T*`), AbstractQuestHandler singletons, ChatCommand singletons, service singletons.
- **Connections:** `std::shared_ptr<AionConnection>` (commons).
- **Construction** follows the handlers design: `VisibleObject::create<T>(...)` → `makeRef` → `postConstruct()`. The AI is created inside `Creature::postConstruct` in Java order.

### 2.3 Rules by use case

| Use case | Java | C++ | Cycle breaker / note |
|---|---|---|---|
| World registry, player containers, instance/region object maps | CHM | `StableMap<int32_t, Ref<VisibleObject>>` | root. `World::removeObject` keeps the identity check `allObjects.get(id).get() == &object` |
| KnownList (two-way) | CHM<Integer, KnownObject> | `StableMap<int32_t, Ref<KnownObject>>`, `KnownObject{ const Ref<VisibleObject> object; bool visible; }` | `World.despawn → clearKnownlist` clears both sides; census asserts it is empty after removal |
| target | `VisibleObject target` | `Ref<VisibleObject>` | cleared in `notSee` |
| AggroList | CHM<Integer, AggroInfo> | `StableMap<int32_t, Ref<AggroInfo>>`, `AggroInfo{ const Ref<Creature> attacker; int32_t damage, hate; }` | cleared in onDespawn/revive; `hateReductionTask` pinned to the owner, cancelled in `clear()` |
| Effect effector/effected | final | `const Ref<Creature> effector, effected` | a deleted effector stays readable (NpcController.java:293-297). effected↔Effect is broken by `endEffect`/`removeAllEffects` |
| Summon/Pet master, SummonedObject creator, Player.summon/pet | final / field | `const Ref<Player>`, `const Ref<Creature>`, `Ref<Summon>` | `SummonsService.release → setSummon(nullptr)` |
| Team member (incl. logged out) | `final Player player` | `PlayerTeamMember{ const Ref<Player> player; int64_t lastOnlineTime; }`, `Player.playerGroup: Ref<PlayerGroup>` | `onRemoveMember → setPlayerGroup(nullptr)` (verified); timeout, relogin swap, disband |
| Handler fields (`Npc boss`, `List<Npc>`) | fields | `Ref<Npc>`, `ArrayList<Ref<Npc>>` | Java semantics kept. Porting checklist: fields referencing an object whose AI/handler references back (add↔spawner) must be cleared in `handleDespawned`/`handleDied`, or accepted and listed. The census reports misses |
| InstanceHandler | final on instance, handler holds instance | `WorldMapInstance::handler: Ref<InstanceHandler>`; `InstanceHandler::instance: const Ref<WorldMapInstance>` | after `onInstanceDestroy()`, `instance.detachHandler()` sets the shared `NoopInstanceHandler`. The old handler lives while tasks pin it. Deviation: `getInstanceHandler()` on a destroyed instance returns the no-op handler |
| WorldPosition | strong `mapRegion` → parent | `Ref<MapRegion> mapRegion` (retains the instance), `float x,y,z`, `int8_t h`, `bool isSpawned` | instance→regions→objects→position is broken because destroyInstance deletes objects; stale objects keep their instance readable, as in Java |
| AI replacement (`//ai set`) | reflective final field | `Creature::replaceAi(std::unique_ptr<AbstractAI>)` moves the old AI into `retiredAis` | freed with the Creature |
| Spawn template | final `spawnTemplate` | `const Ref<SpawnTemplate>` (retains the SpawnGroup) | SpawnsData holds `Ref<SpawnGroup>`; event removal only drops the SpawnsData ref |
| Scheduled tasks | captures | pins retained by the Future until the callable is destroyed (after run or on cancel) | controller-task cycles are temporary |
| Server packets | lazy, hold objects | `Ref<T>`/value fields; live until the end of the job | nothing survives the flush |
| Player ↔ AionConnection | fields / AtomicReference | `Player::clientConnection: std::shared_ptr<AionConnection>` (logic-only field); `AionConnection::activePlayer: Ref<Player>` (logic-only) | `leaveWorld` clears both. `~AionConnection` may run on IO: if `activePlayer` is still set, it posts `[r = std::move(activePlayer)]{}` to logic (moving does not touch the count) |
| Items | CHM storages | `StableMap<int32_t, Ref<Item>>`, `ArrayList<Ref<Item>> deletedItems` | ID release DB-driven (§3) |
| ID-based Java refs (GeneralUpdateTask, DecayTask, `Npc.creatorId`, `registeredObjects`) | int | unchanged `int32_t` + lookup | quarantine |
| `WeakReference` (DropNpc.java:28, InstanceScaler.java:25) | weak | DropNpc: `Ref<TemporaryPlayerTeam>` (team lifetime short); InstanceScaler: `std::unordered_map<int32_t instanceKey, Scaling>` erased in `destroyInstance` | the only 2 weak sites |

### 2.4 Identity, equality, relogin

- Java `a == b` → `a == b` on Ref/Ptr (pointer identity), at the ~14 audited sites.
- Java `a.equals(b)` → `a->equals(*b)`: objectId equality, and objectId 0 is never equal.
- `hashCode` → `ObjectIdHash`. Object-keyed collections (3 Player, 2 Creature, 1 VisibleObject, 5 Item) become `StableMap<int32_t, Ref<T>>`.
- `List.remove(Object)` → `CollectionUtil::removeFirstEqual`.
- Relogin: `PlayerService::getPlayer` makes a new `Ref<Player>` with the DB objectId. Old instances stay readable while teams, effects or tasks hold them, and they compare `equals` to the new instance.

### 2.5 Java-named container and atomic shims

```cpp
/** Java ConcurrentHashMap / ConcurrentHashMap.newKeySet as used by game code, single-threaded.
 *  V must be Ref<T>, a handle, or trivially copyable. forEach tolerates put/remove from inside the callback. */
template <class K, class V> class StableMap {
public:
	using Borrowed = /* Ptr<T> for Ref<T>, else V */;
	Borrowed get(const K& key) const;
	bool containsKey(const K& key) const;
	std::optional<V> put(K key, V value);          // Java returns previous value
	std::optional<V> putIfAbsent(K key, V value);
	std::optional<V> remove(const K& key);         // tombstone while iterating
	bool remove(const K& key, const V& expected);  // Java remove(k, v), identity for Refs
	template <class F> Borrowed computeIfAbsent(const K& key, F&& make);
	template <class F> void compute(const K& key, F&& remap); // remap(Borrowed old) -> V
	template <class F> void forEach(F&& fn);        // fn(K, Borrowed) by value; index loop to the start count; tombstones skipped
	std::vector<Borrowed> values() const;           // borrowed snapshot, valid for the job
	int32_t size() const noexcept; bool isEmpty() const noexcept; void clear();
private:
	std::vector<std::pair<K, std::optional<V>>> dense; std::unordered_map<K, uint32_t> index;
	uint32_t depth = 0, tombstones = 0; std::vector<V> erasedDuringIteration; // destroyed at depth 0
};
// StableSet<K>, ArrayList<T>/HashMap<K,V> (modCount -> ConcurrentModificationException), AtomicBoolean/Integer/Long/Reference<T>
// (non-atomic, Java API: get/set/compareAndSet/getAndSet/incrementAndGet), CopyOnWriteArrayList<T> = snapshot-iterating ArrayList.
```

`synchronized` blocks are deleted and replaced by a `// Java: synchronized (x)` comment. `ReentrantLock`/`StampedLock` are deleted.

### 2.6 Cycles and leak detection

- Lifecycle hooks break the cycles, as in Java: despawn clears KnownList and aggro, `onDelete` calls `cancelAllTasks`, own effects end, observers are cleared, team swap/timeout, summon release.
- **Leak census** (`LeakCensus`): every AionObject removed from World is recorded. If it is still alive after `gameserver.debug.leak_census_minutes` (default 10), it is logged with class, name, id, refcount and the source locations of the tasks pinning it.
  - `//debug leaks` lists these.
  - Scenario tests assert zero after `destroyInstance` and logout.
- A known Java leak (a fire-and-forget periodic task that never ends) shows up in the census; the ID leaks as it does in Java.

---

## 3. ID lifecycle

| Group | Objects | Release point | Notes |
|---|---|---|---|
| (a) auto-release | Npc, Gatherable, StaticObject, Summon, HouseObject, PlayerGroup (new), PlayerAlliance, League | `~AionObject(): if (autoRelease && id) if (!RespawnService::setAutoReleaseId(id)) IDFactory::releaseId(id);`. Runs in the Reclaimer after the last Ref or borrow in the job | no reuse while any Ref holder exists; RespawnService deferral kept verbatim |
| (b) explicit | Items after a successful DB delete (InventoryDAO:240), PlayerRegisteredItemsDAO:164,167, ExchangeService:274,301, ItemSplitService:90, PetAdoptionService:95, HTMLService:144, CM_CREATE_CHARACTER:68, CMT_CHARACTER_INFORMATION:153 | same call sites, then quarantine | Item objects outlive their IDs as in Java (RepurchaseService) |
| (c) never | FlyRing, Road, CuringObject, AssembledNpcPart, HTML message ids, `CustomInstanceService` static id (becomes a lazy accessor) | never | |
| (d) DB-owned | Players, Legions, Mail, Houses, Guides, Pets | never by lifetime; IDFactory locks them from the DAOs at startup | |

```cpp
class IDFactory { // std::mutex: startup workers / future off-thread SQL callers
public:
	int32_t nextId();                          // lowest free (Java policy, 6484 mask), never a quarantined id
	void releaseId(int32_t id);                // -> FIFO {id, steady_now}
	void releaseObjectIds(std::span<const int32_t> ids);
	void tickQuarantine(std::chrono::steady_clock::time_point now); // logic, 1 Hz: ids older than release_delay (60 s) back to the BitSet
	size_t quarantineSize() const;
};
```

ABA: Ref and Ptr holders are exact. Bare-int holders are protected for 60 s, and the long-lived ones are already defended in Java (RespawnTask checks spawnTemplate identity; `World::removeObject` checks identity).

---

## 4. Scheduler, Future and cron

### 4.1 API

```cpp
namespace aion::gameserver::utils {

/** Retains up to 4 owners for the lifetime of a task. Accepts RefCounted*, RefPart* (retains the owner), Immortal* (no-op), Ref/Ptr. */
class Pin {
public:
	template <class T> Pin(T* object) noexcept;
	template <class T> Pin(const model::Ref<T>& object) noexcept;
	Pin(std::initializer_list<Pin> pins);          // schedule({this, &player}, [this, &player] {...}, delay)
};

/** Java ScheduledFuture / FutureTask / RunnableFuture. Logic thread only. */
class Future final : public model::RefCounted {
public:
	bool cancel(bool mayInterruptIfRunning = false) noexcept; // flag ignored (no interrupts, critic.md). Frees callable+pins now, or after its own run returns
	bool isCancelled() const noexcept;
	bool isDone() const noexcept;                          // one-shot: ran or cancelled; periodic: cancelled
	int64_t getDelay(TimeUnit unit = TimeUnit::MILLISECONDS) const noexcept; // due - now; stays valid after cancel (DropService.java:119)
	/** Java run()+get(): if PENDING, remove from the timer queue and run inline (same job, no flush/reclaim boundary).
	 *  Scheduled task: exceptions logged (RunnableWrapper catch=true). Deferred task: exception rethrown as ExecutionException. */
	bool runNowIfPending();
	static model::Ref<Future> deferred(Pin pin, Job job, std::source_location = std::source_location::current()); // new FutureTask<>(r, null)
};
using FutureRef = model::Ref<Future>;

template <class F> concept UnpinnedTask =
	std::convertible_to<F, void (*)()> || std::derived_from<std::remove_cvref_t<F>, TaskStruct> || IsBoundTask<std::remove_cvref_t<F>>;
template <class T> concept DetachedArg = /* arithmetic, enum, std::string, const Template*, Ref<T> (logic tasks only), shared_ptr<const non-game>, weak_ptr<AionConnection> */;
template <class F, DetachedArg... A> BoundTask<F, std::decay_t<A>...> bindTask(F captureless, A&&... args);

class ThreadPoolManager { // Java name kept; facade over GameExecutor + BlockingPool
public:
	static ThreadPoolManager& getInstance();
	FutureRef schedule(Pin pin, std::invocable auto&& task, int64_t delayMillis, std::source_location = std::source_location::current());
	FutureRef schedule(UnpinnedTask auto&& task, int64_t delayMillis, std::source_location = std::source_location::current());
	template <class Self> FutureRef schedule(Self* self, void (Self::*method)(), int64_t delayMillis, std::source_location = std::source_location::current()); // this::m
	FutureRef scheduleAtFixedRate(Pin pin, std::invocable auto&& task, int64_t delay, int64_t period, std::source_location = std::source_location::current());
	FutureRef scheduleAtFixedRate(UnpinnedTask auto&& task, int64_t delay, int64_t period, std::source_location = std::source_location::current());
	FutureRef scheduleAtFixedRate(Pin pin, std::invocable<Future&> auto&& task, int64_t delay, int64_t period, std::source_location = std::source_location::current()); // self-cancel
	void execute(Pin pin, std::invocable auto&& task, std::source_location = std::source_location::current());   // post
	void execute(UnpinnedTask auto&& task, std::source_location = std::source_location::current());
	FutureRef submit(Pin pin, std::invocable auto&& task, std::source_location = std::source_location::current()); // exception stored
	void executeLongRunning(DetachedTask auto&& task, std::source_location = std::source_location::current());     // BlockingPool
	template <class Work, class Then> void submitBlocking(Pin pin, Work work, Then then, std::source_location = std::source_location::current());
	std::vector<TaskInfo> tasksPinning(const model::RefCounted& owner) const; // //tasks <target>: source, due, period
	std::vector<std::string> getStats() const;  // RunnableStatsManager keyed by source_location
	void shutdown();                            // drops delayed tasks (Java policy); later schedules return cancelled Futures
};
}
```

### 4.2 Semantics

- **Timer queue:** a 4-ary indexed min-heap of `(due, seq, Future*)` that owns a Ref to each pending Future. Cancel erases in O(log n).
- **States:** `PENDING → RUNNING → PENDING` (periodic) or `DONE`; `PENDING|RUNNING → CANCELLED`. The callable is destroyed only outside `RUNNING`, so self-cancel is safe.
- **Fixed rate:** `next = prevDue + period`. Late runs execute back to back, never concurrently.
- **Exceptions:** `schedule*` and `execute` log and continue (periodic tasks survive). `submit*` stores the exception for `get()`.
- **Deviation:** cancelled tasks release their captures immediately. Java kept them until the delay expired, because it never set `setRemoveOnCancelPolicy`.
- **`CreatureController` tasks:** `std::array<FutureRef, magic_enum::enum_count<TaskId>()>` with Java's `addTask`, `cancelTask`, `getAndRemoveTask`, `cancelTaskIfPresent`, `cancelAllTasks` and the DESPAWN warn text.
- **`Future<?>` holders:**
  - fields → `FutureRef`;
  - `Future<?>[]` (Effect.java:48, mutated in place at :246-252) → `std::array<FutureRef, N>`;
  - `AtomicReference<Future>` → `AtomicReference<Future>` shim;
  - captured holders → `Ref<FutureHolder>`.
- **Periodic managers** (`AbstractPeriodicTaskManager`, FIFO managers): `start()` is called explicitly from `GameServer::main` in Java order, never lazily. The initial delay stays `Rnd::get(500, 550)`.

### 4.3 Enforcement of capture safety

| Layer | Check |
|---|---|
| Compile time | an unpinned callable must be captureless, a `TaskStruct` (named task classes, ported anonymous Runnables) or `bindTask` with `DetachedArg`s |
| Lint (`tools/porting/lint_tasks.py`, CI + pre-commit, MSVC-friendly regex) | in a pinned lambda, `this` and every `&name` capture must appear in the pin list; `[&]`/`[=]` are rejected; raw `T*` locals and fields of RefCounted types are rejected (`Ptr`/`Ref` only); `Ptr<` members are rejected |
| Checked builds | Ptr job stamp, refcount thread assert |
| Tests | ASan preset (`msvc-asan`), ManualClock scenarios (§10) |
| Optional | clang-tidy `aion-task-captures` in a clang-cl preset |

### 4.4 Cron

```cpp
class CronExpression { // Quartz subset: seconds, ?, names, lists, ranges, /, optional year; L/W/# -> IllegalArgumentException
public:
	static CronExpression parse(std::string_view text);  // PropertyTransformer<CronExpression>
	std::optional<std::chrono::sys_time<std::chrono::milliseconds>> getTimeAfter(std::chrono::sys_time<std::chrono::milliseconds> after,
		const std::chrono::time_zone* zone) const;
	const std::string& getCronExpression() const;
};
class CronService {
public:
	class Job; using JobRef = model::Ref<Job>;
	JobRef schedule(Pin pin, Job::Runnable r, const CronExpression& e, bool longRunning = false, std::type_index kind = typeid(void),
		std::source_location = std::source_location::current());
	bool cancel(const JobRef& job);
	std::vector<JobRef> findJobs(std::type_index kind) const;
	std::map<JobRef, std::chrono::sys_time<std::chrono::milliseconds>, JobOrder> findNextFireTimes(std::type_index kind) const; // SiegeService.java:323
	void shutdown();
};
```

- **Arming:** a one-shot logic timer at `steady_now + (nextFire_wall - wall_now)`.
- **On fire:** if the wall clock is still before `nextFire`, re-arm. Otherwise run once (Quartz smart misfire) and compute the next fire time from `max(nextFire, now)` in `GSConfig::TIME_ZONE_ID`.
- **Drift:** a 60 s check re-arms all jobs on a wall/steady drift above 2 s.
- **`longRunning`:** ignored (logic thread); DB-heavy parts use `submitBlocking`.
- **`AbstractCronTask`:** keeps its `ServerVariablesDAO` run-on-start check.
- **Tests:** `CronServiceTest` vectors, including a DST change.

---

## 5. Server packets

### 5.1 Model

```cpp
namespace aion::gameserver::network::aion {

class AionServerPacket : public commons::network::packet::BaseServerPacket {
public:
	/** true for the connection-dependent packets (generated list, 25 + SM_GROUP/ALLIANCE_MEMBER_INFO). */
	virtual bool dependsOnConnection() const noexcept { return false; }
	/** Logic thread. [u16 len placeholder][obf opcode][0x44][~obf][writeImpl]. Warns above 8,192 bytes; membership-10 name echo. */
	std::shared_ptr<const std::vector<uint8_t>> serialize(AionConnection& con);
protected:
	virtual void writeImpl(AionConnection& con) = 0;        // Java body unchanged
	void writeD(int32_t v) { commons::network::packet::BaseServerPacket::writeD(*buf, v); } // + writeC/H/Q/F/S/B/...
private:
	commons::utils::ByteBuffer* buf = nullptr;              // set only inside serialize (thread-confined)
};

struct WirePacket {                                         // AConnection<WirePacket> queue element
	std::shared_ptr<const std::vector<uint8_t>> frame;
	enum class Special : uint8_t { NONE, SM_KEY } special = Special::NONE;
};

class Outbox {                                              // one, owned by GameExecutor, flushed after every job
public:
	void push(std::shared_ptr<AionConnection> con, std::shared_ptr<AionServerPacket> packet, uint64_t group); // group 0 = single send
	void pushClose(std::shared_ptr<AionConnection> con, std::shared_ptr<AionServerPacket> closePacket);     // ordered with the pushes
	void flush();
};
}
```

**Flush**
```
seen = map<(group, packet*) -> frame>
for entry in pending (packets pushed during flush are appended and flushed in the same call; checked builds assert depth < 4):
	if entry.con is closing: continue                       // Java: close() cleared the queue and no later send passes canSend()
	try:
		frame = entry.packet->dependsOnConnection() || entry.group == 0 ? serialize(con) : seen.try_emplace(...).serialize once
		entry.isClose ? con->close(WirePacket{frame}) : con->sendPacket(WirePacket{frame})
	catch: log with packet name; continue                   // Java: an NIO write exception affected one packet
```

**Porting**
- `sendPacket(player, new SM_X(...))` → `sendPacket(player, SM_X(...))`. A template overload moves the temporary into `make_shared`.
- Packet members that refer to game objects are `Ref`/value (lint).
- `con.close(new SM_QUIT_RESPONSE())` → `con->close(SM_QUIT_RESPONSE())`, which goes through `Outbox::pushClose`.

**Connection-dependence check (checked builds)**
- `serialize` sets a thread-local `SerializingPacket{packet, flagged}`.
- `AionConnection::getActivePlayer()`/`getAccount()` throws `IllegalStateException("<SM_X> reads the connection but dependsOnConnection() is false")` when the packet is unflagged.
- A misclassified packet therefore fails on first use instead of sending one viewer's race obfuscation to everyone.

### 5.2 Special cases

- **The 6 writeImpls that mutate state** run unchanged at flush on logic:
  - SM_ATTACK:89 and SM_CASTSPELL_RESULT:181 (`setLastCounterSkill`): idempotent, once per broadcast.
  - SM_PET cooldowns.
  - SM_PLAY_MOVIE (`setCustomState`): per recipient.
  - SM_GROUP/ALLIANCE_MEMBER_INFO (`event`): per recipient, sequential.
- **SM_KEY:** `WirePacket{special=SM_KEY}`. The strand's `writeData` writes the key frame and then calls `crypt.enableKey()`, keeping the "first encrypt only enables" quirk.
- **Sends from IO threads** (SM_KEY in `initialized()`, the LS/CS link handshake): `sendPacketNow` is available only for packets with the compile-time `ValuePacket` trait. All LS/CS sends from logic use the outbox, which keeps their order.
- **Crypt:** state lives only on the strand (`processData` decrypts and `writeData` encrypts), so no mutex.
- **`gameserver.network.packet_serialization = end_of_job | immediate`:** `immediate` serializes inside `sendPacket`, with identical writeImpl code. It is a diagnosis switch.
- **Deviation:** packet bytes are sampled when the sending job ends. That is later than an immediate write and usually equal to Java, which wrote after the sender returned. Two sends in one job both show the final state.

### 5.3 PacketSendUtility core

```cpp
namespace PacketSendUtility {
	template <std::derived_from<AionServerPacket> P> void sendPacket(Player& player, P&& packet);         // wraps in shared_ptr
	void sendPacket(Player& player, std::shared_ptr<AionServerPacket> packet);                          // cached packets
	template <std::derived_from<AionServerPacket> P> void broadcastPacket(VisibleObject& object, P&& packet);
	template <std::derived_from<AionServerPacket> P> void broadcastPacket(Player& player, P&& packet, bool toSelf);
	template <std::derived_from<AionServerPacket> P> void broadcastPacket(VisibleObject& object, P&& packet, std::predicate<Player&> auto filter);
	template <std::derived_from<AionServerPacket> P> void broadcastToWorld(P&& packet);
	void broadcastMessage(Npc& npc, int32_t msgId, int32_t delay, auto&&... params); // schedule(&npc, ...) keeps isSpawned() check
}
```

---

## 6. Static data at runtime

Consistent with decision B, with these runtime bindings:

1. **Immortal const templates.** Live objects hold `const ItemTemplate*` etc. Capturing a template's `this` in a task needs no pin (`Immortal`).
2. **`HolderRef<H>` stays an atomic pointer.** The BlockingPool reload parse reads published holders through `LoadContext` fallback.
   - `//reload items|skills|quests|...`: parse and post-process on the BlockingPool, which builds no RefCounted objects. Then a logic job calls `publish`, keeps the old holder forever (logged size), rebuilds QuestEngine's 27 event maps and quest handler set, and retires the old quest handlers.
   - In-place Java setters (`XML_QUESTS.setData`, `NPC_SKILL_DATA`, `EVENT_DATA`) become replacements.
3. **Spawn family.**
   - The JAXB `SpawnMap`/`Spawn`/`SpawnSpotTemplate` stay immutable generated classes.
   - Runtime `SpawnGroup` is RefCounted and owns `std::vector<std::unique_ptr<SpawnTemplate>>`. `SpawnTemplate` (with Siege/Rift/Vortex/Base/Town/AhserionsFlight variants) is a RefPart of its group.
   - `SpawnEngine::newSingleTimeSpawn` returns `Ref<SpawnTemplate>` for a fresh group.
   - SpawnsData index maps are `StableMap`s mutated on logic only (events via cron, `saveSpawn` via admin commands), with no mutex.
   - The 47 handler setters on shared instance templates keep Java's cross-instance quirk, now without a race.
   - `SpawnGroup.poolUsedTemplates` is also cleared in `destroyInstance` (a Java leak fix, listed).
4. **RefCounted objects are created only on the logic thread** (main during startup).
   - Spawn-family objects are built while SpawnsData binds on main.
   - If decision B's parallel holder loading is enabled later, SpawnsData stays on main.
5. **WalkerData** (`addTemplate`/`saveData` from FixPath) is logic-only with no mutex. The **ZoneName** intern table keeps its mutex.
6. **Mutable template fields:**
   - `GuideTemplate.activated` and `Spawn.eventTemplate` are generated as plain `mutable` fields with logic-asserting setters.
   - `HostileUpEffect.tempHate` moves to per-effect state (`Effect` reserved value), a DEVIATION.
   - Walker `RouteStep.z` (FixPath) is a mutable field.
7. **Geo** is an immutable arena. BIH trees are built eagerly and in parallel at startup. Per-instance door/placeable/shield state is logic-only.
8. **Config.**
   - `LogicConfig` classes (the ~30 gameplay configs) have plain `static inline` fields.
   - `Config::load`, event overlays, `//reload config` and `//configure` run on logic and assert it.
   - `SharedConfig` classes (NetworkConfig, PffConfig/FloodConfig, ThreadConfig, DatabaseConfig, logging, LS/CS link, IDFactory delay) keep CONVENTIONS' ConfigValue/atomic rule.
   - P4-01 owns the classification table.

---

## 7. Concurrency safety guarantees

**Impossible by construction** (confinement plus API shape, checked by asserts):
- data races on game objects, positions, strings, containers, KnownLists (Java KnownList.java:192 cross-mutation), spawn data, QuestEngine maps and config;
- the `isOnline()`→`getClientConnection()` TOCTOU;
- periodic saves racing gameplay;
- writeImpl reading state that another thread is mutating;
- refcount races;
- template use-after-free (immortal);
- geo lazy-build races (eager build).

**Single-threaded hazards and their controls**

| Hazard | Control |
|---|---|
| Object freed while its member function runs (`AIActions.deleteOwner(this)` then `getOwner()`) | freed only after the job and flush |
| Use-after-free across jobs through a stored borrow | Ref for stored references; lint; Ptr job stamp; ASan scenarios |
| Iterator invalidation from re-entrant callbacks (KnownList clear→notKnow→AggroList; destroyInstance deleting while iterating; walker re-add in MoveTaskManager) | StableMap/StableSet by-value callbacks and tombstones; ArrayList/HashMap shims throw CME |
| Destroying a callable while it runs (self-cancel, owner cancels its own task) | Future state machine |
| Null dereference | Ref/Ptr throw NullPointerException, logged per job |
| Exceptions escaping jobs, IO handlers, destructors | JobScope catch; commons IO handlers catch; destructors noexcept and release-only |
| Off-thread refcount use | assert in checked builds; `DetachedArg` concept on BlockingPool work; `~AionConnection` posts its Ref |
| Nested job reclamation | none exist (coordinator shutdown); `runNowIfPending` is not a boundary |
| Java arithmetic semantics | existing CONVENTIONS rules |

**Remaining locks:** the executor inbox, the commons AConnection guard, `std::atomic<State>`, the DB pool, IDFactory, the ZoneName intern table, logging/RunnableStatsManager, SharedConfig values, HolderRef pointers, and watchdog atomics. None of them is held while calling game code, so no deadlock cycle involving game code is possible.

**Not guaranteed:** Java's logical check-then-act bugs across jobs (e.g. an instance handler's AtomicBoolean pattern now reads deterministically), cross-instance spawn template mutation, and the InventoryDAO UPDATED-after-failure quirk.

---

## 8. Porting mechanics (side by side)

**Porting rules at a glance**

| Java | C++ |
|---|---|
| stored object reference (field, container, capture, packet field) | `Ref<T>` (`const Ref<T>` if final) |
| param, local, return | `Ptr<T>` or `T&`; parts return `T&` (`getOwner()`) |
| `ThreadPoolManager.getInstance().schedule(this::m, d)` | `ThreadPoolManager::getInstance().schedule(this, &X::m, d)` |
| lambda capturing `this`/objects | `schedule({this, &obj}, [this, &obj] {...}, d)`; values/Refs as init-captures |
| `Future<?>` | `FutureRef`; `cancel(true)` unchanged |
| `synchronized (x) {...}` | `{...}` + `// Java: synchronized (x)` |
| `Atomic*`, CHM, COW list | same-named shims / `StableMap` |
| `new SM_X(...)` | `SM_X(...)` |
| `a.equals(b)` / `a == b` | `a->equals(*b)` / `a == b` |
| `(Npc) x` / `x instanceof Npc npc` | `cast<Npc>(x)` / `if (auto npc = as<Npc>(x))` |
| `delete()` | `delete_()` (handlers keyword rule) |

### (a) AI handler: delayed action capturing `this`, checking `isDead()` later

Java `ai/instance/abyssal_splinter/YamenessPortalSummonedAI.java:21-38`:
```java
protected void handleSpawned() {
	super.handleSpawned();
	ThreadPoolManager.getInstance().schedule(this::spawnSummons, 12000);
}
private void spawnSummons() {
	if (isDead() || !getOwner().isSpawned()) return;
	spawn(281903, getOwner().getX() + 3, getOwner().getY() - 3, getOwner().getZ(), (byte) 0);
	ThreadPoolManager.getInstance().schedule(() -> {
		if (!isDead() && getOwner().isSpawned())
			spawn(281903, getOwner().getX() + 3, getOwner().getY() - 3, getOwner().getZ(), (byte) 0);
	}, 60000);
}
```
C++:
```cpp
class YamenessPortalSummonedAI final : public AggressiveNpcAI {
public:
	using AggressiveNpcAI::AggressiveNpcAI;
protected:
	void handleSpawned() override {
		AggressiveNpcAI::handleSpawned();
		ThreadPoolManager::getInstance().schedule(this, &YamenessPortalSummonedAI::spawnSummons, 12000); // pins the Npc (AI is a RefPart)
	}
private:
	void spawnSummons() {
		if (isDead() || !getOwner().isSpawned()) return;         // deleted Npc still readable: Java semantics
		spawn(281903, getOwner().getX() + 3, getOwner().getY() - 3, getOwner().getZ(), 0);
		ThreadPoolManager::getInstance().schedule(this, [this] {
			if (!isDead() && getOwner().isSpawned())
				spawn(281903, getOwner().getX() + 3, getOwner().getY() - 3, getOwner().getZ(), 0);
		}, 60000);
	}
};
AION_AI(YamenessPortalSummonedAI, "yamenessportal");
```

Why retention beats anchoring. Java `ai/siege/IncarnateAI.java:27-35` schedules the claw's delete from a dying boss:
```cpp
void IncarnateAI::despawnClaw() {
	Ptr<Npc> claw = getPosition().getWorldMapInstance().getNpc(701237);
	ThreadPoolManager::getInstance().schedule(bindTask([](Ref<Npc> claw) { claw->getController().delete_(); }, Ref<Npc>(claw)), 60000 * 5);
	// the boss corpse decays after 2 s; the task still runs after 5 min and deletes the claw, exactly like Java
}
```
In Java, a null claw means an NPE at run time. Here `Ref` stores null and `->` throws the same logged NPE.

### (b) Handler keeping an `Npc` field, and an instance handler spawning adds

Java `ai/instance/beshmundirTemple/SacrificialSoulAI.java:18-51`:
```java
private Npc boss;
boss = getPosition().getWorldMapInstance().getNpc(216263);
if (boss != null && !boss.isDead()) { AIActions.targetCreature(this, boss); getMoveController().moveToTargetObject(); }
// handleMoveArrived (later job)
if (boss != null && !boss.isDead()) { SkillEngine.getInstance().getSkill(getOwner(), 18960, 55, boss).useNoAnimationSkill(); AIActions.deleteOwner(this); }
```
C++:
```cpp
Ref<Npc> boss;
boss = getPosition().getWorldMapInstance().getNpc(216263);                 // Ptr -> Ref
if (boss && !boss->isDead()) { AIActions::targetCreature(*this, *boss); getMoveController().moveToTargetObject(); }
// handleMoveArrived
if (boss && !boss->isDead()) {
	SkillEngine::getInstance().getSkill(getOwner(), 18960, 55, *boss)->useNoAnimationSkill();
	AIActions::deleteOwner(*this);                                         // owner is a zombie until the job ends; `this` stays valid
}
```

Java `instance/LowerUdasTempleInstance.java:41-72`:
```java
private List<Npc> traps = new ArrayList<>();
private AtomicBoolean wasSpawned = new AtomicBoolean();
public void onEnterInstance(Player player) {
	if (wasSpawned.compareAndSet(false, true)) {
		traps.add((Npc) spawn(216531, 744.7521f, 885.8238f, 152.7852f, (byte) 30));
		for (WorldPosition position : trap_positions) traps.add((Npc) spawn(216530, position.getX(), position.getY(), position.getZ(), (byte) 0));
	}
}
public void handleUseItemFinish(Player player, Npc npc) {
	for (Npc trap : traps) if (trap != null && trap.getNpcId() != 216531) trap.getController().delete();
}
```
C++:
```cpp
class LowerUdasTempleInstance final : public GeneralInstanceHandler {
	ArrayList<Ref<Npc>> traps;                  // modCount shim: a re-entrant add during iteration throws CME, as Java
	AtomicBoolean wasSpawned;                   // non-atomic shim, same API
public:
	using GeneralInstanceHandler::GeneralInstanceHandler;
	void onEnterInstance(Player& player) override {
		if (wasSpawned.compareAndSet(false, true)) {
			traps.add(cast<Npc>(spawn(216531, 744.7521f, 885.8238f, 152.7852f, 30)));
			for (const WorldPosition& position : trap_positions)
				traps.add(cast<Npc>(spawn(216530, position.getX(), position.getY(), position.getZ(), 0)));
		}
	}
	void handleUseItemFinish(Player& player, Npc& npc) override {
		for (Ptr<Npc> trap : traps)
			if (trap && trap->getNpcId() != 216531) trap->getController().delete_();
	}
};
AION_INSTANCE_HANDLER(LowerUdasTempleInstance, 300160000);
```
At `destroyInstance`: objects are deleted, `onInstanceDestroy()` runs, then `detachHandler()`. The handler (still holding the zombie traps) is freed once no task pins it. The instance is freed after that.

### (c) Effect whose effector despawns mid-DoT

Java `AbstractOverTimeEffect.java:50-56`, `PoisonEffect.java:44-50`:
```java
Future<?> task = ThreadPoolManager.getInstance().scheduleAtFixedRate(() -> onPeriodicAction(effect), initialDelay, checktime);
effect.setPeriodicTask(task, position);
public void onPeriodicAction(Effect effect) {
	Creature effected = effect.getEffected();
	effected.getController().onAttack(effect, TYPE.DAMAGE, effect.getReserveds(position).getValue(), false, LOG.POISON, hopType, effect.isMagicalCritical(position));
	effected.getObserveController().notifyDotAttackedObservers(effect.getEffector(), effect);
}
```
C++:
```cpp
class Effect final : public RefCounted, public StatOwner {
	const Ref<Creature> effector;               // deleted summon/npc stays readable until the effect ends
	const Ref<Creature> effected;
	const SkillTemplate* skillTemplate;         // immortal
	FutureRef endTask;
	std::array<FutureRef, 4> periodicTasks;     // Java Future<?>[], set in place (Effect.java:246-252)
	FutureRef periodicActionsTask;
	int32_t tempHate = 0;                       // was HostileUpEffect.tempHate (DEVIATION)
	...
};
void AbstractOverTimeEffect::startEffect(Effect& effect, std::optional<AbnormalState> abnormal) const {
	int64_t initialDelay = 300 + checktime;
	FutureRef task = ThreadPoolManager::getInstance().scheduleAtFixedRate({this, &effect}, [this, &effect] { onPeriodicAction(effect); },
		initialDelay, checktime);               // `this` immortal (no-op pin), effect retained
	effect.setPeriodicTask(std::move(task), position);
}
void PoisonEffect::onPeriodicAction(Effect& effect) const {
	Ptr<Creature> effected = effect.getEffected();
	effected->getController().onAttack(effect, TYPE::DAMAGE, effect.getReserveds(position)->getValue(), false, LOG::POISON, hopType,
		effect.isMagicalCritical(position));
	effected->getObserveController().notifyDotAttackedObservers(effect.getEffector(), effect);
}
// NpcController::onAttack keeps: if (as<Summon>(attacker) && attacker->isSpawned()) actingCreature = attacker; else actingCreature = attacker->getActingCreature();
```
Sequence:
1. The effector is released or deleted. Only its own effects end.
2. The DoT keeps ticking; the stale Summon answers `isSpawned()==false`, and `getActingCreature()` returns the master (Java behaviour).
3. `endEffect` cancels the periodic task, which releases the Effect pin.
4. The EffectController removes the Effect.
5. At the end of that job the Effect, then the Summon, are freed, and the Summon's ID enters quarantine.

### (d) Group keeping a logged-out player

Java `PlayerTeamMember.java:8`, `PlayerGroupService.OfflinePlayerChecker`, `PlayerConnectedEvent`:
```java
final Player player;
group.forEachTeamMember(member -> { if (!member.isOnline() && TimeUtil.isExpired(member.getLastOnlineTime() + GroupConfig.GROUP_REMOVE_TIME * 1000))
	group.onEvent(new PlayerGroupLeavedEvent(group, member.getObject(), LeaveReson.LEAVE_TIMEOUT)); });
group.removeMember(player.getObjectId());
group.addMember(new PlayerGroupMember(player));
if (player.equals(group.getLeader().getObject())) { ... }
```
C++:
```cpp
class PlayerTeamMember : public RefCounted, public TeamMember<Player> {
	const Ref<Player> player;                   // logged-out Player stays readable up to 600 s
	int64_t lastOnlineTime = 0;
public:
	explicit PlayerTeamMember(Player& player) : player(player) {}
	Player& getObject() const { return *player; }
	bool isOnline() const { return player->isOnline(); }
};
struct OfflinePlayerChecker : TaskStruct {     // started with scheduleAtFixedRate(OfflinePlayerChecker{}, 1000, 30000)
	void operator()() const {
		PlayerGroupService::groups.forEach([](int32_t, Ptr<PlayerGroup> group) {
			group->forEachTeamMember([&](Ptr<PlayerGroupMember> member) {   // StableMap: removal inside the callback is safe
				if (!member->isOnline() && TimeUtil::isExpired(member->getLastOnlineTime() + GroupConfig::GROUP_REMOVE_TIME * 1000))
					group->onEvent(PlayerGroupLeavedEvent(*group, member->getObject(), LeaveReson::LEAVE_TIMEOUT));
			});
		});
	}
};
void PlayerConnectedEvent::handleEvent() {
	group.removeMember(player->getObjectId());   // onRemoveMember: old instance ->setPlayerGroup(nullptr); old Player freed at job end if unreferenced
	group.addMember(makeRef<PlayerGroupMember>(*player));
	if (player->equals(*group.getLeader()->getObject())) { ... }
}
```

### (e) `PacketSendUtility.broadcastPacket` of an SM_ packet

Java `ai/ActionItemNpcAI.java:64`, `PacketSendUtility.java:83-97`:
```java
PacketSendUtility.broadcastPacket(player, new SM_EMOTION(player, EmotionType.START_QUESTLOOT, 0, getObjectId()), true);
public static void broadcastPacket(VisibleObject object, AionServerPacket packet) { object.getKnownList().forEachPlayer(player -> sendPacket(player, packet)); }
```
C++:
```cpp
PacketSendUtility::broadcastPacket(player, SM_EMOTION(player, EmotionType::START_QUESTLOOT, 0, getObjectId()), true);

template <std::derived_from<AionServerPacket> P>
void PacketSendUtility::broadcastPacket(Player& player, P&& packet, bool toSelf) {
	auto shared = std::make_shared<std::decay_t<P>>(std::forward<P>(packet));   // SM_EMOTION stores Ref<Creature>
	uint64_t group = Outbox::newGroup();                                         // serialized once at flush unless dependsOnConnection()
	if (toSelf) sendTo(player, shared, group);
	player.getKnownList().forEachPlayer([&](Ptr<Player> other) { sendTo(*other, shared, group); });
}
void PacketSendUtility::sendTo(Player& player, const std::shared_ptr<AionServerPacket>& packet, uint64_t group) {
	if (const auto& con = player.getClientConnection())                          // logic-only field: no TOCTOU
		GameExecutor::getInstance().outbox().push(con, packet, group);
}
```

### (f) DAO save: periodic and at logout

Java `PlayerEnterWorldService.java:367-371, 496-521`:
```java
player.getController().addTask(TaskId.PLAYER_UPDATE, ThreadPoolManager.getInstance().scheduleAtFixedRate(
	new GeneralUpdateTask(player.getObjectId()), PeriodicSaveConfig.PLAYER_GENERAL * 1000, PeriodicSaveConfig.PLAYER_GENERAL * 1000));
public void run() {
	Player player = World.getInstance().getPlayer(playerId);
	if (player != null) { try { AbyssRankDAO.storeAbyssRank(player); PlayerSkillListDAO.storeSkills(player); PlayerQuestListDAO.store(player);
		PlayerDAO.storePlayer(player); for (House house : player.getHouses()) house.save();
	} catch (Exception ex) { log.error("Exception during periodic saving of player " + player.getName(), ex); } }
}
```
C++:
```cpp
player.getController().addTask(TaskId::PLAYER_UPDATE, ThreadPoolManager::getInstance().scheduleAtFixedRate(
	GeneralUpdateTask{.playerId = player.getObjectId()}, PeriodicSaveConfig::PLAYER_GENERAL * 1000, PeriodicSaveConfig::PLAYER_GENERAL * 1000));

struct GeneralUpdateTask : TaskStruct {            // captures only the id, like Java
	int32_t playerId;
	void operator()() const {
		Ptr<Player> player = World::getInstance().getPlayer(playerId);
		if (player) {
			try {
				AbyssRankDAO::storeAbyssRank(*player); PlayerSkillListDAO::storeSkills(*player); PlayerQuestListDAO::store(*player);
				PlayerDAO::storePlayer(*player);
				for (Ptr<House> house : player->getHouses()) house->save();
			} catch (...) { log.errorCurrentException("Exception during periodic saving of player " + player->getName()); }
		}
	}
};
// PlayerLeaveWorldService::leaveWorld (PlayerLeaveWorldService.java:55-155): line by line on logic; delete_() then storePlayer works
// because the Player is a zombie until the job ends; PlayerDAO::onlinePlayer(player, false) still gates relogin; con->setActivePlayer(nullptr).
```
The save runs inline on logic. There is no race with gameplay, and the stall is to be measured (prototype 5).

### (g) `CreatureController.addTask(TaskId, schedule(...))`, cancel on despawn, teleport run-now

Java `CreatureController.java:400-427`, `PlayerLeaveWorldService.java:52-55`, `CM_TELEPORT_ANIMATION_DONE.java:34-48`:
```java
tasks.compute(taskId.ordinal(), (k, oldTask) -> { if (oldTask != null) { oldTask.cancel(false); if (taskId == TaskId.DESPAWN) log.warn(...); } return task; });
Future<?> leaveWorldTask = ThreadPoolManager.getInstance().schedule(() -> leaveWorld(player), delayInMillis);
player.getController().addTask(TaskId.DESPAWN, leaveWorldTask);
Future<?> task = player.getController().getAndRemoveTask(TaskId.TELEPORT);
if (task instanceof RunnableFuture && !task.isDone()) try { spawnTask.run(); spawnTask.get(); } catch (InterruptedException | ExecutionException e) { ... }
```
C++:
```cpp
std::array<FutureRef, magic_enum::enum_count<TaskId>()> tasks;   // Java CHM<Integer, Future<?>>
void CreatureController::addTask(TaskId taskId, FutureRef task) {
	FutureRef old = std::exchange(tasks[std::to_underlying(taskId)], std::move(task));
	if (old) {
		old->cancel(false);
		if (taskId == TaskId::DESPAWN) log.warn("Despawn task for {} was cancelled and replaced with another one, possibly delaying the intended despawn time.", getOwner());
	}
}
FutureRef CreatureController::getAndRemoveTask(TaskId id) { return std::exchange(tasks[std::to_underlying(id)], nullptr); }
void CreatureController::cancelAllTasks() { for (FutureRef& t : tasks) if (FutureRef f = std::exchange(t, nullptr)) f->cancel(false); }
void CreatureController::onDelete() { cancelAllTasks(); VisibleObjectController::onDelete(); }

void PlayerLeaveWorldService::leaveWorldDelayed(Player& player, int64_t delayInMillis) {
	FutureRef leaveWorldTask = ThreadPoolManager::getInstance().schedule(&player, [&player] { leaveWorld(player); }, delayInMillis);
	player.getController().addTask(TaskId::DESPAWN, std::move(leaveWorldTask));
}

void CM_TELEPORT_ANIMATION_DONE::runImpl() {
	Ptr<Player> player = getConnection()->getActivePlayer();
	FutureRef task = player->getController().getAndRemoveTask(TaskId::TELEPORT);
	if (task && !task->isDone())
		try {
			task->runNowIfPending();            // deferred SpawnTask (TeleportService.java:191) or scheduled (PvPZone.java:45, PlayerReviveService.java:251)
		} catch (const ExecutionException& e) { // only a deferred task rethrows (scheduled ones are RunnableWrapper-caught, as in Java)
			log.error("", e.getCause());
			if (!player->isSpawned()) { PacketSendUtility::sendPacket(*player, SM_PLAYER_INFO(*player)); World::getInstance().spawn(*player); }
		}
}
// TeleportService::sendLoc: player.getController().addTask(TaskId::TELEPORT, Future::deferred(&player, SpawnTask(player, ...)));
// DropService: if (FutureRef decayTask = npc->getController().cancelTask(TaskId::DECAY)) dropNpc->setRemaingDecayTime(decayTask->getDelay());
```

### (h) MoveTaskManager periodic NPC movement

Java `MoveTaskManager.java:39-55` (verified: MOVE_ARRIVED → WalkManager → `NpcController.onStartMove:311` → `addCreature` during iteration):
```java
movingCreatures.values().parallelStream().forEach(creature -> {
	if (!creature.isSpawned()) { if (removeCreature(creature)) log.warn(...); return; }
	creature.getMoveController().moveToDestination();
	if (creature.getAi().isDestinationReached()) { removeCreature(creature); creature.getAi().onGeneralEvent(AIEventType.MOVE_ARRIVED); ZoneUpdateService.getInstance().add(creature); }
	else creature.getAi().onGeneralEvent(AIEventType.MOVE_VALIDATE);
});
```
C++:
```cpp
class MoveTaskManager final : public AbstractPeriodicTaskManager {  // start() from GameServer::main, fixed rate 200 ms
	StableMap<int32_t, Ref<Creature>> movingCreatures;
public:
	void run() override {                   // Deviation: sequential, insertion order
		movingCreatures.forEach([this](int32_t, Ptr<Creature> creature) {        // by-value Ptr: re-add can't invalidate it
			if (!creature->isSpawned()) {
				if (removeCreature(*creature)) log.warn("{} was still in moving creatures list but already despawned", *creature);
				return;
			}
			creature->getMoveController().moveToDestination();
			if (creature->getAi().isDestinationReached()) {
				removeCreature(*creature);                                       // tombstone
				creature->getAi().onGeneralEvent(AIEventType::MOVE_ARRIVED);     // may re-add the same creature: new slot, not visited this tick
				ZoneUpdateService::getInstance().add(*creature);
			} else {
				creature->getAi().onGeneralEvent(AIEventType::MOVE_VALIDATE);
			}
		});
	}
};
```
Re-adding the same key while its old slot is a tombstone creates a new dense slot. The visit bound is fixed at the start of `forEach`, so the new slot is visited on the next tick (Java's CHM iteration may or may not visit it).

### How mechanical is it

**Schedule sites (684 in handlers, script-classified by the handles proposal)**

| Share | Sites | Change per site |
|---|---|---|
| ~82% | ~563 | capture only `this`: add `this,` as the pin, `this::m` → `this, &X::m` (regex-assisted) |
| ~17% | ~115 | also capture objects (quests: player/env): `{this, &player}` pins or `bindTask`, plus init-captured values |
| ~40-50 sites | | self-cancel holders, stateful anonymous Runnables → `TaskStruct`; no scope decisions needed |
| 2 sites | | rewrites: FixPath continuation, teleport `deferred` + `runNowIfPending` |

Source (src) sites follow the same split: ~170 sites plus ~100 delayed `broadcastMessage` calls.

**Handlers**
- Design-specific edits are about 3% of handler lines: pins, `Future<?>` → `FutureRef` (~260 declarations), Ref for fields.
- `synchronized`, `Atomic*` and concurrent collections port by deletion or rename.
- 1,035 quests are barely affected (39 schedule sites).
- writeImpl, DAO and quest bodies port without design edits.

---

## 9. Performance, debuggability, foundation effort

### 9.1 Performance (estimates, to be measured in prototypes)

- **Objects:** 60-100k spawned at startup. At 1.5-3 KB each, NPCs take ~150-300 MB. Static data is 300-500 MB (decision B), geo ~300 MB. Total about 1-1.2 GB.
- **CPU:** idle is near zero, because AI think and NpcKnownList work are gated by `isMapRegionActive`. A movement tick with up to ~2,000 movers near a few players costs about 10-40 ms of one core per 200 ms.
- **Hot-path primitives:** refcount inc/dec ~1 ns (plus the thread-id compare in checked builds); timer operations O(log n) at ~50k timers; each broadcast serialized once.
- **Latency:** DB stalls are the main cost. Enter world ~10-40 ms, logout save ~5-30 ms, periodic saves every 900 s. All are invisible at a few players; measured in prototype 5.
- **Startup:** parallel XML parse and BIH build; binding and `spawnAll` serial on main, expected to take seconds.
- **Fallbacks if measured necessary:** a two-phase parallel movement tick (parallel compute on read-only data, serial commit) and row-offloaded loaders.

### 9.2 Debuggability

- **Determinism:** one thread, `(due, seq)` timer order, seeded `Rnd`, ManualClock with `runReady/advance`. Inbox recording and replay can be added later.
- **Readable failures:**
  - complete stacks with no pool hops;
  - NPE per job with `std::stacktrace`;
  - slow-job warnings and stats keyed by `source_location`;
  - watchdog stall dump;
  - Windows minidumps from the unhandled-exception filter.
- **Introspection:** `//tasks <target>`, `//debug leaks`, `//debug refs` (live and zombie counts per class), inbox length and job-time percentiles in `SystemInfo`.
- **Caveat:** a breakpoint freezes the world. The watchdog is disabled under a debugger and the debug config relaxes timeouts.

### 9.3 Foundation effort (C++ lines incl. tests, one experienced developer)

| Component | Lines |
|---|---|
| GameExecutor (loop, inbox, 4-ary heap, ManualClock, JobScope, stats, Windows timers), watchdog | 1,700 |
| Future state machine, ThreadPoolManager facade, Pin, bindTask/TaskStruct concepts, BlockingPool + continuations | 1,300 |
| RefCounted/RefPart/Ref/Ptr (stamps), Reclaimer, LeakCensus, thread asserts | 1,000 |
| StableMap/StableSet, ArrayList/HashMap CME shims, Atomic*/COW shims, CollectionUtil | 1,000 |
| CronExpression + CronService + CronServiceTest vectors | 1,200 |
| Outbox, AionServerPacket serialize/buf, WirePacket, SM_KEY strand path, connection-dependence check, game Crypt, PacketSendUtility core | 1,400 |
| IDFactory with quarantine | 350 |
| ShutdownCoordinator, console handler, `lint_tasks.py` | 600 |
| **Total** | **~8.5k lines, ~4-6 focused weeks.** It belongs to chunk P4-02 (runtime) and must be done before the S0b spine headers freeze |

---

## 10. Risks and what to prototype first

| Risk | Impact | Mitigation |
|---|---|---|
| Single-core ceiling (siege, world raid) | stutter | prototype 3 profile; two-phase movement fallback; per-instance executors out of scope (would need atomic refs) |
| Inline DB stalls | hitches | prototype 5; pre-warmed pool; row-offloaded loaders only where measured |
| Refcount cycles GC used to collect (handler back-references, aborted despawns) | memory and ID leaks | Java hooks, handler detach, per-port field checklist, LeakCensus in CI scenarios and at runtime |
| Discipline drift (unpinned `&x` capture, stored Ptr, raw pointers) | late use-after-free | compile-time unpinned check, lint in CI, Ptr stamps and asserts in checked builds, ASan scenario runs |
| A fault in any handler takes down the world | lost session state | NPE semantics, CME shims, `.at()` in helpers, minidumps, periodic saves |
| Timing deviations (end-of-job sampling, execute after job, sequential movement, earlier capture release, ordered LS/CS, quarantine) | subtle behaviour changes | DEVIATIONS entries, `immediate` switch, real-client checkpoints |
| Leak-on-reload | memory growth per `//reload` | logged; acceptable for development use |

**Prototype order, each with a pass criterion**
1. **Kernel conformance** (ManualClock, ASan):
   - Future: cancel before and while running, self-cancel, `getDelay` after cancel, deferred vs scheduled `runNowIfPending` exception behaviour, fixed-rate catch-up, periodic task surviving an exception, shutdown dropping delayed tasks.
   - Ref, Reclaimer, zombie resurrection and Ptr stamps.
   - StableMap re-entrant put/remove, including the MoveTaskManager walker re-add regression.

   Pass: all green under ASan, zero census leaks.
2. **Runtime microbenchmark:** 80k Npc shells over a region grid with KnownLists, 2,000 movers every 200 ms, synthetic AI events and SM_MOVE broadcasts to 5 fake connections. Pass: tick p99 < 50 ms in RelWithDebInfo checked build; record memory per Npc and reclaim cost.
3. **Handler vertical slice with ManualClock:** CreatureController tasks, YamenessPortalSummonedAI, IncarnateAI (5-min claw delete survives corpse decay), SacrificialSoulAI, LowerUdasTempleInstance; despawn mid-delay, `destroyInstance` with a pending 60 s task, 50 instance create/destroy cycles. Pass: no ASan errors, IDs released only after tasks end, memory back to baseline, census zero.
4. **Packet path against the real 4.8 client:** SM_KEY special frame, deduplicated broadcast, per-recipient SM_PLAYER_INFO, SM_PLAY_MOVIE mutation at flush, `close(SM_QUIT_RESPONSE)` after other packets in the same job, a deliberately misflagged packet tripping the connection-dependence check.
5. **DB-inline stall:** `PlayerService::getPlayer` + `storePlayer` for a character with full inventory, warehouse and quests on local MariaDB. Record p50/p99 job time; decide on offloading.
6. **Relogin + group:** logout in a group, relogin within 600 s, and another run with the timeout firing. Check the swap, the `World::removeObject` identity check, and that the old instance is freed.
7. **Effect with a despawned effector:** a summon's DoT, summon released mid-DoT. Check `onAttack` acting creature, reclaim after the effect ends, ID quarantine.
8. **Shutdown coordinator** with 5 connected fake clients: every `leaveWorld` runs, saves complete, no nested jobs, exit code correct.
9. **Porting-cost calibration:** 20 AI and 10 instance files with `lint_tasks.py` enabled. Measure minutes per file and lint false positives before batching phase 6.


## Red team review: problems found and required amendments

These findings are accepted as amendments to the design above. Each must be addressed when the corresponding part is implemented.

The single logic thread, pins, outbox and coordinator design survives the thread-safety attacks. The real breakage comes from GC semantics that the port inherits without Java's collector.

**Refcount cycles the design never lists.** Its cycle breakers are the existing Java hooks, but several cycles have no hook in Java at all:
- **Alliance:** the alliance and its groups reference each other, and nothing clears the leader. Every disbanded alliance leaks its last leader's whole Player.
- **Target:** a self-target, or a target outside the KnownList, is never cleared on logout or delete.
- **Kisk:** the Kisk creator and `player.kisk` reference each other.
- **Parts inside one owner:** a Ref stored inside the same owner keeps that owner alive forever. MapRegion's neighbour list is the example: every destroyed instance leaks.

**Account ownership is missing.** Account, PlayerAccountData and PlayerCommonData have no owner in the design. A stale team member then reads freed memory, and account-warehouse Items are released on an IO thread.

**Threading holes:**
- Shutdown calls NioServer::shutdown on the coordinator, which runs onServerClose and leaveWorld off the logic thread.
- AionConnection's constructor schedules a task, sendPacketInfo sends, and toString reads state, all on IO threads.
- `//reload events` builds SpawnGroups on the BlockingPool.

**Behaviour change from end-of-job packets.** SM_PLAYER_INFO is written after CM_LEVEL_READY and SpawnTask reset the arrival animation to NONE in the same job, so teleport animations break on every teleport.

All of these fit the design with local fixes:
- non-retaining owner pointers for references inside one owner;
- a few C++-only cycle breakers, listed in DEVIATIONS;
- Account as RefCounted;
- posting connection work to logic;
- binding on logic for spawn-building XML;
- a per-class `serializeOnSend` trait with a scripted audit of the lazy packets.

Add each scenario to the LeakCensus and prototype suites (3, 4, 6 and 8). The minor findings are about classification and robustness:
- per-run service objects must not be marked Immortal;
- retired quest handlers must never be freed;
- by-copy Ptr captures are not linted, and a failed Ptr stamp should throw, not terminate;
- StableMap iteration depth must unwind correctly when a callback throws;
- fixed-rate catch-up after a debugger pause should be coalesced.

### RT-1 [serious]

**Scenario:** Stale logged-out Player outlives its Account. Player.java:88-89 and 190-194 hold `final PlayerAccountData playerAccountData` and `final Account playerAccount`, both taken from the connection. getCommonData() (Player.java:239-240) and getName() read through PlayerAccountData to a PlayerCommonData that the Account owns. The Account is loaded by AccountService.loadAccount (AccountService.java:75-81, including the account warehouse Storage full of Items) and attached with AionConnection.setAccount (LoginServer.java:167). The design's ownership table (section 2.2/2.3) has no row for Account, PlayerAccountData, PlayerCommonData or the account-warehouse Storage. The only rule for connection state is that ~AionConnection posts `activePlayer`. Sequence: player P is in a group, the client crashes, leaveWorldDelayed runs, the group keeps P for 600 s, the connection's last shared_ptr drops (often on an IO strand handler), and ~AionConnection destroys the Account. The next OfflinePlayerChecker tick or PlayerGroupStats.updateMinMaxLevelPlayers (PlayerGroupStats.java:37-50, `player.getCommonData().getExp()` over all members, offline ones included) then reads freed memory. The same destructor releases the account-warehouse Items' Refs on the IO thread.

**What breaks:** Use-after-free on stale team members, which the design explicitly promises stay readable (section 8d). Refcount operations on RefCounted Items happen off the logic thread: terminate in checked builds, a race with the Reclaimer in release builds.

**Fix:** Classify Account as RefCounted. PlayerAccountData and the account Storage become RefParts of Account, and PlayerCommonData a RefPart of its PlayerAccountData. Player holds `const Ref<Account>` and `const Ref<PlayerAccountData>`. AionConnection::account becomes a logic-only `Ref<Account>`, and ~AionConnection posts it together with activePlayer, e.g. `[a = std::move(account), p = std::move(activePlayer)]{}`. Add a relogin/crash-in-group scenario to prototype 6 that frees the connection before the 600 s timeout.

### RT-2 [serious]

**Scenario:** Porting rule 'stored object reference -> Ref<T>' combined with RefPart forwarding makes an owner retain itself. MapRegion (a RefPart of WorldMapInstance) has `MapRegion[] neighboursIncludingSelf = { this }` plus addNeighbourRegion (MapRegion.java:29,58-61). Ported as Ref<MapRegion>, every entry adds a count to the owning WorldMapInstance. The same happens with `ZoneInstance[] zonesSortedByTypeAndPriority` if zone instances are parts, with SpawnGroup.poolUsedTemplates `Map<Integer, Set<SpawnTemplate>>` (SpawnGroup.java:163-189, where Ref<SpawnTemplate> forwards to its own group), and with part->owner fields such as AbstractAI.owner, KnownList.owner and AggroList.owner. The lint rejects raw T* fields of RefCounted types, which pushes porters towards Ref. Sequence: InstanceService.destroyInstance (InstanceService.java:92-106) deletes all objects and detaches the handler, but the instance's refcount never reaches 0.

**What breaks:** Every destroyed instance leaks for good: regions, zone instances, per-instance door/geo state, and any stale objects in region maps. The design's 'instance freed after the handler' promise (section 8b) and prototype 3's 'memory back to baseline' fail. For Creatures, a Ref-typed AI or owner field would make every Npc immortal and never release its ID.

**Fix:** Add a non-retaining `OwnerPtr<T>` (or `T&`) for references that stay inside one ownership unit: part->owner, part->sibling part, owner->own part, and group->own template. Make the lint require it whenever holder and target share a refOwner. In checked builds, assert in `Ref(T*)` when the target's refOwner() is the object currently being constructed or iterated. Add a census check right after destroyInstance: the WorldMapInstance refcount must be at most 1 plus the task pins.

### RT-3 [serious]

**Scenario:** Alliance cycle retains the last leader. PlayerAlliance.groups (PlayerAlliance.java:20,26-33) holds 4 PlayerAllianceGroup objects, and each has `final PlayerAlliance alliance` (PlayerAllianceGroup.java:11-16). Both are AionObject/TemporaryPlayerTeam, so the design makes them RefCounted with `const Ref` fields. No Java code clears either side; Java relied on GC. On disband (PlayerAllianceService.java:186-195 -> AllianceDisbandEvent -> PlayerAllianceLeavedEvent per member) the members are removed, but GeneralTeam.leader (GeneralTeam.java:31) is never nulled.

**What breaks:** Every alliance ever formed forms a permanent refcount cycle. It keeps the alliance ID (autoRelease=true), its 4 groups, and through `leader` the last leader's PlayerAllianceMember and complete Player object (inventory Items, quest states, account data) alive after logout. The ported cycle-breaker hooks cannot fix this, because Java has none.

**Fix:** Model PlayerAllianceGroup as an owner-forwarding part of PlayerAlliance, with a non-retaining back pointer, so that Player.playerAllianceGroup Refs keep the alliance alive. Alternatively, add a C++-only breaker in the disband path that clears `groups` and `leader` after the last member leaves, and list it in DEVIATIONS. Add 'form alliance, disband, logout' to the LeakCensus scenario tests.

### RT-4 [serious]

**Scenario:** Self-target and out-of-sight target cycles. VisibleObject.target becomes Ref<VisibleObject>, and the design names notSee as its breaker. notSee runs only for objects in the KnownList (CreatureController.java:70-74), and an object is never in its own KnownList. Players target themselves (CM_TARGET_SELECT.java:57 `newTarget = player`) and can target team members who are not in their KnownList (CM_TARGET_SELECT.java:61). NPCs target themselves for every FirstTarget=ME skill (SkillAttackManager.java:80). Nothing in PlayerLeaveWorldService.leaveWorld (PlayerLeaveWorldService.java:62-155), CreatureController.onDespawn (547-557) or onDelete clears the target. DiedEventHandler.java:22 clears it only on death.

**What breaks:** A player who logs out while self-targeted, which is common after self-buffing, never frees the Player object or its Items: a self-cycle. Two players targeting each other from outside their KnownLists leak each other. NPCs deleted without dying (342 delete() calls in handlers, instance destroy, event end) right after a self-buff leak the Npc and its ID. Every logout grows memory.

**Fix:** Store a self-target as a flag (`targetIsSelf`), not a Ref. getTarget() returns the object itself when the flag is set, so Java semantics stay exact. Add a C++-only `setTarget(nullptr)` at the end of leaveWorld and in VisibleObjectController::onDelete for cross-object targets, listed in DEVIATIONS. Add 'logout while self-targeted' and 'delete a self-buffed npc' to the census scenarios.

### RT-5 [serious]

**Scenario:** Kisk creator cycle. ToyPetSpawnAction.java:130 binds the creator to their own kisk, so Kisk.addPlayer sets `player.setKisk(this)` (Kisk.java:168-175). SummonedObject.creator is `final T creator` (SummonedObject.java:22), which becomes `const Ref<Player>`. When the creator logs out, KiskService.onLogout (KiskService.java:74-80) only records the binding by id and does not clear the old instance's kisk field. When the kisk expires, removeKisk clears `setKisk(null)` only for currently online members, resolved by id through World (KiskService.java:47-52 and Kisk.getCurrentMemberList).

**What breaks:** Stale Player P1 (kisk=K) and Kisk K (creator=P1) form a permanent cycle whenever a creator logs out before the kisk ends, which is the normal case since kisks last hours. The Player graph and the Kisk Npc leak, and the Kisk's auto-release ID is never returned.

**Fix:** At the end of leaveWorld, after KiskService.onLogout has stored the binding, add a C++-only `player.setKisk(nullptr)`. onLogin re-establishes it through kisk.addPlayer, so behaviour is unchanged. Also audit every `final X creator/owner/master` field whose target holds a field pointing back, and add a census scenario for it.

### RT-6 [serious]

**Scenario:** End-of-job sampling deterministically changes a lazy packet's content. CM_LEVEL_READY.runImpl sends `new SM_PLAYER_INFO(activePlayer)` (CM_LEVEL_READY.java:52), then runs World.spawn, siege, rift and quest updates, and at :103 calls `activePlayer.setPortAnimation(ArrivalAnimation.NONE)` in the same job. SM_PLAYER_INFO.writeImpl reads `player.getPortAnimationId()` at write time (SM_PLAYER_INFO.java:174). TeleportService's SpawnTask sets the arrival animation (TeleportService.java:523), and for same-map teleports spawnOnSameMap sends SM_PLAYER_INFO and resets to NONE in the same call (TeleportService.java:208-218). In Java the NIO thread writes SM_PLAYER_INFO while the long runImpl continues, so the arrival animation is sent. With the outbox, the packet is always serialized after the reset.

**What breaks:** Every teleport, portal, revive-landing and fly-in arrival animation is replaced by NONE for the teleporting player. This is a systematic visible regression, and it disproves the assumption behind 'two sends in one job both show the final state; usually equal to Java'. Only a global `immediate` switch exists as a workaround.

**Fix:** Add a per-class `static constexpr bool serializeOnSend` trait. The outbox serializes such packets inside sendPacket (on logic, still ordered) and keeps end-of-job serialization for the others. Mark SM_PLAYER_INFO. Script an audit over the 63 lazy packets: for each send site, flag setters of fields read by that writeImpl that run later in the same method or call chain. Add prototype 4 checks for teleport arrival animations.

### RT-7 [serious]

**Scenario:** Shutdown runs leaveWorld on the coordinator thread. The design's step 3 calls NioServer::shutdown() on the ShutdownCoordinator thread. Commons NioServer::shutdown calls `connection->onServerClose()` on the calling thread (NioServer.h shutdown doc; NioServer.cpp:389). The Java body being ported, AionConnection.onServerClose (AionConnection.java:264-266), does `close(); safeLogout();`, and safeLogout calls PlayerLeaveWorldService.leaveWorld synchronously. That happens on the coordinator thread while the logic loop is concurrently running posted onDisconnect jobs of other connections (AionConnection.java:241, isShuttingDownSoon -> safeLogout).

**What breaks:** All player saves at shutdown run off the logic thread. Checked builds hit the refcount/World asserts and terminate mid-shutdown without saving. Release builds get data races (World maps, KnownLists, DAOs) between two threads doing leaveWorld at once: corrupted saves or crashes exactly when state must be persisted.

**Fix:** Before calling NioServer::shutdown, the coordinator posts one job that runs Java's onServerClose body (`close(); safeLogout();`) for every active AionConnection, and waits on its promise. AionConnection::onServerClose is overridden to post the same body and skip it if already done. Only then does it call NioServer::shutdown and post the PeriodicSaveService/CronService/ThreadPoolManager job. Extend prototype 8 to assert that no leaveWorld runs off logic (isLogicThread assert inside leaveWorld).

### RT-8 [serious]

**Scenario:** AionConnection code that runs on IO threads touches logic-only runtime state. (1) The AionConnection constructor (AionConnection.java:117, 402-406) runs in the ConnectionFactory on an IO thread (commons NioServer.h: 'ConnectionFactory, initialized()... runs on IO thread'). It creates ConnectionAliveChecker, which calls ThreadPoolManager.scheduleAtFixedRate. The design asserts the scheduler thread in release builds and forbids creating RefCounted objects (Future) off logic. (2) processData calls sendPacketInfo(pck) (AionConnection.java:189, 225-232), which reads getAccount().getMembership() and pushes an SM_MESSAGE, a connection-dependent packet, from the IO thread, and outbox push is logic-asserted. (3) toString (AionConnection.java:393-395) reads activePlayer and the Account. It is used in IO-thread logs (decrypt failure, fake packet, flooding) and in commons' own IO and shutdown log messages.

**What breaks:** (1) The server terminates on the first client connection, in every build. (2) It terminates when a membership-10 account has packet info enabled. (3) Ref/Player reads race with logic, and can be use-after-free if the Player is reclaimed while an IO thread formats a log line.

**Fix:** Start ConnectionAliveChecker from a job posted in initialized(), keep its FutureRef in a logic-only field, and use bindTask with weak_ptr<AionConnection>. Move sendPacketInfo into the posted logic job before `pck->run()`, and do the outgoing-side call during flush. Give AionConnection an atomically swapped immutable `std::shared_ptr<const std::string>` description (account name, player name), updated on logic, for toString. Extend the release assert on the IO path to anything that reaches Ref, the scheduler or the outbox from AionConnection.

### RT-9 [serious]

**Scenario:** `//reload events` builds RefCounted runtime spawn objects on the BlockingPool. EventTemplate contains a nested `SpawnsData spawns` element (EventTemplate.java:53-54). SpawnsData.afterUnmarshal (SpawnsData.java:68-99) runs addRegularSpawns etc., which call `new SpawnGroup(map.getMapId(), spawn)` and mutate the index maps. Reload.java:87-93 re-parses timed_events and calls EVENT_DATA.setEvents. Per sections 6.2 and 6.4, reload parsing and post-processing run on the BlockingPool and 'build no RefCounted objects', and SpawnsData maps are logic-only StableMaps with asserts. That contradicts the XML hook.

**What breaks:** `//reload events` creates SpawnGroups/SpawnTemplates (RefCounted, non-atomic counts) and StableMap entries off the logic thread. Checked builds terminate the server; release builds get refcount races once the new holder is published while events start on logic.

**Fix:** For holders whose hooks build runtime spawn objects (EventData via nested SpawnsData, and SpawnsData itself), parse the DOM on the BlockingPool but post the bind and hooks to a logic job. Alternatively, have the hook record SpawnMap references and build SpawnGroups in a logic-side post-processing step before publish. The static-data holder registry should mark such holders as `bindOnLogic`.

### RT-10 [minor]

**Scenario:** Per-run service objects that capture `this` in tasks are neither singletons nor listed as RefCounted. BaseService creates `new CasualBase/StainedBase/SiegeBase/PanesterraBase(loc)` for each capture cycle (BaseService.java:103-106). FortressSiege schedules lambdas capturing `this` (FortressSiege.java:87). WorldRaid holds preparationTask and anonymous Runnables (WorldRaid.java:63-109). Event.java, Assault/FortressAssault and AhserionRaid follow the same pattern. Section 2.2 lists only 'service singletons' as Immortal and does not classify these, while Pin needs RefCounted, RefPart or Immortal.

**What breaks:** A porter who marks Siege, Base, WorldRaid or Event as Immortal to satisfy Pin gets a use-after-free when a siege or base cycle ends and the object is dropped while a 10-30 minute task is still pending.

**Fix:** Add these classes to the RefCounted list (Siege and subclasses, Assault, Base, WorldRaid, Event, AhserionRaid, Invasion, AgentFight, RiftManager spawns). In checked builds, make `Immortal` verifiable: require a static-storage instance, or register the address in a startup set and assert on Pin.

### RT-11 [minor]

**Scenario:** `//reload quests` calls QuestEngine.reload (QuestEngine.java:112-116), which in Java shuts down the script manager and re-instantiates every handler. The design keeps AbstractQuestHandler singletons as Immortal, so the 39 quest schedule sites capture `this` without a pin, and it says reload 'retires the old quest handlers'.

**What breaks:** If 'retire' means destroy, any pending quest task (for example an escort or timer lambda capturing the handler) runs on a freed handler after the reload.

**Fix:** Define retirement as 'moved to a never-freed list', like old static-data holders, and say so in DEVIATIONS. Otherwise make quest handlers RefCounted and pin them.

### RT-12 [minor]

**Scenario:** Java pattern `Player player = env.getPlayer(); schedule(() -> ... player ..., d)`. Ported as `Ptr<Player> player = ...; schedule(this, [this, player]{...}, d)`, this compiles: Pin covers only `this`, and the lint checks `&name` captures and `[&]`/`[=]`, not by-copy captures of Ptr locals. It affects about 115 handler sites (17%) that capture objects. The Ptr stamp fires only when the task actually runs in a checked build, and the design makes that an assert (terminate).

**What breaks:** A release build gets a silent use-after-free if the Player is reclaimed before the task runs. A checked build, which is the build the user actually runs, loses the whole world to std::terminate on a harmless path the tests did not cover.

**Fix:** Lint: resolve the declared type of each by-copy capture name within the enclosing function and reject Ptr<...> captures (require Ref init-captures). Make a Ptr stamp mismatch throw IllegalStateException before dereferencing, so it is logged per job, instead of asserting. The pointer has not been used yet, so this is safe.

### RT-13 [minor]

**Scenario:** NPE-as-control-flow is a design goal (Ref/Ptr throw NullPointerException, and JobScope logs it and continues). StableMap::forEach increments `depth`, and ArrayList iterators track modCount. If a callback throws, for example in MoveTaskManager.run or KnownList.forEachPlayer broadcasts, and depth is not restored, the map never reaches depth 0.

**What breaks:** Tombstones and `erasedDuringIteration` grow without bound, and the Refs held there are never released. The affected objects (for example every creature that ever left movingCreatures) and their IDs leak. Iteration slows down forever after a single handler exception.

**Fix:** Specify RAII depth guards in StableMap/StableSet/ArrayList iteration, with compaction when the guard unwinds. Specify clear() during iteration as writing tombstones. Add a kernel conformance test: a callback throws mid-forEach, then compaction and destruction still happen.

### RT-14 [minor]

**Scenario:** Fixed-rate semantics (`next = prevDue + period`, late runs back to back) combined with the documented debugger workflow. Sequence: pause at a breakpoint for 3 minutes (the watchdog is disabled under a debugger), then resume. MoveTaskManager (200 ms), MovementNotifyTask, ZoneUpdateService, TeamStatUpdater and every AI periodic task owe hundreds of runs each. With the 20 ms timer budget they alternate with the inbox for many seconds.

**What breaks:** After every breakpoint the single logic thread keeps churning through catch-up ticks. Client packets are delayed and pings time out, so debugging a live client repeatedly disconnects it. Java has the same policy but spreads it over pool threads.

**Fix:** Coalesce missed periods when lag exceeds N periods (run once, then realign `next` to now + period), either always for AbstractPeriodicTaskManager or when `IsDebuggerPresent()` / a debug config flag is set. Record it in DEVIATIONS.

## Alternatives considered

## Scores

| Proposal | Porting fidelity | C++ safety | Hobby pragmatics | Mean |
|---|---|---|---|---|
| **single-logic-executor (base)** | **8** | 7 | **8.5** | **7.8** |
| handles-world-owned | 5 | **8** | 6 | 6.3 |
| free-threaded | 6 | 4.5 | 3.5 | 4.7 |

## Why free-threaded lost
It keeps Java's pools, monitors and parallel movement. On paper it is the most literal port, but that literalness costs too much:

- **Memory safety depends on discipline over ~390k lines.**
  - Every non-final field of a shared class must become `Field<T>`: about 2,100 in model, 400 in skillengine and 333 in handlers.
  - Every collection needs a choice of concurrent wrapper.
  - The proposal's own worked example gets this wrong. `Effect.periodicTasks` is listed as "replaced wholesale", but Effect.java:246-252 mutates the array in place.
- **Races can't be tested on the user's toolchain.** `Field<T>` hides races from TSan, and TSan needs a Linux clang build the user doesn't use. Bugs stay nondeterministic.
- **The epoch reclaimer is fragile global infrastructure.** One stuck task (FixPath's 5 s wait, a deadlock) stalls reclamation for the whole process.
- **Use-after-free on `//ai set`.** Its part model pins the owner, not the AI, so a pinned AI can be freed.
- **Deadlocks stay possible**, exactly as in Java.
- **Grafted:** the Pin-first schedule overload, detaching the instance handler at destroy (instead of a weak position reference), debug borrow stamps, and source_location-keyed task stats.

## Why handles-world-owned lost
It scored highest on safety (8). Registry ownership rules out cycles, generations make stale handles ABA-proof, and it had the best-grounded research (684 schedule sites classified by script). It is also the least faithful port and the heaviest to port by hand:

- **Every stored reference becomes nullable.** That includes 202 `getEffector` uses in 101 files, targets, aggro attackers and handler fields. A forgotten check is a whole-server crash, because `get()` returns a raw `T*`.
- **Anchored tasks silently drop behaviour Java relied on.**
  - IncarnateAI.java:27-35 schedules a claw delete 5 minutes out from a boss whose corpse decays after 2 s, so the claw leaks permanently.
  - CaptainXastaAI schedules Ariana's walk and door changes from a dying AI, 1 to 26 s later.
  - Choosing the right scope at each site is a semantic decision that parallel porting agents won't make consistently.
- **Extra machinery:** DetachedPlayers pins, EffectorInfo snapshots, item re-lookup, and two identity notions. The regex capture rule also misses `[this, raksha]`.
- **It still has the single-core ceiling and DB stalls**, so it pays both costs without Ref's retention semantics.
- **Grafted:** confinement asserts outside debug builds, compile-time captureless unpinned tasks (`bindTask`/`TaskStruct`), the shutdown coordinator thread, the logic-only config split, task introspection, and the script-based site classification used for porting estimates.

## Defects fixed in the base proposal
The judges found these in single-logic-executor; each is fixed in the final design:

| Defect | Fix |
|---|---|
| **StableMap use-after-free.** `forEach` handed out `Ref<Creature>&` into a vector that `addCreature` reallocates during the MOVE_ARRIVED → WalkManager → `onStartMove` path (verified, NpcController.java:311) | Values are Ref or scalar only, callbacks receive them by value, iteration is by index, removals leave tombstones |
| **Nested reclamation in `pumpUntil`** | Removed. Shutdown is sequenced by a coordinator thread |
| **The weak position→instance reference and "hollow shell"** | A strong `Ref<MapRegion>` plus detaching the handler after `onInstanceDestroy` |
| **Outbox skipping packets once the connection is closed, and `close(packet)` ordering** | `close` is an ordered outbox entry. Java's `close(packet)` also clears the queue (commons AConnection.h:258 matches) |
| **Outbox dedupe keyed by packet address** | Dedupe by broadcast group, plus a per-packet exception guard |
| **Thread asserts in debug builds only** | Full checks in Debug and RelWithDebInfo, boundary checks in Release |
| **`keepAlive` capture convention** | Pin-first overload plus compile-time unpinned check |
| **Coroutine for FixPath** | Continuation object |

## Conventions to add once accepted

- Game logic is confined to the logic thread (GameExecutor). Other threads only post Jobs or run BlockingPool work that touches no game state: captures must satisfy DetachedArg (ids, strings, row structs, const templates). Never create or copy a Ref<T> off the logic thread.
- Stored game object references (fields, container elements, lambda captures, packet members) are Ref<T> (`const Ref<T>` for Java final fields). Parameters, locals and return values are Ptr<T> or T&. Never store a Ptr or a raw pointer to a RefCounted type. Parts (AI, controllers, KnownList, EffectController, AggroList, stats, MapRegion, SpawnTemplate) derive from RefPart, and Ref<Part> retains the owner.
- Game objects are created only with VisibleObject::create<T>() or makeRef<T>(), never `new` or the stack. Destructors of RefCounted types are noexcept and may only release Refs and IDs: no packets, no events, no Ptr dereferences, no Ref to this.
- Scheduling: `schedule(this, [this] {...}, delay)` or `schedule(this, &X::method, delay)`. List every object a lambda captures by `this` or `&name` in the pin list (`{this, &player}`). Other captures must be values or Refs as init-captures. Unpinned callables must be captureless, a named TaskStruct, or bindTask(fn, args...). `[&]` and `[=]` are forbidden in task lambdas (lint_tasks.py).
- Java `Future<?>` becomes FutureRef; `Future<?>[]` becomes std::array<FutureRef, N>. `cancel(true)` stays as written (the interrupt flag is ignored). Use Future::deferred for unscheduled FutureTask and runNowIfPending for run()+get().
- Java `synchronized` blocks and methods are deleted, leaving a `// Java: synchronized (x)` comment. ReentrantLock and StampedLock are deleted. Atomic* classes keep their Java names as non-atomic shims.
- Java ConcurrentHashMap, newKeySet and CopyOnWriteArrayList fields become StableMap, StableSet and CopyOnWriteArrayList shims. Their values must be Ref<T>, handles or trivially copyable, so Java objects stored by value (AggroInfo, KnownObject) become RefCounted. forEach callbacks receive values by copy and may put or remove re-entrantly.
- Java ArrayList and HashMap fields of game objects and handlers use the aion ArrayList<T>/HashMap<K,V> shims, which throw ConcurrentModificationException on modification during iteration. Local collections stay std::.
- Server packets: `sendPacket(player, SM_X(...))` (drop `new`). writeImpl keeps the Java body using writeD(v) and the other write methods through the transient serialization buffer. Packets whose writeImpl reads the connection must override dependsOnConnection(). `con.close(new SM_X())` becomes `con->close(SM_X())`, which goes through the outbox.
- Handler object fields that point at objects referring back to the handler's owner (for example an add's AI holding its spawner) must be cleared in handleDespawned/handleDied, or listed in the chunk's leak note. Scenario tests assert LeakCensus is zero after destroyInstance and logout.
- Config classes are classified LogicConfig (plain static inline fields, rebound only on the logic thread) or SharedConfig (read by IO, pool or watchdog threads; the ConfigValue/std::atomic rule applies). The classification table lives with the config chunk (P4-01).
- Checked builds (Debug and RelWithDebInfo) assert logic-thread confinement on every refcount operation and stamp every Ptr with the job serial. Run the dev server from RelWithDebInfo. Release builds assert only at subsystem boundaries.
- Handler and AI tests use GameServerHarness with ManualClock (runReady/advance) and a seeded Rnd, and cover despawn mid-delay, destroyInstance with pending tasks and relogin, under the msvc-asan preset.

## Prototype plan

1. Kernel conformance tests with ManualClock under the msvc-asan preset. Future: cancel before and while running, self-cancel, getDelay after cancel, runNowIfPending on a deferred task (rethrows) and on a scheduled task (logs), fixed-rate catch-up, a periodic task surviving an exception, shutdown dropping delayed tasks. Ref/Reclaimer: zombie resurrection, cascading destruction, Ptr job-stamp assert on an escaped borrow. StableMap: re-entrant put and remove, including the MoveTaskManager walker re-add regression. Pass: all green, no ASan errors, LeakCensus zero.
2. Runtime microbenchmark: 80k Npc shells on a MapRegion grid with KnownLists, 2,000 movers ticking every 200 ms, synthetic AI events, SM_MOVE broadcasts to 5 fake connections through the outbox. Pass: tick p99 under 50 ms in the RelWithDebInfo checked build. Record bytes per Npc, reclaim cost and refcount overhead.
3. Handler vertical slice on GameServerHarness: CreatureController tasks, YamenessPortalSummonedAI, IncarnateAI (the 5-minute claw delete must still run after the corpse decays), SacrificialSoulAI, LowerUdasTempleInstance. Scenarios: despawn mid-delay, destroyInstance with a pending 60 s task, 50 instance create/destroy cycles. Pass: no ASan errors, IDs released only after pinning tasks end, memory back to baseline, census zero, handler detach verified.
4. Packet path against the real 4.8 client: SM_KEY special frame on the strand, deduplicated broadcast frame, per-recipient SM_PLAYER_INFO, SM_PLAY_MOVIE mutating at flush, close(SM_QUIT_RESPONSE) queued after other packets in the same job, a deliberately misflagged packet tripping the connection-dependence check. Pass: client connects and behaves; the check fires.
5. DB-inline stall measurement: PlayerService::getPlayer plus storePlayer and the character list for a character with full inventory, warehouse and quests on local MariaDB. Record p50/p99 job time and decide whether any loader needs a rows-off-thread split.
6. Relogin within a group: log out, relogin within 600 s, and in a second run let OfflinePlayerChecker time out. Pass: PlayerConnectedEvent swap works, World::removeObject identity check holds, old Player instance freed at the end of the job, census clean.
7. Effect with a despawned effector: a summon's DoT on an Npc, summon released mid-DoT. Pass: NpcController::onAttack attributes hate to the master, Effect and summon reclaimed after the effect ends, summon ID quarantined for 60 s.
8. Shutdown coordinator with 5 connected FakeGameClients: countdown, NioServer::shutdown on the coordinator thread, every leaveWorld runs on the logic thread, periodic saves complete, no nested job boundaries, correct exit code.
9. Porting-cost calibration: port 20 AI and 10 instance handlers with lint_tasks.py active; measure minutes per file, pin edits per 100 lines and lint false positives before batching phase 6.

## Questions for the user

- **When should server packet bytes be produced?** Options: End of the sending job, via the outbox (Java-like lazy write; the 6 writeImpls that change state stay unchanged) / Eagerly inside sendPacket (simpler mental model; a packet shows state at the moment of the call). Recommendation: End of job, with the `packet_serialization=immediate` switch available for diagnosis. It matches Java's observable timing most closely: bytes are written after the sender finishes. writeImpl code is identical in both modes, so this is about which default feels right, not about porting cost.
- **What should the watchdog do when a logic job stalls, e.g. an endless loop or a hung DB call?** Options: Dump stacks or a minidump and keep running (restart only if restart_on_stall=true) / Dump and exit with the RESTART code like Java's DeadLockDetector. Recommendation: Dump only by default, restart opt-in; watchdog disabled while a debugger is attached. On a local hobby server an automatic restart usually destroys the state you want to inspect. Java parity is still one config flag away.
- **Which build runs the day-to-day server?** Options: RelWithDebInfo with full checks (thread assert per refcount op, Ptr job stamps) / Release with only boundary checks, for maximum speed. Recommendation: RelWithDebInfo checked. The checks cost roughly a nanosecond per operation, which doesn't matter with a few players. They turn discipline slips (a leaked borrow, a Ref touched off-thread) into immediate stack traces instead of silent corruption.
- **How far should the port move DB work off the logic thread before it is measured?** Options: Nothing: inline everywhere, as Java calls DAOs, until prototype 5 shows noticeable hitches / Pre-emptively split character list and enter-world loaders into rows loaded off-thread with objects built on the logic thread. Recommendation: Inline everywhere first, measure, then split only the proven hot loaders. Inline keeps the 288 DAO call sites line-by-line portable and removes Java's save-vs-gameplay race. Splitting loaders is a non-mechanical DAO rewrite that only pays off if local MariaDB stalls are actually noticeable.
