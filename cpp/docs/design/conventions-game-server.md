# Game server porting conventions (proposed)

> These rules follow from the runtime architecture. They become part of CONVENTIONS.md once the prototypes (P1-P9) validate them.

- Threads and task scopes: game code runs on the Java-shaped pools (PacketProcessor, ScheduledPool, InstantPool, LongRunning, Cron, ForkJoin), on IO threads only inside AionConnection callbacks, and in main/shutdown phases. Every entry point opens a runtime::TaskScope. Never create std::thread/std::async/detach outside runtime/.
- Reference kinds: stored object references (fields, container elements, captures, packet members) are Ref<T>. Parameters, locals and return values are Ptr<T> or T&. A borrow is valid until the current task ends; never store or capture a Ptr, raw pointer or reference to a RefCounted object. Templates are const T* and immortal.
- RefCounted classes have protected destructors and are created only with X::create(...) (makeRef). Classes inferred K5 CONFINED by fieldmap.py are plain value types and may live on the stack; never store them in shared objects, captures or packets.
- Member declarations of shared (K4) classes are derived mechanically from the Java field, never chosen by hand: final/effectively final -> const T or Final<T>; non-final or volatile -> Field<T> (Field<Ref<X>>, Field<std::string>, Field<FutureRef>); arrays -> Array<T>; collection fields -> the same-named runtime shim with object elements as Ref<T>; Atomic* keep their names; ThreadLocal -> thread_local. Copy the block printed by tools/gen/fieldmap.py, including generated structs for anonymous classes; lint_concurrency.py checks it. Waivers need '// confined: <reason>' or '// fieldmap: <reason>'.
- Collections: local collections stay std::. Iterate shims with range-for or snapshot(); explicit Java iterators become `auto it = x.iterator();` with hasNext/next/remove. Shims follow Java equals/hashCode for elements whose Java class overrides them, identity otherwise.
- Parts: objects Java creates for an owner (new X(this), setF(new X(this)) in a constructor, controllers bound with setOwner) derive OwnedPart and are held as const std::unique_ptr<X>, PartSlot<X>, PartMap<K,X> or PartList<X> as parts.json says. A part's reference to its owner is OwnerRef; a field that may hold the owner or a foreign object is SelfOrRef. Parts are destroyed by their owner or by the Reclaimer after replacement.
- Locks: `synchronized (x)` -> SYNCHRONIZED(x), synchronized methods -> SYNCHRONIZED(*this) as the first statement. ReentrantLock -> Monitor, StampedLock/Semaphore -> same-named shims. Code inside ConcurrentHashMap compute/merge callbacks ports as written (callbacks run under reentrant stripe Monitors). RankedMutex is for runtime/ and network/ internals only and never guards user callbacks.
- Scheduling and stored callbacks: schedule(this, &X::method, delay) or schedule(this, [this] {...}, delay). Every `this`/`&name` capture must be in the pin list ({this, &player}). Other captures are values or Ref init-captures. Unpinned callables must be captureless, an aggregate TaskStruct with TaskArg members, or bindTask(fn, args...). The same rules apply to observers, request handlers, cron jobs and event end tasks (PinnedCallback). [&] and [=] are forbidden in stored lambdas. Future<?> -> FutureRef.
- Destructors of RefCounted/OwnedPart types are noexcept and release-only: drop Refs, push the object id to the Cleaner queue, log ids. No dereference, no container call, no SYNCHRONIZED, no packets, no events, no service calls.
- Immortal is only for static-storage singletons, templates, quest handlers and commands. Per-run service objects (Siege, Base, WorldRaid, Event, Assault, AhserionRaid, Invasion, AgentFight) are RefCounted and pinned.
- Cycles: every retaining field or captured variable that can form a reference cycle has an entry in cycles.toml (part / java-hook / cpp-breaker / zombie-safe / accepted). C++-only breakers live in LogoutBreakers or onDelete and run from a finally guard, never as the last statements of a function that can throw. They are listed in DEVIATIONS and covered by LeakCensus scenario tests.
- Long work: loops that visit many objects in one task open a runtime::QuiescentScope at the top of the task body, iterate a std::vector<Ref<T>> snapshot and call runtime::quiescentPoint() per element. Blocking waits (Future::get, Semaphore, DB) run inside BlockingRegion (built into the shims and DAO layer).
- KnownList pairs are added only through KnownList::addPair.
- Server packets: sendPacket(player, SM_X(...)) without new. Object members are Ref<T>. writeImpl keeps the Java body and never sends packets. Packets whose writeImpl reads the connection override recipients() (generated). Only packets without write-time side effects may be cached. con.close(new SM_X()) -> con->close(SM_X()).
- AionConnection methods on IO threads (constructor, initialized, processData, writeData) open a TaskScope and contain no game logic. Tasks they schedule capture std::weak_ptr<AionConnection>.
- Banned in game-server code: strtok, localtime, gmtime, asctime, ctime, rand/srand/std::rand, setlocale, runtime getenv, mutable function-local statics, std::string_view/std::span/Ptr members in shared classes, thread_local Ref/Ptr.
- Config fields rebound at runtime follow the existing rule: scalars std::atomic<T>, non-scalars ConfigValue<T> read through a local snapshot. The game server requires database.socket_timeout > 0.
- Builds and tests: the dev server runs the checked RelWithDebInfo build (AION_CHECKED). Handler, AI and instance tests use GameServerHarness on DeterministicExecutor with ManualClock and seeded Rnd under the msvc-asan preset. Lock-order validator reports fail tests.
- When porting a known Java race intentionally, keep it and mark it `// java-race: <what>`. Fixing it requires a DEVIATIONS entry.

## Added after the kernel prototypes (2026-09-13)

- Compute-family callbacks (`compute`, `computeIfAbsent`, `merge`, ...) must not write other keys of the same `ConcurrentHashMap`
  (Java's CHM contract). Move the second write out of the callback or guard the whole operation with an explicit map-level Monitor
  (design §21, lint L20).
- Collection fields are never `const` (views and iterators write through).
- `synchronized` blocks become `SYNCHRONIZED(x) { ... }`; a synchronized method wraps its body in `SYNCHRONIZED(*this) { ... }`.
- Java `list.remove(int)` becomes `removeAt(int)`; map reads return `Nullable<V>` (`Ptr` for references, `std::optional` for values).
- RefCounted classes with protected constructors declare `AION_MAKE_REF_FRIEND`; create objects only with `T::create`/`makeRef`.
- A `Ptr`/`T&` borrow is valid only inside the task that obtained it; store `Ref`s. `T&` obtained from `*ref` is valid only while the Ref is held.
- Future, Pin, PinnedCallback and TimeUnit live in `aion::gameserver::runtime` and are re-exported in `aion::gameserver::utils`.
