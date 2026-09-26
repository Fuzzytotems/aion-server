# Hub headers (spine step S0b): style guide

> **Status:** binding for S0b and S0c, and for header requests after the freeze (2026-09-14). Written by the S0b pattern stage and extended
> with the S0b/S0c rules; refines [handlers-and-porting-plan.md](handlers-and-porting-plan.md) §2.5 and its amendments §2, §4, §8 on top of
> [runtime-architecture.md](runtime-architecture.md) and [conventions-game-server.md](conventions-game-server.md). Reference implementation:
> `model/gameobjects/AionObject`, `Persistable` and `VisibleObject` (`.h`/`.cpp` under `cpp/game-server/src/aion/gameserver/model/gameobjects/`).
> What S0b/S0c delivered and the freeze state: [spine-status.md](spine-status.md).

After S0b and S0c the spine is frozen: every later chunk compiles against these headers and changes them only through header requests (§14).
A hub header therefore has to be right in two things that are expensive to change later: the **member layout** and the **declarations**.
Bodies are cheap: almost all of them stay `AION_UNPORTED();`.

Contents: 1 Workflow · 2 What S0b ports · 3 File shape and includes · 4 Members · 5 Reference kinds in signatures · 6 Strings, numbers,
external types · 7 Collections, iteration, callbacks, varargs · 8 Generics · 9 Classes, interfaces, nested types, virtual · 10 Construction
(`create<T>`/`postConstruct`), parts, owners · 11 Statics, singletons, loggers, locks · 12 Packets and connections · 13 Draft TODOs ·
14 Header requests after the freeze · 15 Per-group checklist

---

## 1. Workflow

1. Regenerate the draft of your hub (the pattern stage fixed the systematic draft errors listed in §13):
   `python tools/gen/skeleton.py --draft <FQN> --with-dependencies --out build/s0b-drafts-<key>` (from `cpp/`). A draft follows existing
   hand-written headers, so rerun it after a base class of your hub lands.
2. Print the member block: `python tools/gen/fieldmap.py --class <Java FQN>` (and `--class <FQN of an anonymous class key>` for callback
   structs). Read the Java file next to both.
3. Write `X.h` and `X.cpp` at the mirrored path (`tools/porting/chunks.py owner <path>` must name the chunk you were given). Copy from the draft,
   then apply the rules below. Never keep a `TODO(...)` line in a hub header: resolve it or turn it into a declaration plus a comment.
4. Build your own build directory twice (a new `.cpp` compiles only on the second build), then check: zero warnings,
   `aion_gs_header_check` (your header is in its explicit list and joins it automatically once the file exists, §3.4),
   `python tools/porting/lint_concurrency.py --werror --cycles=core game-server/src` (the CTest `gs.lint.concurrency` command line).

## 2. What S0b ports and what stays `AION_UNPORTED`

| Ported in S0b (inline in the header where §3.3 allows, otherwise in the `.cpp`) | Stays `AION_UNPORTED();` in the `.cpp` |
|---|---|
| Trivial accessors: Java `return f;`, `this.f = p;`, `return <literal>;` | Every other method body |
| Empty Java bodies (`{}` inline in the header; `/wd4100` is global, so unused parameters do not warn), `return <parameter>;` pass-throughs and plain member comparisons (`isInState`): default hooks of AIs, instance, zone and quest handlers would otherwise throw | |
| Constructors whose Java body only stores fields, calls `super(...)`/`this(...)`, creates parts (`new X(this)`) or binds owners | Constructors doing anything else (service calls, registration, packets): the member initializer list is still written, then `AION_UNPORTED();` |
| Bodies a constructor, `postConstruct` or `create` reaches that only delegate to a trivial accessor of a member (`SpawnTemplate::getWorldId`) or store a new part into its container (`SpawnTemplate::addTemplate`, `SpawnGroup::addSpawnTemplate`) | |
| Destructors (release-only, runtime-architecture.md §2.7), including `CleanerQueue::push` of auto-release ids | |
| `VisibleObject::create<T>` / `postConstruct()` plumbing; part getters and setters; owner binding (`setOwner` → `bindOwner`) | `postConstruct()` statements that call services (e.g. `AIEngine::newAI`): `AION_UNPORTED();` inside the override |
| Narrowing accessors (§8.2): a cast of the base accessor | |
| `equals`/`hashCode`/`compareTo` that only read final scalar members (the collection shims call them) | `toString()` |
| Whatever a static initializer runs at load time (it must never reach `AION_UNPORTED`, which would abort every executable): `Persistable::newPredicate` | |

A `[[noreturn]]` `AION_UNPORTED();` needs no `return`. A `noexcept` function never holds a bare `AION_UNPORTED();`: port it, or, where the
`noexcept` contract must not terminate the caller, wrap it as `try { AION_UNPORTED(); } catch (const runtime::UnportedException&) {}`
(`LogoutBreakers::run`/`onDelete` stubs). C++-only breaker helpers that `cycles.toml` names (`ObserveController::clearWithoutNotify`,
`PlayerController::breakStanceObserver`, `EffectController::clearEffectMapsWithoutNotify`,
`CreatureGameStats::clearEffectFunctionsWithoutNotify`) are declared without `noexcept` while their bodies are unported; `LogoutBreakers` wraps
each step. Stub parameters keep the Java names; a parameter that would hide a data member of the class **or of a base class** (C4458, e.g.
`NpcController::petLoot(Npc& owner)` against `VisibleObjectController::owner`) is renamed `value` in the definition only, or `<name>Value`
(then `<name>Arg`) where several overloads would collide.

- **Creatable objects.** A constructor that only needs pure helpers (string formatting, arithmetic, `commons::utils::currentTimeMillis()`)
  is ported with them, so registries and tests can create the object before the rest of the class is ported: `ChatCommand`'s
  `parseSyntaxInfo` (every command), `QuestState(questId, status)`. A helper of a class that is not ported yet stays local to the `.cpp` with a
  comment naming the Java method it stands for. Constructors that read static data or call services stay `AION_UNPORTED();` (§2 table).
- **Unported `synchronized` stubs.** Lint L7 compares the `SYNCHRONIZED`/`lock()` count of each body with Java and skips bodies that consist
  only of `AION_UNPORTED();` (S0c), so stubs need no `// lint: L7` waiver (the S0b waivers are removed). The port adds the `SYNCHRONIZED`
  Java takes and L7 compares it again.
- **Static initialization.** A static initializer that touches a runtime shim (a `ConcurrentHashMap` put) opens a STARTUP `TaskScope` around
  it: `ZoneName::NONE` and `Effect_ForceType::DEFAULT`/`MATERIAL_SKILL` are defined in the `.cpp` by lambdas that open the scope and intern the
  object (verified to run before `main` in the checked build; the scope pins nothing).

## 3. File shape and includes

### 3.1 Header

```cpp
#pragma once

#include <cstdint>                                              // std headers first, sorted
#include <string>

#include "aion/gameserver/runtime/fields/Field.h"               // lean runtime headers
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/controllers/fwd.h"                    // fwd.h of every package a declaration names
#include "aion/gameserver/model/gameobjects/AionObject.h"       // the full header of the direct base class(es) only

namespace aion::gameserver::model::gameobjects {

/**
 * <Java javadoc, shortened>
 * <p>
 * <C++ notes: what differs from Java and why, with design references>
 *
 * @author <Java authors>
 */
class VisibleObject : public AionObject {
```

A hub header includes only:

| Allowed | Examples |
|---|---|
| Standard headers | `<cstdint>`, `<string>`, `<memory>`, `<optional>`, `<vector>`, `<functional>`, `<span>`, `<any>`, `<chrono>` |
| `fwd.h` of any package | `aion/gameserver/world/fwd.h` (declares classes and generated enums with their underlying type) |
| Generated enum headers | `aion/gameserver/model/gameobjects/Persistable_PersistentState.h` (needed for nested aliases and default member initializers) |
| Value types held by value | `geoEngine/math/Vector3f.h` and the other `fieldmap.toml [settings] value_types` |
| Commons | `aion/commons/utils/ByteBuffer.h`, `aion/commons/database/SqlTypes.h` (not `Logger.h`, see §11.3) |
| Lean runtime headers | `runtime/lifetime/{Ref,RefCounted,Parts}.h`, `runtime/fields/{Field,Final,Array,Atomic}.h`, `runtime/collections/*.h`, `runtime/sync/{Monitor,Semaphore,StampedLock}.h`, `runtime/sched/{Future,Pin,PinnedCallback,TimeUnit,TaskConcepts}.h`, `runtime/base/Exceptions.h` |
| The full header of each direct base class and implemented interface | `AionObject.h` in `VisibleObject.h`; `Persistable.h` in `Item.h` |
| A hoisted nested class (§9.3) | `skillengine/model/Effect_ForceType.h` in `SkillEngine.h` |
| The DAO or model header of a nested type a signature or member names (a nested class cannot be forward-declared; the class comment says why) | `dao/AbyssRankDAO.h` (`RankingListPlayer`), `dao/BookmarkDAO.h`, `dao/PlayerDAO.h`, `model/team/legion/Legion.h` (`Announcement`), `model/house/HouseBids.h` (`Bid`) |
| `runtime/base/Unported.h` | only in headers of class templates (inline stub bodies, §8.3) |

Never: another hub's full header (except a base), `services/`, `dataholders/` holders, `ThreadPoolManager.h`, `<windows.h>`/Asio, spdlog.
Where a declaration seems to need a complete type, change the declaration (reference or pointer, out-of-line body), not the include list.
The one Asio exception is `network/aion/AionConnection.h`, whose direct base is commons `AConnection` (`WindowsMacroGuard.h` is its last
include). Client packet headers do not include it: commons `BaseClientPacket<T>` binds the connection's `toString` in `setConnection()`, so
only the translation unit that calls `setConnection` needs the complete connection type, and `AionClientPacket.h` forward-declares it (S0c).

### 3.2 Source file

The `.cpp` includes its own header first, then `runtime/base/Unported.h`, then whatever the bodies need (full hub headers are fine here).
Private loggers live here (§11.3).

### 3.3 Complete types, inline bodies and `__has_include` guards

Members of incomplete types (`const Ref<X>`, `Field<Ref<X>>`, `PartSlot<X>`, `std::unique_ptr<X>`) are fine in the class body. Anything
that releases, deletes or checks such a member needs `X` complete and therefore lives in the `.cpp`:

| Inline in the header | Out of line in the `.cpp` |
|---|---|
| Scalar `Field<T>` getters and setters, `Field<std::string>` getters/setters, `const` scalar getters | Constructors and the destructor (they instantiate member destructors) |
| `Field<Ref<X>>` / `const Ref<X>` / `SelfOrRef<X>` getters returning `Ptr<X>` (`return position.get();`, `return spawnTemplate;`) | `Field<Ref<X>>` and `SelfOrRef` setters (they release the previous value) |
| `OwnerRef` and late-bound `Final<O*>` owner getters, template pointer getters, literal returns, references to collection-field shims | Part getters (`PartSlot::operator*` names `typeid(X)`), part setters and `setOwner` (`bindOwner`) |
| | Narrowing accessors (the cast needs both types) |

**Since the freeze no guard is allowed** (`skeleton.py SPINE_FROZEN = True`: any `__has_include` guard outside `runtime/` fails tools.gen).
A header that a new definition needs is written first, as a declaration header or through a header request (§14). The rest of this section
records how S0b and S0c used guards.

A definition that needs a header which does not exist yet is wrapped, exactly that definition, in a `__has_include` guard with a marker
comment, as `VisibleObject.cpp` did. There are two kinds of guard:

| Kind (marker comment) | Headers it names | Removed |
|---|---|---|
| `S0b transition` | only hub headers (`skeleton.py HUBS`) and spine headers (`SPINE_HEADERS`, §3.4) written by other S0b groups | by the integrator at the freeze (every hub header exists by then) |
| `Member types` | at least one non-hub header: a part, member, element or return type (`AIEventLog.h`, `TransformModel.h`, `AggroInfo.h`) | in the change that adds the last header it names; none may remain at the freeze (§3.5) |

```cpp
// S0b transition (docs/design/hub-headers.md §3.3): ... Remove the guard once they exist (spine freeze).
#if __has_include("aion/gameserver/controllers/VisibleObjectController.h") && __has_include("aion/gameserver/world/WorldPosition.h")
#define AION_S0B_VISIBLE_OBJECT_PARTS 1
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/world/WorldPosition.h"
#else
#define AION_S0B_VISIBLE_OBJECT_PARTS 0
#endif
```

Rules (`python tools/gen/skeleton.py --guards` lists every guard in `game-server/{src,tests,handlers}` outside `runtime/` with the headers it
still waits for; the tools.gen test `SpineGuardsTest` enforces them):
- **An open guard is an error** (every header it names exists), except an `S0b transition` guard before the freeze. MSBuild records only the
  headers a compile read (the CL tlogs never list a header `__has_include` did not find), so it does **not** rebuild a guarded `.cpp` when the
  header appears: the guarded constructor stays undefined (LNK2019 at the first `create`) and its code uncompiled until a clean build. Whoever
  adds a header removes the guards waiting for it in the same change, and reconfigures or rebuilds the files.
- Guard only on headers that are scheduled: every missing header is on the §3.5 list or in the plan of the chunk that writes it.
- A nested type of a missing header used in a declaration (`HouseBids::Bid`, `Legion::Announcement`) needed a `Member types` guard around those
  declarations in the header, with the matching definitions in the `.cpp` under the same macro; `--guards` listed header guards too.
- Tests follow the same rules (the scenario guards of `tests/objects/SpinePrototypeTest.cpp`).

### 3.4 Header check

`aion_gs_header_check` (`game-server/CMakeLists.txt`) compiles every hub header in its own `/W4 /WX` TU. The hub list is explicit
(`gs_hub_headers`: `skeleton.py HUBS` without generated enums, plus `utils/ThreadPoolManager.h` and `utils/idfactory/IDFactory.h`); each entry is a
`CONFIGURE_DEPENDS` glob matching exactly that file, so a header that does not exist yet is skipped and a header that appears re-runs the
configure step on the next build. The C++-only **spine headers** the hubs and `HandlerRegistry.h` depend on (`gs_spine_headers` =
`skeleton.py SPINE_HEADERS`: `model/Expirable.h`, `model/GameEngine.h`, `model/gameobjects/player/LogoutBreakers.h`,
`model/stats/calc/StatOwner.h`, `model/team/GeneralTeam.h`, `model/team/TeamMember.h`, `model/templates/L10n.h`,
`network/aion/SerializedBody.h`, `network/aion/StateSet.h`, `world/zone/handler/GeneralZoneHandler.h`) are frozen with the spine and checked
the same way. Configure prints `N of 74 hub and spine headers`. `tools/gen/tests/test_skeleton_tree.py` keeps both lists equal to `HUBS` and
`SPINE_HEADERS`; a new hub or spine header needs an entry in both.

### 3.5 Freeze gates

The spine is frozen (handlers-and-porting-plan.md §2.5) only when all of these hold; the integrator then sets `skeleton.py SPINE_FROZEN = True`,
which turns every later `__has_include` guard into a tools.gen failure:
1. `python tools/gen/skeleton.py --guards --freeze` reports no guard: every `S0b transition` guard is removed and every `Member types` header
   exists.
2. Hence every hub constructor and destructor is compiled and linked. The member-type headers come as **S0c declaration headers**: the class
   with its correct runtime base (fieldmap `base`), the constructors, `create` overloads and accessors the hub definitions call, and all other
   bodies `AION_UNPORTED();`. The chunk that owns the directory writes them (or the integrator, before the freeze).
3. `tests/objects/SpinePrototypeTest.cpp` runs its Npc scenario unskipped (no scenario guard left, no `GTEST_SKIP` on an unported body).
4. Two builds with zero warnings, `aion_gs_header_check`, `lint_concurrency.py --werror --cycles=core game-server/src`, tools.gen and the full
   CTest pass.

**State at the freeze (S0c, 2026-09-14): all gates hold.** `--guards --freeze` reports 0 guards and 0 missing headers, and `SPINE_FROZEN` is
`True`. Every member-type header exists: the S0c model lanes wrote 139 declaration classes (the former table of about 90 headers here, from
`AIEventLog.h` and `TransformModel.h` to `GeoMap.h` and `Account.h`, plus their closure) and the integrator wrote 83 more headers that no lane
had scheduled (Siege, SiegeLocation, HouseObject, Letter, DropItem, Event, Area, Point3D, PetFeedProgress, ...). `python tools/gen/skeleton.py
--definitions` (tools.gen `HubDefinitionsTest`) checks that every declared member function of the 82 hub and spine headers has a definition
(about 1,840 declarations, 0 undefined; overloads are not told apart by signature). `SpinePrototypeTest` runs the Npc scenario unskipped;
what it does not create (a full Player, an instance handler) is listed as freeze exceptions in [spine-status.md](spine-status.md).

Static data the handler bases read in their constructors was an ordering, not a header gate. `QuestsData::getQuestById` is ported (S0c), so
`AbstractQuestHandler(questId)` and `QuestZoneHandler(questId)` are ported too; `QuestZoneHandler` throws
`IllegalStateException("com.aionemu.gameserver.world.zone.handler.ZoneNameAnnotation missing element questId")` for quest 0 or a quest without
a template, as Java's annotation check does. Tests publish quest data with
`DataManager::QUEST_DATA.publish(xml::bindString<QuestsData>(context, xml, name))` and call `resetForTests()` afterwards
(`SpinePrototypeTest.cpp`).

## 4. Members

- **The layout is `fieldmap.py --class` verbatim**, qualified from the class scope, in Java declaration order, with Java access (package-private
  → `public`). Never choose a member type by hand. `fieldmap.py` itself prints the erasure rule (§8.1), the §6 external spellings (Timestamp,
  Date, `Ref<JobDetail>`, CronExpression, chrono types), `static const X* const` constants of the class's own type and the lock-class
  initializers (§4 below). A deviation is a **`fieldmap.toml` decision** with a reason (runtime-architecture.md §3.2.2): `[fields]` overrides
  (also `drop = true`; `retains = [...]` names cycle-graph targets the Java type hides, `DropNpc.lootingTeam` behind a `WeakReference`),
  `[kinds]`, `[bases]`, `[captures]` and `[cpp_members]` for C++-only members (`AionObject.autoReleaseObjectId`,
  `Player.legionStorageProxy`, `Effect.hostileUpTempHate`). L1/L19 accept a member spelled exactly like such a decision, so it needs no waiver; the header carries a plain
  `// fieldmap.toml: <reason>` note. A `// fieldmap: <reason>` waiver (L1/L2/L3) is the interim form until the integrator adds the decision; at
  the freeze none is left in `game-server/src`. Never waivable: a C++-only member of a retaining type (`Ref`, `FutureRef`, parts, `SelfOrRef`,
  `unique_ptr`/`shared_ptr`) that `fieldmap.json` does not list (L2), an `OwnerRef` member outside an `OwnedPart` class (L3, §10.3) and a
  missing lock class (below).
- **Lock classes (RR-16, runtime-architecture.md §3.4).** Every `Monitor`, `StampedLock`, `Semaphore`, collection shim (`ArrayList`,
  `HashMap`, `HashSet`, `TreeMap`, `EnumMap`, `ConcurrentHashMap`, `CopyOnWriteArrayList`, `ArrayDeque`, ...) and `Atomic*` member, static
  members included, is initialized with its static lock class, the Java declaring class and field name: `ArrayList<int32_t>
  questOnDie{AION_LOCK_CLASS(QuestEngine::questOnDie)};`, `Semaphore permits{AION_LOCK_CLASS(X::permits), 1}` (Java literal arguments follow the
  tag), `ConcurrentHashMap<...> knownObjects{AION_LOCK_CLASS(KnownList::knownObjects#stripe)};` (stripe monitors), nested classes
  `Outer::Inner::field`, a renamed member keeps the Java name (`onInvisibleTimerEnd_{AION_LOCK_CLASS(QuestEngine::onInvisibleTimerEnd)}`).
  Without the tag the checked build's lock-order validator reports the shim type ("ArrayList") and merges unrelated collections into one
  node. `Field<Ref<RcX>>` collections get the tag where the body creates them (`RcArrayList<int32_t>::create(AION_LOCK_CLASS(X::f))`).
  `skeleton.py` drafts and `fieldmap.py --class` print it; a line over 150 columns breaks after the `{`, and a comment then goes on the line
  above (clang-format breaks `#stripe` and is not run on these headers). The tag may sit in the member declaration, in the member initializer
  list of a constructor of the same class, or in the out-of-line definition of a static member (`Type Class::member{AION_LOCK_CLASS(...)}`).
  Since the freeze a missing tag is a lint warning (`LOCK_CLASS_SEVERITY = 'warning'`), so `--werror` and CTest enforce it; no waiver covers it.
- **Initializers:** `{}` for fields and references, the lock class for lockable members (above); `{*this}` for
  `PartSlot`/`PartMap`/`PartList`/`SelfOrRef`; Java literal
  initializers go into the braces (`Field<bool> lookingForGroup{false};`); other Java field initializers (`CreatureState.ACTIVE.getId()`,
  `System.currentTimeMillis()`, `new byte[ZoneType.values().length]`) go into the member initializer list of every constructor, in Java
  order, as Java runs them before the constructor body. No brace initializer for `const` members set by a constructor and for `OwnerRef`.
- **Static members** follow §11.1. `static inline` in the header only for literal constants and default-constructed shims.
- **Parts** come from `parts.json` via fieldmap: `const std::unique_ptr<X>`, `PartSlot<X[, RetireTo::RECLAIMER]>`, `PartMap<K, X>`, `PartList<X>`.
  Part classes derive `runtime::OwnedPart` (fieldmap `base`).
- **Cycles:** never change a member kind to break a cycle; resolutions go into `cycles.toml` (S0b reviewer). The exception is a link that
  does not own its target in Java either, spelled non-retaining by a `fieldmap.toml` override: the parent link of a tree whose parent owns
  its children (`Spatial.parent` is `Field<Node*>`: with a `Ref`, every node that `GeoWorldLoader` builds and drops would keep its children
  and itself alive) and back links into objects owned elsewhere (`Link.input`/`output`).
- **C++-only state** that replaces a JVM mechanism (Cleaner registration, retired AIs, `AionConnection::monitor_` for `synchronized (this)`,
  `AbstractAI::registryEntry` for `@AIName`) is allowed with a `fieldmap.toml [cpp_members]` entry and a comment naming the Java mechanism.
- **Cycle breakers** that `cycles.toml` names are C++-only methods called `<verb>WithoutNotify` or `break<Member>`
  (`VisibleObject::breakTarget`, `IdianStone::breakActionListener`), declared next to the member they cut and listed with their edge. A porter who
  adds a retaining reference reruns `fieldmap.py` and adds `cycles.toml` entries for the new edges (lint L16, `--cycles=core` in CTest).

## 5. Reference kinds in signatures

The kinds of runtime-architecture.md §2.1, made mechanical:

| Java type in a signature | Parameter | Return | Notes |
|---|---|---|---|
| K3/K4 class (RefCounted, OwnedPart, interfaces they implement), **non-null** | `X&` | `Ptr<X>` | default for parameters (§5.1) |
| same, **nullable** | `runtime::Ptr<X>` | `runtime::Ptr<X>` | evidence rules §5.1 |
| a part, owner or singleton the method always has | – | `X&` | `getController()`, `getKnownList()`, `getAi()`, `getOwner()`, `getInstance()`; throws `NullPointerException` when a `PartSlot` is empty |
| a newly created object (Java body `return new X(...)`, factories) | – | `runtime::Ref<X>` | never return a `Ptr` to an object nobody holds |
| a newly created part (`createAggroList()` returning `new AggroList(this)`) | `std::unique_ptr<X>` | `std::unique_ptr<X>` | the owner stores it into its `PartSlot` |
| K1 static data template (JAXB class, xmlgen shell) | `const X*` | `const X*` | nullable like Java, immortal. A RefCounted class that derives a template root through xmlgen (`PlayerCommonData` via `CreatureTemplate`) is not a template: `IsStaticTemplate<X>` is false for every RefCounted class, so its pointer is no `TaskArg` and `Pin` retains it; `getObjectTemplate()` of a Player is never captured (lint L5) |
| interned immortal (`fieldmap.toml [immortal]`: `ZoneName`, `Effect.ForceType`) | `const X*` | `const X*` | |
| K5 confined class | `X&`; nullable: `X*` (`Terrain::collide(..., CollisionResults*)`, GeoMap.canSee passes null) | `X` by value, `std::unique_ptr<X>` if abstract; `std::optional<X>` if Java may return null (`CollisionResults::getClosestCollision`) | never stored; collections of K5 elements hold values (`std::vector<CollisionResult>`) |
| K2 server packet | `network::aion::AionServerPacket&` (or the concrete `SM_X&`); nullable: a borrowed `SM_X*` (`cancelCurrentSkill`, packets are stack temporaries) | `SM_X` by value; a cached packet Java returns and shares: `std::shared_ptr<SM_X>` (`AbyssRankingCache::getLegions`) | §12 |
| `AionConnection` | `network::aion::AionConnection*` | `std::shared_ptr<AionConnection>` | commons convention |
| value types (`Vector3f`, ...) | `const Vector3f&` | `Vector3f` | |
| enums | by value | by value | generated enums only |
| `this` passed to a call | `*this` | | binds to `X&` and to `Ptr<X>` |

Locals are `Ptr<X>` (or `X&` bound from a known non-null expression); stored references are always `Ref<X>`/`Field<Ref<X>>`. A `Ptr` or `X&`
never outlives the task (lints L3, L5).

### 5.1 Nullable or not

A parameter of object type is `Ptr<X>` when any of these holds, otherwise `X&`:
1. a call site anywhere passes `null` **directly** (the argument is a literal, a cast `(X) null` or a conditional with a `null` branch) to a
   method of that name and arity at that position (constructors: `new X(...)`, `super(...)`, `this(...)`); a `null` inside a nested call or
   instantiation of the argument (`entries.add(new Entry(x ? c : null))`) belongs to that inner call, not to `add`;
2. a body of the **method family** (the same name and arity in the topmost supertypes declaring it and all their subtypes, handlers and
   anonymous classes included, so every override keeps one signature) compares the parameter with `null` (`p == null`, `Objects.isNull(p)`);
3. a one-statement setter of the family stores it into a field.

Never nullable: the owner a part stores (`OwnerRef`, late-bound `Final<O*>`). A part handed to a constructor or setter is `std::unique_ptr<X>`
(§10.2). `skeleton.py` applies exactly these rules (§13), so the draft is the starting point. Review the parameters that receive fields or
getter results Java may hold as null without a literal (`onTargetChanged(oldTarget, newTarget)` gets the previous target, null at first): make
them `Ptr<X>` and say why in a comment. Overloads that differ only in related object types at one position (`isEnemyFrom(Creature)`,
`isEnemyFrom(Player)`) must use the same kind at that position; with `X&`, C++ picks the most derived overload like Java, with `Ptr` a `T&`
argument is ambiguous.

Return values are `Ptr<X>` except in the rows above. Where Java checks a part accessor for null, the accessor returns `Ptr<X>` instead
(`Creature::getGameStats`/`getLifeStats`/`getEffectController`/`getMoveController`, `Player::getMailbox`, `Npc::getSkillList`,
`Item::getConditioningInfo`), and so do its narrowing redeclarations. The same evidence applies to non-object kinds, which `skeleton.py` does
not check: enums, boxed numbers and Timestamps that receive `null` become `std::optional` (§6), `String` parameters whose `null` changes
behaviour or reaches SQL become `std::optional<std::string_view>` (`removeLegionMember` kickerName, `OldNamesDAO::isNameReserved`). A Java
`null` String that is only written with `writeS` (`SM_MESSAGE` senderName) may be passed as `""`, with a comment on the declaration.

### 5.2 Call-site syntax (for the body porters, fixed by these signatures)

```cpp
Ptr<Npc> npc = instance->getNpc(701237);          // Java: Npc npc = instance.getNpc(701237);
if (npc && !npc->isDead())                          // null check and dereference of a Ptr
	AIActions::targetCreature(*this, *npc);        // X& parameters take *ptr (NullPointerException if null) and *this
creature.getController().onAttack(*this, 0, true);  // part accessors return references
```

## 6. Strings, numbers, external types

| Java | C++ |
|---|---|
| `String` parameter / return / field | `std::string_view` / `std::string` / `Field<std::string>` or `const std::string` (fieldmap) |
| `String` that Java distinguishes from `null` (null checks, `null` passed) | `std::optional<std::string_view>` parameter, `std::optional<std::string>` return; fields per fieldmap |
| boxed `Integer`, `Long`, ... (nullable) | `std::optional<int32_t>` etc. in parameters, returns and `Field<std::optional<T>>`; plain `T` as container elements |
| `char` / `byte` / `short` / `long` | `char16_t` / `int8_t` / `int16_t` / `int64_t` |
| `byte[]` parameter / return | `std::span<const uint8_t>` / `std::vector<uint8_t>` |
| `T[]` parameter / return | `std::span<const T'>` / `std::vector<T'>` (`T'` = element kind of §7.1). A `std::span<const int32_t>` cannot bind a braced list in C++23: call sites write `std::array{...}` (`defaultOnKillEvent`, `sendQuestEndDialog`) |
| array Java stores and returns as the same object (`Player.captchaImage`, `battleReturnCoords`, `byte[]` members with accessors) | `Ptr<runtime::Array<T>>` parameters and returns, keeping null and identity |
| `java.util.BitSet` field | `runtime::HashSet<int32_t>` of the set bit indexes (no BitSet shim; `DespawnableNode.instances`) |
| `Optional<X>` return | `Ptr<X>` for objects, `std::optional<T>` for values |
| `java.sql.Timestamp`, `java.util.Date`, `java.time.Instant` | `commons::database::Timestamp` (`std::chrono::sys_time<std::chrono::milliseconds>`, `SqlTypes.h`); nullable: `std::optional<Timestamp>` |
| nullable `Timestamp`/`Date` **field** (Java stores or compares `null`: `QuestState.completeTime`, `House.acquiredTime`) | `runtime::Field<std::optional<commons::database::Timestamp>>`; parameters and returns `std::optional<Timestamp>` |
| enum **field** that Java sets to `null` or compares with `null` (`Npc.overriddenType`, `Player.panesterraFaction`, `SpawnGroup.handlerType`) | `runtime::Field<std::optional<E>>`; a parameter that receives `null` (§5.1 evidence) and a return that may be `null` are `std::optional<E>` (`CreatureLifeStats::reduceHp(std::optional<SM_ATTACK_STATUS_TYPE> type, ...)`); never-null enums stay plain `E` |
| `java.time.LocalDate` | `commons::database::Date` (`std::chrono::year_month_day`) |
| `java.time.LocalDateTime` / `ZonedDateTime` | `std::chrono::local_time<std::chrono::milliseconds>` / `std::chrono::sys_time<std::chrono::milliseconds>` plus the zone where Java keeps one |
| `java.time.Duration`, millisecond `long` durations | `std::chrono::milliseconds` only where Java uses `Duration`; `long` millis stay `int64_t` |
| Quartz `JobDetail` | `runtime::Ref<services::cron::JobDetail>` (`Field<Ref<JobDetail>>` in fields) |
| Quartz `CronExpression` | `services::cron::CronExpression` (immutable value; held by value or `const CronExpression*` from `CronExpressions`) |
| `java.util.regex.Pattern` | `std::regex`, or `std::wregex` where Java counts characters (names, chat) |
| `File` / `Path` | `std::filesystem::path` |
| `Random` | `utils::Rnd` (no member) |
| `Cleaner` | `runtime::CleanerQueue` (no member, §10.4) |
| `Object` parameter / return (not varargs) | `const std::any&` / `std::any` (§7.4) |
| `Class<?>` / `Class<T>` parameter | `const std::type_info&` / a template parameter |
| `Enum<E>` bound, `EnumSet<E>`, `EnumMap<E, V>` in signatures | the enum; `std::set<E>` (Java iteration order); `std::map<E, V>` |

## 7. Collections, iteration, callbacks, varargs

### 7.1 Collections

| Where | C++ |
|---|---|
| Field | the same-named shim (`ArrayList<Ref<X>>`, `ConcurrentHashMap<int32_t, Ref<X>>`) or `Field<Ref<RcArrayList<...>>>`, exactly as fieldmap prints it; never `const` |
| Parameter (read or iterated by the callee) | `const std::vector<Ptr<X>>&`, `const std::unordered_map<K, Ptr<V>>&`, `const std::unordered_set<Ptr<X>>&`; value elements plain |
| Parameter the callee stores (Java keeps the list) | `std::vector<Ref<X>>` by value (moved in) |
| Parameter the callee fills on behalf of the caller or of a shared field | the shim by non-const reference (`runtime::RcHashSet<Ref<DropItem>>& droppedItems`, `runtime::RcTreeSet<Ref<ManaStone>>&`), or `std::vector<Ptr<X>>&` for a caller-local list |
| Return of a newly built collection (Java `new ArrayList<>(...)`, `stream().collect(...)`) | `std::vector<Ptr<X>>` (borrowed elements, like `snapshot()`); `std::vector<Ref<X>>` when the list holds the only references (removed or newly created objects: `Legion::addHistory`, `Skill`'s effect lists) |
| Return of a collection field (Java returns the live collection) | a reference to the shim: `runtime::ArrayList<runtime::Ref<X>>& getHouses()`; for `Field<Ref<RcX>>` fields `runtime::Ptr<runtime::RcArrayList<...>>`; a live field Java returns or `null` (other race): a shim pointer, nullptr for null (`BrokerService::getRaceBrokerItems`) |
| Collection Java stores directly into a (volatile) field | `Ref<RcX>` returns and shim-reference parameters (`EventService::collectActiveEvents`) |
| `Collections.unmodifiableList(field)` | `std::vector<Ptr<X>>` snapshot |
| `TreeMap`/`TreeSet` with value keys | `std::map`/`std::set` |
| a `LinkedHashMap` result ordered by value (`getPlayerDamageCounter`) | `std::vector<std::pair<K, V>>` |
| server packets built for one call (`RiftInformer.getPackets`) | `std::vector<std::unique_ptr<network::aion::AionServerPacket>>`, taken by `const&` by the functions that send them |

### 7.2 Iteration

| Java | C++ |
|---|---|
| `Iterator<X> iterator()` of an `Iterable<X>` class | `runtime::JavaIterator<runtime::Ptr<X>> iterator();` plus `runtime::SnapshotIterator<runtime::Ptr<X>> begin();` and `std::default_sentinel_t end() const noexcept { return {}; }` so range-for works |
| `Stream<X> stream()` and friends | `std::vector<Ptr<X>>` under the Java name; call sites use `std::ranges`/`std::views` on it |
| `forEach(Consumer<? super X>)` | `void forEach(const std::function<void(X&)>& action)` |

### 7.3 Callbacks

| Java | C++ |
|---|---|
| `Predicate/Consumer/Function/Supplier/Bi*` parameter, invoked during the call | `const std::function<R(Args)>&`; object arguments `X&` (elements are never null), object results `Ptr<X>` |
| same, **stored** or scheduled by the callee (field, observer list, task) | `runtime::PinnedCallback<R(Args)>` by value (capture rules of runtime-architecture.md §7.3) |
| `Runnable` / `Callable<T>` invoked synchronously / stored or scheduled | `const std::function<void()>&` / `runtime::PinnedCallback<void()>` |
| functional-interface field or constant | as fieldmap prints it (`static const PinnedCallback<bool(Persistable&)> NEW;`, defined in the `.cpp`) |
| anonymous or local class stored in a field or collection | the struct `fieldmap.py --class '<key>'` prints (`IdianStone_ActionObserver`), **defined in the `.cpp`**; the member keeps the Java static type (`Field<Ref<ActionObserver>>`) |
| lambda returned by a factory (`newPredicate`) | a local `TaskStruct` in the `.cpp` (see `Persistable.cpp`) |
| static `Comparator` constant (`BrokerItem.NAME_SORT_ASC`) | `static const std::function<int32_t(Ptr<X>, Ptr<X>)>`, defined in the `.cpp` |
| static `Function` constant of a packet (`DYNAMIC_BODY_PART_SIZE_CALCULATOR`) | `static const runtime::PinnedCallback<...>` defined in the `.cpp` from a captureless lambda or `TaskStruct`; its body never reaches `AION_UNPORTED` at static initialization |
| named `Runnable` class handed to `schedule*`/a stored-callback API (`new X(...)`, or `this` inside X) | never K5: with object members it is K4, RefCounted with a protected constructor, `create()` and `AION_MAKE_REF_FRIEND` (`LifeStatsRestoreService::HpRestoreTask`, `PetController::PetUpdateTask`); with only immutable value members it may stay a K3 `TaskStruct` (`GeneralUpdateTask`). Copy the `fieldmap.py --class` block, which also lists C++-only members |

### 7.4 Varargs and `Object`

| Java | C++ |
|---|---|
| `int... ids`, `String... names`, `X... objects` | `std::initializer_list<T'>` (`T'` per §7.1 element kinds: `Ptr<X>` for objects), declared `= {}` (Java calls it without varargs) unless an overload with one parameter less exists (the call would be ambiguous; overrides repeat the default); varargs the method only passes on as an array become `std::span<const T'>` (`AbstractAI::handleCustomEvent`, `AbstractQuestHandler::onCanAct` with `std::span<const std::any>`, `ChatCommand::execute` with `std::span<const std::string>`: `ChatProcessor` passes a runtime array, which `std::initializer_list` cannot hold) |
| `AionServerPacket... packets` | `std::initializer_list<std::reference_wrapper<network::aion::AionServerPacket>>` (call sites name the packets: `SM_A a(...); team.sendPackets({a, b});`) |
| `Object... params` that are only formatted into a message (`sendMonologue`, `broadcastMessage`, `SM_SYSTEM_MESSAGE`, `SM_QUESTION_WINDOW`, `SM_CLOSE_QUESTION_WINDOW`) | a variadic template `auto&&... params` converting each argument with `toJavaString` into `std::vector<std::string>`, forwarding to a non-template overload. `toJavaString` must resolve at the call site: `PacketSendUtility` uses a defaulted template parameter `SysMsg = SM_SYSTEM_MESSAGE` (callers that pass parameters include `SM_SYSTEM_MESSAGE.h`); the question-window packets declare their own `int32_t`/`int64_t` overloads instead of including it |
| `String... args` that are only formatted (`ChatCommand::sendInfo`) | a non-template `std::span<const std::string>` overload plus a variadic forwarding template constrained to `std::string_view`-convertible arguments, so `sendInfo(player, "...")` and `sendInfo(player)` keep their syntax |
| `Object... args` carrying objects (`onCustomEvent`, `notifyObservers`, `onCanAct`, item actions) | `std::initializer_list<std::any>`; `Object[]` → `std::span<const std::any>`. Objects are stored as `runtime::Ref<C>` of the class the receiver casts to (the API comment names it), numbers as the C++ primitive, strings as `std::string`, templates as `const T*`; receivers read `std::any_cast<runtime::Ref<Npc>>(args.begin()[0])` |

## 8. Generics

### 8.1 Erasure rule

A Java generic class whose type parameters **all** have a bound naming a project type is one **non-template** C++ class, erased like javac
erases it: every type variable is spelled as its first bound, and wildcard or raw uses (`VisibleObjectController<? extends VisibleObject>`,
`GeneralTeam<?, ?>`, `Siege<?>`) are the plain class name. Examples: `AbstractAI`, `VisibleObjectController`, `CreatureController`,
`CreatureMoveController`, `CreatureGameStats`, `CreatureLifeStats`, `GeneralTeam`, `TemporaryPlayerTeam`, `InstanceScore`, `SkillList`,
`HouseObject`, `SummonedObject`, `Siege`. `TeamMember<M>` (unbounded) is erased too, with `M` spelled `AionObject` (the bound its users give).

Stay templates: unbounded utility generics (`SplitList<Type>`, `AbstractFIFOPeriodicTaskManager<T>`) and `AITemplate<T>` (with
`using OwnerType = T;`, HandlerRegistry.h) plus the `AIEngine.DummyAI<T>` derived from it. `skeleton.py` (`erased_generic`, `ERASURE_BOUNDS`,
`TEMPLATE_GENERICS`) and the committed `fwd.h` files implement exactly this list.

### 8.2 Narrowing accessors (Java covariant overrides and bound type variables)

- A subclass that binds a type variable (`NpcController extends CreatureController<Npc>`) **redeclares** each accessor whose Java return type is
  that variable, non-virtual, with the narrower type: `model::gameobjects::Npc& getOwner() const;` defined in the `.cpp` as
  `return static_cast<model::gameobjects::Npc&>(CreatureController::getOwner());`.
- A Java override whose body is only `return (X) super.m(...);` (`Npc.getController()`, `Player.getGameStats()`, `SiegeNpc.getSpawn()`) is the
  same thing: the base method is **not** virtual on its account, and the subclass redeclares `m` non-virtually with the narrower type
  (`const templates::npc::NpcTemplate* getObjectTemplate() const;`, `controllers::NpcController& getController() const;`). The cast is ported.
- A real covariant override (the body does more than cast, e.g. `Summon.getMaster()`) keeps the base's C++ return type and `override`; callers
  needing the narrower type cast (`cast<Player>(summon.getMaster())`). Mention the Java return type in a comment.
- Parameters of type-variable type are the bound in every override (javac's bridge methods); bodies cast where Java relied on the generic type.

### 8.3 Other generic constructs

| Java | C++ |
|---|---|
| generic method `<T> T get(Class<T>)`, `<T extends X> void f(T t)` | member function template in the header, inline body `{ AION_UNPORTED(); }` (the header then includes `runtime/base/Unported.h`); `Class<T>` parameters disappear into the template argument |
| class template members (AITemplate) | defined inline in the header, `AION_UNPORTED();` bodies |
| `? extends X` / `? super X` in parameters | `X` |
| `?` alone in a parameter | the bound of the declaring type variable |

## 9. Classes, interfaces, nested types, virtual

### 9.1 Declarations

- Java `final class` → `final`. Class name, method names and parameter names stay Java (keyword rule: `delete_()`, `register_()`).
- **Declare every Java method, private ones included**, in Java order (a later private helper would otherwise be a header request).
  C++-only members (`create`, `postConstruct`, narrowing accessors) go next to the Java member they belong to.
- Overloads stay overloads; never fold them into default arguments. If two Java overloads map to one C++ signature, file a header request (none
  in the hubs). Overloads that differ only in integer width or `bool` (`SM_UPGRADE_ARCADE(int32_t, int64_t)`/`(int32_t, bool)`) need exactly
  typed arguments at call sites, as the Java callers' casts already provide.
- **Name hiding.** A C++ override, or a redeclaration, of one overload hides the base's other overloads of that name. The subclass then
  declares `using Base::name;` (`Summon`/`Player` `isEnemyFrom`, `Storage` for the `IStorage` names, `NpcController`/`PlayerController`
  `onAttack`/`useSkill`, `House` `getSpawn`, `SummonGameStats::getStat`, WorldMapInstance subclasses for `getRegion`, effect subclasses for
  `EffectTemplate::calculate`). Every overload a using-declaration names must be accessible to the subclass (MSVC C2876), so a Java-private
  overload of such a name is `protected` (the 10-argument `CreatureController::onAttack`).
- **Java abstract classes** have `protected` constructors (and destructors for controllers), also where Java's constructor is public
  (`Storage`, `CreatureLifeStats`, `NpcAI`, `CreatureMoveController`).
- **Static data classes** (K1 shells) declare their hand-written methods `const`, virtual ones included, because templates are referenced as
  `const X*`. xmlgen emits a generated trivial getter `virtual` when a Java subclass declares the same name and arity
  (`EffectTemplate::getValue`, `getDuration2`, `isNoResist`); the overriding shell declares `T name() const override;`. An abstract method of an
  xmlgen behaviour root becomes `= 0` only once every shell the binders instantiate declares the override (`EffectTemplate::applyEffect`,
  110 effect shells).
- `virtual` exactly when the method is abstract, declared by an interface, or overridden in `src/` or `data/handlers` by something other than a
  cast-only override (§8.2). Overrides say `override` (Java `final` on an override: `override final`); abstract methods are `= 0`; Java
  `final` methods that override nothing are plain non-virtual functions.
- `const` member functions: `equals`, `hashCode`, `compareTo` (the shims require it), inline trivial getters, and `toString() const` only on
  value-like classes (K3/K5 without a runtime base, static data). `toString()` of RefCounted, OwnedPart and Immortal classes is non-const
  (their bodies call virtual getters). Every other method of a runtime-based class is non-const; its virtual methods are never `const` except
  `equals`/`hashCode`/`compareTo`. Static data classes and interfaces implemented mostly by static data (`L10n`: `getL10nId() const`) are the
  exception (above). What such a `const` member calls on `this` or on a parameter is `const` too, virtual families included, so the Java body
  can be ported without `const_cast`: `IStatFunction::getPriority() const` and `isBonus() const` for the default `compareTo`,
  `ItemStone::getItemTemplate() const` for `getL10nId() const` (freeze review).
- `equals(Object)` → `bool equals(const X& obj) const` (X = the class declaring the Java override, e.g. `AionObject`); `hashCode()` →
  `int32_t hashCode() const`; `compareTo(X)` → `int32_t compareTo(const X& other) const`. Only where Java overrides them (fieldmap `hasEquals`).
- `clone()`: value classes get the copy constructor; RefCounted classes `runtime::Ref<X> clone()`. `finalize()` is dropped.
- Destructors: RefCounted classes `protected: ~X() override;`; parts (OwnedPart) `public: ~X() override;` (the owner's `unique_ptr`/`PartSlot`
  deletes them); both out of line. Interfaces `virtual ~X() = default;` public.
- `throws` clauses disappear.

### 9.2 Interfaces

An interface is an abstract class without data members and without a runtime base (`Persistable.h`): pure virtual methods, default methods as
virtual functions with bodies in the `.cpp`, static methods as `static`, constants as `static constexpr` (or `static const` defined in the
`.cpp`), protected defaulted constructor and copy operations, public virtual destructor. Implementors list it after the class base
(`class Item : public AionObject, public Persistable`); an interface inherited twice is listed once.

**Interfaces held by `Ref<I>`** (a fieldmap member names `Ref<I>`, e.g. `StatOwner`, `IStatFunction`; `HandlerRegistry.h` returns
`Ref<InstanceHandler>` and `Ref<ZoneHandler>`; erased `GeneralTeam` members are `Ref<TeamMember>`) declare the reference count operations, and
the first class of each implementing hierarchy that has a runtime base forwards them:

```cpp
class InstanceHandler {
public:
	/** C++ only: Ref<InstanceHandler> retains the implementing object (hub-headers.md §9.2). */
	virtual void retain() const noexcept = 0;
	virtual void release() const noexcept = 0;
	...
};
class GeneralInstanceHandler : public runtime::RefCounted, public InstanceHandler {
public:
	void retain() const noexcept override { runtime::RefCounted::retain(); }
	void release() const noexcept override { runtime::RefCounted::release(); }
```
`skeleton.py` (`retainable_interfaces`, `RETAINABLE_INTERFACES`) emits both sides. `cast<>`/`as<>` from `Ptr<I>` use `dynamic_cast`.

- **Implementors without a count.** Immortal and static data implementors of a `Ref`-held interface define `retain`/`release` as no-ops:
  quest handlers, commands (`admincommands.Speed` as a `StatOwner`), `InstanceScaler`, and the static data shells `TitleTemplate` and
  `ItemSetTemplate` (`StatOwner`). Static data shells list the Java interfaces they implement as bases (`L10n` on `VisibleObjectTemplate` and
  11 other templates; Java enums implementing `L10n` cannot).
- **Mixed lifetimes in one hierarchy.** When subclasses of one Java base need different runtime bases (`ItemStone`: `GodStone`/`ManaStone`
  RefCounted, `IdianStone` a part of Item), the base gets no runtime base (`fieldmap.toml [bases] none`) and each subclass forwards the
  interface's `retain`/`release`. `StatFunction` implements `IStatFunction` with no-op `retain`/`release` for immortal static data modifiers;
  run-time functions are `RcStatFunction<T>::create(...)` (or a subclass deriving RefCounted that forwards), and
  `StatFunction::ofTemplate(const StatFunction*)` turns a template into the `Ptr<StatFunction>` the stat containers take. `IsStaticTemplate` is
  false for the whole family; a list mixing both kinds holds `Ref<StatFunction>` (`Item.currentModifiers`).

### 9.3 Nested, inner, anonymous and local classes

| Java | C++ |
|---|---|
| static nested class used by a member, a signature or a subclass/handler | defined inside the outer class (after it if it derives from the outer class) |
| static nested class named in signatures of **other** hubs, whose outer header they may not include | hoisted to a lean `Outer_Inner.h` as class `Outer_Inner` (like nested enums), `using Inner = Outer_Inner;` in the outer class, `// fieldmap-class: <FQN>` on the hoisted class and `fieldmap.toml` spellings for its members (`skillengine/model/Effect_ForceType.h`) |
| static nested class used only by bodies (also when its outer header needs types bodies-only users cannot include, e.g. a missing template base) | `class Inner;` inside the outer class, defined in the `.cpp` (`BrokerPeriodicTaskManager`, `LifeStatsRestoreService` tasks) |
| inner (non-static) class | as above, with an explicit owner member (`OwnerRef<Outer>` or what fieldmap prints for `this$0`); a task class defined in the `.cpp` may instead take the enclosing instance as a constructor parameter or pointer member (`NpcShoutsService::NpcShoutTask`, `SurveyService::TaskUpdate`), or copy the outer values it reads (`HouseBids::Bid`, `fieldmap.toml [captures] drop = true`) |
| nested enum | `using Inner = Outer_Inner;` of the generated header, never a definition |
| anonymous / local class | the fieldmap callback struct, defined in the `.cpp` (§7.3) |
| private and protected members of nested classes | public (Java lets the whole top-level class use them; an enclosing C++ class cannot reach a nested class's protected members, `BindPointTeleportService::Cooldown`) |

## 10. Construction, parts, owners

### 10.1 `VisibleObject::create<T>` and `postConstruct`

Every class derived from `VisibleObject` is constructed only by `VisibleObject::create<T>(args...)` (handlers-and-porting-plan.md §1.7):

```cpp
// VisibleObject.h (reference)
protected:
	struct CreateKey { private: CreateKey() = default; friend class VisibleObject; };
	virtual void postConstruct() {}
public:
	template <std::derived_from<VisibleObject> T, class... Args>
	[[nodiscard]] static runtime::Ref<T> create(Args&&... args) {
		runtime::Ref<T> object = runtime::makeRef<T>(CreateKey(), std::forward<Args>(args)...);
		static_cast<VisibleObject&>(*object).postConstruct();
		return object;
	}

// Npc.h (Java: public Npc(NpcController controller, SpawnTemplate spawnTemplate, NpcTemplate objectTemplate))
class Npc : public Creature {
	AION_MAKE_REF_FRIEND
protected:
	Npc(CreateKey key, std::unique_ptr<controllers::NpcController> controller, templates::spawns::SpawnTemplate& spawnTemplate,
		const templates::npc::NpcTemplate* objectTemplate);                       // only stores members (Java field initializers included)
	~Npc() override;
	void postConstruct() override;                                              // Creature::postConstruct(); then the Java constructor body
public:
	controllers::NpcController& getController() const;                          // narrowing accessor (§8.2)
```

- Constructors take `CreateKey key` first, pass it to the base constructor, and only store members. No subclass declares `create`.
- `postConstruct()` overrides call the base version first, then the Java constructor-body statements that need the dynamic type, in Java order
  (`Creature`: AI creation; `Npc`: `getController().setOwner(*this)`, move controller, skill list, `setupStatContainers()`).
- Java `new Npc(...)` everywhere (handlers included) → `VisibleObject::create<Npc>(...)`.
- Other RefCounted classes (not visible objects) keep `static runtime::Ref<X> create(<constructor parameters>)` defined in the `.cpp`, with
  protected constructors and `AION_MAKE_REF_FRIEND`.
- **`this` during construction is safe.** `runtime::makeRef` sets the count to 1 before the constructor runs (Ref.h `makeRef`), so a
  constructor may take `Ref(*this)`, store `this` in a part or a sibling (`SpawnTemplate(SpawnGroup&)` added by the group's constructor), pin
  `this` for a task or call `PartSlot::set`: no temporary retain/release reaches 0 and the Reclaimer never sees the object under construction.
  What is not safe is a virtual call expecting the subclass (hence `postConstruct`, §10.1), and letting the object escape to other threads
  before `create` returns (publish in `postConstruct` or after `create`).
- **Virtual calls in Java constructors of other classes** use the same two-phase idea without `CreateKey`. `WorldMapInstance`'s constructor
  calls the abstract `initMapRegions()`: the `create()` of `WorldMap2DInstance`/`WorldMap3DInstance` calls it after construction and before
  publication. `Skill`'s constructor calls the overridable `initializeSkillMethod()`: `PenaltySkill`'s constructor calls its own
  `initializeSkillMethod()` after the base constructor (P5-02).

### 10.2 Parts

| Java | Member (fieldmap) | Accessors |
|---|---|---|
| `private final X x = new X(this)` / set in the constructor | `const std::unique_ptr<X> x;` | `X& getX() const;` (`.cpp`) |
| `setX(new X(this))` in a constructor (pattern 2) | `runtime::PartSlot<X> x{*this};` | `X& getX() const;` and `void setX(std::unique_ptr<X> x);` (`.cpp`) |
| part passed to `super(...)` (controllers, pattern 3) | `const std::unique_ptr<X> controller;` | constructor parameter `std::unique_ptr<X>`; getter `X& getController() const;` |
| map of parts | `runtime::PartMap<K, X> x{*this};` | `runtime::Ptr<X> getX(K key)` |

A part getter throws `NullPointerException` when the slot is empty (Java NPE), never returns null, unless Java checks it for null (§5.1:
then it returns `Ptr<X>`).

`RetireTo`: `RECLAIMER` for parts replaced repeatedly while the owner lives, which tasks may still borrow (`Player.store` PrivateStore,
`Item.idianStone`, the per-call `Player.legionStorageProxy`, storages); `OWNER` (default) for parts set about once (`Creature.ai`, the friend
list). A class that is also held by `Ref` elsewhere (an `ActionObserver` such as `ChargeInfo`, a RefCounted team such as
`PlayerAllianceGroup`) cannot be a part: `fieldmap.py` drops part types below a shared K3/K4 superclass, and the holder stores a `Ref`.

### 10.3 Owners

- A part's owner is `OwnerRef<O> owner;` (bound in the part constructor: `explicit X(O& owner)`) or, for late-bound controllers,
  `runtime::Final<O*> owner;` with `void setOwner(O& owner)` (`.cpp`: `owner.set(&value); bindOwner(value);`) and an inline
  `O& getOwner() const { return *owner.get(); }`. Erased generics narrow `getOwner()` in each binding subclass (§8.2).
- `SelfOrRef<O>` where fieldmap says so, including `VisibleObject::target` (`fieldmap.toml` override, RT-4).
- `OwnerRef<O>` is allowed only in `OwnedPart` classes (lint L3). A non-retaining back reference in any other class needs a reviewed
  `fieldmap.toml [fields]` override that carries the lifetime argument (`ChargeInfo.item`: the charge info is reachable only from its item and,
  while equipped, from the ObserveController, which unequip and `LogoutBreakers` L7 clear; `UseableItemObject.UseDataWriter.obj`). A
  `// fieldmap:` comment does not waive it. An override with `holders = ["FQN.field", ...]` and `accessor = "name"` makes the argument checked
  (`ChargeInfo.item`: holder `Item.conditioningInfo`, accessor `getItem()`): lint L3 rejects any other member and any stored-lambda capture or
  pin naming the class, and reads of the member outside the constructors and the accessor, whose body terminates in checked builds when the
  referenced object is no longer managed (`AION_CHECK("C4", item.isManaged(), ...)`). Neither finding is waivable.
- An owner Java reads as `null` before `setOwner` is `Field<O*>`, not `Final<O*>` (a checked `Final` aborts on a read before set):
  `TitleList.owner`.

### 10.4 Destructors and ids

Destructors are `noexcept`, release-only (L9). `~AionObject` pushes an auto-release id to `runtime::CleanerQueue` (the Java `Cleaner`); the
drain runs `RespawnService.setAutoReleaseId`/`IDFactory.releaseId` later on the instant pool.

## 11. Statics, singletons, loggers, locks

### 11.1 Statics

| Java | C++ |
|---|---|
| `static final` primitive/String with a literal (or literal arithmetic) initializer | `static constexpr int32_t X = 5;` / `static inline const std::string X = "...";`, or `static constexpr std::string_view X` where `fieldmap.py` prints that (`SpawnTemplate::NO_AI`) |
| `static final` object with a non-literal initializer | `static const T X;` in the class, defined in the `.cpp` (the initializer must not reach `AION_UNPORTED`; port what it calls) |
| `static final` RefCounted object | `static const runtime::Ref<X>& NAME;` bound in the `.cpp` to `*new runtime::Ref<X>(X::create(...))`: created at static initialization, never released, so no Reclaimer work runs during static destruction (`ItemService::DEFAULT_UPDATE_PREDICATE`, `PanesterraTeam` positions) |
| `static final` constant of the class's own type (interned) | `static const X* const NAME;` defined in the `.cpp` by a lambda that opens a STARTUP `TaskScope` (§2, `ZoneName::NONE`) |
| `static final` primitive array with literal contents | `static constexpr std::array` (`AbyssService.killAnnounceMaps`, `SM_HOUSE_SCRIPTS.SCRIPT_PADDING`, `SM_L2AUTH_LOGIN_CHECK` tables), recorded in `fieldmap.toml` |
| static collection filled only by the static initializer | `static const` std container in Java iteration order (`Motion.motionType` is a `std::map`), so no Monitor is taken during static initialization |
| `static`/`volatile` collection initialized with `Collections.emptyMap()`/`emptySet()`/`emptyList()` | a null `Field<Ref<...>>{}`; the port creates the empty collection where Java would read it (no RefCounted objects at static initialization) |
| `static final` whose initializer reads configs or static data (`WorldConfig.WORLD_REGION_SIZE`, `DataManager.X_DATA`, `GameServer.versionInfo`) | no C++ static initializer (it would run before `Config::load`): a static function that caches the value on first use (`WorldMapInstance::regionSize()`), a `Field` filled lazily (`PetFeedCalculator` arrays), or no declaration and the body reads the source directly (`TradeService.tradeListData`, `PlayerEnterWorldService.VERSION_INFO`) |
| mutable `static` | `static inline runtime::Field<T>` / static shim (fieldmap) |
| static-only utility class (`PacketSendUtility`) | class with static member functions; Java's private constructor becomes `X() = delete;` |

### 11.2 Singletons

`SingletonHolder`/`getInstance()` classes derive `runtime::Immortal` (fieldmap `base`), declare a private constructor and
`static X& getInstance();` defined in the `.cpp` as `static X instance; return instance;` (Java `SingletonHolder`). Per-run service objects in
`fieldmap.toml [settings] per_run_services` stay RefCounted. Engines keep Java's instance members (`QuestEngine`, `SkillEngine`, `World`).
Any nested `*Holder` class counts as a singleton holder (`RiftService`, `VortexService`, `AutoGroupService`). An empty Java constructor is
`= default`, one that only logs is ported, any other keeps its member initializer list and then `AION_UNPORTED();` (the singleton then throws
from `getInstance()` until its chunk ports it). The per-run `AhserionRaid` keeps Java's static `getInstance()`, returning the object of a
never-released `Ref`.

### 11.3 Loggers

- `private static final Logger log` → `static const auto log = commons::logging::LoggerFactory::getLogger("<Java FQN or logger name>");` at
  namespace scope in the `.cpp`, never in a header.
- `protected`/package loggers used by subclasses (`GeneralInstanceHandler.log` "INSTANCE_LOG", `ConsoleCommand.log` "ADMINAUDIT_LOG") →
  `static const commons::logging::Logger log;` in the class (with `namespace aion::commons::logging { class Logger; }` instead of `Logger.h`),
  defined in the `.cpp`.
- A logger created inline in a Java body (`LoggerFactory.getLogger(X.class).warn(...)`) is the `.cpp` logger.

### 11.4 Locks

- `synchronized` method: the declaration keeps a `// synchronized` comment; the ported body is `SYNCHRONIZED(*this) { ... }`.
- `ReentrantLock`, `final Object lock = new Object()` → `runtime::Monitor` members (`mutable` when locked from const members); `StampedLock`,
  `Semaphore` → the same-named runtime shims (fieldmap).

## 12. Packets and connections

- Server packets are stack temporaries (K2): object members `Ref<X>`, constructors with the §5 parameter kinds, `writeImpl(AionConnection* con)`
  per runtime-architecture.md §8.2.
- Send APIs take the packet by reference in a non-template function and add a forwarding template for temporaries:
  ```cpp
  static void sendPacket(model::gameobjects::player::Player& player, network::aion::AionServerPacket& packet);
  template <std::derived_from<network::aion::AionServerPacket> P>
  static void sendPacket(model::gameobjects::player::Player& player, P&& packet) { sendPacket(player, static_cast<network::aion::AionServerPacket&>(packet)); }
  ```
  (a forward declaration of `AionServerPacket` suffices: the constraint and the conversion are checked at the call site).
- APIs that may send later (a `delay`, `scheduleOrRun`) cannot borrow the packet: they take `std::shared_ptr<AionServerPacket>`, their
  forwarding template copies or moves the argument with `std::make_shared<std::remove_cvref_t<P>>`, and their filters are
  `runtime::PinnedCallback<bool(Player&)>` (`PacketSendUtility`).
- Connections: `std::shared_ptr<AionConnection>` returns and `Field<std::shared_ptr<AionConnection>>` members, `AionConnection*` parameters.

Packet declarations as S0c wrote them (eager model, runtime-architecture.md §8.2):
- `AionServerPacket` has `SerializedBody serialize(AionConnection* con)` instead of Java's `write(con, buffer)`, static write helpers on a
  thread-local buffer (`writeD(v)` keeps Java's call syntax; `getBuf()` is public static for `ItemInfoBlob`), and `writeImpl` keeps Java's empty
  default body. There is no counterpart of Java's no-argument constructor that looks the opcode up by `getClass()`: every packet constructor
  passes `opcodeOf<SM_X>` to its base.
- Abstract packet bases and packets with Java subclasses (`AbstractPlayerInfoPacket`, `AbstractHouseInfoPacket`, `SM_LEGION_INFO`,
  `SM_LEGION_MEMBERLIST`) declare a C++-only protected constructor with a leading `int32_t opCode`; their public constructors delegate with
  `opcodeOf<Self>`. A Java class without a constructor gets an explicit public default constructor (`// Java: implicit default constructor`).
- A packet with `Ref`, `unique_ptr`, `shared_ptr` or collection-of-`Ref` members declares `~SM_X() override;`, defined `= default` in the
  `.cpp`, so a temporary needs no complete member types at the call site; scalar-only packets keep the implicit destructor.
- A cached constant `static final SM_X C = new SM_X(...)` is `static const std::shared_ptr<SM_X> C;` defined in the `.cpp` (a `static inline`
  initializer cannot construct the incomplete class) and sent as `*SM_X::C` (`SM_FRIEND_RESPONSE`). Shared static packet objects are safe only
  while `serialize`/`writeImpl` stay read-only (`SM_MACRO_RESULT`, with an L4 waiver).
- Packets whose `writeImpl` reads the connection, directly or through a helper or base (`SM_CREATE_CHARACTER` via `writePlayerInfo`), override
  `Recipients recipients() const noexcept override { return Recipients::PER_RECIPIENT; }` by hand. Lint L10 checks the overrides against the
  `PER_RECIPIENT` list in `lint_concurrency.py`; the planned `ServerPacketTraits.gen.h` is not produced. `SM_KEY` does not override it: P4-15
  serializes it with its connection in `AionConnection::initialized`.
- `SM_CUSTOM_PACKET` re-exposes `AionServerPacket`'s protected static write helpers with public using-declarations, so the generated
  `PacketElementType` companion can call them.

## 13. Draft markers and the drafts after the pattern stage

`skeleton.py` now applies (tools.gen goldens updated; every `@hubs` draft with its dependencies compiles at `/W4 /WX`): the erasure rule
(§8.1, including stripped type arguments in fieldmap member types and erased parameter types in overrides), nullable-or-reference parameters
(§5.1), `std::unique_ptr<X>` part parameters and `X&` part/owner getters with ported bodies (§10.2, §10.3), references to collection-field
shims (§7.1), `std::vector<Ptr<X>>` collections in signatures, `std::initializer_list` varargs and `std::any` for `Object` (§7.4), `X&`
callback arguments (§7.3), lock classes on lockable members (§4), `= {}` on varargs (§7.4), direct-argument null evidence (§5.1), override
detection by erased parameter types (a private same-named helper is not an override), `const X*` for interned immortals, non-virtual bases and ported narrowing redeclarations for cast-only overrides
(§8.2), non-const `toString()` on runtime-based classes, `CreateKey` constructors without `create` for visible objects (§10.1) and
`retain`/`release` for interfaces held by `Ref` (§9.2). What remains by hand:

| Marker | Resolution |
|---|---|
| `TODO(fieldmap)` | the field has no usable fieldmap entry (raw generic, array bound expression, static initializer block): write the member from `fieldmap.py --class` by §4; arrays of parts (`Storage[] petBags`) → `std::array<std::unique_ptr<X>, N>` with `N` from the generated enum companion or a `static constexpr` |
| `TODO(signature)` | apply §5-§8: `Object`/varargs §7.4, `Stream`/`Iterator` §7.2, wildcards and generic methods §8, covariant returns §8.2, `Date` etc. §6, stored functional interfaces §7.3, packets in containers §7.1, `clone()` §9.1; the type nested in an enum (`LegionHistoryAction.Type`) is `LegionHistoryAction_Type` |
| `TODO(callbacks)` | the anonymous classes and stored lambdas of the class: nothing in the header unless a member stores one (§7.3); keep the key in a comment above the stub that creates it |
| `TODO(enum)` | never define an enum: generated header + `using` alias; constructor data and methods go into the enum's companion header (owned by the enum's chunk) |
| `TODO(logger)` | §11.3 |
| `TODO(xmlgen)` | the class is an xmlgen behaviour shell: extend the shell in place (keep the `X.xml.h` include, `#include "X.xml.inc"` as the first line of the body and the `StaticTemplate` base) |
| `unportedArgument<T>()` in constructor stubs | replace with the real member initializers (§2) |
| "trivial accessor ...: inline once the member exists" | write the accessor (§3.3) |
| "@Override: the hand-written C++ base does not declare it (yet)" | the base hub must declare the method; coordinate with its group or add `override` once it lands |

## 14. Header requests after the freeze

- File an entry in `docs/porting/header-requests.md` (the integrator creates the file with the first request) or send a message to the
  integrator: requesting chunk, header, the exact declaration change,
  the Java evidence (file:line) and whether it is **additive** (a new C++-only helper, a missing overload, a new narrowing accessor) or a
  **layout/signature** change (member type, parameter kind, virtual-ness, access).
- Additive declarations with an `AION_UNPORTED` stub are batched by the integrator once a day without review. Layout and signature changes need
  the reviewer, because every dependent chunk recompiles and may need edits.
- Until a request lands, a chunk does not work around a wrong signature with casts or duplicate helpers in its own files; it ports the body
  against the requested signature on its branch and marks the call with `// header-request: <entry>`.
- Bodies never need a request: ported code replaces `AION_UNPORTED();` in the owning chunk's `.cpp`. Neither do new files a chunk owns
  (a new declaration header, a hand-written enum companion, the `.cpp` of a header-only shell), as long as no frozen header changes.
- A request that changes a member of a K3/K4 class also updates `fieldmap.toml` (and `cycles.toml` for a new retaining edge), so lint L2/L16
  stay clean; the integrator applies both with the header change.
- Known layout candidates, each a layout request if a chunk needs it: ordered containers for packet members whose `writeImpl` iterates an
  `unordered_map`/`unordered_set` (`SM_NEARBY_QUESTS`, `SM_MOTION.activeMotions`, `SM_TOWNS_LIST`, `SM_SIEGE_LOCATION_INFO`,
  `SM_RECIPE_COOLDOWN`, `SM_RECIPE_LIST`), the `AionConnection` send queue type (runtime-architecture.md §21, row 8.4) and a per-send
  `SM_MACRO_RESULT` if `serialize` ever mutates packets.

## 15. Per-group checklist

For every hub file of the group:
- [ ] Header at the mirrored path, owned by the chunk (`chunks.py owner`), `#pragma once`, class comment with the Java authors and the C++ notes.
- [ ] Includes follow §3.1; no other hub's full header except bases; the header compiles alone (header check).
- [ ] Members equal `fieldmap.py --class` (types, order, access, initializers); every deviation is a `fieldmap.toml` decision (§4); lint clean.
- [ ] Every lockable member carries its lock class `{AION_LOCK_CLASS(JavaClass::field)}` (§4).
- [ ] Every Java method declared, private ones included, in Java order; C++-only additions (`create`, `postConstruct`, narrowing accessors,
      `begin`/`end`) next to their Java counterparts.
- [ ] Parameter kinds by §5.1, returns by §5 (parts/owners/singletons `X&`, factories `Ref<X>`), strings/boxed/external types by §6.
- [ ] Collections, callbacks, varargs by §7; generics erased or templates by §8.1; covariant returns by §8.2.
- [ ] `virtual`/`override`/`final`/`= 0`/`const` by §9.1; destructors protected (RefCounted) or public (parts), out of line.
- [ ] Visible objects: `CreateKey` constructors, `postConstruct` overrides in Java order, no `create` (§10.1). Other RefCounted: `create` in the `.cpp`.
- [ ] Parts and owners by §10.2/§10.3; generated enums aliased, never defined.
- [ ] The `.cpp` defines every declared function: ported per §2, `AION_UNPORTED();` otherwise (`skeleton.py --definitions` for hub and spine
      headers); no `__has_include` guard (`skeleton.py --guards --freeze`, §3.3).
- [ ] `HandlerRegistry.h` shapes: `ai::AbstractAI`, `model::gameobjects::Creature`, `instance::handlers::InstanceHandler`,
      `world::WorldMapInstance`, `world::zone::handler::{ZoneHandler, QuestZoneHandler}`, `questEngine::handlers::AbstractQuestHandler`,
      `utils::chathandlers::{ChatCommand, AdminCommand, PlayerCommand, ConsoleCommand}`, `network::aion::{AionClientPacket, StateSet}` are
      non-template classes in exactly those namespaces; `AITemplate<T>` has `using OwnerType = T;`; instance and zone handlers have
      `static Ref<C> create(...)` of exactly their own type.
- [ ] No `TODO(...)` left in the header; open questions are in the stage result (`changeRequests`/`openIssues`), not in comments.
- [ ] Two builds with zero warnings, header check, lint, and the tests the group touched.
