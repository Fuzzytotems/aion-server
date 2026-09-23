# Phase 5 roadmap

The order phase 5 is finished in, decided by the user on 2026-09-23: **run the milestones below in order, one after another, without stopping to
ask between them.** Phase 5 is the server's own logic - the engine, the services and the client packets. Phase 6 (the ~1,500 scripted handlers:
Java quests, instance bosses, world AI) comes after it.

## Where phase 5 stood on 2026-09-23

| Measured | |
|---|---|
| `AION_UNPORTED` bodies across the P5 chunks | 1,863 |
| Client packets with no C++ file | 148 of 188 |
| Root AI handlers with no C++ file | 40 of 43 (the AI framework itself is complete) |
| Effect bodies the headers never declared | ~85, being surfaced by M5b-2 stage 1's header batch |
| Java lines in the phase-5 chunks | ~98,500 |

Largest areas by unported bodies: P5-10 teams 294, P5-12b world events 197, P5-08 player services 166, P5-06 quest engine 163, P5-09 loot and
economy 121, P5-13 instances and restrictions 118, P5-07 items 109, P5-12a siege 105, P5-11 legion and housing 86.

## The order

Each milestone ends with something a player can do, in roughly the order a new character in Poeta meets it.

| # | Milestone | What a player can do after it | Chunks, mainly |
|---|---|---|---|
| 1 | **M5b-2 abilities** | Cast skills; buffs and debuffs; casters play properly | P5-02a/b, P5-03/04 (the 34-class subset), P5-01 magical half |
| 2 | **M5b-3 loot and items** | Loot a corpse; use and move items | P5-09 (drop), P5-07, P5-13 (rest of restrictions) |
| 3 | **M5c vendors and economy** | Sell loot, buy potions, trade, mail, private store, crafting | P5-09 (trade, mail, craft, broker), P5-07 |
| 4 | **M5d quest engine** | **~4,184 XML-template quests come online** | P5-06 |
| 5 | **M5e training and progression** | Learn skills from trainers, class change | P5-08 (skill learn, class change, dialog) |
| 6 | **M5f travel and instances** | Teleporters, flight paths, the instance engine | P5-08 (teleport), P5-13 |
| 7 | **M5g groups** | Parties, alliances, find group - the largest single area | P5-10 |
| 8 | **M5h legion and housing** | Legions, houses, towns | P5-11 |
| 9 | **M5i siege and world events** | Fortresses, rifts, vortex, bases, world raids | P5-12a, P5-12b |
| 10 | **M5j the rest** | Remaining services and client packets; the chat server link | P5-14, P5-15/16 leftovers, P5-05's remaining root AI handlers |

**Why the quest engine is fourth and not later:** of the game's 8,043 quests, 4,184 are XML templates that need only the P5-06 engine, no
per-quest script. One 163-body milestone therefore brings about half of all questing online without waiting for phase 6 - the best ratio of payoff
to work on the list. It follows loot and vendors because quests hand out items and money. Moving it straight after M5b-3 is a reasonable
alternative if questing matters more than shops; the user has not asked for that.

## How each milestone runs

The shape M5a and M5b-1 established, which the M5b-2 plan follows:

1. **Plan.** A read-only analysis writes `docs/design/<milestone>-plan.md` in the shape of `m5b-plan.md`: the path end to end with file:line
   citations, work items, lanes, a gate specification, risks, and a split if it is too big. Its §12-style section separates what was measured
   from what was inferred.
2. **Review the plan** before any wave. M5b's first draft had 14 defects, one critical; a wave launched against it would have failed.
3. **Waves** of at most six lanes, chunks disjoint within a stage, every lane followed by an adversarial reviewer whose first job is to
   mutation-test the lane's new assertions. The integrator verifies (full build, full suite, lint, ownership) and commits between parts.
4. **A gate** (`gs.scenario.<milestone>`, plus a geo variant where geo matters) that plays the feature through a fake client, with packets read
   by decoders written from Java's `writeImpl`, never from the port.
5. **Re-green the earlier gates.** Wiring a system in wakes dormant code: M5b-1 predicted two hidden prerequisites and found eight.
6. **Offer the user a real-client session** at the end - their two sessions found the SiegeShield spawns, the targeting gap, the per-kill error
   and the leaked Player, none of which any test had produced - but do not stop the roadmap waiting for it.

## Decisions along the way

The user asked for the roadmap to run without stopping. Where a milestone plan leaves a decision open, the integrator takes the plan's
recommendation, records it in that plan's decisions table as "taken by the integrator under the standing instruction", and names it in the next
progress update so the user can reverse it. Decisions that are genuinely the user's - anything that changes what they asked for, anything that
loads their machine beyond the resource rules, and the design of the capacity tests - still go to them.

**Capacity testing is designed together with the user after M5b-2**, not by the integrator alone (m5b-plan.md, the note before §10): the user has
stress scenarios of their own, and the integrator brings proposals from where the port is most likely to break.
