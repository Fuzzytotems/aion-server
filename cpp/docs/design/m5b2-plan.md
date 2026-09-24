# M5b-2 work plan (abilities: skills and effects)

> **Status:** plan **rev 1**, 2026-09-23. A **read-only** analysis over HEAD `340c05c5e` ("M5b-1 stage 1: a monster can fight back") plus the
> uncommitted stage-2 gate lane in the working tree, against the Java 4.8 tree. **Nothing was compiled, built or run for this plan**; every C++
> statement below comes from reading the two trees, from `game-server/chunks.cmake` and from counting `AION_UNPORTED(` / `AION_PARTIAL(` call
> sites with grep. Every number is stated with the file it came from, and §12 lists which claims were **measured** and which were **inferred**.
>
> It follows the shape of [m5b-plan.md](m5b-plan.md): path analysis with file:line, numbered work items with owners and dependencies, lanes, a
> gate specification, risks and an honest split. Ownership follows `tools/porting/chunks.py`; header changes follow
> [hub-headers.md](hub-headers.md) §14 through `docs/porting/header-requests.md`.
> Inputs: m5b-plan.md §2/§4/§6/§8/§9, [m5b-client-session.md](m5b-client-session.md) S-1/S-2/S-3, [m5a-client-session.md](m5a-client-session.md)
> F-2/F-3, [runtime-architecture.md](runtime-architecture.md) §5.3/§7.3, `game-server/generated/concurrency/cycles.toml`,
> `tests/scenario/m5b_partial_allowlist.txt`.
>
> **It does not inherit m5b-plan.md §9's sizing, and §3 says where the two disagree.** The Java-LOC estimate (~17,800) holds almost exactly
> (measured 17,835), and so does the site count once it is added up honestly: **436** in the three skill chunks plus 9 in P5-01 and 3 stand-ins
> in P4-11b = **448**, against §9's "~450". **What §9 could not see is that ~85 further bodies exist that no `AION_UNPORTED` count can reach**,
> because their classes have a shell header and no `.cpp` at all — so the milestone is ~521 bodies, not ~450. The wave count is the bigger
> correction: §9 said "2 waves, probably 3", and this analysis says **4 stages, of which stage 0 is a 2-day one-lane wave that should land on its
> own**, because the piece that unblocks every caster class is 4 bodies and is not coupled to the effect engine at all.
>
> **And one number nobody has:** the milestone is only one milestone because **34 of the 184 effect classes** cover every skill the four starting
> classes, the two start maps' npcs and the whole tree's post-spawn entries can produce (§2.4). Everything else stays `AION_UNPORTED` on purpose.

---

## 1. Summary

**M5b-1 left the skill engine an empty shell with a working frame around it.** Everything a cast touches *outside* `skillengine/` is ported and
green: the controllers, the stat containers, the damage arithmetic of the physical half, the aggro list, the AI, and — this is the finding that
shapes the gate — **every server packet the skill and effect engine sends**.

| Already ported, 0 `AION_UNPORTED` | Evidence |
|---|---|
| `PlayerController::useSkill` (the `CM_CASTSPELL` body) | `controllers/PlayerController.cpp:555-580` (Java PlayerController.java:458-481) |
| `CreatureController::useSkill` / `useChargeSkill` (the npc and item entry points) | `controllers/CreatureController.cpp:469-519` (Java CreatureController.java:448-498) |
| `Creature::setCasting`, `isCasting`, `getCastingSkill`, `isSkillDisabled`, `setSkillCoolDown`, `getSkillCooldown`, `canUseSkillInMove` | `model/gameobjects/Creature.cpp`, `Creature.h:135-137` |
| `Player::getSkillList`, `getChainSkills`, `setHitTimeBoost`, `isSkillDisabled`; `PlayerSkillList` (1 body left), `PlayerSkillEntry`, `SkillEntry` | `model/gameobjects/player/Player.cpp`, `model/skill/*.cpp` |
| **Every skill/effect server packet**: `SM_CASTSPELL`, `SM_CASTSPELL_RESULT`, `SM_SKILL_CANCEL`, `SM_SKILL_ACTIVATION`, `SM_SKILL_COOLDOWN`, `SM_ABNORMAL_STATE`, `SM_ABNORMAL_EFFECT`, `SM_SKILL_LIST`, `SM_SKILL_REMOVE`, `SM_MANTRA_EFFECT`, `SM_ITEM_USAGE_ANIMATION`, `SM_RESURRECT`, `SM_STATUPDATE_MP`, `SM_PLAYER_STATE`, `SM_POSITION`, `SM_FORCED_MOVE`, `SM_EMOTION` | all 20 exist under `network/aion/serverpackets/`, **0 `AION_UNPORTED` in every one** (measured) |
| `PlayerEffectsDAO` (load and store `player_effects`), `PlayerSkillListDAO` (`player_skills`) | `dao/PlayerEffectsDAO.cpp`, `dao/PlayerSkillListDAO.cpp`, 0 unported each |
| `ObserveController` — `checkShieldStatus`, `checkAttackStatus`, the attack-calc observer list | `controllers/ObserveController.cpp`, 0 unported |
| `AttackUtil` **physical** half (11 bodies), `AggroList`, `DamageList`, `TeamDamageList`, `AttackResult`, `KillCounter`, `StatFunctions` combat + reward halves | `controllers/attack/*.cpp`, `utils/stats/StatFunctions.cpp` — 24 of 30 `AttackUtil` bodies ported, 19 of 27 `StatFunctions` |
| `ai/manager/{AttackManager,SimpleAttackManager,EmoteManager,WalkManager,FollowManager}`, `GeneralNpcAI`, `NpcAI`, `ai/handler/*` | `ai/**`, `handlers/aion/gameserver/handlers/ai/*` — one `AION_PARTIAL` left in the whole handler tree (`GeneralNpcAI.cpp:122`) |
| `cycles.toml` rows for the effect system — **55 of them**, already written and reviewed at the S0b/S0c freeze | `generated/concurrency/cycles.toml:273-313` and :56-57, :62, :108, :170-173 |

**What is empty is `skillengine/` itself**, and it is of four very different sizes:

| # | Hole | Where | Size (measured) |
|---|---|---|---|
| 1 | **The cast machine.** `Skill` 48 of 55 bodies, `SkillEngine` 14 of 16 (+2 `AION_PARTIAL`), `ChainSkills` 4, `ChainSkill` 2; the whole `skillengine/properties/` package — **`Properties.h` is a shell with no behaviour declared and 7 of its 8 classes have no C++ file at all**. | **P5-02** | 68 bodies + 21 new, ~2,270 Java LOC |
| 2 | **The effect machine.** `Effect` 83 of 88 bodies, `EffectTemplate` 26 of 26, `EffectController` **41 of 58 bodies (42 sites — one body holds two)** plus 1 partial, `PlayerEffectController` 5 (+1), `CumulativeResist` 5, `effect/modifier/` 10. | **P5-02** / **P5-03** | 171 bodies, ~2,750 Java LOC |
| 3 | **The 184 effect classes.** 126 have a `.cpp` with **174** `AION_UNPORTED`; **29 have a shell header, no `.cpp` and 45 Java method bodies that are not even declared**; 29 are data-only and need nothing. | **P5-03** (100), **P5-04** (74) | **219** bodies, 8,330 Java LOC |
| 4 | **The magical arithmetic.** `AttackUtil` 6 bodies, `StatFunctions` 3 — and **4 of those 9 are the reason a Mage, Priest or Spiritmaster cannot swing a weapon** (m5b-client-session.md S-3). | **P5-01** | 9 bodies, ~200 Java LOC |

**The decisive finding is §2.4: 34 effect classes are enough.** Every skill a level-1..10 character of the four starting classes can use, every
skill any npc on Poeta or Ishalgen owns, every `is_post_spawn` skill in the whole tree, and Soul Sickness, together need **34 of the 184 effect
classes — 71 bodies and 2,269 Java LOC: 18 % of the classes, 32 % of the bodies, 27 % of the LOC.** A milestone that ports 184 effect classes is
not a milestone; this is the subset that makes the starting classes playable, and it is named class by class in §2.4.

**The second finding is that the caster unblock is 4 bodies and has no dependency on any of the above** (§2.5). `AttackUtil.calculateMagAttackResult`
with `isSkill = false` calls `calculateMagicalStatus` → `StatFunctions.calculateMagicalResistRate` / `calculateMagicalCriticalRate`, and then four
bodies that M5b-1 already ported. No `Effect`, no `EffectTemplate`, no `Skill`. It should land first, in a stage of its own, and it turns the
§10 real-client checklist from "melee classes only" into "any class".

**The third finding is structural and needs a human decision before anything starts** (D1). `skillengine/**` + `model/skill/**` +
`controllers/effect/**` are **one chunk**, P5-02, holding **262 of the 436 unported sites of the three skill chunks**. A chunk is the unit of ownership and a
lane owns a chunk, so as the manifest stands M5b-2's core is one lane for the whole milestone. §5 proposes splitting P5-02 along the
`Skill` | `Effect` seam into two parts sharing one target, exactly as P4-07a/P4-07b share `aion_gs_templates`.

---

## 2. The path a cast takes, end to end

### 2.1 The trace

Every step names the Java source and the state of the C++ body at HEAD. "ported" means the body exists and contains no `AION_UNPORTED`.

| # | Step | Java | C++ today | Chunk |
|---|---|---|---|---|
| 1 | Client sends `CM_CASTSPELL(spellid, level, targetType, [objId \| x,y,z], hitTime)`; dead check, `spellid == 0` → `cancelCurrentSkill`, pet-order check, `SKILL_DATA.getSkillTemplate`, passive check, stop spawn protection, `cancelUseItem`, the `getNextSkillUse()` audit, then `player.getController().useSkill(template, targetType, x, y, z, hitTime, level)` | CM_CASTSPELL.java:36-109 | **missing entirely** — no `CM_CASTSPELL.cpp`/`.h` (41 of the 190 `CM_*` classes exist) | **P5-15** |
| 2 | `PlayerController.useSkill`: `SkillEngine.getSkillFor(player, template, player.getTarget())`, the transform-panel arm, cancel an item cast, `PlayerRestrictions.canUseSkill`, `setTargetType`, `setClientHitTime`, `skill.useSkill()` | PlayerController.java:458-481 | **ported** (`PlayerController.cpp:555-580`), calling `standins::playerRestrictionsCanUseSkill` (`ControllerStandIns.cpp`, `AION_UNPORTED`) | P4-11b |
| 3 | `PlayerRestrictions.canUseSkill`: casting/stance/silence/`isSkillDisabled`/`hasEvadeEffect`/resurrect checks | PlayerRestrictions.java:51-116 | the file exists (M5b-1 C-01) and this body is **`AION_PARTIAL` returning false** (`restrictions/PlayerRestrictions.cpp:53-62`) — §B row of `m5b_partial_allowlist.txt` | **P5-13** |
| 4 | `SkillEngine.getSkillFor`: `ActivationAttribute.PROVOKED` bypass, `skillList.isSkillPresent`, `new Skill(template, player, target)` | SkillEngine.java:56-68 | **`AION_UNPORTED`** (`skillengine/SkillEngine.cpp:25-28`); 14 of the 16 `SkillEngine` bodies are unported, `applyEffectDirectly(SkillTemplate*,…)` is the M5a O-09 partial (`:79`) and `createCriticalProcEffect`'s `id != 0` arm is the D14 partial (`:138`) | **P5-02** |
| 5 | `Skill.useSkill(checkAnimation, checkProperties)`: boost-skill-cost observers, `canUseSkill(CAST_START)`, `updateCastDurationAndSpeed`, `updateHitTime`, start-cast observers, `effector.setCasting(this)`, `startCast()`, `AISubState.CAST`, the npc `lastSkill` bookkeeping, `ai.onStartUseSkill`, then **`schedule(this::endCast, castDuration)`** or `endCast()` when instant | Skill.java:274-319 | `Skill.cpp` — **48 of 55 bodies unported**; the 7 ported are the three `create` overloads, `setFirstTarget`, `initializeSkillMethod`, `getSkillId`, `isPassive` (wave 5a) | **P5-02** |
| 6 | `Skill.canUseSkill(CastState)` → `skillTemplate.getProperties().validate(skill, castState)` → the seven `*Property.set(...)` steps that pick the first target and the effected list | Skill.java; Properties.java:70-118; FirstTargetProperty.java, TargetRangeProperty.java, TargetRelationProperty.java, TargetStatusProperty.java, TargetSpeciesProperty.java, FirstTargetRangeProperty.java, MaxCountProperty.java | **`Properties.h` is a shell that declares only the 15 generated getters**, and **7 of the 8 property classes have no C++ file at all** (907 Java LOC). This is the single largest "missing file" block of the milestone | **P5-02** |
| 7 | `Skill.startCast`: broadcast `SM_CASTSPELL` (the three `targetType` arms), `ShoutEventHandler.onCast` for an npc, `player.setNextSkillUse(now + MIN_SKILL_CAST_INTERVAL_MILLIS)`, attach the `DeathObserver` on the first target | Skill.java:495-539 | unported (part of the 48); `SM_CASTSPELL` itself is **ported**, `ShoutEventHandler` is **ported** (M5b-1 A-01) | P5-02 |
| 8 | `Skill.endCast`: `removeObservers`, `properties.endCastValidate`, `validateEffectedList`, `preUsageCheck`, `isInvalidRecall`, **`payCastCosts`** (the `endconditions`), `setCasting(null)`, then **per effected creature `new Effect(this, effected); effect.initialize();`**, the chain/stance/multicast arms, `QuestEngine.onUseSkill`, `setCooldowns`, `startPenaltySkill`, `enterCombat`, **`applyEffect(effects)` now or `schedule(…, hitTime)`**, `sendCastSpellEnd`, the animation-time bookkeeping, `ai.onEndUseSkill`, `lastSkill.fireOnEndCastEvents`, **`SkillAttackManager.afterUseSkill`**, `instanceHandler.onEndCastSkill` | Skill.java:556-702 | unported | P5-02 |
| 9 | `Effect.initialize()`: `effectController.isConflicting`, `template.calculate(this)` for every `EffectTemplate`, `calculateHateForSuccessEffects`, the sub-effect pass, the **10 % critical proc** (`SkillEngine.createCriticalProcEffect`), the DODGE/RESIST fallbacks, the `SpellStatus` switch | Effect.java:512-583 | `Effect.cpp` — **83 of 88 bodies unported**; the 5 ported are the four `create` overloads and `setSubEffect` | **P5-02** |
| 10 | `EffectTemplate.calculate(effect, statEnum, spellStatus)` → `AttackUtil.calculatePhysicalStatus(attacker, attacked, template, effect)` or `calculateMagicalStatus`; `calculateDamage`, `calculateSubEffect`, `calculateHate`, `getModifiers` | EffectTemplate.java:281-500 | `skillengine/effect/EffectTemplate.cpp` — **26 of 26 bodies unported**; it is owned by **P5-03**, not P5-02 (`chunks.cmake:264`, glob `effect/[A-L]*`) | **P5-03** |
| 11 | `AttackUtil.calculateSkillResult` / `calculateEffectResult` / `calculateMagicalOverTimeSkillResult` → `StatFunctions.calculateMagicalSkillDamage`, `adjustDamageByPvpOrPveModifiers` → `effect.setReserveds(...)` | AttackUtil.java:… , StatFunctions.java:420, 609 | **6 of 30 `AttackUtil` bodies unported** (`controllers/attack/AttackUtil.cpp`): `calculateSkillResult`, `calculateEffectResult`, `calculateMagAttackResult`, `calculateMagicalOverTimeSkillResult`, `calculatePhysicalStatus(…EffectTemplate*, Effect&)`, `calculateMagicalStatus`; **3 of 27 `StatFunctions`**: `calculateMagicalSkillDamage`, `calculateMagicalCriticalRate`, `calculateMagicalResistRate` | **P5-01** |
| 12 | `Effect.applyEffect()` → per success template `applyEffect(effect)` → damage goes to `Creature.getController().onAttack(...)`, i.e. **step 6 of m5b-plan.md §2.1, which M5b-1 proved** | Effect.java:596-620 | unported | P5-02 |
| 13 | `Effect.startEffect()` → `addToEffectedController()` → `EffectController.addEffect` / `put` → `broadCastEffects` → `SM_ABNORMAL_STATE` / `SM_ABNORMAL_EFFECT`; `schedulePeriodicActions`, the per-position `setPeriodicTask`, the end task at `+duration` | Effect.java:652-703, 864-876 | `EffectController.cpp` — **41 of 58 bodies unported (42 sites)**; ported already: `broadCastEffects`, `getAllEffects`, `filterEffects`, `removeAllEffects` ×2, `canRemoveOnDie`, `isUnderFear`, `isConfused`, `getAbnormalEffects`, `getAbnormalEffectsToShow`, `isAbnormalSet`, `isInAnyAbnormalState`, `isEmpty`, `clearEffectMapsWithoutNotify` (M5a logout work). `removeByDispelSlotType` is `AION_PARTIAL` (`:209`, §C allow-list row) | **P5-02** |
| 14 | The per-tick work: `AbstractOverTimeEffect.startEffect` schedules `onPeriodicAction` every `checkTime`; `Effect.schedulePeriodicActions` schedules `PeriodicActions.getPeriodicActions()` at `checktime`; `Effect.stopTasks` cancels both | AbstractOverTimeEffect.java:35-68; Effect.java:864-882 | `AbstractOverTimeEffect.cpp` 3 of 4 unported; `Effect.periodicTasks` / `periodicActionsTask` are declared (`Effect.h:70-71`) and the cycle rows exist (`cycles.toml:273, 278, 283, 286-287`) | P5-02 / P5-03 |
| 15 | `Effect.endEffect()` → `removeObservers`, `stopTasks`, `CreatureGameStats.endEffect`, `EffectController.clearEffect`, `SM_ABNORMAL_STATE` again | Effect.java:704-765 | unported. **This is the hook `cycles.toml` names 40 times** ("java-hook: Effect.endEffect -> removeObservers …"), so a wrong body is a leak, not a bug | P5-02 |
| 16 | Cancellation: `PlayerController.cancelCurrentSkill` → `castingSkill.cancelCast()`, `SM_SKILL_CANCEL`, `STR_SKILL_CANCELED`; movement cancels through `onStartMove`/`onStopMove`; damage cancels through `CreatureController.onAttack`'s `cancelRate` roll | PlayerController.java:514-546; CreatureController.java:225-230 | `PlayerController::cancelCurrentSkill` and the `onAttack` cancel arm are **ported**; `Skill::cancelCast` and `cancelCurrentSkillCast` are not | P4-11b ported / P5-02 |
| 17 | Npc abilities: `GeneralNpcAI.chooseAttackIntention` → `chooseSkillAttack` → `SkillAttackManager.chooseNextSkill` → `NpcSkillList.getRandomSkill` / `NpcSkillTemplateEntry.conditionReady` → `AttackManager.chooseAttack(SKILL_ATTACK)` → `SkillAttackManager.performAttack` → `skillAction` → `CreatureController.useSkill(skillId, level)` → step 4 | GeneralNpcAI.java:116-138; SkillAttackManager.java:20-114 | `chooseSkillAttack` is `AION_PARTIAL` returning false (`handlers/ai/GeneralNpcAI.cpp:122`); all four `SkillAttackManager` bodies are `AION_PARTIAL` (`ai/manager/SkillAttackManager.cpp:19,23,28,40`), `afterUseSkill` is really ported; `NpcSkillTemplateEntry::conditionReady` and `fireOnEndCastEvents` are `AION_UNPORTED` (`model/skill/NpcSkillTemplateEntry.cpp:78,115`) | **P5-05**, P5-02 |
| 18 | Post-spawn skills: `SpawnEventHandler.onSpawn` → `npc.getSkillList().getPostSpawnSkills()` → `SkillEngine.getSkill(...).useWithoutPropSkill()` | SpawnEventHandler.java:20-22 | `NpcSkillList::getPostSpawnSkills` is `AION_PARTIAL` returning empty (`model/skill/NpcSkillList.cpp:90`, D3) — **§A allow-list row, hit once per spawn, ~83,885 times a run** | P5-02 |
| 19 | Passive skills: `PlayerEnterWorldService.activatePassiveSkillEffects` → `SkillEngine.applyEffectDirectly(template, lvl, player, player)` | SkillEngine.java | **`AION_PARTIAL`** (`SkillEngine.cpp:79`, M5a O-09) — **8 hits per enter world**, §A allow-list row. Closing it is what makes a character's stats right, and it is the milestone's biggest cross-gate hazard (**D2**) |P5-02 |
| 20 | Saved effects: `PlayerEffectsDAO.loadPlayerEffects` → `PlayerEffectController.addSavedEffect`; logout writes back every effect with `canSaveOnLogout() && remainingTimeMillis > 28000` | PlayerEffectController.java:110-117; PlayerEffectsDAO.java:36 | the DAO is **ported**; `addSavedEffect`'s restore is `AION_PARTIAL` (`PlayerEffectController.cpp:120`, §C allow-list row). **Nothing writes `player_effects` today; M5b-2 is the wave that starts** | P5-02 |
| 21 | Soul sickness: `PlayerController.updateSoulSickness` → `SkillEngine.getSkill(player, 8291, deathCount, player).useSkill()` | PlayerController.java:723-733 | ported (`PlayerController.cpp:832`) and **throws today**, which is why the M5b-1 profile sets `gameserver.soulsickness.disable=0` (m5b-plan.md D1). Skill 8291 uses **only `StatdownEffect`** (`skill_templates.xml`, three `<statdown>` on MAXHP/MAXMP/SPEED), which is in §2.4's subset — so M5b-2 removes the key (**D5**) | P5-02 |

### 2.2 Status by area (measured at HEAD)

Counted with `grep -o 'AION_UNPORTED('` over the exact C++ file set each chunk's globs select (`chunks.cmake:256-270`), and `wc -l` over the Java
file set. "java no cpp" counts Java classes with no `.cpp`, no `.h` under `src/` and no generated header.

| Area | Chunk | `AION_UNPORTED` | `AION_PARTIAL` | Undeclared bodies | java no cpp | Java LOC |
|---|---|---|---|---|---|---|
| `skillengine/**` (minus `effect/*`) + `model/skill/**` + `controllers/effect/**` | **P5-02** | **262** | 5 | 4 (`Properties`) + 36 in 11 missing classes | **11** | 9,505 |
| `skillengine/effect/[A-L]*` | **P5-03** | **100** | 0 | 22 (in 14 classes) | 0 | 4,422 |
| `skillengine/effect/*` minus `[A-L]*` | **P5-04** | **74** | 0 | 23 (in 14 classes) | 0 | 3,908 |
| `AttackUtil` magical half + `StatFunctions` magical half | P5-01 (part) | **9** | 0 | 0 | 0 | ~200 |
| `restrictions/PlayerRestrictions::canUseSkill` | P5-13 (part) | 0 | 1 | 0 | 0 | 65 |
| `controllers/ControllerStandIns` (the three skill stand-ins) | P4-11b (part) | 3 | 0 | 0 | 0 | – |
| `SkillAttackManager` + `GeneralNpcAI::chooseSkillAttack` | P5-05 | 0 | 5 | 0 | 0 | ~230 |
| `model/skill/NpcSkillTemplateEntry` (`conditionReady`, `fireOnEndCastEvents`) | P5-02 (already inside its 262) | *2* | 0 | 0 | 0 | ~30 |
| `CM_CASTSPELL` / `CM_USE_CHARGE_SKILL` / `CM_TOGGLE_SKILL_DEACTIVATE` / `CM_REMOVE_ALTERED_STATE` | P5-15, P5-16 | – | – | – | **4** | 229 |
| **Total, without double counting** | | **448** = 436 in the three skill chunks + 9 (P5-01) + 3 (P4-11b). `NpcSkillTemplateEntry`'s 2 are already inside P5-02's 262 | 11 | **~85** | 15 | **17,835** (the three chunks; the 941 LOC of the 11 missing classes and the 948 of the 29 shells are inside it) |

**Three corrections to m5b-plan.md §9, and one of them changes how the milestone should be counted.**

1. **448 in total, which is §9's "~450" — but the per-chunk numbers moved.** P5-02 is **262**, not the 271/269 of m5b-plan.md §2.2/§9: M5b-1's stage-1 npc-skills-shim lane closed seven of them
   (`NpcSkillList` and `NpcSkillEntry` bodies, `SkillEngine::createCriticalProcEffect`'s reordered switch). P5-03 is 100 and P5-04 is 74, both
   unchanged. **Verified by grep at HEAD.**
2. **~85 bodies are invisible to the site count, and this is the correction that matters.** A class whose C++ shell has a header and no `.cpp`
   has **zero** `AION_UNPORTED` sites and is still entirely unported. There are **29 such effect classes with 45 Java method bodies that the
   shell headers do not even declare** (`StatupEffect.h` is the shortest example: Java overrides `endEffect`, the header declares nothing), plus
   `Properties` (4) and 11 P5-02 classes with no C++ file at all (36 bodies, 941 Java LOC). `docs/porting/header-requests.md` **shells-2 already
   records this as an open issue** — "override declarations of the virtuals `EffectTemplate` already declares (`startEffect`, `endEffect`,
   `calculate`, `onPeriodicAction`, …) in the other effect shells (open issue)". M5b-2 is the milestone that closes it, and **the true body count
   of the three chunks is ~521, not 436**.
3. **The Java-LOC estimate was right.** 9,505 + 4,422 + 3,908 = **17,835** against §9's "~17,800". That number can be trusted; the site count
   cannot.

`P5-02`'s 262 sites, per file, so that §4 can carve them (the six largest are 82 % of the chunk):

| Sites | File | | Sites | File |
|---|---|---|---|---|
| **83** | `skillengine/model/Effect.cpp` | | 10 | `skillengine/effect/modifier/*` (5 files × 2) |
| **48** | `skillengine/model/Skill.cpp` | | 8 | `skillengine/action/*` (4 files × 2) |
| **42** (+1 partial) | `controllers/effect/EffectController.cpp` | | 4 | `skillengine/model/ChainSkills.cpp` |
| **14** (+2 partials) | `skillengine/SkillEngine.cpp` | | 3 | `skillengine/condition/OnFlyCondition.cpp` |
| 35 | `skillengine/condition/*` (22 files) | | 2 | `skillengine/model/ChainSkill.cpp`, `model/skill/NpcSkillTemplateEntry.cpp` |
| 5 + 5 | `controllers/effect/{CumulativeResist,PlayerEffectController}.cpp` | | 2 | `skillengine/periodicaction/*` |

### 2.3 The effect system as it actually is

**What an `EffectTemplate` is.** One `<effects>` child element of a `<skill_template>` in `data/static_data/skills/skill_templates.xml` — bound
by JAXB in Java and by xmlgen in C++ — is one `EffectTemplate` subclass instance, shared by every cast of that skill. `Effects.java:30-200` maps
**170 XML element names to Java classes** (`<skillatk>` → `SkillAttackInstantEffect`, `<statup>` → `StatupEffect`, `<root>` → `RootEffect`, …).
`EffectTemplate` (574 Java lines, `skillengine/effect/EffectTemplate.java:35-500`) carries the bound attributes (`duration1`, `duration2`,
`effectid`, `e` = position, `hittype`, `element`, `hoptype`/`hopa`/`hopb` for hate, `accmod1/2`, `critprobmod1/2`, `value`, `delta`, …) and the
five behaviour hooks the subclasses override: **`calculate`**, **`applyEffect`** (abstract), **`startEffect`**, **`onPeriodicAction`**,
**`endEffect`**. An `Effect` (`skillengine/model/Effect.java`, 1,212 lines) is **one application** of a skill's templates from one effector to one
effected creature, and it is the object that lives in the `EffectController` maps, owns the periodic and end tasks and holds the stat functions.

**How many effect classes exist, and what the C++ tree has** (measured over `skillengine/effect/*.java`, 184 files):

| Kind | Count | Java LOC | Bodies to write | Meaning |
|---|---|---|---|---|
| shell + `.cpp` with `AION_UNPORTED` stubs | **126** | 5,868 | **174** | the normal case: bodies to fill |
| **shell header only, no `.cpp`** | **29** | 948 | **45**, spread over **28** of them (`SkillAttackInstantEffect`'s three Java methods are all generated getters) | the invisible case of §2.2 item 2 — **and each of the 28 needs a header change, because the override is not declared** |
| data-only, wholly generated | **29** | 424 | 0 | an empty subclass (`SpellAttackInstantEffect extends DamageEffect {}`) — `generated/…/SpellAttackInstantEffect.h` says "data-only: generated completely" |
| no C++ file at all | **0** | – | – | every Java effect class is represented |
| **Total** | **184** | **8,330** | **219** | |

Beware the data-only row when reading an inventory: `SpellAttackInstantEffect`, `StatboostEffect` and 27 others look "done" and carry no work,
but the work is in their **base** (`DamageEffect`, `BufEffect`). Any subset must be closed under `extends`, which is what §2.4 does.

### 2.4 The subset that makes the starting classes playable — **34 classes**

Derived from the Java data files, not guessed. Method: `skill_tree.xml` gives every `skillId` with its `classId` and `minLevel`;
`skill_templates.xml` gives each template's `<effects>` children; `Effects.java` maps the element names to classes; the set is then closed under
`extends`. Scripts are throw-away; the numbers below are reproducible with them and **must be re-derived by the oracle before a lane is sized
from them** (m5b-plan.md risk 20, and §12 of this plan).

**(a) Player skills.** For the four **starting** classes (`WARRIOR`, `SCOUT`, `MAGE`, `PRIEST`) at `minLevel <= 10`: **49 skill templates**
(16 passive, 33 active) using **21 leaf effect classes**, 26 with bases. **Mind the class-less rows**: `skill_tree.xml` has entries with no
`classId` at all, which belong to *every* class — `243` *Return* (`ReturnEffect`), `245` *Bandage Heal* (`HealInstantEffect`) and `302` *Escape*
(`EscapeEffect`) at `minLevel="1"` (skill_tree.xml:303-304, 429). A scan that filters on `classId` misses them; the first draft of this section
did, and `ReturnEffect` and `EscapeEffect` are on every character's bar from the first minute. `oracle.py m5a-creation --race ELYOS --class MAGE`
is the cross-check and it lists them. Widening the scope costs a lot fast, which is the argument for stopping at 10:

| Scope | Skill templates | Leaf classes | With bases | Effect bodies | Java LOC |
|---|---|---|---|---|---|
| starting classes, level ≤ 10 | 49 (16 passive, 33 active) | 21 | 26 | **59** | 1,784 |
| all 12 classes, level ≤ 10 | 132 | 42 | 49 | 81 | 2,883 |
| all 12 classes, level ≤ 20 | 411 | 71 | 79 | 112 | 4,238 |
| all 12 classes, level ≤ 25 | 570 | 81 | 89 | 121 | 4,549 |
| all 12 classes, all 65 levels | 2,985 | 105 | 113 | 151 | 5,527 |

**(b) Npc skills of the two start maps.** m5b-plan.md D4 says "neither of the two gate monsters has any entry in `npc_skills.xml`", which is true
of **210663** (verified: no `npc_ids` list contains it) — but it is **not** true of the map. **32 of Poeta's 147 npc ids and 36 of Ishalgen's 166
own skills**, all of them `ai="aggressive"`, and several are level 1-4 and within 80 m of the Elyos spawn point:

| npc | level | ai | nearest spot | distance from the Elyos spawn | skills | name |
|---|---|---|---|---|---|---|
| **210133** | **1** | aggressive, `srange=7`, 18 spots | (1153.9, 995.87, 137.0) `static_id=0` | **76.8 m** | 16419 *Brandish* | striped kerub |
| 210134 | 2 | aggressive, 23 spots | (1080.35, 1111.82, 121.305) | 149.8 m | 16419 | striped kerub |
| 210205 | 3 | aggressive, `srange=8` | (728.595, 1078.25, 99.375) | 487.3 m | 16417 *Scratch* | pinkbeak airon |
| 210306 | 8 | aggressive, 1 spot | (679.122, 1896.6, 143.746) | 1005 m | 17018 *Bite* (**BleedEffect**) | scar |

The 29 distinct npc skill ids of the two maps need **8 leaf effect classes**: `SkillAttackInstantEffect`, `SpellAttackInstantEffect`,
`StatupEffect`, `ShieldEffect`, `HideEffect`, `BleedEffect`, `PolymorphEffect`, `ProvokerEffect` — **five of which the player set already needs**,
so npc abilities cost **three** extra classes (`BleedEffect`, `PolymorphEffect`, `ProvokerEffect`, 8 bodies).

**(c) Post-spawn skills** (m5b-plan.md D3). 199 `is_post_spawn="true"` entries in 12 files under `data/static_data/npc_skills/`, **33 distinct
skill ids**, needing **10 effect classes** — of which only **`HealEffect`, `ReflectorEffect`, `SanctuaryEffect`** are new (3 bodies). *(m5b-plan.md
D3 and risk 13 say "185 distinct npc ids … 13 of the 25 XML files"; I measure 199 entries in 12 files and 491 distinct npc ids once the
`npc_ids="a,b,c"` lists are expanded. The denominators differ; the oracle should settle it — §12.)*

**(d) Soul sickness** (skill 8291): three `<statdown>` → `StatdownEffect`. Already in the set.

**The union, closed under `extends`: 27 leaves, 34 classes, 71 bodies (56 `AION_UNPORTED` + 15 undeclared), 2,269 Java LOC.**

| Chunk | Classes | Names |
|---|---|---|
| **P5-03** (A-L), 14 | | `AbstractHealEffect`, `AbstractOverTimeEffect`, `AlwaysDodgeEffect`, `AlwaysResistEffect`, **`ArmorMasteryEffect`**†, **`BleedEffect`**†, `BufEffect`, `DamageEffect`, **`EffectTemplate`** (26 bodies), `EscapeEffect`, `HealEffect`, `HealInstantEffect`, `HealOverTimeEffect`, `HideEffect` |
| **P5-04** (M-Z), 20 | | **`PolymorphEffect`**†, `ProvokerEffect`, `ReflectorEffect`, `ReturnEffect`, `RootEffect`, `SanctuaryEffect`, `ShieldEffect`, **`ShieldMasteryEffect`**†, **`SkillAttackInstantEffect`**†, `SlowEffect`, `SnareEffect`, **`SpellAttackEffect`**†, `SpellAttackInstantEffect`‡, `StatboostEffect`‡, **`StatdownEffect`**†, **`StatupEffect`**†, `StunEffect`, `TransformEffect`, `WeaponDualEffect`, **`WeaponMasteryEffect`**† |

† shell header only: needs a **header request** (an `override` declaration) plus a new `.cpp`. ‡ data-only: nothing to write.
That is **11 of the 34** that carry no `AION_UNPORTED` site today and are nevertheless unported — the §2.2 item-2 effect in miniature.

**The level-1 skills this buys, and what each one proves.** Each class autolearns its own at character creation — confirmed against
`oracle.py m5a-creation` for all four — so the gate needs **no learn path, no trainer dialog and no levelling**. A "–" below means the field was
not measured, not that it is absent.

| Class | Skill | Cast | Cost | Effects | What it makes observable |
|---|---|---|---|---|---|
| WARRIOR | **2864 Ferocious Strike** | instant (`duration="0"`) | none | `skillatk value=27` | an instant skill, a chain skill (`<chain category="W_CHAINA_1TH_1"/>`), a weapon start-condition |
| MAGE | **1282 Flame Bolt** | **2,000 ms** | **19 MP** | `spellatkinstant value=141 element=FIRE` | **a cast bar**, MP consumption, cast interruption, magical damage |
| MAGE | **1328 Root** | instant | 38 MP | `root duration2=20000` | **a 20-second debuff** in the `DEBUFF` slot with a dispel category |
| PRIEST | **1838 Healing Light** | 2,000 ms | 13 MP | `healinstant value=110` | a heal (HP rises), `TARGETORME` first-target |
| PRIEST | **4012 Smite** | – | – | `spellatkinstant` | magical damage from a second class |
| SCOUT | **3195 Focused Evasion** | instant | none | `alwaysdodge` + `alwaysresist`, both `duration2=5000` | **a two-entry self-buff** with a 5-second lifetime |
| SCOUT | **3182 Swift Edge** | instant | none | `skillatk` | physical skill damage |
| **all four** | **243 Return**, **245 Bandage Heal**, **302 Escape** | – | – | `ReturnEffect`, `HealInstantEffect`, `EscapeEffect` | the three class-less autolearn skills every character has; `Return` and `Escape` are also the two a real player presses when a fight goes wrong |
| any | **8291 Soul Sickness** (PROVOKED) | – | – | 3 × `statdown` on MAXHP/MAXMP/SPEED, `duration2=40000 duration1=20000` | the revive debuff, and **a visible change in `SM_STATS_INFO`** |

The oracle confirms the set and the resources: `oracle.py m5a-creation --race ELYOS --class MAGE` returns skills
`40, 100, 103, 243, 245, 302, 1282, 1328, 30001` with `baseStats.maxMp = 405`, so a level-1 Mage can pay *Flame Bolt*'s 19 MP and *Root*'s 38 MP
many times over; a Warrior gets `37, 39, 40, 41, 42, 43, 103, 140, 243, 245, 302, 2864, 30001` with `maxHp 284 / maxMp 170`.

**Everything a cast bar, a buff icon and a debuff icon need is at character level 1.** That is the reason §9 recommends stopping the effect
subset at the starting classes rather than at level 25.

### 2.5 The magical half (O-02) — **4 bodies, and it should land first**

m5b-plan.md O-02 lists eight names as one item. They are not one item.

| Body | Needs | Verdict |
|---|---|---|
| `AttackUtil::calculateMagAttackResult` | `calculateMagicalStatus`; then `StatFunctions::calculateAttackDamage`, `adjustDamageByStatModifiers`, `amplifyDamageByAdditionalHitCount`, `modifyDamageByNpcAi`, `ObserveController::checkShieldStatus` — **all five ported at HEAD** | **auto-attack half** |
| `AttackUtil::calculateMagicalStatus` | `StatFunctions::calculateMagicalResistRate`, `calculateMagicalCriticalRate` | **auto-attack half** |
| `StatFunctions::calculateMagicalResistRate` | `ObserveController::checkAttackStatus` (ported), `getMResist`/`getMAccuracy` (ported), `limit` (ported) | **auto-attack half** |
| `StatFunctions::calculateMagicalCriticalRate` | `getMCritical`/`getMCR` (ported) | **auto-attack half** |
| `AttackUtil::calculateSkillResult` | `Effect&`, `DamageEffect*` in the signature | effect engine |
| `AttackUtil::calculateEffectResult` | `Effect&` | effect engine |
| `AttackUtil::calculateMagicalOverTimeSkillResult` | `Effect&`, `EffectTemplate*`, `EffectTemplate::calculateCritAddDmg` | effect engine |
| `AttackUtil::calculatePhysicalStatus(…, const EffectTemplate*, Effect&)` | `EffectTemplate::getAccMod2`, `calculateCritProbMod`, `SkillAttackInstantEffect::isCannotmiss` | effect engine |
| `StatFunctions::calculateMagicalSkillDamage` | `EffectTemplate*` | effect engine |

**Answer to the question m5b-client-session.md S-3 raises: the caster unblock is a small piece that can land first.** Four bodies in `P5-01`
(`AttackUtil.cpp` and `StatFunctions.cpp`) plus deleting one stand-in in `P4-11b` (`standins::attackUtilCalculateMagAttackResult`, the only
remaining caller is `CreatureController.cpp`'s `attackTarget`). Nothing in it mentions `Effect`, `Skill` or `EffectTemplate`. It is **stage 0**
of §5, one lane, and until it lands the §10 checklist has to keep saying "melee classes only". The other five bodies take `Effect&` or
`EffectTemplate*` parameters and cannot have a real body before §4's engine items, so they are **inseparable** and belong to stage 1.

### 2.6 Npc abilities

`SkillAttackManager` is a shell of four `AION_PARTIAL` bodies that return the neutral answer (`ai/manager/SkillAttackManager.cpp:19,23,28,40`);
only `afterUseSkill` is really ported, because its two statements need no skill engine. `GeneralNpcAI::chooseSkillAttack` is a second
`AION_PARTIAL` returning false (`handlers/ai/GeneralNpcAI.cpp:122`), so `chooseAttackIntention` always answers `SIMPLE_ATTACK` and
`SkillAttackManager` is unreachable — which is why the M5b-1 allow-list asserts its four sites are hit **exactly zero times** (§B) while
`chooseSkillAttack` is asserted to be hit **at least once** (§A: it runs on every attack decision whatever the data holds).

Reopening it needs, in order: `NpcSkillTemplateEntry::conditionReady` and `fireOnEndCastEvents` (2 `AION_UNPORTED`, `model/skill/NpcSkillTemplateEntry.cpp:78,115`),
the four `SkillAttackManager` bodies (215 Java lines), `NpcGameStats::canUseNextSkill`/`setNextSkillDelay`/`setLastSkill` (check before porting —
M5b-1's B-05 ported `getNextAttackInterval` and `getInitialSkillDelay`, not necessarily these), and `CreatureController::useSkill`, which is
**already ported** and calls `SkillEngine::getSkill`.

**One risk the reopening carries, and it is the shape of m5b-client-session.md S-1.** `CreatureController::useSkill` wraps its body in
`catch (const std::exception& ex) { log.error("Exception during skill use: " + …, ex); }` (`CreatureController.cpp:473-484`, Java
CreatureController.java:455-...). Any unported body the npc skill path reaches becomes a silent ERROR line per attack rather than a crash —
exactly the `registerDrop` failure mode the first real-client session found. The gate's "no ERROR line" check (Q1) is what turns it back into a
failure, and it must not be relaxed.

**Effect coverage is not the blocker here.** §2.4(b) shows the two start maps' 29 npc skill ids need 8 effect classes, six of them already in the
player subset. The blocker is the manager and the entry bodies, ~260 Java lines.

### 2.7 The client packets

| Packet | Java | C++ | Chunk | Needed by |
|---|---|---|---|---|
| `CM_CASTSPELL` | CM_CASTSPELL.java, 110 lines | **missing** | **P5-15** (`CM_[A-K]*`) | **required** — it is the only way a player starts a cast |
| `CM_USE_CHARGE_SKILL` | 32 lines | **missing** | **P5-16** | optional: charge skills need `ChargeSkill` (no C++ file, 44 Java lines) and the `chargeSkillGetAndUse` stand-in. No level-1..10 starting-class skill is a charge skill (`skill_charge.xml`), so this is **W** |
| `CM_TOGGLE_SKILL_DEACTIVATE` | 43 lines | **missing** | **P5-16** | required for toggles/stances; no starting-class skill ≤ 10 is a toggle, so **W** for the gate and **R** for the real client |
| `CM_REMOVE_ALTERED_STATE` | 44 lines | **missing** | **P5-16** | required: the client's "right-click a buff icon to cancel it" path; it reaches `EffectController::removeEffect`. **R** — it is cheap and it is the only client-driven way to end an effect early |
| `CM_SKILL_ACTIVATION`, `CM_LEARN_SKILL`, `CM_MOTION_CHECK` | – | no Java class in 4.8 / not needed | – | not applicable |

**Server packets: none needed.** All 20 packets the engine sends exist with 0 `AION_UNPORTED` (§1). What M5b-2 does need is **independent
decoders** for the gate (m5a-plan.md D9): `SM_CASTSPELL`, `SM_CASTSPELL_RESULT`, `SM_SKILL_CANCEL`, `SM_ABNORMAL_STATE`, `SM_ABNORMAL_EFFECT`,
`SM_SKILL_COOLDOWN`, `SM_STATUPDATE_MP` — written from the Java `writeImpl`, in a new `tests/scenario/decoders/SkillDecoders.{h,cpp}` beside
`CombatDecoders` (item G-02).

The wire shapes the gate reads:
- `SM_CASTSPELL` (SM_CASTSPELL.java:46-76): `writeD(effectorObjectId)`, `writeH(spellId)`, `writeC(level)`, `writeC(targetType)`, the three
  target arms, then **`writeH(castDuration)`** and a `writeC(0)` — `castDuration` is the cast bar, and it is a `writeH`, so it saturates above
  65,535 ms.
- `SM_CASTSPELL_RESULT` (SM_CASTSPELL_RESULT.java:51-...): effector, `targetType`, the target arm, `skillId`, `lvl`, `cooldown`, `hitTime`, then
  **16** when the effect list is empty, **32** on a successful chain, 0 otherwise — the three states the client draws differently.
- `SM_ABNORMAL_STATE` (SM_ABNORMAL_STATE.java:25-38): `abnormals`, `0`, `0`, `slot`, `effects.size()`, then per effect
  `effectorId`, `skillId`, `skillLevel`, `targetSlot.ordinal()`, **`remainingTimeToDisplay`** — the buff and debuff icons with their timers.

---

## 3. Where this plan disagrees with m5b-plan.md §9

| m5b-plan.md §9 | This plan | Why |
|---|---|---|
| "~450" unported sites | **448** measured (436 in P5-02/03/04 + 9 in P5-01 + 3 stand-ins) — **§9's total was right**, its P5-02 figure was not | M5b-1 closed 7 of P5-02's 269; P5-03/P5-04 unchanged |
| P5-02 "269 bodies" | **262** | same |
| "~17,800 Java LOC" | **17,835** | confirmed |
| implicitly, sites == work | **~521 bodies**: 436 sites + 45 undeclared effect methods + 4 `Properties` + 36 in 11 classes with no C++ file | §2.2 item 2; `header-requests.md` shells-2 already calls it an open issue |
| "M5b-2 … P5-01 (magical half)" as one item | **4 + 5 bodies**, separable | §2.5 |
| "2 waves, probably 3" | **4 stages**: a 1-lane stage 0 (~2 days), a 6-lane stage 1, a 4-lane stage 2, a 2-lane stage 3 | §6, §9 |
| D4: "neither of the two gate monsters has any entry in `npc_skills.xml`" | true of **210663**; **32 Poeta npc ids do**, one of them level 1 at 77 m | §2.4(b) — this is what gives the gate an npc-skill target |
| §2.4: "Poeta spawns **149** distinct npc ids over **1,031** spots" | **147 over 1,029** | two `<spawn>` blocks are inside XML comments (`210010000_Poeta.xml:378-381`, "Ellino (Fast Track Spawn)" 801032 and 801033). A raw grep counts them, the parser does not, and the server does not spawn them. The derived "130 of 149 = 87 %" coverage figure moves with it |
| D3/risk 13: "185 distinct npc ids … 13 of the 25 XML files" | **199 entries in 12 files, 33 distinct skill ids, 491 npc ids once `npc_ids` lists are expanded** | different denominators; the oracle should settle it (§12) |
| risk 7: "`cycles.toml` … has **none** for the AI" | still true, **and it is also true for M5b-1's own tasks** | `cycles.toml`'s last commit is `ecbca4f21`, *before* M5b-1. No row was added for `AttackManager`'s scheduled lambdas, `SimpleAttackManager.SimpleCheckedAttackAction` or `AggroEventHandler.AggroNotifier`. Nothing failed, because the unresolved edges all live under `data/handlers` and CTest runs `--cycles=core`, which skips them. **The effect system is the opposite case: 55 rows already exist** (§6) |

---

## 4. Decisions

**Accepted by the user, 2026-09-23: stage 0 runs FIRST, then D1's manifest split.** D2 to D10 stand as written. The order matters for a
reason beyond sequencing: stage 0 makes Mage, Priest and Spiritmaster characters able to auto-attack at all (m5b-client-session.md S-3),
so the user can play a caster before abilities exist. It has no dependency on the effect engine and does not touch P5-02, so it is also the
last work that can land cleanly before the chunk is split.

| # | Decision | Why |
|---|---|---|
| **D1** | **P5-02 is split in the manifest into P5-02a and P5-02b, two parts sharing the target `aion_gs_skills`, along the `Skill` \| `Effect` seam.** P5-02a: `skillengine/{SkillEngine.*, model/{Skill*,Chain*}, properties, condition, action, change, periodicaction, task}`. P5-02b: `skillengine/model/Effect*`, `skillengine/effect/modifier`, `controllers/effect/**`, `model/skill/**`. **The integrator decides this before stage 1 starts; it is a manifest change, not a precedent.** | P5-02 holds **262 of the three skill chunks' 436 sites and 9,505 of their 17,835 Java LOC in one chunk**, and a chunk is the unit of ownership: as the manifest stands, the core of M5b-2 is one lane for the whole milestone and the two halves that could be written in parallel cannot be. The pattern exists: **P4-07a and P4-07b share `aion_gs_templates`** (`chunks.cmake:139-151`), and P5-05 already has two parts. The seam is the natural review seam too: `Skill` is the cast state machine, `Effect` is the application and lifetime. Split sizes: **P5-02a ≈ 113 sites + 21 new bodies, P5-02b ≈ 150 sites.** The alternative — one lane over three stages, or file leases under I-03 — is honest but costs the milestone a stage. |
| **D2** | **Closing `SkillEngine::applyEffectDirectly` (M5a O-09) changes every character's stats, so `gs.scenario.m5a`, `gs.scenario.m5b` and `oracle.py m5a-creation` must be re-measured in the same wave.** This is M5b-2's D2, the analogue of M5b-1's D2 (registering `GeneralNpcAI`). | The site is hit **8 times per enter world** (`m5b_partial_allowlist.txt` §A). Those 8 are a level-1 Elyos Warrior's passives: 37/39 `WeaponMasteryEffect` (`<change stat="PHYSICAL_ATTACK" func="PERCENT" value="16"/`, `20`), 40/41/42/103 `ArmorMasteryEffect` (`PHYSICAL_DEFENSE` +10 %), 43 `ShieldMasteryEffect` (`DAMAGE_REDUCE` 0 %), 140 `StatboostEffect`. **`oracle.py m5a-creation` states the assumption in its own docstring** — `m5a/creation.py:160`: "the starting gear carries no MAXHP modifier, **passive skill effects are not applied**, and the HEALTH stat is the class value". The M5a gate's V9 asserts the oracle's base HP/MP against every `SM_STATS_INFO` of the enter-world burst (`M5aScenarioTest.cpp:1568-1602`). **On the evidence, V9 survives**: none of the eight touches `MAXHP`, `MAXMP` or `ATTACK_SPEED`, and `getMaxHp().getBase()` is a base, not a bonus. But nobody has run it, the character's damage per hit *will* change, and a wave that lands O-09 and does not re-run both gates leaves them unverified. **Item G-05.** |
| **D3** | **The gate seeds `player_skills` rows to give a character a skill, exactly as m5b-plan.md D12 seeds `player_life_stat.hp`.** | `player_skills` is `(player_id, skill_id, skill_level)` and `PlayerSkillListDAO` is **fully ported** (`dao/PlayerSkillListDAO.cpp:26-49`, 0 unported). `SkillEngine.getSkillFor` gates on `skillList.isSkillPresent(skillId)` (SkillEngine.java:58-60), which is exactly what a seeded row satisfies. This buys the gate any skill it wants without the item path (M5b-3), the trainer dialog or levelling — and it keeps the scripted path away from `SkillLearnService`, which is not in scope. |
| **D4** | **The gate creates a second character, an Elyos `MAGE`, beside the M5b-1 Elyos `WARRIOR`.** | A level-1 Warrior's only active skill, 2864 *Ferocious Strike*, has `duration="0"` and no `<mp>` end-condition: **it can prove neither a cast bar nor MP consumption nor cast interruption.** A level-1 Mage has 1282 *Flame Bolt* (2,000 ms, 19 MP) and 1328 *Root* (a 20-second debuff). The M5a gate already creates characters of both races and the harness's `buildCM_CREATE_CHARACTER` takes a class id (`M5aScenarioTest.cpp:1315-1316` asks the oracle for `("ELYOS","WARRIOR")` and `("ASMODIANS","MAGE")`), so this is a second character on the same account, not new machinery. **It also makes the gate depend on stage 0** (a Mage cannot auto-attack until §2.5 lands), which is a feature: the gate then proves S-3 is closed. |
| **D5** | **`gameserver.soulsickness.disable=0` leaves the M5b-2 profile, and the revive debuff becomes an assertion.** | m5b-plan.md D1 added the key only because `SkillEngine::getSkill` threw and killed the revive. Skill 8291 uses **only `StatdownEffect`**, which §2.4 ports. Removing the key restores Java's own behaviour and gives the gate a free, exactly-specified effect: `-30 % MAXHP`, `-30 % MAXMP`, `-50 % SPEED` for `40000 + 20000 × deathCount` ms, observable as `SM_ABNORMAL_STATE` and as a changed `maxHp` in `SM_STATS_INFO`. **A gate that still needs the key has not finished.** |
| **D6** | **The effect subset is the 34 classes of §2.4 and no others. Every other effect class stays `AION_UNPORTED` and therefore throws.** No blanket `AION_PARTIAL` is added to the rest. | `AION_PARTIAL` is for a body whose neutral answer is safe; an effect class's `applyEffect` has no safe neutral answer — a silently skipped debuff is a wrong fight. Leaving them `AION_UNPORTED` means the first skill outside the subset fails loudly, in `unported_trace.txt`, instead of quietly. The cost is that the real-client checklist must stay inside the starting classes and the two start maps (§10), and that is the honest boundary of this milestone. |
| **D7** | **`NpcSkillList::getPostSpawnSkills` (D3) closes in M5b-2**, because §2.4(c) measured its effect surface at 10 classes, 3 of them new. `GeneralNpcAI::chooseSkillAttack` and `SkillAttackManager` (D4) close in **stage 2**. `SkillEngine::createCriticalProcEffect`'s `id != 0` arm and `EffectController::isUnderNormalShield` (D14) close in **stage 1**, together, as m5b-plan.md D14 promised. | Three of the four M5b-1 partials are M5b-2's to close. The fourth, `DropRegistrationService::registerDrop` (D5), is M5b-3's. Each closure **removes a row from `m5b_partial_allowlist.txt`**, and the M5b-1 gate's Q1 asserts §A rows are hit *at least once* — so removing a row and forgetting the allow-list turns the M5b-1 gate red. That is deliberate and it is what G-05 checks. |
| **D8** | **Damage from a skill is asserted as bounds and invariants; the effect's *duration*, *slot*, *MP cost* and *cast duration* are asserted exactly.** | m5b-plan.md D6's reasoning carries over for damage (`Rnd` lives in a child process). But `duration2`, `tslot`, the `<mp>` end-condition value and `castDuration` are template constants with no randomness: `Root` is 20,000 ms in the `DEBUFF` slot, *Flame Bolt* costs exactly 19 MP and shows a 2,000 ms bar before any cast-speed modifier. Those are the assertions that catch a wrong effect engine, and they are exact. |
| **D9** | **Faithfulness beats a nicer engine.** The `Effect`/`EffectController` cycle hooks port exactly as Java writes them, with the `cycles.toml` rows that already exist as the specification, and `// java-race` where Java races. | `cycles.toml:273-313` resolves **40 effect edges** as `java-hook: Effect.endEffect -> removeObservers …` and `java-hook: Effect.stopTasks cancels the periodic task held in Effect.periodicTasks`. Those rows were written and reviewed at the S0b/S0c freeze against the Java code; a body that "improves" the hook invalidates a reviewed resolution. §8 item 1. |
| **D10** | **The M5b-2 gate is a new test `gs.scenario.m5b2` (+ `_geo`) with its own schema pair, output directory and allow-list, in the same binary and under the same `RESOURCE_LOCK`.** The M5b-1 gate is **kept and re-run**, not replaced. | m5b-plan.md D10's argument, one milestone later. Keeping `gs.scenario.m5b` is what makes D2 and D7 checkable: the melee fight, the experience and the respawn must still be right after the effect engine lands. The cost is a fourth serialized server run on the user's machine; §8 risk 12 budgets it. |
| **D11** | **Taken by the integrator under the standing instruction, 2026-09-23.** **The O-09 and D7 closures that stage 1 part 2 ported are held back behind their old `AION_PARTIAL`s until part 3 lands the effect classes they reach; part 3 removes the holdbacks in the same commit that ports those classes.** `SkillEngine::applyEffectDirectly(const SkillTemplate*, …)` (O-09) and `NpcSkillList::getPostSpawnSkills` (D7) keep their ported paths behind the partial, and the allow-list rows moved with them (`SkillEngine.cpp:137`, `NpcSkillList.cpp:91`). | The part 2 reviews proved both closures reach unported leaves at once. O-09: every enter-world passive reaches `BufEffect::applyEffect`, and `PlayerEnterWorldService::activatePassiveSkillEffects` does not catch, so no character could enter the world. D7: 46 post-spawn npcs in `220070000_Gelkmaros.xml` and `220080000_Enshar.xml` cast statup and hide skills synchronously from `onSpawn`, so every server start would log an ERROR and delete them. This is the M5b-1 rule of keeping every commit green, applied to a join the plan had placed inside one stage. |
| **D12** | **Taken by the integrator under the standing instruction, 2026-09-23.** **Part 3 gains item F-06, a P5-01 lane for the five effect-engine attack bodies that §2.5 moved to stage 1 but no stage 1 item owned:** `AttackUtil::calculateSkillResult`, `calculateEffectResult`, `calculateMagicalOverTimeSkillResult`, `calculatePhysicalStatus(…, const EffectTemplate*, Effect&)` and `StatFunctions::calculateMagicalSkillDamage`. | §2.5 calls them "inseparable and belong to stage 1", and §5 has no item for them. Every damage skill of the gate (2864, 1282) runs `DamageEffect::calculateDamage` into them, so without F-06 part 3 would port `DamageEffect` into a wall. |
| **D13** | **Taken by the integrator under the standing instruction, 2026-09-23.** **Part 3 also ports four small classes outside section 2.4's 34, because part 2 made them reachable in live play:** `StumbleEffect` and `StaggerEffect` (P5-04, 4 bodies each), which a critical auto-attack with a POLEARM, STAFF, GREATSWORD (8218) or BOW (8217) reaches through `SkillEngine::createCriticalProcEffect`, and `MPHealEffect` (4) and `ProcVPHealInstantEffect` (1), which `CuringZoneService` reaches once a second for every player near a curing object (skill 8751, maps 700010000 and 710010000). | M5b-1's `createCriticalProcEffect` partial answered null for those weapons, so part 2 turned a quiet no-op into a throw out of `CreatureController::attackTarget` on about 10 % of critical hits for every class that changes to Gladiator, Templar, Chanter or Ranger at level 10. The four classes are 234 Java lines together. Godstone procs and material skills also reach effect classes outside the set; they wait for the item milestone (M5b-3), which is where godstones come from. The full table is in docs/deviations/P5-02a.md, "Reachable after part 2". |

---

## 5. Work items

Effort: **S** < 1 agent-day, **M** 1-2, **L** 2-4, **XL** > 4. Need: **R** required, **W** stub-with-warning allowed, **O** optional.
Owner is the chunk (after D1's split where it applies); the lane is in §6.

### Integrator

| Id | What | Owner | Deps | Need | Eff |
|---|---|---|---|---|---|
| I-01 | **D1's manifest change**: split P5-02 into P5-02a/P5-02b sharing `aion_gs_skills`, and add the test directories `tests/effects_al` (P5-03) and `tests/effects_mz` (P5-04), which do not exist today and are *derived*, not declared, names (`chunks.cmake` header §12). `tests/skills` exists and goes to P5-02a; P5-02b needs a `TESTS` keyword or its own directory. **Decide before stage 1 starts; the plan must not assume it.** | manifest | – | R | S |
| I-02 | The header-request batch of §7, decided before stage 1 starts. It is larger than M5b-1's: **~30 shell headers gain `override` declarations** (the shells-2 open issue) and **8 new headers** appear in `skillengine/properties/` and `skillengine/model/`. | manifest | – | R | M |
| I-03 | On demand: file leases for cross-chunk fixes (one active lease per chunk, released at merge). Two are foreseeable: the npc-abilities lane needs `model/skill/NpcSkillTemplateEntry.*` (P5-02b), and the gate lane needs nothing outside P5-SC. | manifest | – | O | S |

### Stage 0 — the magical auto-attack (P5-01, P4-11b)

| Id | What | Java refs | Deps | Need | Eff |
|---|---|---|---|---|---|
| **M-01** | `StatFunctions::calculateMagicalResistRate` and `calculateMagicalCriticalRate` (`utils/stats/StatFunctions.cpp`). Both read only ported stat accessors and `limit`. | StatFunctions.java:420-433, 609-626 | – | R | S |
| **M-02** | `AttackUtil::calculateMagicalStatus` and `calculateMagAttackResult` (`controllers/attack/AttackUtil.cpp`). Every callee but M-01 is ported. | AttackUtil.java:417-425, 494-506 | M-01 | R | S |
| **M-03** | Delete `standins::attackUtilCalculateMagAttackResult` (`controllers/ControllerStandIns.{h,cpp}`) and call `AttackUtil::calculateMagAttackResult` directly from `CreatureController::attackTarget`. **A signature header request** (§7). | ControllerStandIns.h | M-02 | R | S |
| **M-04** | Tests in `tests/stats`: golden vectors for `calculateMagAttackResult` with a seeded `Rnd` (`Rnd::seedCurrentThreadForTests`), the resist-rate level-difference table (`levelDiff > 4` → `+100` per level), the PvP `min(500, …)` clamp, and the `Servant`/`Homing` false arm of the critical rate. Mutation-proven: drop the `levelDiff` term, drop the clamp, swap `MResist`/`MAccuracy` — name the test that goes red for each. | – | M-01, M-02 | R | M |
| **M-05** | Extend `gs.scenario.m5b` (or a `tests/controllers` case) with a **Mage** that auto-attacks a monster, so S-3 is closed by a test and not by a claim. The cheapest honest form: a `tests/stats` + `tests/controllers` pair, plus one line in the §10 checklist. If the gate lane is already open, D4's Mage does it for free. | m5b-client-session.md S-3 | M-03 | R | S |

### Stage 1, the cast machine (P5-02a)

| Id | What | Java refs | Deps | Need | Eff |
|---|---|---|---|---|---|
| **S-01** | `SkillEngine` — the 14 `AION_UNPORTED` bodies: `getSkillFor` ×3, `getSkill` ×2, `getChargeSkill`, `getPenaltySkill`, `applyEffectDirectly` ×3 (+ the O-09 partial), `applyEffectsDirectly`, `applyEffect` ×2, `checkAndGetSkillTemplate`. Plus **closing the O-09 partial at `:79`** (D2) and **`createCriticalProcEffect`'s `id != 0` arm and the `isUnderNormalShield` guard at `:138`** (D7/D14), with the `docs/deviations/P5-02.md` rows the M5b-1 lane left pointing at M5b-2. | SkillEngine.java:39-227 | S-02, K-01 | R | M |
| **S-02** | `Skill` — the 48 unported bodies: the three `useSkill` entry points and the private `useSkill(checkAnimation, checkProperties)`, `canUseSkill` ×2, `isValidTarget`, `validateEffectedList`, `setCooldowns`, `getCooldown`, `updateCastDurationAndSpeed`, `calculateCastDuration`, `calculateMagicalCastDuration`, `calculateChargeCastDuration`, `getSkillCastBoostStat`, `updateHitTime`, `getDistanceTolerance`, `isSuspiciousClientHitTime`, `collectUncertaintyFactorsForHitTime`, **`startCast`**, `cancelCast`, `cancelCurrentSkillCast`, **`endCast`** (146 Java lines), `removeObservers`, `applyEffect`, `addResistedEffectHateAndNotifyFriends`, `sendCastSpellEnd`, `payCastCosts`, `preCastCheck`, `preUsageCheck`, `endCondCheck`, `startPenaltySkill`, `isInvalidRecall`, `isHostile`, and the 18 template predicates. | Skill.java:262-1095 | S-03, S-04 | R | XL |
| **S-03** | **`skillengine/properties/` — 7 new files and one shell to fill.** `FirstTargetProperty` (173), `TargetRangeProperty` (173), `TargetRelationProperty` (85), `FirstTargetRangeProperty` (76), `MaxCountProperty` (46), `TargetStatusProperty` (34), `TargetSpeciesProperty` (18); and `Properties`'s four behaviour methods (`validate`, `endCastValidate`, `validateEffectedList` ×2) plus the nested `ValidationResult`, which the shell header does not declare. 907 Java LOC. **This is the block that decides whether a skill hits the right creature**, and it has no C++ file at all today. | Properties.java:70-216 and the seven `*Property.java` | – | R | L |
| **S-04** | `skillengine/condition/` — the 35 unported bodies over 22 files. **11 of them are on the level-1..10 path** (`ChainCondition`, `ChargeArmorCondition`, `ChargeWeaponCondition`, `CombatCheckCondition`, `MpCondition`, `PlayerMovedCondition`, `PolishChargeCondition`, `TargetCondition`, `WeaponCondition`); the rest (`HpCondition`, `DpCondition`, the four flying conditions, `RaceCondition`, `AbnormalStateCondition`, `FormCondition`, `LeftHandCondition`, `ItemChargeCondition`, `RideRobotCondition`, `OnFlyCondition`) are small and should be ported with them rather than left as a second visit. | skillengine/condition/*.java | – | R | M |
| **S-05** | `skillengine/action/` (8 bodies: `MpUseAction`, `HpUseAction`, `DpUseAction`, `ItemUseAction`) and `skillengine/periodicaction/` (2: `HpUsePeriodicAction`, `MpUsePeriodicAction`). **No starting-class skill ≤ 10 has an `<actions>` element** (measured), so these are **W**. **Do not confuse them with the skill cost**: the `<mp>` cost is an *end condition* and `MpCondition.validate` reduces the MP itself (MpCondition.java:31-35), so `MpCondition` in S-04 is what X4 needs, not `MpUseAction`. | skillengine/action/*.java | – | W | S |
| **S-06** | `ChainSkills` (4) and `ChainSkill` (2) — the chain state a player carries between casts. **Required**: the Warrior's 2864 and the Mage's 1282 are both `skill_category="CHAIN_SKILL"` with a `<chain>` start-condition, so `endCast`'s chain arm and `ChainCondition` run on the gate's own path. | ChainSkills.java, ChainSkill.java | S-04 | R | S |
| **S-07** | New files `ChargeSkill` (44 Java lines), `PenaltySkill` (21), `WeaponTypeWrapper` (97). `PenaltySkill` is needed by `Skill.startPenaltySkill`; `WeaponTypeWrapper` by the weapon conditions; `ChargeSkill` only by `CM_USE_CHARGE_SKILL`. **W** for `ChargeSkill`, **R** for the other two. | skillengine/model/*.java | – | W/R | S |
| **S-08** | Tests in `tests/skills`: the cast phase table (instant vs timed, `castDuration` from `updateCastDurationAndSpeed`), `payCastCosts` against a seeded MP value, `Properties.validate` per `FirstTargetAttribute`/`TargetRangeAttribute`/`TargetRelationAttribute` combination that the 75 skills of §2.4 use, `ChainCondition` over a chain window, and the `endCast` ordering (`setCasting(null)` before the effects). The fixture exists: `tests/skills` already builds against `aion_gs_skills` and M5b-1's `CriticalProcEffectTest` and `NpcSkillListTest` live there. | – | S-01..S-07 | R | L |

### Stage 1, the effect machine (P5-02b)

| Id | What | Java refs | Deps | Need | Eff |
|---|---|---|---|---|---|
| **K-01** | `Effect` — the 83 unported bodies. The load-bearing ones: **`initialize`** (72 Java lines, the whole damage-and-status pre-calculation), **`applyEffect`**, **`startEffect`**, **`endEffect` ×2**, `addToEffectedController`, `stopTasks`, `schedulePeriodicActions`, `calculateEffectsDuration`/`calculateTemplateDuration`/`applyCumulativeResistDurationMultiplier`, `setReserveds`/`getReserveds`/`getReservedEffectsToSend`, `rollMagicalCritical`/`reuseMagicalCritical`/`isMagicalCritical`, `broadcastHate`, `shouldApplyFurtherEffects`, `activateToggleSkill`/`deactivateToggleSkill`, `canSaveOnLogout`, `getRemainingTimeToDisplay`, `addObserver` ×2, `removeObservers`, plus ~50 accessors. | Effect.java:126-1212 | – | R | XL |
| **K-02** | `EffectController` — the 41 unported bodies (42 sites): `addEffect`, `put`, `clearEffect`, `removeEffect` ×n, `isConflicting`, `checkDuelCondition`, `broadCastEffectsImp`, `updatePlayerEffectIcons`, `getAbnormals`, `hasAbnormalEffect` (the one `PlayerController::onDie` evaluates lazily, m5b-plan.md §6.3 P3), **`isUnderNormalShield`** (D7/D14's other half), `calculateAndApplyCumulativeResistDuration`, `resetDesignatedDispelEffect`, the dispel family; **plus closing the `removeByDispelSlotType` partial at `:209`** (§C allow-list row) and `PlayerEffectController::addSavedEffect` at `:120`. | EffectController.java (774 lines), PlayerEffectController.java | K-01 | R | XL |
| **K-03** | `CumulativeResist` (5 bodies) — the diminishing-returns table for fear/paralyze/sleep. Reached from `Effect.calculateEffectsDuration` for a player effected by a player, i.e. **not** on the gate's PvE path. **W**, but cheap. | CumulativeResist.java | K-01 | W | S |
| **K-04** | `skillengine/effect/modifier/` — 10 bodies over `AbnormalDamageModifier`, `BackDamageModifier`, `FrontDamageModifier`, `TargetClassDamageModifier`, `TargetRaceDamageModifier`. Read by `EffectTemplate.getModifiers`. None of the 75 skills of §2.4 carries a `<modifiers>` element (measured), so **W**. | effect/modifier/*.java | – | W | S |
| **K-05** | `model/skill/NpcSkillTemplateEntry::conditionReady` and `fireOnEndCastEvents` (2), `PlayerSkillList`'s last body, and **closing `NpcSkillList::getPostSpawnSkills`** (D7, §A allow-list row → the row is deleted). | NpcSkillList.java:70-76, NpcSkillTemplateEntry.java | K-01, F-01 | R | S |
| **K-06** | **`cycles.toml` and `fieldmap.toml` review for the bodies this lane writes.** The 55 existing effect rows are the specification (§6); every one names the exact hook the body must contain. Add rows for anything the report gains, and — a debt M5b-1 left — the rows for `AttackManager`'s scheduled lambdas, `SimpleAttackManager.SimpleCheckedAttackAction` and `AggroEventHandler.AggroNotifier` (m5b-plan.md risk 7; `cycles.toml` has not been touched since `ecbca4f21`). | cycles.toml:273-313 | K-01, K-02 | R | M |
| **K-07** | Tests in `tests/skills` (or a new `tests/effects` if I-01 gives P5-02b its own directory): `Effect.initialize` status table (CONFLICT → DODGE, physical → DODGE/CRITICAL_DODGE, magical → RESIST), the duration arithmetic (`duration2 + duration1 × skillLevel`, the `randomtime` subtraction, the `pvpDuration` percentage), `EffectController` conflict and stacking (`stack` group, `dispel_category`, `req_dispel_level`), `removeByDispelSlotType`, `addSavedEffect` round-trip through `PlayerEffectsDAO`, and **a leak case on a `DeterministicExecutor`/`ManualClock`: start a 20-second periodic effect, let it tick, end it, assert `Effect`, the observers and the stat functions are all reclaimed.** | – | K-01..K-06 | R | L |

### Stage 1, the effect classes (P5-03 and P5-04)

| Id | What | Java refs | Deps | Need | Eff |
|---|---|---|---|---|---|
| **F-01** | **`EffectTemplate` — all 26 bodies** (574 Java lines): `calculate` ×3, `calculateDamage`, `calculateSubEffect`, `calculateHate`, `calculateCritAddDmg`, `calculateCritProbMod`, `startEffect`, `endEffect`, `onPeriodicAction`, `getModifiers`, `getActionModifiers`, the `HitType` roll, the `preeffect` chain. **Owner P5-03** (`effect/[A-L]*`). Everything else in this section depends on it, so it merges first. | EffectTemplate.java:281-500 | – | R | L |
| **F-02** | The **P5-03 half of §2.4's subset**, 13 classes beyond F-01: `AbstractHealEffect`, `AbstractOverTimeEffect`, `AlwaysDodgeEffect`, `AlwaysResistEffect`, **`ArmorMasteryEffect`**†, **`BleedEffect`**†, `BufEffect`, `DamageEffect`, `EscapeEffect`, `HealEffect`, `HealInstantEffect`, `HealOverTimeEffect`, `HideEffect`. † = new `.cpp` **and** a header request. | skillengine/effect/[A-L]*.java | F-01 | R | L |
| **F-03** | The **P5-04 half**, 20 classes: `PolymorphEffect`†, `ProvokerEffect`, `ReflectorEffect`, `ReturnEffect`, `RootEffect`, `SanctuaryEffect`, `ShieldEffect`, `ShieldMasteryEffect`†, `SkillAttackInstantEffect`†, `SlowEffect`, `SnareEffect`, `SpellAttackEffect`†, `SpellAttackInstantEffect`‡, `StatboostEffect`‡, `StatdownEffect`†, `StatupEffect`†, `StunEffect`, `TransformEffect`, `WeaponDualEffect`, `WeaponMasteryEffect`†. † = new `.cpp` + header request; ‡ = nothing to write. | skillengine/effect/[M-Z]*.java | F-01 | R | L |
| **F-04** | The **header-request batch for the 11 shells**, filed before F-02/F-03 start: `override` declarations for `startEffect`, `endEffect`, `calculate`, `onPeriodicAction`, `resolveMagicalCritical`, `removeEffect` on `ArmorMasteryEffect`, `BleedEffect`, `PolymorphEffect`, `ShieldMasteryEffect`, `SpellAttackEffect`, `StatdownEffect`, `StatupEffect`, `WeaponMasteryEffect` (+ the 18 further shells the milestone does not port, filed together or left). **Additive** — the virtuals already exist on `EffectTemplate`, so no vtable slot is added — which makes it a same-day batch under hub-headers.md §14. | header-requests.md shells-2 | I-02 | R | S |
| **F-05** | Tests in the new `tests/effects_al` / `tests/effects_mz`: per class, one case that drives `calculate` → `applyEffect` → `startEffect` → `endEffect` against a fabricated `Effect`, asserting the stat function added and removed, the packet sent, and the duration. **The two directories do not exist**; I-01 creates them. Mutation-proven for the seven that carry arithmetic (`DamageEffect`, `SkillAttackInstantEffect`, `SpellAttackEffect`, `BleedEffect`, `AbstractHealEffect`, `ShieldEffect`, `AbstractOverTimeEffect`). | – | F-01..F-04 | R | L |
| **F-06** | **Added by D12.** The five P5-01 bodies of §2.5 that take an `Effect&` or `EffectTemplate*`: `AttackUtil::calculateSkillResult`, `calculateEffectResult`, `calculateMagicalOverTimeSkillResult`, `calculatePhysicalStatus(…, const EffectTemplate*, Effect&)`, `StatFunctions::calculateMagicalSkillDamage`. Tests in `tests/stats`, golden vectors checked against the Java arithmetic, float exactness per §8 risk 11. | AttackUtil.java, StatFunctions.java | F-01, K-01 | R | M |

### Stage 1, the player side (P5-13, P5-15, P5-16, P4-11b)

| Id | What | Java refs | Deps | Need | Eff |
|---|---|---|---|---|---|
| **P-01** | **`PlayerRestrictions::canUseSkill`** — close the `AION_PARTIAL` at `restrictions/PlayerRestrictions.cpp:60` (65 Java lines). Deletes the §B allow-list row of `m5b_partial_allowlist.txt`. | PlayerRestrictions.java:51-116 | S-02 | R | S |
| **P-02** | **`CM_CASTSPELL`** (P5-15) with its `readImpl` byte vectors: `readUH` skill id, `readUC` level, `readUC` targetType and the four arms, `readUH` hitTime, `readD` unk. Plus the `AION_CLIENT_PACKET(CM_CASTSPELL);` marker and a `tests/cm_ak` byte-vector test. | CM_CASTSPELL.java:36-109 | P-01 | R | S |
| **P-03** | `CM_REMOVE_ALTERED_STATE` (P5-16) — the client's cancel-a-buff path into `EffectController`. `CM_TOGGLE_SKILL_DEACTIVATE` and `CM_USE_CHARGE_SKILL` are **W** (no level-1..10 starting-class skill is a toggle or a charge skill). | CM_REMOVE_ALTERED_STATE.java | K-02 | R / W | S |
| **P-04** | Delete `standins::playerRestrictionsCanUseSkill` and `standins::chargeSkillGetAndUse` from `controllers/ControllerStandIns.{h,cpp}` and call the real bodies from `PlayerController::useSkill` and `CreatureController::useChargeSkill`. **Signature header request.** With M-03 this leaves 8 stand-ins, none of them on a skill path. | ControllerStandIns.h | P-01, S-07 | R | S |
| **P-05** | Tests: `tests/cm_ak` byte vectors for `CM_CASTSPELL` and an in-process run test over `InWorldPacketRunSupport.h`; `tests/instance` for `PlayerRestrictions::canUseSkill`'s decision table (casting, stance, silence, `isSkillDisabled`, resurrect). | – | P-01..P-04 | R | M |

### Stage 2, npc abilities (P5-05, + a P5-02b lease)

| Id | What | Java refs | Deps | Need | Eff |
|---|---|---|---|---|---|
| **N-01** | `SkillAttackManager` — the four `AION_PARTIAL` bodies (`performAttack`, `skillAction`, `cantUseSkill`, `chooseNextSkill`), 215 Java lines. Deletes four §B allow-list rows. | SkillAttackManager.java:20-114 | K-05, S-01 | R | M |
| **N-02** | `GeneralNpcAI::chooseSkillAttack` — close the `AION_PARTIAL` at `handlers/ai/GeneralNpcAI.cpp:122`. Deletes the §A allow-list row, **which is an assertion change**: the site disappears instead of being hit. | GeneralNpcAI.java:130-138 | N-01 | R | S |
| **N-03** | `NpcGameStats` skill bookkeeping if M5b-1 left any: `canUseNextSkill`, `setNextSkillDelay`, `setLastSkill`, `getLastSkill`. **Verify at branch time** — M5b-1's B-05 ported `getNextAttackInterval` and `getInitialSkillDelay`, and this plan did not re-check the rest. | NpcGameStats.java:123-158 | – | R | S |
| **N-04** | Tests in `tests/ai`: `chooseAttackIntention` returns `SKILL_ATTACK` for an npc with a ready skill and `SIMPLE_ATTACK` for one without; the `cantUseSkill` table; the `AISubState::CAST` enter/leave pair around a cast; `afterUseSkill` resuming the attack loop. The fixture exists (`tests/ai/AiTestSupport.h`, `AiWorldTestSupport.h`). | – | N-01..N-03 | R | M |

### Stage 2, the gate (P5-SC)

| Id | What | Deps | Need | Eff |
|---|---|---|---|---|
| **G-01** | `tools/oracle` command **`m5b2-skills --race --class [--level]`**: the autolearn skill ids and levels of a fresh character (extend `m5a/creation.py`, which already computes them), and per skill id the template's `skilltype`, `activation`, `duration` (the cast bar), `cooldown`, the `<mp>`/`<hp>`/`<dp>` end-condition values, the `<effects>` element names with their `duration1`/`duration2`/`e`/`tslot`, and the **effect-class names** through `Effects.java`. Plus `--npc <id>` for an npc's skills. Plus `tools/oracle` tests. **This is what lets the gate assert exact numbers without hardcoding them**, and it is also the tool that re-derives §2.4's counts (m5b-plan.md risk 20). | – | R | M |
| **G-02** | `GameSession` builders `buildCM_CASTSPELL(spellId, level, targetType, …)` and `buildCM_REMOVE_ALTERED_STATE`, a `castAndWait(skillId, timeout)` helper, and the new **independent decoders** `tests/scenario/decoders/SkillDecoders.{h,cpp}` for `SM_CASTSPELL`, `SM_CASTSPELL_RESULT`, `SM_SKILL_CANCEL`, `SM_ABNORMAL_STATE`, `SM_ABNORMAL_EFFECT`, `SM_SKILL_COOLDOWN`, `SM_STATUPDATE_MP` (D9: written from the Java `writeImpl`, no `serverpackets/` include, body consumed exactly), with `SkillDecodersTest.cpp`. | – | R | M |
| **G-03** | `TEST(M5b2Scenario, Run)` — the cases of §6, its own output directory `<bin>/scenario/m5b2`, schema pair `aion_{ls,gs}_test_m5b2_<hash>`, the same `RESOURCE_LOCK`, `tests/scenario/m5b2_partial_allowlist.txt`, and the CTest registration `gs.scenario.m5b2` in `ScenarioTests.cmake`. | G-01, G-02, all of stage 1 | R | L |
| **G-04** | `gs.scenario.m5b2_geo`: the same script with `gameserver.geodata.enable=true`. The one thing it can assert that a geo-off run cannot: **a skill cast at a target behind geometry** — `Properties.validate` → `TargetRangeProperty` does **not** call `GeoService.canSee`, but `Skill.endCast`'s `validateEffectedList` and the effect application do reach creature-visibility checks. **Verify this before writing it**; if it turns out geo changes nothing on the cast path, say so and keep the geo gate as a re-run of the whole script, exactly as m5b-plan.md §6.4 did when it found the same gap. | G-03 | R | M |
| **G-05** | **Re-green `gs.scenario.m5b` and `gs.scenario.m5a`** (D2, D7). Four things move: (a) the §A allow-list rows for `SkillEngine.cpp:79`, `NpcSkillList.cpp:90`, `GeneralNpcAI.cpp:122` **disappear** as their partials close, and Q1 asserts §A rows are hit at least once, so the rows must be deleted in the same commit; (b) the §B row `SkillEngine.cpp:138` and the four `SkillAttackManager` rows disappear; (c) `gameserver.soulsickness.disable=0` leaves the M5b profile (D5) and case K8's revive takes Java's real path; (d) the character's passives now apply, so every damage number changes and V9's stat assertions must be re-measured against a re-run `oracle.py m5a-creation` (D2). **Record the before/after numbers in the wave report.** | G-03, N-02 | R | L |
| **G-06** | Extend the stress nightly (m5b-plan.md G-07) with casting clients: 20 `FakeGameClient`s that pull, cast, buff, debuff and die for 30 minutes under ASan. Asserts: 0 reused-id warnings, empty final census, and **0 live `Effect`, `AttackResult` and `EffectReserved`** at the end. This is the run that catches the class of bug §8 item 2 describes, and it is the only place it can be caught. | G-03, G-07 | R | M |
| **G-07** | `CheckOutput` (P5-14): add `Effect`, `EffectReserved`, `Skill` and the effect observers to `zeroLiveClasses()` and the summary rows. **Cross-chunk (P5-14)**: either the owner adds them or the gate lane takes a lease. Note the working tree already has uncommitted `CheckOutput.{h,cpp}` changes from M5b-1's stage-2 lane; rebase before touching them. | E-03 of m5b-plan.md | R | S |

### Deferred

| Id | What | Milestone |
|---|---|---|
| **O-01** | The other **152 effect classes** (150 bodies + 30 undeclared), including everything summons, stigma, transformations, signets, dispels beyond the four the subset needs, polymorph beyond `PolymorphEffect`, and the whole `AbstractAbsoluteStatEffect` family. | M5b-4 / phase 6, as the classes a real player reaches |
| **O-02** | `skillengine/task/CraftingTask` (174 Java lines, no C++ file) — crafting, not combat. | M5b-3 (with the item path) |
| **O-03** | `CM_USE_CHARGE_SKILL` + `ChargeSkill`, `CM_TOGGLE_SKILL_DEACTIVATE` + stances and toggles. | M5b-2 stage 3 or M5b-4 |
| **O-04** | `StatFunctions` PvP half (5 bodies), `PvpService::doReward`, `AbyssPointsService::addAp`. | a PvP milestone |
| **O-05** | Summons and pets (`SummonEffect`, `SummonHomingEffect`, `SummonServantEffect`, `CM_SUMMON_CASTSPELL`, …). | a summons milestone |

---

## 6. Lanes

At most 6 lanes per stage; chunks are disjoint **within** a stage (across stages they may repeat — P5-01 and P4-11b appear in stage 0 and again
in stage 1's player-side lane, which is legal and is the same argument m5b-plan.md §5 made for P4-11b).

| Stage | Lane | Chunks | Items | Tests |
|---|---|---|---|---|
| **0** | **magical-autoattack** | P5-01, P4-11b | M-01..M-05 | `tests/stats`, `tests/controllers` |
| 1 | **cast** | **P5-02a** | S-01..S-08 | `tests/skills` |
| 1 | **effect-core** | **P5-02b** | K-01..K-07 | `tests/skills` (or its own, I-01) |
| 1 | **effects-al** | P5-03 | F-01, F-02, F-04 | new `tests/effects_al` |
| 1 | **effects-mz** | P5-04 | F-03 | new `tests/effects_mz` |
| 1 | **player-side** | P5-13, P5-15, P5-16, P4-11b | P-01..P-05 | `tests/instance`, `tests/cm_ak`, `tests/cm_lz` |
| 1 | **gate-harness** | P5-SC, `tools/oracle` | G-01, G-02 | `tools.oracle`, decoder self-tests |
| 2 | **gate** | P5-SC | G-03, G-04 | `gs.scenario.m5b2`, `gs.scenario.m5b2_geo` |
| 2 | **npc-abilities** | P5-05 (+ a P5-02b lease for `NpcSkillTemplateEntry`) | N-01..N-04 | `tests/ai`, `tests/handlers_ai_core` |
| 2 | **regate** | P5-SC (second lease) *or* serialized in the gate lane | G-05 | `gs.scenario.m5a`, `gs.scenario.m5b`, `gs.smoke.startup` |
| 2 | **fixups** | whichever stage-1 chunks the gate names | the gate's findings | owning chunk tests + gate rerun |
| 3 | **stress + census** | P5-14, P5-SC | G-06, G-07 | `gs.scenario.m5a_stress` (nightly) |
| all | integrator | manifest, runtime, leases | I-01..I-03 | full verification |

**Notes on the lane split.**

- **Stage 1 has six lanes, the cap, and it only fits because of D1.** Without the P5-02 split, `cast` and `effect-core` are the same chunk and
  stage 1 becomes five lanes with a ~240-body serial lane in the middle — the critical path doubles.
- **F-01 (`EffectTemplate`) is the whole section's prerequisite and lives in P5-03.** The `effects-mz` lane cannot start its bodies before F-01
  merges, so either `effects-al` merges F-01 on day 1-2 or the integrator leases `EffectTemplate.*` to a shared start. Say which in the wave
  report; do not discover it on day 4.
- **Merge order in stage 1.** F-04's header batch (day 0, it blocks 11 classes) → F-01 → S-03 and S-04 (Skill cannot compile a real `canUseSkill`
  without `Properties`) → K-01 → S-02 → K-02 → S-01 → P-01/P-02 → F-02/F-03. G-01 and G-02 have no body dependencies and start on day 1.
- **Critical path.** **`cast` and `effect-core` are both long poles**: S-02 (`Skill`, 48 bodies over 1,095 Java lines, `endCast` alone 146) and
  K-01 + K-02 (125 bodies over ~2,000 Java lines). Budget both like m5a's stats-skills lane (7-9 days). `effects-al` is F-01's 574 lines plus 12
  small classes; `effects-mz` is 19 small classes and should finish first and then help.
- **Stage 0 can run before stage 1 or beside it.** It touches P5-01 and P4-11b, which stage 1's player-side lane also touches — so if they run
  concurrently, give the player-side lane P5-13/P5-15/P5-16 only and move P-04 into stage 2. Running stage 0 first is simpler and it is 2 days.

---

## 7. Header requests expected

Bodies never need a request (hub-headers.md §14); these are the declaration changes the analysis predicts. This batch is **much larger than
M5b-1's**, and I-02 should put it in front of the reviewer before stage 1 opens.

| Request | Kind | For |
|---|---|---|
| **The shells-2 open issue, closed.** `override` declarations of the `EffectTemplate` virtuals (`calculate`, `startEffect`, `endEffect`, `onPeriodicAction`, `applyEffect`, `resolveMagicalCritical`, `removeEffect`) in **11 of §2.4's shells**: `ArmorMasteryEffect`(1), `BleedEffect`(5), `PolymorphEffect`(2), `ShieldMasteryEffect`(1), `SpellAttackEffect`(3), `StatdownEffect`(1), `StatupEffect`(1), `WeaponMasteryEffect`(1) — and 18 further shells outside the subset (27 more methods) if the integrator wants one batch instead of two | **additive** (the virtuals exist on `EffectTemplate`, so no vtable slot is added) | **F-04**, `header-requests.md` shells-2 |
| `skillengine/properties/`: **7 new declaration headers** (`FirstTargetProperty`, `FirstTargetRangeProperty`, `MaxCountProperty`, `TargetRangeProperty`, `TargetRelationProperty`, `TargetSpeciesProperty`, `TargetStatusProperty`) + `Properties.h` gains `validate`, `endCastValidate`, `validateEffectedList` ×2, `isAddWeaponRange` and the nested `ValidationResult` | new files (no request) + **additive** on `Properties.h` | **S-03** |
| `skillengine/model/`: new headers `ChargeSkill.h`, `PenaltySkill.h`, `WeaponTypeWrapper.h` (+ the `fwd.h` regeneration, `skeleton.py --fwd`) | new files (no request) | **S-07** |
| `controllers/ControllerStandIns.h`: **delete** `attackUtilCalculateMagAttackResult` (stage 0), `playerRestrictionsCanUseSkill` and `chargeSkillGetAndUse` (stage 1). Of the 13 declarations left at HEAD, 3 go | **signature ×2** | **M-03**, **P-04** |
| `skillengine/SkillEngine.h` — none expected; every declaration exists, only bodies change | – | S-01 |
| `skillengine/model/{Skill,Effect}.h`, `controllers/effect/EffectController.h` — none expected; they are hub headers written at the freeze and `Effect.h:44-47` already names the callback structs the bodies will add **inside the `.cpp`** | – | S-02, K-01, K-02 |
| P5-14 `CheckOutput`: `zeroLiveClasses()` gains `Effect`, `EffectReserved`, `Skill`; the summary gains the effect rows | additive | **G-07** |
| **Manifest (D1)**: split P5-02 into P5-02a/P5-02b sharing `aion_gs_skills`; test directories `tests/effects_al`, `tests/effects_mz`, and a `TESTS` keyword for whichever of P5-02a/b does not inherit `tests/skills` | **build** | **I-01** |
| `game-server/config/m5b2.properties.example` in the **Java** tree beside `m5a.properties.example` and `m5b.properties.example` (m5b-plan.md I-01 decided this location) | none | gate profile |

---

## 8. Risks

Ordered by what is most likely to be wrong, with the evidence for each.

**Free-threaded porting risks — and why this milestone's shape is different from M5b-1's.**

1. **The cycle rows already exist, which is a help and a trap.** `cycles.toml` carries **55 skillengine resolutions**, written and reviewed at the
   S0b/S0c freeze: 40 of the form "java-hook: `Effect.endEffect` → `removeObservers` removes the observer from the target's `ObserveController`
   and clears `observerRemoveTasks` (Effect.java:826-839)", plus `Effect.effector`/`effected` as "accepted: cut elsewhere: EffectController maps,
   ObserveController observers and CreatureGameStats functions", `Effect.designatedDispelEffect` as a **cpp-breaker**, and
   `EffectController.abnormalEffectMap`/`passiveEffectMap` as cpp-breakers through `clearEffectMapsWithoutNotify`. **Each row is a specification
   the body must satisfy.** The trap is that lint L16 is satisfied by the row existing, not by the body honouring it: a `removeObservers` that
   forgets one list, or an `endEffect` that returns early, leaves a cycle that no check sees until the census. **K-07's leak case and G-06 are the
   only two things that can catch it.**
2. **A periodic effect is a task that holds two creatures, and it is the exact shape `cycles.toml` exists for.** `Effect.periodicTasks` is
   `Field<Ref<Array<FutureRef>>>` and `periodicActionsTask` a `Field<FutureRef>` (`Effect.h:70-71`); the effect holds `const Ref<Creature>`
   effector and effected. So **a running over-time effect pins both creatures for its whole duration** — a 20-second Root on a monster that then
   dies keeps the monster alive until `stopTasks` runs. Java's garbage collector does not care; refcounting does. The resolutions say
   `Effect.stopTasks` cancels them from `endEffect`, and `Future::cancel` releases the captures (`docs/deviations/P5-02.md`, the interaction-task
   section: "cancelling releases the captures (`Future::cancel` → `releaseCaptures`)"). **The failure mode to fear is a body that throws before
   reaching `stopTasks`**, which is exactly what the P5-02 deviations record for `AbstractInteractionTask`: "A throw in that window leaves the
   field naming a task that will never run, so nothing but an explicit `stop()` cuts the cycle — the sibling of the throwing periodic body of
   m5a-plan.md §10.5". Read that section before writing `Effect::endEffect`.
3. **`DelayedOnAttack` gains a third `Ref` for the first time.** `CreatureController::DelayedOnAttack` already declares
   `Field<Ref<Effect>> criticalProcEffect` (`CreatureController.cpp:105-113`), and today it is always empty because D14's partial returns null.
   When S-01 closes that arm, a scheduled delayed attack pins **an `Effect`, which pins two more creatures**, for `time` ms. `fieldmap.toml:63`
   classifies the class K4 with the reason "run() clears its references", and `run()` does (`CreatureController.cpp:143-148`). **The row is
   right; the new exposure is that the effect is no longer null.** m5b-plan.md §6.3 Q2 asserts `AttackResult` live 0; M5b-2's Q-row must add
   `Effect` live 0 (**G-07**).
4. **`Effect` is a `StatOwner`, so a leaked effect leaks stat functions on a live creature.** `class Effect : public runtime::RefCounted, public
   model::stats::calc::StatOwner` (`Effect.h:53`), and `cycles.toml:170-172` resolves `StatFunctionProxy.owner` and `CreatureGameStats.stats`
   through "Effect end, unequip, LogoutBreakers::onDelete D4". A buff that ends without removing its functions is not a leak that the census sees
   as an `Effect` — it is a creature whose stats are permanently wrong. **`tests/effects_*` must assert the stat function is gone after
   `endEffect`, not only that the effect is.**
5. **Three threads apply effects at once.** A cast ends on a scheduler thread (`schedule(this::endCast, castDuration)`), `applyEffect` runs on
   another (`schedule(() -> applyEffect(effects), hitTime)`), the periodic ticks on a third, and the client packet that started it all arrived on
   an IO thread. `EffectController`'s maps are the shared state; Java synchronizes some of it and not all. Port the synchronization exactly and
   mark what Java leaves racy with `// java-race` (D9).
6. **Swallowed exceptions hide the first M5b-2 bug, in two places.** `CreatureController::useSkill` catches and logs (`CreatureController.cpp:480-482`)
   and `NpcController::onDie` catches the reward block (m5b-plan.md risk 6). The npc skill path of stage 2 runs through the first one. Q1's "no
   ERROR line" is what turns both into failures — and m5b-client-session.md S-1 is the proof that it matters: five kills, five ERROR lines, a
   green-looking session.

**Scope and process risks.**

7. **D1 is the plan's biggest single hazard.** If the manifest is not split, stage 1 has one lane doing 262 sites and the milestone is 4-5 waves,
   not 4 stages. **Decide it before stage 1 opens**, not during.
8. **D2 is the second.** Applying passive effects changes every character's numbers. The analysis says V9 survives (§4 D2) and **the analysis did
   not build**. If it does not survive, G-05 grows from "re-run" to "re-model the oracle", which is a lane of its own.
9. **The 45 invisible bodies are how this milestone gets estimated wrong.** A lane that plans from `grep -c AION_UNPORTED` will size
   `StatupEffect` at zero and discover on day 3 that it needs a header request, a new `.cpp` and a `fwd.h` regeneration. **F-04 exists to front-load
   exactly this**, and the wave report should state the header batch landed before any effect body was written.
10. **`Properties` is the silent prerequisite.** `Skill.canUseSkill` and `Skill.endCast` both go through it, 907 Java lines exist and **no C++
    file does**. It is not on anybody's inventory because it has no `AION_UNPORTED` site to count. If S-03 slips, S-02 cannot finish and the
    whole cast lane stalls.
11. **Float exactness, again and worse.** `StatFunctions::calculateMagicalSkillDamage`, `EffectTemplate::calculateDamage` and the heal
    arithmetic are float-heavy, and CONVENTIONS.md names `stats/AttackUtil` as a `/fp:strict` candidate. M-04's and F-05's golden vectors must be
    run in **RelWithDebInfo as well as Debug**.
12. **`gs.scenario.m5b2` adds a fourth server run to the same `RESOURCE_LOCK`.** `gs.scenario.m5a` takes ~32 s, `m5a_geo` ~20 s, `m5b` 60-90 s,
    and M5b-2 adds two more serialized runs on the user's own machine. Budget the new gate at 90-120 s (two characters, two enter-worlds, a
    20-second debuff to watch expire) and **keep the lock**.
13. **The npc-skill gate case is a real fight with a real monster.** §2.4(b)'s npc 210133 is level 1 with `srange=7`; the gate's character will be
    attacked while it is measuring, exactly as M5b-1's K5/K6 overlap. Script it as M5b-1 scripted K4b, with the oracle choosing the spot.
14. **m5b-plan.md's data counts were wrong in at least two more places** (§3), and both were counts over XML the server reads differently from a
    grep: commented-out spawns, and `npc_ids="a,b,c"` lists. **Every number in §2.4 of this plan has the same exposure.** G-01 is where the oracle
    takes them over; until then treat them as sizing, not as gate constants.

---

## 9. The split: four stages, and why in this order

| Stage | What a player can do at the end | Chunks | Sites closed | Bodies | Lanes |
|---|---|---|---|---|---|
| **0 — casters can swing** | A Mage, Priest or Spiritmaster can auto-attack. m5b-client-session.md **S-3 closed**; the §10 checklist drops "melee classes only" | P5-01, P4-11b | 4 + 1 stand-in | ~4 | **1 lane, ~2 days** |
| **1 — a character can cast** | Cast a level-1..10 skill of a starting class; see the cast bar, the MP, the damage, the heal, the buff icon and the debuff icon; the revive debuff works; passive skills apply | P5-02a, P5-02b, P5-03 (part), P5-04 (part), P5-13, P5-15, P5-16, P4-11b, P5-SC | **~320** = 262 (P5-02) + 56 (the subset's share of P5-03/04) + 2 stand-ins | **~360** = those, + 15 undeclared effect methods + 4 `Properties` + ~27 in the missing classes + `CM_CASTSPELL` | **6 lanes** |
| **2 — monsters cast too, and the gate proves it** | A Poeta monster uses its skill; the M5b-1 and M5a gates are green again; four M5b-1 partials are closed | P5-05, P5-SC, P5-02b (lease), the fixups | ~12 + 7 allow-list rows | ~20 | **4 lanes** |
| **3 — the leak surface** | The stress nightly fights with abilities under ASan; the census names `Effect` | P5-14, P5-SC | 0 | ~5 | **2 lanes** |

Four arguments for this order.

1. **Stage 0 is free and it unblocks three of the eight classes.** Four bodies, no dependency on anything else in the milestone, and it removes
   the one thing the first real-client session found that a user actually hits: a Mage that cannot swing. Holding it inside stage 1 means the
   user waits two more weeks for a two-day fix. **This is the single strongest recommendation in this plan.**
2. **Stage 1 has a gate that can pass, and it is exactly the level-1 skill set.** §2.4's table shows a cast bar, an MP cost, an instant skill, a
   heal, a 5-second self-buff and a 20-second debuff all at character level 1, with no item path, no levelling and no learn path — because of D3's
   `player_skills` seed and D4's second character. There is a green point between "no skill works" and "every skill works", which is what M5b-1's
   §9 argument asks for.
3. **Stage 2 is gated on stage 1, not the reverse.** `SkillAttackManager.skillAction` calls `CreatureController.useSkill` → `SkillEngine.getSkill`
   → `Skill.useSkill`: the npc rotation is a *caller* of the engine. Porting it first would mean debugging the rotation against an engine that
   does not exist — the same argument m5b-plan.md §9 item 2 made for putting M5b-1 before M5b-2.
4. **The effect subset is what makes one milestone possible at all.** The alternative sizing — port the 184 classes — is 219 bodies and 8,330
   Java lines of effect behaviour on top of ~300 bodies of engine, and it has no natural stopping point. §2.4's 34 classes are a stopping point
   with a data-derived justification: they are exactly what the four starting classes, the two start maps and the whole tree's post-spawn skills
   use. **The classes outside the subset stay `AION_UNPORTED` and throw (D6)**, which is what keeps the boundary honest — the first skill outside
   it fails loudly in `unported_trace.txt` rather than quietly doing nothing.

**What this plan does not claim.** It does not claim that stage 1 fits in one wave. ~400 bodies over 6 lanes is comparable to M5b-1's stage 1
(which closed ~150 sites over 6 lanes) at roughly two and a half times the size. **If stage 1 has to split, split it at the effect classes**: land
the engine (S-*, K-*, F-01 and the ten or so leaf classes a level-1 Warrior and a level-1 Mage reach — `SkillAttackInstantEffect`,
`SpellAttackInstantEffect`, `RootEffect`, `WeaponMasteryEffect`, `ArmorMasteryEffect`, `ShieldMasteryEffect`, `StatboostEffect`,
`StatdownEffect`, `HealInstantEffect`, `ReturnEffect`, `EscapeEffect` and their four bases) with a gate that asserts only 2864, 1282, 1328 and
8291, and take the other ~19 classes plus the Priest/Scout cases and the npc rotation in a second wave. Splitting at the *engine* instead — `Skill` in one wave, `Effect` in the next —
leaves a wave with no green point, because a cast that creates no effect is not a behaviour anyone can assert.

---

## 10. Gate specification (`ctest -L scenario`, `gs.scenario.m5b2`)

### 10.1 Processes, databases and profile

Identical to m5b-plan.md §6.1 except:

| Piece | M5b-2 |
|---|---|
| Schemas | `aion_ls_test_m5b2_<hash>` / `aion_gs_test_m5b2_<hash>`, same `SchemaLease` and sweep |
| Output directory | `<bin>/scenario/m5b2`, its own `gs_log` and `ls_run` |
| `RESOURCE_LOCK` | the same `"aion_game_server_log;aion_login_server_log"` |
| Profile | the M5b-1 `-D` set **minus `gameserver.soulsickness.disable`** (D5), `gameserver.geodata.enable=false`, `gameserver.character.reentry.time=1`. Written out as `game-server/config/m5b2.properties.example` in the Java tree |
| Allow-list | `tests/scenario/m5b2_partial_allowlist.txt`, three sections as M5b-1's. **§A** should be nearly empty: the M5a startup rows (`QuestEngine.cpp:111`, `BaseService.cpp:18`) and `DropRegistrationService.cpp:43` (M5b-3's). **§B** the partials this milestone leaves: `CumulativeResist` if K-03 stays W, the modifier classes if K-04 stays W. **§C** the timing rows M5b-1 lists |
| Characters | **two**: the M5b-1 Elyos Warrior (account A) and an Elyos **Mage** (D4), created in the same run |
| Target | npc **210663** for the melee cases (m5b-plan.md D11) and npc **210133** "striped kerub" (level 1, skill 16419, nearest spot 76.8 m from the Elyos spawn, `static_id=0`) for the npc-skill case — **both chosen by the oracle, not hardcoded** |

### 10.2 Cases

C1-C3 are M5a cases 1-4 replayed (login, create, enter world, level ready), for the Warrior first.

| # | Case | Steps |
|---|---|---|
| **C0** | the oracle answers | `oracle.py m5b2-skills --race ELYOS --class WARRIOR` and `--class MAGE` return the autolearn skill ids, their levels, cast durations, MP costs and effect classes with their `duration1`/`duration2`/`tslot` |
| **C4** | **the passives applied** | Immediately after enter world, before anything else: read the enter-world `SM_STATS_INFO` burst and compare against the oracle's **new** model (D2) |
| **C5** | **an instant skill** | Warrior: approach npc 210663 to 2 m as M5b-1's K4b does, `CM_TARGET_SELECT`, then `CM_CASTSPELL(2864, 1, targetType=0, objId, hitTime)` |
| **C6** | **a timed cast** | Quit, re-enter as the **Mage**, approach, `CM_CASTSPELL(1282, 1, 0, objId, hitTime)` and wait |
| **C7** | **cast interruption** | Start 1282 again and send `CM_MOVE` 300 ms into the cast |
| **C8** | **a debuff with a lifetime** | `CM_CASTSPELL(1328, 1, 0, objId, …)` — Root, 20,000 ms, `tslot="DEBUFF"` — then **send nothing at the monster for 25 s**. The silence is required, not incidental: `RootEffect.startEffect` attaches an `ATTACKED` observer that removes the effect on `Rnd.chance() >= resistchance` with `resistchance="10"` (RootEffect.java:47-53), i.e. **≈90 % per hit**. A gate that keeps attacking measures a random duration |
| **C9** | **a self-buff** | Seed `player_skills` with 3195 (D3), relog, cast it on self; two entries, 5,000 ms each |
| **C10** | **a heal** | Seed 1838, let the monster bring the Mage below full HP, cast it on self |
| **C11** | **the npc casts** | Pull npc **210133** and take the whole recording of the fight |
| **C12** | **the revive debuff** | Seed `player_life_stat.hp` low (m5b-plan.md D12), die, `CM_REVIVE(BIND_REVIVE)` — Soul Sickness 8291 must apply (D5) |
| **C13** | **the saved effect round trip** | With a > 28 s effect up, `CM_QUIT(0)`, read `player_effects`, re-enter |
| **C14** | reports and shutdown | the M5a Q8 bar plus the M5b-2 rows; the stop file with a character online |

### 10.3 Assertions

Every row states **what it proves** and **what it cannot** — because m5b-plan.md rev 1 shipped four assertions that a fake client could not
observe at all, and rev 3 found three more against a real run.

| # | Case | Assertion | Proves / cannot prove | What a wrong port does |
|---|---|---|---|---|
| **X1** | C4 | The enter-world `SM_STATS_INFO` burst matches the oracle's model **with passive effects applied**: `physicalAttack` is the class base raised by the weapon-mastery percentage that matches the equipped weapon group, and the base max HP/MP are **unchanged** from the M5a value. | **Proves:** `applyEffectDirectly` created real effects and `CreatureGameStats` took their functions (D2). **Cannot prove:** that the *percentages* are retail-correct — the oracle and the server would both be reading `skill_templates.xml`, so this is a consistency check, not an independent one. **Cannot prove** the `ArmorMastery` arms unless the character's equipped armour matches. | O-09 left partial (no change at all), a `StatboostEffect` applied twice, a `WeaponMasteryEffect` that skips its weapon-group check |
| **X2** | C5 | `CM_CASTSPELL(2864)` is answered by `SM_CASTSPELL` with **`castDuration == 0`**, then by `SM_CASTSPELL_RESULT` with `skillId == 2864` and a non-16 status byte, then by `SM_ATTACK_STATUS` for the monster with `value > 0`. No `SM_SKILL_CANCEL`. | **Proves:** the whole `CM_CASTSPELL` → `getSkillFor` → `canUseSkill` → `Properties.validate` → `endCast` → `Effect.initialize` → `applyEffect` → `reduceHp` chain for an instant physical skill. **Cannot prove:** the damage *number* (D8) or anything about a cast bar. | a missing `CM_CASTSPELL`, a `canUseSkill` that refuses, a `Properties` that picks no target (status byte 16) |
| **X3** | C5 | The `SM_CASTSPELL_RESULT` status byte is **32** on the first cast (`chainSuccess`, `chain_skill_prob=100`) and the chain window is observable in a second cast within `chain_usage_duration`. | **Proves:** `ChainSkills.updateChain` and `ChainCondition`. **Cannot prove:** the chain *reset* path, which needs a failed cast. | `endCast`'s chain arm dropped — the byte is 0 |
| **X4** | C6 | `CM_CASTSPELL(1282)` is answered **immediately** by `SM_CASTSPELL` with **`castDuration` within ±10 % of 2,000 ms**, and `SM_CASTSPELL_RESULT` arrives **not before 1,800 ms later**. At the *end* of the cast, **`SM_ATTACK_STATUS` with `type == USED_MP` and `value == 19`** and an `SM_STATUPDATE_MP` whose `currentMp` fell by the same 19. | **Proves:** the cast bar, the two-phase cast (`startCast` now, `endCast` scheduled), and that the MP came out of **`MpCondition.validate`** — which is where Java puts it (MpCondition.java:31-35 calls `reduceMp(TYPE.USED_MP, getCost(skill), 0, LOG.REGULAR)` itself), reached through `payCastCosts` → `endCondCheck` → `Conditions.validate`. **Cannot prove:** the cast-speed modifier arithmetic — a level-1 character has no cast-speed stat change, so `castDuration` is the template value; nor the `boostSkillCost` term, which is 0 with no buffs up. | an engine that applies effects at `startCast`; an `MpCondition` that only *checks* the MP (a natural mistake: the cost looks like an `<actions>` job and is not) |
| **X5** | C7 | A `CM_MOVE` 300 ms into the cast is answered by **`SM_SKILL_CANCEL`** and by **no `SM_CASTSPELL_RESULT`**, and the MP is **unchanged**. | **Proves:** `PlayerController::onStartMove` → `cancelCurrentSkill` → `Skill.cancelCast`, and that `payCastCosts` runs in `endCast` and not in `useSkill`. **Cannot prove:** interruption by *damage* (`cancelRate`), which is a random roll — that belongs in a unit test. | a cast that charges MP up front; a `cancelCast` that does not set `isCancelled`, so `endCast` still fires |
| **X6** | C8 | After `CM_CASTSPELL(1328)`: one `SM_ABNORMAL_STATE` whose entry list contains `skillId == 1328`, `targetSlot == DEBUFF` and **`remainingTimeToDisplay` within [19,000, 20,000]**; the monster sends **no `SM_MOVE`** while it is up (`Creature.canPerformMove` refuses while `isInAnyAbnormalState(CANT_MOVE_STATE)`, Creature.java:182-184 — and that body is **ported**, so this row tests `RootEffect.startEffect`'s `setAbnormal`, not the move controller); and **a second `SM_ABNORMAL_STATE` without that entry arrives 20 ± 1 s later**. | **Proves:** `Effect.startEffect` → `EffectController.addEffect` → `broadCastEffects`, the duration arithmetic (`duration2 + duration1 × skillLevel`), the end task, and that `RootEffect` actually roots. **Cannot prove:** the icon the client draws; the `dispel_category`/`req_dispel_level` fields, which no packet carries; and the `resistchance` break, which is random and is why C8 forbids attacking. | an effect that never ends (no second packet), a duration computed from `duration1` only, a `RootEffect` whose `startEffect` does not set the state |
| **X7** | C9 | After `CM_CASTSPELL(3195)` on self: one `SM_ABNORMAL_STATE` with **two** entries, both `skillId == 3195`, `targetSlot == BUFF`, `remainingTimeToDisplay` ≈ 5,000; an empty one 5 ± 1 s later. | **Proves:** several effect templates of one skill become several entries of one `Effect`, and the `preeffect` chain (`alwaysresist` has `preeffect="1"`). **Cannot prove:** that the dodge and resist *behaviour* works — a fake client cannot make the monster miss on demand. That is `tests/effects_mz`' job. | a controller that keeps one entry per skill instead of per template; a `preeffect` chain that drops the second |
| **X8** | C10 | `CM_CASTSPELL(1838)` on self raises the Mage's HP: `SM_STATUPDATE_HP` shows `currentHp` **increase**, and the increase is **≤ `maxHp - currentHp`** (no overheal on the wire) and **> 0**. | **Proves:** `HealInstantEffect` → `AbstractHealEffect` → `CreatureLifeStats.increaseHp`. **Cannot prove:** the exact heal value — it goes through `calculateBaseHealValue` and the heal-boost stat (D8). | a heal that reduces HP (sign error), one that overheals past max, one that never reaches the life stats |
| **X9** | C11 | In the recording of the fight with npc 210133: at least one `SM_CASTSPELL` whose sender is the **monster** and whose `skillId == 16419`, followed by an `SM_CASTSPELL_RESULT` and an `SM_ATTACK_STATUS` for the **character**. | **Proves:** `chooseAttackIntention` → `SkillAttackManager` → `useSkill` end to end for an npc. **Cannot prove:** the 25 % `prob` — one fight is not a sample; the gate asserts *at least one* over a fight long enough that `1 - 0.75^n < 10^-4` (n ≥ 33 attack decisions) and the failure message says so. | `chooseSkillAttack` still answering false; `conditionReady` refusing everything; a rotation that casts on every swing |
| **X10** | C12 | After `CM_REVIVE(BIND_REVIVE)`: an `SM_ABNORMAL_STATE` containing `skillId == 8291`, and an `SM_STATS_INFO` whose **`maxHp`** (the *current* max, not `baseMaxHp`, which a PERCENT stat function does not touch) is **70 % ± 1** of the pre-death value, with `baseMaxHp` **unchanged**. The profile carries **no `gameserver.soulsickness.disable`** (D5). | **Proves:** `PROVOKED` activation (the skill is not in the player's skill list, so `getSkillFor`'s `isSkillPresent` gate must be bypassed — SkillEngine.java:58), `StatdownEffect` on `MAXHP`, and that M5b-1's configuration workaround is gone. **Cannot prove:** the `deathCount` scaling of `duration1` (`40000 + 20000 × deathCount`), which needs a second death in the same session. | a `getSkillFor` that still checks `isSkillPresent` for a PROVOKED skill; a `StatdownEffect` that writes the base instead of the bonus (then `baseMaxHp` moves and the row catches it) |
| **X11** | C13 | With a > 28 s effect up at `CM_QUIT`: `player_effects` holds exactly one row for that skill id; after re-entry an `SM_ABNORMAL_STATE` carries it again with a **smaller** `remainingTimeToDisplay`. | **Proves:** `Effect.canSaveOnLogout`, the DAO round trip, and `PlayerEffectController::addSavedEffect` (the §C partial, now closed). **Cannot prove:** the 28-second predicate boundary — one effect is one sample. | `addSavedEffect` still partial (no second packet); a stored `endTime` that is re-based on load |
| **X12** | C14 | The M5a Q8 bar: `unported_trace.txt` **empty**, `census.txt` empty, lockdep empty, no watchdog dump, no ERROR in either log; `partial_trace.txt` ⊆ the M5b-2 allow-list with §A hit ≥ 1 and §B hit **exactly 0**. | **Proves:** no skill on the scripted path fell outside §2.4's subset, and nothing threw inside the two swallowing `catch` blocks (§8 item 6). **Cannot prove:** that skills *off* the scripted path work — D6 is what makes them fail loudly when a real client finds them. | any effect class outside the subset; a `CreatureController::useSkill` that logs and continues |
| **X13** | C14 | `live_counts.txt`: **`Effect` live 0 with `created > 0`**, `EffectReserved` live 0, `AttackResult` live 0 (M5b-1's row), and `Skill` live 0. | **Proves:** `endEffect` ran its full hook chain for every effect the run created, including the ones that ended by their own timer. **Cannot prove:** a leak of an effect whose creature is still in the world and still hating — that shows up as a bounded, not zero, count and needs G-06's 30-minute run. | a `removeObservers` that misses a list; an `endEffect` that returns early; a periodic task that is never cancelled |

### 10.4 Mutation proof (the standard)

Every assertion above must be watched failing, with the exact mutation and both quoted outputs. The minimum set, **including the rows that record
what the gate deliberately cannot catch**:

| Mutation | Must fail | Must stay green |
|---|---|---|
| `Skill::endCast`: move `payCastCosts()` into `useSkill()` | **X5** (MP falls on a cancelled cast) | X4's MP delta on a completed cast |
| `Skill::useSkill`: call `endCast()` directly instead of scheduling it | **X4** (`SM_CASTSPELL_RESULT` arrives immediately) | X2 (2864 is instant either way) |
| `Effect::calculateTemplateDuration`: use `duration1 × skillLevel` only | **X6** (`remainingTimeToDisplay` ≈ 0) | X2, X4 |
| `Effect::endEffect`: skip `stopTasks()` | **X13** and G-06 — **not X6**, because the icon still disappears when the end task runs | X6 |
| `EffectController::addEffect`: keep one entry per skill instead of per template | **X7** (one entry instead of two) | X6 |
| `AbstractHealEffect`: negate the heal | **X8** | everything else |
| `SkillEngine::getSkillFor`: check `isSkillPresent` before the `PROVOKED` bypass | **X10** (no soul sickness) | X2, X4 |
| `GeneralNpcAI::chooseSkillAttack`: return false (i.e. revert N-02) | **X9** | every other row |
| `SkillEngine::applyEffectDirectly`: leave the O-09 partial | **X1** | X2-X13 |
| `Skill::cancelCast`: do not set `isCancelled` | **X5** (`SM_CASTSPELL_RESULT` arrives after the cancel) | X4 |
| `AttackUtil::calculateMagicalResistRate`: drop the `levelDiff > 4` term | **nothing in the gate** — the gate's Mage and its target are within 4 levels. **M-04's unit vector must catch it.** | `gs.scenario.m5b2` green |
| `EffectTemplate::calculateHate`: return 0 | **nothing in the gate** — hate is not on any wire the scenario client reads, and with one attacker the relative order cannot change. **A `tests/stats` assertion over `AggroList::getFinalDamageList` must catch it.** | everything |
| an `Effect` leaked deliberately (keep a `Ref` in a static) | **X13** | X12 |

### 10.5 The geo gate

`gs.scenario.m5b2_geo` runs the same script with `gameserver.geodata.enable=true`, `LABELS "scenario;realdata;geo"`, `TIMEOUT 2700`, the same
`RESOURCE_LOCK`. **G-04 must first establish whether geo changes anything on the cast path at all**; m5b-plan.md §6.4 is the precedent for saying
"it does not, and here is what the geo run does assert instead" rather than inventing a row that cannot fail.

---

## 11. Real-client checklist (user, after stage 1)

Prerequisites as m5b-plan.md §10 steps 1-6, with `mygs.properties` from `m5b2.properties.example`, then:

1. **After stage 0 only**: make a **Mage** and left-click a monster. It swings, damage numbers appear, the monster dies. This is the S-3 check and
   it needs nothing else from M5b-2.
2. Make an Elyos Warrior. The skill bar shows *Ferocious Strike*. Use it: the animation plays, the damage number is larger than an auto-attack,
   and the chain indicator lights up.
3. Make an Elyos Mage. Use *Flame Bolt*: **a cast bar appears and runs for about two seconds**, the MP bar drops, and the damage lands at the end.
4. Move during the cast: the bar disappears, the client says the skill was cancelled, and **the MP does not drop**.
5. Use *Root* on a monster: a **debuff icon** appears over its head with a 20-second timer, it stops moving, and the icon disappears when the
   timer runs out.
6. Make a Scout and use *Focused Evasion*: **two buff icons** appear on your own frame for 5 seconds.
7. Make a Priest, take damage, use *Healing Light*: the HP bar rises.
8. **Let a monster kill you, then revive at the bind point.** The Soul Sickness icon appears and your HP and MP bars are visibly shorter. This is
   the step that proves D5 and that M5b-1's configuration workaround is gone.
9. Log out with a long buff up, log back in: **the buff is still there with less time left**.
10. **After stage 2**: fight a "striped kerub" (they are the small kerubs near the Elyos start). It should sometimes use a skill on you — a
    different animation and a bigger number than its normal swing.
11. **Attack something the subset does not cover** — a monster in Verteron or Eltnen, or any class skill above level 10. Expect a refusal or an
    `UnportedException` in the log; **that is D6 working**, not a regression. Note which skill ids appear in `unported_trace.txt`: they are the
    entry list for the next milestone, exactly as F-2 was for wave B.
12. Send `game-server/log/`, `m5a_summary.txt`, `live_counts.txt`, `partial_trace.txt` and `unported_trace.txt`.

---

## 12. What was measured and what was inferred

The M5b plan's first draft had 14 defects found in review, one of them a body on the gate's own hot path that was never made a work item. This
section exists so the same review can be done on this plan cheaply.

**Measured** (grep or a parse over the two trees at HEAD `340c05c5e`; re-runnable):

- Every `AION_UNPORTED` / `AION_PARTIAL` count in §2.2 and the per-file table, including P5-02 = 262, P5-03 = 100, P5-04 = 74.
- The 184 effect classes and their three kinds (126 / 29 / 29); the **45 undeclared method bodies** and the 11 P5-02 classes with no C++ file.
- Java LOC per chunk (9,505 / 4,422 / 3,908 = 17,835).
- Every "0 unported" claim about a packet: the 20 server packets of §1 and §2.7 were each grepped.
- `CM_CASTSPELL`, `CM_USE_CHARGE_SKILL`, `CM_TOGGLE_SKILL_DEACTIVATE`, `CM_REMOVE_ALTERED_STATE` absent; 41 `CM_*.cpp` exist of 190 Java classes.
- The `AttackUtil` 6 / `StatFunctions` 3 split and the **dependency closure of `calculateMagAttackResult`** (every callee read and checked).
- The skill/effect data of §2.4: the 49 / 132 / 411 / 570 / 2,985 skill counts, the effect-class sets and their `extends` closures, the per-skill
  tables (2864, 1282, 1328, 1838, 3195, 8291, 243/245/302), the 32 Poeta / 36 Ishalgen npc-skill ids and their 29 skill ids, the 199
  post-spawn entries and their 10 effect classes, and the union's 27 leaves / 34 classes / 71 bodies / 2,269 Java LOC.
- The autolearn sets and the base HP/MP of all four starting classes, **cross-checked against `oracle.py m5a-creation`** (run read-only, it
  reads the Java tree and prints; nothing was built).
- **A defect this analysis found in its own first pass, and the reason §12 exists.** The first draft of §2.4 filtered `skill_tree.xml` on
  `classId in {WARRIOR, SCOUT, MAGE, PRIEST}` and so missed the rows with **no `classId` at all**, which belong to every class: `243` *Return*,
  `245` *Bandage Heal* and `302` *Escape* at `minLevel="1"`. The subset was 32 classes and had to become **34** (`ReturnEffect`, `EscapeEffect`).
  The oracle is what caught it. **Any lane that re-derives these numbers should cross-check against `m5a-creation` the same way.**
- Poeta's **147** spawn elements over **1,029** spots, and that the two extra ids a grep finds are inside an XML comment at `210010000_Poeta.xml:378-381`.
- `cycles.toml` has 310 resolutions, 55 of them skillengine, 376 unresolved edges all under `data/handlers`, and the file's last commit is
  `ecbca4f21` — i.e. **M5b-1 added no cycle row**.
- `PlayerSkillListDAO` and `PlayerEffectsDAO` fully ported; `player_skills` is `(player_id, skill_id, skill_level)`.
- The M5b-1 allow-list's three sections and every row in them.
- `oracle.py m5a-creation`'s "passive skill effects are not applied" assumption (`tools/oracle/m5a/creation.py:160`).

**Inferred, and a lane should confirm before relying on it:**

- **That closing O-09 leaves the M5a gate's V9 green** (D2). The reasoning is that none of the eight level-1 passives touches `MAXHP`, `MAXMP` or
  `ATTACK_SPEED` and that `getMaxHp().getBase()` is a base. **Nothing was built or run.** G-05 is the item that settles it.
- **That `Skill`'s 48 bodies are all required for a level-1 cast.** They were listed, not individually traced; some (`calculateChargeCastDuration`,
  `isSummonType`, `isInvalidRecall`) are plainly not on the level-1 path and could be stubbed if the lane is short of time.
- **That `Effect`'s 83 bodies are all required.** Same caveat; ~50 of them are accessors and cheap either way.
- **That the geo gate can assert something the geo-off one cannot** (G-04). m5b-plan.md §6.4 found the opposite for the attack path and said so;
  the same honesty is required here.
- **That `NpcGameStats`'s skill bookkeeping is unported** (N-03). M5b-1's B-05 ported two of its bodies; the rest was not re-checked.
- **The effort letters.** They come from comparing body counts and Java LOC with M5a's and M5b-1's lanes, not from measurement.
- **That `AION_PARTIAL` is not the right tool for an unported effect class** (D6). This is a judgement about failure modes, not a fact.

**Claims of m5b-plan.md that this analysis checked and found wrong**, all in §3: the **"sites == work" assumption** (the big one), the P5-02
per-chunk figure, D4's reading of the starter maps' npc skills, §2.4's Poeta spawn counts, and D3's post-spawn file and id counts.
**Claims it checked and confirmed**: the ~17,800 Java LOC and the ~450 total site count (448 measured, once P5-01 and the stand-ins are added and
`NpcSkillTemplateEntry` is not double counted), D14's `createCriticalProcEffect` reasoning (the reordered body at `SkillEngine.cpp:104-140`
matches what D14 specified), D3's "the partial is the whole body, not an arm behind a data check", and risk 7's "no AI cycle rows" — which is
still true today, and now also true of M5b-1's own tasks.

---

## 13. Open questions this analysis could not settle without building

Listed so that the first lane does not rediscover them.

1. **Whether the M5a gate's V9 survives O-09** (D2, §12). The first thing G-05 must measure, and the answer decides whether the oracle needs a
   passive-effect model.
2. **Whether `Properties.validate` reaches `GeoService.canSee` anywhere.** It decides whether `gs.scenario.m5b2_geo` has a row of its own (G-04)
   or is a second run of the same script.
3. **What the 20-second Root does to the monster's AI.** `RootEffect` sets a movement-forbidding state; the M5b-1 AI has a chase and a giveup
   timer (`SimpleAttackManager.attackAction`'s 15-second arm). A rooted monster that cannot reach its target may give up and walk home while
   rooted — which is either a correct port of a Java quirk or a bug, and only a run can tell. X6's "no `SM_MOVE`" assertion is where it shows.
4. **Whether a fake client can make a second `SM_ABNORMAL_STATE` arrive at all for a self-buff**, or whether the packet is only broadcast to
   *other* players — the trap m5b-plan.md T1 fell into with `SM_TARGET_UPDATE` (`broadcastToSightedPlayers` excludes the owner). `EffectController::broadCastEffects`
   is ported (`EffectController.cpp`) and should be read before X7 is written.
5. **Whether `CreatureController::useSkill`'s `catch` should be narrowed for the npc path** (§2.6). Java swallows too, so a faithful port swallows
   — but Java's callee cannot throw `UnportedException`. Decide with the deviations owner before N-01.
6. **How long a fight against npc 210133 has to be for X9's "at least one skill cast" to be safe.** The `prob` is 25 per skill entry, but the
   rotation's decision rate depends on `getNextAttackInterval` and on `canUseNextSkill`'s delay, neither of which this analysis modelled.
7. **Whether the ~24 effect classes outside the Warrior/Mage pair can be deferred to a second wave** (§9's fallback split) without leaving the
   gate unable to assert a buff or a heal. On the numbers it can — `StatupEffect`, `AlwaysDodgeEffect`, `HealInstantEffect` and `RootEffect` are
   four small classes — but the boundary was not drawn precisely here.
8. **Whether `tests/skills` should be split when P5-02 is** (I-01). The directory already holds M5b-1's `CriticalProcEffectTest` and
   `NpcSkillListTest`, which would land on opposite sides of the D1 seam.
