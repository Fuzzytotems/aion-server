# P6 instances (docs/design/p6-instances-plan.md): deviations and corrections

The instance handlers and their AIs (chunks I1-I6) are ported statement for statement. The per-checkpoint notes (what each test covers, what
is left to a gate) are in the plan's progress section. This file lists the Java bugs found while porting: those the owner approved as
corrections, and those still only proposed.

## Corrections of the Java code (applied)

### IronWallWarfrontInstance: the start-position teleport overrides portToStartPosition (owner's decision 2026-10-15, both branches)

Java (IronWallWarfrontInstance.java:157-171) declares `portToPosition(Player, WorldMapInstance)`, which overrides nothing and has no caller.
BasicPvpInstance.onReviveEvent and IronWallWarfrontInstance.onStart (`instance.getPlayersInside().forEach(this::portToStartPosition)`)
reach GeneralInstanceHandler.portToStartPosition, which throws UnsupportedOperationException. So with players inside the start task fails
before it schedules endPreparingAndStart: the run never starts, and a revive throws too.

C++ (P6I-8a, its own commit): the method is `portToStartPosition(Player&) override`, on the handler's instance; the body is Java's.
Test: `PvpTest.IronWallsStartSendsEachRaceToItsSide` (tests/handlers_inst_3/PvpTest.cpp): two players of the two races inside, the start
runs without throwing, and each player stands at one of their race's start positions, the two on opposite sides. Mutation proof 2 of 2
(the side condition swapped, a start position moved).

### KamarBattlefieldInstance adds 232853's points before ending the run (owner's decision 2026-10-15, both branches; was J-P6I8-1)

Java (KamarBattlefieldInstance.java:165-212) calls `onStop(true)` inside the switch for 232853, before `updatePoints`, which then finds the
run ended (not START_PROGRESS) and drops the 3500 points.

C++: the switch only sets the points; `updatePoints` runs, then `onStop(true)` for 232853, unless the points already ended the run (a lead of
20,000 calls `onStop(false)` in updatePoints; a second stop would give the rewards twice). Test: `PvpTest.KamarsEnds` expects 4550 Elyos points
after the death. Mutation proof 2 of 2 (the stop before the points; the stop removed).

### VaultKeeper counts a room up only for a new player (owner's decision 2026-10-19, both branches; was J-I6-1)

Java (VaultKeeper.java:37-61) counts `room` up in every branch, also for a player coming back to his room, so he takes the next room
away from the next new player and the rooms run out early.

C++: `room` counts up only for a player not yet in playerAndRoom. Test:
`ShugoEmperorsVaultTest.AReturningPlayerKeepsHisRoomWithoutTakingTheNext` (two visitors, one return, then the third new player keeps
the third room: no closing dialog). Mutation proof 1 of 1 (the room counted up for a returning player).

### RuthlessJabaraki's phases are its switch's 95, 80, 65, 50, 35 and 20 % (owner's decision 2026-10-19, both branches; was J-I6-2)

Java (RuthlessJabaraki.java:27-41) builds HpPhases(96, 80, 60, 45, 40, 35) but switches on 95, 80, 65, 50, 35 and 20, so only the 80 %
and 35 % phases called adds.

C++: HpPhases(95, 80, 65, 50, 35, 20), one wave a phase. Test: `ShugoEmperorsVaultTest.JabarakisWaves`
(all six waves). Mutation proof 1 of 1 (Java's phases).

### ShugoImperialTombInstance's despawnNpcs takes 219512 and 219513 (owner's decision 2026-10-19, both branches; was J-I6-3)

Java (ShugoImperialTombInstance.java:1321-1324) lists 291512 and 291513 for 219512 (Krall Lookout Commander) and 219513 (Swift Krall
Graverobber), so a destroyed monument left those two standing.

C++: the ids 219512 and 219513. Test:
`ShugoImperialTombTest.ADestroyedMonumentEndsTheStage` (both are taken away). Mutation proof 1 of 1 (Java's ids).

### Terath's handleBackHome clears the Aetheric Field it deletes (owner's decision 2026-10-19, both branches; was J-I6-4)

Java (BrigadeGeneralTerathAI.java:156-168) deletes the Aetheric Field but keeps the reference, so the next fight spawned no field and
did not close door 706 again.

C++: the reference is cleared after the delete. Test: `TiamatTest.TerathsGravityEvent` (the next fight
brings a new field). Mutation proof 1 of 1 (the reference kept).

### Tahabata's altars check their bands on 283116 and 283118 (owner's decision 2026-10-19, both branches; was J-I6-5)

Java (TahabataAltarAI.java:32-47) chooses the debuff for 283116/283118 but checks the bands on 283253/283255, ids no npc template has,
so no altar ever debuffed.

C++: the bands on 283116 (25-37 m, 20970) and 283118 (20-25 m, 20971). Test:
`TiamatRestAiTest.TahabatasAltarsDebuffInTheirBands` (a cast inside the band, none outside or at an npc). Mutation proof 1 of 1 (Java's
ids).

### Tiamat's eyes speak with the ids the handler spawns them as, 283177-283180 (owner's decision 2026-10-19, both branches; was J-I6-6)

Java (TiamatEyeAI.java:20-39) switches on the 4.0 ids 283913-283916, but TiamatStrongHoldInstance spawns the eyes as 283177-283180, so
no eye spoke.

C++: the switch on 283177-283180. Test: `TiamatRestAiTest.TheEyesSpeakWithTheHandlersIds` (each eye's message 2 s after
its spawn; the 4.0 id says nothing). Mutation proof 1 of 1 (Java's ids).

### spawnChests keeps the chest locations (owner's decision 2026-10-19, both branches; was J-P6I4-1)

Java (AbstractInnerUpperAbyssInstance.java:164-180) clears chestLocations and sets it null at the end of spawnChests, and a keep's two boss
ids both call it (e.g. KrotanBarracks `case 233632, 233633`), so the second boss death threw a NullPointerException.

C++: the owner's choice is to keep the list: spawnChests no longer clears it, and the second boss death brings the chests again. Test:
`AbyssKeepTest.TheBossesDeathBringsTheChests` (22 chests and 2 bonus chests after both deaths). Mutation proof 1 of 1 (Java's clear and null).

### Vasharti's Sea of Fire ends with its fire effects deleted (owner's decision 2026-10-19, both branches; was J-P6I7-1)

Java (BrigadeGeneralVashartiAI.java:132-148) switches on Vasharti's own npc id (`getNpcId()`) instead of the visited npc's
(`n.getNpcId()`), so the Sea of Fire effects 283010-283012 were never deleted there.

C++: the switch on `n.getNpcId()`. Test: `RentusTest.TheEndOfTheSeaOfFireTakesTheFire` (the three fire effects she knows are deleted, another
npc stays). Mutation proof 1 of 1 (Java's `getNpcId()`).

## Proposed corrections (Java bugs, kept as ported)

