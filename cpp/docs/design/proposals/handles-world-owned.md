# Proposal: One world thread, registry-owned objects, anchored tasks and generational handles: runtime architecture for the C++ Aion 4.8 game server

> One of three competing proposals scored by the design panel (not the chosen design; see ../runtime-architecture.md).

All game state lives on one world thread. The main thread becomes that thread after startup. It is an event loop, not a fixed tick: a timer heap plus an inbox that other threads post to. N Asio IO threads do only framing, crypto and pure readImpl. A small blocking pool runs database and other slow work, and posts results back to the world thread. Game objects have exactly one owner: World, WorldMap, the team registries, EffectController, Storage or DetachedPlayers. Everything that has to survive past the current unit of world-thread work holds a generational handle: Ref<T> = objectId + generation, PlayerId, EffectRef, or a TaskHandle into a slot map. Resolving a handle returns nullptr once the object is gone. Scheduled work is attached to a TaskAnchor on the object that owns it. A task can safely capture that object's `this`, and deleting the owner cancels and destroys all of its tasks right away. Objects removed during a unit of work are destroyed only after it finishes (a per-turn graveyard), so Java's "delete, then keep using getOwner()" idiom stays valid. Object IDs are released when the object is destroyed, then held in a fixed quarantine before reuse. This replaces Java's GC Cleaner deterministically. Server packets are serialized eagerly on the world thread into shared immutable bytes and encrypted per connection on the strand. Static templates are immortal `const T*`. Reloads retire old holders and never free them. The spawn family is world-thread-mutable shared data. This design makes data races, cross-time use-after-free, ABA and iterator invalidation impossible by construction, or checks them on every access. Most of the ~250 synchronized blocks, ~400 Atomic* uses and ~250 concurrent collections become plain code. About 80% of the 684 handler schedule sites port almost verbatim. The costs are honest: explicit null handling (effector, targets, handler fields), a per-site scope decision for the few tasks that must outlive their owner, a single-core ceiling, and inline DB stalls until the heavy load and save paths are offloaded.

# Runtime architecture for the C++ game server

**Decision in one line:** one world thread with thread confinement, registry ownership, anchored tasks, generational handles, a per-turn graveyard, eager packets and immortal templates.

Sources for all claims: the research maps in `cpp/build/workflows/gs-research/*.md`. I re-verified these against the Java source:
- `ThreadPoolManager`, `CreatureController.addTask/cancelAllTasks`, `MoveTaskManager`, `AionObject` (Cleaner), `World.removeObject/despawn`
- `RespawnService`, `Effect`, `AbstractOverTimeEffect`, `PoisonEffect`, `NpcController.onAttack`, `CreatureController.onAttack`, `AggroList.isAware`
- `PlayerTeamMember`, `PlayerGroupService.OfflinePlayerChecker`, `PlayerConnectedEvent`, `SM_GROUP_MEMBER_INFO`, `PlayerLeaveWorldService.leaveWorld`
- `PlayerEnterWorldService` save tasks, `PacketSendUtility`, `SM_NPC_INFO`, `CM_TELEPORT_ANIMATION_DONE`, `TeleportService.sendLoc`, `DropService` getDelay, `FixPath`
- handlers: `YamenessPortalSummonedAI`, `KaluvaSpawnAI`, `EnragedNightmareAI`, `SacrificialSoulAI`, `CaptainXastaAI`, `AturamSkyFortressInstance`, `LowerUdasTempleInstance`, `EmpyreanCrucibleInstance`, `DanuarReliquaryInstance`

I also ran a classification script over all 684 `ThreadPoolManager.getInstance().schedule*` sites in `data/handlers` (details in section 8).

---

## 0. Decisions at a glance

| Topic | Decision |
|---|---|
| Game logic threads | **One world thread.** It runs every client packet runImpl, every task, cron callback and periodic manager, AI, movement, geo query and service call. It is the main thread after startup. |
| Other threads | N Asio IO threads (accept, read, decrypt, readImpl, write, encrypt); disconnect executor (posts to world); blocking pool (DB/slow jobs, posts results back); watchdog; console reader; logging sinks. No Quartz, ForkJoin, Timer or Cleaner threads. |
| Cross-thread handover | `GameExecutor::post()`: an MPSC inbox. Other threads never see game objects or handles, only values, `unique_ptr`s of objects no one else can reach, and immutable bytes. |
| Game object ownership | Unique ownership (`std::unique_ptr`) by a registry: World (NPCs, summons, gatherables, statics, pets, players in world), WorldMap (instances), team registries, EffectController (effects), Storage/Mailbox/Broker/Repurchase (items), HouseRegistry (house objects), DetachedPlayers (logged-out team members). |
| References across time | Generational handles: `Ref<T>` {objectId, generation}, `PlayerId`, `EffectRef`, `InstanceRef`, `TaskHandle`. They are resolved at use and return `nullptr` when the object is gone. |
| References within a turn | Plain `T&`/`T*` on the stack. Guaranteed valid because destruction is deferred to the end of the turn (graveyard). |
| Task lifetime | `TaskAnchor` member on each scheduling owner (object, AI, instance handler, Effect, Skill). Unpublishing or destroying the owner cancels its tasks and destroys their callables. `[this]` in anchored tasks is safe by construction. |
| ID release | When the object is destroyed, the ID goes into a quarantine (default 60 s), then back to IDFactory. RespawnService deferral is kept. Items are released after DB delete, also through quarantine. The Java "never released" group stays that way. |
| Packets | Eager serialization on the world thread into `shared_ptr<const bytes>`: once per broadcast, or per recipient for the 25 connection-dependent packets. Encryption per connection on the strand. |
| Static data | Templates are immortal `const T*`. The 9 reloadable holders swap an atomic pointer and retire the old holder, which is never freed before shutdown. The spawn family (`SpawnTemplate`/`SpawnGroup`) is `std::shared_ptr`, mutable on the world thread only. |
| DB I/O | Phase 1: inline on the world thread, as Java does on its pool threads. Phase 2, if measurements justify it: character list and enter-world load on the blocking pool, building an unshared `unique_ptr<Player>`. Saves stay inline unless measured too slow. |

---

## 1. Threading model

### 1.1 Why a single world thread

- **The Java server was never thread-safe.**
  - It forbids more than one NIO thread ("the game server is not thread-safe").
  - It mutates the other side's KnownList without a lock and reads live objects from the NIO thread.
  - It iterates ConcurrentHashMaps while other threads modify them.
  - All of this is memory-safe only because of the JVM. A faithful multi-pool C++ port would turn thousands of these tolerated races into UB.
- **Critic verification:**
  - Game logic has exactly 2 blocking waits (both handled below).
  - No code relies on interrupts, so `cancel(true)` equals `cancel(false)`.
  - readImpl is effectively pure: only CM_BUY_ITEM and CM_ATREIAN_PASSPORT read the active player, and only for logging.
  - So nothing prevents confining all game state to one thread.
- **Load is small.** The target is local play: a few players, tens of thousands of NPCs spawned, but only the ones in active regions (near players) move, think or regen. The Java concurrency buys throughput this target does not need.
- **Rejected: per-map executors.** Cross-map operations are pervasive:
  - teleport: 271 handler references to TeleportService, and synchronous despawn→spawn across instances
  - teams, legions, broadcastToWorld, siege/rift/world raid services, instance creation, chat, broker/exchange
  - ~1,600 handler files that call any service synchronously

  Per-map strands would turn these into message passing or locks and change the semantics of synchronous calls. That makes it an evolution path, not a starting point. Handles and anchors are exactly what a later sharding would need (section 9).
- **Rejected: Java-style pools with locks.** You would need atomics or locks on every hot field, a C++ replacement for ConcurrentHashMap's weakly consistent iteration, snapshotting for packet writes, and atomic refcounts. That is maximum effort for the least safety.

### 1.2 Threads

| Thread | Count | Runs | Touches game state? |
|---|---|---|---|
| **World thread** (`main` after startup, name "World") | 1 | Client packet runImpl; all scheduled, periodic and deferred tasks; cron callbacks; periodic managers (MoveTaskManager, MovementNotifyTask, ZoneUpdateService, TeamStatUpdater, ...); AI events; geo queries; LS/CS packets; admin and console commands; DB calls (phase 1); continuations from the blocking pool; the shutdown sequence. Java `DatabaseCleaningService`'s "must be main thread" check holds naturally. | Yes, exclusively |
| Asio IO threads | `gameserver.network.nio.threads` (the >1 ban is lifted, a documented deviation) | accept, read, AionConnection::processData (decrypt, PFF check, client packet factory, readImpl), writeData (frame, encrypt, write) | No. Reads `std::atomic` connection state only. |
| Disconnect executor (commons) | 2 | `AionConnection::onDisconnect` posts `leaveWorldDelayed` to the world thread | No |
| Blocking pool | 4 (DB pool size ≥ 6) | Phase-2 DB loads, AbstractCronTask DB phases, AbyssRankUpdateService DB phase, the custom-instance neural network training, HTML cache load, LS/CS blocking `connect` | Never game objects. Only values, and `unique_ptr`s of objects nobody else can reach. |
| Startup pool (reuses the blocking pool) | = cores | Parallel static-data parsing per file, eager BIH build for all 25,437 meshes, geo file parsing, WorldMap creation | Before the world thread starts accepting work |
| Watchdog | 1 | Watches the world thread's turn start time: warns after 5 s (Java RunnableWrapper), dumps the current task name/source_location after 60 s, then exits with `RESTART` (DeadLockDetector analogue) | No |
| Console reader | 1 (optional) | Reads stdin commands and posts them. Ctrl+C handler posts the shutdown request. | No |
| Logging (spdlog async/Discord) | existing | | No |

### 1.3 The world executor

```cpp
namespace aion::gameserver::utils {

/** The world thread's event loop. Java: no equivalent (the Java server runs game logic on ~5 pools). */
class GameExecutor {
public:
	static GameExecutor& getInstance();

	/** Any thread. Queues work for the world thread (FIFO: packets of one connection keep their order). */
	void post(std::move_only_function<void()> work, std::source_location where = std::source_location::current());

	/** World thread only. Runs the loop until stop(); called by GameServer::main after startup. */
	void run();
	void stop();

	/** World thread only, used by shutdown: keeps processing work until the predicate is true or the timeout expires. */
	bool pumpUntil(std::move_only_function<bool()> done, std::chrono::milliseconds timeout);

	[[nodiscard]] bool isWorldThread() const noexcept;

	/** Destroys the object after the current top-level turn (graveyard). Used for every game entity removed from its owner. */
	void retire(std::unique_ptr<Retirable> object);

	/** Debug introspection: current turn source location, turn duration histogram, inbox length. */
	TurnStats stats() const;
};

/** Asserts in all builds (one thread-id compare); on violation logs a stack trace and terminates. */
inline void assertWorldThread(std::source_location where = std::source_location::current());
}
```

**Loop.**

```
while (!stopped) {
	runDueTimers(now, budget = 20 ms)    // each task = one turn
	drainInbox(budget = 20 ms)           // each posted item = one turn
	after every turn: sweep graveyard (destroy retired objects, IDs to quarantine)
	once per second: IDFactory::tickQuarantine(now)
	wait on condvar until min(next timer due, inbox non-empty)
}
```

A **turn** is one top-level invocation: one packet runImpl, one task run, or one posted closure. Turns are the unit of the "no destruction under your feet" guarantee (1.6 and 2.4).

- **Time.** Scheduling uses `steady_clock`. Game logic keeps `currentTimeMillis()` like Java.
- **Fairness.** Timers and inbox alternate with budgets. Java gives no ordering guarantee between pools, so this is no loss.
- **Flooding.** A client flooding packets is limited by PFF and by the inbox budget.

### 1.4 What runs where

| Java origin | C++ |
|---|---|
| `AionConnection.processData` → `PacketProcessor.executePacket` (4 threads, serial per connection) | IO strand: decrypt, factory, state check, readImpl. Then `GameExecutor::post(ClientPacketTurn{std::move(packet)})`. The world thread re-checks `isValid()` and calls `runImpl()`. Per-connection order is preserved by the single FIFO consumer. The commons PacketProcessor is not used by the game connection. |
| `ThreadPoolManager.schedule/scheduleAtFixedRate` (scheduled pool) | `GameScheduler` on the world thread (section 4) |
| `ThreadPoolManager.execute/submit` (instant pool): MapRegion activation, cron jobs, LS/CS packets | `post()`, which runs in a later turn. Java runs these concurrently and asynchronously. Here they are asynchronous and serialized. |
| `executeLongRunning/submitLongRunning` | Blocking pool, only for jobs that touch no game state (audited: 5 src sites). Otherwise split into a DB phase plus a posted continuation. |
| Quartz CronService (1 thread handing off to pools) | `CronService` computes the next fire time in the server time zone and schedules a one-shot world task that re-arms itself. `longRunning=true` jobs run on the world thread. Their DB-heavy parts can move to the blocking pool with a continuation. |
| MoveTaskManager `parallelStream` every 200 ms | Plain loop on the world thread (8h) |
| AI events (synchronous on the caller thread) | Synchronous on the world thread |
| GeoService queries | Inline. Geo data is immutable after the eager BIH build. Per-instance door/placeable/shield state is world-thread-only. |
| DB (DAOs from services, model mutators, client packets) | Phase 1 inline. Phase 2: move the heavy loads (1.7). |
| LS/CS links: IO on NIO, packets on the instant pool (unordered) | IO on Asio. Packets posted in order (deviation: now ordered, as the C++ login server already does for GS packets). Reconnect timers are global tasks capturing `weak_ptr<Connection>`. Blocking `openSocket` runs on the blocking pool, then `registerConnection` is posted. |
| `NetFlusher` java.util.Timer | A global periodic task |
| Cleaner thread | Graveyard sweep plus ID quarantine (section 3) |
| Admin chat commands (//, ., console commands from the client) | They are client packets → world thread |
| Server console (C++ addition) | Reader thread posts the command |

### 1.5 The two blocking sites

1. **`CM_TELEPORT_ANIMATION_DONE`** runs `spawnTask.run()` then `get()` on the calling thread. The task is either a never-scheduled `FutureTask` (`TeleportService.sendLoc`) or a real scheduled future (PvPZone, PlayerReviveService) stored under `TaskId.TELEPORT`.
   - C++: `TaskHandle::runNowIfPending()` runs the callable synchronously on the world thread if it has not started, marks it done so the heap entry becomes inert, and **rethrows** the task's exception (that is Java's `get()`).
   - `GameScheduler::deferred(anchor, fn)` creates the unscheduled variant.
   - There is no cross-thread wait at all (translation in 8g).
2. **`FixPath`** blocks an instant-pool thread for up to 5 s per route step, waiting on a fixed-rate poll that watches the admin's Z.
   - On a single world thread this would freeze the world and never succeed, because the movement packets it waits for are queued behind it.
   - C++: a continuation object owned by a periodic task anchored to the admin player. It polls every 250 ms, advances to the next route step when Z settled, and aborts after 5 s. About 60 lines and no blocking.

```cpp
class PathFixJob { // replaces FixPath's execute() + getZ() blocking loop
public:
	void poll(TaskContext& ctx) {
		Player* admin = adminRef.get();
		if (!admin || cancelled) { finish(ctx); return; }
		if (waitingForZ) {
			float z = admin->getZ();
			bool settled = admin->getMoveController().getMovementMask() == MovementMask::IMMEDIATE && z != lastZ && admin->isSpawned()
				&& !admin->isInState(CreatureState::DEAD);
			if (settled) { corrections.push_back(z); waitingForZ = false; ++step; }
			else if (Clock::now() > deadline) { sendInfo(*admin, "Aborted path fixing due to timeout."); finish(ctx); }
			return;
		}
		if (step >= maxStep) { saveAndApply(*admin); finish(ctx); return; }
		TeleportService::teleportTo(*admin, positionOfStep(step)); // world thread: synchronous like Java
		lastZ = admin->getZ(); waitingForZ = true; deadline = Clock::now() + 5s;
	}
private:
	PlayerRef adminRef; std::shared_ptr<WalkerTemplate> route; size_t step = 0, maxStep; bool waitingForZ = false, cancelled = false;
	float lastZ = 0; Clock::time_point deadline; std::vector<float> corrections;
	void finish(TaskContext& ctx);
};
// FixPath::execute:
runner = admin.scheduleAtFixedRate([job = std::make_unique<PathFixJob>(...)](TaskContext& ctx) { job->poll(ctx); }, 0ms, 250ms);
```

### 1.6 The turn graveyard

The Java idiom "delete, then keep using the object" is everywhere:
- `AIActions.deleteOwner(this)` followed by more AI code
- `VisibleObjectController.deleteAndScheduleRespawn` uses `getOwner()` after `delete()`
- `PlayerLeaveWorldService` deletes the player, then saves it
- `InstanceService.destroyInstance` deletes objects, then calls `onInstanceDestroy`

So nothing a registry removes is destroyed synchronously. `World::removeObject` unpublishes the object, meaning every handle resolves to null from now on, and cancels its anchor. Then it moves the `unique_ptr` into `GameExecutor::retire`. The destructor runs after the current top-level turn.

The same applies to effects removed from an EffectController, instances, instance handlers, replaced AIs, disbanded teams and items removed from storages. Nested pumps (`pumpUntil` during shutdown) sweep only at depth 0.

The rule **"raw `T&`/`T*` are valid until the end of the turn"** therefore holds by construction.

### 1.7 DB I/O policy

- **Phase 1 (default): inline on the world thread**, exactly where Java calls DAOs.
  - Java runs them on packet and scheduled threads, so the call structure stays identical: 288 scattered call sites, including model mutators like `Item`, `PetList`, `House`.
  - Serialization guarantees like `PlayerDAO.isOnline`/`onlinePlayer(false)` stay as they are.
  - Cost: stalls. Enter world ≈ 30 queries + N+1 stone loads; storePlayer ≈ 17 DAOs with batches. Against a local MariaDB that is probably 20–200 ms (to be measured, section 10).
  - The DB pool reserves one connection for the world thread, so pool size ≥ blocking workers + 2.
- **Phase 2 (only if measured):**
  - (a) Character list, account load, and `PlayerService::getPlayer` move to the blocking pool. The loader builds an unpublished `std::unique_ptr<Player>` that nobody else can reach, then hands it to the world thread, which publishes and spawns it. No snapshot is needed because ownership is transferred.
    - Precondition: the audit of loaders that touch World (`PlayerRegisteredItemsDAO.constructObject` → `World.findVisibleObject`, `getOrLoadPlayerCommonData`) moves those bits into the continuation.
  - (b) Periodic saves of very large inventories can use row snapshots (8f). Logout saves stay inline, because the ordering with re-login matters more than 100 ms.

### 1.8 Shutdown and disconnect choreography

The commons `NioServer::shutdown()` waits for all `onDisconnect` callbacks. The game `onDisconnect` needs the world thread (`leaveWorld`). Calling `shutdown()` *on* the world thread would therefore deadlock.

The shutdown coordinator runs on its own thread:
1. It posts the countdown (a scheduler task).
2. It calls `nioServer.shutdown()`.
3. Meanwhile the world thread keeps pumping, processing the posted `leaveWorld` turns.
4. The coordinator then posts the rest of the Java sequence (PeriodicSaveService.onShutdown, saveGameTime, CronService/GameScheduler shutdown) and waits for a completion signal.
5. `main` returns the exit code.

---

## 2. Object ownership and references

### 2.0 Why explicit, checkable lifetimes beat emulating the GC

The alternative on the table was intrusive `Ref<T>` refcounting with ID release in the last destructor, a "Cleaner analogue". It reproduces Java's retention behaviour, but it imports the GC's problems without the GC:

1. **Cycles are everywhere, and Java's GC is what hides them.**
   - Npc → controller tasks → lambda → Npc
   - `AggroList.hateReductionTask` → AggroList → owner
   - Summon.master ↔ Player.summon
   - team ↔ Player.playerGroup
   - Effect effector/effected pairs across two creatures' EffectControllers
   - KnownList two-way entries when a despawn hook throws

   Refcounting leaks all of them unless you break them explicitly. But then you are writing explicit lifetime code anyway, just without a type system that tells you where.
2. **Non-deterministic lifetime means non-deterministic ID reuse.** With refcounts, an ID is released whenever the last stray reference drops: a queued packet, a cancelled-but-not-destroyed task, a forgotten handler field. ABA bugs then depend on timing. Registry ownership plus quarantine makes release a function of events plus a fixed delay.
3. **The Java code already treats deletion as the semantic end.** It checks `isDead()` 378×, `isSpawned()` 105×, `isInWorld`/`isOnline` many times, and `delete()` is idempotent. GC retention mostly buys *memory safety* for code that has already decided to do nothing. A handle whose `get()` can fail puts that decision in the type. `if (Npc* boss = bossRef.get(); boss && !boss->isDead())` is the Java guard made explicit.
4. **"Who keeps this alive?" stops being a question.** The ownership graph is a forest. Every live object sits in exactly one registry, which can be enumerated for leak reports: graveyard size, detached players, anchors with pending tasks, quarantine length. Refcounted graphs need heap-profiler archaeology.
5. **No refcount traffic in the hot loops** (knownlist broadcasts, movement, aggro scans).
6. **Where GC retention genuinely mattered, the design gives explicit mechanisms**, each one greppable:
   - tasks outliving their owner → a scope decision (4.4)
   - effects outliving their effector → handle plus `EffectorInfo` snapshot
   - offline team members → the `DetachedPlayers` registry with pins
   - "delete then use" → the graveyard

The price is honest: more null handling, and per-site scope decisions (section 8 counts them).

### 2.1 Owners

| Entity | Owner (`std::unique_ptr` unless noted) | Published in ObjectRegistry? | Destruction |
|---|---|---|---|
| Npc and subclasses (SiegeNpc, Kisk, Trap, Servant, Homing, GroupGate, SummonedHouseNpc), Summon, Pet, Gatherable, StaticObject/StaticDoor, FlyRing, Road, CuringObject | `World` (`storeObject(std::unique_ptr<VisibleObject>)`). Before storing (ClusteredNpc walkers until `organizeAndSpawn`), the spawner's local `unique_ptr`. | at `storeObject` | `removeObject` → graveyard |
| Player (in world) | `World`, moved in at enter world | at `storeObject`, generation +1 per login | leaveWorld: graveyard, or `DetachedPlayers` if in a team |
| Player (logged out, still a group/alliance member) | `DetachedPlayers`, pinned per team membership | no | when the last pin is released (timeout, relogin replacement, disband) |
| HouseObject<T> | `House::getRegistry()`. World only indexes it while spawned (`storeObject(VisibleObject&)`). | while stored | registry removal (asserts `!isInWorld()`) |
| Effect | Skill (`std::vector<std::unique_ptr<Effect>>`) until applyEffect, then the effected's `EffectController` | `EffectRef` = effected `Ref` + 64-bit serial | removal → graveyard |
| Skill (pending hitTime) | the hitTime task closure (move-only) | `SkillRef` serial, only needed for `castingSkill` | task completion or cancel |
| AbstractAI | its Creature (`std::unique_ptr<AbstractAI>`); `//ai set` retires the old one | AI serial token for anchors | with the owner / replacement |
| Controllers, KnownList, stats, EffectController, MoveController, ObserveController, AggroList, Player parts | by value or `unique_ptr` members; raw back-pointer to the owner | no | with the owner |
| WorldMapInstance (+ MapRegions, zones, InstanceHandler) | `WorldMap` | `InstanceRef` {mapId, instanceId, serial} | destroyInstance → graveyard |
| PlayerGroup, PlayerAlliance, League | `PlayerGroupService::groups` / `PlayerAllianceService` / `LeagueService` | yes (they are AionObjects with IDs) | disband → graveyard |
| Item | the Storage (Inventory, Warehouse, Equipment), Letter, BrokerItem, RepurchaseService entry, exchange escrow | no. Referenced across time by (owner, itemObjectId) re-lookup, the existing Java pattern | removal from the owner container |
| Legion, House, Town, SiegeLocation, ... | their services (already ID-keyed in Java) | no | service |
| AionConnection | `std::shared_ptr` (commons design) | no; `weak_ptr` where needed | refcount |
| Account, PlayerAccountData, PlayerCommonData | `std::shared_ptr` shared by connection, lobby and Player (non-game value data, no back-references) | no | refcount |
| Templates | DataManager holders, immortal | `const T*` | never, until shutdown |
| SpawnTemplate, SpawnGroup | `std::shared_ptr` (SpawnsData, Npc, RespawnTask, event data) | no | refcount, world-thread-only mutation |

### 2.2 Reference kinds and rules

| Kind | C++ type | When allowed | Checked by |
|---|---|---|---|
| Owning | `std::unique_ptr<T>` | only in the owners of 2.1 | the type |
| **Handle** | `Ref<T>`, `PlayerId`, `EffectRef`, `InstanceRef`, `TaskHandle` | **every** field, container element, captured variable, event or queue entry that can outlive the current turn | `get()` returns null for gone objects; generation check |
| Turn-local borrow | `T&`, `T*` | parameters, locals, return values (`instance.getNpc(id)`, `spawn(...)`) | the graveyard keeps objects alive until the turn ends; storing one is a lint error |
| Anchor-bound `this` | `[this]` in `schedule` members of an anchored owner (AI, InstanceHandler, controller, Effect, Player/Npc) | tasks whose body is about that owner | the anchor cancels the tasks when the owner is unpublished or destroyed |
| Part → whole back-pointer | `Npc& owner` in AI/controller/KnownList/AggroList/... | same lifetime by composition | construction order |
| Registry index | raw `VisibleObject*` inside World, MapRegion and WorldMapInstance maps | maintained in the same call as publish/unpublish | debug verify on iteration |
| Immortal data | `const ItemTemplate*` etc. | always | reload never frees |
| Shared value data | `std::shared_ptr<(const) T>` | SpawnTemplate family, Account/PlayerCommonData, `ConfigValue` snapshots, serialized packet bytes | no game-object members allowed in these types |
| Weak | `std::weak_ptr<AionConnection>` | tasks and LS maps referencing connections | refcount |

### 2.3 Handle API

```cpp
namespace aion::gameserver::model::gameobjects {

struct ObjectHandle {
	int32_t objectId = 0;
	uint32_t generation = 0;
	friend bool operator==(ObjectHandle, ObjectHandle) = default;
};

/**
 * Generational reference to a published AionObject. 8 bytes, trivially copyable, world thread only.
 * Never dangles: get() returns nullptr once the object was unpublished (deleted from World, logged out, disbanded).
 */
template <std::derived_from<AionObject> T>
class Ref {
public:
	Ref() noexcept = default;                                   // Java: null
	explicit Ref(const T& published) noexcept;                  // asserts the object is published
	[[nodiscard]] T* get() const noexcept;                      // O(1): paged slot array lookup + generation compare
	[[nodiscard]] T& getOrThrow(std::source_location = std::source_location::current()) const; // for invariant-backed uses; IllegalStateException
	[[nodiscard]] bool isAlive() const noexcept { return get() != nullptr; }
	[[nodiscard]] bool isNull() const noexcept { return handle.objectId == 0; }
	[[nodiscard]] int32_t getObjectId() const noexcept { return handle.objectId; }
	[[nodiscard]] bool sameInstance(const Ref& other) const noexcept { return handle == other.handle; }
	template <std::derived_from<AionObject> U> [[nodiscard]] Ref<U> cast() const noexcept; // checked with the object's kind tag at resolve
	/** Java equals()/hashCode(): objectId only, so a relogged Player equals its previous instance */
	friend bool operator==(const Ref& a, const Ref& b) noexcept { return a.handle.objectId == b.handle.objectId; }
	explicit operator bool() const = delete; // forces get()/isAlive(): "if (ref)" would read as "alive" but only mean "non-null"
private:
	ObjectHandle handle;
};
using CreatureRef = Ref<Creature>; using NpcRef = Ref<Npc>; using PlayerRef = Ref<Player>; using VisibleObjectRef = Ref<VisibleObject>;

/** Stable character identity across logins (DB objectId). Resolved by World (online) and DetachedPlayers (offline team members). */
enum class PlayerId : int32_t {};

/** Table from objectId to (object, generation). Not an owner. Java: World.allObjects plus the Cleaner bookkeeping. */
class ObjectRegistry {
public:
	static ObjectRegistry& getInstance();
	ObjectHandle publish(AionObject& object);        // ++generation of the id's slot
	void unpublish(AionObject& object) noexcept;     // slot.object = nullptr, generation kept
	[[nodiscard]] AionObject* resolve(ObjectHandle handle) const noexcept; // assertWorldThread()
private:
	struct Slot { AionObject* object = nullptr; uint32_t generation = 0; };
	std::vector<std::unique_ptr<std::array<Slot, 1 << 16>>> pages; // lazily allocated per 65,536 ids
};
}
```

`AionObject` gets `ObjectHandle handle` (set at publish), `ObjectKind kind` (for checked casts) and `TaskAnchor& taskAnchor()` for VisibleObjects and teams.

### 2.4 Rules per use case

| Use case (Java) | C++ rule |
|---|---|
| **KnownList** `Map<Integer, KnownObject>`, two-way, cleared on despawn on both sides | `StableIdMap<KnownObject>` with `KnownObject { VisibleObjectRef ref; bool visible; }`. The invariant "A knows B ⇒ B is spawned" is maintained synchronously by `World::spawn/despawn/updatePosition`. The despawn cleanup runs in a scope guard, so the exception path of `removeObject` ("did not leave world cleanly") cannot leave stale entries. Iteration resolves handles (O(1)); a null resolve logs an error and erases the entry. `forEachPlayer`/`forEachObject` pass `Player&`/`VisibleObject&` to the callback. Re-entrant removal during iteration is safe (7.3). |
| **target** `VisibleObject target` | `VisibleObjectRef target`. `getTarget()` returns `VisibleObject*` (null if unset or gone). `notSee` still clears it. |
| **AggroList** `CHM<Integer, AggroInfo>`, `AggroInfo.attacker` | `StableIdMap<AggroInfo>`, `AggroInfo { CreatureRef attacker; int damage, hate; ... }`. `isAware(nullptr)` returns false (Java already checks `creature != null`, and a despawned attacker is never known). Damage and reward lists skip attackers that no longer resolve. Java would credit a stale offline Player that fails range checks anyway, so the outcome is nearly the same; listed as a deviation. `hateReductionTask` is anchored to the owner. |
| **Effect.effector/effected** | `Effect` is owned by the effected's EffectController, so `Creature& getEffected()` is a back-pointer. The effector is `CreatureRef` plus `EffectorInfo` captured at creation: objectId, race, level, isPlayer, name, `CreatureRef actingCreature` (master for summons and SummonedObjects), tribe. `Creature* getEffector()` is nullable. `Creature* getCreditedAttacker()` returns the live effector, or else the snapshot's acting creature (preserves NpcController.java:294 "despawned summon's DoT aggro goes to the master"). Effect tasks are anchored to the Effect. |
| **Skill** effector/firstTarget/effectedList, hitTime task | The Skill is owned by its hitTime task closure. Targets are `CreatureRef`s. `applyEffect` resolves each effected (Java's `shouldApplyFurtherEffects` already skips despawned ones) and moves each Effect into its effected's controller. A deleted caster does **not** cancel projectiles in flight (same as Java). The effector is nullable in apply paths. |
| **Summon/Pet.master, SummonedObject.creator, Player.summon/pet/kisk/postman** | `PlayerRef master`, `CreatureRef creator` (Java `Npc.creatorId` is already ID-based), `Ref<Summon> summon` etc. Invariant: summon/pet/postman are released or deleted in leaveWorld before the master leaves, so `getMaster()` is `Player&` with an assert; `getMasterOrNull()` exists for the gap between release and delete. |
| **Team members incl. logged-out players** | `PlayerTeamMember { PlayerId playerId; int64_t lastOnlineTime; }`. `Player& getObject()` resolves World first, then `DetachedPlayers`. Invariant: a member always resolves, because membership pins the detached instance. Team registries own the teams; `Player::getPlayerGroup()` resolves `Ref<PlayerGroup>`. See 2.6 and 8d. |
| **Handler fields** (`Npc boss`, `List<Npc> traps`, `Future<?> task`, AtomicBoolean) | `NpcRef boss`, `std::vector<NpcRef> traps`, `TaskHandle task`, plain `bool`/`int32_t`. Java `AtomicInteger` etc. port to `WorldAtomic<T>`, which keeps `compareAndSet`/`incrementAndGet` names but is non-atomic and debug-asserts the world thread. The 511 `getNpc`/`getNpcs` lookups stay turn-local raw pointers. |
| **Tasks capturing objects** | Anchored to the owner they are about; everything else captured becomes a handle or value (section 4). |
| **Packets** | Eager serialization, so no packet outlives the send call as an object (section 5). |
| **Player ↔ AionConnection** | `Player::clientConnection` is a `std::shared_ptr<AionConnection>`, written and read on the world thread only, which removes the `isOnline()`/`getClientConnection()` TOCTOU. `AionConnection::activePlayer` is a `PlayerRef`, world thread only; IO threads read only the `std::atomic<State>`. |
| **DropNpc** (inRangePlayers, lootingPlayer, `WeakReference<TemporaryPlayerTeam>`) | `std::vector<PlayerId>`/`PlayerRef`, `Ref<TemporaryPlayerTeam>`. The one Java weak reference maps 1:1 to a handle. |
| **InstanceScaler `WeakHashMap<WorldMapInstance, Scaling>`** | `std::unordered_map<InstanceRef, Scaling>` with explicit removal in `destroyInstance` |
| **StatOwner identity** (stat functions removed by owner) | `StatOwnerKey { kind, id/serial }` instead of pointer identity |
| **Items across time** (ItemUseObserver, item-use tasks, exchange, repurchase) | `(PlayerRef/PlayerId owner, int32_t itemObjectId)` re-looked up via `getInventory().getItemByObjId`. Exchange escrow owns moved items. |
| **ID-keyed service maps** (pendingRespawns, DropRegistrationService, KiskService, Legion member IDs) | Unchanged (int keys), protected by ID quarantine (section 3) |
| **World.removeObject identity check and ~14 other pointer-identity sites** | `&a == &b` within a turn, or `Ref::sameInstance` across time, audited per site |

### 2.5 Identity and equality

- `AionObject::operator==` and `Ref::operator==` compare the objectId only. This is Java `equals`, used on ~72 lines, and it keeps relogged Players equal to their previous instance.
- `sameInstance` (objectId + generation) and pointer comparison exist for the audited identity sites: `World::removeObject` rejects a different instance with the same ID, and `AttackUtil.java:510/528` does `getTarget() == target`.
- Hashing is by objectId (`std::hash<Ref<T>>`).

### 2.6 Relogin and detached players

- Each login constructs a new Player (Java PlayerService.java:106). `publish()` increments the slot generation, so every `PlayerRef` to the previous instance resolves to null. Most of them are cancelled anyway: player-anchored tasks are cancelled at logout.
- `PlayerLeaveWorldService::leaveWorld` ends like Java. Afterwards it claims the removed instance:

  ```cpp
  auto owned = World::getInstance().claimRemoved(player); // takes it back from the graveyard
  if (player.isInGroup() || player.isInAlliance()) DetachedPlayers::getInstance().adopt(std::move(owned), PinReason::TEAM);
  else GameExecutor::getInstance().retire(std::move(owned));
  ```
- `DetachedPlayers` owns fully saved, unpublished, despawned Players with no connection, cancelled tasks and removed effects. Pins per reason: GROUP, ALLIANCE, LEAGUE. Releasing the last pin retires the object.
  - `OfflinePlayerChecker` (600 s) → unpin.
  - `PlayerConnectedEvent` replaces the member with the new instance → unpin the old one.
  - disband → unpin.
- `//gc`-style stats list detached players with their pins.
- Everything else that Java kept stale (DropNpc looters, effects cast by the player) holds handles and simply stops resolving.

---

## 3. ID lifecycle

**Two mechanisms.**
1. **Generations** make handles ABA-proof. They are needed regardless, because Players reuse their DB objectId by design.
2. **Quarantine** protects the *bare int* IDs that stay in the code: client packets that target an ID (resolved through KnownList, so already safe), service maps keyed by ID, `DecayTask(objectId)`, `RespawnTask.oldObjectId`, `RepurchaseService` matching.

```cpp
class IDFactory { // thread safe (mutex): the blocking pool may allocate item/character ids in phase 2
public:
	int32_t nextId();                               // lowest free id (Java policy), skipping the client-invisible bit pattern; never an id in quarantine
	void releaseId(int32_t id);                     // -> quarantine
	void releaseObjectIds(std::span<const int32_t>);
	void tickQuarantine(Clock::time_point now);     // world thread, 1 Hz: ids older than QUARANTINE (config, default 60 s) return to the bitset
	size_t quarantineSize() const;
};

/** Called by the destructor of auto-release objects (Java: the Cleaner action) */
inline void onAutoReleaseObjectDestroyed(int32_t objectId) {
	if (!RespawnService::setAutoReleaseId(objectId)) // pending respawn keeps the id reserved until it completes/cancels
		IDFactory::getInstance().releaseId(objectId);
}
```

| Lifecycle group | Java | C++ release point | Notes |
|---|---|---|---|
| (a) Auto-release: Npc and subclasses, Gatherable, StaticObject, Summon, HouseObject, PlayerAlliance, League, new PlayerGroups | Cleaner after GC; RespawnService may defer | Destructor, run at the graveyard sweep after the turn of `removeObject` (or for HouseObjects when the registry drops them, for teams at disband). Then RespawnService deferral, then quarantine. | Deterministic. Java's delay was "whenever GC ran", typically seconds to minutes; 60 s quarantine covers that range. |
| (b) Explicit, DB-driven: Items (`InventoryDAO` after a successful delete), house objects/decor (`PlayerRegisteredItemsDAO`), ExchangeService, ItemSplitService, PetAdoptionService, HTMLService, CM_CREATE_CHARACTER, CMT_CHARACTER_INFORMATION | `IDFactory.releaseId` immediately | Same call sites, but into quarantine | Fixes the latent RepurchaseService aliasing for 60 s (deviation, noted). The Item *object* lifetime stays independent (its owner container). |
| (c) Never released: FlyRing, Road, CuringObject, AssembledNpcPart, HTML message IDs, `CustomInstanceService.LEADERBOARD_WINDOW_OBJECT_ID` | never | never | Kept as is. The static initializer becomes a lazy accessor, since IDFactory must be initialized first. |
| Players | DB-owned | never while the character exists; generation per login | |
| Legions, Letters, Houses | DB/service-owned | as Java | |

**ABA analysis.**
- `Ref`/`EffectRef`/`TaskHandle`: exact, because generation and serial are never reused. Effect and Skill serials are 64-bit monotonic.
- Bare int IDs: safe while their holder's lifetime is under 60 s after the owner's destruction. Longer-lived int holders are already defended in Java (`pendingRespawns` checks the spawn template identity) or are converted to handles: `DecayTask` becomes `NpcRef`, `DropRegistrationService` entries are cleared in onDespawn.
- Admin audit list: every `Map<Integer, …>` of non-DB objects in services (Kisk, Rift, Invasion, Siege player maps).

---

## 4. Scheduler / Future API

### 4.1 Anchors, handles, contexts

```cpp
namespace aion::gameserver::utils {

/**
 * Lifetime link between an owner and the tasks it schedules. A member of VisibleObject, AbstractAI, InstanceHandler,
 * WorldMapInstance, Effect, Skill, teams. cancelAll() runs on unpublish/destroyInstance/endEffect;
 * the destructor asserts it is empty (and cancels).
 */
class TaskAnchor {
public:
	TaskAnchor() = default;
	TaskAnchor(const TaskAnchor&) = delete;
	~TaskAnchor();
	void cancelAll() noexcept;          // tasks not running: callable destroyed now; a running task: destroyed after it returns
	[[nodiscard]] size_t pendingTasks() const noexcept;
private:
	friend class GameScheduler;
	uint32_t head = 0;                  // intrusive list through task slots
};

/** Java: Future<?> / ScheduledFuture<?> / RunnableFuture. 16 bytes value; default-constructed == Java null. */
class TaskHandle {
public:
	TaskHandle() noexcept = default;
	bool cancel() noexcept;                                   // Java cancel(false) and cancel(true) (no interrupts in game code); false if already done
	[[nodiscard]] bool isDone() const noexcept;               // completed one-shot, cancelled, or cancelled with its anchor
	[[nodiscard]] bool isCancelled() const noexcept;
	[[nodiscard]] std::chrono::milliseconds getDelay() const noexcept; // one-shots keep their due time even after cancel (DropService)
	void runNowIfPending();                                   // Java run()+get(): synchronous, rethrows the task's exception
	[[nodiscard]] bool isNull() const noexcept { return generation == 0; }
private:
	uint32_t slot = 0, generation = 0;
	Clock::time_point due{};
};

/** Passed to callables that take it: self-cancel and handle access (Java: waitTask[0].cancel / Future<?>[] holders) */
class TaskContext {
public:
	void cancel() noexcept;
	[[nodiscard]] const TaskHandle& handle() const noexcept;
};

template <class F>
concept TaskCallable = std::invocable<F&> || std::invocable<F&, TaskContext&>;

class GameScheduler { // world thread only; ThreadPoolManager is a thin facade over it
public:
	TaskHandle schedule(TaskAnchor& anchor, TaskCallable auto&& task, std::chrono::milliseconds delay,
		std::source_location where = std::source_location::current());
	TaskHandle scheduleAtFixedRate(TaskAnchor& anchor, TaskCallable auto&& task, std::chrono::milliseconds initialDelay,
		std::chrono::milliseconds period, std::source_location where = std::source_location::current());
	/** Java: new FutureTask<>(r, null) stored as a controller task and run by runNowIfPending() */
	TaskHandle deferred(TaskAnchor& anchor, TaskCallable auto&& task, std::source_location where = std::source_location::current());

	/** Global tasks: the callable must be captureless; state is passed as bound arguments checked by SafeTaskArg. */
	template <class F, SafeTaskArg... Args> requires CapturelessInvocable<F, Args...>
	TaskHandle scheduleGlobal(BoundTask<F, Args...> task, std::chrono::milliseconds delay, std::source_location = std::source_location::current());
	template <class F, SafeTaskArg... Args> requires CapturelessInvocable<F, Args...>
	TaskHandle scheduleGlobalAtFixedRate(BoundTask<F, Args...> task, std::chrono::milliseconds initialDelay, std::chrono::milliseconds period,
		std::source_location = std::source_location::current());

	void shutdown(); // drops delayed tasks (Java setExecuteExistingDelayedTasksAfterShutdownPolicy(false)); later schedules return inert handles
};

/** bindTask([](NpcRef npc, int32_t id) { ... }, npcRef, 42) */
template <class F, class... Args> BoundTask<F, std::decay_t<Args>...> bindTask(F f, Args&&... args);

/** Rejects raw pointers/references to game entities, and std::function/lambdas as arguments; allows handles, arithmetic, enums,
    std::string, const templates, shared_ptr<const non-game>, weak_ptr<AionConnection> */
template <class T> concept SafeTaskArg = /* trait table */;
template <class F, class... Args> concept CapturelessInvocable = std::is_convertible_v<F, void (*)(Args...)> || /* TaskContext& variant */;
}
```

**Implementation.**
- Tasks live in a slot map: `{generation, state, due, period, anchor links, std::move_only_function<void(TaskContext&)>, source_location}`.
- Timing is a 4-ary min-heap of `(due, sequence, slot, generation)` with lazy deletion.
- **Cancel** marks the slot and destroys the callable immediately. Destruction goes through a local vector, because destroying a closure can cancel other tasks. Stale heap entries are skipped when popped.
- **Running:** state is RUNNING. `cancel()` or `anchor.cancelAll()` during the run sets CANCELLED. After return, a periodic task is re-queued at `due += period` (fixed-rate catch-up, never concurrent), or its callable is destroyed. This makes safe self-cancel a structural property.
- **Exceptions:** every run goes through commons `ExecuteWrapper::execute(runnable, 5000, true)`, so the exception is logged with the task's type and source location and **a periodic task continues** (Java RunnableWrapper, the opposite of the login server's ScheduledExecutor).

### 4.2 Owner convenience and the capture rule

```cpp
class AbstractAI {
protected:
	/** Anchored to this AI (dropped when the owner is unpublished or the AI replaced). The callable may capture `this`,
	    handles and values only - enforced by aion-task-captures (clang-tidy) and a regex pre-commit check on the MSVC build. */
	template <class Self> TaskHandle schedule(this Self& self, TaskCallable auto&& task, std::chrono::milliseconds delay,
		std::source_location where = std::source_location::current());
	template <class Self> TaskHandle schedule(this Self& self, void (Self::*method)(), std::chrono::milliseconds delay,
		std::source_location where = std::source_location::current());   // Java this::method (142 method references)
	// scheduleAtFixedRate likewise
};
// Same members on GeneralInstanceHandler (anchor: the handler), VisibleObjectController, Effect, Player/Npc (anchor: the object).
```

The rule is enforced at three levels:
- **Global tasks: compile-time proof** of no hidden captures (captureless lambda plus the `SafeTaskArg` concept).
- **Anchored member tasks:** `[this]` is fine by construction.
  - Additional captures must be handles or values. C++ cannot introspect lambda captures, so this is checked by a clang-tidy AST check `aion-task-captures`, run in CI with a clang-cl preset: it flags lambdas passed to `[[aion::task]]`-marked parameters that capture a pointer or reference to an `AionObject`/`AbstractAI`/`InstanceHandler`/`Effect`-derived type other than `this`.
  - A regex pre-commit rejects `[&` and `[=` in schedule calls.
- **Runtime:** ASan test builds plus debug graveyard poisoning catch what slips through.

### 4.3 Java feature to C++ mapping

| Java usage (counts from research) | C++ |
|---|---|
| `schedule(r, delay)` 728 / `scheduleAtFixedRate` 126 | `schedule(anchor, f, delay)` / `scheduleAtFixedRate(...)`, or the owner members; `scheduleGlobal*` for the ~30 src service timers |
| `cancel(false)` 51 / `cancel(true)` 154 | `cancel()` |
| `isDone()` 72 / `isCancelled()` 74 | same names |
| `getDelay(MILLISECONDS)` (DropService.java:119, after cancel) | `getDelay()`, valid after cancel for one-shots |
| `RunnableFuture.run()` + `get()` (CM_TELEPORT_ANIMATION_DONE) | `runNowIfPending()` |
| unscheduled `new FutureTask` as a controller task (TeleportService.java:191) | `deferred(anchor, f)` |
| `get(5, SECONDS)` (FixPath) | removed: continuation (1.5) |
| self-cancel from `run()`; `Future<?>[]` / `AtomicReference<Future>` holders (6) | `TaskContext::cancel()` or `handle.cancel()` inside the run; `std::array<TaskHandle, N>` |
| `CreatureController.addTask(TaskId, future)` 93 sites; `cancelAllTasks` in onDelete | `std::array<TaskHandle, TaskId::COUNT>`, same semantics (8g); the anchor makes cancel-on-delete total, not just controller tasks |
| exceptions do not stop periodic tasks (RunnableWrapper) | same |
| slow-task warning 5000 ms | same (`ThreadConfig::MAXIMUM_RUNTIME_IN_MILLISEC_WITHOUT_WARNING`) |
| `execute`/`submit` | `GameExecutor::post` |
| `executeLongRunning`/`submitLongRunning` | `BlockingPool::submit(job, anchorOrNull, continuation)` |
| AbstractPeriodicTaskManager (scheduling from a lazy singleton constructor) | `start()` called explicitly in `GameServer::main`, then a global fixed-rate task; the Rnd 500–550 ms initial delay is kept |
| Delayed `PacketSendUtility.broadcastMessage(npc, id, delay)` ~53 | anchored to the npc; the body keeps the `isSpawned()` check |

```cpp
class BlockingPool { // Java: longRunningPool, plus a DB worker role
public:
	/** job runs on a pool thread and must not touch game state (values in, values or unreachable unique_ptrs out).
	    then(result) is posted to the world thread; it is dropped if `anchor` was cancelled meanwhile (nullptr: never dropped). */
	template <class Job, class Then>
	void submit(Job job, TaskAnchor* anchor, Then then, std::source_location = std::source_location::current());
};
```

### 4.4 Scope choice: the one real semantic decision per site

A task is anchored to **the entity whose disappearance makes the task pointless**:
- body about its AI/npc: AI
- body about the instance (doors, stage spawns, instance score): instance handler
- body about a player (quest timers, item use): that player
- global: none

Java runs every task regardless and relies on guards. The difference shows only for a task anchored to X that does meaningful work after X is deleted. The research found this pattern in a small minority. Real example, `CaptainXastaAI.handleDied`:
- It schedules walks and skills for **another** npc (Ariana) and door changes 13–26 s later.
- It runs from an AI whose owner will decay.
- Anchoring those tasks to the AI would silently drop Ariana's scripted walk.
- The correct port anchors them to the instance handler and captures `NpcRef ariana`.

Mitigation for mistakes: dropped tasks log at DEBUG with source location ("task dropped: anchor cancelled while pending"), and the handler pilot (section 10) counts such sites.

### 4.5 Cron

```cpp
class CronService {
public:
	/** Quartz syntax subset actually used: seconds, ?, names, lists, ranges, increments, optional year; L/W/# rejected with an error */
	CronJob schedule(std::move_only_function<void()> job, const CronExpression& expression, bool longRunning = false,
		std::type_index tag = typeid(void), std::source_location where = std::source_location::current());
	bool cancel(const CronJob& job);
	std::vector<std::chrono::sys_seconds> findNextFireTimes(std::type_index tag) const; // SiegeService.java:323
	void shutdown();
};
class CronExpression {
public:
	static CronExpression parse(std::string_view text);                                    // PropertyTransformer<CronExpression> for the 17 config fields
	std::optional<std::chrono::sys_seconds> getTimeAfter(std::chrono::sys_seconds after) const; // in GSConfig::TIME_ZONE_ID
};
```

Each job is a world-thread one-shot re-armed after each fire, computed with `std::chrono::zoned_time` in the server zone (DST-correct). The Java `CronServiceTest` vectors port as unit tests. `AbstractCronTask`'s "run on start if missed" (`ServerVariablesDAO`) is ported as is.

---

## 5. Server packets

### 5.1 Eager serialization

```cpp
namespace aion::gameserver::network::aion {

using PacketBytes = std::shared_ptr<const std::vector<uint8_t>>; // opcode header (unencrypted form) + body

class AionServerPacket : public commons::network::packet::BaseServerPacket {
public:
	/** true for the 25 packets whose bytes depend on the recipient (con.getActivePlayer()/getAccount(), SM_KEY) */
	[[nodiscard]] virtual bool dependsOnConnection() const noexcept { return false; }
	/** World thread. con may be null only if !dependsOnConnection(). Logs the >8,192-byte client limit warning here (Java: after write). */
	[[nodiscard]] PacketBytes serialize(AionConnection* con) const;
	/** Runs on the connection strand after the bytes were handed to the cipher (only SM_KEY: enable the connection's crypt key). */
	[[nodiscard]] virtual void (*afterWrite() const noexcept)(AionConnection&) { return nullptr; }
protected:
	virtual void writeImpl(utils::ByteBuffer& buf, AionConnection* con) const = 0; // world thread: may read any game state
};

/** Serializes at most once per broadcast for connection-independent packets. */
class OnceSerialized {
public:
	explicit OnceSerialized(const AionServerPacket& packet) noexcept : packet(packet) {}
	PacketBytes forConnection(AionConnection& con);   // cached unless packet.dependsOnConnection()
private:
	const AionServerPacket& packet; PacketBytes cached;
};

class AionConnection : public commons::network::AConnection<OutgoingPacket> {
public:
	void sendPacket(const AionServerPacket& packet);       // world thread: serialize + queue
	void sendBytes(PacketBytes bytes, void (*afterWrite)(AionConnection&) = nullptr); // queue (guard mutex), requestWrite
	[[nodiscard]] Player* getActivePlayer() const noexcept; // world thread only (PlayerRef resolve)
	[[nodiscard]] State getState() const noexcept;          // atomic, any thread
protected:
	bool writeData(commons::utils::ByteBuffer& data) override; // strand: frame [len][obf opcode][0x44][~obf][body], crypt.encrypt(span), afterWrite
};
}
```

- **Why eager.**
  - Queued packets never own or reference game objects, which removes the "packet keeps object alive" requirement.
  - IO threads read only immutable bytes, so `nio.threads > 1` becomes safe.
  - Serialization happens on the thread that owns the state.
  - Java's byte-sampling moment moves from "when the NIO thread dequeues" to "when sendPacket is called". That is usually milliseconds apart and more deterministic; a documented deviation.
- **Broadcasts.** A connection-independent packet is serialized once and the `PacketBytes` are shared by all recipients' queues. Encryption copies into each connection's write buffer on its strand, so the shared bytes stay immutable.
- **Per-recipient packets.** The 25 connection-dependent packets (SM_PLAYER_INFO, SM_MESSAGE, SM_DIALOG_WINDOW, SM_PRICES, SM_LOOT_ITEMLIST, …) are serialized per recipient. `dependsOnConnection()` is required by a generated check: the packet generator and tests fail if `writeImpl` touches `con` without the flag. This prevents the "wrong race obfuscation" risk.
- **The 6 state-mutating writeImpls.** They run on the world thread now, so their mutations are race-free.
  - `SM_ATTACK`/`SM_CASTSPELL_RESULT` → `Player::setLastCounterSkill` at send time (in Java it runs per recipient at write time; idempotent timestamp)
  - `SM_PET` cooldown flags
  - `SM_PLAY_MOVIE` → recipient's custom state (per recipient, since it is connection-dependent)
  - `SM_GROUP_MEMBER_INFO`/`SM_ALLIANCE_MEMBER_INFO` reassign their own `event` field → a local variable in `writeImpl`
  - `SM_KEY` stays special: `afterWrite` enables the crypt on the strand, keeping "SM_KEY is written unencrypted and turns encryption on at write time".
- **Debug echo.** The Java echo of packet names to membership-10 accounts during `writeData` moves into `sendPacket`.
- **Queue and close semantics:** commons `AConnection` (guard mutex, batching, close packet).
- **Client packets.** readImpl on the strand. The two readImpls that read the active player for audit logging move that logging to runImpl (deviation).

### 5.2 Broadcast API (unchanged call shape)

```cpp
namespace PacketSendUtility {
	void sendPacket(Player& player, const AionServerPacket& packet);                 // if (auto con = player.getClientConnection()) con->sendPacket(packet)
	void broadcastPacket(const VisibleObject& object, const AionServerPacket& packet);
	void broadcastPacket(const VisibleObject& object, const AionServerPacket& packet, std::predicate<Player&> auto filter);
	void broadcastPacketAndReceive(VisibleObject& object, const AionServerPacket& packet);
	void broadcastToMap(WorldMapInstance& instance, const AionServerPacket& packet);
	void broadcastToMap(WorldMapInstance& instance, std::unique_ptr<const AionServerPacket> packet, std::chrono::milliseconds delay); // anchored to instance
	void broadcastMessage(Npc& npc, int32_t msgId, std::chrono::milliseconds delay, auto&&... params);  // anchored to npc; params formatted now like Java toString
}
```

A delayed broadcast owns its packet object (`unique_ptr`) and serializes at run time, which matches Java's delayed send. Its handles resolve then.

---

## 6. Static data ownership at runtime

- **Immutable templates.** All loaded holders (items, npcs, skills, zones, world maps, recipes, …) are built by the generated loaders during startup (parallel per file on the startup pool). They are finalized by the ordered afterUnmarshal/validate phase and published as `const`. Live objects hold `const NpcTemplate*`, `const SkillTemplate*`, `const EffectTemplate*`; effect templates' member functions are `const`. Capturing a template's `this` in tasks is allowed (immortal).
- **Reloadable holders** (//reload: QUEST_DATA, SKILL_DATA, ITEM_DATA, CUSTOM_NPC_DROP, UPGRADE_ARCADE_DATA, DECOMPOSABLE_ITEMS_DATA, plus the in-place setters of XML_QUESTS, NPC_SKILL_DATA, EVENT_DATA rebuilt as new holders):

```cpp
template <class T>
class ReloadableHolder {
public:
	const T* operator->() const noexcept { return current.load(std::memory_order_acquire); } // any thread (the blocking pool reads ITEM_DATA while loading players)
	const T& get() const noexcept { return *operator->(); }
	/** World thread. The previous holder is retired and kept until shutdown: every const Template* handed out stays valid (Java: GC). */
	void replace(std::unique_ptr<const T> fresh);
private:
	std::atomic<const T*> current{nullptr};
	std::vector<std::unique_ptr<const T>> retired;
};
struct DataManager {
	static inline ReloadableHolder<ItemData> ITEM_DATA;      // DataManager::ITEM_DATA->getItemTemplate(id)
	static inline std::unique_ptr<const NpcData> NPC_DATA;   // not reloadable
	// ... 91 fields
};
```

  Memory cost of a reload is the size of the old holder, and it is not reclaimed (item data is probably ~100–200 MB). //reload is a rare admin action; documented. QuestEngine's 27 event maps and handler registries are rebuilt on the world thread, so they need no locks.
- **Spawn family.** `SpawnTemplate`, `SpawnGroup` and the Rift/Siege/Vortex/Base/Town/AhserionsFlight variants are `std::shared_ptr`. They are held by SpawnsData, VisibleObject (`getSpawn()`), RespawnTask and event data. They are mutable, **world-thread only**.
  - They hold no game-object references (enforced by review of the ~15 runtime-constructed template classes).
  - The 47 handler setter calls on shared static templates (e.g. `setWalkerId`) keep Java's cross-instance quirk; single-threaded, it is no longer a race. A copy-on-write per instance can be added later as a fix.
  - Event start/stop removing SpawnGroups no longer frees templates still referenced by live NPCs (shared_ptr).
  - `SpawnGroup.poolUsedTemplates` gains removal on `destroyInstance` (a Java leak fix, documented).
  - `SpawnsData::saveSpawn` and `WalkerData::saveData` (admin write-back) run on the world thread.
- **Other runtime-mutable template fields.**
  - `GuideTemplate.activated` (events): world-thread writes and reads, plain `bool` in a non-const holder section.
  - `HostileUpEffect.tempHate`: per-cast state on a shared template. Preserved as a `mutable` field (race-free now). The cleaner fix is moving it into `Effect`, documented.
  - About 29 runtime `*Template` constructions (FlyRingTemplate, QueuedNpcSkillTemplate, PlayerStatsTemplate, WalkerTemplate in FixPath, …): the generator emits constructible, settable classes for these, owned by their user (`unique_ptr`/`shared_ptr`).
- **Geo.** An immutable arena owned by GeoService for the whole process. The BIH is built eagerly in parallel at startup, which fixes Java's lazy-build race. Per-instance DespawnableNode, door and shield state is world-thread-only. Zone handlers hold `const Spatial*` into the arena.
- **Config.**
  - Runtime rebinding (Event start/stop, //reload config, //configure, //ai) happens only on the world thread. So config fields read *only* by game logic can be plain fields in the game server; this is a deviation from CONVENTIONS' ConfigValue rule with a checked domain split.
  - Fields read by IO, blocking-pool or logging threads (NetworkConfig, PffConfig, FloodConfig, ThreadConfig, DB-loader-relevant fields) stay `ConfigValue<T>`/`std::atomic<T>`.
  - Rebinding asserts the world thread.
  - //configure uses the bind lists extended with field names.

---

## 7. Concurrency safety guarantees

### 7.1 Impossible by construction

| Hazard | Why it cannot happen |
|---|---|
| Data races on game objects, world, services, templates being mutated, spawn data, QuestEngine tables | Thread confinement: only the world thread can reach them. `ObjectRegistry::resolve`, `World`, `GameScheduler` and service singletons call `assertWorldThread()`, enabled in all builds (one thread-id compare, `[[unlikely]]` path logs a stacktrace and terminates). IO and pool threads receive values or unreachable `unique_ptr`s only. |
| Use-after-free across time (tasks, fields, effects, teams, packets) | Only handles cross turns. Anchored tasks are cancelled with their owner. Packets are bytes. |
| Use-after-free within a turn ("delete then use", despawn during iteration) | Graveyard: destruction happens only between top-level turns |
| A task destroyed while running (self-cancel, owner deleted inside its own task) | The scheduler owns the callable and destroys it only after it returns |
| ABA on handles | Generation per objectId slot (+1 per publish); 64-bit serials for effects, skills, AIs, tasks |
| ABA on bare int IDs | 60 s quarantine plus RespawnService deferral |
| Packet writes reading mutable state from IO threads | Eager serialization; IO sees immutable `shared_ptr<const bytes>` |
| Template freed under a pointer | Templates are never freed before shutdown |
| Iterator invalidation from re-entrant mutation | `StableIdMap` (7.3) for every Java map iterated with CHM semantics; other containers follow Java's explicit copies |
| Dangling `this` in handler lambdas | Anchored `[this]` is safe; any other capture of object pointers is flagged by clang-tidy, and global tasks are compile-time captureless |

### 7.2 What needs locks or atomics (small, commons-level)

- `AConnection` send queue and close state: the existing recursive `guard`. The crypt is strand-only. `AionConnection::state` is `std::atomic`.
- `GameExecutor` inbox: mutex + condvar, or an MPSC lock-free queue.
- `IDFactory`: mutex, because the blocking pool may allocate in phase 2.
- `ReloadableHolder` pointers: atomic. `ConfigValue` for cross-thread config fields.
- DB connection pool, logging, RunnableStatsManager: existing.
- The watchdog reads an atomic turn-start timestamp and a pointer to a static `source_location`.

Nothing else. About 250 `synchronized` blocks, ~400 `Atomic*` and ~250 concurrent collections in src and handlers port to plain code: `synchronized(x)` is removed, `AtomicX` becomes `WorldAtomic<X>`, `ConcurrentHashMap` becomes `StableIdMap`/`std::unordered_map`. EffectController's StampedLock disappears.

### 7.3 StableIdMap: ConcurrentHashMap's weakly consistent iteration, single-threaded

```cpp
/** Insertion-ordered id -> V map whose forEach tolerates put/remove from inside the callback (Java: CHM iteration while others modify).
    Entries removed during iteration are skipped; entries added are not visited; storage is compacted when the outermost iteration ends. */
template <class V>
class StableIdMap {
public:
	bool putIfAbsent(int32_t id, V value);
	V* get(int32_t id) noexcept;
	bool remove(int32_t id) noexcept;                 // tombstones while iterating
	[[nodiscard]] size_t size() const noexcept;
	void forEach(std::invocable<int32_t, V&> auto&& fn); // re-entrant (depth counter)
private:
	std::vector<std::pair<int32_t, std::optional<V>>> dense; std::unordered_map<int32_t, uint32_t> index; uint32_t iterating = 0, tombstones = 0;
};
```

Used by: KnownList, MapRegion objects, WorldMapInstance objects/npcs/players, AggroList, EffectController maps, team members, MoveTaskManager, FIFO managers, groups/alliances maps, DropRegistrationService.

### 7.4 Other UB guards

- Java→C++ arithmetic rules from CONVENTIONS (signed overflow, shifts, float→int saturation, `Math.round`) are unchanged.
- No exception escapes a turn: ExecuteWrapper at the task, packet and post boundaries.
- `noexcept` despawn cleanup paths.
- ASan and debug iterator builds in CI for the world/scheduler tests and the handler pilot.
- CRTP/static_cast owner accessors are checked with `assert(dynamic_cast)` in debug.

---

## 8. Porting mechanics, side by side

### (a) AI handler: delayed action capturing `this`, checking `isDead()` later

Java, `ai/instance/abyssal_splinter/YamenessPortalSummonedAI.java`:

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
void YamenessPortalSummonedAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	schedule(&YamenessPortalSummonedAI::spawnSummons, 12000ms); // anchored to this AI: cancelled if the npc is deleted or its AI replaced
}
void YamenessPortalSummonedAI::spawnSummons() {
	if (isDead() || !getOwner().isSpawned()) // kept: a corpse or a despawned-but-stored npc still runs its tasks, exactly like Java
		return;
	spawn(281903, getOwner().getX() + 3, getOwner().getY() - 3, getOwner().getZ(), 0);
	spawn(281904, getOwner().getX() - 3, getOwner().getY() + 3, getOwner().getZ(), 0);
	schedule([this] {
		if (!isDead() && getOwner().isSpawned()) {
			spawn(281903, getOwner().getX() + 3, getOwner().getY() - 3, getOwner().getZ(), 0);
			spawn(281904, getOwner().getX() - 3, getOwner().getY() + 3, getOwner().getZ(), 0);
		}
	}, 60000ms);
}
AI_HANDLER("yamenessportal", YamenessPortalSummonedAI); // marker collected into the generated registration table
```

The same file shape with a `Future` field, `KaluvaSpawnAI`:

```cpp
TaskHandle task;                                                 // Java: private Future<?> task;
void handleDied() override {
	NpcAI::handleDied();
	if (!task.isNull() && !task.isDone()) task.cancel();        // Java: task.cancel(true)
	checkKaluva();
}
void checkKaluva() {
	Npc* kaluva = getPosition().getWorldMapInstance().getNpc(216950); // turn-local borrow
	if (kaluva && !kaluva->isDead()) kaluva->getEffectController().removeEffect(19152);
	AIActions::deleteOwner(*this);                               // owner -> graveyard; `this` valid until the turn ends
}
void scheduleHatch() { task = schedule([this] { if (!isDead()) { hatchAdds(); checkKaluva(); } }, 22000ms); }
```

**Per site:** `ThreadPoolManager.getInstance().schedule(` → `schedule(`; `this::m` → `&Class::m`; `() ->` → `[this]`; `Future<?>` → `TaskHandle`; `cancel(true)` → `cancel()`.

### (b) Instance handler keeping Npcs and spawning adds

Verified finding: **instance** handlers keep no single `Npc boss` fields; they keep Npc collections (`LowerUdasTempleInstance.traps`, `EmpyreanCrucibleInstance` stage lists) and capture spawned Npcs in lambdas. AI handlers keep the `Npc boss` fields (`SacrificialSoulAI`, `EnragedNightmareAI`, `KuharaBombAI`). The examples below show all three shapes.

Java, `instance/AturamSkyFortressInstance.java`:

```java
case 217656:
	int killed2 = chiefKilled.incrementAndGet();
	if (killed2 == 1) startOfficerWalkerEvent();
	else if (killed2 == 2) {
		instance.setDoorState(178, true);
		instance.setDoorState(308, false); // reopen side windows
		ThreadPoolManager.getInstance().schedule(() -> instance.setDoorState(307, true), 10000); // close side windows
	}
	npc.getController().delete();
	break;
case 701029:
	Npc boss = instance.getNpc(217371);
	...
private void startWalk(final Npc npc, final String walkId) {
	ThreadPoolManager.getInstance().schedule(() -> {
		if (!isInstanceDestroyed) {
			npc.getSpawn().setWalkerId(walkId);
			WalkManager.startWalking((NpcAI) npc.getAi());
			npc.setState(CreatureState.ACTIVE, true);
			PacketSendUtility.broadcastPacket(npc, new SM_EMOTION(npc, EmotionType.CHANGE_SPEED, 0, npc.getObjectId()));
		}
	}, 2000);
}
private void startMarbataWalkerEvent() {
	sendMsg(SM_SYSTEM_MESSAGE.STR_MSG_IDStation_3FDoor_311());
	startWalk((Npc) spawn(218577, 193.45583f, 802.1455f, 900.7575f, (byte) 103), "3002400009");
	...
```

C++:

```cpp
case 217656: {
	int32_t killed2 = chiefKilled.incrementAndGet();           // WorldAtomic<int32_t>
	if (killed2 == 1) startOfficerWalkerEvent();
	else if (killed2 == 2) {
		instance.setDoorState(178, true);
		instance.setDoorState(308, false);
		schedule([this] { instance.setDoorState(307, true); }, 10000ms); // anchored to the instance handler
	}
	npc.getController().delete_();                            // Java delete() (C++ keyword)
	break;
}
case 701029: {
	Npc* boss = instance.getNpc(217371);                       // turn-local, never stored
	...
}
void AturamSkyFortressInstance::startWalk(Npc& npc, std::string walkId) {
	schedule([this, npcRef = NpcRef(npc), walkId = std::move(walkId)] {
		Npc* npc = npcRef.get();
		if (isInstanceDestroyed || !npc) // new: Java would start walking a deleted npc and log "despawned objects cannot move"
			return;
		npc->getSpawn().setWalkerId(walkId);                     // runtime SpawnTemplate: shared_ptr, world-thread mutable
		WalkManager::startWalking(npc->getAi<NpcAI>());
		npc->setState(CreatureState::ACTIVE, true);
		PacketSendUtility::broadcastPacket(*npc, SM_EMOTION(*npc, EmotionType::CHANGE_SPEED, 0, npc->getObjectId()));
	}, 2000ms);
}
void AturamSkyFortressInstance::startMarbataWalkerEvent() {
	sendMsg(SM_SYSTEM_MESSAGE::STR_MSG_IDStation_3FDoor_311());
	startWalk(*spawnNpc(218577, 193.45583f, 802.1455f, 900.7575f, 103), "3002400009"); // spawnNpc: Java (Npc) spawn(...)
}
```

`LowerUdasTempleInstance` (collection field):

```cpp
std::vector<NpcRef> traps;                                     // Java: List<Npc> traps
WorldAtomic<bool> wasSpawned;                                  // Java: AtomicBoolean
void onEnterInstance(Player&) override {
	if (wasSpawned.compareAndSet(false, true)) {
		traps.push_back(NpcRef(*spawnNpc(216531, 744.7521f, 885.8238f, 152.7852f, 30)));
		for (const WorldPosition& p : trap_positions) traps.push_back(NpcRef(*spawnNpc(216530, p.getX(), p.getY(), p.getZ(), 0)));
	}
}
void handleUseItemFinish(Player&, Npc&) override {
	for (NpcRef ref : traps)
		if (Npc* trap = ref.get(); trap && trap->getNpcId() != 216531) // Java: trap != null (never null there; stale traps were deleted again, a no-op)
			trap->getController().delete_();
}
```

AI `Npc boss` field (`SacrificialSoulAI`): `private Npc boss;` → `NpcRef boss;`

```cpp
void handleMoveArrived() override {
	if (Npc* b = boss.get(); b && !b->isDead()) { // Java: boss != null && !boss.isDead() (a deleted-but-not-dead boss would still be targeted in Java)
		SkillEngine::getInstance().getSkill(getOwner(), 18960, 55, *b).useNoAnimationSkill();
		AIActions::deleteOwner(*this);
	}
}
```

`EmpyreanCrucibleInstance.EmpyreanStage.containsNpcs()` compares stored Npcs with `instance.getNpcs()` via `equals`. With `std::vector<NpcRef>` it becomes `std::ranges::any_of(stageNpcs, &NpcRef::isAlive)` restricted to this instance. It is exact even under ID reuse, which Java only achieves through the Cleaner.

### (c) Effect whose effector is despawned mid-DoT

Java:

```java
// AbstractOverTimeEffect.startEffect
Future<?> task = ThreadPoolManager.getInstance().scheduleAtFixedRate(() -> onPeriodicAction(effect), initialDelay, checktime);
effect.setPeriodicTask(task, position);
// PoisonEffect
public void onPeriodicAction(Effect effect) {
	Creature effected = effect.getEffected();
	effected.getController().onAttack(effect, TYPE.DAMAGE, effect.getReserveds(position).getValue(), false, LOG.POISON, hopType,
		effect.isMagicalCritical(position));
	effected.getObserveController().notifyDotAttackedObservers(effect.getEffector(), effect);
}
// CreatureController: onAttack(effect, ...) -> onAttack(effect.getEffector(), effect, ...)
// NpcController.onAttack
// summon should gain its own aggro (except if despawned, for example because of a damage over time effect)
if (attacker instanceof Summon && attacker.isSpawned()) actingCreature = attacker;
else actingCreature = attacker.getActingCreature();
```

C++:

```cpp
void AbstractOverTimeEffect::startEffect(Effect& effect, std::optional<AbnormalState> abnormal) const {
	...
	auto initialDelay = 300ms + checktime;
	TaskHandle task = effect.scheduleAtFixedRate([this](Effect& e) { onPeriodicAction(e); }, initialDelay, checktime); // anchor: the Effect; this: immortal template
	effect.setPeriodicTask(task, position);
}

void PoisonEffect::onPeriodicAction(Effect& effect) const {
	Creature& effected = effect.getEffected();                  // back-pointer: the Effect lives in effected's EffectController
	effected.getController().onAttack(effect, TYPE::DAMAGE, effect.getReserveds(position).getValue(), false, LOG::POISON, hopType,
		effect.isMagicalCritical(position));
	effected.getObserveController().notifyDotAttackedObservers(effect.getEffector() /* nullable */, effect);
}

void CreatureController::onAttack(Effect& effect, TYPE type, int32_t damage, bool notifyAttack, LOG logId, HopType hopType, bool criticalHit) {
	onAttack(effect.getEffector(), &effect, type, damage, notifyAttack, logId, effect.getAttackStatus(), hopType, nullptr, criticalHit);
}

void NpcController::onAttack(Creature* attacker, Effect* effect, TYPE type, int32_t damage, bool notifyAttack, LOG logId, AttackStatus status, HopType hopType) {
	if (getOwner().isDead()) return;
	// summon should gain its own aggro (except if despawned, for example because of a damage over time effect)
	Creature* actingCreature;
	if (attacker && attacker->is<Summon>() && attacker->isSpawned()) actingCreature = attacker;
	else if (attacker) actingCreature = &attacker->getActingCreature();
	else actingCreature = effect ? effect->getEffectorInfo().actingCreature.get() : nullptr; // deleted summon -> its master, as Java
	CreatureController::onAttack(actingCreature, effect, type, damage, notifyAttack, logId, status, hopType); // nullptr attacker: no hate (Java isAware fails too)
	...
}
```

- **Effect lifetime.** `EffectController::removeEffect` retires the Effect: `anchor.cancelAll()` in `endEffect`, then the graveyard.
- **Effected deleted:** `NpcController::onDespawn` → `removeAllEffects` (Java) cancels everything.
- **Effector deleted:** nothing is cancelled, same as Java, and the DoT keeps ticking.
- **Effect on a logging-out player** is removed in leaveWorld.
- **Known deviation.** When a DoT kills with a gone, master-less effector, `lastAttacker` falls back to the owner, and `SM_EMOTION DIE` shows killer id 0 instead of a deleted npc's id.
- **Porting cost:** 202 `getEffector()` uses in 101 files. In apply/calculate paths reached synchronously from a cast, the effector is alive (`Creature& effectorAlive()` asserts). In the 17 periodic/act classes (6 read the effector) and end paths, they need null handling.

### (d) A group keeping a logged-out player

Java:

```java
public class PlayerTeamMember implements TeamMember<Player> {
	final Player player; private long lastOnlineTime;
	public Player getObject() { return player; }
	public boolean isOnline() { return player.isOnline(); }
	public byte getLevel() { return player.getLevel(); } ...
}
// PlayerGroupService.OfflinePlayerChecker (every 30 s)
for (PlayerGroup group : groups.values())
	group.forEachTeamMember(member -> {
		if (!member.isOnline() && TimeUtil.isExpired(member.getLastOnlineTime() + GroupConfig.GROUP_REMOVE_TIME * 1000))
			group.onEvent(new PlayerGroupLeavedEvent(group, member.getObject(), LeaveReson.LEAVE_TIMEOUT));
	});
// PlayerConnectedEvent.handleEvent
group.removeMember(player.getObjectId());
group.addMember(new PlayerGroupMember(player));
```

C++:

```cpp
class PlayerTeamMember : public TeamMember<Player> {
public:
	explicit PlayerTeamMember(Player& player) : playerId(PlayerId{player.getObjectId()}) {}
	/** Online instance from World, else the detached instance pinned by this membership. Never null while the member exists. */
	Player& getObject() const { return TeamPlayers::resolve(playerId); }
	int32_t getObjectId() const noexcept { return static_cast<int32_t>(playerId); }
	bool isOnline() const { return getObject().isOnline(); }
	int8_t getLevel() const { return getObject().getLevel(); }
	int64_t getLastOnlineTime() const noexcept { return lastOnlineTime; }
private:
	PlayerId playerId; int64_t lastOnlineTime = 0;
};

// PlayerGroupService::start(): the periodic checker, global
ThreadPoolManager::getInstance().scheduleGlobalAtFixedRate(bindTask([] {
	for (auto& [id, group] : groups) // StableIdMap: a leave event removing the group during iteration is deferred
		group->forEachTeamMember([&](PlayerGroupMember& member) {
			if (!member.isOnline() && TimeUtil::isExpired(member.getLastOnlineTime() + GroupConfig::GROUP_REMOVE_TIME * 1000))
				group->onEvent(PlayerGroupLeavedEvent(*group, member.getObject(), LeaveReson::LEAVE_TIMEOUT)); // unpins the detached player
		});
}), 1000ms, 30000ms);

void PlayerConnectedEvent::handleEvent() {
	group.removeMember(player.getObjectId());      // removes the member holding the old PlayerId pin -> detached instance retired at end of turn
	group.addMember(PlayerGroupMember(player));    // same PlayerId, now resolves to the online instance
	if (player == group.getLeader().getObject()) { ... } // Java equals: objectId
	...
}

// PlayerLeaveWorldService::leaveWorld (tail)
PlayerDAO::onlinePlayer(player, false);
con->setActivePlayer(nullptr);
if (auto owned = World::getInstance().claimRemoved(player); player.isInGroup() || player.isInAlliance())
	DetachedPlayers::getInstance().adopt(std::move(owned));  // pins added by PlayerGroupService/PlayerAllianceService::onPlayerLogout
else
	GameExecutor::getInstance().retire(std::move(owned));
```

`SM_GROUP_MEMBER_INFO` serializes on the world thread from `member.getObject()` (name, class, level, map, position, fly state), which is valid for the detached instance. Its `event` field becomes a local variable.

### (e) PacketSendUtility.broadcastPacket of an SM_ packet

Java (`EnragedNightmareAI`):

```java
PacketSendUtility.broadcastPacket(getOwner(), new SM_EMOTION(getOwner(), EmotionType.CHANGE_SPEED, 0, getOwner().getObjectId()));
// PacketSendUtility
public static void broadcastPacket(VisibleObject object, AionServerPacket packet) {
	object.getKnownList().forEachPlayer(player -> sendPacket(player, packet));
}
public static void sendPacket(Player player, AionServerPacket packet) {
	if (player.isOnline()) player.getClientConnection().sendPacket(packet); // queued object, written lazily on the NIO thread
}
```

C++:

```cpp
PacketSendUtility::broadcastPacket(getOwner(), SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getOwner().getObjectId()));

void PacketSendUtility::broadcastPacket(const VisibleObject& object, const AionServerPacket& packet) {
	OnceSerialized bytes(packet);                              // one writeImpl for all recipients unless dependsOnConnection()
	object.getKnownList().forEachPlayer([&](Player& player) {
		if (AionConnection* con = player.getClientConnection())  // single read on the world thread: no TOCTOU
			con->sendBytes(bytes.forConnection(*con));             // shared immutable bytes; encrypted per connection on its strand
	});
}
```

For `SM_PLAYER_INFO`, `SM_MESSAGE` and the other connection-dependent packets, `forConnection` serializes per recipient. `SM_NPC_INFO(npc, player)` already takes the viewer in its constructor and is sent per player.

### (f) DAO save of a player: periodic save and logout

Java (`PlayerEnterWorldService`):

```java
player.getController().addTask(TaskId.PLAYER_UPDATE, ThreadPoolManager.getInstance().scheduleAtFixedRate(
	new GeneralUpdateTask(player.getObjectId()), PeriodicSaveConfig.PLAYER_GENERAL * 1000, PeriodicSaveConfig.PLAYER_GENERAL * 1000));
class GeneralUpdateTask implements Runnable {
	public void run() {
		Player player = World.getInstance().getPlayer(playerId);
		if (player != null) {
			try {
				AbyssRankDAO.storeAbyssRank(player); PlayerSkillListDAO.storeSkills(player); PlayerQuestListDAO.store(player);
				PlayerDAO.storePlayer(player);
				for (House house : player.getHouses()) house.save();
			} catch (Exception ex) { log.error("Exception during periodic saving of player " + player.getName(), ex); }
		}
	}
}
// PlayerLeaveWorldService.leaveWorld: ... player.getController().delete(); ... PlayerService.storePlayer(player); ... PlayerDAO.onlinePlayer(player, false);
```

C++ (phase 1: same structure, inline on the world thread; the race Java had on pool threads is gone):

```cpp
player.getController().addTask(TaskId::PLAYER_UPDATE, ThreadPoolManager::getInstance().scheduleGlobalAtFixedRate(
	bindTask(&GeneralUpdateTask::run, player.getObjectId()),
	std::chrono::seconds(PeriodicSaveConfig::PLAYER_GENERAL), std::chrono::seconds(PeriodicSaveConfig::PLAYER_GENERAL)));

struct GeneralUpdateTask {
	static void run(int32_t playerId) {
		Player* player = World::getInstance().getPlayer(playerId); // already ID-based in Java
		if (!player) return;
		try {
			AbyssRankDAO::storeAbyssRank(*player); PlayerSkillListDAO::storeSkills(*player); PlayerQuestListDAO::store(*player);
			PlayerDAO::storePlayer(*player);
			for (House* house : player->getHouses()) house->save();
		} catch (...) { log().errorCurrentException("Exception during periodic saving of player " + player->getName()); }
	}
};
// leaveWorld: identical order; storePlayer(player) runs after delete_() because the player is in the graveyard until the turn ends (2.6)
```

Phase 2 (only if measured). The load moves off-thread with ownership transfer; big inventory saves use a row snapshot:

```cpp
// CM_ENTER_WORLD -> PlayerEnterWorldService::enterWorld
BlockingPool::getInstance().submit(
	[accountId, objectId] { return PlayerService::loadPlayer(objectId, accountId); },       // DB thread: builds an unpublished unique_ptr<Player>
	&con->taskAnchor(),                                                                        // dropped if the connection went away
	[conRef = std::weak_ptr(con)](std::unique_ptr<Player> player) {
		if (auto con = conRef.lock()) PlayerEnterWorldService::enterWorldLoaded(*con, std::move(player)); // world thread: publish, spawn, packets
	});

// ItemUpdateTask, snapshot variant
InventoryRows rows = InventoryDAO::snapshotDirty(*player);                                   // world thread: copies rows + per-item version
BlockingPool::getInstance().submit([rows = std::move(rows)] { return InventoryDAO::storeRows(rows); }, &player->taskAnchor(),
	[playerId](InventoryStoreResult result) {
		if (Player* p = World::getInstance().getPlayer(playerId)) InventoryDAO::applyStoreResult(*p, result); // marks UPDATED if version unchanged, releases deleted ids
	});
```

### (g) CreatureController.addTask(TaskId, schedule(...)) and cancel on despawn

Java:

```java
public void addTask(TaskId taskId, Future<?> task) {
	tasks.compute(taskId.ordinal(), (k, oldTask) -> { if (oldTask != null) { oldTask.cancel(false); if (taskId == TaskId.DESPAWN) log.warn(...); } return task; });
}
public void cancelAllTasks() { for (...) task.cancel(false); tasks.clear(); }
public void onDelete() { cancelAllTasks(); super.onDelete(); }
// PlayerLeaveWorldService.leaveWorldDelayed
Future<?> leaveWorldTask = ThreadPoolManager.getInstance().schedule(() -> leaveWorld(player), delayInMillis);
player.getController().addTask(TaskId.DESPAWN, leaveWorldTask);
// DropService
ScheduledFuture<?> decayTask = (ScheduledFuture<?>) npc.getController().cancelTask(TaskId.DECAY);
if (decayTask != null) dropNpc.setRemaingDecayTime(decayTask.getDelay(TimeUnit.MILLISECONDS));
```

C++:

```cpp
class CreatureController : public VisibleObjectController {
public:
	bool hasTask(TaskId id) const noexcept { return !tasks[index(id)].isNull(); }
	bool hasScheduledTask(TaskId id) const noexcept { const TaskHandle& t = tasks[index(id)]; return !t.isNull() && !t.isDone(); }
	TaskHandle getAndRemoveTask(TaskId id) noexcept { return std::exchange(tasks[index(id)], {}); }
	TaskHandle cancelTask(TaskId id) noexcept { TaskHandle t = getAndRemoveTask(id); t.cancel(); return t; }
	bool cancelTaskIfPresent(TaskId id, const TaskHandle& task) noexcept;
	void addTask(TaskId id, TaskHandle task) {
		TaskHandle old = std::exchange(tasks[index(id)], task);
		if (!old.isNull()) {
			old.cancel();
			if (id == TaskId::DESPAWN) log().warn("Despawn task for {} was cancelled and replaced with another one, possibly delaying the intended despawn time.", getOwner());
		}
	}
	void cancelAllTasks() noexcept { for (TaskHandle& t : tasks) std::exchange(t, {}).cancel(); }
	void onDelete() override { cancelAllTasks(); VisibleObjectController::onDelete(); } // World::removeObject then cancels the whole anchor
private:
	std::array<TaskHandle, magic_enum::enum_count<TaskId>()> tasks;
};

void PlayerLeaveWorldService::leaveWorldDelayed(Player& player, std::chrono::milliseconds delay) {
	TaskHandle leaveWorldTask = player.schedule([&player = player] { leaveWorld(player); }, delay); // anchor: the player itself -> &player is the anchor owner
	player.getController().addTask(TaskId::DESPAWN, leaveWorldTask);
}

// DropService
if (Npc* npc = object_cast<Npc>(World::getInstance().findVisibleObject(npcObjectId))) {
	TaskHandle decayTask = npc->getController().cancelTask(TaskId::DECAY);
	if (!decayTask.isNull()) dropNpc.setRemaingDecayTime(decayTask.getDelay().count()); // one-shot keeps its due time after cancel
}

// CM_TELEPORT_ANIMATION_DONE::runImpl
Player& player = *getConnection()->getActivePlayer();
TaskHandle task = player.getController().getAndRemoveTask(TaskId::TELEPORT);
if (!task.isNull() && !task.isDone()) {
	try {
		task.runNowIfPending();                                    // deferred (TeleportService) or scheduled (PvPZone, PlayerReviveService)
	} catch (...) {
		log().errorCurrentException("");
		if (!player.isSpawned()) { PacketSendUtility::sendPacket(player, SM_PLAYER_INFO(player)); World::getInstance().spawn(player); }
	}
}
// TeleportService::sendLoc
player.getController().addTask(TaskId::TELEPORT, scheduler().deferred(player.taskAnchor(), [spawnTask = SpawnTask(PlayerRef(player), ...)]() mutable { spawnTask.run(); }));
```

### (h) MoveTaskManager periodic NPC movement

Java:

```java
private final Map<Integer, Creature> movingCreatures = new ConcurrentHashMap<>();
public void run() {
	movingCreatures.values().parallelStream().forEach(creature -> {
		if (!creature.isSpawned()) { if (removeCreature(creature)) log.warn(...); return; }
		creature.getMoveController().moveToDestination();
		if (creature.getAi().isDestinationReached()) {
			removeCreature(creature);
			creature.getAi().onGeneralEvent(AIEventType.MOVE_ARRIVED);
			ZoneUpdateService.getInstance().add(creature);
		} else creature.getAi().onGeneralEvent(AIEventType.MOVE_VALIDATE);
	});
}
```

C++:

```cpp
class MoveTaskManager : public AbstractPeriodicTaskManager {
public:
	static MoveTaskManager& getInstance();
	void addCreature(Creature& creature) {
		if (!creature.isSpawned()) { log().warn("Failed attempt to add {} to moving creatures (despawned objects cannot move)", creature, UnsupportedOperationException()); return; }
		movingCreatures.putIfAbsent(creature.getObjectId(), CreatureRef(creature));
	}
	bool removeCreature(const Creature& creature) noexcept { return movingCreatures.remove(creature.getObjectId()); }
protected:
	void run() override { // world thread, every 200 ms; Deviation: sequential in insertion order (Java: parallel, unordered)
		movingCreatures.forEach([this](int32_t id, CreatureRef& ref) {
			Creature* creature = ref.get();
			if (!creature || !creature->isSpawned()) { // deleted or despawned: onDespawn -> abortMove should have removed it
				if (movingCreatures.remove(id)) log().warn("{} was still in moving creatures list but already despawned", id);
				return;
			}
			creature->getMoveController().moveToDestination(); // may despawn/delete others or itself: StableIdMap + graveyard keep this safe
			if (creature->getAi().isDestinationReached()) {
				removeCreature(*creature);
				creature->getAi().onGeneralEvent(AIEventType::MOVE_ARRIVED);
				ZoneUpdateService::getInstance().add(*creature);
			} else {
				creature->getAi().onGeneralEvent(AIEventType::MOVE_VALIDATE);
			}
		});
	}
private:
	MoveTaskManager() : AbstractPeriodicTaskManager(200ms) {}
	StableIdMap<CreatureRef> movingCreatures;
};
```

### How mechanical is the port?

**Schedule sites.** All 684 `ThreadPoolManager.getInstance().schedule*` sites in `data/handlers` were classified by a script: enclosing-method parameters and locals of game-object types that the lambda body references.

| Handler category | Sites | Capture only `this`/fields | Capture game-object locals | Body re-checks liveness |
|---|---|---|---|---|
| ai | 390 | 343 | 47 | 115 |
| instance | 244 | 220 | 24 | 23 |
| quest | 39 | 1 | 38 (player, qs, env) | 0 |
| commands and zone | 11 | 0 | 6 | 1 |

Heuristic counts (regex plus brace matching), expect ±10%.

| Change per site | Sites (est.) | Effort |
|---|---|---|
| Anchored member `schedule([this]…)`, body unchanged apart from Java→C++ syntax | ~560 handler + ~90 src | mechanical, regex-assisted |
| Captured object locals → `Ref` capture plus resolve-and-return at the top (quests: `PlayerRef` + QuestState re-lookup by questId) | ~115 handler + ~40 src | 2–4 lines each, mechanical |
| Scope decision beyond the syntactic owner (Xasta pattern, instance tasks started by AIs, tasks that must survive owner deletion), self-cancelling periodic arrays, FixPath | ~40–70 | needs reading the Java intent; this is the real semantic work |
| `Future<?>` fields and collections (166 handler + 97 src) → `TaskHandle` | ~260 | mechanical |
| Object fields (34 handler + 250 src direct, 7 + 83 collections) → `Ref`/`PlayerId` with resolve at use | ~370 | mostly mechanical; src team/effect/summon fields need the 2.4 rules |
| `getEffector()` null policy | 202 uses / 101 files | ~half trivial (effector known alive), rest need a branch |
| `synchronized`/`Atomic*`/concurrent collections | ~250 / ~400 / ~250 | *simpler* than Java: delete or rename |

**Handlers (~1,600 files).**
- 1,035 quests: almost unaffected (39 schedule sites, no stored object fields).
- 461 AI and 78 instance files: carry nearly all lifetime work.
- ~150 commands: synchronous apart from FixPath/State/Send.

Lifetime-related edits are ~3–5% of handler lines. The dominant cost of phase 6 remains Java→C++ syntax and API naming, which this design keeps (`getOwner()`, `spawn`, `schedule`, `PacketSendUtility`, `SkillEngine`).

---

## 9. Performance, debuggability and implementation effort

### 9.1 Performance for the realistic target

Estimates to validate in section 10.

- **Handle resolve:** paged array read plus generation compare, a few nanoseconds; 10⁶ resolves per second cost a few ms/s. KnownList broadcasts with ~100–300 entries are negligible.
- **Scheduler:** 4-ary heap. Push/pop at 10⁵ pending tasks is ~100–300 ns, so even 10⁵ task runs per second fit in a few % of a core.
  - Idle NPCs in inactive regions hold almost no timers (AI deactivated, no regen).
  - Anchored cancel destroys closures immediately. Java keeps cancelled ScheduledFutures and their captured objects until the delay expires, because setRemoveOnCancelPolicy is never set.
- **Movement:** only creatures in active regions move. With a few players, probably 100–1,000 movers. At an assumed 5–50 µs each (geo Z plus position update plus AI event plus occasional knownlist update), a 200 ms tick costs 0.5–50 ms, so the single thread has headroom. Exceeding the budget shows in the turn histogram, not as corruption.
- **Startup:**
  - static data parsed per file in parallel (pugixml, 156 MB)
  - 25,437 BIH trees (5.5M triangles) built eagerly in parallel
  - `spawnAll` of tens of thousands of objects on the world thread before accepting connections: probably seconds (each object is construct + publish + region insert)
  - The Java `forEachParalllel` for world spawns can return later as "construct in parallel, publish serially" if needed.
- **Memory:**
  - geo ~300 MB (research estimate)
  - NPCs probably 2–4 KB each with components (100–250 MB for 60k)
  - ObjectRegistry 1 MB per touched 65,536-ID page
  - scheduler slots ~64 B plus closure
  - reload retirements as in section 6
- **Latency hazards:**
  - inline DB saves (logout, 900 s periodic) and enter-world loads stall the world for their duration → phase 2 offload
  - long cron DB jobs (AbyssRankUpdateService) → split into DB phase and continuation
- **IO:** encryption and socket writes on N IO threads; broadcasts serialize once.

### 9.2 Debuggability

- **Determinism.** One thread and FIFO inbox: bugs reproduce with the same input order. Breaking in the debugger freezes the world consistently, with no half-updated state on other threads.
- **Named work.** Every task, post and cron job records its `std::source_location`. The watchdog, slow-task warnings, exception logs and RunnableStatsManager show "AturamSkyFortressInstance.cpp:151", not "lambda$12".
- **Introspection** (admin commands and `SystemInfo`):
  - `//tasks [target]`: pending tasks of an anchor with due times and sources
  - graveyard, quarantine and detached-players sizes
  - top anchors by pending tasks (leak finder)
  - inbox length and turn duration percentiles
- **Stale access is observable.** Debug builds count `Ref::get()` misses per call site. Dropped-with-anchor tasks log at DEBUG. `getOrThrow` pinpoints invariant violations. Graveyard poisoning plus ASan catch within-turn misuse of stored raw pointers.
- **No races to chase.** `assertWorldThread` turns a confinement violation into an immediate stack trace.

### 9.3 Foundation effort

Expert C++, excluding the game systems themselves:

| Component | C++ lines (est., incl. tests) |
|---|---|
| GameExecutor (inbox, turn loop, budgets, graveyard, pumpUntil, watchdog, thread assertions) | 900 |
| GameScheduler (slot map, heap, anchors, TaskHandle, fixed rate, deferred, runNowIfPending, ExecuteWrapper integration, stats) | 2,000 |
| ObjectRegistry, Ref/PlayerId/EffectRef/InstanceRef, DetachedPlayers | 1,000 |
| StableIdMap, WorldAtomic shims | 600 |
| IDFactory with quarantine and lifecycle hooks | 600 |
| CronExpression + CronService (+ ported CronServiceTest vectors) | 1,400 |
| BlockingPool with posted continuations | 400 |
| Packet path: AionServerPacket::serialize, OnceSerialized, AionConnection queue/framing/crypt hook, PacketSendUtility core, connection-dependence check | 1,200 |
| ReloadableHolder, config domain split and assertions | 300 |
| Enforcement: regex pre-commit + clang-tidy `aion-task-captures` (optional but recommended) | 300–700 |
| **Total** | **~8–9k lines, ~4–6 focused weeks** |

---

## 10. Risks, and what to prototype first

### Risks

1. **Silent behaviour drops from anchoring.** A task anchored to an owner that Java would have run after the owner's deletion simply never runs. The Xasta pattern is the known instance. Mitigation: the scope rule (4.4), DEBUG logs for dropped tasks, and the handler pilot measuring how common it is.
2. **Null handling spread.** Effector (202 uses), targets, handler Refs and aggro attackers each become a branch. A forgotten check crashes on `nullptr` rather than reading stale data. That is loud, but still a crash. `getOrThrow` and asserts make invariant-backed uses explicit.
3. **Single-core ceiling and DB stalls.** Fine for local play but unproven. If movement, AI or knownlist cost or DB latency exceed the budget, phase-2 offload and later per-map sharding add work.
4. **Capture enforcement** for `[this]`-style lambdas is only as good as the clang-tidy check. An MSVC-only workflow falls back to regex and review.
5. **Shutdown/disconnect choreography** (1.8) and nested pumping are easy to deadlock.
6. **Eager-serialization deviations:** sampling time, the 6 mutators, per-recipient flags. A missed `dependsOnConnection` sends wrong bytes to some viewers.
7. **DetachedPlayers and pins** are special machinery. A pin leak keeps offline players forever (visible in stats). A missing pin makes `TeamPlayers::resolve` assert.
8. **Reload retention** grows memory per //reload of items/skills.
9. **ID quarantine versus Java timing.** Clients or services relying on quick ID reuse are unlikely, but untested.
10. **Header churn.** Anchors, handles and graveyard rules must be in the spine headers (Player, Creature, Effect, AbstractAI, InstanceHandler) before parallel porting starts, or every chunk is reworked.

### Prototype order

See `prototypeFirst`. Each step has a pass criterion.

1. Kernel tests.
2. World vertical slice benchmark.
3. Handler porting pilot with counts of semantic-review sites.
4. Effect/skill slice under ASan.
5. DB timing of load/save.
6. Packet path against the real client.
7. Shutdown choreography.
8. Detached players and relogin scenario.


## Porting cost

Foundation: about 8–9k C++ lines including tests. That covers GameExecutor with graveyard and watchdog, GameScheduler with anchors, TaskHandle, deferred tasks and runNowIfPending, ObjectRegistry and handle types plus DetachedPlayers, StableIdMap, IDFactory with quarantine, CronExpression/CronService, BlockingPool, the eager packet path and ReloadableHolder, plus optional capture-lint tooling. Roughly 4–6 focused weeks for an expert, and it must land before the spine headers are frozen.

Handler schedule sites (684 classified by script):
- ~560 anchored `[this]` sites port nearly verbatim (regex-assisted).
- ~115 capture game-object locals and need Ref conversion (2–4 lines each).
- ~40–70 need a real scope or semantic decision: tasks that must outlive their syntactic owner (CaptainXastaAI pattern), self-cancelling arrays, FixPath's continuation rewrite.
- ~171 src sites follow the same split. About 40 in skills, effects and controllers need Effect/owner anchors.

Other lifetime-related work:
- ~260 Future fields/collections become TaskHandle (mechanical).
- ~370 object fields become Ref/PlayerId with resolve-at-use.
- 202 getEffector uses in 101 files need a null policy (about half trivial).

Offsetting savings: ~250 synchronized blocks, ~400 Atomic* uses and ~250 concurrent collections become plain code.

Net effect on the ~1,600 handler files: lifetime-related edits are about 3–5% of lines. 1,035 quests are barely affected (39 schedule sites). The work concentrates in 461 AI and 78 instance handlers. Overall phase-6 effort is similar to or slightly below a refcounted-Ref design with free threading, because the concurrency constructs disappear. The inline-DB default costs nothing extra; the phase-2 offload of enter-world load and big saves is a further ~1–2k lines if measurements demand it.

## Weaknesses

- Single world thread caps game logic at one core. It is fine for local play with a few players, but untested. If movement, AI or knownlist work, or spawnAll, exceeds the budget, the next steps are phased offloading and possibly per-map sharding, which is a large refactor of synchronous cross-map calls.
- Inline DB calls (enter-world ~30 queries, storePlayer ~17 DAOs, cron DB jobs) stall the whole world while they run. The phase-2 offload needs loader audits (World lookups inside DAOs) and row-snapshot DAOs for big saves, which departs from the line-by-line DAO port.
- Anchored tasks change semantics where Java ran a task after its owner was deleted (e.g. CaptainXastaAI scheduling another NPC's walk from a dying boss). Anchoring to the wrong scope silently drops behaviour. Mitigation is only a porting rule, DEBUG drop logs and pilot counts, not a proof.
- Handles replace GC retention with explicit null checks: 202 getEffector uses, targets, handler Npc fields, aggro and damage-list attackers, team lookups. Porting is more verbose, and a missed check crashes on nullptr where Java would have read stale-but-valid data.
- The capture rule for [this]-style anchored lambdas cannot be proven by the MSVC compiler. It relies on a custom clang-tidy check (needs a clang-cl CI preset) or regex and review. Only global tasks get a compile-time captureless proof.
- Deliberate behaviour deviations to document:
- packet bytes are sampled at send time, not NIO write time; the 6 mutating writeImpls run at send time
- ID reuse is delayed by a fixed quarantine instead of GC timing
- LS/CS packets become ordered
- attackers/effectors that no longer resolve are skipped in aggro and reward lists
- the DIE emotion may show killer id 0 for deleted master-less effectors
- MoveTaskManager runs sequentially in insertion order
- DetachedPlayers with per-team pins is special machinery. A pin leak retains offline players indefinitely (visible in stats), and a missing pin breaks the member-resolves invariant.
- The shutdown sequence must not call NioServer::shutdown on the world thread, because onDisconnect needs the world thread. This needs a coordinator thread plus nested pumping, which is an easy place to deadlock.
- Reloading item or skill holders never frees the old holder before shutdown (it keeps `const T*` valid), so each //reload costs its holder's memory.
- Spawn-family shared_ptr data and a few per-cast template fields (HostileUpEffect.tempHate, GuideTemplate.activated) keep Java's cross-instance mutation quirks. They are race-free now but still semantically shared.
- Anchors, handles and graveyard rules must be baked into the spine headers (Player, Creature, Effect, AbstractAI, InstanceHandler) before parallel porting begins. Late changes ripple through most chunks.
- Two identity notions (Ref objectId equality vs sameInstance generation equality, plus PlayerId) must be applied correctly at the ~14 audited pointer-identity sites and in relogin-sensitive code.

## Prototype first

- Kernel unit tests (GameExecutor, GameScheduler, TaskAnchor, TaskHandle, graveyard, ObjectRegistry/Ref, StableIdMap, IDFactory quarantine). Cover:
- self-cancel via TaskContext and via handle inside run
- anchor.cancelAll while the task is running, including a task that deletes its own owner (AIActions.deleteOwner inside an AI-anchored task)
- closure destruction that cancels other tasks (re-entrancy)
- runNowIfPending on deferred and on scheduled tasks, with exception rethrow
- getDelay after cancel
- fixed-rate catch-up without overlap
- an exception in a periodic task does not stop it
- StableIdMap removal and insertion during nested forEach
- Ref after unpublish/republish of the same objectId

Pass: all green under ASan and debug iterators.
- World vertical slice benchmark: World + WorldMapInstance + MapRegion + KnownList + Npc skeleton + MoveTaskManager + AggressiveNpcAI/GeneralNpcAI stubs. Spawn the largest world map's templates (e.g. 700010000 or 220080000) and measure:
- spawnAll time and memory per Npc
- turn duration histogram with 1, 5 and 20 simulated players walking
- movement tick cost with 100, 1,000 and 5,000 moving NPCs

Decide whether the single world thread has at least 5x headroom.
- Handler porting pilot of ~25 schedule-heavy files: CaptainXastaAI, KaluvaSpawnAI, YamenessPortalSummonedAI, PopuchinAI, HarlequinLordReshkaAI (Future[]), EnragedNightmareAI, SacrificialSoulAI, AturamSkyFortressInstance, LowerUdasTempleInstance, EmpyreanCrucibleInstance, DredgionInstance, TheHexwayInstance, DanuarReliquaryInstance, two quests with timers (_2208MauInTenMinutesADay) and FixPath.

Count the sites that needed a non-owner scope or other semantic review, run the capture lint/clang-tidy check for false positives and negatives, and measure MSVC compile time per handler TU.
- Effect/skill slice under ASan: Skill cast with hitTime, then applyEffect, a Poison DoT and a periodic-action toggle. Scenarios:
- summon effector released mid-DoT: aggro must go to the master
- NPC effector deleted mid-DoT: damage continues, no hate, no crash
- player effector logging out
- effected deleted mid-tick
- caster deleted during hitTime: effects still apply

Validate the EffectorInfo snapshot fields actually needed.
- DB timing against the local MariaDB 11.8: PlayerService.getPlayer (~30 DAOs + stone N+1) and storePlayer/InventoryDAO.store for a character with a full inventory, warehouse and quests, inline on the world thread. If the p95 stall is over ~50 ms, prototype the phase-2 off-thread load with unique_ptr handover and a row-snapshot inventory save.
- Packet path end to end with the real 4.8 client up to character select and entering the world:
- eager serialization, shared bytes for broadcasts, per-recipient serialization for SM_PLAYER_INFO/SM_MESSAGE
- SM_KEY afterWrite crypt enabling
- 3-strike decrypt tolerance
- N>1 IO threads

Add golden-byte tests for the connection-dependent flag check.
- Shutdown and disconnect choreography: coordinator thread calls NioServer::shutdown while the world thread pumps leaveWorld turns, then runs PeriodicSaveService.onShutdown, saveGameTime and cron/scheduler shutdown. Include a client disconnecting during the countdown and the 10 s delayed leaveWorld.
- Detached players and relogin: group of two, member logs out (adopted and pinned), SM_GROUP_MEMBER_INFO for the offline member, relogin within 600 s (PlayerConnectedEvent swaps the instance, old one retired, old PlayerRefs resolve null), timeout removal after 600 s (unpin), alliance/league variant, disband while the member is offline.
