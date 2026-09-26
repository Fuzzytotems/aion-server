# M5b-1 first real-client session, 2026-09-22

The first fight against the C++ server with a real 4.8 client, on the day M5b-1 stage 1 landed (commit 340c05c5e). The user made a new Elyos
Warrior (`Iziseki`), killed five monsters and reached level 2. Recorded here because three of its findings are things no test had produced.

## What the session proves

| | |
|---|---|
| Startup | "Game server started in 149 seconds", 83,847 npc spawns, **0 ERROR lines** before the first kill |
| Combat | A player can target, attack, be attacked, and kill - the whole M5b-1 path runs against a real client |
| Reward | `exp = 400` after five kills, i.e. **exactly 5 x 80** |

That reward number is the best kind of confirmation: two lanes independently found the plan's experience arithmetic was one table row off and
corrected it from 206 to 80 per kill (D7), and a real client then produced 5 x 80 = 400 without anybody arranging it.

## Findings

**S-1. Every kill logs an ERROR, exactly as predicted before the session.** Five kills, five
`onDie() exception for Npc [...]` lines, one per monster (juvenile sparkie x3, striped kerub, longnosed snuffler).
`NpcController::doReward` ends with `DropRegistrationService::registerDrop`, which is `AION_UNPORTED`, and `NpcAI::ask(REWARD_LOOT)` answers
**true** for an ordinary npc, so the call is always made. The `try` around the reward block catches it, so the kill completes and the experience
is kept - but `InstanceHandler::onDie` and the `DIED` AI event are skipped, and the log gets an ERROR per kill. Loot itself is M5b-3; the fix for
M5b-1 is to make `registerDrop` an `AION_PARTIAL` with its reason, so nothing throws and the gate can assert the site is reached.

**S-2. One object leaked.** The shutdown hook reported `1 objects removed from the world are still alive` and `1 objects still tracked`. The run
had no census enabled (a plain start writes no reports), so the object is not named. Five kills and one character are the whole session, so the
leak is on the combat or the death path - which is precisely what the stage-2 gate's Q8 and the stress nightly's one-minute census exist to name.
**Do not close stage 2 without identifying it.**

**S-3. A magical main-hand weapon cannot auto-attack.** `CreatureController::attackTarget` routes a non-PHYSICAL `getAttackType()` through
`standins::attackUtilCalculateMagAttackResult`, which is a stand-in until M5b-2 (O-02). The user was warned before the session and made a Warrior;
a Mage, Priest or Spiritmaster would throw on the first swing. The §10 checklist must say melee classes only until M5b-2.

## Not exercised

Death and revive (the user did not die), gliding, a second race, and everything M5b-2 and M5b-3 own: skills, loot, item use.
