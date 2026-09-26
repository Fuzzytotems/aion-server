# M5g work plan (groups: parties, alliances, leagues, find group)

> **Status:** plan **rev 2**, 2026-09-23 — rev 1 revised after its adversarial review (13 findings: 3 high, 6 medium, 4 low; §14 lists them
> and what changed). A **read-only** analysis. Rev 1 read HEAD `c1edb0afb`; rev 2 re-checked every cited C++ site at HEAD **`760e8ab5c`**
> ("M5b-2 stage 1 part 3: the effect classes"), whose working tree modifies only `AggroList.cpp`, `DuelService.cpp`, `CMakeLists.txt` and
> `tools/oracle` — **no P5-10 file** (checked with `git status`); P5-10 was re-counted at that tree: still **294** `AION_UNPORTED`, **0**
> `AION_PARTIAL`. **Nothing was compiled, built or run for this plan.** C++ statements come from reading both trees, from
> `game-server/chunks.cmake` and `tools/porting/chunks.py files|owner|java`, and from counting `AION_UNPORTED(` / `AION_PARTIAL(` sites. The
> undeclared-body counts come from a throw-away script over `tools/gen/javasrc.py` that lists every Java method with a body and looks for its
> name in the C++ `.h`, `.cpp` and generated header of the same class (session scratchpad, re-runnable). Data statements come from parsing
> `data/static_data` with ElementTree. §12 separates what was **measured** from what was **inferred**.
>
> It follows the shape of [m5b2-plan.md](m5b2-plan.md) and [m5b-plan.md](m5b-plan.md). Inputs: [phase5-roadmap.md](phase5-roadmap.md) row 7,
> and every sibling draft that exists today: [m5b3-plan.md](m5b3-plan.md) rev 2 (D9, O-01: team loot handed to M5g), [m5c-plan.md](m5c-plan.md)
> rev 2 (D-04: `CM_QUESTION_RESPONSE`; D5: the Daeva seed; D13: stress runs are the user's), [m5d-plan.md](m5d-plan.md) rev 2 (§3.7:
> `CM_QUEST_SHARE` handed to M5g; E-04), [m5e-plan.md](m5e-plan.md) rev 1 (W-23/O-07: `AuraEffect`'s team arm; T-02; W-04),
> [m5f-plan.md](m5f-plan.md) rev 1 (O-03 and :302: recall; D3: the group-instance arms; N-06), [m5h-plan.md](m5h-plan.md) rev 2 (A-18, A-19,
> D12), [m5i-plan.md](m5i-plan.md) rev 1 (Z-01, D7), [m5j-plan.md](m5j-plan.md) rev 0 (A-G1, J1), [runtime-architecture.md](runtime-architecture.md)
> §5 and §14.2(d), [hub-headers.md](hub-headers.md) §8-§9 and §14, `generated/concurrency/{cycles,fieldmap}.toml`, `docs/deviations/P5-10.md`,
> [capacity-proposals.md](capacity-proposals.md) §4.11 and §8.3. §0 states what this plan assumes each earlier milestone delivers; §3.1 answers
> every hand-over the sibling plans make to M5g.
>
> **Three corrections to the roadmap, in order of how much they change the milestone.**
>
> 1. **"294 unported bodies" is not M5g's number, in either direction.** Only **158** of P5-10's 294 sites are groups, alliances, leagues and
>    find group. **66** are the legion *model* (`model/team/legion/**` lies in P5-10 by the manifest's glob), **20** are legion and town
>    challenges, **50** are instance matchmaking (`AutoGroupService` and `model/autogroup`) — M5h's and (D16) M5j's work. But the team machinery
>    the 158 sit in is mostly **invisible to a site count**: **49 of its Java classes have no C++ file at all** (every one of the 39 team events,
>    the member classes, `LeagueService`, both team services, both team task updaters) and carry **167 undeclared bodies**, and **66 more bodies**
>    (+3 on the chat flood arm, up to ~17 more if §0's fallbacks activate) lie in eleven other chunks on the paths this milestone turns on.
>    **The milestone is ~391 bodies over ~5,800 Java lines and 54 new classes**, not 294 sites (§2.8).
> 2. **Nothing a group does is reachable today, and all of it wakes at once.** About **35 call sites in 23 files** outside P5-10 already
>    call into the team code behind an `isInTeam()` / `isInGroup()` / `getCurrentTeam()` / `getCurrentTeamId()` guard (§2.10) — five of them
>    through `ControllerStandIns` stand-ins that throw, and several through `GeneralTeam::getTeamId`, which `SM_PLAYER_INFO` calls for every
>    grouped player anyone sees. M5e's `AuraEffect` adds a team arm on a 6.5 s task (§2.10 E-26). The first party a real client forms throws on
>    the first step, the first sight, the first HP change, the first buff, the first mantra tick and the first kill.
> 3. **Java itself holds the alliance and league locks in both orders, and the reverse order is the minority.** Every league event runs under
>    the league's lock and then takes each alliance's lock; **eight** alliance-side sites take the league's lock, and through it the other
>    alliances' locks, while holding their own alliance's (§2.11 item 3). A faithful port inherits two real deadlock windows, and the checked
>    build's lock-order validator reports a `CYCLE` and a `SAME_CLASS_NESTING` the first time the league gate runs. D4 settles it before the
>    league lane starts: the eight minority sites are suppressed, each named.

---

## 0. What this plan assumes the earlier milestones deliver

M5g is roadmap row 7. It starts after M5b-2 (abilities), M5b-3 (loot and items), M5c (vendors and economy), M5d (quest engine), M5e (training)
and M5f (travel and instances). Every work item, gate case and checklist step that stands on one of these rows carries its id, so the plan can be
re-verified in one pass at branch time.

| Id | Assumed delivered by | What | Where it is today (measured) | Needed by | If it was not delivered |
|---|---|---|---|---|---|
| **A-01** | M5b-2 | The skill and effect engine and the effect subset, including **`StatupEffect`, `AlwaysDodgeEffect`, `AlwaysResistEffect`** (m5b2-plan.md §2.4, D13's four extra classes); `CM_CASTSPELL`; the `player_skills` seeding technique (m5b2-plan.md D3) | **committed** in `760e8ab5c`: the three classes have 0 `AION_UNPORTED` (`skillengine/effect/{Statup,AlwaysDodge,AlwaysResist}Effect.cpp`); P5-02a/P5-02b have 0 `AION_UNPORTED` (m5b3-plan.md:33) | gate cases GP6-GP8 (member icons, group buff, the MP updates) | those cases wait; everything else stands |
| **A-02** | M5b-2 | `PlayerEffectController::updatePlayerIconsAndGroup` (already ported, `controllers/effect/PlayerEffectController.cpp:75-87`), the PARTY arms of `TargetRangeProperty`/`TargetRelationProperty`/`FirstTargetProperty` including `TARGET_MYPARTY_NONVISIBLE` (ported, `FirstTargetProperty.cpp:171-178`), `SkillDecoders` (`tests/scenario/decoders/SkillDecoders.h`) and `gs.scenario.m5b2` green | measured present | GP6-GP8, GP13b | – |
| **A-03** | M5b-3 | The drop engine: `DropRegistrationService::registerDrop` **including `initDropNpc`'s team arm** (DropRegistrationService.java:122-162, `groupMembers` per m5b3-plan.md D11) and **`addDropItems`' `member_limit > 1` arm** (DropRegistrationService.java:233-250; 104 global rules), `DropService::requestDropItem` ported whole with its team branches (DropService.java:276-410), `CM_START_LOOT`, `CM_LOOT_ITEM`, `ItemService::addItem`, `ItemPacketService`, the loot decoders and the `gameserver.rates.drop = 1000000` technique (m5b3-plan.md §2.4). `DropGroup.addDropItem`'s `each_member` arm is **already ported** (`model/drop/DropGroup.cpp:51-62`, P4-13; 107 `custom_drop` entries) | `DropRegistrationService.cpp:43` (partial), 26 `AION_UNPORTED`; `DropService.cpp` 13 `AION_UNPORTED` | loot items L-*, GP11's loot rows and GP12-GP15 | the loot cases wait; GP11's experience rows still run, because `registerDrop` is only the reward's last call. m5b3-plan.md:203-206 (D9) hands **every** team arm to M5g: whatever M5b-3 ported without its team arm, **L-02 ports** |
| **A-04** | M5b-3 (optional there) | The `DropService` team helpers of m5b3-plan.md **L-03**: `canDistribute`, `canAutoLoot`, `distributeEqually`, `winningRollActions`, `winningBidActions`, `winningNormalActions`, `TempTradeDropPredicate::changeItem` (DropService.java:188-270, 412-421, 442-526) | unported | L-02 | **item L-02 ports them** (the M5g loot lane owns P5-09 in stage 1) |
| **A-05** | M5c | **`CM_QUESTION_RESPONSE`** (m5c-plan.md D-04, stage 0), its `GameSession` builder and the `SM_QUESTION_WINDOW` decoder (m5c-plan.md G-02); **`CM_DIALOG_SELECT`** (stage 3's portal case) | no C++ file | **every invite** (group, alliance, league) | **blocking**: item K-05 ports `CM_QUESTION_RESPONSE` first |
| **A-06** | M5c | `PlayerRestrictions::canTrade` (m5c-plan.md A-09) | `restrictions/PlayerRestrictions.cpp:192`, `AION_UNPORTED` | `CM_GROUP_DISTRIBUTION` (CM_GROUP_DISTRIBUTION.java:38) | item W-01 ports it |
| **A-07** | M5c | `TemporaryTradeTimeTask` (m5c-plan.md A-08: P5-07, no C++ file) | no `.h`, no `.cpp` | `TempTradeDropPredicate` (DropService.java:505-526) | item L-03 creates it under a P5-07 lease |
| **A-08** | M5c | The two-account gate pattern, kinah and inventory decoders, exp seeding (m5c-plan.md G-01 "Daeva seed ... exp"), **the Daeva seed recipe** (m5c-plan.md D5: `players.player_class`, `player_quests (1006, COMPLETE)`, `players.exp`), `gs.scenario.m5c` green | – | the gate; stage 3's two Daevas (GA12-GA14) | the harness lane writes what is missing (H-02) |
| **A-09** | M5d | `QuestEngine` with the `MonsterHunt` and `ItemCollecting` template handlers, `QuestService::checkStartConditions`, the quest seeding technique (a `player_quests` row) and `QuestDecoders` (`SM_QUEST_ACTION`; m5e-plan.md:66, A-04c, M5d's G-02); `QuestService::getEachDropMembersGroup/Alliance` (m5d-plan.md E-04) | unported (m5d-plan.md §4.2); `QuestService.cpp:388-395` | `CM_QUEST_SHARE` (GP15b), the quest-credit row GP11b (quest **1102**, whose `quest_kill` npcs are **210133/210134**, quest_data.xml:898-900) | `CM_QUEST_SHARE` returns without a word (no quest state, CM_QUEST_SHARE.java:53-55); GP11b and GP15b wait; `getEachDropMembers*` → L-02 |
| **A-10** | M5d | `AbyssPointsService::addAp` (m5d-plan.md E-09) | `AbyssPointsService.cpp:15-17`, unported | the team AP arm (PlayerTeamDistributionService.java:77-83) — **not reached on Poeta**: `NpcAI.ask(REWARD_AP)` is false in an ELYSEA world (NpcAI.java:153-156) | stays W |
| **A-11** | **M5e** | **`AuraEffect` ported whole** (m5e-plan.md E-01) with its team arm written faithfully and dormant (m5e W-23, O-07: AuraEffect.java:54-62 → `p.getCurrentGroup().getOnlineMembers()`); **`CM_TOGGLE_SKILL_DEACTIVATE`** (m5e C-04); `SM_MANTRA_EFFECT` decoding (m5e X14); optionally `HealCastorOnAttackedEffect` (m5e T-02, **O**) whose ATTACKED observer has the same team arm (HealCastorOnAttackedEffect.java:39-48); the class-change level path reaching `standins::teamStatUpdaterAdd` (m5e W-04) | `AuraEffect.cpp` 5 `AION_UNPORTED`, `HealCastorOnAttackedEffect.cpp` 2 | the mantra case GP6b (skill 1809); E-26/E-27 | GP6b waits and GP6's slot check loses its CHANT entry (the "whole list" mutation of §10.4 then survives — recorded); E-26/E-27 stay unreachable |
| **A-12** | **M5f** | `TeleportService::teleportTo` (recall, GP13b) and M5f's `TravelDecoders` (`SM_TELEPORT_LOC`, m5f-plan.md G-02); the instance engine: `InstanceService` incl. `moveToExitPoint`, `getRegisteredInstance`, `getNextAvailableInstance`, `EmptyInstanceCheckerTask`; **`PortalService`'s group, alliance and league arms ported blind** (m5f D3: "M5g's gate re-verifies them", PortalService.java:78-99, 124-190); `PortalDialogAI` (m5f V-*, A1 lease); header request **m5b2-p2-9** (`RecallService::validateCast` takes a `Ptr`) applied by m5f T-07 | P5-13 / P5-08 / A1; `WorldMapInstance::registerTeam` is ported (`WorldMapInstance.cpp:188-194`) and `getRegisteredTeam` inline (`WorldMapInstance.h:165`) | `PlayerLeavedEvent`'s 30-second `INSTANCE_KICK` task (PlayerLeavedEvent.java:57-66); GA12-GA14; GP13b | GA12-GA14 and GP13b wait; W-06 applies m5b2-p2-9 itself (a one-line P5-02a lease on `Skill.cpp:853-860`) |
| **A-13** | all | every earlier gate green with the allow-lists its milestone left | – | G-03, G-05 | the regate lanes grow |
| **A-14** | **none before M5g** | `CM_CHAT_MESSAGE_PUBLIC` — no plan before M5g ports it; m5h-plan.md D12 and A-19 say it is M5g's; m5i-plan.md Z-01 ports it only "if still absent"; m5j-plan.md J1/K-05 (rev 0, written before this plan existed) lists it too | no C++ file | party, alliance and league chat | item K-04 ports it; m5j's J1 then keeps the whisper, `CM_CHAT_GROUP_INFO`, `CM_CHAT_PLAYER_INFO` and the command framework (§3.1) |
| **A-15** | M5f | m5f-plan.md **N-06**'s P5-10 lease on **`AutoGroupService::onLeaveInstance`** (one body, `AutoGroupService.cpp:152`) is **released** before I-01 splits the chunk | – | I-01, I-05 | I-01 waits for the release; P5-10 is 293 sites at branch time and the matchmaking part 49 (48 after K-03's `isInAutoInstance`) |
| **A-16** | M5b-1 (measured) | the death arrangement (m5b-plan.md D12: `player_life_stat.hp` seeded low while the character is offline), `CM_REVIVE` (`clientpackets/CM_REVIVE.cpp` exists), the aggressive npc of m5b-plan.md A5b (210673) | measured present | GP17b | – |

**Re-verification at branch time:** for each row, grep the named files for `AION_UNPORTED(` and check that the C++ file exists; A-05 is the only
blocking row, and K-05 is its fallback.

---

## 1. Summary

**The frame around the teams is ported; the teams are not.** M5a had to make enter world and logout work for a player without a team, and the
packet and model phases wrote everything a team *shows*:

| Already ported, 0 `AION_UNPORTED` | Evidence |
|---|---|
| The player's team state: `getPlayerGroup`, `setPlayerGroup`, `isInGroup`, `getPlayerAlliance`, `isInAlliance`, `isInLeague`, `isInTeam`, `getCurrentTeam`, `getCurrentGroup`, `getCurrentTeamId`, `isInSameTeam`, `isMentor`, `setLookingForGroup` | `model/gameobjects/player/Player.cpp:502, 535, 717, 808-841`; `Player.h:108, 115-116, 687-690` |
| **Every server packet a team sends**: `SM_GROUP_INFO`, `SM_GROUP_MEMBER_INFO`, `SM_LEAVE_GROUP_MEMBER`, `SM_ALLIANCE_READY_CHECK`, `SM_SHOW_BRAND`, `SM_GROUP_LOOT`, `SM_FIND_GROUP`, `SM_AUTO_GROUP`, `SM_QUESTION_WINDOW`, `SM_MESSAGE`, `SM_ABYSS_RANK_UPDATE`, `SM_GROUP_DATA_EXCHANGE`, `SM_CHALLENGE_LIST`, `SM_ICON_INFO`, `SM_RECALLED_BY_OTHER` | 0 `AION_UNPORTED` each (measured); **two exceptions**: `SM_ALLIANCE_INFO.cpp:27` (`leaguePosition`) and `SM_ALLIANCE_MEMBER_INFO.cpp:39` (the constructor) wait for `LeagueMember.h` / `PlayerAllianceMember.h` |
| `PlayerEffectController::updatePlayerIconsAndGroup` — the call into the group services on every effect change | `controllers/effect/PlayerEffectController.cpp:75-87` (Java PlayerEffectController.java:70-82) |
| `TeamDamageList` (the team is one attacker in the kill reward), `getMostDamageByTeam` | `controllers/attack/TeamDamageList.cpp:62-67` |
| `ResponseRequester`, `RequestResponseHandler` (the question-window plumbing an invite uses) | `model/gameobjects/player/ResponseRequester.h:35-45`, `RequestResponseHandler.h`, 0 unported (P4-12) |
| The find-group model (`GroupRecruitment`, `GroupApplication`, `ServerWideGroup`, `FindGroupEntry`), `AutoGroupData::getRecruitableInstanceMaskIds` | `model/gameobjects/findGroup/*.cpp` (P4-11a), `dataholders/AutoGroupData.h:30-33` (P4-09) |
| `GroupConfig` (all six keys), `AbstractFIFOPeriodicTaskManager<T>` (the base of both team updaters), `Predicates::Players::allExcept/WITH_LOOT_PET/canBeMentoredBy` | `configs/main/GroupConfig.cpp:8-13`; `taskmanager/AbstractFIFOPeriodicTaskManager.h`; `utils/collections/Predicates.h:32-38` |
| `EventService::onEnteredTeam/onLeftTeam` (the dispatch; `Event::onEnteredTeam` itself is unported but unreachable with every event off) | `services/event/EventService.cpp:149-157` |
| `RecallService::hasPendingRequest`, `accept`, `cancel`, `remove` (the cancel half every teleport, death and logout already calls) | `services/RecallService.cpp:37-85` |
| **The lifetime specification**: 16 `cycles.toml` rows for teams, the K5 kind of the leave events, `DropNpc.lootingTeam` | `cycles.toml:157-158, 194-206, 356`; `fieldmap.toml:59-61, 115` |

**What is empty is seven holes:**

| # | Hole | Where | Size (measured) |
|---|---|---|---|
| 1 | **The team core**: `GeneralTeam` (24 of its 26 bodies), `TemporaryPlayerTeam` 7, `LootGroupRules` 7, and **no C++ file** for `TeamEvent`, `PlayerTeamMember`, the 7 common events, `PlayerTeamCommandService`, `PlayerTeamDistributionService` (the **experience share**), plus the 5 enum companions | P5-10 (`model/team/*.java`, `common/**`) | 38 sites + **46 undeclared**, 1,218 Java LOC |
| 2 | **Parties**: `PlayerGroup` 10, `PlayerGroupStats` 4, `PlayerGroupService` 18; **no file** for `PlayerGroupMember`, the 11 group events, `TeamMoveUpdater`, `TeamStatUpdater` | P5-10 (`group/**`, `taskmanager/tasks/Team*`) | 32 sites + **39 undeclared**, 914 Java LOC |
| 3 | **Alliances**: `PlayerAlliance` 18, `PlayerAllianceGroup` 10, `PlayerAllianceService` 20; **no file** for `PlayerAllianceMember` and the 12 alliance events | P5-10 (`alliance/**`) | 48 sites + **40 undeclared**, 1,210 Java LOC |
| 4 | **Leagues**: `League` 21; **no file** for `LeagueMember`, `LeagueService` and the 9 league events | P5-10 (`league/**`) | 21 sites + **42 undeclared**, 780 Java LOC |
| 5 | **Find group**: `FindGroupService` 19 — **two of them on the party path** (`onJoinedTeam`, `removeRecruitment(team)`) | P5-10 (`services/findgroup`) | 19 sites, 211 Java LOC |
| 6 | **The bodies outside the chunk**: the invite and chat restrictions, the chat ban and flood checks, the loot distribution, recall, the stand-ins, the dormant team arms of other services (§2.10) | P5-13, P5-09, P5-01, P5-08, P5-04, P4-11b, P4-16, P5-07, P5-06 | 24 bodies + 5 stand-in deletions (+3 on the flood arm, + up to 17 if §0's fallbacks activate) |
| 7 | **The client packets**: 12 of the 147 with no C++ file (§2.9), `CM_CHAT_MESSAGE_PUBLIC` (A-14) and `CM_RECALLED_BY_OTHER_ANSWER` included | P5-15, P5-16 | 42 bodies (+3 if A-05), 918 Java LOC |

**Five findings shape the plan.**

1. **P5-10 is one chunk, and a chunk is one lane** — the M5b-2 D1 problem again, larger: 158 sites + 167 undeclared bodies in scope, plus the
   legion model and matchmaking M5g does not want. **D1 splits the manifest into six parts sharing `aion_gs_team`**; without it the milestone is
   one serial lane.
2. **Parties are a milestone of their own** (the task's "parties first"). The party path needs the core, the groups, find group's two hooks,
   the loot distribution, recall and 12 packets — ~238 bodies — and nothing of the alliance or the league. Stage 1 ports it, stage 2 gates it
   while the alliance and league lanes run, stage 3 gates those and a group entering a group instance (§9).
3. **No holdback is needed, unlike M5b-2 D11.** Every new path starts at a client packet no earlier gate sends (`CM_INVITE_TO_GROUP`,
   `CM_CHAT_MESSAGE_PUBLIC`, `CM_RECALLED_BY_OTHER_ANSWER`, a recall cast), and every dormant site of §2.10 is behind a team check or a
   registry no earlier gate fills: **E-12 is the one site a solo player reaches** — every login iterates the group and alliance registries —
   and it stays silent while no group exists, which is true of every earlier gate. The earlier gates should not move; if one does, that is a
   finding.
4. **The lifetime story is where the port can go wrong while Java looks fine**: a group keeps an offline member's `Player` for
   `gameserver.playergroup.removetime` = 600 s, and the leak census reports an object removed from the world after
   `gameserver.debug.leak_census_minutes` = 10 min (`RuntimeConfig.cpp:14`) — with the offline check running every 30 s, **every member who
   logs out of a party and stays out will be reported as a leak between 600 and 630 s** (§2.11, D6). Two more retention paths — `PlayerGroupStats`
   keeps up to two `Player` references past a member's departure, and a corpse's `DropNpc` keeps a disbanded group (`lootingTeam`) and its
   in-range members (`inRangePlayers`, `DropNpc.h:29`) for up to 300 s — reach the census (D6(b), D7).
5. **Team member order is not Java's.** Leader succession (`changeLeaderToNextAvailablePlayer`, ChangeLeaderEvent.java:23-31) and the round-robin
   looter (DropRegistrationService.java:131-145) follow `members.values()` order; the C++ `ConcurrentHashMap` iterates by 16 stripes chosen by
   the top 4 bits of a Murmur3 spread, not by Java's single table (`runtime/collections/ConcurrentHashMap.h:43-44, 720`). D5 makes it a recorded
   deviation and the gate order-free.

---

## 2. The paths, end to end

"ported" means the body exists and contains no `AION_UNPORTED`. "no file" means the Java class has no C++ `.h` or `.cpp` (only a `fwd.h` line).

### 2.1 Invite, accept, a party

| # | Step | Java | C++ today | Chunk |
|---|---|---|---|---|
| 1 | `CM_INVITE_TO_GROUP(inviteType, name)`: dead check, `World.getPlayer(ChatUtil.getRealCharName(name))` (**an unknown name is refused here, `STR_NO_SUCH_USER`**, CM_INVITE_TO_GROUP.java:46-49), `DeniedStatus.GROUP`; type 0 → group, 12 → alliance, 28 → league | CM_INVITE_TO_GROUP.java:33-68 | **no file** | P5-15 |
| 2 | `PlayerGroupService.inviteToGroup` → `PlayerRestrictions.canInviteToGroup` → `canInviteToTeam` (in order: dead, prison, target null, FFA, **`AutoGroupService.isInAutoInstance`** ×2, **leader**, full, **self**, race, dead target, **target's team**, defence alliance) | PlayerGroupService.java:37-45; PlayerRestrictions.java:118-205 | `inviteToGroup` unported (`PlayerGroupService.cpp:27`); the three restrictions unported (`PlayerRestrictions.cpp:143-154`); **`isInAutoInstance` unported** (`AutoGroupService.cpp:193-194`) and reached on every invite | P5-10, P5-13 |
| 3 | `STR_PARTY_INVITED_HIM` to the inviter; `new PlayerGroupInvite(inviter)`; `invited.getResponseRequester().putRequest(60000, invite)`; `SM_QUESTION_WINDOW(60000, 0, 0, inviterName)` | PlayerGroupService.java:39-43 | `PlayerGroupInvite` **no file**; `putRequest` and the packet ported | P5-10 |
| 4 | The invited answers: `CM_QUESTION_RESPONSE(60000, 1)` → `ResponseRequester.respond` → `PlayerGroupInvite.acceptRequest` (re-checks `canInviteToGroup`; joins the inviter's group or `createGroup`) / `denyRequest` → `STR_PARTY_HE_REJECT_INVITATION` | CM_QUESTION_RESPONSE.java:39-45; PlayerGroupInvite.java:22-36 | packet **no file** (**A-05**); `respond` ported | P5-16 |
| 5 | `createGroup`: `new PlayerGroup(new PlayerGroupMember(leader), GROUP, 0)` (id from `IDFactory`, auto-released), `groups.put`, `addPlayer(leader)`, `addPlayer(invited)`, and on the first group ever **`scheduleAtFixedRate(OfflinePlayerChecker, 1000, 30000)`** | PlayerGroupService.java:47-60; PlayerGroup.java:15-20 | unported (`PlayerGroupService.cpp:31-37`, `PlayerGroup.cpp:14`) | P5-10 |
| 6 | `addPlayer` → `group.onEvent(new PlayerGroupEnteredEvent)`: **`onEvent` takes the team lock**, checks, handles (GeneralTeam.java:37-48) | PlayerGroupService.java:118-121 | `onEvent` unported (`GeneralTeam.cpp:17`); the event **no file** | P5-10 |
| 7 | `PlayerGroupEnteredEvent.handleEvent`: `addPlayerToGroup` (→ `PlayerGroup.addMember` → `GeneralTeam.addMember`, `PlayerGroupStats.onAddPlayer`, `player.setPlayerGroup(this)`; **`FindGroupService.onJoinedTeam`**), then to the new member `SM_GROUP_INFO`, `STR_PARTY_ENTERED_PARTY`, `SM_GROUP_MEMBER_INFO(JOIN)`, `sendBrands`; to each other member `SM_GROUP_MEMBER_INFO(ENTER)` + `STR_PARTY_HE_ENTERED_PARTY`, and the reverse `ENTER` to the new member; `broadcastPacket(SM_ABYSS_RANK_UPDATE(1))`; `EventService.onEnteredTeam` | PlayerGroupEnteredEvent.java:24-45; PlayerEnteredEvent.java:31-33; PlayerGroup.java:23-27; FindGroupService.java:182-194 | all unported or no file; `onJoinedTeam` unported (`FindGroupService.cpp:92-94`); the packets ported | P5-10 |

The wire facts the gate reads (from the Java `writeImpl`, never from the port): `SM_GROUP_INFO` (SM_GROUP_INFO.java:28-48) — groupId, leaderId,
mapId, the **eight loot-rule words** (`LootGroupRules()` defaults: ROUNDROBIN, misc 0, common 0, superior..mythic 2 — LootGroupRules.java:32-40),
`0x02`, `0`, `TeamType.getType()`/`getSubType()` (GROUP = `0x3F`, 0 — TeamType.java:8), message id 0, empty name; `SM_GROUP_MEMBER_INFO`
(SM_GROUP_MEMBER_INFO.java:48-143) — groupId, member id, HP/MP/flight **as zeros for an offline member**, map, position, class, gender, level,
**the `GroupEvent` id** (LEAVE 0, MOVEMENT 1, DISCONNECTED 3, JOIN 5, ENTER_OFFLINE 7, ENTER/UPDATE 13, UPDATE_EFFECTS 65 — GroupEvent.java:8-15),
then per event the name and/or the effect list; for UPDATE_EFFECTS **the slot byte** (`SkillTargetSlot` id: BUFF 1, DEBUFF 2, CHANT 4, SPEC 8,
SPEC2 16 — SkillTargetSlot.java:12-19) and **only the effects of that slot** (`getAbnormalEffectsToTargetSlot`, EffectController.java:695-697,
SM_GROUP_MEMBER_INFO.java:37-38, 99-110); `SM_LEAVE_GROUP_MEMBER` (SM_LEAVE_GROUP_MEMBER.java:12-19) — five constants.

### 2.2 Life in a party

| # | Trigger | Java | C++ today | What wakes |
|---|---|---|---|---|
| 1 | A member moves: `PlayerController.onMove` → `TeamMoveUpdater.add` (FIFO, **2,000 ms**) → `updateGroup(MOVEMENT)` → `PlayerGroupUpdateEvent` → `sendPacket(allExcept(player), SM_GROUP_MEMBER_INFO(MOVEMENT))` | PlayerController.java:492-496; TeamMoveUpdater.java; PlayerGroupUpdateEvent.java:32-34 | **`standins::teamMoveUpdaterAdd` throws** (`PlayerController.cpp:591-592`); the updater **no file** | every `CM_MOVE` of a grouped player |
| 2 | HP or MP changes: `PlayerLifeStats.onHpChanged/onMpChanged` → `sendGroupPacketUpdate` → `TeamStatUpdater.add` (FIFO, **500 ms**) → the same event | PlayerLifeStats.java:41, 55, 61-64; TeamStatUpdater.java | **`AION_UNPORTED`** (`PlayerLifeStats.cpp:67-72`, P5-01) | every damage, heal, regeneration tick and MP cost |
| 3 | Level or stats change: `PlayerController.upgradePlayer` → `TeamStatUpdater.add` | PlayerController.java:604-608 | **`standins::teamStatUpdaterAdd` throws** (`PlayerController.cpp:709-710`) | level up; M5e's class change (m5e W-04) |
| 4 | **An effect starts or ends on a member**: `PlayerEffectController.addEffect/clearEffect/removeAllEffects` → `updatePlayerIconsAndGroup` → `updateGroup(MOVEMENT)` **and** `updateGroupEffects(slot)` → `SM_GROUP_MEMBER_INFO(UPDATE_EFFECTS, slot)` with that slot's effects, to every other member | PlayerEffectController.java:70-82; PlayerGroupService.java:101-113 | caller ported (`PlayerEffectController.cpp:75-87`), both services **unported** (`PlayerGroupService.cpp:65-71`) | every buff, debuff and passive-less effect on a grouped player — the "effect icons of grouped players" |
| 5 | **A group buff**: a skill with `target_type="PARTY"` / `target_relation="MYPARTY"` → the PARTY arm of `TargetRangeProperty` takes `getCurrentGroup().getMembers()`, keeps the online ones within `effective_range` **that pass `checkGeo`** (`GeoService.canSee(firstTarget, member)` when geo is on) → one `Effect` per member → row 4 for each | TargetRangeProperty.java:55-80, 155-167 | ported (M5b-2, A-02, `TargetRangeProperty.cpp:90-106`), calling `GeneralTeam::getMembers` (`GeneralTeam.cpp:61`, unported) | first real use needs a group; **no starting-class skill at level ≤ 10 is a party skill** — measured: **6 party skills exist at level ≤ 20, none ≤ 10** (4384, 4538, 1576, 1727, 4188, 3544) |
| 5b | **A mantra** (the Chanter's group buff): `AuraEffect`'s `AuraTask` every **6,500 ms** (AuraEffect.java:76) → for a player in a team, `getCurrentGroup().getOnlineMembers()` filtered by `distance` × `BOOST_MANTRA_RANGE` and `distance_z` → `SkillEngine.applyEffect(skillId, member, member)` — each member applies the aura's effect **to itself** (AuraEffect.java:54-62, 70-72; SkillEngine.java:169-171) | AuraEffect.java:44-68 | `AuraEffect` 5 `AION_UNPORTED` today, **M5e's** (A-11); the team arm calls `TemporaryPlayerTeam::getOnlineMembers` (`TemporaryPlayerTeam.cpp:36-37`, unported) | every tick of a grouped Chanter's mantra — **37 `aura` templates**, e.g. 1809 *Celerity Mantra*: `aura skill_id="8998" distance="25" distance_z="10"`, 8998 = `tslot="CHANT"` statup SPEED 6,500 ms (skill_templates.xml) |
| 5c | **A heal on being attacked**: `HealCastorOnAttackedEffect`'s ATTACKED observer heals every online member of the caster's group within `range` (HealCastorOnAttackedEffect.java:39-48) | – | 2 `AION_UNPORTED`, M5e's optional T-02 | only if M5e ported it (9 `healcastoronatk` templates: 4631-4638 *Healing Conduit*, 11587 stigma). `HealCastorOnTargetDeadEffect`'s arm (HealCastorOnTargetDeadEffect.java:41-48) needs `healparty`, which its only template (19573 *Offering*) does not set — unreachable by data |
| 6 | Revive: `PlayerReviveService.revive` → `updateGroup(MOVEMENT)` | PlayerReviveService.java:189-212 (the group call at :206-208) | caller ported (`PlayerReviveService.cpp:135-140`), service unported | every revive |
| 7 | Teleport or relog finishes: `CM_LEVEL_READY` → 100 ms later `team.sendBrands(player)` | CM_LEVEL_READY.java:110 | caller ported (`CM_LEVEL_READY.cpp:124-128`); `sendBrands` unported (`TemporaryPlayerTeam.cpp:19-21`) | every enter world of a grouped player |
| 8 | Target marks: `CM_SHOW_BRAND` → leader (or alliance captain) → `updateBrand` → `SM_SHOW_BRAND` to all; **solo → `SM_SHOW_BRAND` to self** | CM_SHOW_BRAND.java:35-44; TemporaryPlayerTeam.java:40-46 | packet **no file**; `updateBrand` unported | also solo |
| 9 | Chat: `CM_CHAT_MESSAGE_PUBLIC` → `ChatProcessor.handleChatCommand` (:48) → **`PlayerRestrictions.canChat`** (:51): prison, **`ChatBanService.isBanned` → `getBanMinutes`**, **`PlayerChatService.isFlooding`** → on a flood `ChatBanService.banPlayer` (2 min: `sendPlayerGagPacket`, `registerUnban` — a GAG controller task) (PlayerRestrictions.java:254-275; ChatBanService.java:26-75; PlayerChatService.java:19-26) → `PlayerChatService.logMessage` (:54) → by type, GROUP/ALLIANCE/GROUP_LEADER/LEAGUE → `getCurrentGroup()`/alliance/league `sendPackets(SM_MESSAGE)` (:57-90) | CM_CHAT_MESSAGE_PUBLIC.java:45-151 | **no file** (A-14); `canChat` unported (`PlayerRestrictions.cpp:196`); **`ChatBanService` 5 unported** (`ChatBanService.cpp:7-26`), **`PlayerChatService::isFlooding` and `logMessage` ×2 unported** (`PlayerChatService.cpp:11-27`); `ChatServer::sendPlayerGagPacket` ported (`ChatServer.cpp:125-128`) | **also solo** — normal chat is the same packet, and every message reaches `isBanned`/`getBanMinutes`/`isFlooding` |
| 10 | UI data sync: `CM_GROUP_DATA_EXCHANGE(action, groupType, unk2, data)` — action 1 broadcasts to the known list and self; any other action sends to the **online members of the group** (groupType 0), **of the alliance group** (1, 2), **never to the sender** | CM_GROUP_DATA_EXCHANGE.java:38-88 | **no file**; `SM_GROUP_DATA_EXCHANGE` ported | also solo (action 1); the team fan-out for every other action |
| 11 | A member dies: `CreatureController.onDie` → `PlayerController.doReward` → `PvpService.doReward` → for a non-player killer, `STR_MSG_COMBAT_MY_DEATH` to the victim and **`team.sendPacket(allExcept(victim), STR_MSG_COMBAT_FRIENDLY_DEATH)`** | PvpService.java:99-108 | **`AION_UNPORTED`** at `PvpService.cpp:75` (P5-08), inside the death path | every death of a grouped player |
| 12 | A grouped player hits an npc first: `NpcController.onAddHate` → `getCurrentTeam().filterMembers(inRange 50)` → `QuestEngine.onAddAggroList` per member | NpcController.java:275-284 | caller ported (`NpcController.cpp:308-315`); `filterMembers` unported (`GeneralTeam.cpp:57`) | every first hit |
| 13 | Tab-target a member far away (`CM_TARGET_SELECT`: not in the known list → `getCurrentTeam().hasMember/getMember`, CM_TARGET_SELECT.java:59-61), instance info for the team, kisk binding, portal cooldown to the team | CM_TARGET_SELECT; CM_INSTANCE_INFO; Kisk.java; PortalCooldownList.java | callers ported (`CM_TARGET_SELECT.cpp:68-69`, `CM_INSTANCE_INFO.cpp:37-45`, `Kisk.cpp:147, 151`, `PortalCooldownList.cpp:85-87`); `hasMember`/`getMember`/`getLeaderObject`/`filterMembers`/`sendPackets` unported | as used |
| 14 | Mentoring: `CM_PLAYER_STATUS_INFO(10/11)` → `PlayerStartMentoringEvent` (needs a member ≥ 10 levels below, Predicates.java:42-44) | PlayerStartMentoringEvent.java:30-40 | no file | needs a level gap (O-03) |
| 15 | **Summon group member** (skill 3777: `first_target="TARGET_MYPARTY_NONVISIBLE"`, `recallinstant`, uses 1 × item 169300011 *Dimensional Fragment*, 4.5 s cast): `Skill.isInvalidRecall` → **`RecallService.validateCast`** (flying, `canRecallAt`, target a player, no pending request, **`canBeSummoned`**) at cast start (Skill.java:153, 566, 742-744); `RecallInstantEffect.calculate` → `canBeSummoned` + the caster's position; `applyEffect` → **`requestSummon`** → `SM_RECALLED_BY_OTHER(casterName, skillId, 30)` and a 30 s timeout; the target answers **`CM_RECALLED_BY_OTHER_ANSWER`** (0 accept → `TeleportService.teleportTo` the caster's position; 1 decline) | RecallService.java:56-174; RecallInstantEffect.java:19-32; CM_RECALLED_BY_OTHER_ANSWER.java:26-35 | `requestSummon`, `validateCast`, `canBeSummoned`, `canRecallAt` unported (`RecallService.cpp:41-42, 87-96`, P5-08); `RecallInstantEffect` 2 unported (P5-04); the packet **no file** (P5-16); `Skill::isInvalidRecall` dereferences a null first target until m5b2-p2-9 (`Skill.cpp:853-860`) | a recall skill cast (solo casts reach `validateCast` too); handed to M5g by m5f-plan.md:183, 285, 302, O-03 |

### 2.3 A kill: experience, quest credit, loot

| # | Step | Java | C++ today | Chunk |
|---|---|---|---|---|
| 1 | `NpcController.doReward`: `toTeamDamages()`; for a `TemporaryPlayerTeam` attacker **`PlayerTeamDistributionService.doReward(team, percentage, npc, winner, finalList)`** | NpcController.java:201-216 | ported, calling **`standins::playerTeamDistributionServiceDoReward`** (`NpcController.cpp:247`; `ControllerStandIns.h:55-62`) | P4-11b |
| 2 | `doReward`: `PlayerTeamRewardStats` over the team (or every alliance of the league, :39-40): a member counts if **online and within `GROUP_MAX_DISTANCE` = 100 m** (9,999 on `DISABLE_RANGE_CHECK_MAPS`); `QuestEngine.onKill` **for every counted member** (:122, inside `team.forEach`, i.e. under the team lock); mentors are counted apart; `partyLvlSum`, `highestLevel` | PlayerTeamDistributionService.java:35-45, 104-136 | **no file** | P5-10 |
| 3 | Per living member: **`rewardXp = Math.round(expReward(highestLevel) × level / (float) partyLvlSum)`**, `rewardDp`; **0 if ≥ 10 levels below the highest**; `× damagePercent`; `addExp(rewardXp, Rates.XP_GROUP_HUNTING, l10n)` — **`min(xp × rate(XP_GROUP_RATES), expNeed × 0.2f)`** (Rates.java:21-27); `addDp`; AP only where `ask(REWARD_AP)` (A-10) | PlayerTeamDistributionService.java:50-84 | no file; `calculateExperienceReward`, `calculateDPReward`, `addExp`, `addDp` ported (M5b-1) | P5-10 |
| 4 | `getMostDamageByTeam`; a mentor replaced by a member of the highest level; `registerDrop(npc, mostDamagePlayer, highestLevel, filteredStats.players)` unless mentors are present at a chest | PlayerTeamDistributionService.java:85-102 | no file; `registerDrop` **A-03** | P5-10, P5-09 |
| 5 | `registerDrop` for a team: `initDropNpc` — **ROUNDROBIN** (`nrRoundRobin` 0 → 1 → 2 … over the in-range members in list order, reset to 1 past the size), **FREEFORALL** (all in range), **LEADER**; `dropNpc.setInRangePlayers`, **`setLootingTeam(team)`** (a `Ref`, `fieldmap.toml:115`); `SM_LOOT_STATUS(LOOT_ENABLE)` to each allowed looter. On the way: **`addDropItems`' `member_limit > 1` arm** (one item per member, up to the limit, shuffled when the limit exceeds the team, DropRegistrationService.java:233-250), `DropGroup.addDropItem`'s **`each_member` arm** (DropGroup.java:86-106, ported), **`QuestService.getQuestDrop`'s group and alliance arms** (QuestService.java:676-724: `drop_each_member` or the first member who needs it, `allowLooting`) | DropRegistrationService.java:84, 103-162, 228-255 | **A-03**; `getQuestDrop` unported (`QuestService.cpp:321-324`, P5-06) | P5-09, P5-06, P4-13 |
| 6 | `requestDropItem` for a team member: **kinah → `distributeEqually` over online, living, non-mentor members within 100 m of the looter** (the last in list order first; the first gets the remainder); a quality at or above the rule → `canDistribute` → **`SM_GROUP_LOOT(0, distributionId)` to every in-range member, `IN_ROLL` mode, and `setPlayersInRoll` schedules an automatic pass after 17,000 ms (roll) or 32,000 ms (bid)**; below it → the looter gets it and `STR_MSG_GET_ITEM_PARTYNOTICE` goes to the others; JUNK with `misc = 1` → round robin over misc | DropService.java:276-410, 188-234; LootGroupRules.java:55-80, 113-124 | requestDropItem **A-03**; the helpers **A-04**; `LootGroupRules` 7 unported (`LootGroupRules.cpp:38-62`) | P5-09, P5-10 |
| 7 | The answer: `CM_GROUP_LOOT(…, mode 2 roll / 3 bid, roll, bid)` → `DropDistributionService.handleRollOrBid` → `handleRoll` (**`luck = Rnd.get(1, maxRoll)`**, `STR_MSG_DICE_RESULT_ME/OTHER`, `SM_GROUP_LOOT(luck)` to all) or `handleBid` → `distributeLoot`: the **strictly greater** value wins (the first roller keeps a tie); when everyone answered, `SM_GROUP_LOOT(winner or 1, 0xFFFFFFFF)` to all; nobody → `STR_MSG_PAY_ALL_GIVEUP` and the item turns free-for-all; a winner → `requestDropItem` again → `winningRollActions` / `winningBidActions` (the bid is paid and split among the others) | CM_GROUP_LOOT.java:39-59; DropDistributionService.java:29-146; DropService.java:442-471 | packet **no file**; `DropDistributionService` 4 unported | P5-15, P5-09 |
| 8 | Loot-rule change: `CM_DISTRIBUTION_SETTINGS` → `changeGroupRules` → `ChangeGroupLootRulesEvent` → `setLootGroupRules` (a **new** `LootGroupRules`, so `nrRoundRobin` restarts at 0; FREEFORALL → `STR_MSG_LOOTING_PET_MESSAGE03` to pet owners) → `SM_GROUP_INFO` to all | CM_DISTRIBUTION_SETTINGS.java:40-77; ChangeGroupLootRulesEvent.java:22-25; TemporaryPlayerTeam.java:79-83; LootGroupRules.java:42-53 | no file / unported | P5-15, P5-10 |
| 9 | Kinah split by hand: `CM_GROUP_DISTRIBUTION(amount, partyType)` (amount ≥ 2, **`canTrade`**) → `TeamKinahDistributionEvent`: `amount / onlineMembers` to each **online member including the giver, no range check**, the giver pays `amount` — **the remainder vanishes** | CM_GROUP_DISTRIBUTION.java:26-55; TeamKinahDistributionEvent.java:24-50 | no file; `canTrade` **A-06** | P5-15, P5-10 |

### 2.4 Leaving

| Trigger | Java | What the members see |
|---|---|---|
| **Leave**: `CM_PLAYER_STATUS_INFO(6 GROUP_REMOVE_MEMBER, 0)` → `PlayerTeamCommandService.executeCommand` → `findMember(…, 0)` = self → `removePlayer` → `PlayerGroupLeavedEvent(LEAVE)` | CM_PLAYER_STATUS_INFO.java:30-54; PlayerTeamCommandService.java:22-90; PlayerGroupLeavedEvent.java:32-76 | the leaver: `SM_LEAVE_GROUP_MEMBER` (PlayerLeavedEvent.java:56-68); the rest: `SM_GROUP_MEMBER_INFO(LEAVE)` + `STR_PARTY_HE_LEAVE_PARTY`; **one member left → `disband`**, else a leaving leader passes leadership (`ChangeGroupLeaderEvent`, next online member in iteration order). **Every leave reads `leavedPlayer.getPosition().getWorldMapInstance().getRegisteredTeam()`** (PlayerLeavedEvent.java:59) — inline and ported (`WorldMapInstance.h:165`) |
| **Kick**: `(2 GROUP_BAN_MEMBER, id)` → `banPlayer` (self, not leader, auto group are refused) | PlayerGroupService.java:136-153 | as leave with `STR_PARTY_HE_IS_BANISHED` / `STR_PARTY_YOU_ARE_BANISHED` |
| **Leader change**: `(3 GROUP_SET_LEADER, id)` → `ChangeGroupLeaderEvent` → `team.changeLeader` | ChangeGroupLeaderEvent.java:24-45 | `SM_GROUP_INFO` (new leaderId) to all; `STR_PARTY_YOU_BECOME_NEW_LEADER` / `STR_PARTY_HE_IS_NEW_LEADER` |
| **Disconnect**: `PlayerLeaveWorldService.leaveWorld` sets the connection null **first** (Java :65, C++ `PlayerLeaveWorldService.cpp:87`), then `onPlayerLogout` (Java :108, C++ :131) → `updateLastOnlineTime`, `PlayerDisconnectedEvent`: **nobody online → `disband`** (PlayerDisconnectedEvent.java:33-34 — so the last online member's logout always disbands); a disconnecting leader passes leadership | PlayerLeaveWorldService.java:63-65, 108; PlayerGroupService.java:89-96; PlayerDisconnectedEvent.java:29-52 | `STR_PARTY_HE_BECOME_OFFLINE` + `SM_GROUP_MEMBER_INFO(DISCONNECTED)`; the offline member **stays in the group with its old `Player` object** |
| **Reconnect**: `PlayerEnterWorldService` → `onPlayerLogin` (iterates **every** group, `group.getMember(id)`) → `PlayerConnectedEvent`: `removeMember(id)` + `addMember(new PlayerGroupMember(newPlayer))` — a **new** `Player` object (PlayerService.java:106) | PlayerEnterWorldService.java:307; PlayerGroupService.java:77-86; PlayerConnectedEvent.java:30-50 | the returner: `SM_GROUP_INFO`, `JOIN`, the others' `ENTER`; the others: `ENTER` |
| **Offline timeout**: `OfflinePlayerChecker` every 30 s: offline and `lastOnline + removetime` expired → `PlayerGroupLeavedEvent(LEAVE_TIMEOUT)` | PlayerGroupService.java:210-222 | `STR_PARTY_HE_BECOME_OFFLINE_TIMEOUT` + `LEAVE`; one left → `disband` |
| **Disband**: `FindGroupService.removeRecruitment(group)`, `groups.remove`, `GroupDisbandEvent` → **a nested `PlayerGroupLeavedEvent(DISBAND)` per member inside `forEach`** | PlayerGroupService.java:158-162; GroupDisbandEvent.java:22-24 | `STR_PARTY_IS_DISPERSED` + `SM_LEAVE_GROUP_MEMBER` |
| **Leave inside a team-registered instance**: the leaver (and, on a disband, every member) gets `STR_MSG_LEAVE_INSTANCE_NOT_PARTY` and an `INSTANCE_KICK` task: 30 s later, still not in that team and still in its instance → `InstanceService.moveToExitPoint` | PlayerLeavedEvent.java:57-66 | A-12; GA14 |

The C++ today: every row runs through unported bodies or classes with no file. **Two rows are reached by code that already runs**: the disconnect
(`PlayerLeaveWorldService.cpp:131-132` → `PlayerGroupService.cpp:57-63`, the `AION_UNPORTED` behind `if (group)`) and the reconnect
(`PlayerEnterWorldService.cpp:537, 541` → `PlayerGroupService.cpp:47-55`, which calls `PlayerGroup::getMember` — `AION_UNPORTED`,
`PlayerGroup.cpp:43-46` — **for every group, on every login, grouped or not**; E-12). A pending, unanswered invite is denied on logout
(`player.getResponseRequester().denyAll()`, `PlayerLeaveWorldService.cpp:105`) → `PlayerGroupInvite.denyRequest` (no file).

### 2.5 Alliances

`CM_INVITE_TO_GROUP(12, name)` → `PlayerAllianceService.inviteToAlliance` (an invited group member redirects the invitation to his leader) →
`SM_QUESTION_WINDOW(70000)` → `PlayerAllianceInvite.acceptRequest`: **the inviter's group and the invited group are dissolved member by member
(no lock held across the two teams) and every member is re-added to a new or the existing alliance** (PlayerAllianceInvite.java:28-67) →
`createAlliance` → 4 fixed alliance groups **with object ids 1000-1003 shared by every alliance** (`new PlayerAllianceGroup(this, groupId)`,
`autoRelease = false`, PlayerAlliance.java:27-34) → `PlayerAllianceEnteredEvent` → `SM_ALLIANCE_INFO` (SM_ALLIANCE_INFO.java:132-183: group
count, ids, **four vice-captain slots**, loot words, type, league id, the four group ids, message) and `SM_ALLIANCE_MEMBER_INFO`. Then
`CM_PLAYER_STATUS_INFO` commands 14 (leave), 16 (ban), 17 (set captain), 20-24 (**ready check**: `CheckAllianceReadyEvent` →
`SM_ALLIANCE_READY_CHECK`), 25/26 (vice captain), 27 (**move a member between alliance groups**: `ChangeMemberGroupEvent`);
`CM_GROUP_DISTRIBUTION(1)` inside an alliance group; alliance chat; the offline checker with **60 s for auto alliances**
(PlayerAllianceService.java:266-282); `disband(alliance, onBefore)`, **always called under the alliance's lock** (PlayerAllianceLeavedEvent.java:62,
70; PlayerDisconnectedEvent.java:57), which **takes the league's lock** (PlayerAllianceService.java:186-195). Max 24 members (PlayerAlliance.java:50),
6 per alliance group. Vortex (defence/offence) alliances reach `VortexService` (PlayerAllianceService.java:151-153, 173-174, 274-276) — M5i's.
C++ today: 48 sites + 40 undeclared bodies; `SM_ALLIANCE_MEMBER_INFO`'s constructor cannot even read its member (`SM_ALLIANCE_MEMBER_INFO.cpp:31-40`);
`PlayerAlliance::getMember` is unported (`PlayerAlliance.cpp:82-84`) and reached by every login once any alliance exists (E-12).

### 2.6 Leagues

`CM_INVITE_TO_GROUP(28, name)` → `LeagueService.inviteToLeague` (between alliance leaders, `SM_QUESTION_WINDOW(902249)`) → `LeagueInviteEvent`
→ `createLeague` (loot rules **FREEFORALL, 0, 0, 2, 2, 2, 2, 2** — LeagueService.java:85) + `LeagueJoinEvent` → `SM_ALLIANCE_INFO` with the
league block; commands 29 (leave), 30 (expel — **`expelAlliance` throws `IllegalArgumentException` for anyone but the league leader's alliance
leader**, LeagueService.java:114-122, which surfaces as an ERROR line for a client-sent expel), 31 (**move an alliance's position**), 32 (leader);
`LeagueLeftEvent` reorganizes positions and disbands a league of one; an alliance in a league reads the league's loot rules
(PlayerAlliance.java:140-143); max 8 alliances (League.java:51). **Every league event runs under `league.onEvent` and takes alliance locks**
(§2.11 item 3). C++ today: `League` 21 sites; `LeagueMember`, `LeagueService` and the 9 events have no file (42 undeclared bodies).

### 2.7 Find group

`CM_FIND_GROUP(action, …)` (CM_FIND_GROUP.java:36-142): `readImpl` knows **18 actions** (0-13, 15, 17, 20, 25) and `runImpl` has **16 arms**
(0-13, 15, 17): 0/4 list recruitments/applications of the own race, 1/2/3 remove/add/update a recruitment (keyed by the **team id when in a
team**, else the player id), 5/6/7 applications, 8-17 "instance groups" (a server-wide group is proxied by a normal team invitation —
FindGroupService.java:162-180), **13 every 50 s while the tab is open**. **20** ("Enter" in the prepare-for-entry window) and **25** (ban from an
instance group) are read and then ignored; every other action logs `Unknown find group action` (:115-117). `FindGroupService` — 19 sites, 2 of
them on the party path (`onJoinedTeam` on every join, `removeRecruitment(team)` on every disband); `showInstanceGroups` is also reached by
M5f's `PortalDialogAI` (`OPEN_INSTANCE_RECRUIT`, PortalDialogAI.java:72-73; m5f W-19). `onLogout` is already ported (`FindGroupService.cpp:96-100`).
The recruitment holds `const Ref<AionObject>` — the player or the team (`GroupRecruitment.h:27`).

### 2.8 Status by area (measured) — and the bodies no count shows

Counted with `grep -o 'AION_UNPORTED('` over `chunks.py files P5-10` at HEAD `760e8ab5c` + working tree, and with the undeclared-method script over
`chunks.py java P5-10` (91 Java files, 642 method bodies, 7,263 Java LOC). "no file" = Java classes with no C++ `.h`/`.cpp`; "undeclared" =
Java method bodies whose name appears in neither the class's C++ header nor its `.cpp` nor its generated header — **including constructors
(56 of the 167 in scope) and the methods of five generated enums whose companions nobody wrote (10)**.

| Area | Part (D1) | `AION_UNPORTED` | `AION_PARTIAL` | no file | undeclared | Java LOC | Milestone |
|---|---|---|---|---|---|---|---|
| team core: `GeneralTeam` 24, `TemporaryPlayerTeam` 7, `LootGroupRules` 7; `PlayerTeamMember`, `TeamEvent`, 7 common events, 2 services, 5 enum companions | P5-10a | **38** | 0 | 11 | **46** | 1,218 | **M5g** |
| parties: `PlayerGroup` 10, `PlayerGroupService` 18, `PlayerGroupStats` 4; `PlayerGroupMember`, 11 events; `TeamMoveUpdater`, `TeamStatUpdater` | P5-10b | **32** | 0 | 14 | **39** | 914 | **M5g** |
| alliances: `PlayerAlliance` 18, `PlayerAllianceGroup` 10, `PlayerAllianceService` 20; `PlayerAllianceMember`, 12 events | P5-10c | **48** | 0 | 13 | **40** | 1,210 | **M5g** |
| leagues: `League` 21; `LeagueMember`, `LeagueService`, 9 events | P5-10d | **21** | 0 | 11 | **42** | 780 | **M5g** |
| find group: `FindGroupService` 19 | P5-10e | **19** | 0 | 0 | 0 | 211 | **M5g** |
| **M5g's share of P5-10** | | **158** | **0** | **49** | **167** | **4,333** | |
| instance matchmaking: `AutoGroupService` 21 (**20 after M5f's N-06**, A-15), `AutoInstance` 12, `LookingForParty` 8, `AutoGroupUtility` 8, `AGPlayer` 1; `AutoHarmonyInstance`, `AutoPvpInstance`, `AutoPvPFFAInstance` no file; `AutoGroupType`'s 20 companion methods | P5-10e | 50 (49 at branch time; M5g's K-03 takes `isInAutoInstance`) | 0 | 3 | 52 | 1,538 | **M5j** (D16, O-01) |
| legion model: `Legion` 27, `LegionWarehouse` 28, `LegionEmblem` 6, `LegionMember` 5 | P5-10f | 66 | 0 | 0 | 7 | 974 | **M5h** (O-02) |
| challenges: `ChallengeTaskService` 8, `ChallengeTask` 7, `ChallengeQuest` 5 | P5-10f | 20 | 0 | 0 | 0 | 418 | **M5h** (O-02) |
| **P5-10 total** | | **294** | 0 | 52 | 226 | 7,263 | |

The legion part's 7 undeclared bodies: `LegionEmblemType` 1, `LegionHistoryAction` 2, `LegionPermissionsMask` 1, `LegionRank` 1, `LegionWarehouse`
2 (m5h-plan.md:74 counts the 5 enum-companion bodies only).

**Outside P5-10, on the paths this milestone turns on** (§2.10 lists the call sites):

| Body | Chunk | Count | Stage |
|---|---|---|---|
| `PlayerRestrictions::canInviteToGroup`, `canInviteToAlliance`, `canInviteToTeam`; `canChat` | P5-13 | 4 | 1 |
| `DropDistributionService` (`handleRollOrBid`, `handleRoll`, `handleBid`, `distributeLoot`) | P5-09 | 4 | 1 |
| `PlayerLifeStats::sendGroupPacketUpdate` | P5-01 | 1 | 1 |
| `PvpService::doReward`'s team arm (one statement; the PvP half stays, O-05); **`PlayerChatService::isFlooding`**, `logMessage` ×2; **`ChatBanService::isBanned`, `getBanMinutes`** — the last five on every chat message | P5-08 | 6 | 1 |
| **`RecallService::requestSummon`, `validateCast`, `canBeSummoned`, `canRecallAt`** (m5f hand-over) | P5-08 | 4 | 1 |
| **`RecallInstantEffect::calculate`, `applyEffect`** | P5-04 | 2 | 1 |
| `AutoGroupService::isInAutoInstance` (on every invite) | P5-10e | 1 | 1 |
| `SM_ALLIANCE_INFO` `leaguePosition`, `SM_ALLIANCE_MEMBER_INFO` constructor | P4-16 | 2 | 2 |
| the 12 client packets of §2.9 (`CM_CHAT_MESSAGE_PUBLIC` and `CM_RECALLED_BY_OTHER_ANSWER` included) | P5-15, P5-16 | 42 | 1 |
| the five team stand-ins in `ControllerStandIns.{h,cpp}` and their five call sites | P4-11b | 5 deletions | 1 |
| **Total outside, firm** | | **66** (+ 5 deletions) | |
| the chat **flood arm**: `ChatBanService::banPlayer`, `unbanPlayer`, `registerUnban` (+ its GAG runnable, `ChatBanService.cpp:15-18`) — ported by W-03 with the rest of the class, reached when a player sends more than `gameserver.security.flood.msg` = 6 messages less than `flood.delay` = 1 s apart (SecurityConfig.cpp:25-26; Player.java:1472-1478) | P5-08 | 3 | 1 |
| *conditional*: `DropService` team helpers (A-04) 7, `TemporaryTradeTimeTask` (A-07, no file) 4, `canTrade` (A-06) 1, `CM_QUESTION_RESPONSE` (A-05) 3, `QuestService::getEachDropMembersGroup/Alliance` (A-09) 2 | P5-09, P5-07, P5-13, P5-16, P5-06 | **0-17** | 1 |
| *conditional arms inside bodies other milestones port* (A-03): `initDropNpc`'s team branch, `requestDropItem`'s team branches, `addDropItems`' `member_limit` arm, `getQuestDrop`'s group and alliance arms | P5-09, P5-06 | 0-5 arms | 1 |

**The milestone, honestly: 158 sites + 167 undeclared + 66 outside = ~391 bodies (394 with the flood arm, up to ~411 if every fallback of §0
activates) over ~5,800 Java lines, 54 new classes** (49 with no file + 5 enum companions). About 56 of the bodies are constructors and ~40 are
one-line accessors, so the **substantive count is ~295**. For comparison M5b-2 was ~521 bodies (m5b2-plan.md §2.2), M5b-3 ~145
(m5b3-plan.md §1). **A lane that sizes from `grep -c AION_UNPORTED` will size the party events at zero** — they are 30 bodies in 11 classes that
have no file; this is the M5b-2 finding at a larger scale.

### 2.9 The client packets (lesson 4)

Of the **147** Java client-packet files with no C++ `.cpp` (146 `CM_*` + `AbstractGmCommandPacket`; 43 exist), M5g must add **12**:

| Packet | Chunk | Java LOC / bodies | Need | Reached solo? | Why |
|---|---|---|---|---|---|
| `CM_INVITE_TO_GROUP` | P5-15 | 72 / 3 | **R** | yes (the inviter) | the only way to form any team |
| `CM_PLAYER_STATUS_INFO` | P5-16 | 56 / 3 | **R** | yes (`GROUP_SET_LFG`) | leave, kick, leader, alliance and league commands (TeamCommand.java:10-30) |
| `CM_DISTRIBUTION_SETTINGS` | P5-15 | 79 / 3 | **R** | no | loot rules (m5b3-plan.md O-01) |
| `CM_GROUP_LOOT` | P5-15 | 60 / 3 | **R** | no | roll and bid (m5b3-plan.md O-01) |
| `CM_GROUP_DISTRIBUTION` | P5-15 | 58 / 3 | **R** | no | kinah split (m5b3-plan.md O-01) |
| `CM_FIND_GROUP` | P5-15 | 143 / 3 | **R** | **yes** — the window sends action 13 every 50 s | find group |
| `CM_SHOW_BRAND` | P5-16 | 45 / 3 | **R** | **yes** | target marks |
| `CM_GROUP_DATA_EXCHANGE` | P5-15 | 90 / 3 | **R** | **yes** (action 1) | the client's party UI sync |
| `CM_CLIENT_COMMAND_ROLL` | P5-15 | 39 / 3 | **R** (cheap) | yes (`/roll`) | handed over by m5b3-plan.md O-01 |
| `CM_QUEST_SHARE` | P5-16 | 83 / 3 | **R** (A-09) | yes ("no members to share with") | handed over by m5d-plan.md §3.7 |
| `CM_CHAT_MESSAGE_PUBLIC` | P5-15 | 155 / 9 | **R** (A-14) | **yes — all chat** | party, alliance, league chat |
| `CM_RECALLED_BY_OTHER_ANSWER` | P5-16 | 38 / 3 | **R** | yes (an answer with no pending request does nothing) | summon group member (m5f-plan.md:285, 302; D15) |
| `CM_QUESTION_RESPONSE` | P5-16 | 46 / 3 | only if A-05 failed | yes | accepting an invite |
| `CM_AUTO_GROUP` | P5-15 | 68 / 3 | **not M5g** — M5j (D16, O-01) | yes | instance entry registration |
| `CM_CHAT_GROUP_INFO` | P5-15 | 41 / 3 | **not M5g** — m5j-plan.md J1 | yes | a player's info from a chat name — not a team packet |
| `CM_INSTANCE_LEAVE` | P5-15 | 30 / 3 | M5f | – | instances |

**Server packets: none to write** (§1); **decoders to write** (the gate, H-02): `SM_GROUP_INFO`, `SM_GROUP_MEMBER_INFO`, `SM_LEAVE_GROUP_MEMBER`,
`SM_ALLIANCE_INFO`, `SM_ALLIANCE_MEMBER_INFO`, `SM_ALLIANCE_READY_CHECK`, `SM_SHOW_BRAND`, `SM_GROUP_LOOT`, `SM_FIND_GROUP`, `SM_MESSAGE`,
`SM_GROUP_DATA_EXCHANGE`, `SM_ABYSS_RANK_UPDATE`, `SM_RECALLED_BY_OTHER` — from the Java `writeImpl` (§2.1, §2.5 cite the lines). **Reused**:
M5d's `QuestDecoders` (`SM_QUEST_ACTION`), M5e's `SM_MANTRA_EFFECT`, M5f's `TravelDecoders` (`SM_TELEPORT_LOC`), M5b-2's `SkillDecoders`.

### 2.10 Reachability from every entry point M5g turns on (lesson 2)

Each row is an entry point, the first unported or partial body it reaches today, and what that means. Everything below is behind a team check a
solo character fails, **except** the rows marked *solo* and E-12.

| # | Entry point | Path | First unported body | Effect today once a team exists |
|---|---|---|---|---|
| E-1 | client: `CM_INVITE_TO_GROUP` | → `inviteToGroup` → `canInviteToTeam` → `isInAutoInstance` | `PlayerGroupService.cpp:27` | packet has no file (logged as unknown) |
| E-2 | client: `CM_QUESTION_RESPONSE(60000/70000/902249)` | → `respond` → `PlayerGroupInvite`/`PlayerAllianceInvite`/`LeagueInviteEvent` | no file | A-05 |
| E-3 | client: `CM_MOVE` of a member | `PlayerController::onMove` | **`standins::teamMoveUpdaterAdd` throws** (`PlayerController.cpp:592`) | an exception out of every move packet |
| E-4 | HP/MP change of a member (combat, regeneration task, MP cost) | `PlayerLifeStats::onHpChanged/onMpChanged` | **`PlayerLifeStats.cpp:70`** | throws on the damage path and inside the restore task |
| E-5 | level up of a member | `PlayerController::upgradePlayer` | **`standins::teamStatUpdaterAdd`** (`:710`) | throws in the level-up path (and in M5e's class change, m5e W-04) |
| E-6 | an effect on a member (any skill, potion, food, soul sickness) | `updatePlayerIconsAndGroup` | **`PlayerGroupService.cpp:66`** / `PlayerAllianceService.cpp:66` | throws inside `Effect.startEffect` / `endEffect` on scheduler threads — **an `endEffect` that throws half-way is the M5b-2 risk-2 leak shape** (m5b2-plan.md §8 item 2) |
| E-7 | npc death with a team attacker | `NpcController::doReward` | **`standins::playerTeamDistributionServiceDoReward`** (`NpcController.cpp:247`) | swallowed by `onDie`'s catch → an ERROR per kill and **no `InstanceHandler::onDie`, no `DIED` event** (the m5b-client-session.md S-1 shape) |
| E-8 | a member hits an npc first | `NpcController::onAddHate` | `GeneralTeam::filterMembers` (`GeneralTeam.cpp:57`) | throws on the attack path |
| E-9 | a member dies | `PlayerController::doReward` → `PvpService::doReward` | **`PvpService.cpp:75`** | throws in the death path |
| E-10 | a member revives | `PlayerReviveService::revive` | `PlayerGroupService.cpp:66` (`updateGroup`) | throws in revive |
| E-11 | enter world / teleport of a member | `CM_LEVEL_READY` +100 ms | `TemporaryPlayerTeam::sendBrands` | throws in a scheduled task |
| **E-12** | **enter world of any player, solo or not, while any group or alliance exists** | `PlayerGroupService::onPlayerLogin` iterates every group and calls `group->getMember(id)` (`PlayerGroupService.cpp:47-49`); `PlayerAllianceService::onPlayerLogin` the same (`PlayerAllianceService.cpp:47-49`) | **`PlayerGroup::getMember`** (`PlayerGroup.cpp:43-46`); **`PlayerAlliance::getMember`** (`PlayerAlliance.cpp:82-84`) | **every other player's login throws** — so GR-01's `getMember` (and AL-01's) must land with the first code that can form a group or an alliance |
| E-13 | logout of a member | `PlayerGroupService::onPlayerLogout` | `PlayerGroupService.cpp:61` | throws in leave world (the logout breakers still run, `PlayerLeaveWorldService.cpp:85`) |
| E-14 | logout with an unanswered invite | `ResponseRequester::denyAll` → `denyRequest` | no file | – |
| E-15 | scheduled: `OfflinePlayerChecker`, `OfflinePlayerAllianceChecker` (30 s), `TeamMoveUpdater` (2 s), `TeamStatUpdater` (0.5 s), `setPlayersInRoll` (17/32 s), `INSTANCE_KICK` (30 s), the chat GAG task (2 min) | – | all unported / no file | – |
| E-16 | tab target, instance info, kisk, portal cooldown | §2.2 row 13 | `GeneralTeam` bodies | throw |
| E-17 | rifts (`RVController.cpp:71, 73` → stand-ins), vortex (`VortexService` 3 bodies) | – | stand-ins / unported | unreachable: `gameserver.rift.enable`/`vortex.enable` are false in every gate profile; W-04 points the rift call sites at the real services, the vortex arms stay M5i's (O-04) |
| E-18 | events (`Event::onEnteredTeam`/`onLeftTeam`, `Event.cpp:50-56`) | `PlayerEnteredEvent`/`PlayerLeavedEvent` → `EventService` | unported | unreachable while `gameserver.event.service.disabled_events = *` (m5b.properties.example:25) — **a user who enables events throws on every join** |
| E-19 | *solo*: `CM_FIND_GROUP`, `CM_SHOW_BRAND`, `CM_GROUP_DATA_EXCHANGE`, `CM_CLIENT_COMMAND_ROLL`, `CM_QUEST_SHARE`, `CM_PLAYER_STATUS_INFO(9)`, `CM_CHAT_MESSAGE_PUBLIC`, `CM_RECALLED_BY_OTHER_ANSWER` | the packets themselves; **every chat message → `canChat` → `ChatBanService::isBanned` → `getBanMinutes`, `PlayerChatService::isFlooding`, then `logMessage`** | no file; then `PlayerRestrictions.cpp:196`, `ChatBanService.cpp:20-26`, `PlayerChatService.cpp:11-27` | a real client sends them today (logged unknown); **porting `CM_CHAT_MESSAGE_PUBLIC` makes `ChatProcessor::handleChatCommand` run command handlers, most of which are phase-6 scripted handlers** — typing `.` or `//` commands will reach unported handlers (§11) |
| E-20 | instance: a member leaves a team inside a team-registered instance | `PlayerLeavedEvent` → `INSTANCE_KICK` → `InstanceService::moveToExitPoint` | A-12 | GA14 gates it |
| E-21 | **visibility**: a grouped player enters anyone's known list | `SM_PLAYER_INFO` writes `getCurrentTeamId()` (`SM_PLAYER_INFO.cpp:200`); `SM_ABYSS_RANK_UPDATE(1)` the same (`:24`) | **`GeneralTeam::getTeamId`** (`GeneralTeam.cpp:81`) | throws while serializing the most common packet of all |
| E-22 | `Player::isInSameTeam` (`Player.cpp:717-720`) from `canSee` (`:728`, a hidden player or summon), `isEnemyFrom`/`isAggroIconTo` (`:685, 691`, FFA), `canPvP` (`:707`, capitals and instances), `DuelService::fixTeamVisibility` (being ported in the working tree, `DuelService.cpp:49-58`; reached only in a duel) | `getCurrentTeamId` | `GeneralTeam::getTeamId` | throws inside visibility and hostility checks |
| E-23 | a team kill's drop registration | `DropNpc::setLootingTeam` (`DropNpc.cpp:46-51`, A-03) | `getTeamId`; `getLootGroupRules` is inline | throws in `registerDrop` |
| E-24 | skill targeting with a team | `FirstTargetProperty.cpp:171-178, 198-201`, `TargetRangeProperty.cpp:92-97`, `TargetRelationProperty.cpp:57-58` | `getMembers`, `getCurrentTeamId` | throws when a grouped player casts |
| E-25 | find group's server-wide groups | `ServerWideGroup::getMembers` (`ServerWideGroup.cpp:27-31`) | `getMembers` | throws in `CM_FIND_GROUP` 10/13/15 for a recruiter in a team |
| **E-26** | scheduled: **a grouped player's mantra** (`AuraEffect.AuraTask`, every 6.5 s; A-11) | `AuraEffect::onPeriodicAction` → `getCurrentGroup()->getOnlineMembers()` | **`TemporaryPlayerTeam::getOnlineMembers`** (`TemporaryPlayerTeam.cpp:36-37`) | once M5e lands `AuraEffect`: throws on a scheduler thread every 6.5 s for as long as the mantra is on |
| **E-27** | a grouped caster's `HealCastorOnAttackedEffect` observer (only if M5e's T-02 landed) | the ATTACKED observer → `getCurrentGroup()->getOnlineMembers()` | `TemporaryPlayerTeam.cpp:36-37` | throws inside `onAttack` of the protected target |
| **E-28** | client: `CM_CASTSPELL` of a recall skill (3777) — **also solo** | `Skill::isInvalidRecall` → `RecallService::validateCast` (cast start) → `RecallInstantEffect::calculate/applyEffect` → `requestSummon` | `RecallService.cpp:87-88` | throws at cast start (a null first target is a null dereference until m5b2-p2-9, `Skill.cpp:853-860`) |
| **E-29** | a team kill's drop and quest-drop arms (A-03) | `addDropItems` with `member_limit > 1` (104 rules), `getQuestDrop` for a grouped killer of an npc with quest drops | `DropRegistrationService.cpp` / `QuestService.cpp:321-324` | whatever M5b-3/M5d left as `AION_UNPORTED` arms throws inside `registerDrop` (L-02) |

`TeamDamageList`'s constructor (`TeamDamageList.cpp:22-35`) keys a member's damage by `getCurrentTeam()` and needs no unported body; `SM_CHAT_WINDOW`
(`:36, 52`) reads `getMin/MaxExpPlayerLevel` but only `CM_CHAT_GROUP_INFO` (no file, M5j) sends it; `WorldMapInstance::registerTeam`
(`WorldMapInstance.cpp:188-194`) is ported and reaches `getTeamId` only through M5f's portal arms (A-12). **E-12 and E-21..E-25 all close with
`GeneralTeam`'s and `PlayerGroup`'s trivial bodies (C-01, GR-01)** — which is why the team core and `PlayerGroup::getMember` must land before
anything can form a team.

**None of E-3..E-29 is reachable by an earlier gate** (they never form a team, never chat, never cast a recall skill; E-12 iterates an empty
registry), which is why no holdback is needed (finding 3). All of E-3..E-16, E-19 and E-21..E-29 close in stage 1, together — a stage that forms a
group and leaves one of them open throws on the gate's own path.

### 2.11 Concurrency and lifetime facts the bodies must honour

1. **One reentrant lock per team, nested freely.** `onEvent`, `forEach`, `forEachTeamMember`, `applyOnMembers` lock `teamLock`
   (GeneralTeam.java:37-123); `TemporaryPlayerTeam.sendPacket`/`sendPackets` go through `forEach`, so **sending to a team locks it**
   (TemporaryPlayerTeam.java:55-66). Handlers call `onEvent` again (`GroupDisbandEvent` inside `forEach`, `PlayerDisconnectedEvent` →
   `ChangeGroupLeaderEvent`), send packets, schedule, call `FindGroupService`, `QuestEngine.onKill` (inside `forEach(filteredStats)`,
   PlayerTeamDistributionService.java:42, 122) and `Storage` kinah changes. The C++ `Monitor` is reentrant (`runtime/sync/Monitor.h:16-19`); the
   nested `forEach` over `members.values()` while a nested event removes members is weakly consistent in both languages.
2. **One lock class for every team today.** `teamLock{AION_LOCK_CLASS(GeneralTeam::teamLock)}` (`GeneralTeam.h:37`) — a group, an alliance, its
   four alliance groups and a league all report the same class. Nesting two locks of one class is `SAME_CLASS_NESTING` — **against every held
   lock, not only the innermost** (`LockOrderValidator.cpp:188-198`), reported once per class (`LockOrderValidator.h:23`).
3. **Java nests the alliance and league locks in both orders, and the reverse order is the minority.**
   - **League → alliance, in every league event.** Each runs under `league.onEvent` and takes alliance locks through `alliance.sendPackets`,
     `sendPacket` or `forEach`: `LeagueCreateEvent.java:20-22`, `LeagueJoinEvent.java:33-39`, `LeagueChangeLeaderEvent.java:32-42`,
     `LeagueLootRulesChangeEvent.java:24`, `LeagueMoveEvent.java:38-52`, `LeagueLeftEvent.java:43, 46, 51-60, 73`, `LeagueDisbandEvent.java:20`
     (a nested `LeagueLeftEvent` per alliance). `LeagueKinahDistributionEvent` takes none (`getOnlineMembers` does not lock).
   - **Alliance → league → other alliances, at eight sites**, each under the alliance's own `onEvent`: `AssignViceCaptainEvent.java:69`,
     `ChangeAllianceLeaderEvent.java:51` and `:63` (the second inside `team.forEach`, :53), `PlayerAllianceEnteredEvent.java:46`,
     `PlayerAllianceLeavedEvent.java:59`, alliance `PlayerDisconnectedEvent.java:59`, and `PlayerAllianceService.disband` at `:190` and `:194`
     (`league.onEvent(new LeagueLeftEvent(…))`, called only from inside alliance events, §2.5). Five of them call `League.broadcast`, which locks the
     league and sends through **every other alliance's** `sendPacket` (League.java:144-159); :63 does the same through `league.forEach`; the disband
     sites run a `LeagueLeftEvent`. **So each holds alliance A while taking alliance B** — `SAME_CLASS_NESTING` of `PlayerAlliance::teamLock` even
     with per-kind classes.
   - **Two Java deadlock windows follow**: (i) an alliance event in A (A → L) against any league event in L (L → A); (ii) alliance events in two
     alliances A and B of one league (A → L → B against B → L → A). With one lock class lockdep reports `SAME_CLASS_NESTING` on the first league
     event; with per-kind classes it reports a `CYCLE` (League ↔ PlayerAlliance) and `SAME_CLASS_NESTING(PlayerAlliance)` the first time an
     alliance-side site runs in a league. D4.
   - Nothing else nests teams: `PlayerAllianceInvite` dissolves the groups before it touches the alliance, with no lock held across them
     (PlayerAllianceInvite.java:46-67); alliance groups are locked only by their own `onEvent` (`distributeKinahInGroup`,
     PlayerAllianceService.java:259-263); `PlayerTeamDistributionService`'s league arm iterates `getMembers()` without the league lock
     (PlayerTeamDistributionService.java:40); `League.sendPackets` (chat, CM_CHAT_MESSAGE_PUBLIC.java:144) takes no league lock.
4. **The cycles and their cuts** (`cycles.toml`): `Player.playerGroup` / `playerAllianceGroup` cut by `removeMember → setPlayerGroup(null)`
   (:157-158); `GeneralTeam.members` by `removeMember` (:195); **`GeneralTeam.leader` is a C++ breaker: "leader cleared on the last leave"**
   (:194; runtime-architecture.md deviation 10, :1489) — the Java field keeps the last leader; **`PlayerAlliance.groups` is a C++ breaker in the
   `disband` port** (:198); `PlayerGroupStats.min/maxLevelPlayer` "accepted: cut elsewhere: Player.playerGroup" (:202-203); `WorldMapInstance.
   registeredTeam` cut at `destroyInstance` (:356). `PlayerLeavedEvent` and its two subclasses are **K5** and their `INSTANCE_KICK` task must
   capture `Ref<Player>` and the team, **not `this`** (`fieldmap.toml:59-61`; runtime-architecture.md §14.2(d)).
5. **What else retains a team or its members after they left:** a corpse's `DropNpc` — `lootingTeam` (a `Ref`, `fieldmap.toml:115`: "the drop
   retains the team until it decays") and **`inRangePlayers`, a list of `Ref<Player>`** (`DropNpc.h:29`), for **300 s** while the corpse still
   has drops — `WorldMapInstance.registeredTeam`, `GroupRecruitment.object`, the `CM_LEVEL_READY` 100 ms task (`CM_LEVEL_READY.cpp:126-128`),
   `INSTANCE_KICK` (30 s), `setPlayersInRoll` (17/32 s, captures the in-range players), the chat GAG task (2 min, captures the player). **The
   `cycles.toml:202-203` resolution assumes the group dies with its members; these holders keep it, and `PlayerGroupStats` keeps two
   `Field<Ref<Player>>` (`PlayerGroupStats.h:26-27`).** Worse, Java never clears them on removal: `onRemovePlayer` recomputes from the stale
   values (PlayerGroupStats.java:26-28, 37-50) and only `onAddPlayer` nulls them (:30-35), so a live group can hold a departed member's `Player`
   (D7).
6. **An offline member is a live `Player` held by the team** for up to `removetime` (600 s) + the 30 s check period, and the leak census reports
   objects removed from the world after 10 min (runtime-architecture.md:640; `RuntimeConfig.cpp:14`) — D6(c). **At shutdown no group or alliance
   survives**: the kick sets each player offline before its logout event (§2.4), so the last online member's `PlayerDisconnectedEvent` finds
   `getOnlineMembers()` empty and disbands (PlayerDisconnectedEvent.java:33-34; alliance :56-57). What the shutdown census
   (`RuntimeLifecycle.cpp:113-121`) can still see is item 5's holders — D6(b).
7. **Unsynchronized Java counters.** `LootGroupRules.nrRoundRobin`/`nrMisc` are plain ints written from the npc death thread, not under the team
   lock (DropRegistrationService.java:131-138); `DropNpc`/`DropItem` roll state likewise. Port faithfully as `Field<int32_t>` with
   `// java-race` (m5b2-plan.md D9).
8. **Identity.** No Java game object overrides `equals` (grep of `AionObject`, `VisibleObject`, `Creature`, `Player`), so every
   `player.equals(member)` in the events is **pointer identity** — and after a relog the team briefly holds the old and the new `Player` of one
   object id (§2.4). A C++ body that compares object ids instead changes `PlayerConnectedEvent`'s leader test (PlayerConnectedEvent.java:33-36).

---

## 3. Where this plan disagrees with the roadmap and the sibling drafts

| Source | This plan | Why |
|---|---|---|
| roadmap: "M5g groups … P5-10 … 294 unported bodies" | **158** of them are M5g's; with the invisible and outside bodies **~391** | §2.8 |
| roadmap: "Parties, alliances, find group" | + **leagues**, **loot rules and kinah split** (m5b3-plan.md D9/O-01), **experience sharing** (the `NpcController` stand-in), **group chat**, **quest share** (m5d-plan.md §3.7), **mantras over a group** (m5e O-07), **summon group member** (m5f O-03), **a group entering a group instance** (m5f D3), `CM_CLIENT_COMMAND_ROLL` | each is a group feature some earlier plan explicitly handed to M5g, or has no other owner (A-14) |
| roadmap: "find group" | the **Find Group window** (`FindGroupService`) only; **instance matchmaking (`AutoGroupService`, `CM_AUTO_GROUP`, `PvPArenaService`) goes to M5j** (D16, O-01) | matchmaking creates instances (A-12), its arenas and Dredgion need phase-6 instance handlers, and `gameserver.autogroup.enable = false` in every gate profile |
| manifest: P5-10 = "model.team, autogroup, findgroup, challenge tasks, team task updaters" | the legion **model** and the challenges are M5h's; the chunk is split (D1) | `model/team/legion/**` matches the P5-10 glob (`chunks.cmake:338-340`) though `LegionService` is P5-11 |
| m5d-plan.md:635: `ChallengeTaskService` → "M5h; M5g (the P5-10 chunk)" | **M5h** (O-02), through D1's P5-10f | it is legion and town content (ChallengeTaskService.java:18-23) |
| m5c-plan.md D13 / capacity-proposals.md §8.3: no stress, soak or ASan run without asking | the group stress run is **a proposal** (S-01), not a required item | the standing resource rule |
| capacity-proposals.md:459, 465-467 | proposes re-running CAP-2/CAP-4a at the end of M5g | per-member fan-out: every HP/MP change of every member sends `SM_GROUP_MEMBER_INFO` to five others every 500 ms |

### 3.1 Every hand-over the sibling plans make to M5g, and the answer

| Sibling says | Where | M5g's answer | Item / gate |
|---|---|---|---|
| M5b-3: every team arm of the drop path is M5g's (member_limit, each_member, quest drops, `DropDistributionService`, `SM_GROUP_LOOT`, `TemporaryTradeTimeTask`) | m5b3-plan.md:203-206, D9 (:399), O-01 | **taken**, all arms (A-03, A-04, A-07) | L-01..L-04; GP11-GP15 |
| M5c: `CM_QUESTION_RESPONSE`, `canTrade`, `TemporaryTradeTimeTask`, dialogs, the Daeva seed | m5c-plan.md D-04, A-08, A-09, D5 | assumed (A-05..A-08); fallbacks K-05, W-01, L-03 | – |
| M5d: `CM_QUEST_SHARE` | m5d-plan.md §3.7 (:340) | **taken** | K-04; **GP15b** |
| M5d: `getEachDropMembersGroup/Alliance` "reached only by a grouped player" | m5d-plan.md E-04 (:543) | **taken** if M5d left them | L-02, L-04 |
| M5e: `AuraEffect`'s team arm (mantras over a group) | m5e-plan.md:270, W-23 (:385), O-07 (:511) | **taken**: closes with C-02's `getOnlineMembers`; gated | C-02; **GP6b** (skill 1809) |
| M5e: `HealCastorOnAttackedEffect` (T-02, optional) — its team arm | m5e-plan.md:498; HealCastorOnAttackedEffect.java:39-48 | closes with C-02 if T-02 landed; a unit case in C-09 | C-09 |
| (no plan) `HealCastorOnTargetDeadEffect`'s team arm | HealCastorOnTargetDeadEffect.java:41-48 | **not reachable by data** (its only template, 19573, has no `healparty`); no owner needed | – |
| M5e: class change → `upgradePlayer` → `teamStatUpdaterAdd` | m5e-plan.md W-04 (:366) | **taken** | W-04 |
| M5f: `RecallService` (4) and `CM_RECALLED_BY_OTHER_ANSWER` (summon group member) | m5f-plan.md:183, 285, 302, O-03 (:438) | **taken** (D15) | W-06, K-02; **GP13b** |
| M5f: "`PortalService`'s group, alliance and league arms … M5g's gate re-verifies them" | m5f-plan.md D3 (:328), risk 8 (:524) | **taken**: the group arm through a real portal in stage 3 (the alliance and league arms need level ≥ 50 instances — unit-tested) | **GA12-GA14**; X-01 |
| M5f: `PeriodicInstanceManager` (rest), `InstanceScore`, `PvPArenaService` → "M5g/M5i" | m5f-plan.md:187, 313, O-03 | **not M5g**: matchmaking and its instance plumbing go to **M5j** (D16) | – |
| M5f: `FindGroupService.showInstanceGroups` from `PortalDialogAI`'s `OPEN_INSTANCE_RECRUIT` | m5f-plan.md W-19 (:242) | **taken** (the body is `FindGroupService`'s); the `INSTANCE_PARTY_MATCH` arm (`AutoGroupType`) is matchmaking → M5j | K-03 |
| M5f: N-06 leases P5-10 for `AutoGroupService::onLeaveInstance` | m5f-plan.md:188, 231, 379 | accepted; the lease must be released before I-01 (A-15) | I-01, I-05 |
| M5h: "M5g's D1 … hands P5-10f … `tests/team/P5-10f` … never a different letter" | m5h-plan.md A-18 (:49) | **kept as named** (D1). m5h's D3 (:382), I-01 (:404), stage heading (:409), X-08 (:483), lane (:505) and §7 row (:543) still say "P5-10b" / `tests/legion` — the integrator aligns them with A-18 (this plan cannot edit m5h) | I-01, m5g-15 |
| M5h: `CM_CHAT_MESSAGE_PUBLIC`, `canChat`, `logMessage` are M5g's | m5h-plan.md A-19 (:50), D12 (:391) | **taken** | K-04, W-01, W-03 |
| M5i: `CM_CHAT_MESSAGE_PUBLIC` "if still absent" | m5i-plan.md Z-01 (:319), :239 | M5g ports it, so Z-01 falls away | – |
| M5i: vortex alliances need `PlayerAllianceService` create/add/remove | m5i-plan.md D7 (:291), :210, :276 | delivered by AL-01..AL-03; the vortex arms inside them (`VortexService`) stay M5i's (O-04) | – |
| M5j: A-G1 expects P5-10, `TemporaryPlayerTeam::sendPacket`, `RecallService`, `PvPArenaService`, `sendGroupPacketUpdate`, `SM_ALLIANCE_*`, and 10 packets incl. `CM_AUTO_GROUP` and `CM_RECALLED_BY_OTHER_ANSWER` | m5j-plan.md A-G1 (:65) | **all but `CM_AUTO_GROUP` and `PvPArenaService`**, which M5j must take (D16). The integrator records it in m5j A-G1 | D16 |
| M5j: J1/K-04/K-05 plans `CM_CHAT_MESSAGE_PUBLIC`, `canChat`, `PlayerChatService` 4, `ChatBanService` 5 | m5j-plan.md J1 (:165-170), K-04/K-05 (:527-528) | M5g ports `CM_CHAT_MESSAGE_PUBLIC`, `canChat`, `ChatBanService` 5, `isFlooding`, `logMessage` ×2; **J1 keeps** the whisper, `CM_CHAT_GROUP_INFO`, `CM_CHAT_PLAYER_INFO`, `logWhisper` and the command framework. The integrator records it in m5j J1 | K-04, W-03 |

---

## 4. Decisions

Decisions marked **integrator** follow the roadmap's decision rule (the integrator takes the recommendation and records it); **user** marks the
ones that go to the user.

| # | Decision | Why |
|---|---|---|
| **D1** integrator | **Split P5-10 in the manifest into six parts sharing `aion_gs_team`**: **P5-10a** team core (`model/team/*.java`, `model/team/common/**`), **P5-10b** parties (`model/team/group/**`, `taskmanager/tasks/{TeamMoveUpdater,TeamStatUpdater}`), **P5-10c** alliances (`model/team/alliance/**`), **P5-10d** leagues (`model/team/league/**`), **P5-10e** find group and matchmaking (`services/findgroup/**`, `services/autogroup/**`, `model/autogroup/**`, `services/AutoGroupService`), **P5-10f** legion model and challenges (`model/team/legion/**`, `model/challenge/**`, `services/ChallengeTaskService`), handed to M5h. Test directories `tests/team/P5-10a..f` (the manifest's rule for a shared target, `chunks.cmake:15-16`); `tests/team/TeamServicesM5aTest.cpp` moves to `P5-10b` (it tests group, alliance, find-group and auto-group services; split it if `check-ownership` asks). **The names are final**: m5h-plan.md rev 2 A-18 adopts exactly them ("P5-10f as named there, never a different letter"); its older D3/I-01/lane rows that say "P5-10b" / `tests/legion` are aligned by the integrator (§3.1). The split waits for M5f's N-06 lease to be released (A-15). | 158 + 167 bodies in one chunk is one lane (M5b-2 D1, `chunks.cmake:258-274` precedent). Six parts give stage 1 two P5-10 lanes (core, parties) and a find-group part the packets lane can own, and stage 2 two more (alliances, leagues). The legion part keeps M5h from needing a P5-10 lease. |
| **D2** integrator | **Scope**: parties, alliances, leagues, find group, loot rules and every team loot arm, kinah split, experience sharing, group buffs, mantras and member icons, party/alliance/league chat (with the chat ban and flood checks), quest share, summon group member, target marks, and the verification of a group entering a group instance. **Out**: instance matchmaking (O-01 → M5j), the legion model and challenges (O-02), a gated mentoring case (O-03, unit-tested), vortex team arms (O-04), the PvP half of `PvpService::doReward` (O-05), a gated alliance or league instance entry (O-06, unit-tested), cross-server find-group actions (O-07). | the roadmap row and the hand-overs of §3.1 |
| **D3** integrator | **Three stages, parties first**: stage 1 ports the party path (six lanes); stage 2 gates it (`gs.scenario.m5g`) while the alliance and league lanes run; stage 3 gates alliances, leagues and the group-instance entry (`gs.scenario.m5g_alliance`). | §9 |
| **D4** integrator (header request m5g-1) | **Per-kind lock classes; Java's order kept; the minority alliance → league order suppressed at each of its eight sites.** (a) `GeneralTeam`'s constructor takes the lock class, so `PlayerGroup`, `PlayerAlliance`, `PlayerAllianceGroup` and `League` report `…::teamLock` of their own (m5g-1): League → PlayerAlliance, the order of every league event, is then an ordinary recorded edge. (b) Every nesting keeps Java's order exactly; each alliance-side site carries `// java-race: alliance → league here; league → alliance in every league event (§2.11 item 3)`. (c) **The eight alliance-side sites of §2.11 item 3** — `AssignViceCaptainEvent.java:69`, `ChangeAllianceLeaderEvent.java:51` and `:63`, `PlayerAllianceEnteredEvent.java:46`, `PlayerAllianceLeavedEvent.java:59`, alliance `PlayerDisconnectedEvent.java:59`, `PlayerAllianceService.java:190` and `:194` — each open a `LockdepSuppression` (`// lockdep: Java takes the league, and through it the other alliances, under this alliance's lock`) around the league call. It covers the league lock and every other alliance's lock taken inside (`LockOrderValidator.h:29-30`: no edges, no reports, still recorded as held), i.e. both the reverse edge and the `PlayerAlliance → PlayerAlliance` nesting. The nine league events record their edges normally. (d) The deviation row names both Java deadlock windows of §2.11 item 3. **Rejected**: suppressing the league side (the majority: nine events, and it would hide the one order the league legitimately uses); a documented `CYCLE` plus a `SAME_CLASS_NESTING` allow-list (the scenario gates have no lockdep allow-list — `M5aScenarioTest.cpp:2073-2074`, `M5bScenarioTest.cpp:2145-2146` assert `lockdep.txt` empty — and `SAME_CLASS_NESTING` is reported once per class, so an allowed one hides every later `PlayerAlliance` nesting); reordering the locks (not Java); a snapshot-then-send rewrite (changes what members see mid-change); one lock class for all teams (every league event would be `SAME_CLASS_NESTING`). **Settled before the league lane starts**; LG-04 proves it. | §2.11 items 2-3. Java can deadlock here today; the port must not pretend otherwise, and must not flood the validator either. The suppression covers eight named call sites on the minority side, not a class. |
| **D5** integrator | **Team iteration order is a recorded deviation.** Leader succession and the round-robin looter follow the C++ stripe order, not Java's table order; the gate asserts only order-free facts (a successor is an online member; two consecutive round-robin loots **with an unchanged member set** go to different members). A faithful order would need a Java-`ConcurrentHashMap` order model over a live, resizing map — the `JavaHashMapOrder` helper (`dataholders/detail/JavaHashMapOrder.h:41`) models a static `HashMap` only. **Revisit if the user notices** (§11 asks). | §2.11; finding 5 |
| **D6** user (config) / integrator (code) | **Offline members and the census.** (a) The gate profiles set `gameserver.playergroup.removetime = 5` and `gameserver.playeralliance.removetime = 5`, and every gate loots every corpse empty; (b) **no team-registry dissolution at shutdown** (rev 1 proposed one; the premise was wrong — §2.11 item 6: the last online member's logout already disbands every group and alliance). What can outlive the members is a corpse's `DropNpc` (`lootingTeam`, `inRangePlayers`) and `WorldMapInstance.registeredTeam` (cut at `destroyInstance`). G-02 checks whether M5b-3 clears the drop registration map at shutdown; if not, **G-02's `ShutdownHook` step (P5-14) clears it after every player has left** — a C++-only step with a deviation row — so a team corpse left at shutdown does not put its members in the shutdown census; C-09 tests the holder, C22 stops the server **with a live group** to prove the premise; (c) **the user decides** between raising `gameserver.debug.leak_census_minutes` above `removetime + 30 s` (e.g. 11) and accepting a census warning for every member who logs out of a team for more than 10 minutes. Recommendation: (c) = 11, because the census is only useful while it is quiet. | §2.11 items 5-6; finding 4. (c) changes a default the user sees in his logs. |
| **D7** integrator | **Extend the "last leave" C++ breaker (deviation 10) to `PlayerGroupStats.minLevelPlayer/maxLevelPlayer`**: when the last member leaves, clear the leader **and** both stats references. Observationally neutral (a group with no members is never read again — `PlayerGroup` is not reused after `disband`). **Keep Java's stale-reference behaviour in a live group** (a departed member can stay referenced until the next join — PlayerGroupStats.java:26-50) and record it: it is a Java bug that also skews `getMinExpPlayerLevel`, read by `GroupRecruitment` and `SM_CHAT_WINDOW`. | §2.11 item 5; without it a corpse's `DropNpc.lootingTeam` keeps a disbanded group and up to two `Player` objects for 300 s |
| **D8** integrator | **The party gate uses four accounts, all Elyos**: A Warrior (leader), B Mage, C Priest, D Scout (the outsider; later a member). **Levels are chosen by the oracle (H-01), not by the plan**: the smallest levels with A = B < C (all below a 10-level gap) for which **no asserted experience share reaches its member's cap** `expNeed × 0.2f` (Rates.java:21-27; caps 80 / 206 / 477 / 1,046 at levels 1-4, player_experience_table.xml), the `level / sum` and `1 / size` values differ for A and for C, and the value under `gameserver.rates.xp.group` differs from the solo rate. Working assumption (inferred): A = B = 3 or 4, C two levels higher. The profile sets **`gameserver.rates.xp.group = 1.5, 3.0`** (solo stays `1.0, 2.0`, RatesConfig.java:30-34) so the group rate is observable. B is seeded with skills **3195** *Focused Evasion* (a BUFF-slot self-buff, m5b2-plan.md X7), **1576** *Word of Inspiration* (the group buff: PARTY/MYPARTY, range 20, 3 × `statup` 15,000 ms, `mp 252 delta -7`) and **1809** *Celerity Mantra* (the mantra: aura 8998, CHANT slot, 25 m); A with **3777** *Summon Group Member* and one item **169300011** *Dimensional Fragment*, and 1,000 kinah; B with quests **1102** (START; `quest_kill` 210133/210134, `cannot_share`) and the oracle's **shareable** quest (START; e.g. 1105 *The Snuffler Headache*, `item_collecting`, minlevel 1). **Stage 3 adds two Elyos Daevas E, F at level 25** (the m5c D5 / m5f D4 seed; H-01 picks the class — a Templar, as m5f D4) at the Nochsana Training Camp portal in Eltnen. | Two characters prove a pair, not a party: leader succession, "all except the actor", round robin over more than two and the level split need three; the outsider proves who does *not* receive a packet. The level gap is what makes `level / partyLvlSum` differ from `1 / size`; an uncapped share is what makes the formula visible at all (at A = B = 1 every share of 210663 hits the 80 cap, §14 finding 6). The M5a gate already runs two accounts concurrently (`M5aScenarioTest.cpp:1298-1307`). |
| **D9** integrator | **Loot cases run at `gameserver.rates.drop = 1000000`** (m5b3-plan.md §2.4) **with the rules set by the gate**: `CM_DISTRIBUTION_SETTINGS` to FREEFORALL with every quality word 0 before the brands, then ROUNDROBIN for the experience kills, then the defaults (superior..mythic 2) for the roll case — at that rate 210663 drops a "Potions (Rare)" entry on every kill (RARE → `getQualityRule` true → roll). **Every corpse is looted empty** before the next case (m5b3-plan.md Y14's discipline), because a corpse with drops keeps its team and its in-range members for 300 s. | deterministic prompts without data changes |
| **D10** integrator | **No `AION_PARTIAL` is added.** The alliance and league arms of `CM_INVITE_TO_GROUP`, `CM_PLAYER_STATUS_INFO`, `CM_DISTRIBUTION_SETTINGS` and `CM_GROUP_DISTRIBUTION` are written faithfully in stage 1 and throw into their `AION_UNPORTED` services until stage 2 lands. | m5b2-plan.md D6: an unported arm must fail loudly |
| **D11** integrator | **`PvpService::doReward`'s team arm (one statement) closes in stage 1** under a P5-08 lease; the PvP half stays unported (O-05). | E-9 is on the death path of every grouped player; GP17b gates it |
| **D12** integrator | **No geo re-run of the M5g scripts.** The team paths do not consult geodata: no `GeoService`, `canSee` or `isVisible` in any P5-10 Java file, `DropService`, `CM_INVITE_TO_GROUP` or `PlayerRestrictions` (grep); every range is `PositionUtil.isInRange`. **The one geo-dependent step on the gated path is the skill side**: the PARTY arm of `TargetRangeProperty` filters members through `checkGeo` → `GeoService.canSee(firstTarget, member)` (TargetRangeProperty.java:76, 155-167), the same helper the AREA arms use (:52, :92). If M5b-2's `gs.scenario.m5b2_geo` (m5b2-plan.md G-04) built a line-of-sight fixture, add **one** case to a `gs.scenario.m5g_geo` (a member within 20 m behind geometry does not receive 1576); otherwise leave it to that gate's shared `checkGeo` row and record it in §13. A full geo run would repeat the script and add ~5 min under the `RESOURCE_LOCK`. | m5b-plan.md §6.4 precedent: say what geo changes and assert only that |
| **D13** user | **The group stress run is a proposal** (S-01): 20 clients forming, filling, splitting and dissolving parties and alliances, killing together, rolling, relogging within `removetime`, under ASan — only in a slot the user picks, and its shape joins the capacity conversation. | capacity-proposals.md §8.3 resource rule; m5c-plan.md D13 |
| **D14** integrator | **`CM_CHAT_MESSAGE_PUBLIC` is ported whole in stage 1** (A-14), with `canChat`, **all five `ChatBanService` bodies** (the flood arm included, with its GAG runnable as a controller task — a `cycles.toml` row in C-08) and `PlayerChatService::isFlooding` and `logMessage` ×2. Its LEGION arm reaches `PacketSendUtility::broadcastToLegion` (ported) and is unreachable before M5h. M5j's J1 shrinks accordingly (§3.1). | group chat is a group feature; every message reaches the ban and flood checks, and a player who types fast reaches the flood arm |
| **D15** integrator | **Summon group member is M5g's** (m5f hand-over): `RecallService`'s four bodies, `RecallInstantEffect` (P5-04), `CM_RECALLED_BY_OTHER_ANSWER`, and header request m5b2-p2-9 with its `Skill.cpp` caller if M5e/M5f did not apply it. | it needs a group member target (FirstTargetProperty's `TARGET_MYPARTY_NONVISIBLE`); m5j A-G1 assumes M5g delivers it |
| **D16** integrator | **Instance matchmaking is not M5g's: M5j takes it** — `AutoGroupService` (20 after N-06), `model/autogroup`, `AutoGroupType`'s companion, `CM_AUTO_GROUP`, `PvPArenaService`, and what m5f leaves of `PeriodicInstanceManager` and `InstanceScore` — as one lane beside m5j's J13 instance residue, with the phase-6 handlers of the matched instances. The integrator records it in m5j A-G1 (drop `CM_AUTO_GROUP` and `PvPArenaService` from what M5j assumes of M5g) and in m5f O-03 ("M5g/M5i" → M5j/M5i). | matchmaking creates instances and scores PvP arenas: without M5f's engine and phase-6 instance handlers it asserts nothing; `gameserver.autogroup.enable = false` in every gate profile. It moves work between milestones without dropping it |
| **D17** integrator | **The group-instance entry M5f ported blind is gated in stage 3**, through a real portal: two level-25 Daevas at `portal_dialog` npc 800507 in Eltnen → Nochsana Training Camp (300030000, min level 25, max 6 — the lowest group instance; instance_cooltimes.xml, portal_template2.xml:1357-1360) (GA12-GA14). The alliance and league arms need an instance with more than 6 members, the lowest of which is level 50 (Abyssal Splinter 300220000); they are verified by a unit test with real teams (X-01), not a gate. | m5f D3 relies on it; nobody else can run it before phase 6 |

---

## 5. Work items

Effort: **S** < 1 agent-day, **M** 1-2, **L** 2-4, **XL** > 4 (budgeted as S ≈ 0.5, M ≈ 1.5, L ≈ 3, the m5b3-plan.md:524-525 scale). Need: **R**
required, **W** stub allowed, **O** optional. "Deps" names items and §0 assumptions.

### Integrator (stage 0)

| Id | What | Deps | Need | Eff |
|---|---|---|---|---|
| I-01 | **D1's manifest split** into P5-10a..f with their `tests/team/P5-10x` directories; move `TeamServicesM5aTest.cpp`; `chunks.py check` and `check-ownership` green | A-15 | R | S |
| I-02 | **The header batch of §7** — 54 new class headers **and a `.cpp` shell with an `AION_UNPORTED` body for every one of the 49 no-file classes** (so stage-1 packets link against `LeagueService`, `PlayerAllianceMember` and the events before their lanes run), the `GeneralTeam` lock-class change (D4), the stand-in deletions, the `PlayerGroupStats` breaker method (D7), the `fieldmap.toml` kinds of the new events. **In two parts**: **I-02a** (day 0-1) — core, parties, `LeagueService.h`, `PlayerAllianceMember.h` and whatever stage 1's packets name; **I-02b** (during stage 1) — the alliance and league events | – | R | **L** |
| I-03 | Leases (one active per chunk): P5-07 for L-03 if A-07 failed; P5-06 for L-02's quest-drop arms; P5-14 (`CheckOutput`, `ShutdownHook`) for G-02; P4-16 for AL-04; P5-08 for W-03 and W-06; P5-04 and P5-02a (one line) for W-06; P5-13 test lease (`tests/instance`) for X-01 in stage 3 | – | O | S |
| I-04 | `game-server/config/m5g.properties.example` in the Java tree beside the earlier examples (the m5b-plan.md I-01 location): the M5d profile + `rates.drop = 1000000`, **`rates.xp.group = 1.5, 3.0`**, both `removetime = 5`, events off, `autogroup`/`rift`/`vortex` off | – | R | S |
| I-05 | **Branch-time check of §0** (A-01..A-16): record which fallbacks (K-04, K-05, L-02, L-03, W-01's `canTrade`, W-06's m5b2-p2-9) activate; re-count P5-10 and P5-10e after N-06 | – | R | S |

### Stage 1, team core (P5-10a)

| Id | What | Java refs | Deps | Need | Eff |
|---|---|---|---|---|---|
| **C-01** | `GeneralTeam` — 24 bodies; `onEvent`'s lock + `checkCondition` + `log.warn("[TEAM] skipped event")`; **the last-leave breaker** (leader and, through D7, the `PlayerGroupStats` references) — and **`getName()` must not throw after the leader is cleared**, because the skipped-event warning formats the team (a deviation row) | GeneralTeam.java:37-207 | I-02a | R | M |
| **C-02** | `TemporaryPlayerTeam` 7 (`updateBrand`, `sendBrands`, `getRace`, `sendPackets`, `sendPacket` with the `AionObject&` → `Player&` adapter, **`getOnlineMembers`** — which also closes M5e's `AuraEffect` and `HealCastorOnAttackedEffect` team arms, A-11 —, `setLootGroupRules`); **new** `TeamEvent.h`, **`PlayerTeamMember`** (12 bodies; `RefCounted` + `TeamMember`, forwarding `retain`/`release`, runtime-architecture.md §14.2(d) shows the shape) | TemporaryPlayerTeam.java:40-83; PlayerTeamMember.java | C-01 | R | M |
| **C-03** | **The 7 common events** (new files, 18 bodies): `AbstractTeamPlayerEvent`, `AlwaysTrueTeamEvent`, `ChangeLeaderEvent` (`changeLeaderToNextAvailablePlayer`), `PlayerEnteredEvent`, **`PlayerLeavedEvent`** (the K5 `INSTANCE_KICK` capture, A-12), `PlayerStopMentoringEvent`, `TeamKinahDistributionEvent`. Generic `T` erased to `TemporaryPlayerTeam&` with narrowing in the subclasses (hub-headers.md §8.1-§8.2) | common/events/*.java | C-01 | R | M |
| **C-04** | **The 5 enum companions** (new): `TeamType` (`getType`, `getSubType`, `isAutoTeam`, `isOffence`, `isDefence`), `TeamCommand` (`getCodeId`, `getCommand` — throws for an unknown code, TeamCommand.java:52-56), `GroupEvent`, `LootRuleType`, `PlayerAllianceEvent` `getId`. **O**: replace the packets' local tables (`detail::teamTypeType`, `lootRuleId`, `groupEventId`, `allianceEventId` — `PacketSupport.h:137-151`, `SM_GROUP_MEMBER_INFO.cpp:27`, `SM_ALLIANCE_MEMBER_INFO.cpp:22`) under a P4-16/P4-17 lease (header-requests.md:164) | the five enums | I-02a | R / O | S |
| **C-05** | `LootGroupRules` 7 (`getQualityRule`, `isMisc`, `getAutodistributionId`, **`setPlayersInRoll`** — a scheduled task capturing the players, → `DropDistributionService::handleRollOrBid`, `addItemToBeDistributed`, `containDropItem`, `removeItemToBeDistributed`) | LootGroupRules.java:55-150 | – | R | S |
| **C-06** | **New** `PlayerTeamCommandService` (3): every `TeamCommand` arm; the alliance and league arms call their services (D10) | PlayerTeamCommandService.java:22-90 | C-01 | R | S |
| **C-07** | **New** `PlayerTeamDistributionService` + nested `PlayerTeamRewardStats` (3 bodies, **the experience share**): `Math.round(... / (float) partyLvlSum)` (a `float` round to `int`), `rewardXp *= damagePercent` as Java's compound narrowing `(long)(rewardXp * damagePercent)`, the ≥ 10-level rule, the league branch, the mentor and chest arms, `registerDrop` (A-03) | PlayerTeamDistributionService.java:35-136 | C-01, A-03 | R | M |
| **C-08** | `cycles.toml` / `fieldmap.toml` review for the new captures: `setPlayersInRoll`'s task, `INSTANCE_KICK`, the two FIFO updaters' `Ref<Player>` queues, `OfflinePlayerChecker` (K3), the invite handlers held by `ResponseRequester` (`cycles.toml:206` already resolves `LeagueInviteEvent.invited`), **the chat GAG runnable** (`ChatBanService$1`, a controller task — no row today, m5j-plan.md:527), **the recall timeout** (`spine-status.md:382` accepted it as a one-shot); re-check `:202-203` against D7 | – | C-01..C-07 | R | S |
| **C-09** | Tests `tests/team/P5-10a`: `onEvent` skip path; nested events under the reentrant lock on a `DeterministicExecutor`; `filterMembers`/`applyOnMembers` stop semantics; **`PlayerTeamDistributionService` golden vectors** checked against the Java arithmetic (uncapped levels, a member 10 levels below, a mentor, an out-of-range member, damage shares 1.0 and 0.37, a capped share under `XP_GROUP_HUNTING` and the same xp under `XP_HUNTING` with different rates) — mutation-proven: replace `partyLvlSum` by the member count, drop the `>= 10` rule, drop the range check, swap the rate; `TeamKinahDistributionEvent` remainder; `getOnlineMembers` with an offline member (the aura and heal-on-attacked team arms' source); **the last-leave breaker with a fabricated external holder — a `DropNpc` holding `lootingTeam` and `inRangePlayers`** — the group and every `Player` are reclaimed once the holder goes | – | C-01..C-07 | R | L |

### Stage 1, parties (P5-10b)

| Id | What | Java refs | Deps | Need | Eff |
|---|---|---|---|---|---|
| **GR-01** | `PlayerGroup` 10 (constructor with `IDFactory` id, `addMember`/`onRemoveMember`, **the four narrowing accessors — `getMember` first: every login iterates the group registry (E-12), so it must merge with, never after, the first code that can form a group**), **new** `PlayerGroupMember`, `PlayerGroupStats` 4 (faithful, stale references kept, D7's clear method) | PlayerGroup.java:15-52; PlayerGroupStats.java | C-01, C-02 | R | S |
| **GR-02** | `PlayerGroupService` 18 incl. `OfflinePlayerChecker` and `initializeOfflineCheck` (1,000 ms / 30,000 ms, started once by `compareAndSet`) | PlayerGroupService.java:37-222 | GR-01 | R | M |
| **GR-03** | **The 11 group events** (new, 30 bodies): `PlayerGroupInvite` (a `RequestResponseHandler` subclass, created with `create()`), `PlayerGroupEnteredEvent`, `PlayerGroupLeavedEvent`, `GroupDisbandEvent`, `ChangeGroupLeaderEvent`, `ChangeGroupLootRulesEvent`, `PlayerConnectedEvent`, `PlayerDisconnectedEvent`, `PlayerGroupUpdateEvent`, `PlayerStartMentoringEvent`, `PlayerGroupStopMentoringEvent` | group/events/*.java | GR-02, C-03 | R | L |
| **GR-04** | **New** `TeamMoveUpdater` (2,000 ms) and `TeamStatUpdater` (500 ms) on `AbstractFIFOPeriodicTaskManager<Player>` (the `MovementNotifyTask` precedent, P4-10), including the alliance arm | TeamMoveUpdater.java; TeamStatUpdater.java | GR-02 | R | S |
| **GR-05** | Tests `tests/team/P5-10b`: every §2.4 row as an in-process packet recording on `PlayerEventsTestSupport.h` players (the fixture lives in `tests/playersvc` and `tests/team` already includes it, `TeamServicesM5aTest.cpp:9`); the leader-succession set; the offline timeout on a `ManualClock`; the relog replacement (old `Player` reclaimed, new one in the group); a solo login while a group exists (E-12); disband inside `forEach`; `PlayerGroupStats`' stale reference as Java has it (a characterization test naming PlayerGroupStats.java:26-50); the updater queues drained. Mutation-proven: `PlayerGroupUpdateEvent` sending to the actor too; `ChangeGroupLeaderEvent` skipping `changeLeader`; `GroupDisbandEvent` not nesting | – | GR-01..GR-04 | R | L |

### Stage 1, loot distribution (P5-09, + P5-07 and P5-06 leases)

| Id | What | Java refs | Deps | Need | Eff |
|---|---|---|---|---|---|
| **L-01** | `DropDistributionService` 4 | DropDistributionService.java:29-146 | C-05, A-03 | R | M |
| **L-02** | **Every team arm M5b-3 and M5d left** (A-03, A-04, A-09; m5b3 D9): the `DropService` team helpers (`canDistribute`, `canAutoLoot`, `distributeEqually`, `winningRollActions`, `winningBidActions`, `winningNormalActions`, `TempTradeDropPredicate::changeItem`) **if A-04 failed**; and whichever of these M5b-3/M5d ported without their team arm: `initDropNpc`'s team branch (DropRegistrationService.java:126-159), `requestDropItem`'s team branches (DropService.java:310-395), **`addDropItems`' `member_limit > 1` arm** (DropRegistrationService.java:233-250), **`QuestService.getQuestDrop`'s group and alliance arms** (QuestService.java:676-724, P5-06 lease) and **`getEachDropMembersGroup/Alliance`** (QuestService.java:903-933). `DropGroup.addDropItem`'s `each_member` arm is already ported (`DropGroup.cpp:51-62`) — verify only | DropService.java:188-270, 412-421, 442-526 | C-05, A-03 | R | M (L if every arm is left) |
| **L-03** | `TemporaryTradeTimeTask` (**only if A-07 failed**; P5-07 lease) | TemporaryTradeTimeTask.java | – | R | S |
| **L-04** | Tests `tests/economy`: roll and bid tables with a seeded `Rnd` (strict `>`: the first roller keeps a tie; all pass → free for all; a bid larger than the kinah is 0), the 17 s / 32 s automatic pass on a `DeterministicExecutor`, `distributeEqually`'s remainder to list index 0, the misc round robin, `canAutoLoot` with an offline member; **one case per team drop arm**: a `member_limit = 2` rule for a group of three (two items, two distinct members), an `each_member` custom drop for a group of three, a group quest drop with and without `drop_each_member`, the alliance variants. Mutation-proven: `>=` for `>`; the automatic pass never scheduled; `member_limit` ignored | – | L-01..L-03 | R | M |

### Stage 1, packets and find group (P5-15, P5-16, P5-10e)

| Id | What | Java refs | Deps | Need | Eff |
|---|---|---|---|---|---|
| **K-01** | `CM_INVITE_TO_GROUP`, `CM_PLAYER_STATUS_INFO`, `CM_DISTRIBUTION_SETTINGS`, `CM_GROUP_DISTRIBUTION` with `AION_CLIENT_PACKET` markers | §2.9 | I-02a, A-06 | R | M |
| **K-02** | `CM_GROUP_LOOT`, `CM_CLIENT_COMMAND_ROLL`, `CM_SHOW_BRAND`, `CM_GROUP_DATA_EXCHANGE` (its `MAX_EXCHANGE_DATA_SIZE` guard), **`CM_RECALLED_BY_OTHER_ANSWER`** (D15) | §2.9; CM_RECALLED_BY_OTHER_ANSWER.java:25-36 | I-02a | R | M |
| **K-03** | `CM_FIND_GROUP` (18 read cases, 16 run arms, §2.7) + `FindGroupService` 19 + `AutoGroupService::isInAutoInstance` (one line, AutoGroupService.java:349-351) | CM_FIND_GROUP.java:36-142; FindGroupService.java | C-01 | R | M |
| **K-04** | `CM_CHAT_MESSAGE_PUBLIC` (A-14, D14) and `CM_QUEST_SHARE` (A-09: without M5d no player holds a quest state, so it returns at CM_QUEST_SHARE.java:53-55) | CM_CHAT_MESSAGE_PUBLIC.java:39-151; CM_QUEST_SHARE.java:39-82 | W-01, W-03 | R | M |
| **K-05** | `CM_QUESTION_RESPONSE` (**only if A-05 failed**) | CM_QUESTION_RESPONSE.java:27-45 | – | R | S |
| **K-06** | Tests `tests/cm_ak`, `tests/cm_lz`: byte vectors per `readImpl` (every `CM_FIND_GROUP` action incl. the silent 20 and 25, `CM_GROUP_DATA_EXCHANGE` with action 1 and not 1, `CM_RECALLED_BY_OTHER_ANSWER` 0/1/2) and in-process run tests over `InWorldPacketRunSupport.h` (`CM_QUEST_SHARE`'s range filter and its "cannot share" / "no members" answers); `tests/team/P5-10e` for `FindGroupService` (recruitment keyed by team id, `onJoinedTeam` re-posting a leader's recruitment, the full-team removal, the race filter) | – | K-01..K-05 | R | L |

### Stage 1, the dormant arms, recall and the stand-ins (P5-13, P5-01, P5-08, P5-04, P4-11b)

| Id | What | Deps | Need | Eff |
|---|---|---|---|---|
| **W-01** | `PlayerRestrictions::canInviteToGroup`, `canInviteToAlliance`, `canInviteToTeam` (PlayerRestrictions.java:118-205); `canTrade` if A-06 failed; `canChat` (:254-275) | K-03 (`isInAutoInstance`) | R | S |
| **W-02** | `PlayerLifeStats::sendGroupPacketUpdate` (P5-01) → `TeamStatUpdater` | GR-04 | R | S |
| **W-03** | P5-08 lease: `PvpService::doReward`'s team arm (D11); **`ChatBanService` 5** (`banPlayer`, `unbanPlayer`, `registerUnban` + the GAG runnable as a controller task, `isBanned`, `getBanMinutes` — ChatBanService.java:26-75); **`PlayerChatService::isFlooding`** and `logMessage` ×2 (PlayerChatService.java:19-75); unit cases in `tests/playersvc` (a ban, its minutes rounded up, its expiry on a `ManualClock`, the flood count at `flood.msg` + 1) | C-02 | R | M |
| **W-04** | **Delete the five team stand-ins** (`playerTeamDistributionServiceDoReward`, `playerGroupServiceRemovePlayer`, `playerAllianceServiceRemovePlayer`, `teamMoveUpdaterAdd`, `teamStatUpdaterAdd` — `ControllerStandIns.h:55-74`) and call the real API at **all five call sites**: `NpcController.cpp:247`, `PlayerController.cpp:592, 710`, **and the rift accept arm `RVController.cpp:71, 73`** (the vortex bodies stay M5i's, O-04). Header request m5g-10. After it, the stand-in file holds only non-team entries | C-07, GR-02, GR-04 | R | S |
| **W-05** | Tests: `tests/instance` for the `canInviteToTeam` decision table (all 16 checks in their order, both team kinds, the redirect of an alliance invite to a group's leader); `tests/controllers` seam tests that fail with `UnportedException` if a stand-in comes back (the m5b2-1 pattern) | W-01..W-04 | R | M |
| **W-06** | **Recall** (D15): `RecallService::requestSummon`, `validateCast`, `canBeSummoned`, `canRecallAt` (RecallService.java:56-174; P5-08 lease), `RecallInstantEffect::calculate`/`applyEffect` (RecallInstantEffect.java:19-32; P5-04 lease), and **m5b2-p2-9** (`validateCast(Player&, Ptr<VisibleObject>)`) with its caller `Skill.cpp:853-860` (P5-02a one-line lease) **if M5e/M5f did not apply it** (A-12); unit cases in `tests/playersvc`: the 30 s timeout on a `ManualClock` (caster told `STR_MSG_Recall_DONOT_ACCEPT_EFFECT`), the decline (both told), `canRecallAt` against a LIMIT zone without the RECALL flag, a duplicate request | A-12 | R | M |

### Stage 1, the gate harness (P5-SC, `tools/oracle`)

| Id | What | Deps | Need | Eff |
|---|---|---|---|---|
| **H-01** | `tools/oracle` command **`m5g-team`**, everything **parsed from the Java sources and data**, never typed: the characters (race, class, level from a seeded exp) with base HP/MP (`m5a/creation.py`); **the level search of D8** and the team experience for a list of member levels, in-range flags and a damage share against an npc (the m5b-monster arithmetic plus `Math.round(expReward(highest) × lvl / (float) sum)`, the ≥ 10 rule, `(long)(x × damagePercent)`, `XP_GROUP_HUNTING` with the profile's `XP_GROUP_RATES` and the `expNeed × 0.2f` cap) — **the command fails if any share a GP row asserts is at its cap, or if `level/sum` and `1/size` give the same value for A or for C, or if the group and solo rates give the same value**; the `LootGroupRules()` and league defaults, `TeamType` words, `GroupEvent`/`PlayerAllianceEvent` ids, `TeamCommand` codes, `SkillTargetSlot` ids, the three question ids and the system-message ids the gate expects; the quality of every candidate of the drop rules of 210663 and 210133 (whether it rolls under a given rule set) through M5b-3's oracle (A-03); skill 1576's, 3195's (the entry count m5b2 X7 derives), 1809/8998's and 3777's constants through `m5b2-skills`; **the quests**: a kill quest for a Poeta npc (1102 → 210133/210134) and a shareable start-level quest with an XML handler (1105 or another), both from `quest_data.xml` + `quest_script_data`; spots within 20 m of each other near a 210663 spot, D within 20 m of B, one spot > 100 m away, an aggressive npc for GP17b (m5b A5b's 210673), 210133's nearest spot; **for stage 3**: the Nochsana portal (npc, dialog id, spot within talk distance) and the exit point `moveToExitPoint` will use. Plus `tools/oracle` tests | A-03, A-01, A-09 | R | L |
| **H-02** | `GameSession` builders for the 12 client packets of §2.9 (and `CM_QUESTION_RESPONSE` if A-05 did not bring it) and **`TeamDecoders.{h,cpp}`** for the 13 server packets of §2.9, written from the Java `writeImpl` with no `serverpackets/` include and the body consumed exactly (m5a-plan.md D9); `TeamDecodersTest.cpp` | – | R | M |
| **H-03** | **A multi-client choreography helper**: N `ScenarioClient`s with one recorder each, `waitFor(client, predicate, deadline)`, `expectNone(client, predicate, window)` for the negative rows, and a tolerant reader that skips the unsolicited `MOVEMENT` updates of the two updaters and the 6.5 s mantra re-applications (they arrive every 0.5-6.5 s while anyone regenerates, moves or chants); a between-cases hook that writes the database while a character is offline (C19); self-tests | – | R | M |

### Stage 2

| Id | What | Deps | Need | Eff |
|---|---|---|---|---|
| **G-01** | `TEST(M5gScenario, Run)` — §10.2's cases, `<bin>/scenario/m5g`, schema pair `aion_{ls,gs}_test_m5g_<hash>`, the shared `RESOURCE_LOCK`, `tests/scenario/m5g_partial_allowlist.txt`, `gs.scenario.m5g` in `ScenarioTests.cmake` | stage 1, H-01..H-03 | R | L |
| **G-02** | `CheckOutput` (P5-14 lease): `PlayerGroup`, `PlayerGroupMember`, `PlayerAlliance`, `PlayerAllianceGroup`, `PlayerAllianceMember`, `League`, `LeagueMember`, `GroupRecruitment` and the invite handlers in `zeroLiveClasses()` and the summary; **D6(b)**: if M5b-3 left the drop registration map uncleared at shutdown, a `ShutdownHook` step that clears it after every player has left | C-01 | R | S |
| **G-03** | **Re-green** `gs.scenario.m5a`, `m5a_geo`, `m5b`, `m5b_geo`, `m5b2`(`_geo`), `m5b3`(`_geo`), `m5c`, `m5d`, and M5e's and M5f's gates, `gs.smoke.startup`(`_geo`). **Expected movement: none** (finding 3). Record before/after | G-01 | R | M |
| **AL-01** | `PlayerAlliance` 18 (**`getMember` first**, E-12), `PlayerAllianceGroup` 10, **new** `PlayerAllianceMember` (5) | C-01, C-02 | R | M |
| **AL-02** | `PlayerAllianceService` 20 incl. `OfflinePlayerAllianceChecker` (60 s for auto alliances) and **the `disband` breaker** (`cycles.toml:198`: clear `groups` after `AllianceDisbandEvent`); `disband`'s two league calls with D4's suppression and marker | AL-01 | R | M |
| **AL-03** | **The 12 alliance events** (new, 35 bodies), incl. `PlayerAllianceInvite`'s group dissolution and **D4's six event-side suppressions** (`AssignViceCaptainEvent:69`, `ChangeAllianceLeaderEvent:51, 63`, `PlayerAllianceEnteredEvent:46`, `PlayerAllianceLeavedEvent:59`, `PlayerDisconnectedEvent:59`) | AL-02, C-03 | R | L |
| **AL-04** | `SM_ALLIANCE_MEMBER_INFO`'s constructor and `SM_ALLIANCE_INFO`'s `leaguePosition` (P4-16 lease) — **after LG-01 merged** (it reads `LeagueMember`) | AL-01, **LG-01** | R | S |
| **AL-05** | Tests `tests/team/P5-10c`: group → alliance conversion (both sides in groups, one side single, the redirect to a leader), `getOpenAllianceGroup` filling 1000 → 1003, `ChangeMemberGroupEvent` swap and move, vice-captain limits, ready check states, the disband breaker (all four `PlayerAllianceGroup`s reclaimed), a solo login while an alliance exists (E-12) | AL-01..AL-04 | R | L |
| **LG-01** | `League` 21, **new** `LeagueMember` (6) | C-01 | R | M |
| **LG-02** | **New** `LeagueService` (12) — **after AL-02 merged** (it drives `PlayerAlliance`) | LG-01, **AL-02** | R | M |
| **LG-03** | **The 9 league events** (new, 24 bodies); every one records League → PlayerAlliance normally (D4) | LG-02 | R | M |
| **LG-04** | Tests `tests/team/P5-10d`: create/join/move/expel (and the expel by a non-leader, which throws, LeagueService.java:114-122)/leave/disband, reorganize positions, the league loot rules read through the alliance; **the lock-order test** under `failOnReport` (`LockOrderValidator.h:33-34`) on a `DeterministicExecutor`: every league event and each of D4's eight alliance-side sites, run league-first and alliance-first; asserts `getReports()` empty and the class graph holding **League::teamLock → PlayerAlliance::teamLock and no reverse edge**. Mutation-proven: remove any one of the eight suppressions → a `CYCLE` (League ↔ PlayerAlliance; `failureCount` grows) **and** a `SAME_CLASS_NESTING(PlayerAlliance::teamLock)` report (read from `getReports()`, because `failOnReport` counts only `CYCLE`); give League and PlayerAlliance one class → `SAME_CLASS_NESTING` on the first league event. **Runs last in stage 2, after AL-03 merged** (it needs the alliance-side sites) | LG-01..LG-03, **AL-03** | R | M |
| **F-01** | Fix-ups: whatever G-01 finds in stage-1 chunks | G-01 | R | M |

### Stage 3

| Id | What | Deps | Need | Eff |
|---|---|---|---|---|
| **G-04** | `TEST(M5gAllianceScenario, Run)` — §10.5 including the group-instance cases GA12-GA14 (D17), `gs.scenario.m5g_alliance`, its own output directory, schemas and allow-list | stage 2, A-12 | R | L |
| **G-05** | Re-green every gate of G-03 plus `gs.scenario.m5g` | G-04 | R | M |
| **X-01** | `tests/instance` (P5-13 test lease): **`PortalService.port`'s alliance and league arms with real teams** (D17) — an alliance and a league formed in-process through `PlayerAllianceService`/`LeagueService`, `registerTeam(alliance)` / `registerTeam(league)` and `getRegisteredInstance` by the alliance's and the league's object id (PortalService.java:90-98, 172-189), `checkPlayerSize`'s alliance and league refusals; the group arm's `!instanceGroupReq` branch (:136-166) | stage 2, A-12 | R | M |
| **F-02** | Fix-ups from G-04 | G-04 | R | M |
| **S-01** | **user (D13), not on the required path**: the stress extension (`StressRun`, `tests/scenario/stress`) with team churn under ASan; asserts empty census, 0 live team objects at the end, watchdog quiet, lockdep empty; registered DISABLED like `gs.scenario.m5a_stress` (`StressTests.cmake:20-42`) | G-04, the user | O | M |

### Deferred

| Id | What | Milestone |
|---|---|---|
| O-01 | Instance matchmaking: `AutoGroupService` (20 after N-06), `AutoInstance`, `LookingForParty`, `AutoGroupUtility`, `AGPlayer`, the three auto-instance classes with no file, `AutoGroupType`'s companion, `CM_AUTO_GROUP`, `PvPArenaService`, the rest of `PeriodicInstanceManager` and `InstanceScore` — 49 P5-10 sites + 52 undeclared + the P5-13 residue | **M5j** (D16), with the phase-6 handlers of the matched instances |
| O-02 | The legion model (`model/team/legion`, 66 + 7) and the challenges (20) — P5-10f | **M5h** |
| O-03 | A gated mentoring case (needs a member ≥ 10 levels below, Predicates.java:42-44); ported and unit-tested in GR-03/GR-05 | a later gate or the real client |
| O-04 | Vortex team arms: `VortexService` (3; reached from `PlayerAllianceService.removePlayer`/`banPlayer`/the offline checker for defence and offence alliances, and from `canInviteToTeam`'s defence arm) — the rift call sites themselves are closed by W-04 | M5i |
| O-05 | The PvP half of `PvpService::doReward` (team kill counting, AP split) | M5i (m5j-plan.md A-I2) |
| O-06 | A **gated** alliance or league instance entry: the lowest instance with more than 6 members is level 50 (300220000) — X-01 unit-tests the arms | none planned (phase 6 or a high-level session) |
| O-07 | `CM_FIND_GROUP` actions outside the 18 read cases (14, 16, 18, 19, 21-24, > 25 log "Unknown find group action", CM_FIND_GROUP.java:115-117); 20 and 25 are read and ignored by Java too | not in Java |

---

## 6. Lanes

At most six lanes per stage; chunks disjoint within a stage.

| Stage | Lane | Chunks | Items | Tests |
|---|---|---|---|---|
| 0 | integrator | manifest, header batch | I-01..I-05 | – |
| 1 | **team-core** | P5-10a | C-01..C-09 | `tests/team/P5-10a` |
| 1 | **parties** | P5-10b | GR-01..GR-05 | `tests/team/P5-10b` |
| 1 | **loot** | P5-09 (+ P5-07, P5-06 leases) | L-01..L-04 | `tests/economy` |
| 1 | **packets** | P5-15, P5-16, P5-10e | K-01..K-06 | `tests/cm_ak`, `tests/cm_lz`, `tests/team/P5-10e` |
| 1 | **wake-ups** | P5-13, P5-01, P4-11b (+ P5-08, P5-04, P5-02a leases) | W-01..W-06 | `tests/instance`, `tests/stats`, `tests/controllers`, `tests/playersvc` (lease) |
| 1 | **gate-harness** | P5-SC, `tools/oracle` | H-01..H-03 | `tools.oracle`, decoder self-tests |
| 2 | **gate** | P5-SC (+ P5-14 lease) | G-01..G-03 | `gs.scenario.m5g` + every earlier gate |
| 2 | **alliances** | P5-10c (+ P4-16 lease) | AL-01..AL-05 | `tests/team/P5-10c` |
| 2 | **leagues** | P5-10d | LG-01..LG-04 | `tests/team/P5-10d` |
| 2 | **fixups** | whichever stage-1 chunk G-01 names | F-01 | owner tests + gate rerun |
| 3 | **gate-2** | P5-SC | G-04, G-05 | `gs.scenario.m5g_alliance` + all |
| 3 | **instance-arms** | `tests/instance` (P5-13 test lease) | X-01 | `tests/instance` |
| 3 | **fixups** | as named | F-02 | – |
| 3 | stress (user) | P5-SC | S-01 | nightly, DISABLED |

**Merge order in stage 1.** I-02a (day 0-1) → C-01, C-02, C-05 (everything else compiles against their bodies) → C-03 → **GR-01 together with
GR-02** (E-12: `getMember` lands with `createGroup`) → GR-03, GR-04, L-01/L-02, K-03 → W-02, W-04 (the stand-ins go when their targets exist, so
no call site ever points at nothing) → C-07 → K-01, K-02, K-04 → W-01, W-03, W-06. I-02b lands while stage 1 runs. H-01..H-03 have no body
dependency and start on day 1. **The integrator commits stage 1 as one unit** (finding 3: nothing between the parts is reachable by a gate, but a
half-landed party path is what the real client would hit).

**Order in stage 2.** The alliance and league lanes interleave, and the order is explicit: AL-01 and LG-01 start together → AL-02 → **LG-02 after
AL-02** → AL-03 and LG-03 → **AL-04 after LG-01** → AL-05 → **LG-04 last, after AL-03**. Neither lane waits long (LG-02 starts about day 3,
LG-04 about day 6 of a 9.5-day alliance lane).

**Sizing, re-budgeted from the item letters (rev 2; S ≈ 0.5, M ≈ 1.5, L ≈ 3 agent-days).** Rev 1's lane estimates did not add up on its own
scale (§14 finding 3).

| Stage | Lane | Items → agent-days | Total |
|---|---|---|---|
| 0 | integrator | I-01 0.5, **I-02 3**, I-03 0.5, I-04 0.5, I-05 0.5 | **5** |
| 1 | team-core | C-01 1.5, C-02 1.5, C-03 1.5, C-04 0.5, C-05 0.5, C-06 0.5, C-07 1.5, C-08 0.5, C-09 3 | **11.5** |
| 1 | parties | GR-01 0.5, GR-02 1.5, GR-03 3, GR-04 0.5, GR-05 3 | **8.5** |
| 1 | loot | L-01 1.5, L-04 1.5; fallbacks L-02 1.5-3, L-03 0.5 | **3-6.5** |
| 1 | packets | K-01 1.5, K-02 1.5, K-03 1.5, K-04 1.5, K-06 3; fallback K-05 0.5 | **9-9.5** |
| 1 | wake-ups | W-01 0.5, W-02 0.5, W-03 1.5, W-04 0.5, W-05 1.5, W-06 1.5 | **6** |
| 1 | gate-harness | H-01 3, H-02 1.5, H-03 1.5 | **6** |
| | **stage 1** | | **44-48** |
| 2 | gate | G-01 3, G-02 0.5, G-03 1.5 | **5** |
| 2 | alliances | AL-01 1.5, AL-02 1.5, AL-03 3, AL-04 0.5, AL-05 3 | **9.5** |
| 2 | leagues | LG-01 1.5, LG-02 1.5, LG-03 1.5, LG-04 1.5 | **6** |
| 2 | fixups | F-01 1.5 | **1.5** |
| | **stage 2** | | **22** |
| 3 | gate-2, instance-arms, fixups | G-04 3.5 (L, with GA12-GA14), G-05 1.5, X-01 1.5, F-02 1.5 | **8** |
| | **M5g** | | **~79-83 agent-days** |

**The critical path**, one agent per lane and the waves back to back: I-02a (~1.5 d) → **C-01 → C-02 → C-03 → GR-03 → GR-05** (1.5 + 1.5 + 1.5 +
3 + 3 = 10.5 d) with the team-core lane's own **C-07 → C-09** (11.5 d in all) beside it → stage 1 ≈ **13 days**; stage 2 ≈ **9.5-10 days**
(the alliance lane, with the league lane interleaved as above; the gate lane's 5 days fit inside it); stage 3 ≈ **5 days** (G-04 → G-05). **About
28 calendar days** before review and integration time. Two levers shorten it: **(a)** split stage 1 at loot (§9) — it removes nothing from the
critical path but lets a smaller wave land earlier; **(b)** start I-02a's parties and core headers in the last days of M5f, since they touch no
M5f chunk.

---

## 7. Header requests expected

Bodies need no request (hub-headers.md §14); new files and declaration changes go through `docs/porting/header-requests.md`, as M5b-2's new
headers did (rows m5b2-s03-*, "pending (new file)"). New client packet files need no request (the m5f-plan.md:488 precedent).

| Request | Kind | For |
|---|---|---|
| **m5g-1** `model/team/GeneralTeam.h` (S0b objects group; `TemporaryPlayerTeam.h` is a hub): the protected constructor takes `const runtime::LockClass& teamLockClass` and `teamLock` is initialized from it; `TemporaryPlayerTeam`'s constructor passes it through; `PlayerGroup`, `PlayerAlliance`, `PlayerAllianceGroup`, `League` pass `AION_LOCK_CLASS(<Class>::teamLock)` | **signature** (protected constructors of 5 classes, no other caller) | D4 |
| m5g-2 `model/team/TeamEvent.h` (new interface: `handleEvent`, `checkCondition`, a C++ `toString` for the skip warning) | new file | C-02 |
| m5g-3 `model/team/PlayerTeamMember.{h,cpp}` (new; `RefCounted` + `TeamMember`, `retain`/`release` forwarded, hub-headers.md §9.2) | new file | C-02 |
| m5g-4 five enum companions (new; naming after `model/GenderInfo.h`/`PlayerClassInfo.h`) | new files | C-04 |
| m5g-5 `model/team/common/events/*` — 7 classes (new; K5; generic `T` erased with narrowing) | new files | C-03 |
| m5g-6 `model/team/common/service/{PlayerTeamCommandService,PlayerTeamDistributionService}` (new; nested `PlayerTeamRewardStats` in the `.cpp`, hub-headers.md §9.3) | new files | C-06, C-07 |
| m5g-7 `model/team/group/PlayerGroupMember.h` + 11 group events (new; `PlayerGroupInvite` RefCounted with `create()`) | new files | GR-01, GR-03 |
| m5g-8 `model/team/group/PlayerGroupStats.h`: C++-only `void releasePlayers();` (D7) | **additive** | C-01, GR-01 |
| m5g-9 `taskmanager/tasks/{TeamMoveUpdater,TeamStatUpdater}.h` (new; declared in `taskmanager/tasks/fwd.h:12-13` already) | new files | GR-04 |
| m5g-10 `controllers/ControllerStandIns.h` (P4-11b): **delete** the five team declarations and bodies; check whether `controllers/attack/fwd.h` and `model/team/fwd.h` still have a user (m5b2-1 kept `attack/fwd.h` only for `playerTeamDistributionServiceDoReward`) | **signature ×5** | W-04 |
| m5g-11 `model/team/alliance/PlayerAllianceMember.h` + 12 alliance events (new) — `PlayerAllianceMember.h` in I-02a, the events in I-02b | new files | AL-01, AL-03 |
| m5g-12 `model/team/league/{LeagueMember,LeagueService}.h` + 9 league events (new) — `LeagueService.h` in I-02a, the rest in I-02b | new files | LG-01..LG-03 |
| m5g-13 `generated/concurrency/fieldmap.toml` `[kinds]`: K5 for every new event (the three leave events are there, `:59-61`), K3/K4 for the invite handlers and `OfflinePlayer*Checker`; `cycles.toml`: the chat GAG runnable | decision | C-08 |
| m5g-14 `CheckOutput` (P5-14): `zeroLiveClasses()` + summary rows; `ShutdownHook`: D6(b)'s drop-map clear — **only if M5b-3 did not add one** | additive | G-02 |
| m5g-15 **Manifest (D1)**: six parts P5-10a..f sharing `aion_gs_team`, six test directories `tests/team/P5-10a..f`; the names m5h-plan.md A-18 adopts | build | I-01 |
| m5g-16 `configs/main/RuntimeConfig` default of `gameserver.debug.leak_census_minutes` — **only if the user picks it in D6(c)** | default change | D6 |
| **m5b2-p2-9** (approved 2026-09-23) `services/RecallService.h`: `validateCast` takes `Ptr<VisibleObject>` — **apply only if M5e/M5f did not** (A-12) | signature (approved) | W-06 |

---

## 8. Risks

Ordered by what is most likely to go wrong, with the evidence for each.

**Free-threaded porting risks.**

1. **The alliance ↔ league inversion is real, and the majority of the nesting is league → alliance** (§2.11 item 3). D4 suppresses the eight
   alliance-side sites; if a ninth appears (a later fix that broadcasts to the league from another alliance event) lockdep reports it at once,
   which is the point. **LG-04's lock-order test is the only place that proves each suppression is needed and sufficient.** Java can deadlock at
   both windows; S-01 is where the port would show it under load.
2. **Retention that Java's collector hides** (§2.11 items 5-6): an offline member (600 s), a departed member in `PlayerGroupStats` (Java never
   clears it), a disbanded group and its in-range members kept by a looted-but-not-empty corpse (300 s), a pending invite holding its inviter
   (`RequestResponseHandler.requester`, `RequestResponseHandler.h`), a flooding player pinned by the GAG task for 2 minutes. The first shows up in
   every real session's census within 10 minutes unless D6(c) lands; the others at shutdown. **C-09's fabricated-holder test and the gates'
   live-count rows are the only checks.**
3. **About 35 dormant call sites in 23 files wake together** (§2.10), plus M5e's mantra task, several inside `catch` blocks, packet
   serialization or scheduler tasks: E-6 inside `Effect.startEffect`/`endEffect` (an `endEffect` that throws before `stopTasks` is m5b2-plan.md §8
   item 2's leak), E-7 inside `NpcController::onDie`'s catch (the S-1 shape), E-4 inside the life-stats restore task, E-26 on the aura task,
   **E-12 in every login**. **The gate's "no ERROR line" row is what turns a swallowed throw back into a failure**; it must not be relaxed.
4. **Nested events under one reentrant lock** (§2.11 item 1): `GroupDisbandEvent` runs a leave per member inside `forEach` while each leave
   removes from `members`; `PlayerGroupLeavedEvent` → `disband` → `GroupDisbandEvent` → more leaves. A C++ `forEach` that snapshots differently
   from Java's weakly consistent iteration, or an iterator used after a nested call retired its table, changes who is told what. The C++
   `ConcurrentHashMap`'s iterator is stamped with its scope (`ConcurrentHashMap.h:126-127`) and never throws on concurrent removal; GR-05 must
   run disband with 3 and 6 members.
5. **Identity, not ids** (§2.11 item 8): after a relog the team holds two `Player` objects of one id for a moment. A body that "improves"
   `player.equals(member)` into an id comparison changes the connected-event leader test and the "all except" filters.
6. **Unsynchronized Java counters** (item 7) — port as `Field` with `// java-race`, do not add a lock.

**Scope and process risks.**

7. **The 167 invisible bodies and 54 new files** are how this milestone gets estimated wrong — the M5b-2 finding at a larger scale, and rev 1 of
   this plan halved its own letters (§14 finding 3). I-02 must create the header **and an `AION_UNPORTED` `.cpp` shell** for every no-file class
   before a lane links against it; I-02a is on the critical path.
8. **D1 is the milestone's structural hazard.** Without the split, P5-10's 325 in-scope bodies are one lane and the milestone is five or six
   waves. Its names are now shared with M5h (A-18); a rename by either plan breaks the other's lane tables.
9. **Six earlier milestones feed it** (§0), and the sibling plans disagree on owners (§3.1: `CM_CHAT_MESSAGE_PUBLIC` is claimed by M5g, M5i and
   M5j; matchmaking by nobody until D16). A-05 (`CM_QUESTION_RESPONSE`) blocks everything, A-03 (the drop engine) blocks the loot cases, A-12
   blocks recall and the stage-3 instance cases. I-05 is the check, and the fallbacks are sized in §5.
10. **The multi-client gate is the longest and most timing-sensitive yet**: four clients, unsolicited `MOVEMENT` updates every 0.5-2 s, mantra
    re-applications every 6.5 s, a 30 s offline-check period, a 17 s roll timeout, random rolls, four kills at 10⁶ drops that must be looted
    empty, a 4.5 s recall cast, a death. **Budget 6-9 minutes** under the shared `RESOURCE_LOCK` for `gs.scenario.m5g` and ~6 for
    `m5g_alliance` (with the 30 s instance kick), on top of about a dozen earlier scenario runs; H-03's tolerant reader and the order-free
    assertions (D5) are what keep it deterministic.
11. **The real client sends what the gate does not** (E-19): `CM_GROUP_DATA_EXCHANGE` payloads of unknown size, `CM_FIND_GROUP(13)` every 50 s,
    brands without a team, chat commands. **Porting `CM_CHAT_MESSAGE_PUBLIC` opens `ChatProcessor` command execution to every player** — `.`
    and `//` commands reach handler classes that are mostly phase-6 work. Faithful and loud (`unported_trace.txt`), but the checklist warns.
12. **`GeneralTeam::getName()` after the last-leave breaker** (C-01): Java prints the old leader in `[TEAM] skipped event`; the port has cleared
    it. A late event on a disbanded team — B's logout thread reads `player.getPlayerGroup()` just before another thread's disband, then its
    `PlayerDisconnectedEvent` fails `checkCondition` (PlayerDisconnectedEvent.java:29-31) and is logged with the team — would throw inside the
    warning unless C-01 guards the formatting.
13. **Alliance group ids 1000-1003 are shared by every alliance** and the C++ breaker clears `PlayerAlliance.groups` on disband: a packet racing
    a disband (`ALLIANCE_CHANGE_GROUP` with a stale alliance `Ptr`) gets `NullPointerException("No such alliance group")` where Java would still
    find the group. A deviation row, and an AL-05 case.
14. **Capacity**: `TeamStatUpdater` turns every regeneration tick of every member into five `SM_GROUP_MEMBER_INFO` per 500 ms; at alliance size
    (24) that is 23 per member; a mantra adds a re-application per member every 6.5 s. capacity-proposals.md:465-467 proposes the re-run; the
    numbers are the user's.
15. **The stage-3 instance cases stand on M5f's portal, instance and Daeva-seed work** (A-08, A-12); if any of it is late, GA12-GA14 wait and
    m5f D3's promise stays open — the case is small, the dependency is not.

---

## 9. The split: three stages, and why in this order

| Stage | What a player can do at the end | Chunks | Bodies | Lanes |
|---|---|---|---|---|
| **1 — a party** | Invite and accept; see members' HP, MP, position and buff icons; group buffs and mantras; party chat; loot rules, rolls, kinah split; shared experience and quest credit; quest share; summon a member; leave, kick, leader change, disconnect and reconnect; find group. **Ungated**; unit-tested | P5-10a, P5-10b, P5-10e (find group), P5-09, P5-15, P5-16, P5-13, P5-01, P4-11b, P5-08, P5-04, P5-06, P5-02a (leases), P5-SC | **~238** (+3 flood arm, + up to 17 fallbacks) | **6** |
| **2 — the party gate; alliances and leagues written** | the party path proven through four fake clients; every earlier gate green; the alliance and league code unit-tested, D4 proven | P5-SC, P5-10c, P5-10d, P4-16 (lease), P5-14 (lease), fixups | **~153** + the gate | **4** |
| **3 — alliances, leagues and a group instance gated** | Form an alliance from parties, move members between alliance groups, ready check, vice captains, alliance chat and experience; a league of two alliances, its loot rules, move, expel and disband; **a group enters a group instance together and is moved out 30 s after it breaks up** | P5-SC, `tests/instance` (lease), fixups | X-01 + fixups | **3** (+ S-01 if the user wants it) |

Four arguments for this order.

1. **Parties are what a player meets first and most**, and they need none of the alliance code: `PlayerGroup` is a `TemporaryPlayerTeam`
   directly, and `PlayerTeamCommandService`'s alliance arms only need declarations and I-02's shells (D10). A stage that stops at parties has a
   green point.
2. **The party path is the one every dormant site sits on** (§2.10): E-3..E-16 and E-21..E-29 all close in stage 1. Alliances add no new
   dormant site except their own `getMember` (E-12); they add their own services behind the same calls.
3. **The alliance and league lanes can run beside the party gate** because their chunks (P5-10c/d) are disjoint from the gate's (P5-SC) and the
   gate's fixups (stage-1 chunks). If G-01 finds a core defect, AL/LG rebase on the fix-up; that is the only coupling with the gate. Between the
   two of them the coupling is explicit (§6's stage-2 order).
4. **The league is last because it carries the inversion** (D4). Gating it separately keeps a lockdep question from blocking the party gate; the
   instance cases join it because they need M5f's heaviest pieces.

**What this plan does not claim.** It does not claim stage 1 fits one wave comfortably: ~238 bodies over 6 lanes is about M5b-1 stage 1's width
at 1.5 times its size, and its critical path is ~13 days (§6). **If stage 1 must split, split off loot** (L-*, K-02's `CM_GROUP_LOOT`,
GP12-GP15): parties without shared loot are still a party, and `initDropNpc`'s team arm then registers drops the loot lane distributes a wave
later — the corpse FFA fallback (`DropService.scheduleFreeForAll`) keeps the loot reachable meanwhile. Splitting at the events instead leaves a
wave with no green point: a party that cannot be formed asserts nothing.

---

## 10. Gate specification

### 10.1 Processes, databases and profile (both M5g gates)

Identical to m5b2-plan.md §10.1 except:

| Piece | M5g |
|---|---|
| Schemas | `aion_ls_test_m5g_<hash>` / `aion_gs_test_m5g_<hash>` (`_m5ga_` for the alliance gate), same `SchemaLease` and sweep |
| Output directory | `<bin>/scenario/m5g`, `<bin>/scenario/m5g_alliance` |
| `RESOURCE_LOCK` | the shared `"aion_game_server_log;aion_login_server_log"` |
| Profile | `m5g.properties.example` (I-04): the M5d profile + `gameserver.rates.drop = 1000000` (D9) + **`gameserver.rates.xp.group = 1.5, 3.0`** (D8) + `gameserver.playergroup.removetime = 5` + `gameserver.playeralliance.removetime = 5` (D6a); events, autogroup, rift, vortex off |
| Allow-list | `tests/scenario/m5g_partial_allowlist.txt`: the §A rows the earlier milestones leave, **no new row** (D10) |
| Accounts | party gate: four, D8: A Warrior, B Mage, C Priest, D Scout at the oracle's levels; seeds of D8. Alliance gate: the same four + **E, F** (Elyos Daevas, level 25, D17) |
| Spots | from `oracle.py m5g-team` (H-01): A, B, C within 20 m of each other and within 100 m of an oracle-chosen 210663 spot, **D within 20 m of B** (inside 1576's and 1809's ranges); a second spot > 100 m away; 210133's nearest spot; the aggressive npc of GP17b; E and F within talk distance of the Nochsana portal in Eltnen |
| Monsters | 210663 (m5b-plan.md D11), 210133 (kinah and 1102's kill, m5b3-plan.md §2.4, quest_data.xml:900), 210673 (the death, m5b-plan.md A5b) |

### 10.2 Cases of `gs.scenario.m5g` (parties)

| # | Case | Steps |
|---|---|---|
| **C0** | oracle | `oracle.py m5g-team` answers every constant of §10.3 and passes its own acceptance checks (D8, H-01) |
| **C1** | setup | the M5a cases 1-4 for the four accounts, the seeds of D8, enter world at the spots |
| **C2** | decline | A `CM_INVITE_TO_GROUP(0, "B")`; B `CM_QUESTION_RESPONSE(60000, 0)` |
| **C3** | accept | A invites B again; B answers 1 |
| **C4** | third member | A invites C; C answers 1 |
| **C5** | refusals | B invites D (not the leader); **B invites B** (not the leader *and* self); A invites A; A invites B (a member); A invites "Nosuchname" |
| **C6** | chat | A `CM_CHAT_MESSAGE_PUBLIC(GROUP, "m5g party")`; D `CM_CHAT_MESSAGE_PUBLIC(NORMAL, "m5g say")` |
| **C6b** | data exchange | A `CM_GROUP_DATA_EXCHANGE(action 0, groupType 0, unk2 0, 16 bytes)`; then A the same with action 1 |
| **C6c** | flood | D sends `CM_CHAT_MESSAGE_PUBLIC(NORMAL)` 100 ms apart until the server answers `STR_FLOODING` (the 8th: `flood.msg` = 6, `flood.delay` = 1 s, Player.java:1472-1478); then one more |
| **C7** | mantra and member icons | B `CM_CASTSPELL(1809)`; wait 14 s (two aura ticks); B `CM_CASTSPELL(3195)` on itself; wait 6 s; B `CM_TOGGLE_SKILL_DEACTIVATE(1809)` (A-11); wait 7 s until every 8998 has ended |
| **C8** | group buff | B casts 1576 (D stands within 20 m, outside the party) |
| **C9** | stat and move updates | B stands still for 10 s after C8's MP cost while its MP regenerates; then C walks 10 m |
| **C10** | loot rules | A `CM_DISTRIBUTION_SETTINGS(0, FREEFORALL, 0, 0,0,0,0,0,0, 0)` |
| **C11** | brands | A `CM_SHOW_BRAND(0, 1, 210663's id)`; B the same with brand 2 |
| **C12** | shared experience and round robin | A `CM_DISTRIBUTION_SETTINGS(ROUNDROBIN, all 0)`; **A kills 210663 twice with A, B, C within 100 m** (D near and outside the party), looting each corpse empty; C moves beyond 100 m; A kills a third 210663; loot empty |
| **C13** | roll | A sets the default qualities (superior..mythic 2); A kills 210663 (C still > 100 m away); the looter loots the "Potions (Rare)" index; A and B `CM_GROUP_LOOT(mode 2, roll 1)`; then the next roll item (one of the two illusion-godstone entries, LEGEND/UNIQUE, which also fire at 10⁶ — m5b3-plan.md §2.4), which nobody answers for 18 s; both pass the third; loot the rest empty. Each prompt waits for the previous final `SM_GROUP_LOOT` (a second prompt while one is open is refused with `STR_MSG_LOOT_ALREADY_DISTRIBUTING_ITEM`, DropService.java:224-226) |
| **C13b** | summon a member | A `CM_TARGET_SELECT(C)` (C is out of A's sight); A `CM_CASTSPELL(3777)`; C `CM_RECALLED_BY_OTHER_ANSWER(0)` |
| **C14** | kinah on a corpse and quest credit | A kills 210133 with A, B, C within 100 m — **B holds 1102 and never attacks**; the looter loots the kinah entry; loot empty |
| **C15** | kinah split | A `CM_GROUP_DISTRIBUTION(100, 1)` |
| **C15b** | quest share | B `CM_QUEST_SHARE(the oracle's shareable quest)`; then B `CM_QUEST_SHARE(1102)` |
| **C16** | leader change | A `CM_PLAYER_STATUS_INFO(3, B)` |
| **C17** | kick | B `CM_PLAYER_STATUS_INFO(2, C)` |
| **C18** | find group | D `CM_FIND_GROUP(2, D, "m5g lfg", 0)`; C `CM_FIND_GROUP(0)`; B invites D, D accepts; C `CM_FIND_GROUP(0)` again |
| **C19** | disconnect and reconnect | B (the leader) `CM_QUIT(false)`; **while B is offline the harness seeds B's `player_life_stat.hp = 1`** (m5b-plan.md D12, after the logout store); B re-enters within 5 s |
| **C19b** | death and revive | B walks to the oracle's aggressive npc and dies; B `CM_REVIVE(BIND_REVIVE)` |
| **C20** | offline timeout | D `CM_QUIT(false)` and stays out; wait up to 36 s |
| **C21** | leave and disband | B `CM_PLAYER_STATUS_INFO(6, 0)` |
| **C22** | reports and shutdown **with a live group** | A invites C, C accepts; the stop file with A and C online in that group; then the M5a Q8 bar and the M5g rows |

### 10.3 Assertions

Every row states what it proves, what it cannot prove, and the mutation that turns it red. Exact constants come from C0.

| # | Case | Assertion | Proves / cannot prove | What a wrong port does |
|---|---|---|---|---|
| **GP1** | C2 | B receives `SM_QUESTION_WINDOW(60000, …, "A's name")`; A receives `STR_PARTY_INVITED_HIM(B)`; after B's 0, A receives `STR_PARTY_HE_REJECT_INVITATION(B)`; **nobody** receives `SM_GROUP_INFO`; the second invite of C3 produces a new question (the first request was removed) | **Proves** `CM_INVITE_TO_GROUP` → `canInviteToTeam` → `putRequest` → `CM_QUESTION_RESPONSE` → `denyRequest`. **Cannot** prove the question text | a handler not removed from `activeRequests` (no second question); `acceptRequest` on a 0 |
| **GP2** | C3 | A receives, in order: `SM_GROUP_INFO` (groupId ≠ 0, leaderId = A, **the eight loot words = the oracle's `LootGroupRules()` defaults**, type/subType = GROUP's `0x3F`/0), `STR_PARTY_ENTERED_PARTY`, `SM_GROUP_MEMBER_INFO(A, JOIN = 5)` with A's name and the oracle's max HP/MP; later `SM_GROUP_MEMBER_INFO(B, ENTER = 13)` + `STR_PARTY_HE_ENTERED_PARTY(B)`. B receives the same groupId and leaderId A, `JOIN` for itself and `ENTER` for A. Both see `SM_ABYSS_RANK_UPDATE(1)` of the other; D sees them for both | **Proves** `createGroup`, `PlayerGroupEnteredEvent`, `GeneralTeam.addMember`, the leader set in the constructor, `setPlayerGroup`, and that `getTeamId` serializes (E-21). **Cannot** prove the client draws the frame | a group created without the leader's own enter event (A lacks `JOIN`); `ENTER` sent to the joiner about itself |
| **GP3** | C4 | C receives `SM_GROUP_INFO` (the same groupId, leader A), `JOIN` for itself and **exactly one `ENTER` each for A and B, in any order**; A and B each receive `ENTER` for C | **Proves** joining an existing group (`PlayerGroupInvite.acceptRequest`'s first arm). **Cannot** prove Java's order of the two `ENTER`s (D5) | a second group created for C (another groupId) |
| **GP4** | C5 | B's invite of D: B receives `STR_PARTY_ONLY_LEADER_CAN_INVITE`, D **no** question; **B's invite of B: `STR_PARTY_ONLY_LEADER_CAN_INVITE`, not `STR_PARTY_CAN_NOT_INVITE_SELF`**; A receives `STR_PARTY_CAN_NOT_INVITE_SELF`, `STR_PARTY_HE_IS_ALREADY_MEMBER_OF_OUR_PARTY(B)`, and for "Nosuchname" `STR_NO_SUCH_USER("Nosuchname")` | **Proves** three of `canInviteToTeam`'s checks — leader, self, own-team membership — and that the leader check precedes the self check (PlayerRestrictions.java:149, 158), plus the packet's own name lookup (CM_INVITE_TO_GROUP.java:46-49). **Cannot** prove the other thirteen checks — W-05's table does | the leader check dropped (D gets a question); self checked before leader (B's self-invite answered with `CAN_NOT_INVITE_SELF`) |
| **GP5** | C6 | A, B, C each receive one `SM_MESSAGE` with chat type GROUP, sender A, text "m5g party"; D receives none; D's NORMAL message reaches A, B, C; no ERROR line (the ban and flood checks ran on both) | **Proves** `CM_CHAT_MESSAGE_PUBLIC`'s GROUP arm → `getCurrentGroup().sendPackets`, and `canChat` → `isBanned`/`getBanMinutes`/`isFlooding` on the normal path. **Cannot** prove the blocklist filter of NORMAL chat | GROUP broadcast to the known list (D receives it) |
| **GP5b** | C6b | For action 0: B and C each receive exactly one `SM_GROUP_DATA_EXCHANGE` carrying A's 16 bytes, action 0, unk2 0; **A and D receive none**. For action 1: A, B, C and D each receive it (the known-list broadcast including the sender, CM_GROUP_DATA_EXCHANGE.java:61-63) | **Proves** the team fan-out through `getPlayerGroup().getOnlineMembers()` and the sender filter (:65-87). **Cannot** prove the alliance-group variants (stage 3) | the sender included (A receives its own action-0 packet); the fan-out over the known list (D receives it) |
| **GP5c** | C6c | D's first seven messages reach A, B, C; the 8th is answered by `STR_FLOODING` and reaches nobody; the next is answered by `STR_INGAME_BLOCK_IN_NO_CHAT(2)` (the ban's minutes, rounded up, ChatBanService.java:74) and reaches nobody; no ERROR line | **Proves** `isFlooding`, `banPlayer`, `registerUnban` (the GAG task — its cancellation at D's C20 logout is GP23's `Player` row), `isBanned`/`getBanMinutes`. **Cannot** prove the automatic unban 2 minutes later (W-03's `ManualClock` case) | `isFlooding` always false (no `STR_FLOODING`); `isBanned` inverted (the 9th message delivered) |
| **GP6** | C7 | After B's 3195: A and C each receive `SM_GROUP_MEMBER_INFO(B, UPDATE_EFFECTS = 65)` with **slot byte 1 (BUFF)** whose entries are **exactly the oracle's entries for 3195** (skill id, level, the count m5b2-plan.md X7 derives — two — remaining ≤ 5,000 ms) **and no 8998 entry, although B holds 8998 (CHANT) at that moment**; plus ≥ 1 `SM_GROUP_MEMBER_INFO(B, MOVEMENT)`; **B receives no member-info about itself**; 5 ± 1 s later a BUFF-slot `UPDATE_EFFECTS` for B with no 3195 entry | **Proves** `updatePlayerIconsAndGroup` → `updateGroup` + `updateGroupEffects(slot)` → `PlayerGroupUpdateEvent` → `sendPacket(allExcept)`, the slot filter, and the end path. **Cannot** prove the icon on the party frame | `allExcept` dropped; `updateGroupEffects` sending the whole effect list instead of the slot (8998 appears in the BUFF update) |
| **GP6b** | C7 | Within 7 s of B's 1809: A, B and C each receive an `SM_ABNORMAL_STATE` containing **8998** (slot CHANT) with remaining ≤ 6,500 ms, and each receives, for each of the other two, an `UPDATE_EFFECTS` with **slot byte 4 (CHANT)** containing 8998; **D, within 20 m of B but outside the party, never receives an `SM_ABNORMAL_STATE` containing 8998**; no member-info lists 1809 itself (`tslot="NOSHOW"`, EffectController.java:695-697); after the toggle-off every 8998 ends within 7 s and none starts again | **Proves** `AuraEffect`'s team arm (AuraEffect.java:54-62) over `getCurrentGroup().getOnlineMembers()` (C-02, E-26), the self-application of each member, the CHANT-slot group update. **Cannot** prove `distance_z` or `BOOST_MANTRA_RANGE` (m5e's unit tests) | the team arm skipped (A and C lack 8998); `getOnlineMembers` answering the known list (D gets 8998); a slot-blind update (3195 in a CHANT update) |
| **GP7** | C8 | A, B and C each receive an `SM_ABNORMAL_STATE` containing skill 1576 in the `BUFF` slot with a remaining time within [14,000, 15,000] ms; **D's `SM_ABNORMAL_STATE` never contains 1576** although D stands within 20 m; each member receives an `UPDATE_EFFECTS` for each of the other two, containing 1576 | **Proves** a real group reaching the PARTY target arm (`TargetRangeProperty` over `getCurrentGroup()`), one effect per member, and the icon broadcast for effects cast by someone else. **Cannot** prove the `effective_range` border (a unit test) | PARTY resolved through the alliance only (A and C miss it); a PARTY arm that selects from the known list instead of the team (D gets it) |
| **GP8** | C9 | **While B stands still after 1576's cost**, B's MP regenerates: for each `SM_STATUPDATE_MP` B receives, A receives within 1.0 s an `SM_GROUP_MEMBER_INFO(B, MOVEMENT)` carrying the same current MP — these can only come from `TeamStatUpdater`, because the effect-driven `MOVEMENT` of C8 fired once, at the cast, and C7's mantra is off; after C's walk, A and B receive within 2.5 s a `MOVEMENT` for C within 0.5 m of C's last `CM_MOVE` position | **Proves** `PlayerLifeStats::sendGroupPacketUpdate` → `TeamStatUpdater` (500 ms) and `onMove` → `TeamMoveUpdater` (2,000 ms), both stand-ins gone. **Cannot** prove the periods exactly | the updater never scheduled (no MP-following packets); the stand-in left in place (GP22) |
| **GP9** | C10 | A, B, C receive `SM_GROUP_INFO` with loot rule 0 and seven zero words | **Proves** `CM_DISTRIBUTION_SETTINGS` → `ChangeGroupLootRulesEvent`. **Cannot** prove the pet message (no pets) | rules applied to the leader's view only |
| **GP10** | C11 | A, B, C receive `SM_SHOW_BRAND(1, 210663)`; B's attempt sends nothing to anyone | **Proves** `updateBrand` and the leader-only rule. **Cannot** prove `sendBrands` on relog here — GP17 does | the leader check dropped |
| **GP11** | C12 | **Kills 1 and 2** (A, B, C within 100 m): each of A, B, C receives exactly the oracle's experience (`STR_GET_EXP` / `SM_STATUPDATE_EXP` delta) for the seeded levels under `XP_GROUP_RATES` = 1.5 — **none at its member's cap** (C0 guarantees it) — C's larger than A's; D receives none. `SM_LOOT_STATUS(LOOT_ENABLE)` goes to **exactly one** member per kill, and **the two recipients differ** (the member set is unchanged between the kills, so the C++ order is too — D5). **Kill 3** (C beyond 100 m): A and B receive the oracle's value for (A, B) alone; C none | **Proves** the stand-in gone, `PlayerTeamRewardStats` (range, level sum), `XP_GROUP_HUNTING` **with the group rate**, the round robin. **Cannot** prove the ≥ 10-level rule, mentors or partial damage shares (C-09 does) | `1 / size` instead of `level / sum` (A's and C's values off the oracle's); the range check dropped (C gets exp on kill 3); `XP_HUNTING` instead of `XP_GROUP_HUNTING` (the solo rate); `nrRoundRobin` not advanced **or always reset to 1** (the same recipient twice) |
| **GP11b** | C14 | **B, who never attacks, receives `SM_QUEST_ACTION` for 1102 with its kill counter + 1** within 1 s of A's kill of 210133 (quest_data.xml:900) | **Proves** `QuestEngine.onKill` for every counted member (PlayerTeamDistributionService.java:122). **Cannot** prove `onAddAggroList` | the quest call made for the killer only |
| **GP12** | C12 | For every item the looter takes, the other in-range members receive `STR_MSG_GET_ITEM_PARTYNOTICE(looter, item)`; each corpse ends with `SM_DELETE` within 1 s of the last loot | **Proves** `winningNormalActions` and that team loot empties a corpse. **Cannot** prove misc distribution (misc = 0) | the notice sent to the looter too |
| **GP13** | C13 | On the RARE index: A and B receive `SM_GROUP_LOOT(playerId 0, distributionId 2, item)`; after both rolls each receives `STR_MSG_DICE_RESULT_ME(luck, max)` and the other's `_OTHER`; then `SM_GROUP_LOOT(winner, 0xFFFFFFFF)` with **winner = the strictly larger luck, A on a tie** (A rolled first); the winner's inventory gains the item (`SM_INVENTORY_ADD_ITEM`/`UPDATE_ITEM`, A-03) and `STR_MSG_LOOT_GET_ITEM_ME`; C (out of range) receives nothing | **Proves** `canDistribute` → `setPlayersInRoll` → `CM_GROUP_LOOT` → `handleRoll` → `distributeLoot` → `requestDropItem` → `winningRollActions`. **Cannot** prove bids (a unit test) | `>=` for `>`; the lowest roll winning; C prompted |
| **GP13b** | C13b | C receives `SM_RECALLED_BY_OTHER` (open, A's name, 3777, 30 s — SM_RECALLED_BY_OTHER.java:33-45) after A's 4.5 s cast; after C's answer 0, C's next position (M5f's `TravelDecoders`) is within 1 m of A's position at the end of the cast, A receives an `SM_PLAYER_INFO` for C, A's Dimensional Fragment count falls by 1; nobody receives an `STR_MSG_Recall_*` refusal; no ERROR line | **Proves** `CM_TARGET_SELECT`'s team arm for a member out of sight (CM_TARGET_SELECT.java:59-61), `TARGET_MYPARTY_NONVISIBLE`, `Skill.isInvalidRecall` → `validateCast` → `canRecallAt`/`canBeSummoned`, `RecallInstantEffect` → `requestSummon`, `CM_RECALLED_BY_OTHER_ANSWER` → `accept` → `TeleportService.teleportTo` (D15). **Cannot** prove the 30 s timeout or the decline (W-06's unit cases) | `accept` not teleporting (C stays > 100 m away); `requestSummon` capturing the summoned player's position instead of the caster's |
| **GP14** | C13 | On the unanswered RARE index, after 17.0-18.5 s: each of A and B receives `STR_MSG_DICE_GIVEUP_ME`, then `STR_MSG_PAY_ALL_GIVEUP`; the entry becomes lootable by either | **Proves** the scheduled automatic pass. **Cannot** prove the 32 s bid timeout | `setPlayersInRoll` never scheduled (the prompt hangs) |
| **GP15** | C14 | The kinah entry's count K (from `SM_LOOT_ITEMLIST`) is split over the in-range members: the kinah deltas sum to **exactly K**, one member gets `K − (n−1)·⌊K/n⌋` and the others `⌊K/n⌋` | **Proves** `distributeEqually`. **Cannot** prove which member gets the remainder (D5) | the remainder lost |
| **GP15b** | C15b | A and C (within 100 m) each receive `SM_QUEST_ACTION(questId, B's id, false)` — the accept offer (CM_QUEST_SHARE.java:78); B receives `SM_SYSTEM_MESSAGE(1100002, name)` once for A and once for C; **B receives no offer**; D (not a member) receives nothing; B's share of 1102 is answered by `SM_SYSTEM_MESSAGE(1100001)` (`cannot_share`, quest_data.xml:898) and nothing else | **Proves** `CM_QUEST_SHARE`'s team arm (`getCurrentGroup().filterMembers(allExcept ∧ ONLINE ∧ in range)`) and `checkStartConditions` per member. **Cannot** prove the range filter (every member is in range here — K-06's run test) | the sharer included (B gets an offer); `cannot_share` ignored (1102 offered) |
| **GP16** | C15 | A's kinah falls by 100 and rises by 33; B and C rise by 33 each; `STR_MSG_SPLIT_ME_TO_B(100, 3, 33)` to A, `STR_MSG_SPLIT_B_TO_ME(A, 100, 3, 33)` to B and C — **1 kinah vanishes, as in Java** | **Proves** `TeamKinahDistributionEvent` and `canTrade`. **Cannot** prove the alliance-group variant (stage 3) | the giver excluded; `amount / (n − 1)` |
| **GP17** | C16, C19 | C16: `SM_GROUP_INFO` with leaderId B to A, B, C; `STR_PARTY_YOU_BECOME_NEW_LEADER` to B, `STR_PARTY_HE_IS_NEW_LEADER(B)` to A and C. C19: A and D receive `STR_PARTY_HE_BECOME_OFFLINE(B)`, `SM_GROUP_MEMBER_INFO(B, DISCONNECTED = 3)` and an `SM_GROUP_INFO` whose leader is **A or D** (an online member); on B's return, B receives `SM_GROUP_INFO` (that leader), `JOIN` for itself, `ENTER` for A and D, and `SM_SHOW_BRAND` of the brands still set (`sendBrands`); A and D receive `ENTER` for B | **Proves** leadership passing on disconnect, `PlayerConnectedEvent`'s member replacement through `onPlayerLogin`'s registry walk (E-12), `CM_LEVEL_READY`'s delayed `sendBrands`. **Cannot** prove which member Java would pick (D5) | the disconnected leader keeping leadership; the old member not replaced (B's return shows no `JOIN`) |
| **GP17b** | C19b | When B dies to the npc: **A and D each receive `STR_MSG_COMBAT_FRIENDLY_DEATH(B)` exactly once; B receives `STR_MSG_COMBAT_MY_DEATH` and no `FRIENDLY_DEATH`**; C (kicked at C17) receives neither. After B's `CM_REVIVE(BIND_REVIVE)`: A and D receive, within 0.6 s of B's revive packets, an `SM_GROUP_MEMBER_INFO(B, MOVEMENT)` with B's HP > 0; no ERROR line | **Proves** `PvpService::doReward`'s team arm (D11, E-9) and the revive path of a grouped player (E-10). **Cannot** separate `PlayerReviveService`'s own `updateGroup(MOVEMENT)` (PlayerReviveService.java:206-208) from `TeamStatUpdater`'s, which follows the revive's HP change within 500 ms — leaving the revive's call **unported** fails GP22; *dropping* it survives | the team arm dropped (A and D get nothing); `allExcept(victim)` dropped (B gets `FRIENDLY_DEATH`) |
| **GP18** | C17 | C receives `SM_LEAVE_GROUP_MEMBER` (the five constants of SM_LEAVE_GROUP_MEMBER.java:14-18) and `STR_PARTY_YOU_ARE_BANISHED`; A and B receive `SM_GROUP_MEMBER_INFO(C, LEAVE = 0)` and `STR_PARTY_HE_IS_BANISHED(C)`; C's next `CM_CHAT_MESSAGE_PUBLIC(GROUP)` reaches nobody | **Proves** `banPlayer`, the leave event, `setPlayerGroup(null)`. **Cannot** prove the instance kick (GA14) | the banished player still in `members` |
| **GP19** | C18 | D receives `STR_PARTY_MATCH_OFFER_PARTY_POSTED` and an `SM_FIND_GROUP(0)` list with D's entry; C's list contains it; after D joins, C receives the removal `SM_FIND_GROUP` for D's id and C's next list lacks it | **Proves** `CM_FIND_GROUP` and `onJoinedTeam`. **Cannot** prove the instance-group tab | `onJoinedTeam` a no-op |
| **GP20** | C20 | Within 36 s of D's quit, A and B receive `STR_PARTY_HE_BECOME_OFFLINE_TIMEOUT(D)` and `SM_GROUP_MEMBER_INFO(D, LEAVE)`; the group survives (3 → 2) | **Proves** `OfflinePlayerChecker` scheduled on the first group and `LEAVE_TIMEOUT`. **Cannot** prove the 600 s production value | the checker never started |
| **GP21** | C21 | B receives `SM_LEAVE_GROUP_MEMBER`; A receives `LEAVE` for B, `STR_PARTY_HE_LEAVE_PARTY(B)`, then `STR_PARTY_IS_DISPERSED` and `SM_LEAVE_GROUP_MEMBER` | **Proves** self-leave through `findMember(…, 0)` and `disband` of a group of one. **Cannot** prove the auto-group exception | a group of one left alive |
| **GP22** | C22 | The M5a Q8 bar: `unported_trace.txt` **empty**, `census.txt` **empty** (D6), lockdep **empty**, no watchdog dump, **no ERROR line** in either log, `partial_trace.txt` ⊆ the allow-list with §A hit ≥ 1 and §B hit 0 | **Proves** nothing on the scripted path fell into an unported body or a swallowing `catch` (§2.10 E-6/E-7/E-26). **Cannot** prove paths off the script | any stand-in left; any dormant site of §2.10 left open |
| **GP23** | C22 | `live_counts.txt`: `PlayerGroup` **live 0 with created ≥ 2** — **C22's group was alive at the stop and disbanded by its last member's shutdown logout** (§2.11 item 6) —, `PlayerGroupMember` live 0, `PlayerGroupInvite` live 0, `GroupRecruitment` live 0, `DropNpc` live 0, `Effect` live 0; `Player` live = online count at each report | **Proves** every cut of §2.11 item 4 ran — including C20's timeout, which must have freed D's `Player` (and D's GAG task, GP5c) — and D6(b)'s premise. **Cannot** prove the last-leave breaker against an external holder (C-09 does) | a `removeMember` that forgets `setPlayerGroup(null)`; a group kept by its recruitment; a disconnect that does not disband at the last online member |

### 10.4 Mutation proof (the minimum set)

| Mutation | Must fail | Must stay green |
|---|---|---|
| `PlayerTeamDistributionService`: `member.getLevel() / partyLvlSum` → `1 / players.size()` | **GP11** (A's and C's exact values differ from the oracle's; C0 guarantees they differ and that none is capped) | GP2-GP10 |
| drop the 100 m range check in `PlayerTeamRewardStats.accept` | **GP11** (C gets kill-3 exp) | GP13 |
| `Rates.XP_GROUP_HUNTING` → `XP_HUNTING` in `doReward` | **GP11** (the profile's group rate 1.5 ≠ the solo 1.0) | GP13 |
| drop the `>= 10` level rule | **nothing in the gate** (no 10-level gap) — **C-09** | everything |
| `PlayerGroupUpdateEvent`: `sendPackets` instead of `sendPacket(allExcept)` | **GP6** | GP7 |
| `updateGroupEffects`: the whole effect list instead of the slot | **GP6** (8998 in the BUFF update) | GP7 |
| `AuraEffect`: the solo arm for a grouped player | **GP6b** (A and C lack 8998) | GP7 |
| `canInviteToTeam`: remove the leader check | **GP4** | GP1-GP3 |
| `canInviteToTeam`: self check before leader check | **GP4** (B's self-invite) | GP1-GP3 |
| `CM_GROUP_DATA_EXCHANGE`: send to the sender too | **GP5b** | GP5 |
| `PlayerChatService::isFlooding`: always false | **GP5c** | GP5 |
| `initDropNpc`: never advance `nrRoundRobin`, or always reset it to 1 | **GP11** (the same recipient on kills 1-2) | GP13 |
| `distributeLoot`: `>=` instead of `>` | **GP13** only when the two rolls tie (p = 1/100) — **L-04** is the reliable check | – |
| `LootGroupRules::setPlayersInRoll`: no task | **GP14** | GP13 |
| `distributeEqually`: every member gets `⌊K/n⌋` | **GP15** unless n divides K — **L-04** | – |
| `RecallService::accept`: no teleport | **GP13b** | GP14 |
| `CM_QUEST_SHARE`: `allExcept(player)` dropped | **GP15b** | GP16 |
| `TeamKinahDistributionEvent`: skip the giver | **GP16** | GP15 |
| `PlayerDisconnectedEvent`: no `ChangeGroupLeaderEvent` | **GP17** | GP18 |
| `PlayerConnectedEvent`: skip `removeMember`/`addMember` | **GP17** (no `JOIN` for B) | GP20 |
| `PvpService::doReward`: team arm dropped | **GP17b** | GP17 |
| `initializeOfflineCheck`: no-op | **GP20** | GP21 |
| `FindGroupService::onJoinedTeam`: no-op | **GP19** | GP3 |
| keep `standins::teamMoveUpdaterAdd` | **GP8** and **GP22** | – |
| `PlayerGroup::onRemoveMember`: forget `setPlayerGroup(null)` | **GP23** (and GP18's chat row) | GP21 |
| `PlayerDisconnectedEvent`: never disband at the last online member | **GP23** (C22's group live at shutdown) | GP17 |
| `GeneralTeam` last-leave breaker removed | **nothing in the gate** — **C-09**'s fabricated holder | GP23 |
| a `Ref` kept deliberately in a static | **GP23** | GP22 |

### 10.5 `gs.scenario.m5g_alliance` (stage 3)

Same processes and profile; the four party accounts at oracle-chosen levels (a fresh run) plus E and F (D17). Alliance 1 grows to A, B, C, D
(GA1-GA6); C and D then split off into alliance 2, and the two form a league (GA7-GA9); E and F take a group into Nochsana (GA12-GA14).

| # | Case | Assertion (what it proves / cannot / which mutation) |
|---|---|---|
| **GA1** | A and B form a group; A invites C (solo) to an alliance (`CM_INVITE_TO_GROUP(12)`, `SM_QUESTION_WINDOW(70000)`) | A and B receive `SM_LEAVE_GROUP_MEMBER` (the group dissolved), then all three `SM_ALLIANCE_INFO` (group count 4, leader A, four zero vice slots, the oracle's loot words, the four group ids 1000-1003) and `SM_ALLIANCE_MEMBER_INFO` with alliance group id **1000** for each. Proves `PlayerAllianceInvite`'s dissolution and `getOpenAllianceGroup`. Cannot prove a full alliance group. Kills: members added before the group is dissolved (both `SM_GROUP_INFO` and `SM_ALLIANCE_INFO` live at once) |
| **GA2** | `CM_PLAYER_STATUS_INFO(27, B, 1001, 0)` | every member receives `SM_ALLIANCE_MEMBER_INFO(B, MEMBER_GROUP_CHANGE)` with 1001. Proves `ChangeMemberGroupEvent`'s move arm. Kills: the member moved in the alliance map but not its group |
| **GA3** | `(25, C)`, then C invites D | `SM_ALLIANCE_INFO` with C in the first vice slot; C's invite reaches D (`isViceCaptain` in `canInviteToTeam`). Kills: vice captains refused |
| **GA4** | `(21)` start, B `(23)`, C `(24)` | the `SM_ALLIANCE_READY_CHECK` sequence of CheckAllianceReadyEvent.java:44-66 to every member. Cannot prove the auto-cancel timer |
| **GA5** | B casts 3195; A `CM_CHAT_MESSAGE_PUBLIC(ALLIANCE)`; A `CM_GROUP_DATA_EXCHANGE(action 0, groupType 1)` | `SM_ALLIANCE_MEMBER_INFO(B, UPDATE_EFFECTS)` to the others, never to B; the message to all alliance members only; the data to A's alliance group's other online members only |
| **GA6** | A kills 210663 | each in-range member gets the oracle's alliance share (the same `doReward` over the alliance), **none at its cap** (C0) |
| **GA7** | C and D leave; C forms alliance 2 with D; A invites C to a league (`CM_INVITE_TO_GROUP(28)`, `SM_QUESTION_WINDOW(902249)`) | all four receive `SM_ALLIANCE_INFO` with a league block of two alliances, positions 0 and 1, and the league loot rules **FREEFORALL, 0, 0, 2, 2, 2, 2, 2** (LeagueService.java:85). Proves `createLeague`, `LeagueJoinEvent` — **the league → alliance order D4 records** —, the alliance reading the league's rules |
| **GA8** | `(31)` move, then A changes his alliance leader to B (`(17, B)`) | the positions swap; the leader change broadcasts `STR_UNION_CHANGE_LEADER_TIMEOUT` to the other alliance — **two of D4's suppressed alliance → league sites** (ChangeAllianceLeaderEvent.java:51, 63) |
| **GA9** | B (now alliance 1's leader, whose alliance leads the league) `(30, alliance 2)` | alliance 2 receives `LEAGUE_EXPELLED`; a league of one is disbanded (`LEAGUE_DISPERSED`) — the league → alliance order again, and a nested `LeagueDisbandEvent`. **The gate sends the expel only from the league leader**: `expelAlliance` throws `IllegalArgumentException` for anyone else (LeagueService.java:114-122), which would be an ERROR line (GA11) — Java behaves the same |
| **GA10** | B quits and stays out; wait up to 36 s; then C `(14)` leaves alliance 2 | A receives `STR_PARTY_ALLIANCE_HE_LEAVED_PARTY_OFFLINE_TIMEOUT(B)` and alliance 1, now of one, disbands; C's leave disbands alliance 2 |
| **GA12** | E (solo) opens the Nochsana portal (the oracle's `CM_DIALOG_SELECT`) | E receives `STR_MSG_ENTER_ONLY_PARTY_DON` (`checkPlayerSize`, PortalService.java:72, 260-282) and no teleport. Kills: the group requirement skipped |
| **GA13** | E invites F; E opens the portal; then F | E lands in a new instance I of 300030000 (M5f's decoders); **F lands in the same instance I** — `getRegisteredInstance(300030000, groupId)` (PortalService.java:84-89, 134-169; `registerTeam(group)`, `WorldMapInstance.cpp:188-194`). Proves m5f D3's group arm. Kills: `registerTeam` skipped (F gets a second instance) |
| **GA14** | F leaves the group inside I (`CM_PLAYER_STATUS_INFO(6, 0)`) | the group of one disbands; **E and F each receive `STR_MSG_LEAVE_INSTANCE_NOT_PARTY`** (PlayerLeavedEvent.java:59-60) and, 30 ± 1.5 s later, are moved to the oracle's exit point (`moveToExitPoint`); M5f's checker destroys I (its registered team disbanded). Proves the K5 `INSTANCE_KICK` capture (E-20). Kills: the kick task never scheduled or capturing the event (they stay inside) |
| **GA11** | reports | the Q8 bar with **lockdep empty** — D4's eight suppressions in place: GA7/GA9 record League → PlayerAlliance, GA8 records nothing —, census empty; `PlayerAlliance`, **`PlayerAllianceGroup`** (the `disband` breaker, `cycles.toml:198`), `PlayerAllianceMember`, `League`, `LeagueMember`, `PlayerGroup` live 0 with created > 0; `WorldMapInstance` back to its count before GA12. Kills: any one alliance-side suppression removed (GA8 closes a `CYCLE` with GA7's edge and nests `PlayerAlliance` under `PlayerAlliance`) |

### 10.6 Geo

D12: no geo re-run. At most one case (GP7g) in a `gs.scenario.m5g_geo`, and only if M5b-2's geo gate left a line-of-sight fixture to stand on.

### 10.7 CTest wiring

`ScenarioTests.cmake` (P5-SC): `gs.scenario.m5g` and `gs.scenario.m5g_alliance`, `LABELS "scenario;realdata"`, the shared `RESOURCE_LOCK`,
`TIMEOUT 1800` each.

---

## 11. Real-client checklist (user)

**Prerequisites.** Steps 1-6 of m5b-plan.md §10 with `mygs.properties` from `m5g.properties.example` **without** the gate's two
`removetime = 5` keys and without `rates.drop = 1000000` (or keep them for a quicker session). **You need a second account**: the login
server creates one when you log in with a new name and password (`loginserver.accounts.autocreate = true`, `login-server/src/.../Config.cpp:35`).
**You need two clients logged in at the same time**, both Elyos or both Asmodian; **steps 2's third-client check and step 12 need a third** (skip
them with two). Two instances of the 4.8 client on one PC may need your launcher's multi-instance option (the stock client can refuse a second
instance — *inferred, not measured*); a second PC or VM pointed at the same server works too. The server allows it
(`gameserver.security.multi_clienting.restriction_mode = NONE`, security.properties:110). **Do not type `.` or `//` chat commands** (§8 item 11).

**After stage 2 (parties):**

1. Stand both characters together. From A, invite B (right-click → Invite, or `/invite`). B sees the invitation window; decline once, then
   accept. **Both see a party frame with the other's HP and MP.**
2. Party chat from both sides (the party channel); the other character sees it; a third client, if any, does not.
3. Let a monster hit B: **A's party frame shows B's HP falling** within about half a second. Walk B away: A's map marker for B follows.
4. B uses a buff: **A sees the buff icon on B's party frame**, and it disappears when it runs out. If one of you plays a Chanter, turn a mantra
   on: the other gets its effect while within about 25 m.
5. Kill monsters together: **both get experience**, less each than solo; walk one more than 100 m away and kill again — the far one gets none.
6. Open the loot settings (leader): switch between Free-for-all, Round robin and Leader; kill and see who may loot. With the default settings a
   green (Rare) item opens **the roll window for both**; roll on one and pass on the other; the item goes to the roller. Let one window time out.
7. Split kinah from the party menu.
8. Mark a target (leader): both see the mark.
9. Change the leader, kick and re-invite, leave.
10. **Log B out while in the party** and back in within a minute: B returns to the same party. Log B out for longer than 10 minutes: B is
    removed, and **note whether `game-server/log` shows a leak-census warning for B's character** (D6).
11. Open the Find Group window, post a recruitment, see it from the other client, and see it disappear when that player joins your party.
12. **Third client only**: tell us who became leader when the leader logged out with three members — the order may differ from retail (D5).
13. Share a quest you hold from the quest window: the other character gets the offer. Let one character die to a monster: the other sees
    "… has died" in the chat.

**After stage 3 (alliances; leagues need four clients):**

14. From a party, invite a third character to an **alliance**: the party turns into an alliance. Move a member between alliance groups (drag in
    the alliance window), run a ready check, promote a vice captain.
15. **Only if you can run four clients**: two alliances of two, invite the other alliance leader to a league; move, expel (from the league
    leader only — anyone else's expel logs an error, as in Java), disband. Otherwise the gate's GA7-GA9 are the evidence.
16. Send `game-server/log/`, `m5a_summary.txt`, `live_counts.txt`, `partial_trace.txt`, `unported_trace.txt` and `census.txt`. The ids in
    `unported_trace.txt` are the next milestone's list, exactly as F-2 was for wave B.

Group instances (level 25+), summon group member (needs the skill and a Dimensional Fragment) and mentoring (a 10-level gap) are gate-only
unless you have such characters.

---

## 12. What was measured and what was inferred

**Measured** (grep or a parse over the two trees at HEAD `760e8ab5c` plus the working tree; re-runnable):

- P5-10's **294** sites, per file, and their areas: team scope **158** (core 38, parties 32, alliances 48, leagues 21, find group 19), legion
  model 66, matchmaking 50, challenges 20; **0** `AION_PARTIAL` in P5-10 — re-counted at `760e8ab5c`.
- P5-10's Java side: 91 files, 642 method bodies, 7,263 Java LOC; **52 classes with no C++ file** (49 in M5g scope, 3 matchmaking), **167
  undeclared bodies in scope** (56 constructors, 10 enum-companion methods), 52 in matchmaking, 7 in the legion model; the per-area Java LOC.
- The 147 client-packet files with no C++ `.cpp` and the 16 of §2.9; the server packets of §1 with 0 `AION_UNPORTED` and the two exceptions.
- The ~35 dormant call sites in 23 files of §2.10 (file:line, from a grep of every `isInTeam`/`isInGroup`/`isInAlliance`/`getCurrentTeam`/
  `getCurrentGroup`/`getCurrentTeamId`/`isInSameTeam` call outside `model/team`), the five team stand-ins and their five call sites, `PvpService.cpp:75`,
  `PlayerLifeStats.cpp:70`, `AutoGroupService.cpp:193-194`, `FindGroupService.cpp:92-94`, `PlayerRestrictions.cpp:143-154, 192, 196`,
  `ChatBanService.cpp:7-26`, `PlayerChatService.cpp:11-27`, `RecallService.cpp:41-42, 87-96`, `RecallInstantEffect.cpp:7-13`,
  `TemporaryPlayerTeam.cpp:36-37`, `PlayerGroup.cpp:43-46`, `PlayerAlliance.cpp:82-84`, `QuestService.cpp:321-324, 388-395`; `DropGroup.cpp:51-62`
  ported.
- **The Java lock nesting** (read, with lines, §2.11 item 3): nine league events take alliance locks under the league's; eight alliance-side
  sites take the league's lock under their alliance's, all eight reaching other alliances' locks; `disband`'s three callers all hold the alliance
  lock. The validator checks same-class nesting against every held lock (`LockOrderValidator.cpp:188-198`); the scenario gates assert
  `lockdep.txt` empty with no allow-list (`M5aScenarioTest.cpp:2073-2074`, `M5bScenarioTest.cpp:2145-2146`).
- The lock class (`GeneralTeam.h:37`), the lockdep report kinds (`LockOrderValidator.h:18-33`), the census minutes (`RuntimeConfig.cpp:14`),
  the shutdown census (`RuntimeLifecycle.cpp:113-121`), the cycles and fieldmap rows cited in §2.11, `DropNpc.inRangePlayers` (`DropNpc.h:29`).
- The logout order (connection null at Java :65 / C++ :87 before `onPlayerLogout` at :108 / :131) and the last-online-member disband
  (PlayerDisconnectedEvent.java:33-34).
- **6 party-targeting skills at level ≤ 20 and none at ≤ 10** (skill_tree.xml × skill_templates.xml); skill 1576's and 3195's templates;
  **1809** (TOGGLE, NOSHOW, `aura` 8998, distance 25, distance_z 10) and **8998** (CHANT, statup SPEED, 6,500 ms); **37** `aura` templates;
  **3777** (TARGET_MYPARTY_NONVISIBLE, MYPARTY, `recallinstant`, `itemuse` 169300011 × 1, 4,500 ms; not in skill_tree.xml); `healcastoronatk` 9
  templates, `healcastorontargetdead` 1 (19573, no `healparty`).
- `SkillTargetSlot` ids (SkillTargetSlot.java:12-19) and the slot filter of UPDATE_EFFECTS (EffectController.java:695-697).
- **Quests**: no quest targets 210663 (it appears only in `data/handlers/consolecommands/data/npcs.xml`); 1102's `quest_kill` is 210133/210134
  and it is `cannot_share`; 1105 is shareable, minlevel 1, an XML `item_collecting` quest (poeta.xml:115); 20 more shareable Poeta quests.
- **Experience caps**: `expNeed × 0.2f` = 80 / 206 / 477 / 1,046 / 1,720 at levels 1-5 (player_experience_table.xml; PlayerCommonData.java:109-115;
  Rates.java:21-27); `XP_SOLO_RATES` and `XP_GROUP_RATES` both default to "1.0, 2.0" (RatesConfig.java:30-34).
- **Drop data**: 104 global rules with `member_limit > 1`; 107 `custom_drop` entries with `each_member`.
- **Group instances**: the lowest is Nochsana Training Camp (300030000, level 25, max 6; `portal_dialog` npcs 700413 Reshanta, 800507 Eltnen at
  327.1/2732.1/263.5, 800510 Morheim); Fire Temple 27; the lowest instance with more than 6 members is level 50 (Abyssal Splinter 300220000),
  test maps excluded.
- `CM_FIND_GROUP`'s 18 read cases and 16 run arms; the flood constants (`SecurityConfig.cpp:25-26`, Player.java:1472-1478).
- No `GeoService`/`canSee`/`isVisible` in any P5-10 Java file; the PARTY skill arm's `checkGeo` (TargetRangeProperty.java:76, 155-167).
- A new `Player` object per enter world (PlayerService.java:106); no `equals` override on the game objects.
- `GroupConfig` defaults (group.properties), the offline checker's 1 s / 30 s, the updaters' 500 ms / 2,000 ms, the roll timeouts 17 s / 32 s.
- Every hand-over of §3.1, read in the sibling plans at the cited lines.

**Inferred, and a lane should confirm before relying on it:**

- **That the earlier gates will not move** (finding 3): every site is behind a team guard or an empty registry; nothing was run.
- **That lockdep reports exactly what §2.11 item 3 predicts**, and nothing else, once D4 is in (the validator's rules were read, not exercised;
  LG-04 and the first G-04 run answer it).
- **That the C++ team iteration order differs from Java's** (D5): from the stripe design; nobody compared orders.
- **D8's working levels** (A = B = 3 or 4, C two higher) and that a group rate of 1.5 keeps every share below its cap: `calculateExperienceReward`
  for 210663 against levels 3-6 was not computed here; H-01 decides and fails loudly otherwise.
- **That a level-1..4 Mage can cast 1576, 1809 and 3195, and A can cast 3777, after seeding**: `MpCondition` against the seeded max MP, the
  charge conditions no-ops without a charged item, 3777's `itemuse` consuming 169300011 without checking its `restrict` levels — the oracle must
  confirm.
- **The effort letters and the ~79-83 agent-days** (§6): item letters on the m5b3-plan.md scale, not measurements.
- **That M5b-3 ports `initDropNpc`'s and `addDropItems`' team arms and `requestDropItem`'s team branches** (A-03), and M5d `getQuestDrop`'s,
  from their plans' text — whether the team arms are ported or left as `AION_UNPORTED` arms is not stated there; L-02 covers both.
- **That two 4.8 clients can run on one PC** (§11).
- **The gate budgets of 6-9 and ~6 minutes**: kill times at the D8 levels and the portal's use delay were not measured here.
- **That `CM_GROUP_DATA_EXCHANGE` payloads stay under `MAX_EXCHANGE_DATA_SIZE`** in real play.
- **That M5f's portal and instance pieces carry GA12-GA14 unchanged** (Nochsana's handler is `GeneralInstanceHandler` unless phase 6 lands one;
  its exit point comes from `instance_exit.xml` or the bind point — the oracle decides).

**Claims of the roadmap this analysis checked and found wrong or incomplete** (§3): the 294 as M5g's size; "find group" as including
matchmaking; the scope list (leagues, loot, exp share, chat, quest share, mantras, recall, group instances missing). **Confirmed:** "the largest
single area" — by bodies it is (~391 vs M5b-3's ~145), though not by difficulty per body.

---

## 13. Open questions this analysis could not settle without building

1. **Does D4's suppression leave lockdep empty for the whole alliance gate**, or does another nesting appear (a team lock under a
   `ConcurrentHashMap` stripe of `PlayerGroupService.groups`, a `Storage` monitor under the team lock in `TeamKinahDistributionEvent`, a quest
   lock under `QuestEngine.onKill` inside `team.forEach`)? LG-04 and the first G-04 run answer it.
2. **Which levels H-01 settles on** (D8), and whether a group rate of 1.5 stays under every cap at them; H-01 fails loudly if not, and the
   plan's fallback is a lower group rate (e.g. 1.2) rather than lower levels.
3. **Whether M5f's Daeva seed places a level-25 character in Eltnen with no side effect** (quest-driven zone triggers on enter world, m5f A-06)
   that disturbs GA12-GA14.
4. **How often the real client sends `CM_GROUP_DATA_EXCHANGE`**, and with what sizes; the first real-client session will show it.
5. **Whether the census warning of D6(c) appears in practice** at 600-630 s, or whether the census scan interval hides it. Step 10 of §11
   answers it.
6. **Whether `PlayerGroupStats`' stale reference ever keeps a logged-out member alive past 10 minutes in a live group** in normal play (a group
   that loses its lowest-exp member and never gains another). The census would name it; D7 leaves it faithful.
7. **Whether `GeneralTeam::getName()` is reached after the last leave** in normal play (risk 12), or only in contrived races.
8. **Whether M5b-2's geo gate built a line-of-sight fixture** the PARTY arm's `checkGeo` can reuse (D12); if not, the member-behind-a-wall case
   of the group buff stays unasserted by any gate.
9. **Whether M5j accepts matchmaking** (D16); if it does not, the owner becomes phase 6 and m5j A-G1 still has to drop `CM_AUTO_GROUP` and
   `PvPArenaService`.

---

## 14. Review, 2026-09-23

An adversarial review of rev 1 returned **needs-revision** with 13 findings (3 high, 6 medium, 4 low). Every finding was re-checked against the
Java and C++ sources, the data and the sibling plans; all were confirmed, two with a different resolution than the one suggested.

| # | Severity | Finding | Resolution in rev 2 |
|---|---|---|---|
| 1 | high | D4 misread the lock order: league → alliance is the normal order of every league event, alliance → league happens at six-plus sites, so rev 1's two suppressions at `LeagueLeftEvent` could not keep lockdep empty; LG-04's "exactly one CYCLE" was wrong | **Confirmed**, and counted: **nine** league events vs **eight** alliance-side sites, each of which also nests alliance under alliance through `League.broadcast` / `league.forEach` (the validator checks every held lock, `LockOrderValidator.cpp:188-198`). **D4 rewritten**: per-kind classes kept, the eight minority sites suppressed and named, the league side recorded; the allow-list option rejected because the gates have no lockdep allow-list. §2.11 item 3, correction 3, AL-02, AL-03, LG-03, **LG-04** (new lock-order test and mutations), GA7-GA9, **GA11**, risk 1 and §13 item 1 rewritten |
| 2 | high | Sibling plans hand work to M5g that rev 1 neither claimed nor reassigned; §0 stale (m5e, m5f exist) | **Confirmed.** New **§3.1** answers every hand-over. §0: A-11 (M5e: `AuraEffect`, `CM_TOGGLE_SKILL_DEACTIVATE`, `HealCastorOnAttackedEffect`), A-12 extended (teleport, portal arms, m5b2-p2-9), A-15 (N-06), A-16. **Mantras**: row 5b, E-26, **GP6b** (skill 1809). **Recall**: D15, W-06, K-02, E-28, **GP13b**. **Group-instance entry**: D17, **GA12-GA14**, X-01 (the alliance/league arms need level-50 instances, so they are unit-tested). **Matchmaking**: D16 — M5j, not M5g; the integrator records it in m5j A-G1 and m5f O-03. N-06's count change in §2.8, D1, I-01. `HealCastorOnTargetDeadEffect`'s arm: unreachable by data (no `healparty` in its only template) — noted, no owner needed |
| 3 | high | Sizing about half of the plan's own letters; stage-2 lanes coupled; I-02 under-rated | **Confirmed.** §6 re-budgeted from the letters: **~79-83 agent-days** (stage 1 44-48, stage 2 22, stage 3 8, integrator 5), critical path ~13 + ~10 + ~5 ≈ **28 calendar days**. I-02 is **L**, creates `AION_UNPORTED` `.cpp` shells for the 49 no-file classes, and is split into I-02a/I-02b. Stage 2 keeps two lanes with an explicit order: LG-02 after AL-02, AL-04 after LG-01, LG-04 after AL-03 |
| 4 | medium | `canChat` reaches three more unported P5-08 bodies on every message | **Confirmed**, and the flood arm too. W-03 ports **all five `ChatBanService` bodies** plus `isFlooding` and `logMessage` ×2; §2.8 outside count 54 → **66 firm** (57 with the chat bodies, 66 with recall) **+3 flood arm**; row 9, E-19, D14, C-08 (the GAG runnable), new gate case **C6c/GP5c** proves the flood and ban arms |
| 5 | medium | C12b impossible: no quest targets 210663 | **Confirmed** (and 1102 is `cannot_share`). GP11b moved to C14's 210133 kill with B holding 1102; H-01 picks the quests from the data; A-09 updated |
| 6 | medium | GP11's exact-exp row did not prove the level split or `XP_GROUP_HUNTING` (A's and B's values were the 80 cap) | **Confirmed.** D8: levels chosen by the oracle so no asserted share is capped (H-01 fails otherwise); `gameserver.rates.xp.group = 1.5, 3.0` against the solo 1.0; §10.4's "C = A" wording corrected; a rate-swap mutation added; GA6 the same |
| 7 | medium | M5b-3 D9 deferred more team drop arms than A-03/L-02 covered | **Confirmed**: `addDropItems`' `member_limit` arm, `getQuestDrop`'s group/alliance arms, `getEachDropMembers*`. A-03, L-02, L-04 (a unit case per arm), E-29, I-03 (P5-06 lease). One correction to the finding: `DropGroup.addDropItem`'s `each_member` arm is **already ported** (`DropGroup.cpp:51-62`, P4-13) — verify only |
| 8 | medium | Four stage-1 wake-ups closed but never asserted | **Confirmed.** New cases: **C19b/GP17b** (friendly death, revive MOVEMENT — with the honest limit that the revive's own update cannot be separated from `TeamStatUpdater`'s), **C15b/GP15b** (quest share), **C6b/GP5b** (data exchange), and the revive's ERROR-free path |
| 9 | medium | The P5-10 split names conflicted with M5h's | **Confirmed**, resolved differently from the suggestion: **m5h-plan.md rev 2 A-18 already adopts M5g's names** ("P5-10f as named there, never a different letter"), so D1 keeps them; m5h's leftover D3/I-01/lane rows are the integrator's to align (§3.1). The legion undeclared count stays 7 (breakdown in §2.8) |
| 10 | low | E-12 understated: every login reaches `PlayerGroup::getMember` once any group exists | **Confirmed.** E-12 restated (and the alliance counterpart); GR-01/AL-01 put `getMember` first and GR-01 merges with GR-02; finding 3 of §1 names E-12 as the one solo-reached site |
| 11 | low | Mutation claims not killed or order-dependent | **Confirmed.** GP6: B carries a CHANT effect (8998) and the expected entries come from the oracle; GP7's MYPARTY claim replaced by a known-list mutation D really kills; C12 runs **two round-robin kills with the same members** before the range kill; GP4 reworded and C5 gains "B invites B" to show leader-before-self |
| 12 | low | D6(b)'s premise doubtful | **Confirmed**: the last online member's logout always disbands, so no group survives a shutdown. D6(b) re-derived to the holders that outlive members (`DropNpc`, `registeredTeam`); C22 now stops the server **with a live group** and GP23 checks it |
| 13 | low | Internal contradictions and citation errors | **Confirmed, all six**: A-01 cites m5b3-plan.md:33; `CM_FIND_GROUP` has 18 read cases and 16 run arms, O-07 corrected; W-04 and O-04 consistent (the rift call sites close in W-04, `VortexService` stays M5i's); GR-05 names `tests/playersvc/PlayerEventsTestSupport.h`; §11 says which steps need a third client; GA9 notes the throw |

**Not changed, and why.** Nothing was rejected outright. Two resolutions differ from the review's suggestion: finding 9 (M5h already adopted
M5g's names, so M5g keeps them) and finding 2's matchmaking point (M5g names M5j as the owner instead of taking it). This plan cannot edit the
sibling plans; the edits they need are listed in §3.1 for the integrator: m5j A-G1 and J1, m5f O-03, m5h D3/I-01/lanes.
