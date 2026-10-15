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

## Proposed corrections (Java bugs, kept as ported)

- **J-P6I7-1** (P6I-7c): BrigadeGeneralVashartiAI.onEffectEnd switches on Vasharti's own npc id (`getNpcId()`) instead of the visited npc's
  (`n.getNpcId()`), so the Sea of Fire effects 283010-283012 are never deleted there. Proposed: switch on `n.getNpcId()`.
- **J-P6I8-1** (P6I-8a): KamarBattlefieldInstance.onDie for 232853 calls `onStop(true)` before `updatePoints`, which then finds the run ended
  (not START_PROGRESS) and drops the 3500 points. Proposed: score before stopping.
