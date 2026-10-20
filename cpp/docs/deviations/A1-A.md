# A1, lane A (portals, quests, classNpc, walkers, events): notes, retests and proposed corrections

The AIs of chunk A1's portals, quests, classNpc, walkers and events directories are ported statement for statement. This file lists what
the unit tests (`tests/handlers_ai_world/<Slice>A1AiTest.cpp`) cannot show, the retests that cover it in the kit, and the Java bugs found
(J-A1A-n, proposed only).

## Portals (checkpoint cp1)

Tests: `tests/handlers_ai_world/PortalsA1AiTest.cpp` (12 cases). The test world has Poeta's map only, so a teleport to another map (or to a
second Poeta instance) stops at its packet; the cases check that the teleport was made by the map it names. Mutation proof 11 of 12. The
survivor, GroupGateAI's `decisionTaken` flag, is not observable by a unit test: the response requester answers each request once.

### Retest: the Sorcerer's group gate (GroupGateAI, "groupgate")

- Where: any world map. A Sorcerer (a Daeva) casts the group gate skill (Summon Group Gate; the gate is npc 833207 for an Asmodian,
  833208 for an Elyos; the binding gates are 749131 and 749132).
- What should happen: the caster and the members of his group can click the gate. Each gets the question "Do you accept the move?"
  (STR_ASK_GROUP_GATE_DO_YOU_ACCEPT_MOVE) and must stay within 9 m. Accepting teleports to Pandaemonium (833207: 120010000, 1657.5 1398.7
  194.7) or Sanctum (833208: 110010000, 1444.9 1577.2 572.9), or to the bind point (749131, 749132), with the jump-in animation. Declining
  does nothing.
- What should be refused: a player who is not a Daeva gets "You cannot use it before changing your class"
  (STR_SKILL_CAN_NOT_USE_GROUPGATE_BEFORE_CHANGE_CLASS); a Daeva who is neither the caster nor in his group gets "You have no right to use it"
  (STR_SKILL_CAN_NOT_USE_GROUPGATE_NO_RIGHT).
- Before this port the gate's npc had a DummyAI: clicking it did nothing (the kit's report).

The other portals are covered by the instance retests that reach them: Dramata (Kaisinel's Academy and Marchutan Priory), the Legion
Dominion portal to Stonespear Reach, the Sealed Danuar Mysticarium entrances, the Occupied Rentus Base portals, the Abyssal Splinter devices,
the training grounds' portal (the auto group window) and the portal_request and portal_elevator npcs.

## Quests (checkpoint cp2)

Tests: `tests/handlers_ai_world/QuestsA1AiTest.cpp` (4 cases; the fixture is `A1LaneATestSupport.h`, the npcs stored in World so that a
delete removes them). Mutation proof 4 of 4 (GarnonQ20060AI's delay, delete and cancel; NidalberBalaurAI's map check).

Retest (no quest scripts, quest maps or walks in the unit tests):
- Quest 20060 (Garnon, "Q20060"): three minutes after Garnon spawns he is replaced by 800020 at 442.3 464.3 341.5.
- The infiltrators ("infiltrator", 282913-282923): each death spawns the next infiltrator of its chain at the dead one's spawn point. The
  last ones of the chains (282914, 282916, 282922, 282923) spawn npc 0, as Java does: a "no template" error in the log, nothing spawned
  (J-A1A-1 below).
- The Nidalber balaur ("nidalber_balaur", maps 310040000 and 320040000): it walks to 271.2 178.9 204.5 and attacks the quest npc (204044 or
  204432); without that npc, or with it dead, the balaur goes.
- "quest_start_use_item" objects open the quest selection when they start quests (else the start talk), and a dialog npc's page 1011 when
  no quest answers; "quest_use_npc" npcs talk on use.

### Proposed correction J-A1A-1 (InfiltratorsAI, kept as ported)

InfiltratorsAI.java:40 spawns `spawnNpc` even when the switch (lines 22-39) gave none (`spawnNpc` stays 0 for 282914, 282916, 282922,
282923 and every other npc id): the spawn of npc 0 fails with an error in the log. Proposed: `if (spawnNpc != 0)` before the spawn.

## classNpc and walkers (checkpoint cp3)

Tests: `tests/handlers_ai_world/ClassNpcA1AiTest.cpp` (5 cases). Mutation proof 6 of 6.
Retest (the servants' spawns and skills, the shouts and the walks are not
in the unit tests):
- The drakan priests ("xdrakanpriest") and medics ("drakanmedic"): attacked, they sometimes (3 %) summon servants around them (282988;
  a medic also one healing servant, 281621 or 281839) when they have none, with message 341784; the servants go when the priest gets
  home, despawns or dies. A healing servant heals its priest every 6 s; an enemy servant attacks its priest's target with skill 16907.
- Naia ("naia", Poeta's walker): she shouts once near the cannon (203145) and once near Qydro (203125); Polor and Serin ("polorserin")
  sheathe their weapons near the adults (203129, 203132); the runners ("generalrunner") draw them at each waypoint.

## Events (checkpoint cp4)

Tests: `tests/handlers_ai_world/EventsA1AiTest.cpp` (7 cases), mutation proof 12 of 12. Events are disabled in the kit (`disabled_events = *`):
these cases are their only check, and no retest is filed for them. Not covered: the item rewards (no item rows), the buffs (no skill rows), the
quest dialogs, the drops and the spawns into World.
