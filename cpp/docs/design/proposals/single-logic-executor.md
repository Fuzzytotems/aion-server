# Proposal: One logic thread: intrusive Ref<T> with end-of-job reclamation, end-of-job packet flush, and a Java-shaped scheduler

> One of three competing proposals scored by the design panel (not the chosen design; see ../runtime-architecture.md).

All game state lives on one dedicated logic thread with its own event loop, not an Asio strand. The loop runs client packet runImpl, timers, cron, AI, movement ticks, DB calls (inline by default) and posted continuations. Asio IO threads only decrypt, decode (readImpl is pure) and post; they encrypt on the connection strand. A small blocking pool takes detached work: reload parsing, heavy pure-SQL jobs, the LS connect. Objects are shared through a non-atomic intrusive `Ref<T>`. Stored references (fields, containers, lambda captures, packets) are `Ref`; parameters, locals and returns are a checked `Ptr<T>` that throws NullPointerException the way Java does. When a count reaches zero the object is not deleted immediately: it goes on a zombie list that is freed after the current job. So any raw use inside a job is safe, including `deleteOwner(this)` followed by more calls on the owner, and the objectId is released in `~AionObject`. That destructor is the exact analogue of the Cleaner, which makes ID reuse ABA-safe for every holder of a Ref. Server packets keep their lazy `writeImpl` bodies unchanged. They are queued in a per-job outbox and serialized on the logic thread when the job ends: once per broadcast, or once per recipient for the 25 connection-dependent packets. So they read the final state of the job, the 6 writeImpls that change state do so legally on the logic thread, and a queued packet never outlives its job. The scheduler keeps the Java API (schedule, scheduleAtFixedRate, Future with cancel/isDone/isCancelled/getDelay/runNowIfPending, deferred FutureTask) and adds a ManualClock for deterministic handler tests. Per schedule site, a port adds `keep = keepAlive()` to the capture list and nothing else. Locks, atomics and ConcurrentHashMap reduce to shims with the same API, because only one thread ever touches the state. For a hobby local server this removes the whole class of data-race UB by construction, keeps GC semantics for stale-but-readable objects, and makes the server debuggable and replayable. The costs are a single-core ceiling and DB hitches on the logic thread, both acceptable at a few players.

# Runtime architecture of the C++ game server

**Status:** proposal for phase 4. It turns decision (A), ownership, into a threading model plus an ownership model.

**Evidence used:** the research maps (boot-infrastructure, object-model-world, network, dao-geo, handlers-registration, services-skills-quests, static-data-jaxb) and critic.md. I checked these Java sources myself:
- `ThreadPoolManager`, `AionObject` (Cleaner), `CreatureController.addTask/cancelAllTasks`
- `CM_TELEPORT_ANIMATION_DONE` with `TeleportService.sendLoc:191`, `PlayerReviveService.scheduleReviveAtBase`
- `FixPath.getZ`, `MoveTaskManager`, `AbstractPeriodicTaskManager`
- `KnownList.clear` (removes from the map it is iterating), `InstanceService.destroyInstance` (deletes while iterating the instance)
- `Effect.startEffect/schedulePeriodicActions`, `AbstractOverTimeEffect`, `PoisonEffect`, `NpcController.onAttack`
- `PlayerTeamMember`, `PlayerGroup.addMember/onRemoveMember`, `PlayerGroupService.OfflinePlayerChecker`, `PlayerConnectedEvent`
- `PacketSendUtility`, `AionServerPacket.write`, `AionConnection.writeData/onDisconnect`, `SM_ATTACK`, `SM_GROUP_MEMBER_INFO`, `SM_PLAY_MOVIE`, `SM_KEY`
- `PlayerLeaveWorldService`, `GeneralUpdateTask`, `RespawnService` (DecayTask, setAutoReleaseId), `WorldPosition.getWorldMapInstance`, `World.removeObject`
- `DropService` (getDelay), `YamenessPortalSummonedAI`, `KaluvaSpawnAI`, `SacrificialSoulAI`, `EmpyreanCrucibleInstance`, `AbstractCronTask`, `PlayerModelController`

On the C++ side I read `AConnection.h`, `PacketProcessor.h`, `BaseServerPacket.h`, `ConfigValue.h` and the login server's `ScheduledExecutor.h`.

---

## 0. Invariants in one page

| # | Invariant | Enforced by |
|---|---|---|
| I1 | Game state (objects, refcounts, World, KnownLists, templates of the spawn family, logic-read config, IDFactory users, QuestEngine maps) is touched only on the **logic thread** | `AION_ASSERT_LOGIC_THREAD()` in debug builds, in `Ref` inc/dec, `ThreadPoolManager::schedule`, the outbox, and World/KnownList mutators |
| I2 | A `RefCounted` object is freed only **between jobs**. It is never freed during a job | Zombie list drained by the executor after each job |
| I3 | Anything **stored** (field, container, capture, packet, Future callable) holds `Ref<T>`/`WeakRef<T>`. Borrowed uses (params, locals, returns) are `Ptr<T>`/`T&` | Review plus a lint script (raw `T*` fields of RefCounted types, `[this` captures without `keepAlive`) |
| I4 | A server packet is serialized on the logic thread when the job that sent it ends. Only bytes cross to IO threads | `AionConnection::sendPacket` pushes to the outbox, never to the socket queue |
| I5 | Containers that game code iterates tolerate insert and erase from inside the iteration callback (the single-threaded counterpart of CHM's weakly consistent iteration) | `StableMap`/`StableSet` |
| I6 | IO threads, blocking pool, watchdog and console thread talk to the logic thread only through `GameExecutor::post` | API shape. They hold no Ref |
| I7 | Delete hooks break cycles: after `World.removeObject` the object has an empty KnownList, empty AggroList, cancelled controller tasks, its own effects ended and its observers cleared. After logout, team membership is swapped or expires | Java already does this. The C++ addition is a leak census that reports deleted objects still alive after N minutes |

---

## 1. Threading model

### 1.1 Threads

| Thread | Count | Runs | Touches game state? |
|---|---|---|---|
| **Logic** (the main thread once startup finishes) | 1 | Client packet `runImpl`, all `schedule*` callbacks, cron callbacks, periodic managers (MoveTaskManager, MovementNotifyTask, ZoneUpdateService, TeamStatUpdater…), AI events, KnownList/visibility, skills/effects, geo queries (`canSee`/`getZ`/collisions), **DAO calls (inline by default)**, server packet serialization, LS/CS incoming packet `runImpl`, `onDisconnect` bodies, `//reload` swaps, `Config.load` from events and `//configure`, shutdown sequence | yes, exclusively |
| Asio IO (`NioServer`) | 1-2 (config) | accept, read, `Crypt.decrypt`, opcode decode, factory state check, PFF/flood, `readImpl`, **post**. Write side: copy the serialized frame and `Crypt.encrypt` on the strand. SM_KEY key enabling on the strand | no |
| BlockingPool | 2 | Java `executeLongRunning`/`submitLongRunning` with detached data: `//reload` XML parsing, NN training on copied datasets (`PlayerModelController`), outbound `openSocket` for LS/CS (blocking connect), and pure-SQL jobs that return rows (opt-in, §1.4) | no (captures must be detached) |
| Startup workers | cores (only before the loop starts) | parallel static-data parsing, geo load plus eager BIH build, World map creation | only immutable data being built |
| Watchdog | 1 | reads an atomic `jobStartedAt`/`jobName`: slow-job warning (RunnableWrapper's 5 s), stall dump (DeadLockDetector analogue, optional minidump plus exit RESTART) | no |
| Console (optional) | 1 | `SetConsoleCtrlHandler`/signals, optional stdin command reader; both post to logic | no |
| Logging (spdlog async, Discord) | existing | | no |

What goes away: the PacketProcessor's 4 threads, the scheduled pool, the instant pool, the Quartz thread, ForkJoin `parallelStream`, NetFlusher's `java.util.Timer` and the Cleaner thread. All of it becomes jobs or timers on the logic thread.

### 1.2 Why a dedicated loop and not an Asio strand
- **Job-boundary hooks are the core of the design** (I2 and I4). A hand-written loop runs `outbox.flush(); reclaimer.reclaim();` after every job.
- With an Asio strand every handler would need wrapping, and it could hop threads, which breaks thread-affinity asserts, thread_local `Rnd` and `AbstractAI.DEPTH`.
- **Timers with Java semantics** (runNow, getDelay, drop the callable on cancel, periodic catch-up) plus an injectable `ManualClock` for deterministic unit tests of AI and instance scripts come to about 400 lines. Doing the same on `asio::basic_waitable_timer` with custom clock traits is uglier.
- The loop sleeps until the next deadline or a post, with no busy tick, so timer precision matches Java's scheduled pool.
  - On Windows, raise the timer resolution (`timeBeginPeriod(1)` or `CREATE_WAITABLE_TIMER_HIGH_RESOLUTION` behind a thin wrapper). Without it, the 200 ms movement tick jitters by 15.6 ms.

```cpp
namespace aion::gameserver::utils {
using Job = std::move_only_function<void()>;          // C++23; own 60-line UniqueFunction if the STL lacks it

/** The single game-logic executor. Not in Java (replaces PacketProcessor + ThreadPoolManager pools + Quartz thread). */
class GameExecutor {
public:
	static GameExecutor& get();
	[[nodiscard]] bool isLogicThread() const noexcept;
	void post(Job job, const char* tag = nullptr);  // thread-safe, FIFO; the only way into the logic thread
	void run();                                      // loop on the calling (main) thread until stop()
	void stop();
	/** Shutdown only: runs jobs on the calling (logic) thread until pred() is true or timeout. Not reentrant from normal jobs. */
	bool pumpUntil(std::function<bool()> pred, std::chrono::milliseconds timeout);
	// tests
	void setClock(Clock*);          // ManualClock in tests
	void runReady();                // drain posted + due timers without waiting
	void advance(std::chrono::milliseconds);
};
}
```
Loop body: take all due timers in `(due, seq)` order and swap out the inbox. Run each job inside `JobScope{ try { job(); } catch (...) { log.errorCurrentException(tag); } outbox.flush(); reclaimer.reclaim(); stats(tag, dt); }`. Then wait until `min(nextDue, inbox signal)`.
- Timers run before posted jobs.
- Jobs with the same deadline run in scheduling order. This is deterministic, whereas Java's order is arbitrary.

### 1.3 What runs where, per event source
- **Client packet:**
  - Strand: `decrypt → factory(opcode, state) → readImpl → post(PacketJob)`.
  - Logic: `if (pck->isValid()) pck->run()`.
  - FIFO posting from the strand keeps per-connection order.
  - The two `readImpl`s that read the active player for logging (CM_BUY_ITEM:48, CM_ATREIAN_PASSPORT:33) move that read into `runImpl`.
  - The connection `state` becomes `std::atomic<State>`: written on logic, read by the factory on IO.
- **LS/CS link:** same pattern. `processData` posts. This fixes Java's unordered `ThreadPoolManager.execute(pck)`, which becomes ordered (deviation).
  - Reconnect: a logic timer posts `openSocket` to the BlockingPool. Its continuation, on logic, calls `registerConnection`. `initialized()` sends SM_GS_AUTH, a value packet that may be serialized on any thread (§5.4).
- **Disconnect:** `nioServer.connect([](auto task){ GameExecutor::get().post(std::move(task)); })`, so `AionConnection::onDisconnect` runs on logic (`leaveWorldDelayed`, `LoginServer.onDisconnect`).
- **Cron:** logic timers (§4.4).
- **NPC movement:** `MoveTaskManager` is an ordinary fixed-rate task, and `parallelStream` becomes a loop (§8h).
  - Escape hatch if a siege profile proves it necessary: a two-phase tick. Next positions and geo rays are computed in parallel on read-only data, then committed serially on logic. That is not needed for the first port.
- **AI:** synchronous on the caller, as in Java. `MapRegion.activate`'s `execute(...)` becomes `post`, so it runs after the current job.
- **Geo:** synchronous queries. Meshes and BIH are immutable after startup. Door, placeable and house state is logic-only, so no lock is needed.
- **DB I/O:** inline on logic (§1.4).
- **Admin console:** the Ctrl+C handler posts `ShutdownHook::initShutdown`. An optional stdin reader posts console commands.
- **Shutdown:** the countdown runs on logic timers. The final stage starts `NioServer::shutdown()` on a helper thread while the logic thread calls `pumpUntil(nioDone)`, so the `onServerClose`/`onDisconnect` work posted meanwhile still runs on logic.
  - After `PeriodicSaveService.onShutdown` and `saveGameTime`, flush logs and `quick_exit(code)`, instead of destroying about 100k objects and running static destructors.

### 1.4 DB work: inline by default, offload by exception
- **Default:** DAOs run synchronously on the logic thread, as the Java packet and scheduled threads did. This keeps "DAOs port almost line by line" and the 33 DAO calls inside model mutators (Item, Equipment, PetList…).
  - It also removes Java's periodic-save race: `GeneralUpdateTask`/`ItemUpdateTask` iterate Player storages, and now nothing mutates them concurrently.
  - Pre-warm the pool (5 connections) at startup so no connect ever happens on the logic thread.
- Expected hitches on local MariaDB (to be measured, §10):
  - enter world: about 40 queries plus N+1 stones, roughly 10-40 ms
  - full `storePlayer`: roughly 5-30 ms
  - character list: a few ms per character
- **Offload API**, for pure SQL whose results are applied on logic (e.g. ranking recomputations, `DatabaseCleaningService`-like maintenance, broker/housing bulk loads, large `//` admin queries):

```cpp
template <std::invocable Work, class Then>   // Then: void(std::expected<R, std::exception_ptr>)
void BlockingPool::submit(Work work, Then then);  // work on pool thread (captures: values only), then posted to logic
```
- Rule: `work` captures only detached values (ids, strings, row structs). It never captures Ref, Ptr or `this` of a game object. A violation trips the debug refcount thread assert immediately.
- Player load and save stay inline. Splitting them into "read rows off-thread, build objects on logic" is a non-mechanical DAO refactor, reserved for proven hot spots.

### 1.5 The two blocking sites
1. **CM_TELEPORT_ANIMATION_DONE** needs `Future::runNowIfPending()` on the logic thread (§4.2). No waiting happens; Java's `get()` only rethrows.
2. **FixPath** runs on the instant pool and blocks on `waitTask.get(5 s)` while packets move the admin. It becomes a C++20 coroutine on the logic executor, one of about three coroutine sites in total; the scheduler API does not require coroutines.

```cpp
// Java: FixPath.getZ(admin) - blocks up to 5 s on waitTask[0].get(5, SECONDS)
GameTask<float> FixPath::getZ(Ref<Player> admin) {
	sendInfo(admin, "Waiting to get Z...");
	float lastZ = admin->getZ();
	bool ok = co_await waitUntil(500ms, 250ms, 5s, [&] {            // periodic poll on the logic timer, resumes the coroutine
		float z = admin->getZ();
		if (admin->getMoveController()->getMovementMask() == MovementMask::IMMEDIATE && z != lastZ && admin->isSpawned()
			&& !admin->isInState(CreatureState::DEAD)) { lastZ = z; return true; }
		return false;
	});
	if (!ok) { sendInfo(admin, "Aborted path fixing due to timeout."); stop(); co_return 0; }
	co_return lastZ;
}
// execute(() -> {...}) becomes spawnDetached(fixPathCoroutine(Ref(admin), template_, worldId, zOff)); the loop body is unchanged except `co_await getZ(admin)`.
```

---

## 2. Object ownership and references

### 2.1 Reference model: non-atomic intrusive refcount, deferred free. Why not the alternatives

| Option | Verdict |
|---|---|
| `std::shared_ptr` | Atomic counts, two-word pointers, `enable_shared_from_this` needed to turn `this`/`getOwner()` into owners (about 1,566 `getOwner()` calls in handlers), no way to share a count with parts (AI, controller), no deferred free. It works, but it is noisier and gives up I2 |
| Handles (id+generation) with lookup | Changes semantics: Java reads deleted objects (`effector` of a DoT, stale team members, `getOwner().getX()` after delete in 102+ guarded tasks). Every `get` would need a null path, which is not mechanical across 1,700 files |
| **Intrusive `Ref<T>`, non-atomic, with a zombie list** | Keeps GC semantics for anything reachable, costs about 1 ns per inc/dec, `Ref(this)` works anywhere, parts forward to their owner, and the destructor is the Cleaner. **Chosen.** Non-atomic is correct because of I1 |

```cpp
namespace aion::gameserver::model {

/** Base of every heap object whose Java reference escapes into fields, tasks, packets or other objects. Not in Java (GC). */
class RefCounted {
public:
	RefCounted(const RefCounted&) = delete;
protected:
	RefCounted() = default;
	virtual ~RefCounted();                 // runs only inside Reclaimer::reclaim() on the logic thread
private:
	mutable uint32_t refs = 0;
	mutable bool queued = false;           // on the zombie list
	mutable WeakAnchor* anchor = nullptr;  // lazily allocated for WeakRef
	friend void refAdd(const RefCounted*) noexcept;
	friend void refRelease(const RefCounted*) noexcept;
	friend class Reclaimer;
};

inline void refRelease(const RefCounted* o) noexcept {
	AION_ASSERT_LOGIC_THREAD();
	if (--o->refs == 0 && !o->queued) { o->queued = true; Reclaimer::push(o); }  // never delete inline (I2)
}
// Reclaimer::reclaim(): loop { swap list; for each o: o->queued=false; if (o->refs==0) { if (o->anchor) o->anchor->obj=nullptr; delete o; } }
// A zombie resurrected during its job (Ref(this) again) is simply skipped.

/** Stored reference (field, container, capture, packet). Java: a reference-typed field. */
template <class T> class Ref {
public:
	Ref() noexcept = default;
	Ref(std::nullptr_t) noexcept {}
	Ref(T* p) noexcept;                     // implicit on purpose: `boss = getNpc(216263);` `Ref(this)`
	template <class U> requires std::convertible_to<U*, T*> Ref(const Ref<U>&) noexcept;
	T* operator->() const { return p ? p : throwNullPointer<T>(); }   // Java NPE semantics: logged by the job, world continues
	T& operator*() const;
	T* get() const noexcept { return p; }
	explicit operator bool() const noexcept { return p != nullptr; }
	friend bool operator==(const Ref&, const Ref&) = default;           // Java `==` (identity)
private:
	T* p = nullptr;                          // refAdd(ownerOf(p)) - parts forward to their owner
};

/** Borrowed reference (param, local, return). Trivially copyable; checked -> like Ref. Must never be stored (lint). */
template <class T> class Ptr { /* same accessors, no counting; implicit from T*, T&, Ref<U> */ };

/** Weak reference; lock() returns null once the object was freed. Java: WeakReference (DropNpc.lootingTeam), InstanceScaler, WorldPosition->instance. */
template <class T> class WeakRef { public: Ptr<T> get() const noexcept; };

template <class T, class... A> Ref<T> makeRef(A&&... a);   // the only way to create RefCounted objects (refs start at 1)
}
```

**Parts** (VisibleObjectController, KnownList, AbstractAI, EffectController, ObserveController, AggroList, stats, move controller, InstanceHandler inside WorldMapInstance) are `unique_ptr` or by-value members with a back-pointer. They derive from `RefPart`, which forwards counting to the owner:

```cpp
class RefPart { public: KeepAlive keepAlive() const; protected: virtual const RefCounted& refOwner() const = 0; };
// KeepAlive = type-erased Ref<RefCounted>; for a RefCounted object keepAlive() is Ref(this).
```
`//ai set` (Ai.java replaces the final `Creature.ai`) must not free the old AI while tasks capture its `this`. So `Creature::setAi` moves the old AI into `retiredAis`, which is freed with the Creature.

**Which classes are RefCounted:**
- the AionObject hierarchy: VisibleObject…Player, Item, Letter, Legion, teams
- Effect, Skill, Future, WorldMapInstance, SpawnTemplate/SpawnGroup
- ActionObserver/AttackCalcObserver, QuestState, DropNpc, RespawnTask

QuestEnv is a copyable value with Ref fields. Rule: any heap object whose reference escapes into a task, packet, container or another object is RefCounted; an object whose lifetime equals its owner's is a RefPart.

### 2.2 Rules per use case

| Use case | C++ reference | Notes / cycle breaker |
|---|---|---|
| `World.allObjects`, player containers, `WorldMapInstance`/`MapRegion` object maps | `StableMap<int32_t, Ref<VisibleObject>>` | The root. `World::removeObject` keeps Java's identity check: `allObjects.get(id).get() == &object` |
| KnownList (two-way) | `StableMap<int32_t, KnownObject{Ref<VisibleObject>; bool visible}>` | Cleared on both sides in `World.despawn`. `clear()` removes during its own iteration, which I5 allows |
| `target` | `Ref<VisibleObject>` | cleared by `notSee` |
| AggroList / `AggroInfo.attacker` | `StableMap<int32_t, AggroInfo{Ref<Creature>…}>` | `notKnow`, `clear()` in onDespawn. `hateReductionTask` is cancelled in `clear()` |
| `Effect.effector/effected`, `Skill.effector/effectedList` | `const Ref<Creature>` | Keeps a despawned effector readable until the effect ends (§8c). Self-effects are broken by `removeAllEffects` on despawn/logout (I7) |
| `Summon.master`, `Pet.master`, `SummonedObject.creator` | `const Ref<…>` | `Player.summon` is `Ref<Summon>`; `SummonsService.release` does `setSummon(nullptr)` |
| Team members (incl. logged out) | `PlayerTeamMember{ const Ref<Player> player; }`; `Player.playerGroup` is `Ref<PlayerGroup>` | The cycle is broken by `PlayerGroup::onRemoveMember → setPlayerGroup(nullptr)` (verified PlayerGroup.java:30-33), on timeout, relogin swap or disband |
| Handler fields (`Npc boss`, `List<Npc>`) | `Ref<Npc>`, `std::vector<Ref<Npc>>` | Keeps the Npc readable, as in Java. Instance→handler→Npc no longer loops back (next row) |
| `WorldPosition` → map instance | `WeakRef<WorldMapInstance>` plus `MapRegion*` valid only while that weak ref is alive | **Deviation (details below)** |
| Scheduled tasks | the callable owns `Ref`s; `this` of a part/object is kept by `keep = keepAlive()` | The Future drops the callable when it finishes or is cancelled (after `run` returns), so controller-task cycles are temporary |
| Server packets | `Ref<T>` fields | Live only until the end of the job (I4). `ArtifactAI`'s two cached packets are fine |
| Player ↔ AionConnection | `Player.clientConnection`: `std::shared_ptr<AionConnection>` (atomically counted, crosses threads). `AionConnection.activePlayer`: `Ref<Player>`, **logic thread only** | `leaveWorld` clears both links as Java does. `~AionConnection` (may run on IO) asserts `!activePlayer`, otherwise posts the Ref's release to logic |
| Templates | `const XTemplate*` (immortal, §6) except the spawn family: `Ref<SpawnTemplate>` | |
| Already ID-based Java refs (DecayTask, GeneralUpdateTask, `Npc.creatorId`, `registeredObjects`, `Legion.memberIds`) | stay `int32_t` + lookup | Same ABA exposure as Java, reduced by quarantine (§3) |

**Why the position holds only a weak reference to its instance (a deliberate deviation).** In Java, `WorldPosition.mapRegion → parent` keeps a destroyed instance reachable. In C++ a strong ref would form a cycle whenever an instance handler keeps its own Npcs (`EmpyreanCrucibleInstance.npcs`): instance → handler → Npc → position → instance.
- With a weak ref, instances are kept alive only by `WorldMap` until `destroyInstance`, and by tasks that captured `keepAlive()` of the handler.
- When the last strong ref goes, the handler, regions and maps are freed. A small *hollow shell* (mapId, instanceId, empty containers, a shared no-op `GeneralInstanceHandler`) stays behind while weak refs exist.
- So `staleNpc->getPosition()->getWorldMapInstance()->getNpc(x)` returns null instead of crashing. That matches what Java returns from a destroyed instance whose objects were deleted.

### 2.3 Identity, equality, relogin
- Java `a == b` becomes `a == b` on Ref/Ptr (identity). This covers about 14 sites, e.g. `AttackUtil:510`, `TargetRangeProperty:50`, `World.removeObject`.
- Java `a.equals(b)` becomes `a->equals(b)`, comparing objectId with Java's "objectId 0 is never equal" rule. `hashCode` is the objectId.
- Object-keyed Java collections (3×`Set<Player>`, 2×`<Creature>`, 5×`<Item>`) become `StableMap<int32_t, Ref<T>>`. This is equivalent because Java's hash and equality were objectId-based.
- `List.remove(Object)` becomes `CollectionUtil::removeFirstEqual(v, x)`, which uses equals. It is not `std::erase`.
- **Relogin:** `PlayerService::getPlayer` builds a new `Ref<Player>` with the same DB objectId. The old instance stays alive as long as a team member, an effect's effector, a DropNpc or a task holds it; it is stale but valid, with a null connection, exactly as in Java. `PlayerConnectedEvent` swaps the member (§8d).

### 2.4 Iteration safety (I5)
```cpp
/** Java: ConcurrentHashMap/newKeySet as used by game code: iteration tolerates put/remove from inside the callback. Not thread-safe (I1). */
template <class K, class V> class StableMap {
public:
	Ptr<V> get(const K&) const;             // V* for values; valid until the next insertion
	bool containsKey(const K&) const;
	std::optional<V> put(K, V);
	bool putIfAbsent(K, V);
	std::optional<V> remove(const K&);
	bool remove(const K&, const V& expected);   // Java remove(k, v)
	template <class F> V& computeIfAbsent(const K&, F&&);
	template <class F> void forEach(F&& f);     // f(const K&, V&): erased entries are skipped (tombstones, compacted at depth 0);
	                                            // entries added during iteration are not visited (deterministic subset of CHM semantics)
	size_t size() const noexcept;
};
```
- Dense slot vector plus an index map, with an iteration-depth counter.
- `CollectionUtil::forEach`, `KnownList::forEachObject/forEachPlayer` and `for (VisibleObject obj : instance)` all go through it.
- Thin shims `AtomicBoolean/AtomicInteger/AtomicReference` (non-atomic, same API: `compareAndSet`, `getAndSet`) let the 311 handler `Atomic*` uses and 245 concurrent collections port verbatim.
- `synchronized` blocks are deleted, with a `// Java: synchronized(x)` comment where the lock documented an invariant.

---

## 3. ID lifecycle

| Group | Java | C++ | ABA |
|---|---|---|---|
| (a) auto-release: Npc, Gatherable, StaticObject, Summon, HouseObject, PlayerGroup/Alliance/League (id==0 ctor) | Cleaner after GC, deferred by `RespawnService.setAutoReleaseId` | `~AionObject() { if (autoRelease && id) if (!RespawnService::setAutoReleaseId(id)) IDFactory::getInstance().releaseId(id); }` runs in `Reclaimer` on logic, after the last Ref/Ptr use (I2). The RespawnTask deferral is kept verbatim | **Safe by construction** for every Ref holder: the id cannot be reused while any Ref exists. WeakRefs to a freed object are null, and a new object has a new anchor |
| (b) explicit: items after a successful DB delete (InventoryDAO:240), house objects/decor (PlayerRegisteredItemsDAO:164-167), Exchange/ItemSplit/PetAdoption/HTML/CM_CREATE_CHARACTER/CMT_CHARACTER_INFORMATION | explicit `releaseId` | unchanged; Item objects are still refcounted independently | same as Java (e.g. RepurchaseService matches sold Items by id after release); quarantine reduces it |
| (c) never released: FlyRing, Road, CuringObject, AssembledNpcPart, HTML message ids, CustomInstanceService static | none | unchanged (the static one becomes a lazy accessor) | n/a |
| Players | DB-owned, stable | unchanged | n/a |

**Quarantine (a C++ addition, listed in DEVIATIONS).**
- The problem: Java releases group (a) ids only after a GC, often seconds to minutes later. C++ releases at the end of the job, so bare-int holders (DecayTask, RespawnTask.oldObjectId, `creatorId`, client packets carrying ids) would meet reuse much sooner than on Java, and the IDFactory hands out the lowest free id next.
- `IDFactory::releaseId` therefore pushes `{id, now}` into a FIFO. `nextId` first moves ids older than `gameserver.idfactory.release_delay` (default 60 s) into the BitSet, keeping the lowest-first rule.
- IDFactory keeps a plain `std::mutex`. It is uncontended, and it lets startup workers and a future off-thread DAO call it.

---

## 4. Scheduler, Future and cron API

### 4.1 Semantics required by the Java usage (critic §2)
- `schedule`, `scheduleAtFixedRate`, `execute`, `submit`, `executeLongRunning`, `submitLongRunning`.
- Every callback is wrapped like `RunnableWrapper`: exceptions are logged, periodic tasks **continue**, and runs over 5 s trigger a warning. Stats are keyed by the callable's type (the existing commons deviation).
- `cancel(bool)`, where `mayInterrupt` is ignored because no game code checks interrupts (critic), `isDone`, `isCancelled`, `getDelay` (DropService:119), run-now of a pending task or of a **never-scheduled** `FutureTask` (TeleportService:191), and self-cancel from inside `run`.
- Fixed-rate catch-up: `next = previousDue + period`, so late runs happen back to back and never concurrently.

```cpp
namespace aion::gameserver::utils {

/** Java: ScheduledFuture / RunnableFuture. Logic thread only. */
class Future final : public model::RefCounted {
public:
	/** Java cancel(mayInterruptIfRunning): false if already done/cancelled. Removes the timer and destroys the callable now,
	 *  or - if called from inside its own run (self-cancel) - right after run returns. */
	bool cancel(bool mayInterruptIfRunning = false) noexcept;
	bool isCancelled() const noexcept;
	/** One-shot: ran (normally or with a logged exception) or cancelled. Periodic: only once cancelled. */
	bool isDone() const noexcept;
	/** Java getDelay(MILLISECONDS): next due - now (negative if overdue); 0 for deferred/done. */
	std::chrono::milliseconds getDelay() const noexcept;
	/** Java RunnableFuture.run() on a pending one-shot: removes it from the timer queue and runs it on the caller.
	 *  Returns false if it was running, done or cancelled. A scheduled task's exception is logged (Java: RunnableWrapper inside
	 *  the ScheduledFutureTask); a deferred() task's exception is rethrown (Java: FutureTask.get() -> ExecutionException). */
	bool runNowIfPending();
};
using FutureRef = model::Ref<Future>;

/** Java: ThreadPoolManager. Name kept for grep; now a facade over GameExecutor + BlockingPool. */
class ThreadPoolManager {
public:
	static ThreadPoolManager& getInstance();
	FutureRef schedule(Job r, int64_t delayMillis);
	FutureRef schedule(Job r, std::chrono::milliseconds delay);
	FutureRef scheduleAtFixedRate(Job r, int64_t delayMillis, int64_t periodMillis);
	FutureRef scheduleAtFixedRate(std::move_only_function<void(Future& self)> r, int64_t delay, int64_t period); // self-cancel sugar
	void execute(Job r);                          // runs after the current job (Java: concurrently, "soon")
	FutureRef submit(Job r);
	void executeLongRunning(DetachedJob r);       // BlockingPool
	static FutureRef deferred(Job r);             // Java: new FutureTask<Void>(r, null) - never on the timer queue
	std::vector<std::string> getStats() const;
	void shutdown();                              // drops pending timers (Java: setExecuteExistingDelayedTasksAfterShutdownPolicy(false))
};

/** Sugar for the 142 Java method references: `this::spawnSummons` */
auto task(RefPartOrObject auto* self, auto memFn) { return [self, keep = self->keepAlive(), memFn] { (self->*memFn)(); }; }
}
```

**Implementation.**
- A 4-ary indexed min-heap of `(due, seq, Future*)`; the heap owns a Ref to each pending Future.
- Cancel erases in O(log n) and frees the callable immediately. Java instead keeps cancelled tasks until their delay expires, so the C++ port frees captured objects and ids earlier (listed as a deviation).
- The state machine is `PENDING → RUNNING → (PENDING for periodic | DONE) / CANCELLED`, and the callable is destroyed only outside `RUNNING`, so self-cancel is safe (critic requirement).
- Expected pending timers: roughly 10-50k (regen, decay/respawn, effects, AI). Heap operations stay around 16 compares.

### 4.2 Run-now site
```cpp
// Java: CM_TELEPORT_ANIMATION_DONE.runImpl
void CM_TELEPORT_ANIMATION_DONE::runImpl() {
	Ptr<Player> player = getConnection()->getActivePlayer();
	FutureRef task = player->getController()->getAndRemoveTask(TaskId::TELEPORT);
	if (task && !task->isDone())
		try {
			task->runNowIfPending();           // deferred SpawnTask (TeleportService) or scheduled revive (PlayerReviveService/PvPZone)
		} catch (...) {
			log().errorCurrentException("");
			if (!player->isSpawned()) { PacketSendUtility::sendPacket(player, makePacket<SM_PLAYER_INFO>(player)); World::getInstance().spawn(player); }
		}
}
// TeleportService.sendLoc: player->getController()->addTask(TaskId::TELEPORT, ThreadPoolManager::deferred([spawnTask = std::move(spawnTask)] mutable { spawnTask.run(); }));
```

### 4.3 Periodic managers
`AbstractPeriodicTaskManager(period)` schedules `run` at a fixed rate with the initial `Rnd::get(500, 550)`. The FIFO managers work the same way. They are singletons, created explicitly in `GameServer::main` in Java's order, never by a lazy first use.

### 4.4 Cron (replaces Quartz)
```cpp
class CronService {
public:
	CronJobRef schedule(Job r, const CronExpression& expr, bool longRunning = false, std::type_index kind = typeid(void));
	bool cancel(const CronJobRef&);
	std::vector<CronJobRef> findJobs(std::type_index kind) const;                      // Java: findJobs(Class)
	std::map<CronJobRef, std::chrono::system_clock::time_point> findNextFireTimes(std::type_index kind) const; // SiegeService:323
};
```
- **Expressions:** seconds field, `?`, names, lists, ranges, `/` increments, optional year. `L`/`W`/`#` are rejected with an error (the critic verified none is used). `getTimeAfter` is evaluated in `GSConfig::TIME_ZONE_ID` with `zoned_time`.
- **Arming:** each job arms a one-shot logic timer for `steady_now + (nextFire_wall - wall_now)`.
- **On fire:** if the wall clock is still before `nextFire` (the clock was adjusted), re-arm. Otherwise run once (Quartz's smart misfire for cron is fire once now) and compute the next time from `max(nextFire, now)`.
- **Clock drift:** a 60 s watchdog timer re-arms all jobs when wall and steady time drift by more than 2 s.
- `longRunning` is ignored: everything runs on logic, and heavy jobs offload DB parts explicitly.
- `AbstractCronTask`'s semaphore plus `executeLongRunning` becomes a synchronous run-on-start check in `main`.
- Port `CronServiceTest` as test vectors.

---

## 5. Server packets

### 5.1 Model: lazy inside the job, eager towards IO
- `writeImpl` bodies stay **byte-for-byte the Java code**: 3,890 lines, fields become `Ref<T>`.
- **When** they run changes: at the end of the job that sent the packet, on the logic thread.
- This keeps Java's "reads live state after the sender finished" behaviour in the common case. The NIO thread normally wrote after the runImpl returned, though Java made no promise.
- Queued packets never extend object lifetimes beyond a job, and IO threads only see bytes, so N IO threads are safe.
- Because serialization is thread-confined, `AionServerPacket` can keep a Java-style `buf` member during `write`, and `writeD(x)` ports unchanged. The commons `BaseServerPacket` stays buffer-free.

```cpp
class AionServerPacket : public commons::network::packet::BaseServerPacket {
public:
	/** true for the 25 packets whose bytes depend on the recipient (generated list; debug builds assert via a recording connection proxy). */
	virtual bool dependsOnConnection() const noexcept { return false; }
	/** Java write(con, buf) minus encryption: [u16 len placeholder][obf opcode][0x44][~op][writeImpl]. Logic thread only. */
	std::shared_ptr<const std::vector<uint8_t>> serialize(AionConnection& con);
protected:
	virtual void writeImpl(AionConnection& con) = 0;       // unchanged Java body: writeD(...), writeS(...)
	void writeD(int32_t v);                                // -> *buf (thread-confined)
private:
	utils::ByteBuffer* buf = nullptr;
};

struct WirePacket {                                        // what crosses to the IO strand
	std::shared_ptr<const std::vector<uint8_t>> frame;     // plaintext frame, shared by all recipients of a broadcast
	enum class Special : uint8_t { NONE, SM_KEY } special = Special::NONE;
};

class AionConnection : public commons::network::AConnection<WirePacket> {
public:
	void sendPacket(std::shared_ptr<AionServerPacket> p);  // logic thread: Outbox::push(sharedFromThis(), std::move(p))
protected:
	bool writeData(utils::ByteBuffer& data) override;      // strand, guard held: copy frame, set len, crypt.encrypt(slice after len)
};

/** Per-logic-thread outbox, flushed after every job. */
class Outbox {
	struct Pending { std::shared_ptr<AionConnection> con; std::shared_ptr<AionServerPacket> pkt; };
	std::vector<Pending> pending;
public:
	void flush() {
		std::unordered_map<const AionServerPacket*, std::shared_ptr<const std::vector<uint8_t>>> once;  // broadcast dedupe
		for (auto& [con, pkt] : std::exchange(pending, {})) {
			if (con->isClosed()) continue;
			auto& cached = once[pkt.get()];
			auto frame = pkt->dependsOnConnection() ? pkt->serialize(*con) : (cached ? cached : (cached = pkt->serialize(*con)));
			con->enqueue(WirePacket{std::move(frame)});                         // commons AConnection::sendPacket (guard + requestWrite)
		}
	}
};
```

### 5.2 Broadcasts and recipient-dependent packets
- `PacketSendUtility::broadcastPacket(obj, pkt)` iterates the KnownList and pushes one entry per recipient.
- On flush, a connection-independent packet is serialized **once**, with the first recipient passed as `con` (no such writeImpl reads it). The frame is shared as `shared_ptr<const vector>`.
- The 25 recipient-dependent packets (SM_PLAYER_INFO race per viewer, SM_MESSAGE staff view, SM_DIALOG_WINDOW…) serialize per recipient.
- In debug builds, `con.getActivePlayer()`/`getAccount()` inside `writeImpl` asserts that the packet declares `dependsOnConnection`. A wrong flag cannot silently send the wrong race obfuscation.

### 5.3 The state-mutating writeImpls
- SM_ATTACK:89 and SM_CASTSPELL_RESULT:181 (`setLastCounterSkill`), SM_PET (mood/gift cooldowns), SM_PLAY_MOVIE (`setCustomState`) and SM_GROUP/ALLIANCE_MEMBER_INFO (`event`) are **unchanged**.
- Their mutation now happens at the end of the sending job on the logic thread. That is legal (I1) and matches Java's "after the sender, before the next packet" as closely as possible.
- For a deduplicated broadcast, the mutation runs once instead of once per recipient. All of these mutations are idempotent sets (verified for SM_ATTACK and SM_GROUP_MEMBER_INFO).
- **SM_KEY** (`writeD(con.enableCryptKey())`) is the only packet that needs connection crypt state. It becomes `WirePacket{special = SM_KEY}`, built and applied on the strand in order, so the key is enabled exactly between that frame and the next.

### 5.4 Other paths
- Packets sent from IO threads: SM_KEY in `initialized()`, and LS/CS link packets, which are value packets. `sendPacketNow(p)` serializes on the calling thread and is only available for packets tagged `ValuePacket` (compile-time trait).
- The 8,192-byte client limit warning and the membership-10 "packet name in chat" echo move to serialization time.
- Encryption is per connection on its strand. The key state is strand-only, so no lock is needed (unlike the login server's CryptEngine).
- A `gameserver.network.packet_serialization = end_of_job | immediate` switch (a debug aid) serializes at `sendPacket` time instead. The `writeImpl` code is identical either way, so any timing-dependent packet can be diagnosed without code changes.

---

## 6. Static data at runtime
- **Immutable holders** (items, npcs, skills, quests, zones…) are loaded before the loop starts, in parallel on startup workers, then published as `DataManager::ITEM_DATA` etc.: plain `const ItemData*` with the owning `unique_ptr` kept in DataManager.
  - Live objects hold `const ItemTemplate*`.
  - IO threads never read DataManager, since `readImpl` is pure and `writeImpl` now runs on logic.
- **`//reload`** (9 holders): parse on the BlockingPool into a new holder. The continuation on logic runs `retired.push_back(std::move(owner)); owner = std::move(fresh); ITEM_DATA = owner.get(); ITEM_DATA->cleanup(); QuestEngine::reload()…` in one job.
  - Old holders are **never freed** (leak-on-reload, logged with size), so every old template pointer and cross-holder IDREF (NpcEquippedGear → old ItemTemplate) stays valid, exactly like Java's GC.
  - It is a dev command, so the memory cost is acceptable.
  - QuestEngine's 27 event maps and the command table are rebuilt inside one job, so no swap scheme is needed.
- **Spawn family:**
  - `SpawnTemplate`, `SpawnGroup` and the Siege/Rift/Vortex/Base/Town/AhserionsFlight variants are `RefCounted` and mutable, owned by `SpawnsData` containers **and** by `VisibleObject::spawnTemplate` and `RespawnTask`.
  - `SpawnEngine::newSingleTimeSpawn` returns `Ref<SpawnTemplate>`.
  - The 47 handler setter calls, `SpawnGroup.poolUsedTemplates`, `SpawnsData.saveSpawn`, and `Event.start/stop` (`addRegularSpawns`, `removeEventSpawnObjects`, `GuideTemplate.setActivated`) run on logic without locks.
  - Removing a SpawnGroup from SpawnsData only drops SpawnsData's Refs; live NPCs keep theirs.
  - The Java quirk of instance spawns sharing static templates across instances is preserved and is no longer a race.
- The generator emits public constructors and setters for the ~15 template types built at runtime (FlyRingTemplate 13×, QueuedNpcSkillTemplate, WalkerTemplate in FixPath, PlayerStatsTemplate…).
- `HostileUpEffect.tempHate` becomes a `mutable` member: a documented Java quirk, now deterministic.
- **Config:**
  - Every runtime rebind path (Config.load from events, `//reload config`, `//configure`, ChatProcessor.reload, `//ai` toggles) runs on logic. So the game-server rule becomes: *fields read only by logic may stay plain*.
  - Fields read by IO, BlockingPool or watchdog (NetworkConfig, PffConfig, ThreadConfig, DatabaseConfig, logging) stay `std::atomic`/`ConfigValue`.
  - This amends CONVENTIONS for the game server. The bind lists also give `//configure` its name table.

---

## 7. Concurrency safety guarantees

**Impossible by construction (I1 plus the API shape):**
- data races on game objects, positions (float x/y/z), strings and containers
- KnownList cross-updates (Java's `KnownList.java:192` mutates the other side's list unsynchronized)
- `isOnline()` then `getClientConnection()` TOCTOU
- periodic saves iterating storages while packets mutate them
- lazy `writeImpl` reading state that another thread mutates
- the geo BIH lazy build: built eagerly before the loop
- QuestEngine and command registries swapped during reload
- refcount races, and non-atomic refcounts are therefore correct
- template reload tearing

**What still needs synchronization, and has it:**
1. the executor inbox (mutex + condvar)
2. the AConnection send queue and close state (commons `guard`)
3. `AionConnection::state` (`std::atomic`, written on logic, read by the factory on IO)
4. the DB pool (existing)
5. IDFactory (a mutex, for startup and future off-thread callers)
6. logger and RunnableStatsManager (existing)
7. IO-read config (atomic/ConfigValue)
8. watchdog heartbeat atomics

The crypt and PFF maps are strand-confined.

**Single-threaded UB and how it is avoided:**

| UB source | Mitigation |
|---|---|
| Object destroyed while one of its member functions still runs (`AIActions.deleteOwner(this)` then `getOwner()`), or a raw local outliving the last Ref | I2: nothing is freed before the job ends |
| Iterator invalidation by reentrant callbacks (`KnownList.clear` → `del` → `notifyNotKnow` → AggroList; `destroyInstance` deleting while iterating; AI events during broadcasts) | I5 `StableMap`/`StableSet`. Plain `std::vector` members that callbacks may mutate are iterated by index with a size re-check, or through a copied `SmallVector<Ref<T>>` |
| Destroying a `std::function` while it runs (self-cancel) | Future state machine (§4.1) |
| Null dereference: Java code full of latent NPEs, logged by RunnableWrapper while the world continues | `Ref`/`Ptr` `operator->` throws `NullPointerException` (one predicted branch). The job boundary logs it. Raw `T*` for game objects is banned by lint, so the remaining crash surface is container indexing (use `.at()` in handler-facing helpers, keep iterator debugging on in Debug) |
| Stored raw pointer or `[this]` capture without keepAlive | Lint: CI script over headers and lambdas (regex on `schedule*(` captures, `T*` fields where T derives from RefCounted). Debug builds also validate "object alive" on every `Ptr` deref through the zombie flag. clang-cl ASan is available on Windows |
| Exceptions escaping jobs, destructors, IO handlers | Jobs catch and log. Destructors are `noexcept` and may only release refs and ids: no events, no packets, no new Refs to `this` |
| Refcount touched off-thread (a Ref captured into a BlockingPool job) | Debug `AION_ASSERT_LOGIC_THREAD` in `refAdd`/`refRelease` |
| Java arithmetic semantics | existing CONVENTIONS (unsigned hashing, saturating float→int, `Math.round`) |

---

## 8. Porting mechanics (side by side)

### (a) AI handler: delayed action capturing `this`, checking `isDead()` later
Java: `ai/instance/abyssal_splinter/YamenessPortalSummonedAI.java`
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
void YamenessPortalSummonedAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	ThreadPoolManager::getInstance().schedule(task(this, &YamenessPortalSummonedAI::spawnSummons), 12000);
}
void YamenessPortalSummonedAI::spawnSummons() {
	if (isDead() || !getOwner()->isSpawned()) return;
	spawn(281903, getOwner()->getX() + 3, getOwner()->getY() - 3, getOwner()->getZ(), 0);
	ThreadPoolManager::getInstance().schedule([this, keep = keepAlive()] {       // the only design-specific edit
		if (!isDead() && getOwner()->isSpawned())
			spawn(281903, getOwner()->getX() + 3, getOwner()->getY() - 3, getOwner()->getZ(), 0);
	}, 60000);
}
```
- `keepAlive()` is a Ref to the owning Npc; the AI is a RefPart.
- If the portal dies and decays at 2 s and is deleted, the Npc object stays valid for the rest of the 60 s. `isDead()` and `isSpawned()` answer truthfully, and the objectId is released only after the task finishes. This is Java's Cleaner timing.

Java: `KaluvaSpawnAI` (Future field)
```java
private Future<?> task;
protected void handleDied() { super.handleDied(); if (task != null && !task.isDone()) task.cancel(true); checkKaluva(); }
```
C++: `FutureRef task;` and `if (task && !task->isDone()) task->cancel(true);`. The cancel destroys the callable, which releases `keep` right away.

### (b) Instance handler keeping Npcs and spawning adds
Java: `instance/crucible/EmpyreanCrucibleInstance.java` and `ai/instance/beshmundirTemple/SacrificialSoulAI.java`
```java
private final List<Npc> npcs = new ArrayList<>();
private class EmpyreanStage { private final List<Npc> stageNpcs;
	private boolean containsNpcs() { for (Npc npc : instance.getNpcs()) for (Npc stageNpc : stageNpcs) if (npc.equals(stageNpc)) return true; return false; } }
public void onDie(Npc npc) { synchronized (npcs) { npcs.remove(npc); } ... sp(217756, 342.65106f, 357.4013f, 96.09094f, (byte) 0); }
// SacrificialSoulAI
private Npc boss;
boss = getPosition().getWorldMapInstance().getNpc(216263);
if (boss != null && !boss.isDead()) { AIActions.targetCreature(this, boss); getMoveController().moveToTargetObject(); }
```
C++:
```cpp
std::vector<Ref<Npc>> npcs;
struct EmpyreanStage { std::vector<Ref<Npc>> stageNpcs;
	bool containsNpcs(const WorldMapInstance& instance) const {
		for (Ptr<Npc> npc : instance.getNpcs()) for (const auto& stageNpc : stageNpcs) if (npc->equals(stageNpc)) return true; return false; } };
void EmpyreanCrucibleInstance::onDie(Ptr<Npc> npc) { CollectionUtil::removeFirstEqual(npcs, npc); /* synchronized dropped */ ... sp(217756, 342.65106f, 357.4013f, 96.09094f, 0); }
// SacrificialSoulAI
Ref<Npc> boss;
boss = getPosition()->getWorldMapInstance()->getNpc(216263);      // Ptr<Npc> -> Ref<Npc>
if (boss && !boss->isDead()) { AIActions::targetCreature(*this, boss); getMoveController()->moveToTargetObject(); }
```
- A deleted stage Npc stays alive while it is in `stageNpcs`, so `equals` by objectId can never alias a new Npc (the same guarantee Java had).
- `sp(...)` creates a runtime `Ref<SpawnTemplate>` owned by the new Npc.
- No instance ↔ Npc cycle forms, because positions hold only a `WeakRef` to the instance (§2.2). After `destroyInstance`, the handler and its `npcs` are freed once no task holds `keepAlive()` of the handler.

### (c) Effect whose effector is despawned mid-DoT
Java: `AbstractOverTimeEffect.startEffect`, `PoisonEffect.onPeriodicAction`, `NpcController.onAttack`
```java
Future<?> task = ThreadPoolManager.getInstance().scheduleAtFixedRate(() -> onPeriodicAction(effect), initialDelay, checktime);
effect.setPeriodicTask(task, position);
// PoisonEffect
Creature effected = effect.getEffected();
effected.getController().onAttack(effect, TYPE.DAMAGE, effect.getReserveds(position).getValue(), false, LOG.POISON, hopType, effect.isMagicalCritical(position));
effected.getObserveController().notifyDotAttackedObservers(effect.getEffector(), effect);
// NpcController.onAttack
if (attacker instanceof Summon && attacker.isSpawned()) actingCreature = attacker; else actingCreature = attacker.getActingCreature();
```
C++:
```cpp
FutureRef task = ThreadPoolManager::getInstance().scheduleAtFixedRate(
	[this, effect = Ref(&effect)] { onPeriodicAction(*effect); }, initialDelay, checktime);   // `this` = immortal template: no keepAlive
effect.setPeriodicTask(task, position);
// PoisonEffect
Ptr<Creature> effected = effect.getEffected();
effected->getController()->onAttack(effect, TYPE::DAMAGE, effect.getReserveds(position)->getValue(), false, LOG::POISON, hopType, effect.isMagicalCritical(position));
effected->getObserveController()->notifyDotAttackedObservers(effect.getEffector(), effect);
// NpcController::onAttack
auto* summon = dynamic_cast<Summon*>(attacker.get());
Ptr<Creature> actingCreature = (summon && summon->isSpawned()) ? attacker : attacker->getActingCreature();
```
- `Effect` holds `const Ref<Creature> effector, effected`. When the effector Summon is released and deleted, only its **own** effects end.
- This Effect keeps the stale Summon readable (`isSpawned()` false, `getActingCreature()` returns its master through the master Ref) until the DoT ends. Then the callable and the Effect are released, and at the end of that job the Summon is freed and its id quarantined.
- The cycle effected → EffectController → Effect → periodic Future → callable → Effect is broken by `endEffect` cancelling the periodic tasks.

### (d) Group keeping a logged-out player
Java: `PlayerTeamMember`, `PlayerGroupService.OfflinePlayerChecker`, `PlayerConnectedEvent`
```java
final Player player;                   // PlayerTeamMember
group.forEachTeamMember(member -> { if (!member.isOnline() && TimeUtil.isExpired(member.getLastOnlineTime() + GroupConfig.GROUP_REMOVE_TIME * 1000))
	group.onEvent(new PlayerGroupLeavedEvent(group, member.getObject(), LeaveReson.LEAVE_TIMEOUT)); });
group.removeMember(player.getObjectId());
group.addMember(new PlayerGroupMember(player));
if (player.equals(group.getLeader().getObject())) { ... }
```
C++:
```cpp
const Ref<Player> player;              // keeps the logged-out (deleted, connection-less) Player readable for up to 600 s
group->forEachTeamMember([&](PlayerGroupMember& member) {       // StableMap: removal from inside the callback is fine
	if (!member.isOnline() && TimeUtil::isExpired(member.getLastOnlineTime() + GroupConfig::GROUP_REMOVE_TIME * 1000))
		group->onEvent(PlayerGroupLeavedEvent(group, member.getObject(), LeaveReson::LEAVE_TIMEOUT)); });
group->removeMember(player->getObjectId());                    // onRemoveMember: old instance ->setPlayerGroup(nullptr) - cycle broken
group->addMember(makeRef<PlayerGroupMember>(player));          // new instance, same DB objectId
if (player->equals(group->getLeader()->getObject())) { ... }   // objectId equality: old vs new instance compare equal
```
After the swap the old instance is released: its inventory, stats and parts are freed at the end of the job, unless an effect or task still holds it.

### (e) `PacketSendUtility.broadcastPacket` of an SM_ packet
Java: `ai/instance/abyssal_splinter/KaluvaAI.java:41`
```java
PacketSendUtility.broadcastPacket(getOwner(), new SM_EMOTION(getOwner(), EmotionType.CHANGE_SPEED, 0, getObjectId()));
```
C++:
```cpp
PacketSendUtility::broadcastPacket(getOwner(), makePacket<SM_EMOTION>(getOwner(), EmotionType::CHANGE_SPEED, 0, getObjectId()));

void PacketSendUtility::broadcastPacket(Ptr<VisibleObject> object, std::shared_ptr<AionServerPacket> packet) {
	object->getKnownList()->forEachPlayer([&](Ptr<Player> player) { sendPacket(player, packet); });
}
void PacketSendUtility::sendPacket(Ptr<Player> player, std::shared_ptr<AionServerPacket> packet) {
	if (auto con = player->getClientConnection()) con->sendPacket(std::move(packet));    // one read: no TOCTOU; -> Outbox
}
```
- SM_EMOTION is connection-independent, so it is serialized once at the end of the job and N connections share one frame.
- A lazy packet like `SM_ATTACK(attacker, target, …)` keeps `Ref<Creature>` fields, reads HP% at flush, and runs its `setLastCounterSkill` there.

### (f) DAO save at logout and the periodic save
Java: `PlayerLeaveWorldService.leaveWorld` (tail) and `GeneralUpdateTask.run`
```java
PlayerService.storePlayer(player);
player.getInventory().setOwner(null); ...
PlayerDAO.onlinePlayer(player, false); // marks that player was fully saved and may enter world again
con.setActivePlayer(null);
// GeneralUpdateTask
Player player = World.getInstance().getPlayer(playerId);
if (player != null) { try { AbyssRankDAO.storeAbyssRank(player); PlayerSkillListDAO.storeSkills(player); PlayerQuestListDAO.store(player); PlayerDAO.storePlayer(player); for (House house : player.getHouses()) house.save(); }
	catch (Exception ex) { log.error("Exception during periodic saving of player " + player.getName(), ex); } }
```
C++:
```cpp
PlayerService::storePlayer(player);                  // inline on logic: ~17 DAOs, local MariaDB est. 5-30 ms; no race with gameplay
player->getInventory()->setOwner(nullptr); ...
PlayerDAO::onlinePlayer(player, false);
con->setActivePlayer(nullptr);                       // drops the connection's Ref<Player>
// GeneralUpdateTask (captures only playerId, as in Java)
if (Ptr<Player> player = World::getInstance().getPlayer(playerId)) {
	try { AbyssRankDAO::storeAbyssRank(player); PlayerSkillListDAO::storeSkills(player); PlayerQuestListDAO::store(player); PlayerDAO::storePlayer(player);
	      for (Ptr<House> house : player->getHouses()) house->save(); }
	catch (...) { log.errorCurrentException("Exception during periodic saving of player " + player->getName()); }
}
// Opt-in offload for a pure-SQL job (not for Player saves):
BlockingPool::submit([] { return SomeRankingDAO::loadRows(); }, [](auto rows) { if (rows) AbyssRankingCache::getInstance().apply(std::move(*rows)); });
```

### (g) `CreatureController.addTask(TaskId, schedule(...))` and cancel on despawn
Java: `CreatureController:368-427`, `PlayerLeaveWorldService.leaveWorldDelayed`, `DropService:117-121`
```java
tasks.compute(taskId.ordinal(), (k, oldTask) -> { if (oldTask != null) { oldTask.cancel(false); if (taskId == TaskId.DESPAWN) log.warn(...); } return task; });
for (Entry<Integer, Future<?>> e : tasks.entrySet()) { ... task.cancel(false); } tasks.clear();
Future<?> leaveWorldTask = ThreadPoolManager.getInstance().schedule(() -> leaveWorld(player), delayInMillis);
player.getController().addTask(TaskId.DESPAWN, leaveWorldTask);
ScheduledFuture<?> decayTask = (ScheduledFuture<?>) npc.getController().cancelTask(TaskId.DECAY);
if (decayTask != null) dropNpc.setRemaingDecayTime(decayTask.getDelay(TimeUnit.MILLISECONDS));
```
C++:
```cpp
std::array<FutureRef, magic_enum::enum_count<TaskId>()> tasks;      // Java: ConcurrentHashMap<Integer, Future<?>>
void CreatureController::addTask(TaskId taskId, FutureRef task) {
	auto& slot = tasks[std::to_underlying(taskId)];
	if (slot) { slot->cancel(false); if (taskId == TaskId::DESPAWN) log.warn("Despawn task for {} was cancelled and replaced ...", *getOwner()); }
	slot = std::move(task);
}
FutureRef CreatureController::getAndRemoveTask(TaskId id) { return std::exchange(tasks[std::to_underlying(id)], nullptr); }
void CreatureController::cancelAllTasks() { for (auto& t : tasks) if (auto f = std::exchange(t, nullptr)) f->cancel(false); }
void CreatureController::onDelete() { cancelAllTasks(); VisibleObjectController::onDelete(); }

void PlayerLeaveWorldService::leaveWorldDelayed(Ptr<Player> player, int64_t delayInMillis) {
	auto leaveWorldTask = ThreadPoolManager::getInstance().schedule([player = Ref(player)] { leaveWorld(player); }, delayInMillis);
	player->getController()->addTask(TaskId::DESPAWN, leaveWorldTask);
}
if (FutureRef decayTask = npc->getController()->cancelTask(TaskId::DECAY)) dropNpc->setRemaingDecayTime(decayTask->getDelay().count());
```
- A cancelled Future still answers `getDelay()`; this is how the DropService code reads the remaining decay time.
- The cycle Player → tasks → Future → callable → Player lasts only until the task runs or is cancelled.

### (h) MoveTaskManager's periodic NPC movement
Java: `taskmanager/tasks/MoveTaskManager.java`
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
StableMap<int32_t, Ref<Creature>> movingCreatures;
void MoveTaskManager::run() {                                         // fixed-rate 200 ms job on the logic thread
	movingCreatures.forEach([this](int32_t, Ref<Creature>& creature) {  // removal and addition from AI callbacks are safe (I5)
		if (!creature->isSpawned()) { if (removeCreature(creature)) log.warn("{} was still in moving creatures list but already despawned", *creature); return; }
		creature->getMoveController()->moveToDestination();
		if (creature->getAi()->isDestinationReached()) { removeCreature(creature); creature->getAi()->onGeneralEvent(AIEventType::MOVE_ARRIVED); ZoneUpdateService::getInstance().add(creature); }
		else creature->getAi()->onGeneralEvent(AIEventType::MOVE_VALIDATE);
	});
}
```
Deviation: creatures move in insertion order instead of in parallel, so KnownList and AI side effects of one creature are visible to the next.

### Mechanical port estimate

**The ~850 `schedule`/`scheduleAtFixedRate` sites**, plus 15 execute/submit, 21 cron and ~100 delayed `broadcastMessage` calls:

| Share | What changes per site |
|---|---|
| ~75% | Syntax only plus the capture list: `[this, keep = keepAlive()]` or `task(this, &X::m)`, and `[player = Ref(player)]` for captured locals. Bodies unchanged. Regex- or LLM-friendly |
| ~20% | Also a type rename: `Future<?>` → `FutureRef` (286 field declarations), `Future<?>[]` → `std::array<FutureRef,N>`, `List<Future<?>>` → `std::vector<FutureRef>`, `AtomicReference<Future>` → shim |
| ~5% (≈40) | Needs a human look: self-cancel through an array holder, the 121 fire-and-forget periodic tasks in AI files that never self-terminate (same leak in Java, but check the census), `cancel` inside `run` |
| 2 | Redesign: FixPath as a coroutine, and TeleportService/CM_TELEPORT_ANIMATION_DONE with `deferred` + `runNowIfPending` |

**The ~1,600 handler files:** the design adds no structural work.
- Type mapping: Java object type → `Ptr<T>` for params and locals, `Ref<T>` for fields and captures; `.` → `->` on game objects.
- `synchronized` blocks are deleted, and `Atomic*`/CHM become shims.
- `instanceof X x` becomes `auto* x = dynamic_cast<X*>(p.get())`.
- Design-specific edits are under 3% of handler lines, mostly capture lists. Everything else is the Java→C++ translation you would do under any model.
- The 1,035 quest handlers schedule in only 36 files, so they are almost untouched by this design.

---

## 9. Performance, debuggability, effort

### 9.1 Performance for the realistic target
- **Objects:**
  - ~131,896 spawn spots, of which world maps spawn perhaps 60-100k objects at startup (instances and events only on demand).
  - At 1.5-3 KB per Npc including parts, that is ~150-300 MB. Refcount overhead is 16 bytes per object.
  - Geo is ~300 MB (dao-geo estimate), and static data a few hundred MB. Expect about 1 GB in total.
- **Idle cost is near zero:** AI thinking, NpcKnownList updates and shouts are gated by `isMapRegionActive()` (ThinkEventHandler:32, NpcKnownList:17), so only the neighbourhoods of the few players cost CPU. The loop sleeps until the next deadline.
- **Movement tick:** assume up to ~2,000 moving creatures near players. At 5-20 µs each (vector math, `World.updatePosition`/KnownList, optional geo ray), a 200 ms tick costs 10-40 ms of one core. A siege with thousands of NPCs is the unknown; see the escape hatch in §1.3 and prototype 1.
- **Hot-path costs:** refcount inc/dec about 1 ns (non-atomic, no contention). StableMap iteration is a dense vector scan. Timer operations O(log n) on ~50k entries. Broadcasts serialize once per packet. SM_NPC_INFO-sized writes take microseconds.
- **Latency:** client packets start within a millisecond unless a long job is running. The watchdog names any job over 5 s, and the stats page lists per-type totals (RunnableStatsManager). DB-inline hitches (enter world ~10-40 ms, logout save ~5-30 ms, 900 s periodic saves) are invisible at a few players.

### 9.2 Debuggability
- **Deterministic:** one thread, FIFO inbox, `(due, seq)` timer order, a single seedable `Rnd` stream. Recording the inbox (decoded client packets with timestamps) plus the seed allows replay.
- **Testable:** `ManualClock` + `runReady()/advance()`. A GoogleTest can spawn an instance, advance 22 s, and assert that KaluvaSpawnAI hatched adds and the leak census is zero after `destroyInstance`.
- **Readable failures:**
  - Stacks are complete (no pool hops).
  - The Java-style NPE is logged per job with a `std::stacktrace` (commons Exception).
  - Crash minidumps come from an unhandled-exception filter.
  - Leak census: `//debug leaks` lists objects deleted more than N minutes ago that are still alive. A debug build can record which Refs point at them (optional per-Ref registry).
- **Caveat:** a breakpoint freezes the whole world, so client ping and alive checks time out. A config flag relaxes the timeouts in Debug.

### 9.3 Foundation effort (C++ lines including tests; one experienced developer)

| Component | LOC (impl + tests) |
|---|---|
| GameExecutor (loop, inbox, 4-ary timer heap, ManualClock, JobScope, stats, Windows timer resolution) | 900 + 700 |
| ThreadPoolManager facade, Future state machine, deferred/runNow, BlockingPool + continuations | 500 + 500 |
| RefCounted/Ref/Ptr/WeakRef/RefPart/keepAlive, Reclaimer, leak census, debug thread asserts | 450 + 400 |
| StableMap/StableSet, Atomic*/CHM shims, CollectionUtil helpers | 500 + 400 |
| CronExpression (Quartz subset) + CronService + CronServiceTest vectors | 700 + 500 |
| Packet outbox, AionServerPacket serialize/`buf`, WirePacket, SM_KEY strand path, connection-dependence debug probe, game Crypt | 700 + 600 |
| IDFactory with quarantine | 200 + 150 |
| GameTask coroutine (sleep, waitUntil, onBlockingPool) | 250 + 200 |
| Watchdog, console/Ctrl handler, shutdown pump, lint script (Python) | 300 + 150 |
| **Total** | **~4.5k + ~3.6k ≈ 8k lines** |

At a hobby pace that is roughly 3-5 weeks of focused work before any object-model header. The pieces are small and have no dependencies on each other, so they unit-test in isolation. It must exist before the spine headers (Player/Creature/Effect) are frozen.

---

## 10. Risks and what to prototype first

| Risk | Impact | Mitigation / validation |
|---|---|---|
| Single-core ceiling (siege, world raid, many AI + movers) | stutter at scale | Profile a synthetic 200 ms tick (prototype 1). Two-phase parallel movement as a fallback. Per-instance executors would need atomic refs, a major refactor, so they are out of scope |
| Heavy DB on logic (character list for big accounts, legion loads, ranking updates, admin queries) | world hitches | Measure (prototype 4). Move pure-SQL jobs to BlockingPool. Pre-warm the pool |
| Refcount cycles that Java's GC collected | memory and id leaks | I7 hooks, weak position → instance, leak census in CI tests and at runtime |
| Timing deviations: end-of-job serialization, deterministic `execute`/`schedule(0)` after the current job, earlier cancel release, quarantined id reuse, sequential movement, ordered LS/CS packets | subtle behaviour changes, some of which exposed or hid Java races | List in DEVIATIONS.md. `packet_serialization=immediate` switch. Real-client testing |
| One bad handler crashes the whole world (C++ UB instead of a Java NPE) | lost session state | Checked `Ref`/`Ptr` deref throwing NPE, raw-pointer lint, `.at()` helpers, ASan runs, minidumps, frequent periodic saves |
| Discipline drift during mass porting (Ptr stored in a field, `[this]` without keepAlive) | use-after-free weeks later | Lint in CI, debug alive checks on every Ptr deref, handler tests with ManualClock covering despawn and instance destroy mid-delay |
| Hollow-shell instance semantics differ in rare late tasks | a task after instance destroy does nothing instead of Java's odd behaviour | Acceptable. Log once per shell access in Debug |
| Shutdown pumping reentrancy | deadlock or double logout | Dedicated test with connected fake clients |

**Prototype first, in order:**
1. **Core runtime microbenchmark:**
   - GameExecutor + Ref + zombie reclaim + StableMap.
   - Spawn 80k Npc shells with KnownLists over a region grid, move 2,000 of them every 200 ms with synthetic AI events and SM_MOVE broadcasts to 5 fake connections.
   - Measure tick time, memory and reclaim cost.
2. **Vertical handler slice with ManualClock:**
   - CreatureController tasks, YamenessPortalSummonedAI, KaluvaSpawnAI, and an Abyssal Splinter-like instance with a `Ref<Npc>` field.
   - Scenarios: despawn mid-delay, cancel in `handleDied`, `destroyInstance` while a 60 s task is pending.
   - Assert: no ASan errors, ids released only after the tasks finish, leak census zero.
3. **Packet path against the real 4.8 client:**
   - SM_KEY special frame on the strand, deduplicated broadcast frame, per-recipient SM_PLAYER_INFO.
   - A packet that mutates in `writeImpl` (SM_PLAY_MOVIE).
   - Confirm the client tolerates batched writes on the game connection.
4. **DB-inline hitch:** `PlayerService.getPlayer` + `storePlayer` for a character with full inventory, warehouse and quests against local MariaDB. Record p50/p99 job time to decide whether any BlockingPool offload is needed.
5. **Relogin + group:**
   - Log out in a group, relogin within 600 s, and another time let the 600 s timeout fire.
   - Check `PlayerConnectedEvent` swap, `World.removeObject` identity check, old instance freed, census clean.
6. **Effect with a despawned effector:** a summon's DoT on an Npc, summon released mid-DoT. Check `NpcController.onAttack` behaviour, reclaim after the effect ends, id quarantine.
7. **Scheduler edge cases:** `runNowIfPending` on deferred and scheduled Futures with an exception each way, self-cancel of a periodic task, `getDelay` on a cancelled decay task, FixPath coroutine with a fake client moving.
8. **Porting-cost calibration:** port 20 AI and 10 instance handler files with the lint enabled; measure minutes per file and design-specific edits per 100 lines.
9. **CronExpression** against the Java CronServiceTest vectors and the siege/rift/world_raid schedule XMLs, including a DST transition in the configured zone.

**New entries for DEVIATIONS.md:**
- Single logic thread replaces PacketProcessor and the pools.
- End-of-job packet serialization and the SM_KEY strand frame.
- Ordered LS/CS packets.
- Cancelled tasks release their captures immediately.
- ID release quarantine.
- Sequential movement tick.
- Position holds a weak reference to its instance, with the hollow shell.
- Leak-on-reload of static data holders.
- Eager BIH (already recommended by the geo research).
- `ThreadPoolManager` names kept on a re-architected implementation.
- NN training on a copied dataset: the model is used untrained until training finishes, where Java raced.

## Porting cost

Foundation: about 8k C++ lines including tests (executor and timers ~1.6k, Future and BlockingPool ~1k, Ref/Ptr/WeakRef/reclaimer/census ~0.85k, StableMap and shims ~0.9k, cron ~1.2k, packet outbox and crypt glue ~1.3k, IDFactory quarantine, coroutine, watchdog and lint ~1.2k). That is roughly 3-5 focused weeks, and it must land before the spine headers are frozen.

Schedule sites (about 850, plus about 136 execute/cron/delayed-broadcast sites):
- ~75% change only the capture list: `[this, keep = keepAlive()]`, `task(this, &X::m)`, `[player = Ref(player)]`.
- ~20% also rename a type: Future<?> becomes FutureRef, and arrays or lists of futures become std::array/std::vector.
- ~5% (about 40) need review: self-cancel holders, never-ending periodic tasks, cancel inside run.
- 2 need a redesign: FixPath as a coroutine, and the teleport run-now through deferred + runNowIfPending.

Handlers (about 1,600 files, 158k lines): design-specific edits are under 3% of lines, mostly capture lists. The rest is the Java-to-C++ translation any model needs:
- Ptr/Ref type mapping, `.` to `->`, dynamic_cast for instanceof.
- synchronized deleted; Atomic* and ConcurrentHashMap replaced by API-identical shims.
- writeImpl bodies, DAO bodies and quest handlers (only 36 quest files schedule) port without design changes.

Calibrate the per-file cost with prototype 8 (20 AI files and 10 instance files) before batching phase 6.

## Weaknesses

- Single-core ceiling: game logic cannot use more than one core. Large sieges or world raids with thousands of active NPCs may exceed the 200 ms movement budget. The only cheap fallback is a two-phase parallel movement tick; true multi-core logic would need atomic refcounts and a major refactor.
- Blocking DB calls run on the logic thread by default: enter world (~40 queries plus N+1 stone loads), logout storePlayer, 900 s periodic saves, legion/broker/housing loads and heavy cron or admin SQL stall the whole world for their duration. Moving them off-thread is only mechanical for pure-SQL jobs, not for Player load or save.
- Reference counting does not collect cycles the way Java's GC did. Correctness depends on the Java delete, despawn and logout hooks breaking cycles (KnownList, aggro, controller tasks, self-effects, observers, team membership), on the weak position-to-instance reference, and on a leak census. Missed cases leak memory and object IDs silently until the census reports them.
- Timing deviations from Java:
- packets are serialized at the end of the job instead of whenever the NIO thread wrote them
- execute/schedule(0) always run after the current job
- cancelled tasks release their captured objects immediately
- IDs are released deterministically, behind a quarantine
- movement is sequential in insertion order
- LS/CS packets are ordered
Each is defensible, but some latent Java races become deterministic orderings, which can expose or hide behaviour differences.
- A C++ fault in any handler (null raw pointer, bad index, UB) takes down the whole world, whereas Java only logged an NPE for one task. Checked Ref/Ptr dereference, lint rules and ASan reduce the risk but cannot remove it.
- The model relies on porting discipline: store Ref and never Ptr/raw, add keepAlive to captures, use StableMap for anything iterated reentrantly. Violations are use-after-free bugs that show up late. The design depends on a CI lint script and debug-build alive and thread asserts actually being run.
- Hollow-shell instances: a late task on a stale object whose instance was destroyed and freed sees an empty no-op instance instead of Java's destroyed-but-intact one. This is a small semantic change in rare paths.
- Deferred reclamation holds garbage until the end of the job. Very long jobs (startup spawnAll, big cron jobs, instance destruction) peak higher in memory, and destructors must stay trivial: no events, no packets.
- Debugging with breakpoints freezes the entire world, so connected clients time out unless the timeouts are relaxed in Debug builds.
- Leak-on-reload: every //reload of items, skills or quests keeps the old holder forever (tens of MB each). This is fine for dev use, not for frequent reloads.
- Shutdown needs a special pump, because NioServer::shutdown waits for onDisconnect work that must run on the logic thread. It is a reentrancy corner that needs its own test.

## Prototype first

- Core runtime microbenchmark:
- GameExecutor + non-atomic Ref + zombie reclaim + StableMap.
- Spawn 80k Npc shells with KnownLists on a region grid, move 2,000 of them every 200 ms with synthetic AI events and SM_MOVE broadcasts to 5 fake connections.
- Measure tick time, memory and reclaim cost.
- Vertical handler slice with ManualClock:
- CreatureController tasks, YamenessPortalSummonedAI, KaluvaSpawnAI, and an Abyssal Splinter-like instance holding Ref<Npc> fields.
- Test despawn mid-delay, cancel in handleDied, and destroyInstance while a 60 s task is pending.
- Assert: ASan clean, object IDs released only after the tasks finish, leak census zero.
- Packet path against the real 4.8 client:
- SM_KEY special frame applied on the strand, end-of-job flush with a deduplicated broadcast frame, per-recipient SM_PLAYER_INFO.
- SM_PLAY_MOVIE mutating state in writeImpl.
- Confirm the client tolerates batched writes on the game connection.
- DB-inline hitch measurement: PlayerService.getPlayer and storePlayer for a character with full inventory, warehouse and quests against local MariaDB. Record p50/p99 job time to decide whether any BlockingPool offload is needed.
- Relogin with a group:
- Log out while in a group, relogin within 600 s, and another time let the OfflinePlayerChecker timeout fire.
- Verify the PlayerConnectedEvent swap, the World.removeObject identity check, that the old Player instance is freed, and that the leak census is clean.
- Effect with a despawned effector: a summon's DoT on an Npc, with the summon released mid-DoT. Verify the NpcController.onAttack behaviour, reclamation after the effect ends, and the ID quarantine.
- Scheduler edge cases:
- runNowIfPending on deferred and scheduled futures, with an exception thrown in each case.
- Self-cancel of a periodic task, and getDelay on a cancelled decay task.
- The FixPath coroutine driven by a fake client's movement packets.
- Porting-cost calibration: port 20 AI and 10 instance handler files with the capture/raw-pointer lint enabled. Measure minutes per file and design-specific edits per 100 lines, to validate the mechanical-port estimate before phase 6 batching.
- CronExpression and CronService: test against the Java CronServiceTest vectors and the siege/rift/world_raid schedule XMLs, including a DST transition and a wall-clock adjustment in the configured time zone.
