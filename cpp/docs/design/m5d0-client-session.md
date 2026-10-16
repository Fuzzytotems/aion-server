# M5c complete real-client session, 2026-09-28

The owner played a frozen copy of commit `5bbd3551f` (M5c stage 3: the crafting gate case; M5c complete) from the play kit, with a real
4.8 client. The server log covers one process, 17:50 to 20:26 (`game-server/log/server_console.log`: started in 154 seconds, shut down
cleanly). Characters: `Rectangle`, `Illgotten`, `Lightwave`, `October`, `Grail` (account 4), `Circuit` and the Muse `Zatsuko` (account 2),
`Errant` and `Wish` (account 5). No Java server was run. The owner reported seven things. A read-only research pass compared Java and C++
on every path; the fixes were made on branch `fix/play-session-2026-09-28`.

**None of the four bugs is a port difference.** On each path the port matches Java statement for statement, and no `AION_UNPORTED` or
`AION_PARTIAL` body is on any of them. Reports 1, 2 and 4 are Java's own behaviour: Java HEAD (`024f4c0c8`, with #175 and #184) shows them
too, so the owner may want to report them upstream. They are fixed in C++ after the owner's precedent for a Java bug (M5c D7,
docs/design/owner-decisions.md: fix it and record the fix), each as a recorded deviation. Report 3 needed the client's packet sequence; the
owner captured it on 2026-09-29, and it is a Java bug as well, fixed the same way (R-3).

## The reports

| # | Report (character) | Outcome |
|---|---|---|
| 1 | Moving does not cancel a tuning (Rectangle) | **Java bug, fixed** (docs/deviations/P4-11b.md) |
| 2 | With two weapons, using an ability no longer starts auto-attack (Rectangle) | **Java regression of #175, fixed** (P4-11b.md); it was not a charge skill |
| 3 | Moving the sword from the main hand to the off hand works, but also says the inventory is full (Rectangle) | **Java bug, fixed 2026-09-29** after the client packet trace captured the swap (P5-15.md; the trace: P4-15.md, P4-01.md) |
| 4 | A second tuning can start while one runs; the first item stays greyed until a late "Canceled tuning of X" (Errant) | **Java bug, fixed** (P5-16.md) |
| 5 | Charge skills still do not work (Zatsuko, Muse) | Known: `CM_USE_CHARGE_SKILL` is not ported (m5e-plan.md C-04) |
| 6 | Circuit reached level 10 | **Confirmed**: finding F-1 of the 2026-09-25 session (m5c0-client-session.md) is fixed |
| 7 | Zatsuko stayed at level 9 after ascending through the simple class window | **Java-faithful**; the owner decided "Leave it as Java has it" |

**R-1. Moving does not cancel a tuning.** "Tuning" was most likely identification: `STR_MSG_ITEM_IDENTIFY_CANCELED` and
`STR_MSG_ITEM_REIDENTIFY_CANCELED` both read "Canceled tuning of %0.", and the log shows unidentified drops (Fiendish Sword, Fiendish
Warhammer, "of the Prairie" armour) and no tuning scroll. Both flows share the flaw. An item use attaches a one-time `ItemUseObserver` of
type ALL, and `notifyObservers` removes a one-time observer on the first notification it matches. That includes HP_CHANGED, whose hook the
item-use observer leaves empty. The first HP regeneration tick during the 5 s use (after a fight, about 5 times out of 6) therefore detached
the observer without aborting the use, and moving afterwards found nothing to cancel; at full HP the move cancels, as Java does. **Fix:** an
`ItemUseObserver` is matched without the three notifications it ignores (HP_CHANGED, ABNORMALSETTED, SUMMONRELEASE), in `ObserveController.cpp`
only; every other observer keeps Java's matching. Every ported item use attaches the same kind of observer, so soul binding,
decomposition, enchanting, extraction, socketing, quest start and reading items are fixed the same way. Gathering has the same flaw and is
left alone until `CM_GATHER` is ported.

**R-2. Dual wield, an ability no longer starts auto-attack.** There is no server-side auto-attack loop: the client sends one CM_ATTACK per
swing, the first right after the ability. Since Java #175 (`6ffedcd4f`, 2026-08-26), a hostile skill's `endCast` calls `enterCombat(true)`,
which writes `lastAttackMillis`, and `attackTarget` uses the same field as its anti-speed-hack throttle. So that first swing gets
`STOP_WITHOUT_MESSAGE` and the client stops auto-attack silently. Two weapons raise the attack speed (main + off hand / 4: 1750 for two
swords instead of 1400) and widen the refusal window from about 1.1 s to 1.45 s, past the end of most skill animations. **Fix:** the throttle
reads its own swing timestamp (`lastAutoAttackMillis`), written only by a swing it lets through; `enterCombat`, `isInCombat` and
`getLastCombatTime` keep #175's meaning, and two swings back to back are still refused. **Not a charge skill:** while a skill charges,
`canAttack` is false, so a charged ability would refuse CM_ATTACK as well. But the server warns once per process the first time a client
sends the unported `CM_USE_CHARGE_SKILL`, and the session's only warning is Zatsuko's at 19:56:29. Rectangle played from 18:11:56 until 18:32
in the same process, so Rectangle never released a charge skill.

**R-3. Main hand to off hand also says the inventory is full.** The message is `STR_UI_INVENTORY_FULL` (1300042), which Java sends for
**every** failed unequip (CM_EQUIP_ITEM.java:50-53), and the port does the same. The likely sequence for two one-handed weapons:
1. unequip the main-hand sword: Java's "retail like" rule moves **both** weapons to the cube (Equipment.java:239-247);
2. equip the sword into the off hand;
3. unequip the old off-hand weapon: it is no longer equipped, the unequip fails, the message comes;
4. equip the old off-hand weapon into the main hand.

The swap ends correctly, with a spurious message, as Java would. It could not be confirmed without the client's packets, so CM_EQUIP_ITEM's
answer was not changed then. Added instead: the C++-only config key `gameserver.network.trace.client_packets` (see "Next session").

**R-3, understood and fixed (2026-09-29).** The owner played with the trace on and swapped Rectangle's two one-handed weapons twelve times
(22:26-22:28). The client sends each swap as four CM_EQUIP_ITEMs within a few milliseconds: it unequips both weapons, then equips the new
main-hand weapon (slot mask 1) and the new off-hand weapon (2). The order of the two unequips follows the drag (the weapon of the slot dropped
on comes first, which fits the owner's report):
- the off-hand weapon dropped on the main hand unequips the **main-hand** weapon first (10 of the 12 sequences). The retail-like rule moves
  both weapons to the cube, the second unequip names an item that is no longer equipped, `unEquipItem` answers null, and CM_EQUIP_ITEM
  answers that null with `STR_UI_INVENTORY_FULL`; the two equips then complete the swap. This is the owner's "every time";
- the main-hand weapon dropped on the off hand unequips the **off-hand** weapon first (2 of 12): each unequip moves one weapon, no message.

So the sequence differs from the guess above (two unequips first, then two equips), but the cause is the one named there. Java HEAD
(`upstream/4.8`) has the same CM_EQUIP_ITEM.java and the same rule, so the owner may want to report it upstream. **Fix** (M5c D7's
precedent, docs/deviations/P5-15.md): CM_EQUIP_ITEM sends `STR_UI_INVENTORY_FULL` only when the item is still equipped after the failed
unequip, i.e. when the cube was the reason (a full cube, or fewer than two free slots for a main-hand weapon with a weapon in the off hand);
an unequip of an item that is not equipped fails silently. Both captured forms are replayed by `WeaponSwapTest` (tests/cm_ak), and the two cube
causes still answer with the message.

**R-4. A second tuning starts while one runs.** `CM_TUNE` has no busy guard in Java. The second use's `addTask(ITEM_USE)` cancels the first
task silently, the first item stays greyed, and the first use's observer stays attached. The next move, hit, equip or skill then aborts it,
printing "Canceled tuning of <first item>" long after the fact. **Fix:** CM_TUNE ends a running use first (`cancelUseItem`, as CM_CASTSPELL
and CM_EQUIP_ITEM do); for a tuning scroll only after `canAct` accepted it. The first item's CANCELED message and cancel animation come at
once, and nothing stale is left.

**R-5. Charge skills do not work.** As in m5c0-client-session.md F-4: the log says "sent CM_USE_CHARGE_SKILL, which is not ported yet"
(Zatsuko, 19:56:29). It is m5e-plan.md W-09 / C-04, with the release packet `CM_USE_CHARGE_SKILL`; the charge engine itself is ported.

**R-6. Circuit reached level 10.** F-1 of the 2026-09-25 session (a seeded Daeva logged in at level 9 because `QuestState`'s restore threw)
is fixed by M5d's quest engine: this session's log has no "Could not restore QuestStateList" line.

**R-7. Zatsuko stayed at level 9 after the simple class window.** Java's simple route (`ClassChangeService.changeClassToSelection` ->
`setClass(..., updateDaevaStatus)`) changes the class, completes quest 1006 / 2008 without its reward and makes the character a Daeva, but
awards no experience. A level-9 character with a full bar (the non-Daeva cap, PlayerCommonData.java:276-281) stays at level 9 until its next
experience gain. The owner decided: "Leave it as Java has it". The retail ascension quest rewards exp 73200 (quest 1006
`game-server/data/static_data/quest_data/quest_data.xml:65-66`, quest 2008 :9299-9300), which comes with the retail route, M5f stage 3.

## Side findings (verified against the code)

- **`PlayerLifeStats::sendGroupPacketUpdate` is `AION_UNPORTED` for a player in a team** (PlayerLifeStats.cpp:67-72; Java
  PlayerLifeStats.java:62-63 hands the player to `TeamStatUpdater`). `onHpChanged` calls it (:48) before `CreatureLifeStats::onHpChanged`
  (:54), so for a grouped player the throw skips `onDie` and `notifyHPChangeObservers` on every HP change. Reachable only once groups work
  (M5g, P5-10).
- **`CM_GATHER` and `CM_BIND_POINT_TELEPORT` are not ported yet.** Both are registered without a class (ClientPacketInfo.gen.inc:35 and :201),
  and the log warned once each (Illgotten 17:56:13, Lightwave 18:44:21). Gathering comes after quests (M5c D10 / M5d D13); the bind point
  teleport with M5f.
- **`TeleportService::showMap` is `AION_UNPORTED`** (TeleportService.cpp:296-298), reached twice from `CM_DIALOG_SELECT` ->
  `DialogService::onDialogSelect` (DialogService.cpp:286) at 18:49:04 (Lightwave, a teleporter's map). These are the session's only ERROR
  lines. M5f.

## Next session: what to capture for R-3

(Done on 2026-09-29: the trace below captured the swap, and R-3 is fixed. The key stays available for later reports. A second C++-only key,
`gameserver.network.trace.server_packets`, logs when the server sends a named server packet, e.g. `SM_GATHERABLE_INFO,SM_NPC_INFO`, as
`Server packet trace: sent [017] SM_GATHERABLE_INFO to Rectangle`; with `CM_MOVE` in the client key it times the late Sanctum crafting benches
of that session against the owner's movement (docs/deviations/P4-15.md, P4-01.md).)

1. Play a build from `fix/play-session-2026-09-28` or later (the trace does not exist in `5bbd3551f`).
2. Set this key in the game server's `config/mygs.properties` (C++ server only; no shipped properties file names it, because `config/`
   belongs to the Java tree; docs/deviations/P4-01.md documents it):

   ```properties
   gameserver.network.trace.client_packets = CM_EQUIP_ITEM
   ```

   Add `CM_ATTACK,CM_CASTSPELL` to see when the client sends its swings around an ability. Each run of a named packet logs one INFO line,
   such as `Client packet trace: Rectangle sent CM_EQUIP_ITEM [action=1, slot=1, itemObjId=...]`. Actions: 0 equip, 1 unequip, 2 switch
   weapon sets. The slot is the slot mask: 1 main hand, 2 off hand.
3. With a one-handed weapon in each hand (and, separately, with only a main-hand weapon), drag the main-hand weapon to the off hand. Note
   the time and whether "inventory full" appeared.
4. Send the `Client packet trace` lines of that minute from `game-server/log/server_console.log`. If they show the four-step sequence above,
   the fix is to send the message only when the cube is the reason. That deviates from CM_EQUIP_ITEM.java:52-53 and changes the
   expectation of `EquipDeleteRunTest.AnUnequipThatFailsIsAnsweredWithInventoryFull`.

Remove the key afterwards. The empty default logs nothing.

## Verification of the fixes

The tests, the mutation evidence and the header requests (`psf-1`..`psf-3`, docs/porting/header-requests.md) are in the "Play-session
fixes 2026-09-28" sections of docs/deviations/P4-11b.md, P5-16.md, P4-15.md, P4-01.md, P5-15.md and P5-07.md; R-3's fix of 2026-09-29 is in
P5-15.md's "Play-session fix 2026-09-29" section (9 mutants, all killed). The first schemata build had
12 mutants, all killed. The review's own run found 14 non-equivalent survivors, so the second build added six cases and 21 more mutants
(`AION_PSF2_MUT`), all killed. It pins every notification the item-use observer still handles, a swing that enters combat, a refused
swing that does not move the throttle, CM_TUNE's do-nothing paths, and the trace's place before `runImpl` and after `isValid`.
