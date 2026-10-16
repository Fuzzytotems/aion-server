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

## Added after wave 1 (2026-09-14)

- Value-type classes listed in `fieldmap.toml [settings] value_types` (geomath Vector2f, Vector3f, Matrix3f, Matrix4f, Ray) are copied:
  `const Vector3f` or `Field<Vector3f>` members, passed by value or reference, never RefCounted and never `Ref`.
- Interned classes listed in `fieldmap.toml [immortal]` (ZoneName, Effect.ForceType) derive `Immortal` and are referenced as `const X*` or
  `Field<const X*>`, like templates. Singletons found by the `SingletonHolder` pattern are Immortal too.
- Per-run service objects that use Java's singleton pattern must be listed in `fieldmap.toml [settings] per_run_services` (today:
  AhserionRaid), otherwise the singleton detection makes them Immortal.
- Lint waivers and mapping comments are listed in runtime-architecture.md §12.2 (`// lint: Lx <reason>`, `// fieldmap-class: <FQN>`); a
  waiver without a reason is reported as W0.

## Added after spine step S0a (2026-09-14)

- Pinning a part (`{this, &part}`) keeps that part alive while the task is pending, even if it is replaced in a `PartSlot<RECLAIMER>` or
  `PartMap`. An owner and its part share one of the 4 pin slots; two parts of one owner take two.
- The kernel starts and stops through `runtime::RuntimeLifecycle` (runtime-architecture.md §23). `shutdown()` runs only outside any
  `TaskScope`; pool tasks and cron jobs post the shutdown to the ShutdownHook thread.
- Static data hierarchy roots (generated structs and xmlgen shells) derive `runtime::StaticTemplate`, so `const T*` templates can be pinned
  and captured without trait specializations.

## Added after spine steps S0b and S0c (2026-09-14)

The frozen headers follow [hub-headers.md](hub-headers.md); these are the rules every porter meets first.
- A RefCounted class that derives a static data root through xmlgen (`PlayerCommonData`) is not a template: `IsStaticTemplate` is false for
  it, so its pointer is no `TaskArg` and `Pin` retains it. No trait specialization is needed.
- Object parameters are `X&` unless Java passes `null` directly at a call site, compares the parameter with `null` in the method family, or
  stores it in a one-statement setter; then `Ptr<X>` (hub-headers.md §5.1). Enums, boxed numbers and Timestamps with the same evidence are
  `std::optional`. Returns are `Ptr<X>`; part, owner and singleton accessors return `X&`; factories of new objects return `Ref<X>`, of new
  parts `std::unique_ptr<X>`.
- A Java generic whose type parameters all have a project bound is one non-template class (erasure rule, hub-headers.md §8.1); `fwd.h`
  declares it as a class. A Java override that only casts `super.m()` is a non-virtual narrowing redeclaration, not a virtual override.
- Visible objects are created only with `VisibleObject::create<T>(...)`; constructors take `CreateKey` first and only store members, the
  constructor-body work that needs the dynamic type runs in `postConstruct()`.
- Interfaces held by `Ref<I>` declare pure virtual `retain()`/`release()`; the first implementor with a runtime base forwards them, Immortal
  and static data implementors define no-ops. `toString()` is non-const on RefCounted, OwnedPart and Immortal classes; methods of static data
  classes are `const`.
- A deviation from `fieldmap.py --class` is a `fieldmap.toml` decision with a reason (`[fields]`, `[kinds]`, `[bases]`, `[captures]`,
  `[cpp_members]`); the header carries a `// fieldmap.toml: <reason>` note. `// fieldmap:` waivers are only an interim form. Copy the
  `--class` block, which also prints lock classes and C++-only members.
- Every lockable member carries `{AION_LOCK_CLASS(JavaClass::field)}`; a missing tag fails `gs.lint.concurrency`.
- `OwnerRef<O>` members exist only in `OwnedPart` classes; any other non-retaining back reference needs a reviewed `fieldmap.toml` override
  with the lifetime argument.
- A Runnable class handed to `schedule*` or a stored-callback API is never K5: with object members it is K4, RefCounted with a protected
  constructor, `create()` and `AION_MAKE_REF_FRIEND`; with only immutable members it may be a K3 `TaskStruct`.
- C++-only cycle breakers are named `<verb>WithoutNotify` or `break<Member>`, are `noexcept` once ported, and are listed next to their
  `cycles.toml` edge. A porter who adds a retaining reference reruns `fieldmap.py` and adds `cycles.toml` entries for the new edges.
- Static comparator constants are `static const std::function<...>` defined in the `.cpp`. Java generic methods are inline member templates
  with `AION_UNPORTED();` bodies. `LinkedHashMap` results ordered by value are `std::vector<std::pair<K, V>>`. A `static final` RefCounted
  object is `static const Ref<X>&` bound in the `.cpp` to a never-destroyed `Ref` (`ItemService::DEFAULT_UPDATE_PREDICATE`, `PanesterraTeam`).
- The spine is frozen: no `__has_include` guards (tools.gen fails on any), and changes to frozen headers go through header requests
  (hub-headers.md §14). Bodies and new files of the owning chunk need no request.
