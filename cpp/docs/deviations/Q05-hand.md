# Q05 (Poeta, Eltnen, Oriel quest handlers): hand ports

Q05's record of its hand ports, kept under this name: the two handlers' comments cite it. The route-gen lane creates
`docs/deviations/Q05.md`; the route-hand lane of 2026-09-29 writes here so the two lanes do not edit one file, and Q05.md's section
"Hand ports" is a link to this file (one line, added by the integrator when the lanes merge).

## Hand ports

The route-hand lane (phase 6, 2026-09-29, the retail ascension route) hand-ported the two Poeta quests that `tools/gen/questgen` refuses.
Both follow the questgen output's conventions (one `.cpp` per quest, the class in `aion::gameserver::handlers::quest::poeta`,
`AION_QUEST_HANDLER` at the end, Java statement order and literals, `// N` step comments kept) and are ported statement by statement.
**Neither has a deviation from Java.**

| File | Java | Why questgen refuses it | How the refused construct is ported |
|---|---|---|---|
| `handlers/.../quest/poeta/_1002RequestoftheElim.cpp` | `quest/poeta/_1002RequestoftheElim.java` | the anonymous `Runnable` of `:140`; `new SM_ASCENSION_MORPH` of `:170` (not in questgen's API table) | The Runnable (fieldmap `_1002RequestoftheElim$1`, storage: task) is a pinned task, `schedule({this, &env, &flyer}, [...], 43000)`: its three captures (the handler, Immortal under RT-11; the env; the player) are the Pin, as `AbstractQuestHandler.cpp:955` pins its own item-use task. `SM_ASCENSION_MORPH` is ported (`SM_ASCENSION_MORPH(int32_t)`) and the prelude names it |
| `handlers/.../quest/poeta/_1114TheNymphsGown.cpp` | `quest/poeta/_1114TheNymphsGown.java` | the lambda of `:115` | `KnownList::forEachNpc` takes a `std::function` it runs before it returns, so the lambda is a plain synchronous visitor capturing the player by reference |

Java behaviour kept as it is (no `// java-bug kept` marker is needed; none is a bug):

- 1002 enters `WorldMapType.KARAMATIS` (310010000), the pre-ascension Karamatis, not Karamatis B (310020000) of 1006. Its enter-world hook
  sends `SM_ASCENSION_MORPH(1)` inside 310010000 and otherwise puts step 20 back to 13, which is how a player who left the instance before
  Belpartan (205000) gets back to Daminu (730008).
- 1002's Belpartan arm has no `break` after its inner switch (the last case of the outer switch), so any other dialog there answers `false`.
- 1114's item-started dialog (`targetId == 0`) sends `SM_DIALOG_WINDOW(0, 0)` for every action other than `QUEST_ACCEPT_1` while the quest is
  startable, then falls through to `if (qs == null) return false`.

Tests (chunk Q09's test directory, `tests/quest_handlers_zones`, because the two ports are part of one lane with Q09's four):
`PoetaHandPortsTest.cpp` (every hook and step of both quests through the real `QuestEngine` on a real World: registration, the dialogs of
every step with their pages, var writes, item gives and removals, movie 20, the flute-gated sleeping elders and their respawn, `onCanAct`,
the enter-world arms, the 43 s Belpartan flight on the ManualClock with its flight transporter (1001), Seirenia's hate (50), Namus'
reward straight from step 2 and from step 3, both reward groups of 1114 with their kinah, 1002's finish with its first
selectable item, the level-up and quest-completed starts) and `SoloInstanceQuestTest.cpp` (Karamatis entered through Daminu, the morph
at CM_LEVEL_READY, Belpartan's flight back to Poeta, the mission finished outside; and the early leave that puts step 20 back to 13 and lets
Daminu send the player into a new instance, not the one he left and is still registered with). Q09's executable links Q09's library only, so `PoetaHandPortsTest.cpp` compiles the two Q05
files by `#include`, the arrangement P5-05's test executable uses for the two quest npc AIs of A1 (chunks.cmake, the P5-05 lease row).
Structural parity (`python tools/parity/parity.py pair <java> <cpp>`): 1114 is at parity; 1002's only mismatches are the `Runnable` and
`run` tokens of Java's anonymous class (call multiset and call order), which a C++ lambda has no counterpart for; every literal,
DialogAction, other call and spawn id matches (parity has no per-line waiver yet, phase6-inventory.md §9.2).
Mutation proof: 6 schemata over the two files (switch `AION_RHAND_MUT`), each killed by at least one case; sources restored by sha256.
The review pass (2026-09-29) closed the gaps the mutation review found with 4 more schemata under the same switch, each killed: 1002's
`getNextAvailableInstance` at re-entry and its flight transporter id, 1114's `var == 2` reward arm and its hate amount.
