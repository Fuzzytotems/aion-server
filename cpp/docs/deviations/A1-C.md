# A1, lane C (the world AIs of inggison, heiron, kaldor, gelkmaros, levinshor, pernon, eltnen, brusthonin): corrections

## Corrections of the Java code (applied)

### GeyserBlockerAI deletes the geyser after the known list's update (owner's decision 2026-10-19, both branches; was J-A1C-1)

Java (GeyserBlockerAI.java:40-48) deletes the geyser inside handleCreatureSee, which runs inside the known list's update under its lock
(KnownList.update is synchronized, KnownList.java:43), while World.removeObject takes the npc's: the reverse of the Npc -> NpcKnownList lock
order, which the port had to suppress (a LockdepSuppression).

C++: the delete is a task (delay 0) that runs once the update has
returned, and the suppression is gone. Tests: `InggisonA1AiTest.TheGeyserBlockerHoldsTheGeyserUntilItDies` (the geyser still stands inside
see() and is gone after the task), `TheGeyserBlockerTellsThePlayersNearby` (runs the task before the death). Mutation proof 1 of 1 (the
delete inside see()).

### BollvigAI.cancelTask cancels all four tasks (owner's decision 2026-10-19, both branches; was J-A1C-2)

Java (BollvigAI.java:102-111) cancels the tasks in an else-if chain, so only the first that is not done; a phase or a return home left the
older round's Charming Attraction running.

C++: independent ifs, every task that is not done is cancelled. Test:
`HeironA1AiTest.APhaseCancelsEveryTaskOfTheRound` (after the 25 % phase only its own round's 280804 comes, 46 s after the 50 % phase, not
one second earlier). Mutation proof 1 of 1 (the else-if chain).
