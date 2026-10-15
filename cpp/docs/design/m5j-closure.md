# M5j closure (stage 4 CP8, m5j-plan.md §10.6)

Lane A, 2026-10-14, branch `lane-a/m5j-s4-cp8` (over `lane-a/m5j-d12-cp5b`). Item I-04, decision D11, gate item G-42.

## 1. What the check is

- `tools/porting/closure.py --check`, CTest `porting.phase5.census` (no label; it runs with the other tool checks, about a second).
- It scans the `.cpp`/`.h`/`.inc` files below `game-server/src` and `game-server/handlers` for `AION_UNPORTED(` and `AION_PARTIAL(` calls outside
  comments, strings and `#define`s (the macros of `runtime/base/Unported.h` are not sites). A file counts when `chunks.owners_of` puts it in a
  phase-5 chunk (`P5-*`) or in C1/C2. Sites are keyed by file and enclosing function, so a line moving inside a function does not churn the list.
- Every key needs a `site|path|function|count|owner` row of `docs/porting/phase5-allowlist.txt` with the same count and a non-empty owner.
- Every opcode registered in `ClientPacketInfo.gen.inc` needs `clientpackets/<Class>.cpp`, or an
  `opcode|<Class>|owner` row.
- A row that no longer matches a site or a missing class fails as stale, so porting a body forces the row out in the same commit.
- `tools/porting/tests/test_closure.py` covers the scanner and each failure kind (porting suite, 75 tests).
- §10.6's smoke half: the command smoke (`Registered/CommandSmokeTest`, `CommandSmokeListTest`, L-04) and the AI smoke
  (`EveryRootAi/RootAiSmokeTest`, `QuestNpcAiSmokeTest`, N-02) now carry the CTest label `smoke` beside the three startup smokes:
  `ctest -L smoke` runs 173 tests, 173 passed (2026-10-14, under the gate lock).

## 2. The measure (2026-10-14, this branch)

**487 sites in 461 functions; 33 registered opcodes without a C++ class.** By chunk:

| Chunk | Sites |
|---|---:|
| C2 | 1 |
| P5-06a | 1 |
| P5-09a | 3 |
| P5-09b | 11 |
| P5-10c | 3 |
| P5-10f | 86 |
| P5-11 | 89 |
| P5-12a | 104 |
| P5-12b | 187 |
| P5-13 | 2 |

M5j's own chunks (C1, C2, P5-13) leave three sites, each kept by an earlier decision: C2's `//reload quests` arm (A-D1, with P5-06a's
`QuestEngine::reload`), `InstanceEngine::addInstanceHandlerClass` (m5f-plan D10) and `InstanceService::getOrCreateHouseInstance` (M5h,
m5f-plan O-05). Everything else belongs to milestones that have not closed (lane B's M5g/M5h/M5i), to a user-deferred feature, to phase 6,
or to an open pull request.

## 3. Every remaining site, by owner

Counts are sites; the allow-list has the function of each one.

### M5i (lane B; m5j-plan A-I1: P5-12a/b, siege and world events)

266 sites in 257 functions, 32 files:

- `src/aion/gameserver/model/base/Base.cpp` 25
- `src/aion/gameserver/model/base/BaseLocation.cpp` 3
- `src/aion/gameserver/model/event/Headhunter.cpp` 2
- `src/aion/gameserver/model/rift/RiftLocation.cpp` 6
- `src/aion/gameserver/model/siege/AgentLocation.cpp` 2
- `src/aion/gameserver/model/siege/ArtifactLocation.cpp` 7
- `src/aion/gameserver/model/siege/FortressLocation.cpp` 3
- `src/aion/gameserver/model/siege/OutpostLocation.cpp` 3
- `src/aion/gameserver/model/siege/SiegeLocation.cpp` 6
- `src/aion/gameserver/model/vortex/VortexLocation.cpp` 5
- `src/aion/gameserver/services/BaseService.cpp` 10
- `src/aion/gameserver/services/RiftService.cpp` 7
- `src/aion/gameserver/services/ShieldService.cpp` 1
- `src/aion/gameserver/services/SiegeService.cpp` 23
- `src/aion/gameserver/services/VortexService.cpp` 10
- `src/aion/gameserver/services/WorldRaidService.cpp` 3
- `src/aion/gameserver/services/event/Event.cpp` 15
- `src/aion/gameserver/services/event/EventBuffHandler.cpp` 26
- `src/aion/gameserver/services/event/EventBuffHandler.h` 1
- `src/aion/gameserver/services/event/EventService.cpp` 1
- `src/aion/gameserver/services/panesterra/PanesterraService.cpp` 20
- `src/aion/gameserver/services/rift/RiftManager.cpp` 4
- `src/aion/gameserver/services/siege/ArtifactAssault.cpp` 5
- `src/aion/gameserver/services/siege/Assault.cpp` 5
- `src/aion/gameserver/services/siege/BalaurAssaultService.cpp` 7
- `src/aion/gameserver/services/siege/FortressAssault.cpp` 12
- `src/aion/gameserver/services/siege/Siege.cpp` 14
- `src/aion/gameserver/services/siege/SiegeCounter.cpp` 5
- `src/aion/gameserver/services/siege/SiegeRaceCounter.cpp` 9
- `src/aion/gameserver/services/siege/SiegeRaceCounter.h` 2
- `src/aion/gameserver/services/vortex/DimensionalVortex.cpp` 7
- `src/aion/gameserver/services/worldraid/WorldRaid.cpp` 17

### M5h (lane B; m5j-plan A-H1: P5-11 and the legion services)

163 sites in 146 functions, 14 files:

- `src/aion/gameserver/model/challenge/ChallengeQuest.cpp` 5
- `src/aion/gameserver/model/challenge/ChallengeTask.cpp` 7
- `src/aion/gameserver/model/house/HouseRegistry.h` 1
- `src/aion/gameserver/model/team/legion/Legion.cpp` 27
- `src/aion/gameserver/model/team/legion/LegionEmblem.cpp` 6
- `src/aion/gameserver/model/team/legion/LegionMember.cpp` 5
- `src/aion/gameserver/model/team/legion/LegionWarehouse.cpp` 28
- `src/aion/gameserver/model/town/Town.cpp` 2
- `src/aion/gameserver/services/ChallengeTaskService.cpp` 8
- `src/aion/gameserver/services/HousingBidService.cpp` 7
- `src/aion/gameserver/services/LegionService.cpp` 62
- `src/aion/gameserver/taskmanager/tasks/housing/AuctionAutoFillTask.cpp` 2
- `src/aion/gameserver/taskmanager/tasks/housing/AuctionEndTask.cpp` 1
- `src/aion/gameserver/taskmanager/tasks/housing/MaintenanceTask.cpp` 2

### phase 6 (m5j-plan A-I1, m5i O-01: Ahserion)

23 sites in 23 functions, 2 files:

- `src/aion/gameserver/services/panesterra/ahserion/AhserionRaid.cpp` 15
- `src/aion/gameserver/services/panesterra/ahserion/PanesterraTeam.cpp` 8

### M5h (lane B; m5j-plan A-H2: Legion Dominion, m5h O-01)

12 sites in 12 functions, 2 files:

- `src/aion/gameserver/model/legionDominion/LegionDominionLocation.cpp` 6
- `src/aion/gameserver/services/LegionDominionService.cpp` 6

### capital-economy milestone (m5j-plan A-C4, user decision m5c-plan D2)

11 sites in 11 functions, 1 file:

- `src/aion/gameserver/services/BrokerService.cpp` 11

### M5g (lane B; m5j-plan A-G1: PlayerAllianceService, lane-b/m5g-parties)

3 sites in 3 functions, 1 file:

- `src/aion/gameserver/model/team/alliance/PlayerAllianceService.cpp` 3

### PR #132 (M5b-3 CP4 bodies, not on this stack: AdventService/AtreianPassportService)

3 sites in 3 functions, 2 files:

- `src/aion/gameserver/services/AtreianPassportService.cpp` 1
- `src/aion/gameserver/services/reward/AdventService.cpp` 2

### A-D1 (m5d-plan D8: QuestEngine.reload stays unported; //reload quests stays out)

2 sites in 2 functions, 2 files:

- `handlers/aion/gameserver/handlers/admincommands/Reload.cpp` 1
- `src/aion/gameserver/questEngine/QuestEngine.cpp` 1

### a PvP milestone the roadmap does not have yet (m5j-plan A-I2, m5i O-04: the conqueror/protector PvP arms)

2 sites in 2 functions, 1 file:

- `src/aion/gameserver/services/conquerorAndProtectorSystem/ConquerorAndProtectorService.cpp` 2

### m5f-plan D10 (stays: instance handlers register by AION_INSTANCE_HANDLER markers)

1 site in 1 function, 1 file:

- `src/aion/gameserver/instance/InstanceEngine.cpp` 1

### M5h (m5f-plan O-05: house instances and studios)

1 site in 1 function, 1 file:

- `src/aion/gameserver/services/instance/InstanceService.cpp` 1

## 4. Registered opcodes without a C++ class, by owner

§10.6 says "only the two commented-out ones may be missing". The two registrations commented out in Java are not in the generated
`ClientPacketInfo.gen.inc` at all, so they need no row; but 33 registered opcodes have no class. Each belongs to a milestone that has not
run yet, so they are allow-list rows with owners, not failures. **This is a reading of §10.6, not a change to it: for the integrator.** When M5h and the capital-economy milestone land,
the rows go and the check is §10.6's strict form.

- **M5h (lane B; m5j-plan A-H1: the legion and housing packets)** (23): `CM_LEGION_SEND_EMBLEM_INFO`, `CM_HOUSE_SCRIPT`, `CM_LEGION`, `CM_LEGION_SEND_EMBLEM`, `CM_LEGION_HISTORY`, `CM_LEGION_MODIFY_EMBLEM`, `CM_HOUSE_KICK`, `CM_HOUSE_SETTINGS`, `CM_HOUSE_DECORATE`, `CM_LEGION_WH_KINAH`, `CM_HOUSE_EDIT`, `CM_HOUSE_TELEPORT_BACK`, `CM_LEGION_UPLOAD_INFO`, `CM_LEGION_UPLOAD_EMBLEM`, `CM_GET_HOUSE_BIDS`, `CM_REGISTER_HOUSE`, `CM_PLACE_BID`, `CM_HOUSE_TELEPORT`, `CM_HOUSE_PAY_RENT`, `CM_USE_HOUSE_OBJECT`, `CM_RELEASE_OBJECT`, `CM_HOUSE_OPEN_DOOR`, `CM_CHALLENGE_LIST`
- **capital-economy milestone (m5j-plan A-C4, user decision m5c-plan D2)** (9): `CM_BROKER_SELL_WINDOW`, `CM_BROKER_LIST`, `CM_BROKER_SEARCH`, `CM_BROKER_REGISTERED`, `CM_BUY_BROKER_ITEM`, `CM_REGISTER_BROKER_ITEM`, `CM_BROKER_CANCEL_REGISTERED`, `CM_BROKER_SETTLE_LIST`, `CM_BROKER_SETTLE_ACCOUNT`
- **M5h (lane B; m5j-plan A-H2: Legion Dominion, m5h O-01)** (1): `CM_LEGION_DOMINION_REQUEST_RANKING`

## 5. Out of the check's scope, listed for completeness

Phase-4 and A1 sites are not phase-5 work and the check does not count them; the ones that M5j's plan names:

- P4-09: `EventData::setEvents`, `NpcSkillData::setNpcSkillTemplates`, `XMLQuests::setData` (Java's reload setters; //reload republishes
  the holder instead, item 3 of 2026-10-13), `ZoneData::saveData`.
- P4-10: `ZoneService::saveMaterialZones`.
- P4-11a: `HouseObject::getPlacementLimit` and `UseableItemObject`'s `placementLimitOf` (M5h, housing).
- P4-11b: the `ControllerStandIns` `followStartService` and `riftEnumData` stand-ins (M5i: the follow-start service and the rifts).
- P4-16: `SM_FORTRESS_STATUS` and `SM_INFLUENCE_RATIO` (M5i), `SM_HOUSE_BIDS` (M5h).

## 6. Running it

```
python tools/porting/closure.py            # the check (exit 1 on a problem)
python tools/porting/closure.py --list     # every site and missing opcode as an allow-list row with the owner empty
ctest --test-dir build/msvc -C Debug -R porting.phase5.census
ctest --test-dir build/msvc -C Debug -L smoke
```

**When the lane stacks combine.** This list was measured on lane A's stack only. Lane B's M5h stacks (#161-#244) port most of the 23
M5h opcodes, and lanes B and C port many of the listed functions on their own stacks. After a merge those rows go stale, and the check fails
on each of them by name: `... (stale): remove the row site|<path>|<function>|...`, or `opcode|<CM_X>|...`, or `set the row count to N`
when a function keeps fewer sites. Whoever merges second deletes or lowers exactly the rows the check names, in the merge commit.

When a body is ported, delete or lower its row in the same commit; when a site is added in scope, add a row with its owner. A milestone
that closes (D11 offers the check to M5b-3..M5i as their end check) should leave no row naming it.
