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
