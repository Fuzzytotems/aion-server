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

## Proposed corrections (Java bugs, kept as ported)

- **J-P6I7-1** (P6I-7c): BrigadeGeneralVashartiAI.onEffectEnd switches on Vasharti's own npc id (`getNpcId()`) instead of the visited npc's
  (`n.getNpcId()`), so the Sea of Fire effects 283010-283012 are never deleted there. Proposed: switch on `n.getNpcId()`.
