# M5b work plan (combat and AI)

> **Status:** plan **rev 2**, 2026-09-22. A **read-only** analysis over HEAD `ecbca4f21` (stage 3 wave B, clean tree, 2,354 tests passing) against the
> Java 4.8 tree. **Nothing was compiled, built or run for this plan**; every C++ statement below comes from reading the tree, from
> `tools/porting/chunks.py owner` and from the committed manifest snapshot `build/msvc/game-server/chunks.json`.
>
> **Rev 2** is an adversarial review of rev 1 re-checked against both trees. It corrects the counts of §2.2, §2.4 and D3, turns three things rev 1
> left as prose into work items with owners (**D14/E-05** `createCriticalProcEffect`, **D15/A-00** the unguarded `NpcAI` casts, **E-01a** the
> AI-facing stand-ins moved into stage 1), changes the gate's monster spot (**D11**), and **rewrites A1, A3, A4, A6, R1, P3 and Q1**, which
> asserted things a fake client cannot observe. Every rewritten assertion now names what it proves *and what it cannot*.
> Two rev-1 claims were challenged and **survived**, with the evidence restated where they live: **R3**'s "exactly once per kill" (§6.3, and §11 item 3
> is closed as a result) and **R2**'s `REWARD_AP` arm (§6.3). The note after §6.3 records the corrections of record.
> It follows the shape of [m5a-plan.md](m5a-plan.md): path analysis, numbered work items with owners, lanes, gate, risks. Ownership follows
> `tools/porting/chunks.py`; header changes follow [hub-headers.md](hub-headers.md) §14 through `docs/porting/header-requests.md`.
> Inputs: m5a-plan.md §3/§5/§10/§11, [m5a-client-session.md](m5a-client-session.md) F-2, [runtime-architecture.md](runtime-architecture.md),
> `game-server/generated/concurrency/cycles.toml`.

---

## 1. Summary

**The mechanical half of a fight is already ported.** That is the finding that decides the shape of this milestone, and it was not obvious
before the trace. M5a had to port the controllers to make spawn, enter-world and logout work, and the controllers *are* the combat loop:

| Already ported, 0 `AION_UNPORTED` | Evidence |
|---|---|
| `CreatureController::attackTarget`, the six `onAttack` overloads, `onDie`, `DelayedOnAttack`, `calculateGodStoneEffects` | `controllers/CreatureController.cpp:203-397` (Java CreatureController.java:154-359, 563-583) |
| `PlayerController` — **all 48 bodies**, including `attackTarget`, `onAttack`, `onDie`, `doReward`, `useSkill`, `scheduleShowResurrectionOptions`, `showResurrectionOptions` | `controllers/PlayerController.cpp` (852 lines, 0 unported; Java PlayerController.java:152, 268, 376, 396-456, 458) |
| `NpcController::onDie`, `doReward`, `petLoot`, `findPetForLooting`, `onAddHate`, `onAttack`, `loseAggro` | `controllers/NpcController.cpp:162-278` (Java NpcController.java:137-273) |
| `CreatureLifeStats::reduceHp` / `onHpChanged` — the path that calls `getController().onDie(...)` | `model/stats/container/CreatureLifeStats.cpp:66-102, 269-276` |
| `NpcMoveController` (the chase) | `controllers/movement/NpcMoveController.cpp`, 508 lines, 0 unported |
| `RespawnService` (respawn + decay tasks) | `services/RespawnService.cpp`, 0 unported |
| `GeoService::canSee` / `getClosestCollision` | `world/geo/GeoService.cpp:117-206`, 0 unported |
| Every combat **server** packet | `SM_ATTACK`, `SM_ATTACK_STATUS`, `SM_ATTACK_RESPONSE`, `SM_CASTSPELL`, `SM_CASTSPELL_RESULT`, `SM_SKILL_CANCEL`, `SM_SKILL_ACTIVATION`, `SM_TARGET_SELECTED`, `SM_TARGET_UPDATE`, `SM_LOOT_STATUS`, `SM_LOOT_ITEMLIST`, `SM_DIE`, `SM_EMOTION`, `SM_ABNORMAL_STATE`, `SM_ABNORMAL_EFFECT`, `SM_STATUPDATE_HP/MP`, `SM_QUESTION_WINDOW` — all present, all 0 unported |
| `AggroInfo` (with the `java-race` markers for Java's unsynchronized hate/damage) | `controllers/attack/AggroInfo.cpp:18-40` |
| `AbstractAI` dispatch (`onGeneralEvent`, `onCreatureEvent`, `handleGeneralEvent`, `handleCreatureEvent`, `setThinking`, `logEvent`), `AIEngine::init`, `AILogger` | `ai/AbstractAI.cpp`, `ai/AIEngine.cpp:76-85`, `ai/AILogger.cpp` |

**What is empty is everything those bodies call into**, and it is of four very different sizes:

| # | Hole | Where | Size |
|---|---|---|---|
| 1 | **The NPC brain.** `NpcAI` is **27** `AION_UNPORTED` bodies (`ai/NpcAI.cpp:17-123`) and the packages it delegates to **hold only their `fwd.h`**: `ai/handler/` has 1 of its 14 classes (`FreezeEventHandler`), `ai/manager/` 0 of 6, `ai/follow/` 0 of 2, and `AIActions`, `HpPhases`, `AIRequest` are missing. `game-server/handlers/aion/gameserver/handlers/ai/` contains **only `AiPrelude.h`** — 0 of the 461 Java AI handlers are ported. | P5-05, A1 | **2,285 Java LOC** across the missing `ai/handler` (1,007), `ai/manager` (815), `ai/follow` (92), `AIActions` (113), `HpPhases` (45), `AIRequest` (15) and `NpcAI` (198), plus **206** for the three root handlers |
| 2 | **The damage arithmetic.** `AttackUtil` is 17 of 17 bodies unported (121 C++ stub lines against 548 Java), `StatFunctions` 23 of 23 (698 Java lines), the write half of `AggroList` 13 of 21, and `DamageList`/`TeamDamageList`/`DamageInfo`/`PlayerAggroList` are wholly unported. `KillCounter.java` has no C++ file. | P5-01 (92 sites) | ~1,550 Java LOC, float-exact |
| 3 | **The skill and effect engine.** `SkillEngine` is unported except the one `AION_PARTIAL` of O-09; `Skill` 48 of 60 bodies, `Effect` 83 of 95, `EffectController` 53 sites, `EffectTemplate` 26 of 27, and 174 more sites across the 184 effect classes. | P5-02 (271), P5-03 (100), P5-04 (74) | ~17,800 Java LOC |
| 4 | **Loot and the item path.** `DropRegistrationService`, `DropService`, `DropDistributionService` are wholly unported; `ItemService::addItem` (all 7 overloads) and all of `ItemPacketService` are unported; `restrictions/` contains only `fwd.h`. | P5-09 (123), P5-07 (109), P5-13 | ~2,400 Java LOC |

**Therefore M5b as written in the milestone list — "a player can fight a monster and the monster fights back" — is not one milestone.** Hole 3
alone is larger than all of wave 5a's ported bodies put together. §9 proposes the split; the short form is:

- **M5b-1 "the monster fights back"** — melee auto-attack both ways, targeting, aggro and hate, NPC AI for an aggressive monster, death,
  respawn, and the experience the kill awards. **One wave of 6 lanes plus a 4-lane gate wave** (rev 2: the sixth stage-1 lane is the P4-11b
  seam that A-06 makes live — §5). This is what this plan specifies in full.
- **M5b-2 "skills"** — SkillEngine, Skill, Effect, EffectController and the effect classes a level-1..10 character and a Poeta monster use,
  `CM_CASTSPELL`. A milestone of its own, probably two waves.
- **M5b-3 "loot"** — drops, the drop list, `ItemService::addItem`, `CM_START_LOOT`/`CM_LOOT_ITEM`, and with them `CM_USE_ITEM`/`CM_MOVE_ITEM`
  (m5a-client-session.md F-2). A third milestone, and the one that finally closes the F-2 list.

M5b-1 is deliberately scoped so that the gate can assert a **complete** fight: a character walks up to a Poeta monster, hits it, the monster
answers, one of them dies, the survivor's experience or the corpse's respawn is checked in packets and in the database. Loot is
`AION_PARTIAL` in M5b-1 and the gate asserts the two things that *are* deterministic on the drop path (the `SM_LOOT_STATUS(LOOT_ENABLE)` that
`registerDrop` sends unconditionally, and its absence when the AI answers `REWARD_LOOT` false).

---

## 2. The path a fight takes, end to end

### 2.1 The trace

Every step names the Java source and the state of the C++ body. "ported" means the body exists and contains no `AION_UNPORTED`.

| # | Step | Java | C++ today | Chunk |
|---|---|---|---|---|
| 1 | Client selects a target: `CM_TARGET_SELECT` → `Player::setTarget` → `PlayerController::onTargetChanged` → `SM_TARGET_SELECTED` + `SM_TARGET_UPDATE` | CM_TARGET_SELECT.java:30-68; PlayerController.java:152 | **ported** (wave B): `clientpackets/CM_TARGET_SELECT.cpp`, `PlayerController.cpp:243-244` | P5-16, P4-11b |
| 2 | Client sends `CM_ATTACK(targetObjectId, attackno, time, type)`; stops the spawn protection; looks the target up in the knownlist; `player.getController().attackTarget(creature, time, false)` | CM_ATTACK.java:36-59 | **missing entirely** — no `CM_ATTACK.cpp`/`.h` | P5-15 |
| 3 | `PlayerController::attackTarget`: `PlayerRestrictions.canAttack`, attack-range check with the first-hit tolerance `PositionUtil.calculateMaxCoveredDistance(owner, 100)`, `GeoService.canSee` → `SM_ATTACK_RESPONSE.TARGET_TOO_FAR_AWAY` / `STOP_OBSTACLE_IN_THE_WAY`, `QuestEngine.onAttack`, the attack-speed hack check, `enterCombat(true)` | PlayerController.java:396-431 | **ported**, but the first statement calls `standins::playerRestrictionsCanAttack`, which is `AION_UNPORTED` (`controllers/ControllerStandIns.cpp:94`) | P4-11b / P5-13 |
| 4 | `CreatureController::attackTarget`: `AttackUtil.calculatePhysAttackResult` (or `calculateMagAttackResult`), sum the damages, `AttackStatus.getBaseStatus`, the 10 % critical-proc effect, broadcast `SM_ATTACK`, `increaseAttackCounter`, notify attack observers, then `target.getController().onAttack(...)` now or through `DelayedOnAttack` after `time` ms | CreatureController.java:307-359 (the proc at **341-345**), 563-583 | **ported**, calling `standins::attackUtilCalculatePhysAttackResult` / `...Mag...` (`ControllerStandIns.cpp:46,51`, both `AION_UNPORTED`) and, at `CreatureController.cpp:375`, `SkillEngine::createCriticalProcEffect`, which is `AION_UNPORTED` (`SkillEngine.cpp:97-100`) and **throws**. This is on the gate's own hot path — see **D14** and item **E-05**. | P4-11b / P5-01 / P5-02 |
| 5 | `AttackUtil.calculatePhysAttackResult` → `calculatePhysicalStatus` (dodge/parry/block/critical rolls) → `StatFunctions.calculateAttackDamage` → `adjustDamageByStatModifiers`, `calculateAdditionalHitCount`, `amplifyDamageByAdditionalHitCount`, `modifyDamageByNpcAi` | AttackUtil.java (548 lines); StatFunctions.java | **17 of 17 bodies unported** (`controllers/attack/AttackUtil.cpp:17-117`); **23 of 23** in `utils/stats/StatFunctions.cpp:22-115` | P5-01 |
| 6 | `CreatureController::onAttack` (private, the real one): cast interruption by `cancelRate`, `notifyAttackedObservers`, `aggroList.addDamage(attacker, damage, notifyAttack, hopType)` (**:244**), `forEachNpc(CREATURE_NEEDS_SUPPORT)`, `lifeStats.reduceHp(...)` (**:248**, with the *original* damage — the clamp of step 7 is on a local copy and never reaches this call), `incrementAttackedCount`, godstone procs | CreatureController.java:220-258 | **ported** (`CreatureController.cpp:253-298`) | P4-11b |
| 7 | `AggroList.addDamage`: `isAware` (knownlist + SANCTUARY + tribe hostility), clamp the damage to the remaining HP, start the hate-reduction task, `StatFunctions.calculateHate(attacker, damage * 10)`, `addDamageAndHate` → `computeIfAbsent(new AggroInfo)` → `owner.getController().onAddHate(creature, isNewInAggroList)` | AggroList.java:37-73, 199-216; StatFunctions.java:267 | `addDamage`, `addHate`, `addDamageAndHate`, `shouldAddHateToMaster`, `isTauntingSpirit`, `startHateReductionTask`, `getMostPlayerDamage`, `stream`, `getTarget` ×2, `streamValidTargets`, `streamValidTargetInfo`, `getFinalDamageList` **unported** (`AggroList.cpp:23-46, 104-143`); `stopHating`, `remove` ×2, `transferDamagesToMaster`, `clear`, `isHating`, `getHate`, `isAware` **ported** (wave 5a needed them for logout) | P5-01 |
| 8 | `CreatureController::onAddHate` → `getAi().onCreatureEvent(AIEventType::ATTACK, attacker)` → `AbstractAI::handleCreatureEvent` → `NpcAI::handleAttack` (via `GeneralNpcAI`) → `AttackEventHandler.onAttack`: leave RETURNING, stop walking, `renewLastAttackedTime`, `setStateIfNot(FIGHT)`, `setTarget(creature)`, `AttackManager.startAttacking`, `ShoutEventHandler.onAttackBegin` | CreatureController.java:175-177; AttackEventHandler.java:24-56; GeneralNpcAI.java:32-34 | `onAddHate` and the `AbstractAI` dispatch are **ported**; `GeneralNpcAI`, `AttackEventHandler` and `NpcAI::handleAttack` **do not exist** | P4-11b ported / P5-05 missing |
| 9 | `AttackManager.startAttacking` → `setFightStartingTime`, `EmoteManager.emoteStartAttacking`, `scheduleNextAttack` → `chooseAttack(npcAI, NpcGameStats.getNextAttackInterval())` → `npcAI.chooseAttackIntention()` (`GeneralNpcAI`: most-hated target, skill or simple) → `SimpleAttackManager.performAttack` → `attackAction` → `npc.getController().attackTarget(target, 0, true)` → step 4 with the npc as attacker | AttackManager.java:20-65; SimpleAttackManager.java:20-90; GeneralNpcAI.java:116-131; NpcGameStats.java:142-158 | **all missing or unported**: `AttackManager`, `SimpleAttackManager`, `EmoteManager`, `GeneralNpcAI` missing; `NpcGameStats::getNextAttackInterval` and `getInitialSkillDelay` `AION_UNPORTED` (`NpcGameStats.cpp:143,188`) | P5-05, P5-01 |
| 10 | The chase: `AttackManager.targetTooFar` → switch to the most hated, `checkGiveupDistance` (chase-target and chase-home from the world map's `aiInfo`), `npc.getMoveController().moveToTargetObject()` | AttackManager.java:67-129 | the move controller is **ported** (`NpcMoveController.cpp`), but it calls `standins::targetEventHandlerOnTargetReached` (`NpcMoveController.cpp:287` → `ControllerStandIns.cpp:17`) and `standins::walkManagerStopWalking` (`NpcMoveController.cpp:412,416` → `ControllerStandIns.cpp:21`), both `AION_UNPORTED`. **These two are prerequisites of A-06, not follow-ups**: they are unreachable today only because no npc has an AI that ever starts a walk or a return, and the first registered `GeneralNpcAI` changes that. Item **E-01a**, stage 1. | P4-11b / P5-05 |
| 11 | Aggro without being hit: `NpcController.see` → `CREATURE_SEE` (or `MovementNotifyTask` → `CREATURE_MOVED`) → `CreatureEventHandler.checkAggro`: state, dead, BLINKING, flag, spawned, `canSee`, SANCTUARY, active map region, `isInSeeRange` (aggro range and aggro angle, short aggro range 4 m), `TribeRelationService.isAggressive/isFriend`, `isEnemyFrom`, `validateAggro` (level difference < 10 or GUARD), `GeoService.canSee` → `CREATURE_AGGRO` → `AggroEventHandler.onAggro` → a 500 ms `AggroNotifier` → `addHate(target, 1)` → step 7 | CreatureEventHandler.java:29-110; AggroEventHandler.java:21-79; Npc.java:204-215 | `NpcController::see` and `TribeRelationService` (8 bodies) are **ported**; `CreatureEventHandler` and `AggroEventHandler` **do not exist**; `NpcAI::handleCreatureMoved`, `handleTargetChanged` **unported** | P4-11b, P5-14 ported / P5-05 missing |
| 12 | Death: `reduceHp` reaches 0 → `onHpChanged` → `getController().onDie(lastAttacker)` → `CreatureController::onDie` (abort move, clear casting, `removeAllEffects`, state DEAD, death observers, broadcast `SM_EMOTION(DIE)`, every known creature `stopHating`) | CreatureLifeStats.java; CreatureController.java:154-170 | **ported** (`CreatureLifeStats.cpp:269-276`, `CreatureController.cpp:203-220`); `EffectController::removeAllEffects` is ported, the rest of `EffectController` is not | P4-11b, P5-01 |
| 13 | Npc death: `NpcController::onDie` — `resetPoolSpot`, `ai.ask(ALLOW_RESPAWN)` → `RespawnService.scheduleRespawn`, then in a try: `ask(ALLOW_DECAY)`, `ask(REWARD_LOOT)`, `ask(REWARD_AP_XP_DP_LOOT)` → `doReward()`, `instanceHandler.onDie`, `ai.onGeneralEvent(DIED)` → `DiedEventHandler.onDie`; then `petLoot`, `scheduleDecayTask`, or instant `delete()` | NpcController.java:137-169; DiedEventHandler.java:13-23; NpcAI.java:147-160 | the controller is **ported** (`NpcController.cpp:162-195`), but **`NpcAI::ask` is unported** and `DummyAI::ask` returns `false` for everything — so today an npc death would schedule no respawn, run no reward and take the `delete_()` arm. `SiegeService::isRespawnAllowed`, which `ask(ALLOW_RESPAWN)` calls, is also `AION_UNPORTED` (`services/SiegeService.cpp:221`) | P4-11b ported / P5-05, P5-12a |
| 14 | The reward: `NpcController::doReward` — `aggroList.getFinalDamageList().toTeamDamages()`, most damage wins, per attacker `StatFunctions.calculateExperienceReward` / `calculateDPReward`, quest `onKill`, `EventService.onPveKill`, `commonData.addExp(xp, Rates.XP_HUNTING, l10n)`, `addDp`, `ask(REWARD_AP)` → `AbyssPointsService.addAp`, `ask(REWARD_LOOT)` → `DropRegistrationService.registerDrop` | NpcController.java:201-273; StatFunctions.java:49-90, 96, 112 | `doReward` is **ported** (`NpcController.cpp:229-278`) and `PlayerCommonData::addExp`/`addDp` are **ported** (`PlayerCommonData.cpp:97-131` and `:224`); the four `StatFunctions` calls go through `ControllerStandIns.cpp:29,33,37` (`AION_UNPORTED`); `DamageList`/`TeamDamageList` are unported; `DropRegistrationService::registerDrop` is unported; `AbyssPointsService::addAp` is unported **and stays so** (C-04, D16) | P5-01, P5-09, P5-08 |
| 15 | Player death: `PlayerController::onDie` — cancel the cast, `RecallService.cancel`, duel branch, release the summon, clear flying states, `super.onDie`, `scheduleShowResurrectionOptions` → `SM_DIE`, instance/map-region `onDie`, `doReward` (PvP), `calculateExpLoss` | PlayerController.java:268-... | **ported** (`PlayerController.cpp`, 0 unported), calling `PvpService::doReward` (unported, `services/PvpService.cpp`) | P4-11b / P5-08 |
| 16 | Revive: `CM_REVIVE(reviveId)` → `PlayerReviveService.bindRevive` / `rebirthRevive` / … | CM_REVIVE.java:32-63 | `CM_REVIVE` **missing entirely**; `PlayerReviveService` 12 of 12 bodies unported (`services/player/PlayerReviveService.cpp:11-56`) | P5-16, P5-08 |
| 17 | Loot: `registerDrop` builds the drop set from `CUSTOM_NPC_DROP` and the global rules, sends `SM_LOOT_STATUS(LOOT_ENABLE)` to every allowed looter and calls `DropService.scheduleFreeForAll`; `CM_START_LOOT` → `requestDropList` → `SM_LOOT_ITEMLIST`; `CM_LOOT_ITEM` → `requestDropItem` → `winningNormalActions` → `ItemService.addItem` | DropRegistrationService.java:59-110; DropService.java:54, 90, 272-276, 473; CM_START_LOOT.java:40-54; CM_LOOT_ITEM.java:28-34 | `DropService` + `DropRegistrationService` + `DropDistributionService` = **123 unported sites, nothing ported**; `CM_START_LOOT` and `CM_LOOT_ITEM` missing; `ItemService::addItem` (7 overloads) and all 9 `ItemPacketService` bodies unported | P5-09, P5-16, P5-07 |
| 18 | Skills: `CM_CASTSPELL` → `PlayerController::useSkill` → `SkillEngine.getSkillFor` → `Skill.useSkill` → `startCast` → `endCast` → `Effect` → `AttackUtil.calculateSkillResult` → step 6 | CM_CASTSPELL.java:36-110; Skill.java:262-702 | `PlayerController::useSkill` is **ported**; `SkillEngine` is unported except the O-09 `AION_PARTIAL`; `Skill` 48/60, `Effect` 83/95 unported; `CM_CASTSPELL` missing | P4-11b ported / P5-02, P5-15 |

### 2.2 Status by area

Counted from `build/msvc/game-server/chunks.json` plus a scan for `AION_UNPORTED()`; "java no cpp" counts Java classes with no C++ file of that
name anywhere in `src`/`generated`/`handlers`.

| Area | Chunk(s) | C++ files | `AION_UNPORTED` sites | Java files | Java LOC | java no cpp |
|---|---|---|---|---|---|---|
| Stats, `AttackUtil`, `AggroList`, `StatFunctions` | **P5-01** | 79 | **92** | 55 | 6,447 | 7 (`KillCounter`, `StatCondition`, the two mastery functions, 3 stat containers) |
| Skill engine, `Skill`, `Effect`, `EffectController` | **P5-02** | 144 | **271** | 119 | 9,505 | 11 (`ChargeSkill`, `PenaltySkill`, the 7 `properties/`, …) |
| Effects A-L | **P5-03** | 132 | **100** | 89 | 4,422 | 0 |
| Effects M-Z | **P5-04** | 150 | **74** | 95 | 3,908 | 0 |
| AI framework + root AI handlers | **P5-05** | 22 | **28** | 81 | 6,249 | **68** (43 root handlers, the whole `ai/handler`, `ai/manager`, `ai/follow`, `AIActions`, `HpPhases`, `AIRequest`, `AIName`) |
| World AI handlers (worlds, siege, portals, events, quests, classNpc, walkers) | **A1** | **0** | 0 | 110 | 6,997 | **110** |
| Drops and the drop list | **P5-09** | 59 | **123** | 30 | 5,960 | 0 |
| Item services (`addItem`, packets) | **P5-07** | 84 | **109** | 59 | 7,178 | 2 |
| Revive, PvP, abyss points | **P5-08** | 79 | **180** | 41 | 5,461 | 1 |
| Restrictions (+ instance) | **P5-13** | 45 | **114** | 52 | 6,985 | 29, incl. **`restrictions/PlayerRestrictions.java` (390 lines)** — the directory holds only `fwd.h` |
| Controllers (the fight loop itself) | **P4-11b** | 95 | **21** — *all of them in `ControllerStandIns.cpp`* | 46 | 5,712 | 0 |
| Combat server packets | P4-16, P4-17 | 486 | 6 (none on the combat path) | 239 | 13,014 | 0 |
| Client packets | P5-15, P5-16 | 29 | 0 | 168 | 9,277 | **154** (36 of 190 CM classes exist) |

Tree total: **2,064 `AION_UNPORTED` call sites, 18 `AION_PARTIAL`**.

`P4-11b`'s 21 sites deserve a line of their own: they are *not* controller bodies. They are `controllers/ControllerStandIns.cpp`, the
C++-only file whose header says it holds "stand-ins for Java methods whose classes have no C++ declaration header yet, so the controller bodies
can be ported line by line against them" (`ControllerStandIns.h:24-31`). The file declares **23** stand-ins, 21 of them `AION_UNPORTED`
(`gameServerUpdateRatio` at `:90` and `isPvpMapHandler` at `:106` have real bodies). **Thirteen** are on the combat path:
`attackUtilCalculatePhysAttackResult`, `attackUtilCalculateMagAttackResult`, `statFunctionsCalculateExperienceReward`,
`statFunctionsCalculateDPReward`, `statFunctionsCalculatePvEApGained`, `playerRestrictionsCanAttack`, `playerRestrictionsCanUseSkill`,
`targetEventHandlerOnTargetReached`, `walkManagerStopWalking`, `shoutEventHandlerOnAttack`, `shoutEventHandlerOnEnemyAttack`,
`playerTeamDistributionServiceDoReward`, `chargeSkillGetAndUse`. **Deleting these stand-ins is the definition of done for M5b-1's call sites**,
and `aiLoggerMoveinfo` can go today: `ai/AILogger.cpp` already has the real `AILogger::moveinfo`, the call sites in `NpcMoveController.cpp` just
were never switched over.

**Five of those call sites carry an unguarded `runtime::cast<ai::NpcAI>` and are the plan's second ordering hazard** (D15):
`NpcController.cpp:337` (`shoutEventHandlerOnEnemyAttack`, reached whenever *anything* damages an npc — the gate's K5),
`PlayerController.cpp:543` (`shoutEventHandlerOnAttack`, reached whenever an npc damages a player — the gate's K6), and
`NpcMoveController.cpp:287, 412, 416`. Both controller sites are faithful ports of Java's `(NpcAI) attacker.getAi()`
(PlayerController.java:451, NpcController.java:303), and **Java is safe there** because `AIEngine.newAI` throws
`IllegalArgumentException("No AI found for name " + name)` for an unregistered name (AIEngine.java:70-71), so an `Npc` always has an `NpcAI`.
The C++ tree breaks that invariant on purpose: with `gameserver.dev.missing_ai_handlers=warn` — which every scenario run sets
(`tests/scenario/ScenarioServers.cpp:23`) and `gs.smoke.startup` sets too (`cmake/RunStartupSmoke.cmake:215`) — `AIEngine::newAI` hands the npc a
`DummyAI<Creature>`, which derives from `AITemplate<Creature>` and **not** from `NpcAI` (`ai/AIEngine.cpp:31-34, 103-112`;
docs/deviations/P4-01.md), so `runtime::cast` throws `ClassCastException` (`runtime/lifetime/Ref.h:373-375`).

### 2.3 The client packets a fight sends

| Packet | Java | C++ | Needed by |
|---|---|---|---|
| `CM_TARGET_SELECT` | CM_TARGET_SELECT.java | **ported** (wave B), with `TargetSelectTest.cpp` | M5b-1 (done) |
| `CM_ATTACK` | CM_ATTACK.java:36-59, 60 lines | **missing** (P5-15) | M5b-1 |
| `CM_HEADING_UPDATE`, `CM_MOTION`, `CM_PLAYER_STATUS_INFO` | — | missing (P5-15/P5-16) | M5b-1 (the real client sends them during a fight) |
| `CM_REVIVE`, `CM_REJECT_REVIVE` | CM_REVIVE.java:32-63 | missing (P5-16) | M5b-1 |
| `CM_CASTSPELL`, `CM_USE_CHARGE_SKILL`, `CM_TOGGLE_SKILL_DEACTIVATE`, `CM_REMOVE_ALTERED_STATE` | CM_CASTSPELL.java:36-110 | missing (P5-15/P5-16) | **M5b-2** |
| `CM_START_LOOT`, `CM_LOOT_ITEM`, `CM_GROUP_LOOT`, `CM_CLIENT_COMMAND_ROLL` | CM_START_LOOT.java:40-54, CM_LOOT_ITEM.java:28-34 | missing (P5-16) | **M5b-3** |
| `CM_USE_ITEM`, `CM_MOVE_ITEM` | — | missing; **blocked** on `ItemMoveService::moveItem` (P5-07, 3 of 4 bodies unported), the `restrictions` package (P5-13, missing entirely) and the `AbstractItemAction` `canAct`/`act` API (P5-07: `ItemActionService` is 2 of 2 unported) | **M5b-3** |
| `CM_EMOTION`, `CM_FRIEND_STATUS`, `CM_SHOW_BLOCKLIST`, `CM_PLAYER_LISTENER`, `CM_INSTANCE_INFO`, `CM_CHECK_PAK` | — | the remaining six of m5a-client-session.md F-2 are **already ported** (all six `.cpp` files exist), so of the nine the session logged only `CM_TARGET_SELECT` (done), `CM_USE_ITEM` and `CM_MOVE_ITEM` are still open | M5b-3 |
| `CM_SUMMON_ATTACK`, `CM_SUMMON_CASTSPELL`, `CM_SUMMON_COMMAND`, `CM_PET` | — | missing | out of scope (summons are a milestone of their own) |

### 2.4 What an aggressive Poeta monster needs, exactly

Measured from the 4.8 data files, because it sizes the AI item and it fixes the gate's target:

- `data/static_data/spawns/Npcs/210010000_Poeta.xml` spawns **149 distinct npc ids** over 1,031 spots; their `ai=` names are `aggressive` 72,
  `general` **55**, `quest_use_item` 12, `noaction` 3, `resurrect` 2, `simple_abyssguard` 2, `postbox` 1, `useitem` 1, `portal_dialog` 1
  (72+55+12+3+2+2+1+1+1 = 149). `220010000_Ishalgen.xml` has **168** ids over the same nine names (79/52/17/4/2/10/1/2/1).
  **Nine AI handler classes cover both start maps**; the three A-06 registers (`GeneralNpcAI` 139 lines, `AggressiveNpcAI` 35, `NoActionAI` 32)
  cover **130 of Poeta's 149 ids (87 %)** and 135 of Ishalgen's 168 (80 %).
- **The other 19 Poeta ids keep a `DummyAI` after A-06** (`quest_use_item` 12, `resurrect` 2, `simple_abyssguard` 2, `postbox` 1, `useitem` 1,
  `portal_dialog` 1), and every one of them throws `ClassCastException` the moment it is attacked or made to walk, for the reason §2.2 gives.
  The gate's scripted path never touches them; the stress run (G-07) and the real-client checklist (§10) do. D15 is the answer.
- Whole tree for scale: `npc_templates.xml` has 63,287 templates over **432 distinct ai names**, led by `aggressive` 39,358 and `general`
  11,770. The 43 root handlers of `data/handlers/ai/` cover the overwhelming majority; the 418 in the sub-packages (chunk A1 = classNpc 4,
  events 16, portals 15, quests 7, siege 21, walkers 3, worlds 44 = 110, plus `instance/` 308 in chunks I1-I6) are per-world and per-instance
  and stay out of M5b.
- `AggressiveNpcAI extends GeneralNpcAI` and adds three overrides (`handleCreatureSee`, `handleCreatureAggro`,
  `handleCreatureNeedsSupportByGuard`); `GeneralNpcAI extends NpcAI` and delegates every hook to one of the `ai/handler/` statics. So the
  handler package is the work, not the handler classes.

---

## 3. Decisions

| # | Decision | Why |
|---|---|---|
| **D1** | **M5b-1 profile.** The M5a D1 `-D` set unchanged, plus `gameserver.npcshouts.enable=false` (the default, `configs/main/AIConfig.cpp:14`) made explicit, `gameserver.rates.xp.solo=1.0, 2.0` (the default) made explicit, **`gameserver.soulsickness.disable=0` (required, see the right-hand column)**, and `gameserver.geodata.enable=false` for `gs.scenario.m5b`. A second gate `gs.scenario.m5b_geo` reuses the wave-B geo gate's machinery. The profile is written out as `game-server/config/m5b.properties.example`, beside `m5a.properties.example` in the **Java** tree (I-01: profile examples live there, outside the chunk manifest). | **`gameserver.soulsickness.disable` is the key the death case depends on, and the stage-1 fixups added it.** `bindRevive` revives a character outside `EVENT_MODE` with `setSoulSickness = true` (PlayerReviveService.java:104-108), which reaches `PlayerController.updateSoulSickness` → `SkillEngine.getInstance().getSkill(player, 8291, deathCount, player).useSkill()` (PlayerController.java:723-733). `SkillEngine::getSkill` is `AION_UNPORTED` until M5b-2 and **throws**, which kills the revive and leaves the gate's character a corpse — §6.3 **P1/P2** is the case. Java's own guard in front of that call is `!player.hasPermission(MembershipConfig.DISABLE_SOULSICKNESS)` (PlayerController.java:723), and `hasPermission(perm)` is `account.getMembership() >= perm` (Player.java:1275-1277); the `@Property` default is **10** (MembershipConfig.java:37-38) and an ordinary account has membership 0, so the default exempts nobody. Setting the key to **0** exempts every account and takes the branch Java itself provides — a configuration answer, not a C++ deviation, and therefore no `docs/deviations` row. It is removed when M5b-2 lands the skill engine. The `updateSoulSickness` body's **first** statement is `player.getActiveHouse()`, which loads the houses through `HousingService` (a database), so the permission cannot be exercised in a unit test — only on a running server. **The rest of the row, unchanged:** the fight path reads `GeoService::canSee` in three places (`PlayerController.java:411`, `SimpleAttackManager.java:75`, `CreatureEventHandler.java:88`) and `gameserver.geodata.cansee.enable` defaults to **true independently of `geodata.enable`** (`GeoDataConfig.cpp:8-9`), so the geo-off gate still exercises the `canSee` call — including `npc->getAi().ask(CONSIDER_BOUNDS_IN_CAN_SEE_CHECK_WHEN_ATTACK*)` at `GeoService.cpp:128,134`, which is one of the unported `NpcAI::ask` arms. Making shouts explicit keeps `NpcShoutsService` (6 unported bodies, P5-14) off the path. |
| **D2** | **The AI registry is compile time, so registering a handler changes every run, including `gs.scenario.m5a`.** M5b-1 registers exactly three root handlers — `GeneralNpcAI` ("general"), `AggressiveNpcAI` ("aggressive"), `NoActionAI` ("noaction") — and keeps `gameserver.dev.missing_ai_handlers=warn` for the other 430 names. **The M5a gate's §5.8/§5.9 expectations must be updated in the same wave** (item G-05). | There is no config that turns a *registered* handler off: `AIEngine::init` walks `handlers::aiHandlerEntries()` (`ai/AIEngine.cpp:76-85`), which the `AION_AI` markers fill at build time. Once `GeneralNpcAI` exists, `NpcAI::handleSpawned` → `SpawnEventHandler.onSpawn` → `npcAI.think()` runs for all 83,872 spawns, `ThinkEventHandler.thinkIdle` starts `WalkManager` for every walker, and walking npcs broadcast `SM_MOVE` inside the M5a gate's level-ready window. `SM_MOVE` and `SM_EMOTION` are **not** in the M5a async-allowed set (`tests/scenario/AsyncAllowed.cpp:12`). |
| **D3** | **Post-spawn skills are `AION_PARTIAL` in M5b-1**, in `NpcSkillList::getPostSpawnSkills` (P5-02), with an allow-list row and a `docs/deviations/P5-02.md` entry that names M5b-2 as the closing item. | `SpawnEventHandler.onSpawn` calls `npc.getSkillList().getPostSpawnSkills()` and then `SkillEngine.getSkill(...).useWithoutPropSkill()` for every entry (SpawnEventHandler.java:20-22), and `SkillEngine::getSkill` is `AION_UNPORTED`. **185 distinct npc ids carry `is_post_spawn="true"`, spread over 13 of the 25 XML files under `data/static_data/npc_skills/`** — `npc_skills.xml` 58, `siege_skills.xml` 19, `instances/300540000_Eternal_Bastion.xml` 44, `instances/301130000_Sauro_Supply_Base.xml` 26, `instances/300800000_Infinity_Shard.xml` 12, `instances/301390000_Drakenspire_Depths.xml` 6, `open_worlds/220080000_Enshar.xml` 6, `instances/300610000_Raksang_Ruins.xml` 4, `instances/301110000_Danuar_Reliquary.xml` 4, `open_worlds/400030000_Transidium_Annex.xml` 3, `instances/301360000_Infernal_Danuar_Reliquary.xml` 2, `instances/301310000_Idgel_Dome.xml` 1. **All 25 files are loaded** — `static_data.xml:105` is `<import file="npc_skills" singleRootTag="true"/>`, a whole-directory import — so reading only the two root files understates the count by 2.4×. None of the 185 is on Poeta or Ishalgen (checked by intersecting the two spawn files' npc ids), so the M5b gate never reaches it — but `gs.smoke.startup` spawns the whole world and would log "Error during spawn:" (VisibleObjectSpawner.cpp:113, Java VisibleObjectSpawner.java:68) for each of them. That is exactly the F-1 class of bug (m5a-client-session.md), and the cheapest correct answer at M5b-1 is a partial that returns an empty list. |
| **D4** | **Skill attacks are out of M5b-1.** `GeneralNpcAI.chooseAttackIntention` must answer `SIMPLE_ATTACK` whenever `SkillAttackManager.chooseNextSkill` would need the skill engine: `chooseSkillAttack` is ported as an `AION_PARTIAL` that returns `false`, and `SkillAttackManager` is a stub file with `AION_PARTIAL` bodies. | `GeneralNpcAI.java:116-138` reaches `SkillAttackManager` (215 Java lines) and `NpcSkillList::getRandomSkill`, which reach `SkillEngine`. Neither of the two gate monsters has any entry in `npc_skills.xml`, so the partial is never reached on the scripted path — but it must **return**, not throw, because the spawn and attack paths have no catch (m5a-plan.md §7). |
| **D5** | **Loot is `AION_PARTIAL` in M5b-1**, in `DropRegistrationService::registerDrop` (P5-09), and the gate asserts the drop path by what the AI answers rather than by an item. | `registerDrop` is 51 Java lines that reach `CUSTOM_NPC_DROP`, `QuestService.getQuestDrop`, the global-rule evaluator (20 further unported predicates) and `DropService.scheduleFreeForAll`. The one thing on it that is deterministic is the unconditional `SM_LOOT_STATUS(npcObjId, LOOT_ENABLE)` to each allowed looter (DropRegistrationService.java:104-106) — and that is behind the partial either way. M5b-1 therefore asserts that `NpcAI::ask(REWARD_LOOT)` answered `true` and that the partial was hit exactly once per kill (the `partial_trace.txt` hit count is an assertion, not a waiver). |
| **D6** | **Damage is asserted as bounds and invariants, never as an exact number; experience is asserted exactly.** | `StatFunctions.calculateAttackDamage` rolls `Rnd.nextInt(1000)` against `getMaxDamageChance`, and `calculatePhysicalStatus` rolls dodge, parry, block and critical. The gate runs a real server in a child process, so `Rnd::seedCurrentThreadForTests` is out of reach. Experience has no randomness: see D7. |
| **D7** | **The experience assertion is an exact integer and the oracle computes it.** For the gate's monster the arithmetic is: `calculateBaseExp` = `round(maxHp × (ratingMultiplier + rank.ordinal() × 0.2f))`; `calculateExperienceReward` = `round(baseExp × instanceHandler.getExpMultiplier() × xpRewardFrom(npcLevel − playerLevel)/100f)`; then `Rates.XP_HUNTING.calcResult` = `(long) min(xp × rate, expNeed × 0.2f)`. | StatFunctions.java:49-90; GeneralInstanceHandler.java:248-251 (`1.25f` in the open world); XPRewardEnum.java:8-24 (`PLUS_1` = 105); Rates.java:13-18, 175-181; PlayerCommonData.java:105-107 (`getExpShown`), 109-115 (`getExpNeed`). Worked for npc **210663** (level 2, `maxHp="199"`, `rating="NORMAL"` → 2.2, `rank="DISCIPLINED"` → ordinal 1 of `NpcRank{NOVICE,DISCIPLINED,…}` → +0.2) against a level-1 character: baseExp = round(199 × 2.4) = **478**; reward = round(478 × 1.25 × 1.05) = **627**; `expNeed` for level 1 = 400 − 0 = **400**, cap = 400 × 0.2f = 80.0; the character gains **80**. **The cap is the point**: a port that forgets `Math.min` awards 627 and the gate catches it. **Correction of record (stage-1 fixups, found independently by two lanes): rev 2 said 1033 and 206, and was one experience-table row off.** `getExpNeed()` is `getStartExpForLevel(level + 1) − getStartExpForLevel(level)` (PlayerCommonData.java:109-115) and `getStartExpForLevel(level)` is `level == 0 ? 0 : experience[level - 1]` (PlayerExperienceTable.java:29-33) — it is **1-based**. `player_experience_table.xml` begins `<exp>0</exp><exp>400</exp><exp>1433</exp>`, so `getStartExpForLevel(1)` = 0 and `getStartExpForLevel(2)` = 400, and a level-1 character needs **400**, not 1433 − 400. Rev 2 read the table as if the index were the level. The value is computed by the oracle, not hardcoded, so a data change moves it. **Where the number is observable** (rev 2 — R1 used to read `players.exp` mid-run, which cannot work: see D17): `addExp` sends `SM_SYSTEM_MESSAGE` `STR_GET_EXP(name, reward)` = msgId **1370000** with the reward as its **second string parameter** (PlayerCommonData.java:215; SM_SYSTEM_MESSAGE.java:16349-16351 and its `writeImpl`, which writes `chatType`, `0`, `senderObjId`, `msgId`, a parameter count and each parameter with `writeS` — so the number is on the wire as a decimal string), and `setExp` sends `SM_STATUPDATE_EXP(getExpShown(), getExpRecoverable(), getExpNeed(), repose, maxRepose)` as five `writeQ`s (PlayerCommonData.java:286-287; SM_STATUPDATE_EXP.java:33-40). |
| **D8** | **`NpcAI::ask` is the switch that turns death into a reward.** It is a required M5b-1 body, and with it `SiegeService::isRespawnAllowed` (P5-12a, one body) and `AIConfig::SHOUTS_ENABLE`. | `DummyAI::ask` returns false for every question (`ai/AITemplate.h`), so today `NpcController::onDie` schedules **no respawn**, runs **no reward** and takes the instant-`delete_()` arm (`NpcController.cpp:167-194`, Java NpcController.java:137-169). Nothing in M5a ever killed an npc, so this has never been observed. |
| **D9** | **Stub-with-warning keeps the M5a rules.** `AION_PARTIAL` returns; `AION_UNPORTED` throws; the gate requires 0 `AION_UNPORTED` hits and `partial_trace.txt` ⊆ `tests/scenario/m5b_partial_allowlist.txt`, a **new** file, and the M5a list stays as it is. | m5a-plan.md D3. The two gates have different allow-lists because M5b-1 legitimately adds partials (D3, D4, D5) that the M5a scripted path must still not reach. |
| **D10** | **The M5b gate reuses the M5a harness wholesale** (`ScenarioServers`, `FakeLoginClient`, `GameSession`, `PacketSequence`, `AsyncAllowed`, `ScenarioDatabase`, `Oracle`, the `decoders/` directory) and adds one `TEST(M5bScenario, Run)` in the same binary with its own output directory, schema pair and `RESOURCE_LOCK`. | The harness is chunk P5-SC and already carries the job object, the schema lease, the per-run log folders and the report reader (m5a-plan.md §5.1). A third gate in the same `RESOURCE_LOCK` costs one more serialized CTest, not a new harness. |
| **D11** | **The gate's monster is npc `210663`** ("juvenile sparkie", level 2) on map 210010000 at **`(1226.22, 1096.57, 141.93)` heading 2**, 53.4 m from the Elyos spawn point `(1212.94, 1044.85, 140.76)` (m5a-plan.md §5.6). **Rev 1 named `(1210.58, 1083.40, 138.75)` heading 12 at 38.7 m; that spot carries `static_id="4"` and is rejected** — see the right-hand column. The oracle's `m5b-monster` command returns only spots with `static_id="0"` and the gate takes the nearest one, so the coordinates above are a default, not a constant. | `210663` has **38 spots on Poeta, all fixed** — no pool, no `walker_id`, no `random_walk` — so `OracleSpot::isPinnedToFixedSpots()` covers it and V2 asserts its exact position to ±0.01 (m5a-plan.md §5.5). But **two of the 38 carry a `static_id`**: `(1193.01, 1087.08, 137.559)` is 3 and `(1210.58, 1083.40, 138.75)` is 4. A `static_id` is not cosmetic: it makes `Npc::hasStatic()` true, which makes `ask(IS_IMMUNE_TO_ABNORMAL_STATES)` answer **true** (NpcAI.java:152), and it puts `GeoService.despawnPlaceableObject` on the death path (`NpcController.cpp:187-190`, Java NpcController.java:164-165) and `spawnPlaceableObject` on the spawn path (VisibleObjectController.java:101-102), plus a `staticId` argument into every `canSee` (`GeoService.cpp:140-143`). None of that throws — `despawnPlaceableObject` is ported (`GeoService.cpp:207-213`) — but a gate should not pick up couplings it did not ask for, and a rev-1 reader would have believed the "plain fixed spot" claim. **What this costs:** with a non-static spot the M5b gate does not cover the placeable-object despawn on the death path; §11 item 6 records that as open. The template gives the gate everything else deterministically: `level="2" maxHp="199" rating="NORMAL" rank="DISCIPLINED" ai="aggressive" srange="8" sangle="270" arange="2" attack_speed="2142" race="BEAST" tribe="MONSTER"`, `<bound_radius front="0.55" side="0.56"/>`, `respawn_time="20"` (`npc_templates.xml:57809-57813`). Level 2 against a level-1 character puts `xpRewardFrom(+1)` = 105 % into the exp assertion instead of the trivial 100 %, and `validateAggro` passes (1 − 2 = −1 < 10). The alternative, `210115` at 21.6 m, is rejected: it has 2 walker spots and 1 `random_walk` spot among its 10, so V2 cannot pin it. **Rev 4 (the gate lane, measured — the correction that closes G-1): `ai="aggressive"` in that template is not what makes an npc aggressive, and this monster never starts a fight.** `ai` only picks the AI class that looks at what it sees (`AggressiveNpcAI` overrides `handleCreatureSee`); whether the npc aggroes is `TribeRelationService.isAggressive(npc, creature)` (CreatureEventHandler.java:87), which for a character ends in `TRIBE_RELATIONS_DATA.isAggressiveRelation(npcTribe, PC)`. `210663` is tribe **MONSTER**, and the real `tribe_relations.xml` MONSTER row (:2170-2173) has **no `<aggro>` element at all** — nor does the PC row (:2302-2305) — so `isAggressiveRelation(MONSTER, PC)` is false, in Java exactly as in the port. What makes the monster fight back is a different relation: `TribeRelationService::isHostile`'s hard-coded `baseTribe == MONSTER && PC` arm. **This does not move the gate off `210663`** — its 199 HP, its 80 experience and its 20 s respawn are what A3, A4, R1 and R4 are built on, and "it fights back when hit" is M5b-1's sentence — but it does mean the gate needs a **second** npc to prove a server can *start* a fight: §6.3 A5b takes npc `210673` ("paruru slowlegs", tribe AGGRESSIVESINGLEMONSTER, whose row *is* `<aggro>PC PC_DARK</aggro>`, :29-31), the nearest aggressive-to-PC spawn on Poeta at 586.6 m and the only one within 60 m of its spot. Of Poeta's 72 `ai="aggressive"` ids only 27 have a tribe that is aggressive to PC. |
| **D12** | **The player's death is arranged, not hoped for.** The gate seeds `player_life_stat.hp` low before the death case, as M5a's Q3 already does for the restore task. | A level-1 Warrior in starter gear beats a level-2 sparkie most of the time, and a gate that depends on losing a random fight is a flaky gate. |
| **D13** | **Faithfulness beats a nicer fight.** Java's unsynchronized `AggroInfo` counters, the `lastAttackMillis` check-then-act in `PlayerController.attackTarget`, the `hateReductionTask == null` read outside the monitor in `AggroList.addDamage` and the `DelayedOnAttack` that runs after the target may have died all port as written, with `// java-race` and a `docs/deviations/<chunk>.md` row where behaviour could diverge. | m5a-plan.md and runtime-architecture.md §3.7 ("Java's check-then-act and lost updates — kept; `// java-race` marker"). `AggroInfo.cpp:18-40` is the pattern to copy. |
| **D14** | **`SkillEngine::createCriticalProcEffect` is ported in M5b-1 as a *reordered* body whose only `AION_PARTIAL` is the arm the gate cannot reach.** Item **E-05**, owner P5-02. The body computes the weapon-group switch first and returns null when `id == 0` (the Java-exact answer); when `id != 0` it marks `AION_PARTIAL("critical proc stumble/stun needs the effect engine (M5b-2)")` and returns null. The `target.getEffectController().isUnderNormalShield()` guard is **skipped**, with a `docs/deviations/P5-02.md` row. | Rev 1 mentioned this call in §2.1 and never turned it into work, while O-01 defers all of `SkillEngine` to M5b-2. It is on the gate's hot path: `CreatureController.attackTarget` calls it for a `Player` attacker whose first attack status is `CRITICAL` on a 10 % `Rnd.chance()` (CreatureController.java:341-345 → `CreatureController.cpp:373-378`), and `SkillEngine.cpp:97-100` is `AION_UNPORTED()` and throws. A 60-attack fight hits it. **Why the reordering is exact and not a fudge:** Java returns null unless the main-hand `ItemGroup` is `POLEARM`, `STAFF`, `GREATSWORD` (→ 8218 stumble) or `BOW` (→ 8217 stun) (SkillEngine.java:206-216); the gate's Elyos Warrior starts with item **100000094 "Training Sword"**, `item_group="SWORD"` (`player_initial_data.xml:8`, `item_templates.xml:375`), so Java's answer is null and so is ours, and `AION_PARTIAL` never fires. Skipping `isUnderNormalShield` (itself `AION_UNPORTED`, `EffectController.cpp:123-125`) can only differ when a shield effect exists, and no effect exists at M5b-1. **Alternative rejected:** leaving `createCriticalProcEffect` `AION_UNPORTED` and allow-listing the throw. There is no allow-list for `AION_UNPORTED` (D9), and the throw would be swallowed by nothing on this path — `attackTarget` has no catch. |
| **D15** | **`AIEngine`'s warn fallback returns an `NpcAI` for an `Npc` owner.** Item **A-00**, owner P5-05, **merged before A-06**. `AIEngine::newAI` gains a `DummyNpcAI final : public NpcAI` used when the owner is an `Npc` and `missing_ai_handlers=warn` replaced a missing handler; it overrides every `NpcAI` hook back to the `AITemplate` no-op and `ask` back to all-false, so **behaviour is byte-for-byte what a `DummyAI<Creature>` does today** and only the static type changes. `docs/deviations/P4-01.md` gains the row; `tests/ai` gains a case that attacks an npc whose AI name is unregistered. | The five unguarded `runtime::cast<ai::NpcAI>` sites of §2.2 are faithful ports of Java casts that cannot fail in Java, and the C++-only `missing_ai_handlers=warn` deviation is what makes them fail. **Fix the deviation where it lives, not at the call sites.** Poeta alone leaves 19 npc ids on a `DummyAI` after A-06 (§2.4) and the whole tree leaves 429 ai names; adding `if (auto npcAi = runtime::as<NpcAI>(...))` to ported controller bodies would spread a C++-only branch into five Java-faithful methods and would have to be re-added to every future `cast<NpcAI>`. **Alternative rejected:** registering all nine start-map handlers in M5b-1 (+563 Java LOC: `AbyssGuardSimpleAI` 75, `ActionItemNpcAI` 99, `PostboxAI` 32, `ResurrectAI` 109, `QuestItemNpcAI` 77, `PortalDialogAI` 171). It would fix Poeta and Ishalgen and nothing else, and it enlarges the longest lane. Those six stay O-08. |
| **D16** | **`AbyssPointsService::addAp` stays `AION_UNPORTED` in M5b-1, and that is what makes R2 assertable.** C-04's PvE arm drops from **R** to **W**. | `NpcController.doReward` reaches `addAp` only inside `if (getOwner().getAi().ask(AIQuestion.REWARD_AP))` (NpcController.java:235-241, the `addAp` call at **:239**). `NpcAI.ask` answers `REWARD_AP` with `wt == ABYSS \|\| wt != ELYSEA && wt != ASMODAE && apRewardingRaces.contains(getRace())` (NpcAI.java:153-156); Poeta is `ELYSEA`, so the answer is **false** for every kill the gate or the real-client checklist makes there. Leaving the body unported turns R2's negative arm into a **self-enforcing** assertion: a port that answers `REWARD_AP` true throws `UnportedException`, which `NpcController::onDie`'s `catch (const std::exception&)` logs as an ERROR (`NpcController.cpp:172-181`), and Q1's "`unported_trace.txt` empty, no ERROR line" fails. Porting `addAp` instead would replace that with a silent wrong answer. |
| **D17** | **Nothing writes `players.exp` between enter-world and logout, so every mid-run database assertion moves to a packet assertion or to a post-quit read.** | `PeriodicSaveService` schedules exactly two tasks — `LegionWarehouseSaveTask` and `ServerRunTimeSaveTask` — in Java (PeriodicSaveService.java:33) and in C++ (`PeriodicSaveService.cpp:105-109`). **There is no periodic character save in 4.8**; `PlayerService::storePlayer` runs only from the enter-world and leave-world paths (`PlayerService.cpp:158`, `PlayerEnterWorldService.cpp:774`). Rev 1's R1 and P3 read `players.exp` "after the kill" with the character still online, which reads the value the row had at login. R1 and P3 are rewritten in §6.3; the database half survives, moved behind the `CM_QUIT` the gate already sends between K7 and K8. |
| **D18** | **The player's HP regenerates during the fight, in Java as in the port, and the gate asserts it rather than forbidding it.** | A `Player`'s life stats use `CreatureLifeStats::triggerRestoreTask` → `scheduleRestoreTask` → **`HpMpRestoreTask`** (CreatureLifeStats.java:241-244), whose `run()` checks only dead / fully-restored / in-world — it has **no AI-state check at all** (LifeStatsRestoreService.java:71-77). Only `NpcLifeStats` and `SummonLifeStats` use `scheduleHpRestoreTask` → `HpRestoreTask`, and that is the one whose `run()` cancels while `getAi().getState() == AIState.FIGHT` (LifeStatsRestoreService.java:52-59; ported faithfully at `LifeStatsRestoreService.cpp:111-118`). So the player's restore is triggered by the first hit that lowers HP (`PlayerLifeStats.java:42-43`), first ticks 1,700 ms later and then every 6,000 ms, and each tick that restores broadcasts `SM_ATTACK_STATUS` with `TYPE.NATURAL_HP` (=3) and sends `SM_STATUPDATE_HP` (CreatureLifeStats.java:172-193, 227-229). **The monster's HP, by contrast, really is monotone** during the fight — not because of the FIGHT check but because nothing triggers an npc restore task at all: the only caller in the whole tree is `NpcController.loseAggro(true)` (NpcController.java:337-342, `NpcController.cpp:369-374`), and the gate's monster never loses aggro. That asymmetry is what A3 and A6 are now written against. |

---

## 4. Work items (M5b-1)

Effort: **S** < 1 agent-day, **M** 1-2, **L** 2-4, **XL** > 4. Need: **R** required, **W** stub-with-warning allowed, **O** optional.
Owner is the chunk; the lane is in §5.

### Integrator

| Id | What | Owner | Deps | Need | Eff |
|---|---|---|---|---|---|
| I-01 | Manifest and profile rows. **Rev 2 corrects rev 1 on both halves.** (a) **`config/m5b.properties.example` needs a real manifest change, not a precedent.** `game-server/config/` does not exist in the tree and `git ls-files cpp/game-server/config*` is empty: `m5a.properties.example` was never committed, and P5-14's only `OTHER_FILES` entry is `cmake/{AppTests,RunStartupSmoke,RunM4Check}.cmake` (`game-server/chunks.cmake:391`). Header-requests.md 5a-pre-9 item (5) describes the M5a example file as a deliverable but no manifest row carries it. So I-01 either adds `config/*.properties.example` to P5-14's `OTHER_FILES` as a **new request** (§7) and commits both example files, or states that the profile files stay local and untracked as they are today — **the integrator decides; the plan must not assume.** (b) **No test directory is "declared".** `tests/handlers_ai_core`, `tests/effects_al` and `tests/effects_mz` are *derived* names (chunks.cmake header §12-15: the default test directory is the target name without `aion_gs_`), none of them appears in a `TESTS` keyword, and their absence is not an error — P5-03 and P5-04 have had none since phase 5 opened and the tree builds green. I-01 therefore creates **only `tests/handlers_ai_core`**, because A-06 and A-08 put files there; `tests/effects_al` and `tests/effects_mz` are M5b-2's business. **DECIDED (integrator, 2026-09-22): neither arm - the premise is wrong.** The profile examples do not belong under `cpp/` at all: `m5a.properties.example` is tracked today at `game-server/config/m5a.properties.example` in the JAVA tree, which is in this repository and outside the chunk manifest (the manifest governs `cpp/` only), and that is where the game server reads its config from. `m5b.properties.example` goes beside it. No `OTHER_FILES` row is needed and none is added. `tests/handlers_ai_core` is created by the lane that puts the first file in it. | manifest | – | R | S |
| I-02 | The header-request batch of §7, decided before stage 1 starts. | manifest | – | R | S |
| I-03 | On demand in stages 2-3: file leases for cross-chunk fixes (one active lease per chunk, released at merge), as m5a-plan.md R-01. | manifest | – | O | S |

### AI framework and the root handlers (P5-05, both parts)

| Id | What | Java refs | Deps | Need | Eff |
|---|---|---|---|---|---|
| A-00 | **`AIEngine::DummyNpcAI`** (D15): when `missing_ai_handlers=warn` replaces a missing handler and the owner is an `Npc`, `AIEngine::newAI` returns a `DummyNpcAI final : public NpcAI` instead of `DummyAI<Creature>`, with every `NpcAI` hook overridden back to the `AITemplate` no-op and `ask` back to all-false. Plus the `docs/deviations/P4-01.md` row and a `tests/ai` case that attacks an npc whose AI name is unregistered and asserts no throw. **Merge first in stage 1, before A-06** — it is the only item that must land before the first registered handler exists. | ai/AIEngine.java:64-83 (Java throws instead, so there is nothing to port — this is the C++-only fallback of docs/deviations/P4-01.md) | – | R | S |
| A-01 | `ai/handler/` — the 13 missing statics: `ActivateEventHandler`, `AggroEventHandler`, `AttackEventHandler`, `CreatureEventHandler`, `DiedEventHandler`, `MoveEventHandler`, `ReturningEventHandler`, `SpawnEventHandler`, `TalkEventHandler`, `TargetEventHandler`, `ThinkEventHandler` (required) and `FollowEventHandler`, `ShoutEventHandler` (W: `ShoutEventHandler` needs `NpcShoutsService`, off by default under D1). `FreezeEventHandler` exists. | ai/handler/*.java (~1,000 LOC) | A-02, A-03 | R | XL |
| A-02 | `ai/manager/` — `AttackManager`, `SimpleAttackManager`, `EmoteManager`, `WalkManager`, `FollowManager` (all new files); `SkillAttackManager` as an `AION_PARTIAL` shell (D4). | ai/manager/*.java (815 LOC total, of which SkillAttackManager is 215); AttackManager.java:20-129; SimpleAttackManager.java:20-90 | B-04, B-05 | R | L |
| A-03 | `NpcAI` — all **27** bodies (`ai/NpcAI.cpp:17-123`): the **13** narrowing accessors (`getObjectTemplate`, `getSpawnTemplate`, `getLifeStats`, `getRace`, `getTribe`, `getEffectController`, `getKnownList`, `getAggroList`, `getSkillList`, `getCreator`, `getMoveController`, `getNpcId`, `getCreatorId`), `isInRange`, `handleActivate`/`Deactivate`/`BeforeSpawned`/`Spawned`/`Despawned`/`Died`/`MoveArrived`/`TargetChanged`/`CreatureMoved`/`MoveValidate`, `isMoveSupported`, `isDestinationReached`, and **`ask`** (D8). Java's `handleCreatureDetected` has no C++ body to fill: it is an empty hook `GeneralNpcAI` overrides. | NpcAI.java:43-198 (`ask` at **147-160**) | A-00, A-01, A-02, C-05 | R | L |
| A-04 | `ai/AIActions` (113 LOC), `ai/HpPhases` (45), `ai/AIRequest` (15), `ai/AIName` (a C++ no-op: the registry entry replaces the annotation, `AbstractAI.h` already says so) — new files. | ai/AIActions.java, HpPhases.java, AIRequest.java | – | R | M |
| A-05 | `ai/follow/FollowStartService` + `FollowSummonTaskAI`, and the `standins::followStartServiceNewFollowingToTargetCheckTask` call site. **W**: an `AION_PARTIAL` is acceptable at M5b-1 because no summon exists on the scripted path. | ai/follow/*.java | – | W | M |
| A-06 | The three registered root handlers with their `AION_AI` markers in `handlers/aion/gameserver/handlers/ai/`: `GeneralNpcAI` (139 LOC), `AggressiveNpcAI` (35), `NoActionAI` (32) = 206. This is the **first file in the whole handler tree**, so it also proves the `aion_gs_handlers_ai_core` target, the `AiPrelude.h` PCH and `aion_gs_regscan`'s marker rules end to end. **Hard prerequisites (rev 2): A-00 and E-01a.** Until both are merged, the first walker route step and the first attack on a `DummyAI` npc throw. **Two more prerequisites the analysis missed, both found by running the wave (stage-1 fixups):** (1) **`WalkerGroup::targetReached`** (`services/WalkerService`/spawnengine, chunk **P4-10**) was `AION_UNPORTED`. Registering the AI handlers makes walkers actually walk, and `gs.smoke.startup` hit it **28 times** in the first seconds. One Java line (WalkerGroup.java:218); the integrator ported it. A-06 cannot merge without it. (2) **E-01b's physical half** — `standins::attackUtilCalculatePhysAttackResult` (`ControllerStandIns.cpp:46`). Rev 2 put all of E-01b in stage 2 on the grounds that "none of these is reachable before its owner lands"; that is wrong for the physical half. With `AggressiveNpcAI` registered a Poeta monster aggroes the gate's character and swings, so `SimpleAttackManager` → `CreatureController::attackTarget` reaches the stand-in and **throws 12 times in `gs.scenario.m5a`**. The physical half of E-01b therefore belongs to stage 1 with E-01a; the magical half and the three `StatFunctions` stand-ins may stay in stage 2. | data/handlers/ai/{GeneralNpcAI,AggressiveNpcAI,NoActionAI}.java; WalkerGroup.java:218 | A-00, A-01..A-04, **E-01a**, **P4-10 `WalkerGroup::targetReached`**, **E-01b (physical half)** | R | M |
| A-07 | `AIEngine::reload` (`ai/AIEngine.cpp:87`). | AIEngine.java | – | O | S |
| A-08 | Tests: `tests/ai` (event dispatch over a real `GeneralNpcAI`, the aggro predicate table of `CreatureEventHandler.checkAggro` (CreatureEventHandler.java:56-110), the giveup-distance table of `AttackManager.checkGiveupDistance` (AttackManager.java:104-129), `NpcAI::ask` per question, A-00's unregistered-name case) and the new `tests/handlers_ai_core`. The fixture already exists: `tests/ai/AiTestSupport.h` builds real `Npc`s from XML text on a `DeterministicExecutor`/`ManualClock`. **Rev 2 adds the two assertions the gate gave up** (§6.3): the first-hit range tolerance of `PlayerController.attackTarget` (a fabricated position inside `attackRange + calculateMaxCoveredDistance(owner, 100)` but outside `attackRange`, with `isHating` false and then true) belongs here or in `tests/controllers`, not in the gate. | – | A-00..A-06 | R | L |

### Damage, hate and reward arithmetic (P5-01)

| Id | What | Java refs | Deps | Need | Eff |
|---|---|---|---|---|---|
| B-01 | `AttackUtil` **physical half**: `calculatePhysAttackResult`, `calculatePhysicalStatus` (the 6-arg overload), `adjustDamageByStatModifiers`, `calculateAdditionalHitCount`, `amplifyDamageByAdditionalHitCount`, `modifyDamageByNpcAi`, `calculateBlockedDamage`, `calculateWeaponCritical`, `getWeaponMultiplier`, `getWeaponGroup`, `randomizeDamage`. | AttackUtil.java (548 lines) | B-02 | R | XL |
| B-02 | `StatFunctions` **combat half**: `calculateAttackDamage`, `adjustDamageByPvpOrPveModifiers`, `checkIsDodgedHit`, `checkIsParriedHit`, `checkIsBlockedHit`, `checkIsPhysicalCriticalHit`, `calculateHate`, `calculateRatingMultiplier`, `getApNpcRating`, `reduceDamageByElementalDefense`, `getElementalDefenseDenominator`. | StatFunctions.java:62-266, 267-... | – | R | L |
| B-03 | `StatFunctions` **reward half**: `calculateExperienceReward`, `calculateBaseExp`, `calculateDPReward`, `calculatePvEApGained`. | StatFunctions.java:49-90, 96, 112 | – | R | S |
| B-04 | `AggroList` write half: `addDamage`, `addHate`, `addDamageAndHate`, `shouldAddHateToMaster`, `isTauntingSpirit`, `startHateReductionTask`, `getMostPlayerDamage`, `stream`, `getTarget` ×2, `streamValidTargets`, `streamValidTargetInfo`, `getFinalDamageList`; `PlayerAggroList::isAware`; `AttackResult::getDamage`/`setShieldType`; `DamageInfo::addDamage`; `DamageList` (5 bodies); `TeamDamageList` (5 bodies); `KillCounter` (new file, 35 Java lines). | AggroList.java:37-73, 150-216; DamageList.java; TeamDamageList.java; KillCounter.java | B-02 | R | L |
| B-05 | `NpcGameStats::getNextAttackInterval` and `getInitialSkillDelay` (`NpcGameStats.cpp:143,188`) — the monster's attack tempo. | NpcGameStats.java:142-158, 226 | – | R | S |
| B-06 | `StatCondition`, `StatWeaponMasteryFunction`, `StatShieldMasteryFunction` (new files) if the level-1 starter gear reaches them; otherwise defer with a note. | model/stats/calc/ | B-01 | W | M |
| B-07 | ~~`PlayerLifeStats::sendGroupPacketUpdate` as `AION_PARTIAL`~~ — **rev 2: nothing to do.** The body at `PlayerLifeStats.cpp:67-72` is already correct: the `AION_UNPORTED()` sits **inside** the `if (getOwner().isInTeam())` arm, which a solo gate character never enters, so the method returns normally on every `onHpChanged`/`onMpChanged`. It is an `AION_UNPORTED`, not an `AION_PARTIAL`, so it can never appear in `partial_trace.txt` — rev 1's allow-list row for it (§6.1) was unreachable and is removed. Left as a dependency note for P5-10. | PlayerLifeStats.java:61-64 | – | **O** | S |
| B-08 | Tests in `tests/stats`: golden vectors for `calculatePhysAttackResult` with a seeded `Rnd` (`Rnd::seedCurrentThreadForTests`), the `AttackStatus` decision table, hate = `calculateHate(attacker, damage*10)`, `calculateBaseExp` for each `NpcRating`×`NpcRank` pair, `calculateExperienceReward` including the `XPRewardEnum` clamps, `Rates.XP_HUNTING`'s `expNeed × 0.2f` cap, `DamageList`/`TeamDamageList` aggregation with a summon. **Every one of these is mutation-proven**: change the rating multiplier, the `Math.round`, the cap or the `×10` hate factor and name the test that goes red. | – | B-01..B-05 | R | L |

### The player side: restrictions, revive, client packets (P5-13, P5-08, P5-12a, P5-15, P5-16)

| Id | What | Java refs | Deps | Need | Eff |
|---|---|---|---|---|---|
| C-01 | `restrictions/PlayerRestrictions` — a **new file**, 390 Java lines, of which M5b-1 needs `canAttack` (and `canUseSkill` as an `AION_PARTIAL` until M5b-2). It is the first statement of `PlayerController::attackTarget`. | PlayerRestrictions.java:206-..., 51-... | – | R | M |
| C-02 | `CM_ATTACK` (P5-15) with its `readImpl` byte vectors, plus `CM_HEADING_UPDATE` and `CM_MOTION` (the real client sends both while fighting). | CM_ATTACK.java:36-59 | C-01 | R | S |
| C-03 | `CM_REVIVE` + `CM_REJECT_REVIVE` (P5-16) and `PlayerReviveService` — 12 unported bodies (`PlayerReviveService.cpp:11-56`), of which M5b-1 needs `bindRevive` and `revive`; the kisk, rebirth, item and instance arms are **W**. **Plus one body outside P5-08 that the analysis missed and that blocks the whole case (stage-1 fixups): `PanesterraService::teleportToStartPosition`** (P5-12b, `services/panesterra/PanesterraService.cpp:108`). `bindRevive` calls it **unconditionally** for every character who is neither in prison nor in `EVENT_MODE` (PlayerReviveService.java:124) — i.e. exactly the gate's death case on Poeta — and it was `AION_UNPORTED`. The Java body is 10 lines and answers false on its first statement for every map outside Panesterra (PanesterraService.java:297-307); `world::isPanesterraMap` already existed. Ported in stage 1; `getTeam(Player&)` stays unported, because `activeFactionTeams` is filled by `createTeams` alone and no M5b-1 path enters Belus, the Transidium Annex, Aspida, Atanatos or Disillon. | CM_REVIVE.java:32-63; PlayerReviveService.java (261 lines); PanesterraService.java:297-307 | – | R | L |
| C-04 | `AbyssPointsService::addAp` (4 bodies, 65 Java lines) and the `PvpService::doReward` path that `PlayerController::doReward` already calls. **Rev 2 drops both to W** (D16): `NpcController.doReward` guards `addAp` with `ask(REWARD_AP)` (NpcController.java:235-241), which `NpcAI.ask` answers **false** on every `ELYSEA` map including Poeta (NpcAI.java:153-156), so nothing in the M5b-1 gate or in the §10 real-client checklist reaches it. Leaving it `AION_UNPORTED` is what makes §6.3 R2 self-enforcing. Port it when a PvP or Abyss milestone needs it. | AbyssPointsService.java; PvpService.java:248-256 | – | **W** | M |
| C-05 | `SiegeService::isRespawnAllowed` (`services/SiegeService.cpp:221`) — **one body**, but it is on **every** npc death through `NpcAI::ask(ALLOW_RESPAWN)`. Cross-chunk (P5-12a): either the siege owner ports it or the lane takes a file lease. | SiegeService.java | – | R | S |
| C-06 | Tests: `tests/cm_ak`/`tests/cm_lz` byte vectors and in-process run tests over `InWorldPacketRunSupport.h` (the fixture `TargetSelectTest.cpp` already uses), `tests/playersvc` for the revive service, `tests/instance` for `PlayerRestrictions.canAttack`'s decision table. | – | C-01..C-05 | R | M |

### Controllers, packets and the app (P4-11b, P4-16, P4-17, P5-14, P5-02)

| Id | What | Java refs | Deps | Need | Eff |
|---|---|---|---|---|---|
| E-01a | **Stage 1, merged before A-06** (D15, §2.1 step 10). The AI-facing half of the stand-in deletion, which A-06 turns from dead code into live code: `targetEventHandlerOnTargetReached` (`NpcMoveController.cpp:287`), `walkManagerStopWalking` (`NpcMoveController.cpp:412,416`), `shoutEventHandlerOnEnemyAttack` (`NpcController.cpp:337`), `shoutEventHandlerOnAttack` (`PlayerController.cpp:543`), and **`aiLoggerMoveinfo`, which can go today** because `ai/AILogger.cpp` already has the body. The four AI stand-ins are replaced by direct calls into A-01's `TargetEventHandler`, `WalkManager` and `ShoutEventHandler`; the two `runtime::cast<ai::NpcAI>` expressions stay exactly as Java writes them, because A-00 restores the invariant they rely on. **This item owns P4-11b for the whole of stage 1** (§5). | ControllerStandIns.h:24-31; ai/handler/{TargetEventHandler,ShoutEventHandler}.java; ai/manager/WalkManager.java | A-00, A-01, A-02 | R | S |
| E-01b | The arithmetic half: `attackUtilCalculatePhysAttackResult`, `attackUtilCalculateMagAttackResult` (`CreatureController.cpp:354,357`), `statFunctionsCalculateExperienceReward`/`DPReward`/`PvEApGained` (`NpcController.cpp:247,248,266`), `playerRestrictionsCanAttack` (`PlayerController.cpp:486`). **Rev 2 put the whole item in stage 2 on the grounds that "none of these is reachable before its owner lands"; the wave disproved that for `attackUtilCalculatePhysAttackResult` and it is now a stage-1 prerequisite of A-06** (see A-06): the first registered `AggressiveNpcAI` makes a Poeta monster swing, and the stand-in threw 12 times in `gs.scenario.m5a`. The rest (the magical half, the three `StatFunctions` stand-ins, `playerRestrictionsCanAttack`) stays in stage 2. `playerRestrictionsCanUseSkill` (`PlayerController.cpp:569`), `chargeSkillGetAndUse` (`:508`) and `playerTeamDistributionServiceDoReward` (`NpcController.cpp:243`) stay as stand-ins until M5b-2/P5-10. | ControllerStandIns.h:24-31 | B-*, C-01 | R | M |
| E-02 | `NpcSkillList::getPostSpawnSkills` as `AION_PARTIAL` returning empty (D3), plus `NpcSkillList::isEmpty`, `getRandomSkill`, `getSkillOnPosition`, `getSkillsByPriority`, `getChainSkills` and `NpcSkillEntry::setLastTimeUsed` as far as `SimpleAttackManager` and `GeneralNpcAI.chooseAttackIntention` need them. (P5-02, but a **shared** chunk with M5b-2 — see the lane note in §5.) | NpcSkillList.java:59-114 | – | R | M |
| E-03 | `CheckOutput` (P5-14): add the combat classes to `zeroLiveClasses()` — `AggroInfo`, `AttackResult`, `DamageList`, `DropNpc` — and to the summary rows; the `AIEventLog` entries per run; and the `knownListNotifyFailures` follow-up that m5a-plan.md §11 leaves open (the row's **value** is a literal `0` in `writeSummary` and is pinned by nothing). | – | – | R | M |
| E-04 | Combat packet fixes found by the gate in P4-16/P4-17 (`SM_ATTACK`'s result list and `criticalProcEffect` block, `SM_ATTACK_STATUS`'s `criticalHit` flag, `SM_EMOTION(DIE)`, `SM_DIE`), and the new independent decoders of §6.5. | – | G-03 | R | M |
| E-05 | **`SkillEngine::createCriticalProcEffect`** (D14), owner **P5-02** — the weapon-group switch of SkillEngine.java:206-216 ported exactly, `AION_PARTIAL` only on the `id != 0` arm, the `isUnderNormalShield` guard skipped with a `docs/deviations/P5-02.md` row, and a `tests/skills` case per `ItemGroup` (SWORD → null with no partial hit; POLEARM/STAFF/GREATSWORD → the 8218 arm, BOW → 8217, both partial-marked). **Stage 1, in the npc-skills-shim lane with E-02** — a 60-attack gate fight reaches this body and today it throws. | SkillEngine.java:193-224 | – | R | S |

### The gate (P5-SC)

| Id | What | Deps | Need | Eff |
|---|---|---|---|---|
| G-01 | `tools/oracle` command **`m5b-monster --map --npc-id [--player-level]`**: the template's level, `maxHp`, rating, rank, `srange`/`sangle`/`arange`, `attack_speed`, race, tribe and **`bound_radius`**; every fixed spot of that id on the map **with its `static_id` and its distance from the race's spawn point**, so the gate can take the nearest spot whose `static_id` is 0 (D11); the spawn's `respawn_time`; and the derived `baseExp`, `experienceReward(playerLevel)` and `Rates.XP_HUNTING` cap (D7), computed from the Java sources exactly as `m5a-creation` computes the base HP/MP. **Rev 2 adds two range outputs** the rewritten A1 needs: `attackRange = 1 + playerAttackRangeStat/1000 + playerBoundRadius.maxOfFrontAndSide + npcBoundRadius.maxOfFrontAndSide` (PositionUtil.java:243-251, 275-287; PlayerGameStats.java:137) and `toleranceRange = attackRange + calculateMaxCoveredDistance(player, 100)` (PositionUtil.java:289-294), so the harness can pick a provably-out-of-range stand-off distance instead of guessing. Plus `tools/oracle` tests. | – | R | M |
| G-02 | `GameSession` builders: `buildCM_ATTACK`, `buildCM_TARGET_SELECT`, `buildCM_REVIVE`, and a `fightUntil(predicate, timeout)` helper that paces `CM_ATTACK` at the attack speed the gate read from `SM_STATS_INFO` (`PlayerController.java:424-426` rejects a second attack within `attackSpeed − 300` ms with `SM_ATTACK_RESPONSE.STOP_WITHOUT_MESSAGE`). | – | R | M |
| G-03 | `TEST(M5bScenario, Run)` — the cases of §6, its own output directory and schema pair, the same `RESOURCE_LOCK`, `m5b_partial_allowlist.txt`, and the CTest registration `gs.scenario.m5b` in `ScenarioTests.cmake`. | G-01, G-02, all of stage 1 | R | L |
| G-04 | Independent decoders (m5a-plan.md D9: written from the Java `writeImpl`, no `serverpackets/` include, body consumed exactly) for `SM_ATTACK`, `SM_ATTACK_STATUS`, `SM_ATTACK_RESPONSE`, `SM_EMOTION`, `SM_DIE`, `SM_LOOT_STATUS`, `SM_STATUPDATE_HP`. | G-03 | R | M |
| G-05 | **Update the M5a gate for D2**: add `SM_MOVE` and `SM_EMOTION` of npcs to `AsyncAllowed.cpp`, re-measure §5.8's level-ready window, and re-check V1-V4 with the three handlers registered. Re-run `gs.scenario.m5a` and `gs.scenario.m5a_geo` and record the new numbers in m5a-plan.md §5. | A-06 | R | M |
| G-06 | `gs.scenario.m5b_geo`: the same scripted fight with `gameserver.geodata.enable=true`. It is the only run in which `GeoService::canSee` can return **false** on the attack path, and therefore the only one that exercises `SM_ATTACK_RESPONSE.STOP_OBSTACLE_IN_THE_WAY` and `SimpleAttackManager`'s 15-second giveup arm (SimpleAttackManager.java:75-82). | G-03 | R | M |
| G-07 | Extend the stress nightly `gs.scenario.m5a_stress` (m5a-plan.md G-01) with fighting clients: 20 `FakeGameClient`s that pull, kill and are killed for 30 minutes under ASan, with the second-granularity census keys. Asserts: 0 reused-id warnings, zombie cut count 0, empty final census, and **0 live `AggroInfo`, `AttackResult` and `DropNpc`** at the end. | G-03, E-03 | R | M |

### Deferred to M5b-2 and M5b-3

| Id | What | Milestone |
|---|---|---|
| O-01 | `SkillEngine` — **all bodies except `createCriticalProcEffect`, which M5b-1 takes as E-05/D14 because it is on the gate's hot path** — `Skill` (48), `Effect` (83), `EffectController` (53, including `isUnderNormalShield`, which E-05 skips), `EffectTemplate` (26), the 174 effect-class bodies, `skillengine/properties/` (7 missing files), `ChargeSkill`, `PenaltySkill`, `WeaponTypeWrapper`. M5b-2 closes E-05's partial arm and the `isUnderNormalShield` deviation together. | M5b-2 |
| O-02 | `AttackUtil` magical half: `calculateMagAttackResult`, `calculateMagicalStatus`, `calculateSkillResult`, `calculateEffectResult`, `calculateMagicalOverTimeSkillResult`; `StatFunctions.calculateMagicalSkillDamage`, `calculateMagicalCriticalRate`, `calculateMagicalResistRate`. | M5b-2 |
| O-03 | `SkillAttackManager` (215 LOC) and `NpcSkillTemplateEntry::conditionReady`/`fireOnEndCastEvents`; the npc skill rotation. | M5b-2 |
| O-04 | `CM_CASTSPELL`, `CM_USE_CHARGE_SKILL`, `CM_TOGGLE_SKILL_DEACTIVATE`, `CM_REMOVE_ALTERED_STATE`. | M5b-2 |
| O-05 | `DropRegistrationService` (26 bodies), `DropService` (15), `DropDistributionService` (4), the global-rule evaluator, `DropNpc`. | M5b-3 |
| O-06 | `ItemService::addItem` (7 overloads) + `addNonStackableItem`/`addStackableItem`/`copyItemInfo`, all 9 `ItemPacketService` bodies, `ItemMoveService::moveItem`, `ItemActionService`, the `AbstractItemAction` `canAct`/`act` API. | M5b-3 |
| O-07 | `CM_START_LOOT`, `CM_LOOT_ITEM`, `CM_GROUP_LOOT`, `CM_CLIENT_COMMAND_ROLL`, **`CM_USE_ITEM`, `CM_MOVE_ITEM`** (closes m5a-client-session.md F-2). | M5b-3 |
| O-08 | The other six root AI handlers the two start maps use: `AbyssGuardSimpleAI` (75), `ActionItemNpcAI` (99), `PostboxAI` (32), `ResurrectAI` (109), and from chunk A1 `QuestItemNpcAI` (77) and `PortalDialogAI` (171). Under `missing_ai_handlers=warn` their npcs keep `DummyAI` and behave exactly as at M5a. | M5b-3 or later |
| O-09 | The remaining 37 root handlers, the 110 A1 world handlers and the 308 instance handlers. | phase 6 |

---

## 5. Lanes

At most 6 lanes per stage; chunks are disjoint within a stage. `tools/oracle` and `game-server/cmake` sit outside the chunk manifest and are
assigned here. **M5b-1 is two stages: a 6-lane porting stage and a 4-lane gate stage.**

| Stage | Lane | Chunks | Items | Tests |
|---|---|---|---|---|
| 1 | **ai** | P5-05 (both parts: `aion_gs_ai`, `aion_gs_handlers_ai_core`) | A-00..A-08 | `tests/ai`, new `tests/handlers_ai_core` |
| 1 | **attack-math** | P5-01 | B-01..B-08 | `tests/stats` |
| 1 | **player-side** | P5-13, P5-08, P5-12a, P5-15, P5-16 | C-01..C-06 | `tests/instance`, `tests/playersvc`, `tests/siege`, `tests/cm_ak`, `tests/cm_lz` |
| 1 | **npc-skills-shim** | P5-02 | E-02, **E-05** | `tests/skills` |
| 1 | **controllers-ai-seam** | **P4-11b** | **E-01a** | `tests/controllers` |
| 1 | **gate-harness** | P5-SC, `tools/oracle` | G-01, G-02 | `tools.oracle`, harness self-tests |
| 2 | **gate** | P5-SC | G-03, G-04, G-06 | `gs.scenario.m5b`, `gs.scenario.m5b_geo` |
| 2 | **m5a-regate** | P5-SC (second lease) *or* the same gate lane serialized | G-05 | `gs.scenario.m5a`, `gs.scenario.m5a_geo` |
| 2 | **controllers-app** | P4-11b, P5-14, `game-server/cmake` | E-01b, E-03 | `tests/controllers`, `tests/app`, `gs.smoke.startup` |
| 2 | **packets** | P4-16, P4-17 | E-04 | `tests/sm_ak`, `tests/sm_lz` |
| 2 | **fixups** | whichever stage-1 chunks the gate names (P5-01 / P5-05 / P5-08 / P5-13) | the gate's findings | owning chunk tests + gate rerun |
| 3 | **stress** | P5-14, P5-SC | G-07 | `gs.scenario.m5a_stress` (nightly) |
| all | integrator | manifest, runtime, leases | I-01..I-03 | full verification |

**Notes on the lane split.**

- **P5-02 is the awkward one.** `NpcSkillList` (needed by `GeneralNpcAI.chooseAttackIntention` and `SpawnEventHandler.onSpawn`) lives in the same
  chunk as the whole skill engine, which is M5b-2. E-02 is therefore a small, sharply bounded lane of its own in stage 1 — it touches only
  `model/skill/NpcSkillList.*` and `NpcSkillEntry.*` — so that the M5b-2 lanes inherit a clean chunk. Merging E-02 into the **ai** lane instead
  would make P5-02 and P5-05 the same lane and block M5b-2's start.
- **`gs.scenario.m5a` must be re-green in the same wave** (D2, G-05). Registering `GeneralNpcAI` is not reversible by configuration, so a wave
  that lands A-06 and does not land G-05 leaves the M5a gate red. If stage 2 cannot afford both, A-06 is the item to hold back, not G-05.
- **Stage 1 now has six lanes, the cap.** The sixth, **controllers-ai-seam**, exists only because of the two ordering findings of rev 2: A-06
  makes `NpcMoveController.cpp:287,412,416` and `NpcController.cpp:337` / `PlayerController.cpp:543` live, and all five are P4-11b files. The
  alternative — giving the **ai** lane a file lease on `ControllerStandIns.*`, `NpcMoveController.cpp`, `NpcController.cpp` and
  `PlayerController.cpp` under I-03 — is cheaper in lanes and worse in review: four of the five edits are inside Java-faithful controller
  bodies and belong to the chunk owner. P4-11b appears in stage 1 (E-01a) and in stage 2 (E-01b, E-03); chunks must be disjoint *within* a
  stage, not across stages, so this is legal. **If stage 1 cannot afford six lanes, merge controllers-ai-seam into the ai lane with a lease and
  say so in the wave report — do not push E-01a to stage 2, because then stage 1 ends red.**
- **Merge order in stage 1.** **A-00 and E-01a before A-06, without exception** (D15). Then: B-02 and B-05 (A-02 reads `calculateHate` and
  `getNextAttackInterval`), then B-01 and B-04, then A-01/A-02, then A-03/A-06. C-01 and G-01/G-02 have no body dependencies and start on day 1.
  C-05 is one body and should be merged first of all, because it is the only cross-chunk blocker. E-05 is one body and has no dependencies.
- **Critical path.** The **ai** lane is the long pole: 2,285 Java LOC across `ai/handler` (1,007), `ai/manager` (815), `ai/follow` (92), `AIActions` (113), `HpPhases` (45), `AIRequest` (15) and `NpcAI` (198), plus 206 in the three root handlers, almost none of it with an existing
  C++ file to fill in, and it is the lane whose `NpcAI::ask` unblocks respawn and reward. Budget it like m5a's stats-skills lane (7-9 days).
  **attack-math** is comparable in size but has existing stub files and frozen headers to port against, which is faster.

---

## 6. Gate specification (`ctest -L scenario`, `gs.scenario.m5b`)

### 6.1 Processes, databases and profile

Identical to m5a-plan.md §5.1 except:

| Piece | M5b |
|---|---|
| Schemas | `aion_ls_test_m5b_<hash>` / `aion_gs_test_m5b_<hash>`, same `SchemaLease` and sweep |
| Output directory | `<bin>/scenario/m5b`, its own `gs_log` and `ls_run` (the M5a rule: never the shared `game-server/log`) |
| `RESOURCE_LOCK` | the same `"aion_game_server_log;aion_login_server_log"` as `gs.scenario.m5a` and `gs.scenario.m5a_geo`, so three geo-capable servers can never run at once |
| Profile | D1: the M5a `-D` set, `gameserver.geodata.enable=false`, `gameserver.npcshouts.enable=false`, `gameserver.rates.xp.solo=1.0, 2.0`, **`gameserver.soulsickness.disable=0`** (without it case K8's revive throws in `SkillEngine::getSkill` — D1), `gameserver.character.reentry.time=1`, `gameserver.shutdown.delay=2`. The same set, with its reasons, is `game-server/config/m5b.properties.example` in the Java tree |
| Allow-list | `tests/scenario/m5b_partial_allowlist.txt`, read by `M5bScenarioTest.cpp` through the compile definition `AION_SCENARIO_M5B_PARTIAL_ALLOWLIST` (§6.5). **Rev 3 (the gate lane, 2026-09-22, measured against a run) makes it three sections and moves three rows out of rev 2's §B.** **§A — rows Q1 asserts were hit at least once:** the M5a startup rows `QuestEngine.cpp:111` and `BaseService.cpp:18`, `SkillEngine::applyEffectDirectly` (O-09, 8 hits per enter world, inherited), **`NpcSkillList::getPostSpawnSkills` (D3)**, **`GeneralNpcAI::chooseSkillAttack` (D4)** and `DropRegistrationService::registerDrop` (D5, **exactly once per kill** — R3, and the count is asserted, not just the membership). **§B — rows Q1 asserts were hit exactly zero times:** `SkillAttackManager` (all four sites), `PlayerRestrictions::canUseSkill` (C-01, the scripted path casts nothing) and `SkillEngine::createCriticalProcEffect` (D14). **§C — rows that are allowed and counted but whose hit count is not pinned,** because reaching them depends on timing or on a configuration the gate does not set: `QuestEngine.cpp:115`, `PvpMapService.cpp:32`, `InstanceService.cpp:133`/`:153`, `PlayerEffectController.cpp:120` (K7b's relogin), `PlayerService.cpp:268` (EMOTIONS_ALL) and `EffectController.cpp:209` (an npc that walks home inside the run). **What rev 2 got wrong, and it is the same mistake twice:** it read D3 and D4 as statements about *data* ("no Poeta npc has a post-spawn skill", "neither gate monster has an `npc_skills.xml` entry") and concluded that the *sites* were unreachable. They are whole-body partials. `SpawnEventHandler::onSpawn` calls `getPostSpawnSkills()` for every one of the ~83,885 spawns whatever their skill lists hold, and `GeneralNpcAI::chooseAttackIntention` calls `chooseSkillAttack` on every attack decision — the data decides what they *return*, not whether they run. A §B row for either would have failed every correct run. `SkillEngine::createCriticalProcEffect` is the one of the three that really is unreachable, because D14's reordering returns null for a SWORD **before** the `AION_PARTIAL` line. `FollowStartService` is not on the list at all: A-05 left no `AION_PARTIAL` site to name. Rev 1's `PlayerLifeStats::sendGroupPacketUpdate` row stays **removed**: that body is an `AION_UNPORTED` inside an `isInTeam()` guard, so it could never appear in `partial_trace.txt` (B-07). |
| Target | npc **210663** at `(1226.22, 1096.57, 141.93)`, D11 — the nearest spot of that id with `static_id="0"`, chosen by the oracle, not hardcoded |
| Run length | one GS per run, cases in fixed order, case K9 writes the stop file |

### 6.2 Cases

Cases K1-K3 are M5a cases 1-4 replayed (login, create an Elyos Warrior on account A, enter world, level ready), reusing the same code paths and
the same `PacketSequence`. From K4 on:

| # | Case | Steps |
|---|---|---|
| K0 | the monster oracle answers | `oracle.py m5b-monster --map 210010000 --npc-id 210663 --player-level 1` returns the template values of D11, the 38 spots with their `static_id` and distance, and the two range numbers of G-01. |
| K4 | **approach, target, and one shot from out of range** | From the spawn point, `CM_MOVE` in 5 m steps to a **stand-off point 12 m** from the chosen spot — comfortably outside the oracle's `toleranceRange` (~3.3 m) and outside the monster's `srange` of 8, so it has not aggroed; send a stop-move and wait 1 s so `MoveController::isInMove()` is false. Then `CM_TARGET_SELECT(objectId of 210663, 0)` — the object id comes from the `SM_NPC_INFO` the level-ready burst delivered, the same decode V1/V2 use — and **one** `CM_ATTACK`. |
| K4b | **close and hit** | `CM_MOVE` to a point **2 m** from the spot (inside `attackRange`, and inside `srange`, so the monster begins to aggro), stop, then the first in-range `CM_ATTACK`. |
| K5 | **the player hits** | `CM_ATTACK(targetObjectId, attackno, time=0, type)` paced at the attack speed read from `SM_STATS_INFO`, until the monster's HP reaches 0 or 60 attacks / 90 s elapse. |
| K6 | **the monster answers** | Runs concurrently with K5 and is asserted from the same recording: the monster must answer the character's damage and attack back. It must NOT be read as unprovoked aggro - a MONSTER-tribe npc never starts a fight (A5a); that half belongs to K8b and npc 210673. |
| K7 | **the monster dies** | The kill's packets, the reward, the respawn and the decay. |
| K7c | **the chase** | **Rev 3 (the gate lane) gives A8 a case of its own and puts it here**, against the *respawned* monster: pull it to 2 m, wait for its first `SM_ATTACK`, then `CM_MOVE` 40 m away and watch what it does. Rev 2 listed A8 under K6, where it would have interrupted the paced fight A3/A4/A7 are measured from, and it cannot go after K8 either, because the character is then dead or freshly revived at its bind point. It is a **non-gating** case (the shape of M5a's case 4b): its assertion is a disjunction by construction — §6.3 A8 — and what the monster chooses depends on the map's `aiInfo`, so a failure must not cost the run its K7b, K8 and K9 evidence. It ends by walking back outside aggro range and waiting, so that K8 can pull the monster from its spot again. |
| K7b | **quit, read the row, re-enter** | `CM_QUIT(0)`. **This is the only point at which the kill's experience reaches the database** (D17): leaving the world runs `PlayerService::storePlayer` → `PlayerDAO::storePlayer` (`PlayerService.cpp:158`). The harness reads `players.exp` here, then seeds `player_life_stat.hp` to 1 (D12) and re-enters after `gameserver.character.reentry.time`. |
| K8 | **the player dies** | Walk to the monster and let it kill the character. Its own fallback `CM_ATTACK` is the EXPECTED path, not a workaround: 210663 is hostile but not aggressive, so standing next to it provokes nothing (A5a). Then `CM_REVIVE(BIND_REVIVE)`. The character stays online. |
| K9 | **reports and shutdown** | The M5a Q8 bar, plus the M5b rows; then the stop file **with the character online**, as M5a does. The shutdown store is P3's second reading point: after the GS exits, `players.exp` is read once more (m5a-plan.md Q7 already relies on the shutdown writing the row). |

### 6.3 Assertions

Assertion ids are `T*` targeting, `A*` attack, `D1a`/`D1b` **death** (not the decisions D1-D18 of §3), `R*` reward and respawn, `P*` player
death and revive, `Q*` the report bar. Rev 2 keeps rev 1's ids so that a reader can diff the two revisions row by row; the rows it rewrote say
so in place, and every rewritten row states **what it proves** and **what it cannot**.

| # | Case | Assertion | What a wrong port does |
|---|---|---|---|
| **T1** | K4 | `SM_TARGET_SELECTED` arrives with the monster's object id, its level and its max/current HP, and the decoder consumes the body exactly. **Rev 3 (the gate lane, measured): the `SM_TARGET_UPDATE` half is inverted, not dropped.** PlayerController.java:153 sends it with `PacketSendUtility.broadcastToSightedPlayers(getOwner(), …)`, which walks the *owner's* known list — and an object is not in its own known list, so the character that changed its target is the one player who never receives it. A solo scenario client cannot observe it at all (measured: the gate timed out waiting for it), which makes rev 2's "`SM_TARGET_UPDATE` is broadcast" another assertion a fake client cannot make. The gate asserts its **absence** instead, and that is a real failure mode: a port that wrote `broadcastPacket(owner, packet, true)` — the overload that *does* include the owner, and the one most neighbouring call sites use — would send it to self and fail. Seeing it arrive needs a second character in sight range, which this gate does not have. | regression guard for wave B; a `toSelf` broadcast where Java has `broadcastToSightedPlayers` |
| **A1a** | K4 | The stand-off `CM_ATTACK` from 12 m is answered by `SM_ATTACK_RESPONSE.TARGET_TOO_FAR_AWAY(attackCounter)` and by **no** `SM_ATTACK`. **Proves:** the range gate of PlayerController.java:406-409 exists and rejects, and that the tolerance of :404-405 is *bounded* (12 m > `attackRange + calculateMaxCoveredDistance(owner, 100)`). **Cannot prove:** that the tolerance is applied at all. | a server with no range check at all, or one whose tolerance is unbounded |
| **A1b** | K4b | The first in-range `CM_ATTACK` from 2 m is answered by an `SM_ATTACK` whose attacker is the player's object id and whose target is the monster's, and by no `SM_ATTACK_RESPONSE`. **Proves:** the whole `CM_ATTACK` → `PlayerRestrictions.canAttack` → range → `GeoService.canSee` → `CreatureController.attackTarget` → `SM_ATTACK` chain. **Cannot prove:** the first-hit tolerance — **rev 2 gives that up deliberately.** The tolerance branch is guarded by `if (!target.getAggroList().isHating(getOwner()))` (PlayerController.java:404), and the monster's `srange` is 8 m, so by the time the character is inside the ~0.6 m tolerance band (`movementSpeed × 100 / 1_000_000`) it is already hating and the branch is dead. Rev 1's A1 was therefore vacuous as scripted. The band moves to a unit test with a fabricated position and `isHating` forced both ways (A-08); §11 item 7 records the one way the gate could take it back. | a broken `canAttack`, a broken knownlist lookup in `CM_ATTACK.runImpl`, a `canSee` that returns false with geo off |
| **A2** | K5 | A `CM_ATTACK` sent **less than `attackSpeed − 300` ms** after the previous one is answered by `SM_ATTACK_RESPONSE.STOP_WITHOUT_MESSAGE` and by no `SM_ATTACK`. | the hack check of PlayerController.java:424-426 |
| **A3** | K5 | Take the `SM_ATTACK_STATUS` stream filtered to `creatureObjectId == monster`. Then: (i) **every one of them carries `type == 5` (`TYPE.REGULAR`) and `logId == 191` (`LOG.REGULAR`)** — the melee auto-attack path calls `onAttack(creature, damage, status, criticalProcEffect)` → the private overload with `TYPE.REGULAR, LOG.REGULAR` (CreatureController.java:185-187); no `type == 3` (`NATURAL_HP`) packet appears, because nothing starts an npc restore task during a fight (D18); (ii) the `hpOrMp` percentage each packet carries is **monotonically non-increasing** and the last is **0**; (iii) their count equals the number of `SM_ATTACK` packets from the player that carried a non-zero total damage — `sendAttackStatusPacketUpdate` fires only when `newHp != previousHp \|\| skillId != 0`, and `skillId` is 0 on this path (CreatureLifeStats.java:109-112). **Proves:** every hit reached `reduceHp` exactly once, and nothing healed the monster. **Cannot prove:** absolute HP for the monster — `SM_ATTACK_STATUS` writes a *percentage*, not `currentHp` (SM_ATTACK_STATUS.java, `writeImpl`: objectId, value, type, `hpOrMp`, skillId, logId, criticalHit). The absolute arithmetic is A4's job. | a lost `reduceHp`, a double `onAttack` from `DelayedOnAttack`, an npc restore task started by mistake, or an `AttackUtil` that returns a constant |
| **A4** | K5 | Two sums over the same recording. (i) **The applied damage sums to exactly `maxHp` (199):** the `value` field of the monster's `SM_ATTACK_STATUS` packets is `previousHp - newHp` with `newHp = clamp(currentHp - value, 0, currentHp)` (CreatureLifeStats.java:102, 110; `CreatureLifeStats.cpp:88-97`), and `TYPE.REGULAR` falls to the `default:` arm of `writeImpl`, so the wire carries it **positive**. Summed from full HP to 0 it is exactly 199. (ii) **The raw damage sums to ≥ 199, and the excess is confined to the last hit:** `SM_ATTACK` carries each `AttackResult.getDamage()` unclamped, and `sum(raw) − sum(applied)` must equal `max(0, lastRawHit − remainingHpBeforeIt)`. Every per-hit raw damage lies in `[0, maxHp]`, and a 0 only accompanies `AttackStatus` DODGE/RESIST. **Proves:** `reduceHp`'s clamp, the `SM_ATTACK`/`SM_ATTACK_STATUS` agreement, and that no damage was invented or lost between them. **Cannot prove — rev 2 corrects rev 1 here:** the clamp in `AggroList.addDamage` (AggroList.java:41-42). That clamp is applied to a **local copy** of the parameter; `CreatureController.onAttack` passes the *original* `damage` to `reduceHp` one line later (CreatureController.java:244 vs :248), and `SM_ATTACK` was already broadcast with the raw `AttackResult` list before either. The clamped total never leaves `AggroList`/`DamageList`, and with a single attacker `DamageInfo.getDamage() / totalDamage` is 1.0 either way, so **no packet and no database column can see it**. It moves to B-08 as a unit assertion: overkill a mock creature and assert `getFinalDamageList()`'s total equals `maxHp`. | (i) and (ii): a missing `Math.clamp` in `reduceHp`, an `SM_ATTACK` that writes the clamped rather than the raw damage, a lost or duplicated hit |
| **A5a** | **K4b**, asserted in K6 | **Rev 4 (the gate lane) turns rev 3's red row into the statement a faithful server actually makes, and splits A5 in two.** The character stops 2 m from the monster — inside its 8 m aggro range and inside the 4 m short aggro range, so `isInSeeRange` is true whichever way it faces — and **does not touch it**. The assertion is that **no `SM_ATTACK` from the monster arrives within 20 s**, *and* that the monster attacks the character throughout the fight it is then dragged into (the K5 recording has monster `SM_ATTACK`s and every one of them targets the character). **Why the row was inverted, with the data.** Which npc starts a fight is a **tribe** decision, not an `ai="aggressive"` decision: `ai` only picks the AI class that looks at what it sees (`AggressiveNpcAI` overrides `handleCreatureSee`), while `TribeRelationService.isAggressive(npc, creature)` (CreatureEventHandler.java:87) decides, and for a character it ends in `TRIBE_RELATIONS_DATA.isAggressiveRelation(npcTribe, PC)`. Npc 210663 is `ai="aggressive"` **and tribe MONSTER**, and the real `tribe_relations.xml` MONSTER row (:2170-2173) has **no `<aggro>` element at all**; neither has the PC row (:2302-2305). So the answer is false in Java exactly as in the port, and rev 3's row asserted a falsehood. What the monster *is*, is **hostile** — `TribeRelationService::isHostile` has a hard-coded `baseTribe == MONSTER && PC` arm — which is why it fights back the instant it is hit, which is precisely what the gate observed. G-1 is closed as "not a defect" in §6.7, and the unit case of both sides is `tests/ai/UnprovokedAggroTest.cpp`. **What it cannot prove alone:** a negative row also passes for a server whose whole aggro chain is dead. A5b is what forbids that, and neither row may be read without the other. | a port that decides aggro from the `ai` attribute instead of the tribe, or an `isAggressiveRelation` that matches base against base (MONSTER's base is MONSTER, so such a bug makes every MONSTER npc aggressive and this row red); and, through its positive half, the loss of the hostile arm that makes the monster answer damage at all |
| **A5b** | **K8b** | **Rev 4 adds the positive half, against a second npc the oracle chooses.** The character — revived by K8, so this is the last case before K9 — walks to npc **210673** ("paruru slowlegs", `ai="aggressive"` like the gate's monster, tribe **AGGRESSIVESINGLEMONSTER**, whose row *is* `<aggro>PC PC_DARK</aggro>`, tribe_relations.xml:29-31), reads its object id out of the `SM_NPC_INFO` of the walk at a 12 m stand-off (outside its `srange` of 7, so nothing can have aggroed yet, and no `SM_ATTACK` from it may appear there either), then stops **2 m** from it — inside its short aggro range of 3 m (`Npc::getShortAggroRange`, `aggroRange < 8 ? aggroRange / 2 : 4`), so its `sangle` cannot decide the case — and **touches nothing**. Within 10 s an `SM_ATTACK` whose attacker is that npc and whose target is the character must arrive, and the same window must contain **no `SM_ATTACK` from the character**, or the row would be about `addDamage` and not about aggro. The id is a constant, every coordinate and every template value comes from `oracle.py m5b-monster` (K0 asserts the tribe, the `ai`, the level, the single spawn spot and that 2 m is inside the short aggro range). **Why this id:** it is the nearest aggressive-to-PC spawn to the Elyos spawn point on the whole of Poeta (586.6 m; the KRALL tursins are at 923 m), it has exactly **one** spot on the map and is the only aggressive-to-PC npc within 60 m of it, and it is level 4 rather than the KRALLs' 8-10. **Why last in the run:** a level 4 npc hitting a level 1 character anywhere earlier would take HP into K5's fight (which the character must win), into K7b's quit or into K8's arranged death; after K8 the only thing left is K9's report bar, which does not care where the character stands, and K9 runs whatever this case does. The case ends by walking 60 m away and, if the npc was faster than the retreat, reviving the character, so that Q1's census and Q2's `Player` row are not left a body to explain. | the whole `CREATURE_SEE`/`CREATURE_MOVED` → `checkAggro` → `CREATURE_AGGRO` → 500 ms `AggroNotifier` → `addHate` → `ATTACK` → `AttackEventHandler` → `AttackManager` chain (steps 8, 9, 11), which nothing else in the gate reaches |
| **A6** | K6 | `SM_STATUPDATE_HP` is sent to the character on every HP change and carries **absolute `currentHp` and `maxHp`** as two `writeD`s (SM_STATUPDATE_HP.java:26-29; PlayerLifeStats.java:40, 95-97). Reconstruct the character's HP from the ordered `SM_ATTACK_STATUS` stream filtered to `creatureObjectId == player`: subtract every `type == 5` (`TYPE.REGULAR`) value, add every `type == 3` (`TYPE.NATURAL_HP`) value, start at `maxHp`. Assert: (i) the reconstruction equals the `currentHp` of the matching `SM_STATUPDATE_HP` at every step; (ii) at least one `type == 5` packet with `value > 0` arrives; (iii) **at least one `type == 3` packet arrives**, and the first one is ≈1,700 ms after the first damage with the rest ≈6,000 ms apart. **Rev 2 replaces rev 1's "strictly decreasing", which was wrong** (D18): a `Player`'s life stats use `HpMpRestoreTask`, whose `run()` has **no AI-state check at all** (LifeStatsRestoreService.java:71-77), so a correct Java server regenerates the character's HP throughout the fight and rev 1's A6 would have failed it. (The mechanism is *not* the player's `DummyAI` never reaching `AIState.FIGHT` — that check lives in `HpRestoreTask`, which only npcs and summons use.) **Proves:** the whole damage-to-self path, the regen rate, and that the two packets agree. **Cannot prove:** that the regen *value* is retail-correct; `getHpRegenRate()` comes from the stat container and has its own tests. | a lost `reduceHp` on the player side, an `SM_STATUPDATE_HP` that sends a percentage, a restore task that never starts or never stops |
| **A7** | K6 | The interval between two consecutive monster `SM_ATTACK`s is within ±25 % of the template's `attack_speed` (2142 ms) after the first. | `NpcGameStats::getNextAttackInterval` (B-05) returning 0 or a constant |
| **A8** | K6 | If the character moves 40 m away, the monster either follows (its `SM_MOVE` positions approach the character) or the fight ends with `TARGET_GIVEUP`; `checkGiveupDistance`'s chase-home limit is read from the map's `aiInfo`. **No `SM_FORCED_MOVE`.** | `AttackManager.targetTooFar` / `checkGiveupDistance` (AttackManager.java:67-129) |
| **D1a** | K7 | `SM_EMOTION` with `EmotionType.DIE` for the monster. **Rev 2 fixes the field:** the last attacker's object id is the **fourth constructor argument and the fifth field on the wire** — `writeD(senderObjectId)`, `writeC(emotionType)`, `writeH(state)`, `writeF(speed)`, then the `case DIE:` arm's `writeD(targetObjectId)` (SM_EMOTION.java:94-97, 128-134). It is 0 only when the creature killed itself (`getOwner().equals(lastAttacker) ? 0 : lastAttacker.getObjectId()`, CreatureController.java:164-165); here it is the player's object id. | `CreatureController::onDie` regression, a decoder that reads the `emotion` field instead of `targetObjectId` |
| **D1b** | K7 | After the death, no further `SM_ATTACK` from the monster, and the monster's `AggroList` is empty — observable as: a second character entering aggro range gets no attack. | the `forEachObject(stopHating)` of `onDie` |
| **R1** | K7, K7b | **Rev 2 splits rev 1's single database read, which could not work** (D17): nothing writes `players.exp` while the character is online. **(a) From packets, in K7:** exactly one `SM_SYSTEM_MESSAGE` with `msgId == 1370000` (`STR_GET_EXP`) arrives, and its **second string parameter parses to exactly the oracle's `m5b-monster` value** (D7: **80** for this pair — rev 2's 206 was one experience-table row off, see the correction in D7). `writeImpl` writes `chatType`, `0`, `senderObjId`, `msgId`, the parameter count and each parameter with `writeS`, so the number is on the wire. The `STR_GET_EXP` *variant* (rather than `STR_GET_EXP_VITAL_BONUS` = 1370001-class) is itself the proof that repose energy was 0 and that the exact number is the whole reward (PlayerCommonData.java:197-219). **(b) Also from packets, in K7:** one `SM_STATUPDATE_EXP` whose `currentExp` (= `getExpShown()`) rose by the same amount and whose `maxExp` (= `getExpNeed()`) is **400** (rev 2 said 1,033) — five `writeQ`s (SM_STATUPDATE_EXP.java:33-40; PlayerCommonData.java:286-287). **(c) From the database, in K7b, after `CM_QUIT`:** `players.exp` equals the value read before the fight plus the same amount, because leaving the world is what runs `PlayerService::storePlayer` (`PlayerService.cpp:158`). **Proves:** the full `calculateExperienceReward` → `Rates.XP_HUNTING` → `addExp` → `setExp` chain and that it is persisted. **Cannot prove:** anything about exp while the character is online — (c) is a *post-quit* read and says nothing about timing. | a port that drops `Math.min(..., expNeed × 0.2f)` awards 627 and fails (a); a port that updates the packet but not `PlayerCommonData.exp` fails (c); a port that sends `STR_GET_EXP2` (no npc name) fails the msgId check |
| **R2** | K7, K7b | `NpcAI::ask` answered as Java does: `ALLOW_RESPAWN` true → a respawn is scheduled (R4 sees it); `REWARD_AP_XP_DP_LOOT` true → `doReward` ran (R1 proves it); `REWARD_AP` **false** → no `SM_ABYSS_RANK`, and `abyss_rank.ap` unchanged **in the K7b post-quit read**, not the mid-run one (D17 — mid-run the row still holds the login value whatever the server did); `ALLOW_DECAY` true → the corpse is **not** deleted immediately. **Rev 2 keeps this row unchanged and re-verified it:** `NpcAI.ask` answers `REWARD_AP` with `wt == ABYSS \|\| wt != ELYSEA && wt != ASMODAE && apRewardingRaces.contains(getRace())` (NpcAI.java:153-156), Poeta is `ELYSEA`, so the answer is false regardless of the BEAST race — the race term is never reached. **The negative arm is self-enforcing** because D16 leaves `AbyssPointsService::addAp` `AION_UNPORTED`: a wrong `true` throws, the `catch (const std::exception&)` of `NpcController.cpp:172-181` logs an ERROR, and Q1's empty `unported_trace.txt` and no-ERROR checks both fail. | with `DummyAI::ask` (all false) the corpse vanishes at once and nothing is rewarded — D8's finding, and the assertion that catches a lane that forgets `ask` |
| **R3** | K7 | `DropRegistrationService::registerDrop` is hit **exactly once** per kill in `partial_trace.txt` (D5). **Rev 2 re-verified the count rev 1's §11 item 3 doubted, and it was wrong:** `ask(REWARD_LOOT)` really is consulted twice per death — once in `NpcController.onDie` for `shouldLoot` and once in `doReward` — but `shouldLoot` only gates `petLoot`, which reads `DropRegistrationService.getCurrentDropMap()` and never calls `registerDrop` (NpcController.java:161-162, 172-184), and `doReward`'s call is inside `if (attacker.equals(winner) && ask(REWARD_LOOT))` with a single attacker (NpcController.java:243-244). One call per kill. | proves `ask(REWARD_LOOT)` reached the drop path; when M5b-3 lands, this row becomes `SM_LOOT_STATUS(LOOT_ENABLE)` (DropRegistrationService.java:104-106) + `SM_LOOT_ITEMLIST` |
| **R4** | K7 | After `RespawnService`'s respawn delay (the spawn's `respawn_time`, 20 s for 210663) plus slack, a new `SM_NPC_INFO` for npc id 210663 arrives at the **same position to ±0.01** and with HP 100 %. Before it, an `SM_DELETE` for the old object id. | `RespawnService` + `resetPoolSpot` + the decay task; a respawn at a wrong z is exactly the geo class of bug F-1 warned about |
| **P1** | K8 | The character dies: `SM_EMOTION(DIE)` for the character, `SM_DIE` about 500 ms later (`scheduleShowResurrectionOptions`), `players.online` stays 1. | `PlayerController::onDie` + `showResurrectionOptions` |
| **P2** | K8 | `CM_REVIVE(BIND_REVIVE)` → the character is alive at its bind point with the HP and MP percentages `PlayerReviveService.bindRevive` sets, and the database row's x/y/z match after the K9 shutdown store. **The observable burst is `SM_CHANNEL_INFO`, `SM_PLAYER_INFO`, `SM_STATS_INFO`, `SM_MOTION` — not `SM_PLAYER_SPAWN`, which rev 2 asserted and which this case can never see.** A character who dies on Poeta and binds to the Elyos spawn point is on the **same map and the same instance**, so `SpawnTask::run` takes the `spawnOnSameMap` arm (TeleportService.java:524-526); `SM_CHANNEL_INFO` + `SM_PLAYER_SPAWN` is the *other* arm, the one that reloads the map and waits for `CM_LEVEL_READY`. The four packets above are the first four of `spawnOnSameMap` in order (TeleportService.java:209-212); after `World.spawn` the same arm also sends `SM_PLAYER_STATE` (from `startProtectionActiveTask`, broadcast with `toSelf = true`) and `SM_ABNORMAL_STATE` (from `updatePlayerEffectIcons`). P2 asserts the first four in order and the **absence** of `SM_PLAYER_SPAWN`. Four further statements of the same arm — `startProtectionActiveTask`, `updatePlayerEffectIcons`, `updateZone` and `setPortAnimation(NONE)` — and three of `bindRevive`/`revive` are unit-asserted in `tests/playersvc` (stage-1 fixups); the one the gate must own is `bindRevive`'s closing `unsetResPosState()`, which no in-process test can reach (see the note in `tests/playersvc/PlayerReviveServiceTest.cpp`). | C-03 |
| **P3** | K8, K9 | Experience loss. At level 1 this is a **negative** assertion, because `PlayerController::onDie` guards `calculateExpLoss` with `getLevel() > 4`. **Rev 2 gives it two observable halves** (D17): (a) **no** `SM_STATUPDATE_EXP` arrives between the death and the revive — `calculateExpLoss` ends by sending one unconditionally (PlayerCommonData.java:144-146), so its absence is the proof the guard held; (b) after the K9 shutdown store, `players.exp` equals the value K7b read. Rev 1 asserted only (b) and read it mid-run with the character still online, which returns the login-time value and would have passed for a server that *did* take the exp loss. | the `getLevel() > 4` guard is easy to drop; (a) catches it within the case, (b) catches it after the server exits. **And a second bug the same row catches:** the full Java guard is `player.getLevel() > 4 && !player.getEffectController().hasAbnormalEffect(Effect::isNoDeathPenalty)` (PlayerController.java:326-327), and `EffectController::hasAbnormalEffect` is `AION_UNPORTED` (`EffectController.cpp:118-120`). At level 1 Java's `&&` short-circuits before it, so a faithful port never evaluates it — but a port that swaps the operands, or that evaluates both eagerly, throws. Q1's empty `unported_trace.txt` is what sees that. |
| **Q1** | K9 | The M5a Q8 bar: `unported_trace.txt` **empty**, `census.txt` empty, lockdep empty, no watchdog dump, no ERROR in either log or in `gs_log/server_errors.log`, `m5a_summary.txt` (`started true`, `exitCode 0`, `knownListNotifyFailures 0`, `liveCountsEnabled true`, no unported client packet class from the scripted path). **Rev 2 replaces "`partial_trace.txt` ⊆ the M5b allow-list with every row hit"**, which contradicted D3, D4 and A-05 — rev 1's own text says those partials are unreachable on the scripted path — **with the two-sided form of §6.1:** `partial_trace.txt` ⊆ the allow-list, **every §A row hit at least once** (with `registerDrop` hit exactly once per kill, R3) **and every §B row hit exactly zero times**. The zero half is the stronger assertion: it is what catches a monster that unexpectedly has a post-spawn skill, an `AttackIntention` that reaches `SkillAttackManager`, or a Warrior whose starter weapon changed under D14. Note the summary file keeps the literal name `m5a_summary.txt` for every scenario run — `CheckOutput.cpp:241` hardcodes it — so the M5b gate reads that name in its own `<bin>/scenario/m5b` directory. | |
| **Q2** | K9 | **Rev 3 (the gate lane, against the numbers E-03 measured while porting `CheckOutput`): three of rev 2's seven classes do not have the shape rev 2 gave them, and the row is rewritten to what the code can actually assert.** What the gate asserts: **`AttackResult` live 0 with `created > 0`** (`AttackUtil` makes one per hit; it travels in `SM_ATTACK`'s result list and in `DelayedOnAttack` and nothing keeps one once the hit was applied — **this is the row that catches §8 risk 2**); **`Player` and `AbyssRank` live 0 with `created > 0`**; **`DropNpc` live 0 and `created` 0** (its only constructor call site is behind `registerDrop`'s partial, so a non-zero `created` means the drop path came back and the gate has to grow with it); **`AggroInfo` `created > 0`**, live *not* asserted; and the per-connection bound on `Account`, `AccountTime`, `PlayerAccountData`, `PlayerCommonData`, `PlayerAppearance`, `ConnectionAliveChecker` and the `*Storage` classes, which is 1 because K9 leaves one client connected. **Why the three changed.** `AggroInfo` is **bounded, not zero**: an `AggroList` is an `OwnedPart` of a `Creature`, so an `AggroInfo` lives exactly as long as the creature that hates; the shutdown does not despawn the world, and since A-06 the npcs of an active region fight each other, so every npc still in a fight legitimately keeps its hate entries (`CheckOutput.h`, measured `34 36` in the M5a gate run after A-06). A strict 0 would have failed a correct run. `KnownObject` is bounded for the same reason and is dropped from the row. `DamageList` **is not counted at all**: it is not `RefCounted` — `AggroList::getFinalDamageList` returns it by value and `NpcController::doReward` holds it in a local — so only `makeRef<T>` objects have a counter and a row for it would be silently dead in every build forever, which is the failure mode m5a-plan.md §10.2 warns about. Its arithmetic is B-08's unit assertion. The leak rev 2 wanted `AggroInfo` to catch cannot hide anyway: an `AggroInfo` that outlives its `AggroList` retains its creature, so it shows up as a census leak or as a live `Npc` that Q3 counts. | an `AttackResult` captured by a task (the `DelayedOnAttack` that pins both creatures), a `DropNpc` created by a drop path that came back without the gate noticing |
| **Q3** | K9 | **Rev 3 (the gate lane, measured): rev 2's equality is not an invariant of this server.** It assumed the gate's own kill is the only npc death of the run. It is not — since A-06 the npcs of the character's active map region fight and kill **each other**, and a killed npc leaves the world at decay and comes back only after its spawn's respawn time, so at the arbitrary instant the stop file is written `live` is `baseline − (npcs that have died and not yet respawned)`, which is *below* the baseline. Measured over three runs: `82127 → 82125` with 18 respawns, `82127 → 82117` with 33, `82127 → 82127` with 2. Rev 2's row would have failed a correct server two runs out of three. What survives and still catches the leak: **`live ≤ baseline`** — a corpse that is never reclaimed is an `Npc` that is alive and not in the world while its replacement is, so it pushes the count *above* the baseline once the respawns catch up — and **`created > baseline`**, which proves `RespawnService` ran at all. The class-naming leak check is Q1's census; R4 is what proves the gate's own monster came back as a new object at the same spot. | a corpse that is never reclaimed — the one leak a fight can introduce that M5a could not |

**Rev 2 note — what the rewrite did *not* change, and why.** Two things that looked broken are not.

1. **A3's packet count was already right, once the filter is stated.** The objection was that the player's HP regeneration adds `SM_ATTACK_STATUS`
   packets during the fight. It does — but those packets carry `creature = the player`, and A3 counts packets whose `creature` is the *monster*.
   The monster's count is exact because nothing starts an npc restore task during a fight (D18). What was genuinely wrong in rev 1's A3 was the
   sentence "the HP values it carries … start at 199 and reach 0": `SM_ATTACK_STATUS` writes a **percentage** in that slot, never an HP value.
   A3 now says which stream it filters and which field it reads.
2. **R3's "exactly once per kill" is correct** — see the row above. Rev 1's own §11 item 3 doubted it on the grounds that `ask(REWARD_LOOT)` is
   consulted twice; the second consultation does not reach `registerDrop`. Q3 is closed in §11.

One more correction of record: the player's regeneration during a fight is **not** caused by a `Player`'s AI never reaching `AIState.FIGHT`.
`LifeStatsRestoreService`'s FIGHT check lives in `HpRestoreTask` (LifeStatsRestoreService.java:52-59), which only `NpcLifeStats` and
`SummonLifeStats` schedule; a `Player` schedules `HpMpRestoreTask` (:71-77), which has no AI check in any code path. The distinction matters because the first explanation implies a bug that a real player
AI would fix, and the second says the behaviour is Java's and permanent (D18).

### 6.4 The geo gate (`gs.scenario.m5b_geo`)

The same script with `gameserver.geodata.enable=true`, `--gtest_filter=M5bScenarioGeo.Run`, `LABELS "scenario;realdata;geo"`, `TIMEOUT 2700`,
the same `RESOURCE_LOCK`, skipping itself without `game-server/data/geo/*.geo`.

> **Rev 3 (the gate lane, built): the three assertions below are NOT in `gs.scenario.m5b_geo`, and the gate says so rather than faking them.**
> All three need the same thing, and it does not exist: *a position the oracle picks from the geo mesh*. `tools/oracle` has a `geo` package, but
> it serves the M4 static-data check and has no line-of-sight probe, and G-01's brief — the `m5b-monster` command — does not include one.
> Hardcoding a position and asserting the obstacle would give a gate that passes while the geometry happens to be in the way and fails when the
> mesh changes; probing a list of candidates and reporting whichever answered would be a row that *cannot fail*, which is the shape §6.6 exists
> to reject. **What the geo gate does assert** is the whole scripted fight against a server that loaded the 151 `.geo` files, and in particular
> **R4's respawn position to ±0.01 m in a world that has its terrain** — which is the one `gs.scenario.m5b_geo`-only row of the §6.6 mutation
> table (a terrain-z snap on the respawn path), and the same argument wave B made for `gs.scenario.m5a_geo`. **Closing the gap** is one oracle
> command — "give me a point at attack range from (x, y, z) whose segment to it crosses the mesh", written against the `.geo` reader
> `tools/oracle/geo` already has — and then the three cases below.

It would add the assertions a geo-off run **cannot** make:

- **GEO-A1**: attacking the monster from behind a rock (a position the oracle picks from the geo mesh) is answered by
  `SM_ATTACK_RESPONSE.STOP_OBSTACLE_IN_THE_WAY` and by **no** `SM_ATTACK` (PlayerController.java:411-412). With geo off,
  `GeoService::canSee` always returns true on a dummy map and this arm is dead code.
- **GEO-A2**: the monster's 15-second giveup arm of `SimpleAttackManager.attackAction` (SimpleAttackManager.java:75-82), reachable only when
  `GeoService.canSee(npc, target)` is false while the target is in attack range.
- **GEO-A3**: the aggro `canSee` of `CreatureEventHandler.java:88` — a character standing behind geometry within 8 m is **not** aggroed.

This is the answer to the gap m5a-plan.md §5.1 "Geodata" left open in writing: "The gather obstacle check and `getClosestCollision` therefore
stay open until the M5b client packet set". `canSee`'s attack call site (`PlayerController.java:411`) is named there as unreachable at M5a;
`CM_ATTACK` is what reaches it.

### 6.5 CTest wiring

- `gs.scenario.m5b`: `LABELS "scenario;realdata"`, `TIMEOUT 900`, the shared `RESOURCE_LOCK`, `SKIP_REGULAR_EXPRESSION "gs\\.scenario\\.m5b: skipped"`,
  and `AION_SCENARIO_REQUIRE=1` by default exactly as `gs.scenario.m5a` has had it since stage 3 (`ScenarioTests.cmake`) — **a skipped gate is
  not a passed gate**.
- `gs.scenario.m5b_geo`: `LABELS "scenario;realdata;geo"`, `TIMEOUT 2700`.
- The gate reads its own allow-list through a second compile definition, `AION_SCENARIO_M5B_PARTIAL_ALLOWLIST`, beside the M5a one; and its
  own schema pair through `ScenarioServers::Config::schemaPrefix` (`"m5b"`), which is also the prefix `createSchemas()` sweeps for abandoned
  schemas, so a milestone only ever reclaims its own.
- `gs.scenario.m5a`, `gs.scenario.m5a_geo`, `gs.smoke.startup`, `gs.smoke.startup_geo` and `gs.m4.check_static_data` keep passing (G-05).
- `M5bScenario.Run` and `M5bScenarioGeo.Run` are `DISABLED` as discovered cases, like their M5a counterparts, so they only ever run once each.

### 6.6 Mutation proof (the standard)

Every assertion above must be watched failing. The minimum set the gate lane must record, with the exact mutation and both quoted outputs.
**Rev 2 adds a third column meaning to "Must fail": three rows say the gate cannot catch the mutation at all.** Those rows are not filler — a
mutation table that only lists catches is how a plan convinces itself its gate is wider than it is. Each of the three names the test that must
catch it instead, and a lane that cannot produce that test has not finished the item.

| Mutation | Must fail | Must stay green |
|---|---|---|
| `CreatureLifeStats::reduceHp`: drop the `Math.clamp` lower bound (allow negative HP) | A4 (i): the applied sum exceeds 199 | A3's count, A1a/A1b |
| `CreatureController::attackTarget`: broadcast the **clamped** damage in `SM_ATTACK` | A4 (ii): `sum(raw) == sum(applied)` with no overkill | A4 (i), A3 |
| `AggroList::addDamage`: drop the `damage >= currentHp` clamp | **B-08's unit assertion only** — rev 2: this mutation is invisible to the gate, which is why it moved out of A4 (see §6.3 A4) | every gate assertion, `gs.scenario.m5b` green |
| `StatFunctions::calculateExperienceReward`: drop `Math.round` (truncate) | R1 (a) and (c) | A1a-A8 |
| `Rates::XP_HUNTING`: drop the `min(..., expNeed × 0.2f)` cap | R1 (a): 627 instead of 80 | everything else |
| `PlayerCommonData::addExp`: send the packet but skip `setExp` | R1 (c) — the post-quit database read | R1 (a) |
| `NpcAI::ask`: answer `REWARD_AP` `true` unconditionally | R2 **and** Q1 (`unported_trace.txt` non-empty, ERROR line) — D16 | R1 |
| `NpcAI::ask`: answer `ALLOW_RESPAWN` `false` | R4 | R1, R2 |
| `AttackEventHandler::onAttack`: drop `setTarget(creature)` | A5 or A7 | A1a-A4 |
| `NpcGameStats::getNextAttackInterval`: return 0 | A7 | A5 |
| `PlayerController::attackTarget`: delete the whole range check | **A1a** (the 12 m shot is answered by `SM_ATTACK`) | A1b |
| `PlayerController::attackTarget`: drop the first-hit range tolerance | **nothing in the gate** — rev 2: the branch is unreachable after K4b's approach, which is why A1 was rewritten. It must fail A-08's unit case instead. | `gs.scenario.m5b` green |
| `LifeStatsRestoreService::HpMpRestoreTask`: add the `AIState.FIGHT` cancel that `HpRestoreTask` has | **A6 (iii)**: no `type == 3` packet arrives | A6 (i)-(ii) |
| `NpcLifeStats::triggerRestoreTask`: schedule `HpMpRestoreTask` instead of `HpRestoreTask` | A3 (i): a `type == 3` packet appears for the monster | A4 |
| `SkillEngine::createCriticalProcEffect`: mark the `id == 0` arm `AION_PARTIAL` too | Q1's §B zero-hit half (D14) | every packet assertion |
| `AIEngine::newAI`: revert A-00's `DummyNpcAI` to `DummyAI<Creature>` | `gs.smoke.startup` / G-07, and any `tests/ai` case that attacks an unregistered-AI npc | `gs.scenario.m5b` — which is the reason D15 is a decision and not a gate row: **the gate's own monster has a registered AI, so the gate cannot catch this.** |
| `CreatureController::onDie`: drop the `stopHating` loop | D1b | D1a |
| `SM_EMOTION`: write `emotion` instead of `targetObjectId` in the `DIE` arm | D1a | D1b |
| a deliberate `AggroInfo` leak (keep a `Ref` in a static) | Q2 | Q1 |
| a terrain-z snap on the respawn path | R4 in `gs.scenario.m5b_geo` only | `gs.scenario.m5b` — which is the reason G-06 exists, the same argument wave B made for `gs.scenario.m5a_geo` |

**Rev 3 — the four mutations the gate lane actually ran (2026-09-22/23), each applied alone, built, run, reverted and re-run.** Quoted from the
runs, not predicted:

| Mutation | Predicted | Measured |
|---|---|---|
| `Rates::XP_HUNTING`: drop `min(…, expNeed × 0.2f)` (`RatesInfo.cpp:66-67`) | R1 (a): 627 instead of 80 | **R1 (a) red, exactly as written**: `expMessages[0].params[1]` was `"627"` against the oracle's `"80"`. R1 (b) went red with it and says more than the plan expected: `currentExp 227` and `maxExp 1033`, i.e. the character had *levelled to 2* on the uncapped reward, which is the retail bug the cap exists to prevent. A1a-A8, D1a, R2, R4 stayed green. |
| `CreatureLifeStats::reduceHp`: drop the clamp's lower bound (`std::clamp(…, minHp, previousHp)` → `std::min(…, previousHp)`) | A4 (i): the applied sum exceeds 199 | **Caught, but by K5, not by A4** — and this corrects the row above it. With negative HP allowed, `isDead()` never becomes true, so the monster does not die at all: `K5: the monster did not die within 84015 ms and 60 attacks`, and A3/A4 never run because there is no kill to sum. The mutation is caught; the case that goes red is K5. (A side effect worth keeping: the 84 s fight reached `AbyssRankUpdateService.cpp:37`, a partial no 90-second run touches, which is now a §C row of the allow-list.) |
| `NpcAI::ask`: answer `ALLOW_RESPAWN` false | R4 | **R4 red, exactly as written**: `npc 210663 did not respawn at (1226.22, 1096.57, 141.93) within 20 s + 25 s`. R1, R2 and D1a stayed green, which is the other half of the row. |
| `SM_EMOTION`: write `emotion` instead of `targetObjectId` in the `DIE` arm | D1a | **D1a red, exactly as written**: `death->targetObjectId` was `0` against the player's `103881`. D1b, R1, R2 and R4 stayed green. |

### 6.7 What the first runs found (2026-09-22/23, the gate lane)

`gs.scenario.m5b` and `gs.scenario.m5b_geo` both run end to end. **The first runs (2026-09-22) were red on three rows, G-1 to G-3 below; the
runs of 2026-09-23, after the stage-2 fixups and the A5 rewrite, are green.** Every other assertion of §6.3 passed in both from the first run
on, with the same numbers: 199 applied damage against a 199 HP monster, 201-225 raw with the excess confined to the killing blow, 80 experience
on the wire and 80 in `players.exp` after the quit, a respawn at the same spot to ±0.01 m with geo off *and* on, `registerDrop` hit exactly
once, and `unported_trace.txt` empty once **G-2** was ported.

What the three rows turned out to be: **G-1 is not a defect** and §6.3's A5 was the thing that had to change (this row, and the gate now
asserts both sides of the tribe decision); **G-2** was a real two-line gap on the death path, and with the two revive predicates ported P1, P2
and P3 (a) pass; **G-3**'s leaked `Player` was G-2's dead character, and with the revive working the final `census.txt` is empty.

**G-1. An `ai="aggressive"` monster does not start a fight — CLOSED, not a defect.** §6.3 A5a/A5b. **The observation stands and the diagnosis
was wrong.** A character that stands 2 m from npc 210663 — inside its 8 m aggro range and inside its 4 m short aggro range, so `isInSeeRange`
is true whichever way it faces — is not attacked for 20 s, on the first monster and on the respawned one alike, while the same monster fights
back correctly the instant it is hit. The first run read that as a defect "inside `CreatureEventHandler::checkAggro` or in what reaches it".
It is not: **Java answers exactly the same, and the reason is the monster's tribe.**

- `ai="aggressive"` only selects the AI class that looks at what it sees (`AggressiveNpcAI` overrides `handleCreatureSee`). Whether an npc
  aggroes is `TribeRelationService.isAggressive(npc, creature)` (CreatureEventHandler.java:87), which for a character ends in
  `TRIBE_RELATIONS_DATA.isAggressiveRelation(npcTribe, PC)`.
- Npc 210663 is `ai="aggressive"` **and tribe MONSTER** (`npc_templates.xml:57809`). The real `tribe_relations.xml` MONSTER row
  (:2170-2173) has **no `<aggro>` element at all**, and the PC row (:2302-2305) has none either, so `isAggressiveRelation(MONSTER, PC)` is
  false — in Java exactly as in this port — and a MONSTER-tribe npc never starts a fight.
- It fights back when hit because `TribeRelationService::isHostile` has a hard-coded `baseTribe == MONSTER && PC` arm. **Hostile, not
  aggressive** — which is precisely the behaviour the gate measured, on both the first and the respawned monster.
- Of Poeta's 72 `ai="aggressive"` ids only 27 have a tribe that is aggressive to PC. The nearest such spawn to the Elyos spawn point is npc
  210673 ("paruru slowlegs", tribe AGGRESSIVESINGLEMONSTER, `<aggro>PC PC_DARK</aggro>`, :29-31) at 586.6 m; the KRALL tursins
  (210160/210161/210162/210181/210182) are at 923 m and the TOWERMAN dukaki towers (210684/210685) at 1,184 m.

**What replaced the row.** §6.3's A5 is now two rows and the gate asserts **both sides**, so neither can be broken without a red test: **A5a**
(K4b/K6) that the MONSTER-tribe monster does *not* attack an untouched character and *does* answer damage, and **A5b** (the new case K8b) that
npc 210673 *does* attack a character that touches nothing, within the window the 500 ms `AggroNotifier` and one walk into attack range need.
The same pair is asserted as a unit case, one npc of each tribe through the shipped handler bodies, in **`tests/ai/UnprovokedAggroTest.cpp`**
(written 2026-09-23, and the authority for this row). **No owner: there is nothing to fix.** What the first run did establish is that the whole
chain of §2.1 steps 8, 9 and 11 is present and runs — A5b is what proves it end to end against a live server. **Measured, 2026-09-23:** the
untouched character is attacked **421 ms** (`gs.scenario.m5b`) and **482 ms** (`gs.scenario.m5b_geo`) after it stops 2 m from npc 210673, i.e.
the 500 ms `AggroNotifier` and nothing else, while the MONSTER-tribe monster is silent for the full 20 s in the same two runs. The proof that
A5b is not vacuous is §6.6's shape: the same case pointed at npc 210663 (two lines in `M5bScenarioTest.cpp`, named there) fails on that row and
on no other.

**G-2. The character's death path throws, and it is two one-line bodies.** §6.3 P1. `PlayerController::scheduleShowResurrectionOptions` →
`showResurrectionOptions` → `SM_DIE(player)`, whose constructor asks the instance handler, and
`GeneralInstanceHandler::allowKiskRevive` and `allowInstanceRevive` are both `AION_UNPORTED` (`GeneralInstanceHandler.cpp:107, :111`). The
scheduled task throws, the log gets `ERROR … Exception in a Runnable execution: scheduled task PlayerController.cpp:450`, **no `SM_DIE` ever
reaches the client**, and the character can never revive. Java's bodies are `return !instance.getTemplate().isInstance();` and
`return instance.getTemplate().isInstance() && getClass() != GeneralInstanceHandler.class || … == WorldType.PANESTERRA;`
(GeneralInstanceHandler.java:269-276). Until they land, P1, P2 and P3 (a) cannot run. **CLOSED 2026-09-23:** both bodies are ported (`typeid(*this) != typeid(GeneralInstanceHandler)` for Java's exact-class test, which `dynamic_cast` would get backwards), pinned by `tests/instance/GeneralInstanceHandlerReviveTest.cpp` with a mutation per row, and P1/P2/P3(a) pass: `P1: SM_DIE arrived 501 ms after the death emotion was read`.

**G-3. S-2 is reproduced, and the leaked class is `Player`.** m5b-client-session.md S-2 asked for a name and the gate gives one. The run that
reached G-2 ended with `WARN [ShutdownHook] - Runtime shutdown: 1 objects removed from the world are still alive`, `1 objects still tracked`,
and a `census.txt` of exactly one row: `class aion::gameserver::model::gameobjects::player::Player  103881  1`. The server log carries the
whole retinue as live-instance leaks — `AbyssRank`, `BlockList`, `Cooldowns` ×2, `Macros`, `PlayerSettings`, `QuestStateList`, `RecipeList`,
13 `PlayerSkillEntry` — i.e. the dead character and everything hanging off it. **What this does and does not say:** the gate reproduces S-2's
two log lines exactly and names the class for *its* trigger, a character that died and could never revive because of G-2. The user's session
had no death, so their leak may have had another trigger; what is now certain is that the class is `Player`, that a `Player` that leaves the
world while dead is not reclaimed, and that the gate will name the class for any trigger from now on. **CLOSED 2026-09-23, and the cause was G-2:** with the revive predicates ported, the character revives, and `census.txt` of the green run holds no rows at all - `0 objects still tracked`, `liveLeaks 0`, `Player` live 0 of 3 created, in the geo run as well. Nothing was weakened to get there; Q1 was not touched. One transient line remains in the non-geo run's log (`Leak census: ... Player ... still alive 0 minutes ..., refcount 65`) while the final counts are clean: the character is still connected when the stop file is written, so the sweep sees references that drop before the final census. It is not S-2's line and Q1 legitimately does not match it.

**One wiring gap the gate had to work around.** `m5a_summary.txt` writes `censusTracked unknown`: E-03 added
`CheckOutput::Summary::censusTracked` and its writer, but no caller fills it — only the caller that performs the shutdown can, and neither
`ShutdownHook.cpp:303` nor `main.cpp:559` passes the `RuntimeLifecycle::ShutdownReport` value on. The gate therefore reads the S-2 number from
the `ShutdownHook` log line instead, asserts the summary row when it carries a number, and **fails when neither source is available**, so the
row can never pass vacuously. Filling the field is a one-line request to the E-03 lane.

---

## 7. Header requests expected

Bodies never need a request (hub-headers.md §14); these are the declaration changes the analysis predicts.

| Request | Kind | For |
|---|---|---|
| New declaration headers for `ai/handler/*`, `ai/manager/*`, `ai/follow/*`, `ai/AIActions.h`, `ai/HpPhases.h`, `ai/AIRequest.h` — **no request needed**, P5-05 owns the files and adds them itself; listed here so the integrator expects ~25 new headers and the `fwd.h` regeneration (`skeleton.py --fwd`) that goes with them | additive (no request) | A-01..A-04 |
| `restrictions/PlayerRestrictions.h` (new file in P5-13) plus its `fwd.h` entry | additive (no request) | C-01 |
| `controllers/ControllerStandIns.h`: **delete** the combat stand-ins, in **two** batches — the four AI-facing ones in stage 1 (`targetEventHandlerOnTargetReached`, `walkManagerStopWalking`, `shoutEventHandlerOnAttack`, `shoutEventHandlerOnEnemyAttack`, plus `aiLoggerMoveinfo`) and the five arithmetic ones in stage 2. Of the file's 23 declarations, 13 are on the combat path and 8 are gone by the end of M5b-1 | signature ×2 | **E-01a** (stage 1), **E-01b** (stage 2), P4-11b |
| `ai/AIEngine.h`: `DummyNpcAI` alongside `DummyAI`, and the `newAI` branch on the owner type | additive (P5-05 owns it) | **A-00**, D15 |
| `skillengine/SkillEngine.h` — none; the declaration of `createCriticalProcEffect` exists at `:98`, only the body changes | – | **E-05**, D14 |
| `model/skill/NpcSkillList.h` — none expected; the declarations exist | – | E-02 |
| P5-14 `CheckOutput`: `zeroLiveClasses()` gains `AggroInfo`, `AttackResult`, `DamageList`, `DropNpc`; the summary gains `aiEventsLogged` and a per-class live-leak row | additive | E-03 |
| P4-01 `AIConfig`: **no new key.** The temptation is a `gameserver.dev.ai_handlers=none` switch so `gs.scenario.m5a` can keep `DummyAI`; **reject it.** It would be a C++-only deviation that makes the two gates test different servers, and the honest answer is G-05 | rejected | D2 |
| `services/SiegeService.h` — none; the declaration exists, only the body is missing | – | C-05 |
| P5-SC: `GameSession` gains the CM builders of G-02 and a `fightUntil` helper (own chunk, no request) | additive (no request) | G-02 |
| **Manifest, and it is a real request, not a precedent.** `game-server/config/` is not in the tree (`git ls-files cpp/game-server/config*` is empty) and P5-14's `OTHER_FILES` is `cmake/{AppTests,RunStartupSmoke,RunM4Check}.cmake` only (`chunks.cmake:391`), so header-requests.md 5a-pre-9 did **not** establish `m5a.properties.example` as an owned file. Either extend P5-14's `OTHER_FILES` to `config/*.properties.example` and commit both examples, or decide the profile files stay untracked. `tests/handlers_ai_core` created (a derived, not declared, directory) | build | I-01 |

---

## 8. Risks

Ordered by what is most likely to be wrong, with the evidence for each.

**Free-threaded porting risks.**

1. **`AggroInfo`'s lost updates are a data race in C++, not just a Java race.** Java's `hate`, `damage`, `lastInteractionTime` and
   `hateReduceCount` are plain `int`/`long` written from the attacking thread, the hate-reduction task and `stopHating` with no lock
   (AggroInfo.java:27-63). The port has this right already — `Field<int32_t>` with three `// java-race` comments
   (`AggroInfo.cpp:18-40`) — and **the new bodies must not "fix" it**. `AggroList.addDamage` reads `hateReductionTask` outside the monitor
   (AggroList.java:43) while `clear` writes it inside one (AggroList.java:129-137); that asymmetry is Java's and stays, with a `// java-race`.
2. **`DelayedOnAttack` outlives its target.** `CreatureController.attackTarget` schedules `onAttack` `time` ms later with raw references in
   Java (CreatureController.java:563-583). The C++ port already holds `Ref`s through `DelayedOnAttack::create`
   (`CreatureController.cpp:103-146`), which means a scheduled attack **pins both creatures** until it runs. Under G-07's 30-minute stress that
   is a real leak surface: 20 clients × one pending attack each × a 2 s attack speed. `cycles.toml` has no row for it because nothing ever
   scheduled one. **Add a row, and let Q2 assert `AttackResult` and `AggroInfo` at 0.**
3. **The AI's `thinking` flag is per object, not per thread.** `AbstractAI::setThinking`/`unsetThinking` are `SYNCHRONIZED(*this)`
   (`AbstractAI.cpp:180-193`), mirroring Java's `synchronized`. Java also has `private static final ThreadLocal<Integer> DEPTH`
   (AbstractAI.java:31) guarding event recursion depth, and it **is** ported — `static inline thread_local std::optional<int32_t> DEPTH{0}`
   (`AbstractAI.h:50-51`), used with a scope-guard reset at `AbstractAI.cpp:141-152`. `ThinkEventHandler.onThink`
   relies on `setThinking` returning false to **drop** a think, so a port that makes it reentrant turns one dropped think into an infinite
   attack loop. This is the single most dangerous body in A-01.
4. **Aggro fires from three threads at once.** `checkAggro` runs from `NpcController::see` (the knownlist update thread), from
   `MovementNotifyTask` (P4-10's periodic task manager) and from `AggroEventHandler`'s 500 ms scheduled `AggroNotifier`. Java's
   `ai.canThink()` and `setStateIfNot(FIGHT)` are the only serialization (CreatureEventHandler.java:90, AttackEventHandler.java:47); the CAS
   semantics of `setStateIfNot` must be exact or two threads both "start" the fight and the monster attacks at double speed. `AbstractAI::setStateIfNot`
   is ported (`AbstractAI.cpp:85-108`) — A-01 must not add a second path around it.
5. **`forEachNpc(CREATURE_NEEDS_SUPPORT)` runs the AI of every npc in the knownlist inside `onAttack`.** `CreatureController.cpp:285` is a
   fan-out from the attacker's thread into arbitrarily many AIs, each of which may take its own monitor, schedule tasks and send packets. It is
   the widest lock-order surface the port has yet had. The lock-order validator (runtime-architecture.md §4.2) will see edges it has never
   seen; expect one round of `// lockdep:` reasons and read them sceptically.
6. **Swallowed exceptions hide the first M5b bug.** `NpcController::onDie` wraps `ask`, `doReward`, `onDie` and `onGeneralEvent` in a
   `catch (const std::exception&)` that only logs (`NpcController.cpp:172-181`, Java NpcController.java:146-155). Every one of the four is new
   M5b code. Q1's "no ERROR line" is what turns that catch into a failure — it must not be relaxed.
7. **Cycle rows that do not exist yet.** `cycles.toml` has rows for `AggroInfo.attacker`, `AggroList.aggroList` and the hate-reduction task
   (lines 53-55), and for the effect observers. It has **none** for the AI: `AttackManager`'s scheduled lambdas capture the `NpcAI`,
   `SimpleAttackManager.SimpleCheckedAttackAction` holds one in a field and nulls it in `run()` (SimpleAttackManager.java:92-112 — the Java
   idiom for exactly this cycle), and `AggroEventHandler.AggroNotifier` holds the npc and the target and nulls both. Each needs a row, and
   §10.5's lesson applies: a body that throws before reaching the null-out leaves the cycle for the stale-pin report to name.

**Where Java's own races are.**

8. `PlayerController.attackTarget` reads and writes `lastAttackMillis` with no lock (PlayerController.java:424-426) — two client packets on two
   IO threads can both pass the check. Kept, with `// java-race`.
9. `AggroList.addDamageAndHate` reads `ai.getHate() == 0` **after** `computeIfAbsent` and before `addDamage`/`addHate`
   (AggroList.java:68-72), so `isNewInAggroList` can be wrong under concurrent attackers and `onAddHate` fires the AI event twice. Kept.
10. `NpcGameStats.isNextAttackScheduled` / `setNextAttackTime` is a check-then-act across two threads (NpcGameStats.java:123-138) and is how
    Java avoids double attacks. It is racy in Java and must stay racy; `SimpleAttackManager`'s `scheduleCheckedAttackAction` is the Java-side
    mitigation and must be ported with it.
11. `Creature.setTarget` is a `SelfOrRef<VisibleObject>` field (runtime-architecture.md §5.1) read by the AI and written by the client thread;
    `CM_TARGET_SELECT.cpp` already documents the one place where the C++ port reads the field once where Java reads it twice, with a deviations
    note. Expect two or three more such notes in A-01.

**Scope and process risks.**

12. **D2 is the plan's biggest single hazard**: landing `GeneralNpcAI` changes what `gs.scenario.m5a`, `gs.scenario.m5a_geo`,
    `gs.smoke.startup` and `gs.smoke.startup_geo` see — walking npcs, `SM_MOVE`, `SM_EMOTION`, npc heading updates and a per-npc `think()` at
    spawn for 83,872 npcs. The startup cost should be small (`ThinkEventHandler.onThink` takes the `thinkInInactiveRegion` arm while no map
    region is active, ThinkEventHandler.java:32-35), **but nobody has measured it and this plan did not build**. Measure `gs.smoke.startup`
    before and after A-06 and put the two numbers in the wave report.
13. **D3's 185 post-spawn ids are a startup landmine**, not a gate one. `gs.smoke.startup` spawns the whole world; the M5b gate does not reach
    them. A lane that skips the partial sees a green gate and a red smoke test. The count is 185 and not 180 because the loader imports the
    whole `npc_skills/` directory (static_data.xml:105), 13 of whose 25 files carry `is_post_spawn="true"`.
14. **The `restrictions` package has no C++ file at all** and belongs to P5-13, a chunk with 114 unported sites that nobody is otherwise
    touching in M5b-1. If the player-side lane is short of time, C-01 is what slips — and without it `PlayerController::attackTarget` cannot
    lose its `playerRestrictionsCanAttack` stand-in, so **E-01b** slips too and the gate cannot run at all. (E-01a does not depend on C-01,
    which is the other reason rev 2 split the item.)
15. **`gs.scenario.m5b` adds a third server to the same `RESOURCE_LOCK`.** `gs.scenario.m5a` takes 32 s and `gs.scenario.m5a_geo` 18-21 s in a
    checked RelWithDebInfo tree (m5a-plan.md §5.1); M5b adds two more serialized runs, and K7's respawn assertion alone costs the spawn's 20 s
    respawn time. Budget the gate at 60-90 s and keep `RESOURCE_LOCK` — the user's machine is the reason it exists.
16. **Float exactness.** `AttackUtil` and `StatFunctions` are the float-heavy code CONVENTIONS.md warns about: `Math.round` is
    `floor(x + 0.5)`, float→int casts saturate in Java and are UB in C++, and MSVC `/O2 /fp:precise` folds hand-written `abs` and loses
    `-0.0f`. CONVENTIONS.md names `stats/AttackUtil` explicitly as a candidate for `/fp:strict`. B-08's golden vectors must be run in
    **RelWithDebInfo as well as Debug**, or the first difference shows up in the user's own play server.
17. **The gate's exact-exp assertion depends on repose energy being 0.** R1 guards itself with the `STR_GET_EXP` message variant, but if a
    future `updateEnergyOfRepose` change gives a fresh character repose, R1 fails for a correct server. The failure message must say so.
18. **A1 (110 world AI handlers) and I1-I6 (308 instance handlers) are not in this plan and are not small**: 0 C++ files against 6,997 +
    ~41,000 Java lines. Nothing in M5b needs them, and `missing_ai_handlers=warn` keeps them **mostly** harmless — with the exception risk 19
    names — but the "43 root handlers" of m5a-plan.md D2 is not the whole AI debt, and the plan should not be read as if it were.
19. **The gate cannot catch the bug D15 fixes, and that is the point of making it a decision.** `gs.scenario.m5b` attacks npc 210663, whose
    `ai="aggressive"` A-06 registers, so its AI is a real `NpcAI` and the five `runtime::cast<ai::NpcAI>` sites succeed. The 19 remaining Poeta
    ids and the 429 remaining ai names are only reached by `gs.smoke.startup` (walking), by **G-07's stress run** (20 clients pulling whatever
    they find) and by **the §10 real-client checklist step 3** (the user attacks whatever they click). If A-00 slips, the first symptom is a
    `ClassCastException` in the user's own session, not a red gate. **Make A-00 the first merge of stage 1 and put its `tests/ai` case in the
    wave report.**
20. **Rev 1's numbers were wrong in six places and all six were counts of data files, not of code** (§2.4 Poeta ids and `general`, Ishalgen ids,
    the tree's distinct ai names, the three-handler coverage percentage, D3's post-spawn ids). Every one came from reading a subset of a
    directory the server imports whole, or from a stale figure. **Any number in this plan that came from an XML tree must be re-derived by the
    oracle, not quoted from here** — G-01 is where `m5b-monster` does that for the gate's monster, and the same rule should apply to §2.4 before
    anyone sizes the AI lane from it.

---

## 9. The split: why M5b is three milestones

The milestone list says "combat and AI, the point at which a player can fight a monster and the monster fights back". That sentence is M5b-1
and nothing else. Here is the arithmetic.

| Milestone | What a player can do at the end | Chunks | `AION_UNPORTED` sites to close | Java LOC to port | Waves |
|---|---|---|---|---|---|
| **M5b-1** melee, aggro, death, respawn, experience | Target a monster, hit it, be hit back, kill it or die, gain the right experience, see it respawn, revive at the bind point | P5-05 (part), P5-01, P5-13 (1 class), P5-08 (part), P5-12a (1 body), P5-15, P5-16, P5-02 (3 classes), P4-11b, P5-SC | ~150 of the 2,064 | ~4,000 | **1 porting wave of 6 lanes + 1 gate wave of 4** |
| **M5b-2** skills and effects | Cast the class skills a level-1..10 character has, see buffs and debuffs, watch a monster use its own skills | P5-02 (269 remaining), P5-03 (100), P5-04 (74), P5-01 (magical half), P5-15 | ~450 | ~17,800 | **2 waves**, probably 3 |
| **M5b-3** loot and items | Loot the corpse, put the item in the bag, move it, use it — closing the last two entries of m5a-client-session.md F-2 | P5-09 (123), P5-07 (109), P5-16, P5-13 (the rest of `restrictions`) | ~250 | ~5,000 | **1-2 waves** |

Three arguments for taking them in that order rather than as one milestone:

1. **M5b-1 has a gate that can pass.** A fight with no skills and no loot is a complete, assertable behaviour: the packets are all ported, the
   experience is an exact integer (D7) and readable from two packets and one post-quit row (R1), the respawn is observable, and the mutation
   table of §6.6 has twenty entries — **three of which are there to record that the gate deliberately cannot catch them** (the `AggroList`
   clamp, the first-hit tolerance, A-00's cast invariant), which is the honest shape of a gate. A combined milestone
   would have no green point between "nothing fights" and "everything fights", which is exactly the shape wave 5a was built to avoid.
2. **M5b-2 is gated on M5b-1's arithmetic, not the other way round.** `Skill.endCast` (146 Java lines, Skill.java:556-702) ends in
   `AttackUtil.calculateSkillResult` → `calculateEffectResult` → `CreatureController::onAttack` — the same step 6 M5b-1 proves. Porting the
   skill engine first means debugging the effect system against an unproven damage path.
3. **M5b-3 is gated on nothing in M5b-2.** Loot could be done second instead, and there is an argument for it (`CM_USE_ITEM` and `CM_MOVE_ITEM`
   are what the real-client session actually logged, and a player notices an unlootable corpse more than a missing skill). The order above
   puts skills second because `NpcSkillList`, `SkillAttackManager` and `createCriticalProcEffect` are already `AION_PARTIAL` shells that M5b-1
   leaves behind (D3, D4, **D14**), and partials left standing for two milestones are how allow-lists rot. **Either order is defensible; the order that is not defensible is all
   three at once.**

---

### Capacity testing is deferred to after M5b-2 (user's decision, 2026-09-23)

`gs.scenario.m5a_stress` is a CHURN run, not a load run: 20 fake clients cycling login, enter world, move and logout for 30 minutes, which is what
shakes out lifetime bugs. It measures nothing about how many players can share a region, and its "20" is not a capacity finding - peak concurrency
was well under it.

A real capacity run is a different test: N clients simultaneously present in one region, moving and fighting, N climbing until tick latency or the
packet backlog degrades, measuring server tick time and memory per player rather than leaks. The harness already has the client, the movement and
the combat pieces.

**The user asked to leave it until M5b-2 is in, and has stress scenarios of their own for it.** The reason is sound: fighting is far heavier
than standing around, abilities are heavier still, and a number taken before the effect system lands would be obsolete the moment it does.
Do not spend a wave on capacity before then.

**Design the scenarios together.** The user brings what they want to see happen; this side brings what the port is most likely to break
under - the free-threaded runtime's contention points, the knownlist and region churn of many players in one place, the effect system's
per-tick work once abilities land, and whatever the census and the zombie breaker have been quietest about. Bring proposals to that
conversation rather than waiting to be handed a list, and expect the user's ideas to cover cases no risk analysis would have picked.

## 10. Real-client checklist (user, after M5b-1)

Prerequisites as m5a-plan.md §8 steps 1-6 (MariaDB, `mygs.properties` from `m5b.properties.example`, geo on, access level 0), then:

1. Enter Poeta with the Elyos character. Monsters now **move**: walkers walk their routes, idle npcs reset their heading. Nothing rubber-bands
   and the GS log has no ERROR line.
2. Walk within about 8 m of a "juvenile sparkie". It turns, closes and attacks. The client's HP bar falls; the damage numbers are plausible for
   a level-1/2 fight.
3. Attack it back (left click / Tab + attack key). The damage numbers appear, the monster's HP bar falls, and the fight ends with the monster
   dead within a handful of hits.
4. The experience bar moves. Compare the amount with `oracle.py m5b-monster --map 210010000 --npc-id <id> --player-level 1`. Note that
   `players.exp` in MariaDB will **not** move until the character logs out (D17): there is no periodic character save in 4.8.
5. Wait for the respawn timer. The monster reappears **at the same spot, at the same height** and with full HP. On a geo-enabled server a wrong
   height here is the F-1 class of bug and the automated gate cannot see it.
6. Run out of aggro range mid-fight: the monster chases, then gives up and walks home to its spawn point.
7. Pull a monster behind a rock and attack: the client shows "obstacle in the way" and the server sends no attack.
8. Let a monster kill you (pull two or three). The death screen appears, the revive options work, and you wake up at the bind point with the
   HP and MP `PlayerReviveService.bindRevive` gives.
9. Kill 10 monsters in a row, **log out**, then check `players.exp` in MariaDB against the sum the oracle computes (the logout is what writes the
   row, D17); check that no npc is missing from the world (walk the same circuit twice).
10. **Attack a non-combat npc** — a quest item npc, the postbox, the resurrect statue, an abyss guard: 19 of Poeta's 149 npc ids still get a
    `DummyAI` after A-06 (§2.4). With A-00 merged this is a normal refusal or a normal fight; without it, the GS log shows a
    `ClassCastException` from `NpcController::onAttack` and the session may drop. **This step exists to prove A-00, because no automated gate
    can** (§8 risk 19).
11. Stay in combat for 10 minutes, then log out and in. No stale session, no stuck "in combat" state, no ERROR line.
12. Send `game-server/log/`, `m5a_summary.txt`, `live_counts.txt` and `partial_trace.txt`. The `AION_PARTIAL` sites a fight reaches are the
    M5b-2 and M5b-3 entry list, exactly as F-2 was for wave B.

---

## 11. Open questions this analysis could not settle without building

Listed so that the first lane does not rediscover them.

1. **What A-06 costs at startup.** `think()` runs once per npc at spawn (SpawnEventHandler.java:16-23) for 83,872 npcs. The inactive-region arm
   is cheap on paper; nobody has measured it. Measure `gs.smoke.startup` before and after.
2. **Whether `StatCondition` and the two mastery stat functions are on a level-1 character's path** (B-06). They have no C++ file; whether the
   starter weapon's stats reach them is a data question best answered by running `calculatePhysAttackResult` once.
3. ~~**The exact `partial_trace.txt` hit counts** of D3/D4/D5 per gate run.~~ **Closed in rev 2.** `ask(REWARD_LOOT)` is indeed consulted twice
   per death, but the `onDie` consultation only gates `petLoot`, which reads `getCurrentDropMap()` and never calls `registerDrop`
   (NpcController.java:161-162, 172-184). `registerDrop` is reached once per kill; D3's and D4's partials are reached **zero** times on the
   scripted path. §6.1's two-section allow-list and Q1's zero-hit half now state both.
4. **Whether `210663`'s 38 Poeta spots are all inside one map region**, which decides whether K8's second approach needs a region move.
5. **Whether any Poeta npc's template gives it a `walker_id` whose route crosses the gate's fight area**, which would add `SM_MOVE` traffic to
   the K5/K6 recording. The oracle's `m5a-spawns` already knows the walker spots; extend it rather than guess. **Rev 2 makes this sharper,**
   because D11 moved the fight to `(1226.22, 1096.57, 141.93)` and because A-06 is what first makes walkers walk at all.
6. **Whether the `static_id` death path deserves its own coverage.** D11 moved the gate off the two `210663` spots that carry one, so
   `GeoService::despawnPlaceableObject` on the death path (`NpcController.cpp:187-190`) and `spawnPlaceableObject` on the respawn path are now
   uncovered by `gs.scenario.m5b`. The cheap answer is one extra kill in `gs.scenario.m5b_geo` against the `static_id="3"` spot at
   `(1193.01, 1087.08, 137.559)`, where the geo map actually changes state; the honest alternative is to record it as a known gap. **Decide
   before G-06 is written.**
7. **Whether the first-hit range tolerance can be brought back into the gate at all.** A1b gave it up because an `aggressive` monster with
   `srange="8"` is already hating by the time the character is inside the ~0.6 m band (§6.3 A1b). The one idea that would work: run the tolerance
   check against a **non-aggressive** Poeta npc (one of the 55 `ai="general"` ids), which never aggros, so `isHating` stays false and the band is
   stable. That needs `PlayerRestrictions.canAttack` (C-01) to permit attacking it, which this analysis did not settle. If it does, K4 gains a
   third shot and A1 gets its third half; if it does not, A-08's unit case is the whole answer.
8. **What A-00's `DummyNpcAI` does to `NpcAI`'s narrowing accessors.** After A-03 those accessors have real bodies that `DummyNpcAI` will
   inherit. They are pure narrowings of the owner (`getObjectTemplate`, `getAggroList`, …) and should be harmless, but nobody has run them
   against an npc whose handler never registered. The `tests/ai` case of A-00 should call each one once.
