# Game server design (phase 4 onwards)

The game server is ~30× the size of commons + login server combined and relies heavily on the JVM (garbage collection, free threading,
lazy packet writes, runtime-compiled handlers, JAXB). Before porting, a research pass and a design panel produced the documents below.

| Document | Content |
|---|---|
| [runtime-architecture.md](runtime-architecture.md) | **Final (free-threaded)** threading, object ownership and reclamation, field/collection wrappers, locks, scheduler/futures, packets, IDs, race safety without TSan, side-by-side Java/C++ translations, red team reviews, prototype plan |
| [conventions-game-server.md](conventions-game-server.md) | Porting rules that follow from the runtime design (proposed until the prototypes validate them) |
| [static-data.md](static-data.md) | JAXB replacement: Python generator from the Java annotations, pugixml binders, loader, verification without a JDK |
| [handlers-and-porting-plan.md](handlers-and-porting-plan.md) | Handler registration (marker macros + build-time registry scanner) and the phase 4–6 partition: spine, chunks, milestones |
| [runtime-kernel-status.md](runtime-kernel-status.md) | Status of the implemented runtime kernel (prototypes P1-P4): gates, measurements, open items |
| [wave1-status.md](wave1-status.md) | Status of wave 1 (generators, lint, oracles, XML runtime, configs, geo math, crypt, handler registry): delivered components, tests, open issues by next step |
| [spine-status.md](spine-status.md) | Status of the spine steps S0a-S0c (S0a: kernel lifecycle and fixes, core enums, shells, forward headers, preludes, chunk manifest, static data library, header check, `main.cpp` link proof): delivered components, key numbers, tool usage, open issues by next step |
| [proposals/](proposals/) | The three competing runtime proposals, the judges' scores, the superseded single-thread synthesis and the second red team round |
| [research/](research/) | Read-only research maps of the Java game server (startup, static data, object model, network, DAO/geo, handlers, services) and the critic's verified answers |

## How the runtime architecture was chosen

| Proposal | Porting fidelity | C++ safety | Hobby pragmatics |
|---|---|---|---|
| Single logic thread + non-atomic intrusive `Ref<T>` (reclaimed after each job) | **8** | 7 | **8.5** |
| Registry-owned objects + generational handles | 5 | **8** | 6 |
| Free-threaded like Java + atomic refcounts and per-field wrappers | 6 | 4.5 | 3.5 |

The lead architect combined the winner with the best ideas of the others, and a red team attacked the result. The red team found
reference cycles that Java's GC collects (alliance/leader, self-target, kisk/creator, parts referencing their own owner), missing
ownership of account data, off-thread paths (shutdown, connection setup, event reload) and a visible regression from end-of-job packet
serialization (arrival animations). Their fixes are part of the design (RT-1…RT-14).

The user then chose the free-threaded model (D1). The lead architect rebuilt the design on that proposal, addressing every judge's objection
(e.g. field wrappers derived mechanically from Java modifiers by a generator, a lint and checked-build runtime checks instead of TSan) and
carrying over the applicable RT findings. A second red team (lifetimes lens, concurrency/porting lens) found one fatal and twelve serious
issues at the edges; all are resolved in the final document.

## Decisions

| # | Decision | Status |
|---|---|---|
| D1 | Runtime architecture | **Free-threaded like Java** (user's choice, 2026-09-13; the panel had recommended the single logic thread). The final design ([runtime-architecture.md](runtime-architecture.md)) was rebuilt on [proposals/free-threaded.md](proposals/free-threaded.md) and red-teamed twice |
| D2 | Static data generator shape and tooling | **Python (stdlib) generator, generated member blocks `#include`d in hand-written classes**, generated files committed with a drift test |
| D3 | `//reload` of static data and handlers | **Deferred** until after the first enter-world milestone |
| D4 | Porting parallelism | **4–6 agents per wave** |

Defaults adopted unless the user objects (details in the documents):
- Runtime (D1): free-threaded like Java. Java-shaped pools, atomic intrusive Ref<T> with task-scoped borrows reclaimed by an epoch Reclaimer with lazy publication, Field<T> wrappers and Monitors, all derived mechanically from the Java code by tools/gen/fieldmap.py and checked by lint_concurrency.py.
- ConcurrentHashMap compute callbacks and collection comparators run under reentrant, lockdep-tracked Monitors, so Java code inside them ports unchanged; ranked leaf mutexes exist only inside the runtime.
- Server packets are serialized eagerly on the sending thread; the 25 connection-reading packets plus SM_GROUP/ALLIANCE_MEMBER_INFO per recipient, other broadcasts once; connection queues ordered by serialization sequence; SM_KEY on the IO strand; nio.threads > 1 allowed.
- The watchdog dumps stacks and a minidump on a deadlock, stall, reclamation lag or backlog high-water mark and keeps running. Exiting with RESTART is opt-in. It is disabled under a debugger.
- The day-to-day server build is the checked RelWithDebInfo build. Release keeps Reclaimer assertions, leaf lock ranks and the watchdog.
- Database calls stay inline on the calling pool thread, and the game server requires a non-zero database socket timeout.
- Movement ticks run in parallel on the ForkJoin pool; gameserver.debug.serial_movement and single_executor exist for reproduction.
- Object IDs are allocated by a monotone cursor that wraps at 2^27, with a 300 s minimum before any released ID is reused; auto-release IDs go through a Cleaner queue drained on the instant pool.
- C++-only cycle breakers (target, kisk, storage actor, observers) run in a scope guard at logout and in onDelete; a logged zombie breaker cuts known edges of objects removed from the world for more than 30 minutes.
- Fixed-rate tasks more than 10 periods (and at least 2 s) behind run once and realign.
- LS/CS link packets are executed in order per link.
- XML loading is strict in tests and warns at runtime.
- Templates are const after load; the few Java template mutations become Field<T> in an allow-list or move out of templates.
- //reload of static data and handlers is unavailable until after the first enter-world milestone (D3); //reload config stays.
- While porting, NPCs without a ported AI get a dummy AI with a warning; unported paths log AION_UNPORTED.
- Handler libraries use unity builds and precompiled headers.
- Race safety without TSan: lint in CI and pre-commit, checked builds, kernel PCT tests under ASan, nightly stress harness with fault injection; optional Linux clang TSan after M5a.
- Porting waves use 4-6 parallel agents (D4) plus the integrator lane.

Further decisions (2026-09-13):
- **D5** Deadlock detected by the watchdog: dump stacks/minidump and keep running (restart only if configured).
- **D6** Java race bugs found while porting: obvious ones are fixed right away with a DEVIATIONS.md entry and a test; unclear cases are
  marked `// java-race`.
- **D7** Zombie breaker: cut known edges of objects removed from the world for more than 30 minutes and log a warning naming the missing
  cycle breaker.
