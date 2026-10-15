# P6 instances (docs/design/p6-instances-plan.md): deviations and corrections

The instance handlers and their AIs (chunks I1-I6) are ported statement for statement. The per-checkpoint notes (what each test covers, what
is left to a gate) are in the plan's progress section. This file lists the Java bugs found while porting: those the owner approved as
corrections, and those still only proposed.

## Proposed corrections (Java bugs, kept as ported)

- **J-P6I7-1** (P6I-7c): BrigadeGeneralVashartiAI.onEffectEnd switches on Vasharti's own npc id (`getNpcId()`) instead of the visited npc's
  (`n.getNpcId()`), so the Sea of Fire effects 283010-283012 are never deleted there. Proposed: switch on `n.getNpcId()`.
- **J-P6I8-1** (P6I-8a): KamarBattlefieldInstance.onDie for 232853 calls `onStop(true)` before `updatePoints`, which then finds the run ended
  (not START_PROGRESS) and drops the 3500 points. Proposed: score before stopping.
