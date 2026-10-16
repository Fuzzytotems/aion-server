# M5c stage 0 and M5e overlap real-client session, 2026-09-25

The user played a frozen copy of commit `1c3c336f9` (M5c stage 0, the npc-leak fix and the M5e effect classes; unit suite 3,645 of 3,645,
gates 46 of 46) from the play kit in `D:\aion-dev\play`, with the M5b-3 play profile. The kit's database scripts seeded test characters
while they were offline (`make-daeva.ps1`, `give-item.ps1`, `set-kinah.ps1`). Characters: the Mage `Fuzzytotem`, the Gladiators `Kiltsk`
and `Circuit` (seeded), the Muse `Zatsuko` and others. No Java server was run; every finding below is read from the server log.

## What the session proves

| | |
|---|---|
| Npc dialogs (M5c stage 0) | Dialog windows open, the npc functions answer (last session's unported `CM_SHOW_DIALOG`, m5b3-client-session.md S-2, is closed) |
| Manastones | A manastone socketed into a Greatsword Form in the cube - **confirmed by the user** |
| Enchanting | An L10 Enchantment Stone enchanted the Greatsword Form - **confirmed** |
| Extraction | Extraction Tools broke a Greatsword Form into 3 Alpha Enchantment Stones (`ITEM_LOG` 12:41:14: `Deleted 100900002 Greatsword Form`, `166000191 [Alpha Enchantment Stone] ... (count: 3)`) - **confirmed** |
| Soul healing | Palaemon (Verteron) healed Kiltsk after a death: `recoverexp` 287 → 0 and the experience came back - **confirmed** |
| Return | Works inside one map (Fuzzytotem in Poeta) |

## Findings

**F-1. A SQL-seeded Daeva logs in at level 9.** Kiltsk and Circuit were seeded as level-10 Gladiators (class, exp 126,069, quest 1006
`COMPLETE`, the recipe of m5c-plan.md §11 step 11 and m5e-plan.md D8) and logged in at level 9 with a full bar. `PlayerQuestListDAO`'s
restore throws at login ("Could not restore QuestStateList data for player: 104243": `QuestState::setPersistentState` is `AION_UNPORTED`), so
`PlayerCommonData.updateDaeva` never sees quest 1006, `isDaeva` stays false, and Java's non-Daeva cap (`PlayerCommonData.setExp`:
`maxLevel` 10 unless the player is a Daeva) keeps level 9; any exp gained online clamps back to it. **Every gate or client step that seeds a
Daeva needs M5d's `QuestState` restore path first** (m5c-plan.md C19 and A-03c, m5e-plan.md D8 and §11 steps 2-10). M5d's quest-engine lane
has ported it in its worktree; it is not merged.

**F-2. A character moved to another map by SQL cannot bind-revive or Return.** Kiltsk, seeded into Verteron with its bind point still in
Poeta, released after a death and stayed where it died; Return did nothing. `CM_REVIVE` → `PlayerReviveService::bindRevive` →
`TeleportService::moveToBindLocation` → `teleportTo` → `SpawnTask::run` → `InstanceService::onLeaveInstance`, which is `AION_UNPORTED`
(`InstanceService.cpp:171`; log 12:49:20). Cross-map travel is M5f. m5e-plan.md §11 step 9 ("relocate to Verteron by SQL") has the same
limit until then: fight, but do not die or Return there.

**F-3. A database edit made while the client is connected is lost.** The game server loads every character of an account when the client
connects (`AccountService.getAccount` → `loadAccount`), uses that copy through character select and saves it back at logout, as Java does;
inventory rows are read fresh at enter world and survive. Two seedings made while the client sat at character select (Circuit at 12:54,
Kiltsk's move at 13:01) were overwritten at logout. Not a port bug; the play kit's scripts now refuse while `aion.bin` runs, and a gate
that seeds a character row must do it while that account is disconnected.

**F-4. Charged skills do not fire.** Zatsuko, a level-9 Muse (ARTIST), released a charge skill; the log says "sent CM_USE_CHARGE_SKILL, which
is not ported yet. Packet won't be instantiated" (17:55:36). The charge engine (`CreatureController::useChargeSkill`, M5b-2) is ported;
the release packet has no file. It is m5e-plan.md W-09 and item C-04, which assumed only Daevas have charge skills; a starting class has
one before level 10, so this reaches the start maps. Not pulled forward (the user's decision, 2026-09-25).

**F-5. Other client packets the user sent that are not ported** - all expected: `CM_BIND_POINT_TELEPORT` (obelisk binding and its
teleport, M5f), `CM_TUNE` (identifying an item, m5c-plan.md K-02, stage 1), `CM_GATHER` (gathering, m5c-plan.md C-04, stage 2, D10), and
the teleport map (`TeleportService::showMap`, W-08/W-10, M5f).

**Harmless log lines.** Seeded items that are used up log `IDFactory - Couldn't release ID ... because it wasn't taken`: the kit inserts
them above `0x07000000` while the server runs, so the id factory never marked them taken. The quest row the seeding writes logs the F-1
error at every login of that character until M5d.

## Not exercised

Buying, selling, trade, mail, the private store, cube expansion and identification (M5c stage 1, not in this build); crafting (stage 2);
quests (M5d); the class change and level-10+ skills (M5e stage 1, and F-1); group play; the npc-leak check at the juvenile sparkie
(1135, 1013) was not reported.
