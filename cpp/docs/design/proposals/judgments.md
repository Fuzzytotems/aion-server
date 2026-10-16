# Judges' scores for the runtime proposals

## Lens: porting-fidelity

Judged on porting mechanics and Java fidelity, single-logic-executor wins (8). It keeps Java's GC-shaped lifetimes (stale-but-readable effectors, team members, handler fields, delete-then-use) with one uniform rule per schedule site. Concurrency syntax disappears through Java-named shims, and writeImpl, DAO and quest bodies stay unchanged. Its deterministic ManualClock tests are the best way to catch subtle behaviour drift across about 1,600 handler files.

Free-threaded (6) has the most faithful execution model on paper. In practice, the per-field judgment each port needs (Field<T> on thousands of non-final fields, a container wrapper choice for every collection) is less mechanical than claimed. Its own Effect.periodicTasks example is misclassified: Effect.java:246-252 mutates the array in place. The races it keeps are nondeterministic and undetectable on MSVC, so fidelity is hard to verify.

Handles-world-owned (5) is the safest but the least faithful. Generational handles make every stored reference nullable. Anchored tasks change 'Java runs the task and guards' into 'the task is cancelled with its owner'. The verified CaptainXastaAI.handleDied pattern, and isDead-only-guarded AI tasks that still spawn through a deleted owner's position, show that the correct scope is a per-site semantic decision that parallel agents will not make consistently.

Recommended final design: the single-logic-executor core with these grafts:
- free-threaded's Pin-first schedule overload and handler detach at destroyInstance, replacing the weak position ref and hollow shell;
- source_location-keyed task stats and //tasks introspection;
- the handles proposal's shutdown coordinator and scripted site classification;
- a fix so the end-of-job outbox never drops packets queued before a connection close.

### single-logic-executor: 8/10

Strengths:
- Makes the fewest meaning changes at each site. Intrusive Ref<T> with end-of-job reclamation keeps Java's GC behaviour: pending tasks, effects (NpcController.java:294-298, verified), teams and handler fields all keep stale objects readable. The 222 isDead()/isSpawned() guards and the delete-then-use idioms port as they are. Checked against CaptainXastaAI.handleDied: its unguarded Ariana tasks and later deleteIfAliveOrCancelRespawn keep Java behaviour exactly.
- Most of the concurrency syntax disappears at the source level. synchronized is deleted. Atomic* and ConcurrentHashMap become shims with the same API, so the 311 handler Atomic* uses and about 245 concurrent collections port verbatim. There is no per-field shared-or-confined judgment, unlike Field<T> in free-threaded.
- writeImpl bodies stay byte-for-byte Java and are serialized when the job ends. Of the three designs this is closest to Java's lazy write, which in practice ran after the sender. The 6 writeImpls that change state stay legal and unchanged. The planned 'immediate' switch lets anyone check a packet's timing without touching code.
- Handler porting rules are few and uniform: store Ref, borrow Ptr, add keep = keepAlive() or task(this,&X::m), and Ptr throws NullPointerException like Java. Many parallel agents can apply them consistently, and a lint can check them.
- Deterministic single thread plus ManualClock/advance(). Handler ports can be tested against the Java intent: despawn mid-delay, destroyInstance while a task is pending, id release after the task. That is the best tool for catching subtle behaviour drift in about 1,600 handler files.
- The Future contract is complete: deferred, runNowIfPending that rethrows only for deferred tasks, getDelay after cancel (DropService), safe self-cancel. Also retiredAis for //ai set, and an std::array for controller tasks that keeps addTask's warn and cancel semantics.
- DAOs stay inline, so the 33 DAO calls inside model mutators and the 288 service call sites port line by line. The periodic-save race on storages goes away without any restructuring.

Flaws:
- The weak WorldPosition-to-instance reference with a 'hollow shell' changes meaning for every late task on a stale object after destroyInstance. A simpler fix exists: detach the handler to a no-op at destroy, as free-threaded does. That breaks the instance -> handler -> Npc -> position cycle and keeps the strong position ref Java has.
- In Outbox::flush, `if (con->isClosed()) continue;` can drop packets queued in the same job before close(closePacket), such as kick or ban messages or SM_QUIT_RESPONSE. The close path must go through the outbox in order.
- Serializing at job end reads the job's final state. Intermediate snapshots are lost when a job sends a packet and then mutates the same fields: HP updates during multi-hit effects, an item update followed by a move, SM_NPC_INFO followed by despawn. Java usually showed the same thing, but not always. Needs a documented deviation and client testing.
- Several structural deviations: movement runs sequentially in insertion order, so side effects of one creature are seen by the next; execute() runs after the current job; cancel releases captures eagerly; LS/CS packets are ordered. Each is small, but together they change interleavings the Java code was written under.
- Refcount cycles still depend on lifecycle hooks, and the leak census only detects failures. Handler back-references (an add's AI holding its spawner) are not audited per port the way free-threaded proposes.
- keepAlive in the capture list is a convention; a missing keep only shows up as a use-after-free under timing. Free-threaded's Pin-as-first-parameter overload is easier to lint and harder to forget.
- DB work inline on logic is fine for fidelity, but enter-world and storePlayer stall the world. Offloading later is not mechanical.

### free-threaded: 6/10

Strengths:
- Keeps Java's execution model most faithfully: the same pools, PacketProcessor seriality, monitors, parallel movement, and inline DB on the same threads. The question 'is this C++ line equivalent to that Java line?' stays local, and Java race behaviours (lost updates, TOCTOU) are reproduced rather than reordered.
- Refcounting plus epoch-based task-scoped borrows reproduces GC stack-root semantics, including resurrection (`boss = getNpc(...)`). Stale-but-readable effectors, teams and tasks port with no new null checks. Verified: Effect effector/effected are final, NpcController.onAttack uses isSpawned on a despawned summon, and PlayerLeaveWorldService deletes the player and then saves it.
- The Pin-parameter overload `schedule(this, [this]{...})` is a clean edit that a lint can check, and source_location on every schedule call gives call-site stats and slow-task warnings.
- Detaching instance handlers to a shared no-op at destroy breaks the handler cycle with a smaller change than a weak position reference.
- Good fidelity details: sendPacket loads the connection once (removes a TOCTOU without changing behaviour), the Monitor wait-graph watchdog keeps DeadLockDetector parity, the rejection policy is ported, and the IO-side SM_KEY special path is kept.
- A handler whose packet needs the connection but lacks the per-recipient flag gets con == nullptr and throws NullPointerException on its first test, so a misclassified connection-dependent packet fails loudly.

Flaws:
- The per-site workload is large and depends on judgment. Every non-final field of a shared class becomes Field<T>: I counted about 2,100 class-level non-final fields in model, including JAXB, 400 in skillengine and 333 in handlers. Every Java collection field needs a choice among CHM, COW list, SynchronizedList or 'confined'. That is less mechanical than the proposal claims, and agents working in parallel will classify inconsistently.
- The Field classification is already wrong in its own worked example. Effect.periodicTasks is called 'replaced wholesale -> boxed snapshot', but Effect.java:246-252 mutates array elements in place (periodicTasks[position-1] = task). Following the rule literally gives copy-on-write with lost updates when two positions are set concurrently.
- Memory safety depends on discipline across roughly 390k lines. A missed Field<T>, or a raw std::vector member in a shared class, is a silent data race that MSVC cannot detect; it needs a Linux TSan build. Race-induced behaviour differences stay nondeterministic, so fidelity is hard to verify.
- Eager serialization at sendPacket samples state earlier than Java's lazy write, and it allows stale-after-fresh ordering between two concurrent senders. That is a larger timing deviation than end-of-job flush.
- The Reclaimer is global: FixPath's 5 s get(), long enter-world tasks or a deadlock delay all reclamation. It also adds a new class of bug (an unregistered thread touching objects) that Java cannot have.
- Refcount cycles leak just as in the other Ref design, but free-threaded interleavings make cycle-breaking hooks racy: KnownList add is unsynchronized, and despawn can race a concurrent add. The zombie registry only detects leaks.
- Borrowed Ptr getters combined with Java code that captures locals in about 25% of schedule sites means frequent Ptr-in-capture mistakes. Debug stamps catch them only when the task actually runs during a test.

### handles-world-owned: 5/10

Strengths:
- Strongest by-construction safety: single-thread confinement, generational handles that never dangle, anchored tasks with destruction after run, and a per-turn graveyard for delete-then-use.
- The only proposal backed by a real site classification script (684 handler schedule sites split into this-only, captured locals and liveness-checking). The numbers are consistent with the research (685).
- Concurrency constructs mostly disappear (synchronized deleted, WorldAtomic shims, StableIdMap), just as in the single executor.
- Useful operational ideas: //tasks per anchor, dropped-task DEBUG logs with source_location, graveyard/quarantine/detached stats, and a coordinator thread for the NioServer::shutdown vs onDisconnect deadlock.
- Correctly identifies the CaptainXastaAI pattern (verified: handleDied schedules Ariana walks, door changes and a delete 1-26 s later from a dying AI). This is exactly the case where owner-scoped task lifetimes break.

Flaws:
- More meaning changes per site than either Ref design. Every stored object reference becomes nullable at use: 202 getEffector uses in 101 files, targets, handler Npc fields, aggro attackers, captured locals in about 115+40 sites (resolve-and-return instead of Java running on a stale object). A forgotten check crashes where Java read valid stale data.
- Anchoring cancels tasks when the owner is deleted, while Java runs them and relies on guards. Many AI tasks guard only on isDead(), and AbstractAI.spawn still works on a deleted owner through its position, so Java really performs spawns and door changes after the owner is deleted. The designated scope decides whether that behaviour survives, which is a per-site judgment. Parallel agents will decide inconsistently, and a wrong decision fails silently (a dropped task).
- Special machinery replaces the uniform GC semantics: DetachedPlayers with pins, EffectorInfo snapshots, items across time by re-lookup, StatOwnerKey, claimRemoved from the graveyard, and PlayerId vs Ref vs sameInstance identity notions. Each is a new place where the port diverges from the Java line.
- Documented deviations are larger in number and kind: rewards and aggro skip unresolved attackers, the DIE emotion shows killer id 0, dropped tasks, and eager packets sampled at send time plus the single-thread deviations.
- The capture rule for anchored lambdas can only be checked by a custom clang-tidy check that MSVC lacks. The captureless proof covers only global tasks, which are the minority.
- `explicit operator bool() = delete` plus `.get()` at every use adds syntax noise to every Java `x != null` check across the handler corpus.
- The phase-2 DB offload (unpublished unique_ptr<Player>, row snapshots) departs from line-by-line DAO porting if it becomes necessary.

Best ideas to graft:
- Base: a single logic thread with a non-atomic intrusive Ref<T>, a Ptr<T> that throws NullPointerException, and objects freed only between jobs (single-logic-executor). It keeps GC semantics at almost every site with the fewest edits.
- Java-API shims with the same names (AtomicBoolean/Integer/Reference, ConcurrentHashMap -> StableMap with tolerant reentrant iteration). Delete synchronized and leave a '// Java: synchronized(x)' comment, so handler bodies port verbatim.
- Replace the capture-list keepAlive convention with free-threaded's Pin-first overload `schedule(this, [this]{...}, delay)` plus a `task(this,&X::m)` sugar. It is harder to forget and trivially lintable.
- std::source_location on every schedule/post/cron call, feeding slow-task warnings, RunnableStatsManager and a //tasks introspection command (free-threaded + handles).
- Break the instance cycle by detaching the InstanceHandler to a shared no-op after onInstanceDestroy (free-threaded) instead of a weak position-to-instance reference with hollow shells. It changes less meaning and keeps Java's strong position ref.
- Keep writeImpl bodies unchanged and serialize at end of job, with a config switch for immediate serialization. Add a debug recording-connection proxy or null-con check that fails loudly when a packet reads con without dependsOnConnection. Route close(closePacket) through the outbox so packets queued before close are never dropped.
- A complete Future contract: deferred FutureTask, runNowIfPending that rethrows only for deferred tasks, getDelay valid after cancel, destroying the callable after run for safe self-cancel, and RunnableWrapper semantics so periodic tasks survive exceptions.
- ID release in ~AionObject (keeping the RespawnService deferral) plus a FIFO quarantine in IDFactory, keeping the lowest-free-first policy.
- A zombie/leak census listing deleted objects still alive after N minutes, plus free-threaded's per-port audit checklist for handler object fields that hold back-references.
- ManualClock + runReady/advance for deterministic handler conformance tests (despawn mid-delay, destroyInstance with pending tasks, relogin within 600 s), plus handles' scripted site classification to calibrate porting-cost batches.
- Handles' coordinator-thread shutdown choreography (never call NioServer::shutdown on the logic thread; pump logic while onDisconnect work drains).
- FixPath as a small coroutine or continuation object, and TeleportService/CM_TELEPORT_ANIMATION_DONE through deferred + runNowIfPending: the only two non-mechanical scheduler rewrites.

## Lens: cpp-safety

Judged on C++ correctness and robustness, handles-world-owned rules out the most bug classes by construction. Its registry ownership makes reference cycles impossible and gives every live object an owner that can be listed. Generations make cross-turn use-after-free and handle ABA impossible. Anchors make cancel-on-delete total. Global tasks get a compile-time capture proof. Confinement asserts run in release builds too. It has three real holes. First, raw T* from Ref::get() turns a missed null check into a whole-server crash. Second, anchoring silently drops tasks Java relied on: IncarnateAI's 5-minute claw delete is scheduled from an owner whose corpse decays in 2 s, so the claw leaks forever. Third, the anchored-lambda capture rule is only enforced by clang-tidy, and its regex lets `[this, raksha]` through.

single-logic-executor is a close second. It has the best determinism and test tooling (ManualClock, replay), NPE semantics and GC-faithful retention. It still leaves cycles and borrow discipline to lints and a census, and its thread asserts are debug-only over non-atomic refcounts. Its reentrant pump reclaims at nested depth. Its own MoveTaskManager port has a verified use-after-free: MOVE_ARRIVED re-adds the walker through WalkManager -> moveToNextPoint -> addCreature, which invalidates the `Ref<Creature>&` slot being iterated. The handles proposal's StableIdMap has the same V&-into-a-vector hazard.

free-threaded leaves nearly every bug class to discipline. Correctness depends on wrapping every mutable field of 400k lines. TSan is blinded by the atomics and needs a separate Linux build, deadlocks stay as in Java, and there is no determinism. Its global EBR reclaimer is fragile and stalls whenever one task hangs. Its part model has a use-after-free on //ai set.

Recommended synthesis: one world thread with registry ownership and generational handles, and a depth-0 graveyard. Handles resolve to a checked Ptr that throws NPE. Anchors get an explicit instance/world scope and an audit of schedules made from dying owners. Global tasks stay captureless. StableMap buffers insertions and uses node-stable values. Borrows get debug turn-stamps. Add ManualClock-based deterministic handler tests under ASan, and a leak census in CI.

### free-threaded: 4.5/10

Strengths:
- Every refcount and field is atomic, so a thread you forgot to register cannot corrupt the heap through a refcount. In the non-atomic single-thread design the same mistake silently corrupts memory in release builds.
- The epoch-based Reclaimer (EBR) gives a real 'valid until the task ends' guarantee for borrowed pointers. Destructors never run inline and never under someone else's lock, so dropping a Ref inside CHM.compute (CreatureController.addTask, CreatureController.java:400-410) is safe. The long destructor chains that cause recursion stack overflows are also avoided.
- Ptr/Ref/Field<Ref> throw NullPointerException on null. The Java TOCTOU pattern `if (endTask != null) endTask.cancel()` then fails as a logged exception instead of UB.
- In debug builds Ptr is stamped with (thread, taskSerial), so a borrow that escapes into a capture is caught deterministically on the first dereference from another task. This is the best escaped-borrow detector of the three proposals.
- Concurrent containers never expose iterators and forEach runs callbacks on an EBR-protected snapshot. Re-entrant mutation during iteration (e.g. the walker re-add in MoveTaskManager) is therefore memory-safe.
- The Future state machine is thought through: the callable is destroyed after run returns, get() is woken by cancel, which is what FixPath needs, and runNow is a CAS. Diagnostics are keyed by source_location, and a Monitor wait-graph watchdog is included.

Flaws:
- Freedom from data-race UB depends entirely on porting discipline across ~400k lines. Every Java HashMap/ArrayList/String field of every shared class must become Field<>, SynchronizedList or a concurrent wrapper. Missing one raw std::vector member turns a Java ConcurrentModificationException into heap corruption. The header lint is heuristic ('shared class' is nearly everything), and the proposal itself admits the guarantee holds 'given the porting rules'.
- TSan is structurally blinded. Field<T> converts every detectable data race into an atomic but logically racy load-modify-store, so TSan reports nothing while lost updates and cross-field inconsistency remain. TSan also needs a second, Linux clang build of the whole server, and hand-written EBR needs TSan annotations to avoid false positives.
- Hole in the part model: OwnedPart forwards counting to the owner, so Pin(this) on an AI pins the Npc, not the AI. //ai set (data/handlers/admincommands/Ai.java:84-90) replaces the 'final' ai field through reflection. The C++ unique_ptr part is then destroyed while a ScheduledPool task holding `[this]` of the old AI, or the ForkJoin move tick, is running inside it: a use-after-free that the design never addresses.
- Epoch reclamation is global and fragile. One stuck or long task stops reclamation for the whole process: a deadlock (which the design keeps), FixPath's 5 s get(), enter-world, or long-running pool jobs such as custom-instance model training. Resurrection (0->1) plus re-retire requires restamping retireEpoch on an already-queued object, and the API sketch does not show this. Any bug here corrupts everything.
- Deadlocks remain exactly as possible as in Java: 254 synchronized blocks plus explicit locks. The claim that every added lock is a leaf lock is imprecise. CHM.compute calls Future::cancel under the stripe lock, which takes the scheduler heap lock, the Reclaimer queue lock and a logger lock (log.warn). That is a nested chain, and it stays acyclic only by convention.
- Eager serialization on concurrent sender threads allows a stale packet to be enqueued after a fresh one: two threads serialize HP/inventory state, then enqueue in reverse order. writeImpl also reads multi-field state that is being mutated concurrently, which produces internally inconsistent packets.
- Nothing is deterministic: 4 packet threads, N scheduled threads and ForkJoin. single_executor mode only approximates production interleavings, and there is no injectable clock for handler tests. It is the weakest of the three for reproducible tests.
- Reference cycles leak (handler back-references, aborted despawns). The zombie registry only detects them after 10 minutes. Instance handler cycles need a detach deviation.
- An NPE thrown from a destructor or a noexcept path running on the Reclaimer thread calls std::terminate.

### single-logic-executor: 7/10

Strengths:
- Confining everything to one logic thread (I1) removes data races, KnownList cross-updates (KnownList.java:192), writeImpl-vs-mutation races and the periodic save racing with play. This comes from the architecture, not from wrappers.
- The end-of-job zombie list keeps `AIActions.deleteOwner(this)` followed by getOwner() safe. Reclaim runs as a loop, not recursive deletes, so long destructor chains cannot overflow the stack.
- The best testability: ManualClock, runReady/advance, (due, seq) timer order, a single seeded Rnd, and inbox record/replay. Handler scenarios such as despawn mid-delay or destroyInstance with a pending 60 s task become deterministic GoogleTests under ASan.
- Checked Ref/Ptr throw a Java-style NPE that is logged per job. A missed null check becomes a log line, not a world crash.
- It explicitly handles //ai set with a retiredAis list, Future self-cancel, a deferred future plus runNowIfPending, and getDelay on a cancelled task (DropService.java:119).
- It keeps GC retention semantics (stale but readable effector, and group members through PlayerTeamMember's final Player field), so fewer behaviour bugs come from adding null handling. The cycle break for groups is verified in PlayerGroup.java:30-33 (onRemoveMember calls setPlayerGroup(null)).
- A packet_serialization switch between end_of_job and immediate lets you bisect timing-dependent packet bugs without code changes.

Flaws:
- The design's own MoveTaskManager port has a use-after-free. `forEach` hands out `Ref<Creature>&` into the StableMap's dense slot. The callback calls removeCreature(creature), which tombstones that slot. It then calls onGeneralEvent(MOVE_ARRIVED), and for a walker with restTime 0 the path WalkManager.chooseNextRouteStep -> moveToNextPoint -> NpcController.onStartMove -> MoveTaskManager.addCreature (verified in WalkManager.java:172-177, NpcMoveController.java:114-121, NpcController.java:311) calls putIfAbsent on the map while it is being iterated. A push_back into the dense vector can reallocate, so the `creature` reference and the next line `ZoneUpdateService::add(creature)` dangle. 'Stable' iteration as specified does not cover values handed out by reference, nor `get()` returning V* that is 'valid until the next insertion' (AggroInfo and KnownObject are stored by value).
- I1 is enforced only by debug-build asserts, and refcounts are non-atomic. A Ref that reaches a BlockingPool job, an IO callback or the Asio destructor path in a release build is silent heap corruption with no diagnostic.
- pumpUntil during shutdown runs nested jobs, and each JobScope calls reclaimer.reclaim(). Unlike the handles proposal, reclaim is not restricted to depth 0. The outer shutdown job's borrowed Ptrs (for example, iterating players to kick or save) can be freed by inner leaveWorld jobs.
- Reference cycles still leak whenever a Java lifecycle hook misses a case, which exceptions in despawn make likely. The leak census only detects them. Object IDs leak together with the objects.
- The weak position->instance link plus a raw `MapRegion*` 'valid only while the weak ref is alive' is an invariant that every accessor must check. Any direct getMapRegion() on a stale, pinned Npc after destroyInstance, e.g. isMapRegionActive in ThinkEventHandler, is a use-after-free unless the accessor itself goes through the weak ref.
- Borrow discipline (never store a Ptr, keepAlive on every [this] capture) is lint and review only. A missing keepAlive on an AI capture is a cross-job use-after-free. The alive-check on Ptr dereference is debug-only.
- End-of-job serialization reads final state for packets built mid-job (for example, a position captured before a teleport in the same job). Outbox dedupe keyed by packet address serializes a reused cached packet once, with only its last state. An exception in one writeImpl during flush needs per-packet catching, which the design does not specify.
- Coroutines (FixPath) bring their own lifetime pitfalls, and detached frames are not covered by the zombie or census tooling.

### handles-world-owned: 8/10

Strengths:
- It has the most bugs impossible by construction. Ownership is a forest (unique_ptr in registries), so reference cycles cannot exist and every live object can be enumerated. Cross-turn use-after-free is impossible for handles. ABA on handles, TaskHandles and EffectRefs is exact through generations and 64-bit serials, and it survives Player objectId reuse on relogin.
- Anchored tasks make cancel-on-owner-removal total, not just for controller tasks. The scheduler destroys a callable only after it returns, so self-cancel and 'owner deleted inside its own task' are structurally safe.
- Global tasks get a compile-time proof: a captureless callable plus SafeTaskArg arguments. It is the only proposal with a compiler-checked capture rule for part of the ~850 schedule sites.
- assertWorldThread is enabled in all builds at resolve, World and scheduler entry points, so a confinement violation terminates with a stack trace instead of corrupting memory in release.
- The graveyard is swept only at depth 0, which correctly handles nested pumps. Graveyard poisoning plus ASan catches misuse of stored raw pointers within a turn.
- ID release is deterministic (destructor, RespawnService deferral, quarantine), so ABA on bare IDs depends on events, not timing. DecayTask is converted to NpcRef.
- Deterministic and debuggable: FIFO inbox, source_location on every task, //tasks per anchor, graveyard, quarantine and DetachedPlayers stats, per-call-site counters of stale Ref misses, and a watchdog on the turn timestamp.
- Fewest locks: synchronized, Atomic* and the concurrent collections become plain code, so deadlocks are limited to the executor inbox, the AConnection guard and IDFactory.

Flaws:
- `Ref::get()` returns a raw T*, and `Player& player = *getConnection()->getActivePlayer();` (its own CM_TELEPORT_ANIMATION_DONE port) dereferences unchecked. A missed null check across the 202 getEffector uses, targets, handler NpcRefs and aggro attackers is an access violation that takes down every player's session, whereas Java logged an NPE for one task. For a large mechanical port this is the main robustness regression.
- Anchoring silently drops tasks that Java ran after the owner was gone. Concrete case: ai/siege/IncarnateAI.java:40-48. The dying incarnate spawns a claw and schedules `claw.getController().delete()` in 5 minutes from its own AI. The corpse decays after 2 s without drops (RespawnService.java:34 IMMEDIATE_DECAY), so the anchored task is cancelled and the claw is never deleted: a permanent object leak in the siege map. MarabataControllerAI and SuramaTheTraitorAI (a 10 s task after moving) have the same shape. The only mitigation is a DEBUG log plus a porting rule.
- The capture rule for anchored lambdas is not proven. The regex rejects only `[&` and `[=`, so `[this, raksha]` (a raw Npc* in SuramaTheTraitorAI.startDialog, 8 s later) or `[this, &npc]` passes. Only a clang-tidy check, which needs a clang-cl CI preset, catches these. On an MSVC-only workflow the result is a cross-turn use-after-free.
- StableIdMap has the same hazard as the single-executor proposal. `forEach(fn(int32_t, V&))` and `V* get()` point into a `std::vector<pair<int, optional<V>>>`. Insertion from a re-entrant callback reallocates, and a tombstone that resets the optional destroys the value under a live V&. The MoveTaskManager sample avoids this only because it copies to `Creature*` first; KnownObject& and AggroInfo& callbacks do not.
- The DetachedPlayers pin invariant (every team member always resolves) is enforced by an assert. A missing pin, for example if the logout ordering between onPlayerLogout and adopt changes, makes TeamPlayers::resolve assert and crash. A pin leak silently retains offline Players.
- The phase-2 off-thread Player load builds objects on pool threads. Constructors that touch non-atomic singletons (World lookups in PlayerRegisteredItemsDAO.constructObject, caches, QuestEngine) would be real data races that the all-build asserts only catch if those singletons assert.
- It has two identity notions (objectId equality versus sameInstance), and choosing the wrong one at the ~14 identity sites or in relogin paths is a logic bug the type system cannot flag.
- Shutdown needs a coordinator thread plus nested pumping around NioServer::shutdown, an easy place to deadlock.

Best ideas to graft:
- Base the design on 'handles-world-owned': a single world thread, registry unique_ptr ownership (no cycles), generational Ref/TaskHandle/EffectRef for anything that crosses a turn, and a graveyard swept only at depth 0.
- Make Ref::get() return a checked Ptr<T> whose operator-> throws NullPointerException, as the other two proposals do, instead of a raw T*. The job/turn boundary logs it, so a missed null check in one of the 202 getEffector sites is a log line, not a server crash. Keep getOrThrow for invariants.
- Keep world-thread asserts in all builds, not only debug (from handles-world-owned). If any intrusive refcount survives, count on the world thread and assert thread identity in release too.
- Compile-time captureless global tasks with SafeTaskArg/bindTask (from handles-world-owned), plus a mandatory clang-tidy capture check in a clang-cl CI preset. Extend the regex to reject any capture of a non-handle identifier next to this in schedule calls.
- Anchors with an explicit escape hatch: schedule(anchor=instance/world, ...) for tasks that must outlive their syntactic owner. Include a porting audit of every schedule inside handleDied/handleDespawned/after deleteOwner (IncarnateAI claw delete, CaptainXastaAI) and a DEBUG counter of dropped tasks with source_location.
- Fix StableMap/StableIdMap. Buffer insertions made during iteration and apply them at depth 0. Tombstone without destroying the value until compaction. Store V as a stable node (or pass callbacks a copy or handle) so a V&/V* is never invalidated by re-entrant put/remove. Add an ASan regression test for the MoveTaskManager walker re-add (MOVE_ARRIVED -> WalkManager.chooseNextRouteStep -> moveToNextPoint -> addCreature).
- ManualClock, runReady/advance, a seeded Rnd and inbox record/replay (from single-logic-executor), for deterministic handler tests that cover despawn mid-delay, instance destroy with pending tasks, and relogin inside a group.
- Debug stamps on borrowed pointers (from free-threaded): record a turn serial when a Ptr/T* is obtained and assert on dereference in a later turn. This catches escaped borrows deterministically on MSVC without TSan.
- retiredAis/graveyard for //ai set, which replaces the final ai field through reflection (Ai.java:84-90). Never destroy an AI part while tasks or iteration may be inside it.
- The Future state machine from all three proposals: destroy the callable only after run returns, runNowIfPending for deferred and scheduled tasks, getDelay valid after cancel. Also source_location on every schedule call for slow-task, exception and stats reports.
- ID quarantine plus RespawnService deferral, and convert bare-int holders that outlive the quarantine (DecayTask) to handles.
- Leak census and zombie registry (from single-logic-executor/free-threaded) as a test assertion after destroyInstance and logout, even under registry ownership, to catch pin leaks in DetachedPlayers and lingering anchors.
- A packet_serialization=immediate|end_of_job debug switch (from single-logic-executor) layered over eager serialization, with per-packet exception isolation during flush.
- A Linux clang CI build for ASan, UBSan and clang-tidy, even though TSan matters less once logic is confined to one thread.

## Lens: hobby-pragmatics

For one hobby developer running a local server with a few players, the single logic executor wins clearly (8.5). Its mental model is the simplest: one thread, Refs for anything stored, nothing freed mid-job. A non-atomic intrusive Ref with end-of-job reclamation keeps the GC semantics the Java handlers depend on, such as stale effectors, offline team members and deleteOwner-then-use, so the ~1,600 handler files port with almost no semantic edits. ManualClock-driven determinism also makes later custom content pleasant to write and test. The performance ceiling is not a real constraint: I verified that AI thinking is gated on isMapRegionActive, so only NPCs near players cost CPU, and critic.md confirms only two blocking sites. The handles/world-owned design (6) offers the strongest construction-time safety and the best-grounded research (its CaptainXastaAI anchor hazard is real). But it pays the single-thread costs and adds a lot of semantic porting work: null policies for 202 getEffector uses, 40-70 anchor scope decisions, DetachedPlayers pins, and item re-lookup. That is more concept load than a solo hobbyist gains from. The free-threaded design (3.5) is the most faithful to Java, but for this user it is the worst fit. It demands Field/Monitor/EBR discipline across every shared class, keeps Java's nondeterministic races, needs a Linux TSan build to find bugs, and has the longest, riskiest foundation before anything visible works. Recommended final design: the single-logic-executor base, with these grafts: always-on thread asserts; source_location on every task; per-owner task introspection without dropping tasks; a strong position-to-instance reference with handler detach at destroy instead of the WeakRef hollow shell; a zombie registry; outbox flush before close(packet); a shutdown coordinator thread; a logic-only config split; and the phase-2 ownership-transfer DB offload if measurements demand it.

### free-threaded: 3.5/10

Strengths:
- It stays closest to the Java reference. Thread kinds, monitors and lifetimes all match, so checking a C++ line against its Java line stays a local question.
- Task-scoped epoch borrows (EBR) are a clever copy of the JVM stack-root guarantee. Getters return a Ptr with no refcount traffic, resurrection is legal, and destructors never run under someone else's lock.
- It keeps Java's parallel headroom: ForkJoin movement, 4 packet threads, and blocking DB calls that never stall the whole world. It is the only proposal with no single-core ceiling.
- The Future design is complete and correct. It covers the claim-then-run of the deferred teleport task, get(timeout) woken by cancel (FixPath), destroying the callable only after a self-cancelling run returns, and RunnableWrapper-style handling where periodic tasks keep running.
- Good tooling ideas: slow-task stats keyed by source_location, a Monitor wait-graph watchdog (a portable DeadLockDetector), a zombie registry with //debug refs, debug Ptr task stamps, and ID quarantine.
- Eager serialization of stack-temporary packets is simple, allocation-light and makes N IO threads safe.

Flaws:
- It has the heaviest mental model of the three. A hobby developer has to reason about epochs, Field<T> atomic representations, boxed immutable strings, Monitor versus leaf locks, concurrent wrappers with snapshot iteration, and NPE branches. All of that is needed just to write a custom AI script safely.
- Safety depends on discipline, not construction. Every non-final field of every shared class, including handler fields like CaptainXastaAI's plain `canThink` bool touched from scheduled and ForkJoin threads, must be wrapped. One raw member or unpinned [this] brings back timing-dependent UB, and that is miserable to debug alone.
- Real race detection needs TSan, which means a Linux clang build. The user is Windows/MSVC first, so in daily use races go undetected.
- It faithfully keeps Java's logical races: lost updates, TOCTOU, AI events running concurrently on ForkJoin threads. They are memory-safe but still nondeterministic, which is the opposite of 'pleasant to work in'.
- The Reclaimer is subtle, catastrophic-if-wrong global infrastructure. For example, every 1->0 release must re-stamp retireEpoch after a resurrection, or a later borrow can be freed. It deserves a formal-ish stress suite before anything visible works, and FixPath waits or long enter-world tasks delay all reclamation.
- Field<std::string> and wholesale-replaced vectors allocate a box on every write. Monitor and NPE branches on every object, plus atomic refcounts, make the code heavier than idiomatic C++ for no benefit at a few players.
- The 45-60 focused days look optimistic once concurrent containers, EBR, cron, pools and the lint/TSan CI are included. That is the longest time before anything visible works.
- Its main argument, fidelity and checkability, matters less for a hobby server than debuggability. The argument that 'the target doesn't need performance' also works against the free-threaded design itself.

### single-logic-executor: 8.5/10

Strengths:
- It has the simplest correct mental model. One logic thread owns all state, a stored reference is a Ref and a borrowed one is a Ptr, nothing is freed before the job ends, and packets serialize at job end. Locks, atomics and CHM become no-op shims with the same API.
- Non-atomic intrusive Ref with end-of-job reclamation keeps GC semantics (stale-but-readable effectors, offline team members held for 600 s, deleteOwner(this) then more calls) without handler null checks. Per-site handler edits are essentially `keep = keepAlive()`, so the ~1,600 handler files change very little.
- Deterministic and testable. ManualClock with runReady()/advance() lets a GoogleTest run 22 s of an instance script instantly, which is great for later custom content. It adds a (due, seq) timer order, replay from the recorded inbox, and complete stacks.
- It uses a dedicated loop rather than an Asio strand, and justifies this well: job-boundary hooks, thread-affinity asserts, and the thread_local AbstractAI.DEPTH recursion guard.
- End-of-job serialization keeps writeImpl bodies byte-for-byte. The 6 mutating writeImpls become legal, queued packets never outlive a job, and the packet_serialization=immediate switch is a good diagnostic.
- Scale is realistic for the target. I verified that ThinkEventHandler gates AI on isMapRegionActive, so only neighbourhoods near players cost CPU, and a 2,000-mover tick at 10-40 ms of one core fits the 200 ms budget.
- The FixPath coroutine and runNowIfPending are minimal, correct answers to the only two blocking sites that critic.md verified.
- Foundation is about 8k lines in 3-5 weeks. Its parts are small and independent, each unit-testable in isolation.

Flaws:
- The WeakRef position-to-instance with a 'hollow shell' adds a WeakAnchor mechanism plus a semantic deviation. Detaching the instance handler at destroyInstance, as in the free-threaded proposal, breaks the same cycle more simply.
- The outbox interacts with close(packet). CM_QUIT.java:64 and LoginServer.java:143,157 call `con.close(new SM_...)` so the packet goes out before disconnect. If queued packets are not flushed first, the outbox reorders them behind the close. This is not addressed.
- End-of-job serialization can collapse intermediate states. For example, two SM_STATS or SM_INVENTORY_UPDATE sends in one job both carry the final state. That is closer to Java than eager, but still a deviation, and debugging it needs the switch.
- StableMap::get returns a V* valid only until the next insertion (dense vector). For value-type entries like KnownObject that is a single-thread use-after-free trap exactly where reentrant callbacks occur.
- Inline DB calls stall the world. The stall is acceptable at a few players, but prototype 4 must actually measure it. Heavy account character lists and legion loads could stutter.
- It still relies on refcount-cycle discipline (leak census) and on a lint for keepAlive captures, and Ptr-stored-in-field mistakes surface late.
- A single-core ceiling for large sieges or world raids, with only the two-phase movement fallback.

### handles-world-owned: 6/10

Strengths:
- It gives the strongest safety guarantees by construction. assertWorldThread in all builds, generational handles that never dangle, anchors that make cancel-on-delete total, a turn graveyard, and a compile-time captureless proof (SafeTaskArg) for global tasks.
- Deterministic, event-based lifetime and ID release: an ownership forest you can enumerate, no refcount cycles to leak, and the quarantine is a fixed function of events.
- The research is the best grounded of the three: it classified all 684 handler schedule sites by script (343/390 AI and 220/244 instance sites capture only this). I also verified its CaptainXastaAI finding: handleDied schedules Ariana's walk, skill and door changes 1-26 s later from a dying AI.
- Anchored `schedule([this]...)` members on AI and instance handlers are pleasant for writing new custom scripts. Introspection like //tasks, graveyard, quarantine and detached-player sizes, plus source_location on every task, helps a solo developer.
- The phase-2 DB offload design is the cleanest: load an unpublished unique_ptr<Player> on the blocking pool and transfer ownership. It also has an explicit shutdown coordinator thread that avoids the NioServer::shutdown/onDisconnect deadlock, and a clear config domain split.
- Eager packets with stack temporaries and shared immutable bytes are simple and make N IO threads safe.

Flaws:
- It changes semantics wherever Java relied on GC retention, so porting needs real per-site thought. That includes the 202 getEffector uses in 101 files, targets, aggro and reward attackers, handler Npc fields, and ~40-70 anchor scope decisions where a wrong choice silently drops behaviour (the Xasta pattern). Each of those sites needs real thought rather than a mechanical edit.
- It needs more special machinery than the others: DetachedPlayers with per-team pins and claimRemoved from the graveyard, EffectorInfo snapshots, re-lookup of items by (owner, itemObjectId), two equality notions (objectId vs sameInstance) plus PlayerId, and kind-tagged checked casts. It is more concepts for the hobby developer to hold.
- A forgotten null check crashes (nullptr) where Java read stale-but-valid data. Several documented deviations follow (attackers skipped in reward lists, DIE emotion showing killer id 0).
- Capture safety for anchored lambdas depends on a custom clang-tidy check that needs a clang-cl preset. On MSVC-only workflows it falls back to regex and review.
- Items across time (ItemUseObserver, exchange, repurchase) and object fields (~370) need rework beyond syntax. The claim that overall effort is 'similar to or slightly below' a Ref design is doubtful given the null-policy and scope decisions.
- It shares the single-core ceiling and inline-DB stalls of the single executor, but with more porting effort, so it pays the costs of both simplifications without the ease of Ref retention.
- Anchors, handles and graveyard rules must be frozen in the spine headers early, and late changes ripple through every chunk.

Best ideas to graft:
- Use single-logic-executor as the base: one dedicated logic-thread loop with job-boundary hooks, non-atomic intrusive Ref/Ptr, end-of-job zombie reclamation (ID released in ~AionObject behind a quarantine), Java-shaped Future (runNowIfPending, deferred, self-cancel-safe) and ManualClock for deterministic handler tests.
- From handles: make assertWorldThread a one-compare check in all builds (not only debug) on World, ObjectRegistry-style lookups, the scheduler and refcount paths; add a compile-time captureless/SafeTaskArg overload for global service timers.
- From handles: add an optional per-owner TaskAnchor/`schedule` member on AI, instance handler and controller, used for introspection (//tasks per object, top owners by pending tasks) and cancel-on-delete bookkeeping. Keep Ref keepAlive semantics, so no task is silently dropped.
- From free-threaded and handles: tag every schedule/post with std::source_location for slow-task warnings, RunnableStatsManager and watchdog dumps.
- From free-threaded: replace WeakRef position->instance plus hollow shell with a strong ref, and detach the instance handler to a shared no-op handler in destroyInstance after onInstanceDestroy(). Add a zombie registry and a //debug refs command listing objects deleted N minutes ago that are still alive.
- Packets: keep writeImpl bodies unchanged with a serialization-timing switch, but flush the connection's outbox before close(packet) (CM_QUIT, LoginServer). Consider eager stack-temporary packets (free-threaded/handles) as the default if the outbox proves awkward, since they avoid a heap allocation per packet and ordering corner cases.
- From handles: the phase-2 DB offload shape (load an unpublished unique_ptr<Player> on the BlockingPool, publish on the logic thread) if enter-world stalls are measurable. Add a dedicated shutdown coordinator thread so NioServer::shutdown never runs on the logic thread.
- From handles: split config into fields that only the logic thread reads (plain) and fields read by IO or pools (ConfigValue/atomic), with a logic-thread assert on rebind.
- From free-threaded: run LS/CS link packets through an ordered per-link serial path, build the geo BIH eagerly in parallel at startup, and keep reload holders immortal (leak-on-reload, logged).
- From handles: a script-based classification of all schedule sites before phase 6. Also use their Xasta-pattern finding as a test case, so Ref retention keeps Ariana's delayed walk working after the boss AI is deleted.

