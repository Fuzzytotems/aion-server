# Phase 6: the quest transliterator prototype (G1)

> **Status:** prototype **rev 2**, 2026-09-24, over HEAD `5fbb03a08` ("M5b-3 complete") plus the working tree. Rev 1 was measured on
> `4867fbc44`; re-run on `5fbb03a08`, its output was byte-identical. It builds the tool that
> [phase6-inventory.md](phase6-inventory.md) §7.1 recommends (G1, with G2's mirror-pair report) and measures it on all 1,035 Java quest
> handlers. **Nothing was compiled, built or run except Python**: the tool writes C++ into a scratch directory and never into the source
> tree, and no emitted file has been through a compiler. Every number below comes from one run of
> `python -m tools.gen.questgen --dry-run` (about 8 s) and is reproducible (§10). Where a number is a judgement, the text says so.
>
> **Rev 2** follows an adversarial review. The bonus reward list is now a header-signature gap, not an API row (§5.2 item 2, new). The
> varargs and `workItems` gaps become emitter rules instead of hub-header requests (§5.2). The tool now indexes `House`,
> `SpawnSearchResult` and `InstanceHandler`. It keeps the comments rev 1 dropped (238 of 2,097, §2) and marks the known Java bugs it keeps
> (§2, U3). B05 is now gated on M5d E-02. Refusal lists are stated as lower bounds, and the gains they imply as upper bounds (§4.1). The
> parallel lanes now show their real dependencies (§11). **Coverage does not change**: with comments and blank lines removed, the code of
> all 910 emitted files is identical to rev 1.
>
> Code: [`tools/gen/questgen/`](../../tools/gen/questgen/__init__.py) (package), tests
> [`tools/gen/tests/test_questgen.py`](../../tools/gen/tests/test_questgen.py) (40 tests, stdlib `unittest`, no compiler).
> Java files are cited relative to `game-server/data/handlers/quest/` or `game-server/src/com/aionemu/gameserver/`; C++ files relative to
> `cpp/game-server/`.

---

## 0. The answer

1. **The prototype transliterates 910 of the 1,035 quest handlers (87.9%) and refuses 125**, each with a reason. That is 79,666 of
   95,539 Java lines (83.4%). The inventory predicted **914-929 (88-90%)** with every tier-B rule (phase6-inventory.md §7.1, table at
   `:555-561`). The prototype lands 4 files under the low end, with **25 API-table rows** and about 20 idiom rules. On transliteration it
   clears the plan's 70% bar (`handlers-and-porting-plan.md:558`); whether it still does after a compile is the open question (§8.1).
2. **The inventory's classifier holds up.** S2's classifier put 660 files in tier A, and 657 of them transliterate. The 3 others hide a
   switch expression or an `int[]` passed to Java varargs, which the classifier's regexes missed. All 50 idiom-only files (TL)
   transliterate, as do 203 of the 261 API files (TLAPI) and none of the 64 hand files (HAND). Per shard: S1 332 of 365, S2 256 of 277, S3
   247 of 264, S4 75 of 129 (predicted 338, 267, 246 and 63-78).
3. **What blocks the other 125 is mostly not the API table.** 57 files contain closures: 30 lambdas and 27 anonymous `Runnable`s (the
   use-bar timers and the mentor dailies). 9 use Java types with no handler counterpart (`int[][]` 4, `String[]` 2, `List<Integer>`
   fields 3). 6 fill a `Set` in a static initializer and 3 use switch expressions. 2 keep per-player state in handler fields (the
   java-race of phase6-inventory.md §11), 1 throws, and 4 pass an `int[]` to Java varargs. 2 add to a reward list that the C++ hook hands
   over read-only, which is a header signature problem (§5.2 item 2). 41 call an API outside the table. **At most 36 files are refused for
   API gaps alone** (42 once the varargs rule is added), over 40 distinct members. No single gap blocks more than 6 files (§5). These
   counts are upper bounds because a refused file's reason list is a lower bound (§4.1).
4. **One planned declaration and one header signature gate the output.** `HandlerResult.fromBoolean` has no C++ declaration
   (m5d-plan H-06, `m5d-plan.md:556`). **60 transliterated files** call it through a spelling the prototype assumes, and they compile only
   once H-06 lands with that spelling (the inventory's R8). Separately, the `onBonusApplyEvent` hook takes the reward list as
   `const std::vector<const QuestItems*>&` (`AbstractQuestHandler.h:144-145`). Java handlers add to that list, so the list needs a mutable
   type with owned elements. That is a hub-header decision for the M5d owner (§5.2 item 2), and M5d E-02 meets the same problem whether or
   not handlers are generated. The varargs and `workItems` gaps need emitter rules, not header changes (§5.2).
5. **841 emitted handlers directly call nothing unported outside M5d.** They call only quest-engine bodies that are `AION_UNPORTED` today
   and that P5-06 ports in M5d (`sendQuestDialog` in 900 files, `sendQuestEndDialog` 870,
   `QuestState::isStartable` 740, ...). Only the 7 empty `steel_rake` stubs (F10) call nothing unported. The other 69 also call a crafting
   check (M5c, 44 files), a `TeleportService` delegation (M5f / R5, 25) or instance creation (M5f, 8). Bodies that those callees reach were
   not traced, so 841 is an upper bound on the files that need nothing beyond M5d (§6).
6. **Mirror pairs:** there are 186 Elyos/Asmodian pairs of equal structure. 98 are unique literal twins. The other 88 are formed in
   quest-id order inside larger equal-structure groups: they are members of one group, not twins in the inventory's sense. G1 emits 165 of
   the 186 pairs outright. **21 pairs are refused on both sides and are G2's work** (hand-port one, clone the other). The refused files form
   17 equal-structure groups of 45 files (28 clones) for G2's N-way mode (§7).
7. **Recommendation:** adopt the transliterator (U1) and move it to production in the P6-T lane. First comes the compile check this
   prototype could not run (§8.1), then the registration trace and the golden trace against the Java handlers (§8.2-8.3). Before the first
   generated batch, land H-06 with the spelling of §5.2 item 1 and settle the reward-list signature (§5.2 item 2).

---

## 1. What the prototype is

### 1.1 Package and command line

`tools/gen/questgen/` is a stdlib-only Python package (3.12), run from `cpp/`:

```
python -m tools.gen.questgen --dry-run [--emit DIR] [--only FILE...] [--json OUT] [--markdown OUT] [--no-pairs] [-v | -q]
```

| Module | What it does |
|---|---|
| `paths.py` | repository paths; puts `tools/gen` (javasrc, skeleton, dialogaction) and `tools/porting` (census) on `sys.path` |
| `jast.py` | statement and expression trees of Java method bodies over **javasrc's tokens** (`tools/gen/javasrc.py` parses declarations and keeps bodies as token spans; jast adds the statement and expression layer). Unsupported constructs raise `Unsupported(category)` |
| `cppdecl.py` | a declaration reader for the C++ headers a handler reaches: classes, bases, member functions with return and parameter types as written, `using Base::f;`, enums and their constants, the enum companion free functions, spliced member blocks (`SM_SYSTEM_MESSAGE.gen.h`), and the include closure of the quest prelude |
| `api.py` | the vocabulary: the core (tier A) set and the 25-row API table (tier B), checked against the headers in `HEADERS`; `PLANNED` spellings for members the table allows but C++ does not declare yet; the body status of each declaration from `tools/porting/census.py` |
| `emit.py` | the transliterator: one Java file in, one `.cpp` or a refusal out; `KNOWN_JAVA_BUGS` holds the Java bugs it keeps and marks |
| `mirror.py` | Elyos/Asmodian literal twins and N-way groups for G2 |
| `cli.py` | the driver and the dry-run report (text, JSON, Markdown); `--emit` refuses any directory inside the repository |

`--dry-run` transliterates in memory and prints the report of §3-§7. `--emit DIR` also writes each transliterated file to
`DIR/aion/gameserver/handlers/quest/<dir>/<Class>.cpp`, the path it would have under `cpp/game-server/handlers/`.

### 1.2 How a file goes through

1. **Parse** with javasrc (declarations) and jast (bodies). The file must have one top-level class extending `AbstractQuestHandler`. The
   constructor must be `super(<int>)` or `super(<int constant>)`. Member types and initializers are refused.
2. **Fields.** A `final` field with a literal initializer becomes a `static constexpr` member, and so does a non-final one that no method
   writes (`int` → `int32_t`, `int[]` → `std::array<int32_t, N>`, `String` → `std::string_view`, enums). A field that some method writes
   is refused as `mutable-field`: it is per-player state in the singleton handler (phase6-inventory.md §11).
3. **Methods.** A method whose name is a virtual hook of `AbstractQuestHandler.h` gets the C++ hook signature with the Java parameter names
   and `override` (`register()` → `void register_() override`). A hook that is not virtual in C++ (`rideAction`, `onProtectEndEvent`,
   `onProtectFailEvent`, `AbstractQuestHandler.h:140-149`) is refused. Other methods become member functions: `QuestEnv` and `Item`
   parameters by reference, other classes as `runtime::Ptr<T>`.
4. **Statements** keep their Java order and shape: if/else chains, switch groups with their fall-through, loops, returns. Refusals are
   collected per statement, so a refused file usually lists more than its first reason, but not always all of them (§4.1).
5. **Expressions are typed against the C++ headers.** Every call resolves to a declaring C++ class (through bases and `using`
   declarations) or to an enum companion. The Java overload is chosen with Java's applicability rules, and the call is then checked
   **against C++'s ranking** (§1.4). A member outside the vocabulary refuses the file as `api-missing: Class.member`. A call that changes
   a list the C++ hook passes as a `const` reference refuses the file as `header-signature`: no API row can close that (§5.2 item 2).
6. **Tier.** A transliterated file is tier A when it calls only the core vocabulary, and tier B when it uses at least one API-table row.

### 1.3 The vocabulary and the API table

The core is the inventory's tier-A vocabulary. It covers the `AbstractQuestHandler` helpers without the spawn and follow helpers,
`QuestEnv`, `QuestState`, `QuestVars`, the quest state list, `QuestService` start/finish/collect/abandon and `qe.register*` / `addOn*`. It
also covers simple getters (object and npc ids, level, race, class, inventory counts, item ids), `HandlerResult`, `ZoneName.get` and
`PacketSendUtility.sendPacket` with `SM_DIALOG_WINDOW` (`api.py` `CORE`). The API table (`api.py` `API_TABLE`) has 25 rows. Each row names
the C++ classes and members it admits and the milestone that brings their bodies:

| Row | Files | What | Bodies |
|---|---|---|---|
| B11 | 48 | crafting: `CraftSkillUpdateService::canLearnMore*CraftingSkill`, recipe and skill list checks | M5c |
| B02 | 32 | the spawn helpers of `AbstractQuestHandler` | M5d H-05 |
| B06 | 31 | kinah and item removal on the inventory (`IStorage` virtuals, implemented by `PlayerStorage`) | M5b-3 |
| B09 | 31 | positions: `getPosition`, `getWorldMapInstance`, `getX/Y/Z`, heading, instance and map ids | ported |
| B01 | 28 | `TeleportService::teleportTo` | worldId overloads ported; the delegations M5f (R5) |
| B13 | 26 | npc despawn and death: `getController().delete_()`, `deleteAndScheduleRespawn()`, `die()` | ported |
| B19 | 24 | `WorldMapType.X.getId()` → `getId(WorldMapType::X)` (`world/WorldMapTypeInfo.h`) | ported |
| B04 | 16 | quest timers: `QuestService::questTimerStart/End`, `invisibleTimerStart` | M5d E-05 |
| B03 | 14 | the follow helpers (escorts) | M5d stage 3 E-07 |
| B08 | 14 | zone checks: `isInsideZone`, `isInsideItemUseZone` | ported |
| B12 | 8 | instance creation: `InstanceService::getNextAvailableInstance` | M5f |
| B22 | 7 | `Rnd` (a namespace of free functions in commons) | ported |
| B18, B23, B24, B25 | 5 each | packets built in handlers; equipment checks; npc state and `abortMove`; divine power | ported |
| B10, B14, B15, B16 | 4 each | distances; flight state; `applyEffectDirectly` and effect checks; broadcasts and monologues | ported / M5b-2 |
| B05 | 3 | event quests: `QuestService::startEventQuest` | M5d E-02 (`m5d-plan.md:541`; the event content itself is M5i) |
| B07, B17, B21 | 3 each | `ItemService::addItem`; `SM_SYSTEM_MESSAGE` factories; reward pages | M5b-3 / generated / ported |
| B20 | 2 | `PlayerClass.getStartingClass` | ported |

`api.HEADERS` lists the headers the index reads. Rev 2 adds `model/house/House.h`, `model/templates/spawns/SpawnSearchResult.h` and
`instance/handlers/InstanceHandler.h`, three types `QuestPrelude.h` already re-exports (`:92`, `:106`, `:111`). Without them, a local of
one of these types was refused as `type`, which hid the API behind it (§4).

### 1.4 The C++ overload check

Java picks an overload by its own rules, and C++ may pick another from the same arguments: an `int` converts to `bool`, a string literal
converts to `bool` before `std::string_view`, and `int` to `int64_t` ranks the same as `int` to `bool`. After choosing the Java overload,
the tool ranks every viable C++ candidate with the standard conversion ranks (exact, promotion, conversion, user-defined). It uses the
argument **as emitted**, after `*` dereferences and `.value()` unboxing. If another overload would win or tie, it casts the primitive
arguments to the exact parameter types (`static_cast<int64_t>(1)`, `std::string_view("a")`) and checks again. If that does not settle it,
the file is refused as `cpp-overload`. No call in the real corpus needed a cast; the two unit tests (`OverloadPinning`) prove the check on
synthetic headers.

---

## 2. The C++ it emits

`eltnen/_1363ThankingMabangtah.java` (an F01 talk chain), emitted unchanged except for elided lines:

```cpp
#include "aion/gameserver/handlers/quest/QuestPrelude.h"

#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"

namespace aion::gameserver::handlers::quest::eltnen {

class _1363ThankingMabangtah final : public AbstractQuestHandler {
public:
	_1363ThankingMabangtah() : AbstractQuestHandler(1363) {}

	void register_() override {
		qe.registerQuestNpc(203943)->addOnQuestStart(questId); // Turiel
		...
	}

	bool onDialogEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		int32_t targetId = 0;
		if (runtime::as<Npc>(env.getVisibleObject()) != nullptr)
			targetId = (runtime::cast<Npc>(env.getVisibleObject()))->getNpcId();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (targetId == 203943) {
			if (qs == nullptr || qs->isStartable()) {
				...
				else if (env.getDialogActionId() == SETPRO1) {
					qs->setQuestVarById(0, qs->getQuestVarById(0) + 1);
					updateQuestStatus(env);
					PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(env.getVisibleObject()->getObjectId(), 10));
					return true;
				...
	}
};
AION_QUEST_HANDLER(_1363ThankingMabangtah, 1363);

} // namespace aion::gameserver::handlers::quest::eltnen
```

The shape follows the regscan rules (`tools/regscan/README.md`):

- the prelude comes first;
- every declaration is in the namespace of the directory;
- there is no namespace-scope `static`;
- there is no `using namespace` (the prelude's DialogAction directive, `QuestPrelude.h:82`, brings `QUEST_SELECT`);
- the marker with the literal quest id is at namespace scope (`HandlerRegistry.h:319-325`).

The mappings below extend phase6-inventory.md §7.1's emitter table with what the prototype implements:

| Java | C++ | Files |
|---|---|---|
| a local of a class type | `runtime::Ptr<T>`; `runtime::Ref<T>` for `new QuestEnv(...)`; `T&` when the initializer is a part or singleton accessor and Java never reassigns or null-checks it (conventions-game-server.md:6) | all |
| member access | `->` on `Ptr`/`Ref`/`const T*`, `.` on references | all |
| a pointer passed to a `T&` parameter | `*p` | 336 |
| `(Npc) x`, `x instanceof Npc` | `runtime::cast<Npc>(x)` (throws ClassCastException), `runtime::as<Npc>(x) != nullptr` | 344, 324 |
| `x == null`, object `==` | `x == nullptr`, pointer identity | all, 25 |
| `int[] a = {...}`, `new int[] {...}` | `std::array<int32_t, N>`; a constant index stays `a[i]`, any other index is `a.at(i)` | 124, 6 |
| `defaultOnLevelChangedEvent(player, 1, 2)` (Java varargs) | `defaultOnLevelChangedEvent(player, {1, 2})` | 62 |
| `(float) 262.9`, `(byte) 30`, `901f` | `static_cast<float>(262.9)`, `static_cast<int8_t>(30)`, `901.0f` | 41 |
| `Integer` (`QuestState.getRewardGroup`) | `std::optional<int32_t>`, `== std::nullopt`, unboxed with `.value()` | 2 |
| `WorldMapType.X.getId()`, `DialogPage.X.id()` | the enum companions `::aion::gameserver::world::getId(WorldMapType::X)` | 24 + 3 |
| `HandlerResult.fromBoolean(b)` | `::aion::gameserver::questEngine::handlers::fromBoolean(b)` (PLANNED: m5d H-06, §5.2 item 1) | 60 |
| `Rnd.get(1, 3)` | `::aion::commons::utils::Rnd::get(1, 3)` | 7 |
| a switch on an enum | `case QuestStatus::START:` | 2 |
| a case group that declares a local | braced; a local used by a later group refuses the file | 35 |
| a case group that falls through | kept, with `[[fallthrough]]; // Java: no break`. None survives in the real corpus: the 4 candidates fall after an inner switch that returns on every path, which Java cannot fall out of | 0 |
| `x += longValue` into an `int` | `x = static_cast<int32_t>(x + longValue)` (Java narrows implicitly) | 0 real, 1 synthetic |
| a local Java never reads | `[[maybe_unused]]` (MSVC /W4 C4189) | 0 real, 1 synthetic |
| Java comments and blank lines | kept in place: a statement's own-line comments before it and its trailing comment after it; the comments of case labels (trailing and own-line); the comments after if, else and loop heads, including a `{` on the next line; the comments before a method, Javadoc or not. Comments inside an expression are dropped. Measured with a scratch script over the 910 files: every Java comment of the class body and the class Javadoc appears in the C++ exactly as often as in Java. Rev 1 dropped 238 of the 2,097 body comments in 85 files, mostly npc names after case labels | all |
| a Java bug listed in phase6-inventory.md §11 | kept, as decision U3 says, with `// java-bug kept (U3, phase6-inventory.md §11): <note>` before the statement and a report line (`emit.KNOWN_JAVA_BUGS`): `beshmundir/_30348ImprovedAethercannon.java:41` (wrong item), `sanctum/_3963GrowthFlorasThirdCharm.java:70`, `_3964GrowthFlorasFourthCharm.java:70` (kinah taken twice) | 3 |

---

## 3. Coverage against the prediction

| | Measured | Predicted (phase6-inventory.md §7.1) |
|---|---|---|
| transliterated | **910 (87.9%)**, 79,666 lines | 914-929 (88-90%) with every tier-B rule |
| tier A (core vocabulary only) | 699 | 660 (classifier) |
| tier B (API-table rows) | 211 | 254-269 |
| refused | 125 | 106-121 (17 of them to G2's N-way mode) |
| of which compile only after a planned declaration | 60 (`fromBoolean`) | 38 tier-A + more tier-B (R8: 71 files call it; 11 of the 71 are refused for other reasons) |

The tool's "tier A" is wider than the classifier's: it admits locals of any class the headers type, private helper methods, loops and
arithmetic, which S2's step-table tier did not.

**By the inventory's classifier tier** (`review/tierall.json`, S2's `classify.analyse_file` over all 1,035 files):

| Classifier tier | Files | Transliterated | Refused |
|---|---|---|---|
| ST (tier A) | 660 | 657 (653 tier A, 4 tier B) | 3: `beluslan/_24054` (an `int[]` to varargs), `the_eternal_bastion/_18035`, `_28035` (switch expressions) |
| TL (idiom rules only) | 50 | 50 | 0 |
| TLAPI (non-quest API calls) | 261 | 203 | 58 |
| HAND (closures, inner classes) | 64 | 0 | 64 |

**By shard:**

| Shard | Files | Transliterated | Refused | Predicted transliterated |
|---|---|---|---|---|
| S1 tables | 365 | 332 | 33 | 338 (222 + 116; 27 closure files refused) |
| S2 templates + pairs | 277 | 256 | 21 | 267 (217 + 50) |
| S3 singletons + pairs | 264 | 247 | 17 | 246 (221 + 25) |
| S4 scripted singletons | 129 | 75 | 54 | 63-78 |

---

## 4. Refusals

Primary reason per file. A file with several reasons is counted under the first of: closures, other constructs, fields, header
signatures, API gaps, types.

| Files | Reason | Examples |
|---|---|---|
| 30 | `lambda` | `ascension/_1006Ascension.java`, `kaisinel_academy/_37003CamouflageKillers.java:59` |
| 27 | `anonymous-class` (`new Runnable() {...}` use-bar timers) | `altgard/_2208MauInTenMinutesADay.java:85`, `heiron/_3200PriceOfGoodwill.java:102` |
| 41 | `api-missing` (40 distinct members, §5) | `steel_rake/_3208ThePuzzlingBlueprint.java:41` (`QuestService.checkStartConditions`), `reshanta/_24043LazyLanguageLessons.java:131` (`DataManager.SPAWNS_DATA`) |
| 6 | `initializer` (a `Set<Integer>` of butlers filled in a static block) | `oriel/_18806HeartofRock.java` |
| 6 | `type`: `int[][]` 4, `String[]` 2 | `daevanion/_1993AnotherBeginning.java:20`, `abyss_entry/_1044TestingFlightSkills.java:20` |
| 4 | `varargs-array` (8 files have it) | `poeta/_1005BarringtheGate.java:151`, `:157` |
| 3 | `generic-type` (`List<Integer>` fields) | `pangaea/_14220NewZoneNewRules.java` |
| 3 | `switch-expression` | `sanctum/_1917ALingeringMystery.java:56` |
| 2 | `header-signature`: `AbstractQuestHandler.onBonusApplyEvent(rewardItems) is const std::vector<const QuestItems*>&; Java List.add mutates it` (§5.2 item 2) | `event_quests/_80016EventSockHop.java:81`, `_80018EventSockItToEm.java:81` |
| 2 | `mutable-field` | `eltnen/_1367MabangtahsFeast.java`, `morheim/_24026AHandfromEachSide.java` (both in phase6-inventory.md §11) |
| 1 | `throw` | `pandaemonium/_2900NoEscapingDestiny.java:259` |

Counted once per file and category instead: api-missing 84, lambda 30, anonymous-class 27, type 11, generic-type 11, varargs-array 8,
initializer 6, field-initializer 6, switch-expression 3, mutable-field 2, header-signature 2, throw 1.

Rev 1 refused 6 of the files above as `type: SpawnSearchResult` (3), `type: House` (2) and `type: InstanceHandler` (1). Those were a gap
in the tool's header index, not in the port (§1.3). With the headers indexed, the same files are refused for the API behind the type:
`DataManager.SPAWNS_DATA` (3), `Player.getActiveHouse` (2) and `WorldMapInstance.getInstanceHandler` (1). They are still refused, and
coverage does not change.

The closures (57 files) are the inventory's hand work (tier C) and G2's N-way groups. The switch expressions (3) and the `int[][]` /
`String[]` constants (6) are cheap idiom rules for a production version. The static-initializer sets, the generic fields (9 files) and the
two mutable-field files are hand ports.

### 4.1 Reason lists are lower bounds

A refused file lists every reason the tool met, which is not every reason the file has:

- A method whose body does not parse is refused whole, and none of its statements is checked. That covers a lambda, an anonymous class, a
  switch expression or a throw (`emit.py` `parse_block`). **61 refused files** have such a method: 30 lambda, 27 anonymous-class, 3
  switch-expression, 1 throw.
- A local of a refused type is not checked further, and later uses of it are skipped as follow-on failures.

So every "would transliterate if X were closed" count here is an **upper bound**. Of the 42 files refused only for API gaps or varargs, 6
have expressions that were never checked: `daevanion/_1990`, `event_quests/_50008`, `_51008`, `inggison/_10034`, `reshanta/_14043` and
`_24043`. The same holds for all 9 idiom-rule candidates. The 3 switch-expression files had the method refused whole, and the 6 `int[][]` /
`String[]` files have unchecked expressions. `eltnen/_1354PraticalAerobatics.java` is not one of the 9: it has a `String[]` constant and
also calls `Player.getLifeStats`. The production fix is in §8.4.

---

## 5. API gaps

### 5.1 The gaps that block the most files

| Files blocked | Only reason in | API | Owner / note |
|---|---|---|---|
| 60 (transliterated) | – | `HandlerResult.fromBoolean` (`HandlerResult.java:11`) | not declared; m5d H-06, `m5d-plan.md:556`, `:685` (R8); §5.2 item 1 |
| 8 | 4 | an `int[]` passed to `defaultOnLevelChangedEvent` / `defaultOnQuestCompletedEvent` varargs (`AbstractQuestHandler.java:988`, `:1047`) | **an emitter rule, no header change** (§5.2 item 3) |
| 6 | 5 | `AbstractQuestHandler.workItems` (`AbstractQuestHandler.java:53`) | **an emitter rule, no header change** (§5.2 item 4) |
| 6 | 0 | `CreatureController.addTask` (with `hasTask`, `cancelTask`) | the follow and walk tasks of the escorts; the same files have closures |
| 6 | 0 | `Player.getActiveHouse` | the oriel / pernon butler quests; every one also has a static set, a generic field or a Java `Iterator` |
| 4 | 0 | `Creature.getAi`, `WalkManager.startWalking` | escorts and walkers (M5d E-07 / A1) |
| 4 | 2 | `CustomConfig.ENABLE_SIMPLE_2NDCLASS` | a config flag (`ascension/_1007`, `_2009`) |
| 3 | 3 | `DataManager.SPAWNS_DATA` (`getFirstSpawnByNpcId`) | `event_quests/_50008:68`, `_51008:68`, `reshanta/_24043:131`; each also has unchecked uses of the result (§4.1) |
| 3 | 0 | `new SM_ASCENSION_MORPH` | Ascension (M5e / M5f) |
| 2 each | | `ClassChangeService.setClass`, `Creature.getAggroList`, `Equipment.unEquipItem`, `HousingService`, `SiegeService`, `SpawnEngine.spawnObject`, `Storage.isFullSpecialCube`, `TeleportService.teleportToNpc`, `VisibleObject.getSpawn`, `WebRewardService.MaxLevelReward`, `WorldMapInstance.getNpcs`, `broadcastPacketAndReceive`, `PlayerCommonData.updateDaeva`, `Math.sin/cos/toRadians` | mostly S4 scripted singletons |

40 distinct members are missing in all. **At most 36 files would transliterate if every gap were an API-table row, and at most 42 with
the varargs rule** (upper bounds, §4.1). The table stops at 25 rows because each further row would unblock at most 3 files on its own
(`DataManager.SPAWNS_DATA`, itself an upper bound). The 2 bonus-list files are not in these counts: no API row can unblock them (§5.2
item 2).

### 5.2 Header requests and their alternatives (P6-T)

1. **H-06, the `HandlerResult` companion**, with `fromBoolean(std::optional<bool>)` in `questEngine/handlers/HandlerResultInfo.h`,
   namespace `::aion::gameserver::questEngine::handlers` (the enum is generated, `generated/.../HandlerResult.h`). 60 generated files wait
   for it; this is the inventory's R8. **The header name and namespace are the prototype's assumption**. `m5d-plan.md:556` and `:685` name
   only "the `HandlerResult` companion, new file". Either H-06 records this spelling, or `api.PLANNED` changes to whatever H-06 lands and
   the files are regenerated.
2. **The bonus reward list (new in rev 2; for the M5d owner, E-02 and E-09).** Java builds the reward list in
   `QuestService.getRewardItems` and passes it to `QuestEngine.onBonusApplyEvent`. A handler may add a new `QuestItems` to it:
   `rewardItems.add(new QuestItems(188051106, 1))` (`event_quests/_80016EventSockHop.java:81`, `_80018EventSockItToEm.java:81`). Unless
   the handler returns FAILED, `BonusService.getQuestBonus` adds another newly created item (`QuestService.java:197-203`,
   `BonusService.java:36`). The same list is then handed out as the reward.
   C++ declares the list read-only with non-owning elements:
   - `const std::vector<const QuestItems*>&` in `AbstractQuestHandler::onBonusApplyEvent` (`AbstractQuestHandler.h:144-145`) and in
     `QuestEngine::onBonusApplyEvent` (`QuestEngine.h:162-163`; its body is already ported, `QuestEngine.cpp:617`);
   - `std::vector<const QuestItems*>` as the return of `QuestService::getRewardItems` (`QuestService.h:39-40`, `AION_UNPORTED`);
   - `const QuestItems*` as the return of `BonusService::getQuestBonus` (`BonusService.h:24`, `AION_UNPORTED`).

   A handler cannot append to a `const` reference, and a raw `const` pointer cannot own an item made at run time. `QuestItems.h:12`
   already describes `QuestItems` as a value type whose run-time rewards are copies. `runtime::Ref<QuestItems>` is not available, because
   `QuestItems` derives `StaticTemplate`, not `RefCounted` (`RefCounted.h:146-149`). **Proposal:**
   - `std::vector<QuestItems>&` in the hook and in `QuestEngine::onBonusApplyEvent`;
   - `std::vector<QuestItems>` from `getRewardItems`;
   - `std::optional<QuestItems>` from `getQuestBonus`.

   This contradicts `m5d-plan.md:686` ("none expected" for `AbstractQuestHandler.h` and `QuestEngine.h`) and `:689` (none for
   `BonusService.h`). E-02 hits it anyway when it ports `getRewardItems`, generator or not. Once the signature changes, the 2 refused
   handlers need one more idiom rule (`List.add(new X(...))` on a `std::vector<X>&` → `push_back(X(...))`). The **11 transliterated
   handlers** that override the hook without touching the list (`event_quests/_80034` ... `_80147`) take the new parameter type when they
   are regenerated, because the emitter copies the header's signature.
3. **Varargs `std::span` overloads: withdrawn; use an emitter rule.** In the 4 files where this is the only reason, the `int[]` is a local
   array literal that is never reassigned and is used once, in the call (`altgard/_24016:162-169`, `beluslan/_24054:33-40`,
   `poeta/_1005:150-157`, `verteron/_14016:146-153`). The emitter can pass it inline as the braced list that the existing
   `std::initializer_list` parameter takes (`AbstractQuestHandler.h:326`, `:329`). The other 4 of the 8 files have other reasons too. This
   avoids a change to a hub header that `m5d-plan.md:686` expects to stay unchanged.
4. **A `workItems` accessor: withdrawn; use an emitter rule.** `workItems` is protected (`AbstractQuestHandler.h:56`), so derived handlers
   reach it. `Field<Ref<X>>::operator->` returns `X*` (`runtime/fields/Field.h:192`), and `ArrayList` has `get`, `getFirst` and `getLast`
   (`runtime/collections/ArrayList.h:86`, `:266-267`). So `workItems->getFirst()->getItemId()`, `workItems->get(1)` and
   `workItems->getLast()` compile against the declared types. The accessor rev 1 proposed (`const QuestItems* workItem(int)`) would not
   have covered `getFirst` and `getLast` anyway. With the rule, the 5 files where this is the only reason no longer need a header change.
5. **The quest prelude could include five more headers.** The transliterator adds `QuestStateList.h` to 903 files, `QuestNpc.h` to 901,
   `SM_DIALOG_WINDOW.h` to 202, `QuestService.h` to 199 and `Storage.h` to 165. Including them in `QuestPrelude.h`, which is also its PCH,
   removes the per-file includes. The prelude is Q01's file (`handlers-and-porting-plan.md:874`).

---

## 6. API use and body status

The report lists every member the 910 files call, with its call and file counts. It also says whether the C++ side declares the member
(all do except `fromBoolean`) and gives the body status that `tools/porting/census.py` reports for the overloads called. The most used:

| API | Files | Body today |
|---|---|---|
| `Player::getQuestStateList`, `QuestEnv::getPlayer`, `QuestStateList::getQuestState`, `QuestState::getStatus`, `QuestEngine::registerQuestNpc`, `QuestNpc::addOnTalkEvent` | 901-903 | ported |
| `AbstractQuestHandler::sendQuestDialog` | 900 | unported |
| `AbstractQuestHandler::sendQuestEndDialog` | 870 | unported |
| `QuestState::isStartable` | 740 | unported |
| `AbstractQuestHandler::sendQuestStartDialog` | 714 | unported |
| `QuestState::getQuestVarById` | 611 | unported |
| `QuestEnv::getTargetId` | 589 | unported |
| `AbstractQuestHandler::updateQuestStatus` | 443 | unported |
| `QuestState::setStatus` | 426 | unported |
| `AbstractQuestHandler::defaultCloseDialog` | 334 | unported |
| `IStorage::tryDecreaseKinah`, `decreaseKinah`, `decreaseByItemId` | 14, 14, 6 | pure virtual; `PlayerStorage` implements them |

The transliterated files directly call 51 distinct members with unported bodies. **841 files directly call only quest-engine bodies**
(P5-06, M5d), and 7 of them call nothing unported at all (the 7 empty `steel_rake` stubs, F10). 69 files also call a body outside the
quest engine:

- `canLearnMoreExpertCraftingSkill` (30 files) and `canLearnMoreMasterCraftingSkill` (14), M5c;
- the `TeleportService::teleportTo` delegations (25), M5f, R5;
- `InstanceService::getNextAvailableInstance` (8), M5f.

Only direct callees were checked. A body that census reports as ported may still call one that is not, and that deeper reach was not
traced (the `TeleportService` delegations of R5 are the known case). So the 51 members are a lower bound on the bodies these files need,
and 841 is an upper bound on the files that need nothing outside M5d. Generation therefore does not wait for M5d, but running the
generated quests does. This matches the inventory's "generate now, link after the D3 join" (§8.1, `m5d-plan.md:31-37`).

---

## 7. Mirror pairs and G2

`mirror.py` pairs files whose quests `quest_data.xml` permits to opposite races (`race_permitted` ELYOS / ASMODIANS) and whose token
streams are equal once literals, the class name and race words are normalised. A literal twin needs one port plus a substitution row.

| | Pairs |
|---|---|
| equal-structure pairs | **186**: 98 unique literal twins (the only Elyos and the only Asmodian file of their structure) and 88 more formed in quest-id order inside larger equal-structure groups. The 88 are members of one group, not twins: `_37100MutantNinjaIninas` is paired with `_47100WardsAndWardOrbs`. G2 does not depend on the pairing, because its N-way mode works on the groups |
| both transliterated | 165: G1 emits both files; G2 is not needed |
| both refused | **21: G2's case** (hand-port one, clone the other): 8 mentor-daily pairs (F07: kaisinel_academy / marchutan_priory `_37003`, `_37006`; orichalcum_key / the_circle `_37100` ... `_37113`), 4 oriel / pernon housing pairs, 2 daevanion pairs, 2 event pairs, and one pair each in levinshor, pangaea, sanctum / pandaemonium, the_eternal_bastion and tiamat_stronghold |
| one transliterated, one refused | 0 (a literal twin shares its refusal) |
| leading-digit mirrors (1xxx/2xxx, 3xxx/4xxx) | 269, of which 101 are literal twins |

The refused files form **17 equal-structure groups of 45 files**, so G2's N-way mode would clone 28 files from 17 hand-ported
representatives. The inventory planned 17 clones of 3 representatives for S1's closure files alone (§4.1). The twin tool must still
substitute **by slot position** and check the ordered literal sequence (phase6-inventory.md §7.2). For example,
`the_circle/_47106TurningUpTheAmplifiers.java` registers 217173 but counts 217175 (§11 there), and its group-mate
`orichalcum_key/_37106AsmoHunt.java` is in the list above. `_47106` is refused (a lambda), so its hand port must carry the `// java-bug
kept` note itself; `emit.KNOWN_JAVA_BUGS` already holds the note for when the file goes through a tool.

---

## 8. What a production version needs

### 8.1 The compile check this prototype could not run

The machine was reserved for other workflows (no cmake, msbuild or ctest), so the emitted files are **unverified C++**. The prototype's
evidence is structural: the regscan shape, typed calls resolved against the real headers, and the overload check. A review pass also
compared the emitted files with their Java sources in Python. Over all 910 files, the ordered sequence of literals, of operators and of
called names matched, the last after the documented renames. Production:

- A throwaway CMake target per Q chunk compiles the emitted `.cpp` files against the real headers with the project flags (`/W4`,
  `cmake/AionCompilerOptions.cmake:8`) plus `/WX`. It also runs `aion_gs_regscan` over them: the markers, the namespaces and the Java
  cross-check of the quest id. The 70% bar of `handlers-and-porting-plan.md:558` is measured there, on compiled files.
- Failure classes to expect, from reading the output:
  - the 60 `fromBoolean` files, until H-06 lands;
  - C++ overload choices that the rank model of §1.4 simplifies (the implicit object parameter, templates other than `sendPacket`);
  - `Ptr` publication rules on part accessors (`Storage`);
  - missing includes, where a type is complete only through an include the prelude closure does not show.
- A drift check like `sysmsg.py`'s: regenerate and diff, so that hand edits to generated files show up.

### 8.2 The registration trace

For each generated file, record the ordered `(npc or item id, event kind)` list that `register_()` produces against the ported
`QuestEngine::registerQuestNpc` / `QuestNpc::addOn*`, and compare it with the Java statement order (phase6-inventory.md §7.6 item 2). The
order matters because it orders the npcs' quest lists. This form links the generated C++, so it **waits for the compile check (§8.1) and a
build slot**. A Python-only form could run now: compare the Java `register()` statements with the emitted ones. It would add little,
because the emitter keeps statement order and the call-sequence check of §8.1 already found them equal.

### 8.3 Golden traces against the Java handlers

phase6-inventory.md §7.6 item 3: turn S3's path extractor (`s3/steptable.py`) into a per-quest oracle. For every return leaf of every hook,
the oracle records the case: hook, target npc, quest status, var slots, dialog action, and the inventory counts the guards name. It also
records the expected effects in order: dialog page, var and status writes, item gives and removes, movie, return value. These go into
`tools/oracle/expected/quest/<id>.json`. After M5d, a `GameServerHarness` test drives each case through the generated handler and compares
packets and state. The oracle is written from Java, so a flipped page id or var fails a case. The jast trees of this prototype are the
natural input of that extractor. The oracle can be written now; running it waits for M5d.

### 8.4 The rest

- **`tools/parity`** (does not exist yet): literal multisets and the ordered sequences of literals, calls and operators per file. The
  review's scratch scripts are its seed and report 0 mismatches over the 910 files. G2's outputs need the ordered checks.
- **G2 itself**: the substitution rows for the 21 pairs and the N-way groups, by slot position.
- **Emitter rules that replace header requests**: pass a never-reassigned local `int[]` literal inline to a varargs helper (4 files), and
  spell `workItems` as `workItems->...` (5 files) (§5.2 items 3-4). Both are upper bounds (§4.1).
- **Cheap idiom rules** for at most 9 files: switch expressions (3), and `int[][]` and `String[]` constants as nested `std::array` (6).
  This is an upper bound (§4.1).
- **Complete reason lists**: keep checking a method's other statements when one of them contains a closure (parse the closure as an
  opaque node instead of refusing the method whole), and type a refused local's initializer. That turns the upper bounds of §4.1 and §5.1
  into exact counts.
- **`List.add` on a value vector**, for the 2 bonus-list handlers once §5.2 item 2 lands.
- **The API table and the Java-bug table as data**: both are Python today (`api.API_TABLE`, `emit.KNOWN_JAVA_BUGS`, 4 entries). The
  production table should be reviewed with the lane owners, one row per milestone gate. The bug table should be read from the deviation
  file that U3 writes.
- **Comments inside expressions** are dropped; the production emitter should attach them to the nearest statement.
- **Evaluation order**: the tool flags a call or operator with two side-effecting operands, whose order C++ leaves unspecified. None occurs
  in the 910 files, but the check must stay.
- **Commit policy**: generated files are committed into their Q chunk (`handlers-and-porting-plan.md:338`). The integrator runs the
  generator, and the lanes own the hand-finished files (phase6-inventory.md §7.1).

---

## 9. Limits

- `cppdecl.py` is a declaration reader, not a C++ front end. It splits declarations at `;` and bodies, and it resolves class names by
  simple name over about 70 headers. A header shape it does not know is skipped and shows up as an `api-missing` refusal: a false negative,
  not wrong output. A header missing from `api.HEADERS` shows up as a `type` refusal; rev 1 had three (§4).
- Refusal lists are lower bounds and the gains derived from them upper bounds (§4.1).
- Semantics that differ from Java and are accepted:
  - `std::optional::value()` throws `std::bad_optional_access` where Java unboxing throws NullPointerException (2 files);
  - `std::array::at` throws `std::out_of_range` where Java throws ArrayIndexOutOfBoundsException;
  - integer overflow is undefined in C++.
- The tool trusts Java. It does not re-check that a file compiles as Java, and it emits an unused but evaluated Java expression as it is.
  Known Java logic bugs are kept (U3) and marked only when `KNOWN_JAVA_BUGS` lists them.
- Tiers: the tool's tier A counts files that call only the core vocabulary; it is not S2's step-table tier.

---

## 10. Reproduction

From `cpp/`:

```
python -m tools.gen.questgen --dry-run --json OUT.json --markdown OUT.md     # the report (about 8 s)
python -m tools.gen.questgen --dry-run --emit <dir outside the repo>         # also write the 910 .cpp files
python -m tools.gen.questgen --dry-run --only eltnen/_1363ThankingMabangtah.java -v
cd tools/gen && python -m unittest tests.test_questgen                       # 40 tests, no compiler
```

The JSON has, for each file, the status, tier, quest id, refusal reasons, API-table rows, APIs called, planned declarations used, unported
bodies reached and the Java-bug notes placed. It also has the full API and mirror tables.

---

## 11. Work that can run in parallel

No build slot is free now, so every lane that compiles or runs C++ waits. The rest can start at once. The ownership of each lane is in
its right-hand column.

| Lane | Can start | Depends on | Touches |
|---|---|---|---|
| questgen fixes (Python only): the emitter rules of §5.2 items 3-4, the idiom rules and complete reason lists of §8.4 | now | nothing; serialize edits inside `tools/gen/questgen` | `tools/gen/questgen`, its tests |
| `tools/parity` with the ordered literal, call and operator checks (§8.4) | now | nothing | new `tools/parity` |
| golden quest oracle from the S3 path extractor (§8.3), split by Q chunk | now (writing) | M5d to run it through the harness | `tools/oracle/expected/quest` |
| U3 deviation notes for `_30348`, `_3963`, `_3964` (and `_47106` with its hand port) | now | the owner of the deviation file | `docs/deviations/` |
| M5d owner decisions (docs only): pin the H-06 spelling (§5.2 item 1); the reward-list signature (§5.2 item 2) | now | the M5d owner | `m5d-plan.md` §9 |
| G2 twin tool and N-way mode (§7) | after `tools/parity` | `tools/parity` for its check | `tools/gen` |
| hand ports of the refused files by Q chunk | now (writing) | a build slot to verify; M5d, M5c or M5f to run; the 2 bonus-list files wait for §5.2 item 2 | Q01-Q14 chunks |
| H-06 companion header and the reward-list header change | after the decisions | the M5d owner's sign-off and a build slot (`AbstractQuestHandler.h`, `QuestEngine.h` and `QuestService.h` are hub headers) | `questEngine/**`, `services/QuestService.h`, `services/reward/BonusService.h` |
| compile check of the emitted files per Q chunk (§8.1), `/W4 /WX` plus `aion_gs_regscan` | when a build slot is free | a build slot; the questgen fixes first, so that the batch compiled is the one committed | a throwaway CMake target |
| registration trace against the C++ runtime (§8.2) | after the compile check | the compile check | `tools/oracle` |

The compile check gates any run of generated C++, the C++ registration trace and every commit of a generated batch.
