# M5b-3 work plan (loot and items)

> **Status:** plan **rev 2**, 2026-09-23 — rev 1 after its adversarial review (22 findings, 2 high; §14 lists them and what changed).
> **Stage 0 applied 2026-09-24** on `27726d32c`: §15 records what landed, the I-03 leases and what §7 got wrong. **Stage 1 integrated
> 2026-09-24** (L-01 with G-05, committed as `4867fbc44`): §16 records what landed, the re-derived counts, the gates before and after, and
> what stage 2 must know. **Stage 2 (the gate) 2026-09-24**, not yet committed: §17 records `gs.scenario.m5b3` and `_geo`, the camp-fire
> measurement, the corrections to §10, the mutation runs and M5b-2's S14/S10 setup fixes; **§18 records the fixes after the review of stage
> 2** (S10/S14's real cause, a magical critical Flame Bolt; Y15's untouched point; Y2's RY2 run; the documentation corrections).
> **The regate of stage 2, 2026-09-24 (§19):** the full build, the unit suite and all eleven gates passed; **M5b-3 is complete**. A
> **read-only** analysis over HEAD `c1edb0afb` ("M5b-2 stage 1 part 2: the cast engine and the effect
> core") plus the uncommitted M5b-2 part 3 lanes in the working tree (P5-01, P5-03, P5-04), against the Java 4.8 tree. **Nothing was compiled,
> built or run for this plan**; every C++ statement below comes from reading the two trees, from `tools/porting/chunks.py files|owner` and from
> counting `AION_UNPORTED(` / `AION_PARTIAL(` call sites. Every data number comes from a throw-away parse of the Java data files (scripts in the
> session scratchpad, re-derivable; §12 says which) and **must be taken over by the `tools/oracle` commands of G-01 before a gate asserts it**.
>
> It follows the shape of [m5b2-plan.md](m5b2-plan.md) and [m5b-plan.md](m5b-plan.md): the paths end to end with file:line, work items, lanes,
> a gate whose every assertion says what it proves, what it cannot prove and which mutation it kills, risks, stages, a real-client checklist
> and a measured/inferred split. Ownership follows `tools/porting/chunks.py`; header changes follow [hub-headers.md](hub-headers.md) §14.
> Inputs: [phase5-roadmap.md](phase5-roadmap.md) row 2, m5b-plan.md D5/O-05..O-07/§9, m5b2-plan.md D6/D11/D13 and §10,
> `docs/deviations/P5-02a.md` "Reachable after part 2", [m5a-client-session.md](m5a-client-session.md) F-2,
> [m5b-client-session.md](m5b-client-session.md) S-1, `tests/scenario/m5b_partial_allowlist.txt`, `generated/concurrency/{cycles,fieldmap}.toml`.

---

## 1. Summary

**The item model is ported; the services that move items are not.** M5a had to port storage, the item object and every item packet to make
enter world work, so the half of loot that *holds* and *shows* an item is done. What is empty is the half that *changes* it:

| Already ported, 0 `AION_UNPORTED` | Evidence |
|---|---|
| `Storage`, `PlayerStorage`, `ItemStorage`, `IStorage` (add, put, remove, delete, kinah, counts) — **chunk P4-13 has 0 unported bodies** | `model/items/storage/*.cpp`; `Storage.cpp:92-230` |
| `Item` (all 126 Java methods), `DropNpc` (29), `DropItem` (25), `Drop`, `DropGroup`, `NpcDrop.dropCalculator`, `DropModifiers` | `model/gameobjects/{Item,DropNpc}.cpp`, `model/drop/*.cpp` (P4-11a / P4-13) |
| `Equipment` — every Java method but the soul-bind request, including `equipItem`, `equip`, `unEquipItem`, `unEquip`, `switchHands`, `onLoadApplyEquipmentStats` | `model/gameobjects/player/Equipment.cpp:159-760` (P4-12); the one `AION_UNPORTED` is the soul-bind accept (`:138`) |
| `ItemEquipmentListener::onItemEquipment`, `addWeaponStats`, `recalculateItemSet`, `addStoneStats` | `model/stats/listeners/ItemEquipmentListener.cpp` (P5-01), 2 of 9 bodies unported |
| `ItemFactory`, `ExpireTimerTask`, `ItemUseObserver`, `ObserveController::notifyItemuseObservers` / `abortItemUseObservers` | `services/item/ItemFactory.cpp`, `taskmanager/tasks/ExpireTimerTask.cpp`, `controllers/observer/ItemUseObserver.cpp` |
| `DropService::unregisterDrop`, `closeDropList`, `see`; `NpcController::petLoot`, `findPetForLooting`, the `registerDrop` call; `RespawnService::scheduleDecayTask` | `services/drop/DropService.cpp:68-125, 163-172`; `controllers/NpcController.cpp:165-280`; `services/RespawnService.cpp:138-147` |
| **Every item and loot server packet**: `SM_LOOT_STATUS`, `SM_LOOT_ITEMLIST`, `SM_INVENTORY_ADD_ITEM`, `SM_INVENTORY_UPDATE_ITEM`, `SM_INVENTORY_INFO`, `SM_DELETE_ITEM`, `SM_CUBE_UPDATE`, `SM_WAREHOUSE_ADD_ITEM`/`UPDATE_ITEM`/`INFO`, `SM_DELETE_WAREHOUSE_ITEM`, `SM_ITEM_USAGE_ANIMATION`, `SM_ITEM_COOLDOWN`, `SM_UPDATE_PLAYER_APPEARANCE`, `SM_GROUP_LOOT` | all present under `network/aion/serverpackets/`, **0 `AION_UNPORTED` in each** (measured) |
| The skill path an item uses: `SkillEngine::getSkill(…, ItemTemplate*)`, `Skill`'s ITEM arms (`isCombatActivated`, `SM_ITEM_USAGE_ANIMATION`, `payCastCosts`' `decreaseByObjectId(DEC_ITEM_USE)` + `startCooldown`) | `skillengine/model/Skill.cpp:600, 870-908` — **P5-02a and P5-02b have 0 `AION_UNPORTED`** since `c1edb0afb` |
| `PlayerRestrictions::canUseSkill`, `canAttack`, `checkFly` | `restrictions/PlayerRestrictions.cpp:51-190` (M5b-1, M5b-2) |
| The static data: `GlobalDropData`, `GlobalNpcExclusionData`, `GlobalRule` accessors, `EventService::getActiveEventDropRules` | `dataholders/GlobalDropData.cpp`, `generated/.../globaldrops/GlobalRule.h:50-69`, `services/event/EventService.h:85` |

**What is empty is five holes of very different sizes:**

| # | Hole | Where | Size (measured) |
|---|---|---|---|
| 1 | **The drop engine.** `DropRegistrationService` 26 bodies + the M5b-1 partial (`registerDrop`, `DropRegistrationService.cpp:43`), the global-rule evaluator (10 predicates), `DropService` 13, `DropDistributionService` 4. And 4 `QuestService` quest-drop bodies in **P5-06** that `registerDrop` calls on every kill. | **P5-09**, P5-06 | 43 + 1 partial + 4, ~1,110 Java LOC on the solo path |
| 2 | **The item services.** `ItemService` 11 (every `addItem`), `ItemPacketService` 9 (every storage packet a change sends), `ItemMoveService` 3, `ItemSplitService` 4, `ItemRestrictionService` 3, `ItemSocketService::socketGodstone`, `StigmaService::notifyEquipAction` | **P5-07** | 32 of P5-07's 109 sites, ~900 Java LOC |
| 3 | **The item-action API does not exist.** `AbstractItemAction.h` declares **neither `canAct` nor `act`**; `ItemActions.h` has **no `getItemActions()`**; 30 of the 32 bound action classes are shell headers with no `.cpp`. **143 Java method bodies across the 36 action files are undeclared** — invisible to any `AION_UNPORTED` count (lesson 1, §2.3). | **P5-07** | a 32-class layout header batch; 3 bodies to port (`SkillUseAction`) |
| 4 | **The effect classes an item reaches.** Potions and the starter items need 6 classes, godstone procs 5, material skills 3 more — **14 classes outside M5b-2's 38**, 38 sites + 3 undeclared inner bodies | **P5-03** (4), **P5-04** (10) | 38 + 3, 618 Java LOC |
| 5 | **The client packets.** `CM_START_LOOT`, `CM_LOOT_ITEM`, `CM_USE_ITEM`, `CM_MOVE_ITEM`, `CM_EQUIP_ITEM`, `CM_DELETE_ITEM`, `CM_SPLIT_ITEM`, `CM_REPLACE_ITEM`, `CM_MANASTONE` have **no C++ file** (43 of the 190 Java `clientpackets` files exist) | **P5-15**, **P5-16** | 9 packets, 18 bodies, 550 Java LOC |

plus 2 `PlayerRestrictions` bodies (**P5-13**), 2 `ItemEquipmentListener` bodies and a `DropRewardEnum` companion (**P5-01**), the
soul-bind request of `Equipment` (**P4-12**) and `LegionService::addWHItemHistory` (**P5-11**, reached by every cross-storage split, §2.8 E-9).

**The size, honestly (§2.3, §3):** the required scope is **~145 bodies** — 112 `AION_UNPORTED` sites + the `registerDrop` partial + 14 undeclared
bodies + 18 new packet bodies — over **~3,800 Java lines**, plus a **layout header batch of 64 declarations** in 32 item-action classes of which
only `SkillUseAction` gets a body. That is **smaller than m5b-plan.md §9 said** (~250 sites, ~5,000 LOC): §9 counted all of P5-09 (121) and
P5-07 (109), and ~160 of those sites are mail, broker, trade, craft, enchant, stigma, warehouse and cube expansion, which are M5c's. **But §9 also
missed ~70 bodies outside those two chunks** — the 14 effect classes (41), the 18 packet bodies, 4 P5-06 quest-drop bodies, the P5-13,
P5-01 and P4-12 bodies behind `CM_EQUIP_ITEM` and the P5-11 body behind `CM_SPLIT_ITEM` (§2.8) — **and the item-action API, which no count
shows at all**. **One implementation wave plus a gate wave**, not "1-2 waves" — but the wave's two long lanes are **~8-10 agent-days each**,
not 3-4 (§6, rev 2).

**Five findings shape the plan.**

1. **Closing `registerDrop` turns the M5b gate red on the first run, and wiring drops in makes it flaky after that** (§2.8 E-1, D4, risk 1).
   *Red, deterministically:* R3 does `ASSERT_TRUE(hitsByEntry.contains(".../DropRegistrationService.cpp:43"))` and counts that site's hits
   (`M5bScenarioTest.cpp:2130-2136`), Q2 does `EXPECT_EQ(dropNpc->created, 0)` (`:2302-2305`), and Q1 asserts the §A allow-list row
   (`m5b_partial_allowlist.txt:46-50`) is hit at least once — none of which survives the partial's removal, at any drop rate.
   *Flaky:* today a corpse always decays after 2 s (`RespawnService.IMMEDIATE_DECAY`, RespawnService.java:34, 42-50), because the drop map is
   empty. Once drops register, **a corpse that dropped something stays 300 s** (`WITH_DROP_DECAY`, :35) — and the M5b gate's monster 210663
   drops something on **58.2 %** of kills (§2.4) and respawns after 20 s (`210010000_Poeta.xml:640`). That turns R4 (`SM_DELETE` of the corpse
   within respawn + 25 s, `:1650-1693`) and Q3 (npc live ≤ baseline, `:2321-2350` — a retained corpse plus its respawn is exactly what it catches)
   into coin flips. **The fix is a configuration key Java itself provides**, `gameserver.rates.drop = 0` in the earlier gates' profiles (D4) —
   the M5b-1 `soulsickness.disable` pattern — plus R3, Q2 and the allow-list rewritten, **all in stage 1 and merged in the same integrator
   commit as the `registerDrop` closure** (rev 2: rev 1 had scheduled the re-green for stage 2, which left every M5b run red for a whole stage).
   `DropNpc` moves from a zero row to a bounded row in the same stage (D5).
2. **M5b-3's `ItemPacketService` is reachable today, and the user's own characters will hit it this week** (§2.8 E-13/E-14). The starter item
   "Administrator's Boon – 3-Day Pass" (164002039, `expire_time="4321"` minutes, item_templates.xml:834664) expires ~3 days after character
   creation; `ExpireTimerTask` → `Item::onExpire` → `Storage::delete_` → `ItemPacketService::sendItemDeletePacket` → **`AION_UNPORTED`**
   (`ItemPacketService.cpp:23-24`, `Item.cpp:567-590`). The M5a-session Mage (created 2026-09-21) and the M5b-session Warrior (2026-09-22) reach
   it on 2026-09-24 and 2026-09-25. And **skill 245 *Bandage Heal*, which every character autolearns, consumes a Bandage through the same unported
   packet** (skill_templates.xml:3080-3082), so the M5b-2 real-client session can hit it too.
3. **`CM_EQUIP_ITEM` reaches four unported bodies in three other chunks** (lesson 2): `PlayerRestrictions::canChangeEquip` (P5-13),
   `StigmaService::notifyEquipAction` (P5-07, called for *every* item, Equipment.java:135), `ItemPacketService::updateItemAfterEquip` (P5-07) and
   `ItemEquipmentListener::onItemUnequipment` (P5-01). A plan that sizes "equip" from `Equipment.cpp`'s one unported body sees none of them.
4. **The drops of the start maps need the whole global-rule evaluator, not a table.** Poeta's and Ishalgen's monsters have **no custom drop and
   no npc-specific rule** except a few named ones: every sparkie, kerub and brax drops through the 2,196 `gd_rule` elements of
   `global_drops/rules/**`, and **BEAST monsters drop no kinah at all** (the Kinah rule's race list, rules_commons.xml:3-24, has no BEAST) —
   including the M5b gate's own 210663. Kinah needs a different monster (§2.4).
5. **Godstones and material skills are both cheap and both already reachable.** Godstone procs are ported (`CreatureController.cpp:316`) and need
   **5 effect classes** (`ProcAtkInstantEffect`, `PoisonEffect`, `SilenceEffect`, `BlindEffect`, `ParalyzeEffect`); 4.8 sockets a godstone with
   `CM_MANASTONE` action 4, **no npc** (AionClientPacketFactory.java:119). And **Poeta has ~98 bonfire placements whose mesh material casts
   skill 8302 *Flame Strike*** — a `ProcAtkInstantEffect` — so with geo on (the user's setup) walking into a camp fire throws today (§2.6; the
   review's independent parse found 97, and the G-01 oracle settles it).

---

## 2. The paths, end to end

### 2.1 A kill to an item in the cube

"ported" means the body exists with no `AION_UNPORTED`.

| # | Step | Java | C++ today | Chunk |
|---|---|---|---|---|
| 1 | `NpcController.onDie`: `ask(REWARD_LOOT)`, `ask(REWARD_AP_XP_DP_LOOT)` → `doReward()`; after the try, `petLoot` and `RespawnService.scheduleDecayTask(owner)` | NpcController.java:137-169 | ported, `NpcController.cpp:165-199` | P4-11b |
| 2 | `doReward`: for the most-damage winner `if (attacker.equals(winner) && ask(REWARD_LOOT)) registerDrop(npc, player, player.getLevel(), null)` | NpcController.java:244 | ported, `NpcController.cpp:277-279` — **Java passes `null` group members, the frozen signature takes a vector and passes `{}`** (D11) | P4-11b |
| 3 | `registerDrop`: `CUSTOM_NPC_DROP.getNpcDrop`, `initDropNpc` (a `DropNpc` in `dropRegistrationMap`, the allowed looters), `createDropModifiers`, `npcDrop.dropCalculator`, `currentDropMap.put`, `QuestService.getQuestDrop`, the global rules twice (data, then **`EventService.getActiveEventDropRules()`**, :96-98), `instanceHandler.onDropRegistered`, `DROP_REGISTERED` AI event, **`SM_LOOT_STATUS(LOOT_ENABLE)` to every allowed looter**, `DropService.scheduleFreeForAll` | DropRegistrationService.java:59-109 | **`AION_PARTIAL`, the whole body** (`DropRegistrationService.cpp:24-44`, §A row of `m5b_partial_allowlist.txt:50`, hit exactly once per kill: M5b R3). The two hooks are inline no-ops already (`GeneralInstanceHandler.h:136`, `AITemplate.h:102`); `EventService::start`/`collectDropRules` are ported (`EventService.cpp:62, 116, 230`) | **P5-09** |
| 4 | `createDropModifiers` → `calculateBoostDropRate` (`BOOST_DROP_RATE`, `DR_BOOST`, repose, salvation, a PALACE house, `Rates.get(killer, DROP_RATES)`) and `getReductionDropRate` (`DropRewardEnum.dropRewardFrom(npcLevel − highestLevel)`) | DropRegistrationService.java:111-120, 198-219; DropRewardEnum.java | unported (`:46-68`); `DropRewardEnum` is a generated enum with **no companion** (`generated/.../utils/stats/DropRewardEnum.h`; `XPRewardEnumInfo.h` is the pattern) | P5-09, **P5-01** |
| 5 | `QuestService.getQuestDrop(droppedItems, index, npc, groupMembers, looter)` — **writes into `droppedItems`** | QuestService.java:666-760 | **`AION_UNPORTED`**, and its frozen signature takes `const std::unordered_set<Ptr<DropItem>>&` (`QuestService.cpp:321-324`) — it cannot add to it. The quest drops themselves **are** loaded (`QuestEngine.cpp:84-90`). One level deeper, `isQuestDrop` calls `QuestState::getQuestVarById` (`QuestState.cpp:40-42`, → `QuestVars::getVarById`, both `AION_UNPORTED`) for a quest in `START` whose drop has a collecting step (QuestService.java:754-760) — unreachable while no quest can start (M5d), named in L-04 | **P5-06** |
| 6 | The global rules: `hasGlobalNpcExclusions`, `isAllowedDefaultGlobalDropNpc`, per rule `calculateEffectiveChance` → `Rnd.chance() >= chance` → `addDropItems` → `collectDrops` (`checkRuleRestrictions`: race, maps, worlds, ratings, races, tribes, zones, npcs, npc groups, excluded npcs; item race and `min_diff..max_diff`; `Chance.selectElement` down to `max_drop_rule`) → `regDropItem` / `getItemCount` (kinah: `count *= level × (rank × rating)^6`) | DropRegistrationService.java:167-196, 221-474 | unported (`:54-148`); `Chance::selectElement` is ported (`model/Chance.h:25-45`) | P5-09 |
| 7 | `DropService.scheduleFreeForAll`: after 240 s `DropNpc.startFreeForAll()` and `SM_LOOT_STATUS(LOOT_ENABLE)` to the sighted players of the other race | DropService.java:54-71 | unported (`DropService.cpp:64-66`); fieldmap callback `DropService@L55:44` captures only the npc id | P5-09 |
| 8 | Decay: `scheduleDecayTask` picks **`IMMEDIATE_DECAY` 2 s when the drop set is empty, `WITH_DROP_DECAY` 300 s otherwise** | RespawnService.java:34-50 | ported, `RespawnService.cpp:138-147` — **this is the step that moves every earlier gate** (§2.8 E-1) | P4-10 |
| 9 | Client `CM_START_LOOT(targetObjectId, action)`: 0 → `requestDropList`, 1 → `closeDropList` | CM_START_LOOT.java:35-54 | **no C++ file** | **P5-16** |
| 10 | `requestDropList`: close a previous list, `isAllowedToLoot` (else `STR_LOOT_NO_RIGHT`), `isBeingLooted` (else `STR_LOOT_FAIL_ONLOOTING`), `setLootingPlayer`, cancel the `DECAY` task and remember its delay, `SM_LOOT_ITEMLIST`, `SM_LOOT_STATUS(OPEN_DROP_LIST)`, state `LOOTING`, `SM_EMOTION(START_LOOT)` broadcast | DropService.java:90-136 | unported (`:74-76`); `closeDropList` is ported (`:78-125`) | P5-09 |
| 11 | Client `CM_LOOT_ITEM(targetObjectId, index)` → `requestDropItem(player, npcObjId, index)` | CM_LOOT_ITEM.java:23-34 | **no C++ file** | **P5-16** |
| 12 | `requestDropItem`: find the index under `synchronized (dropItems)`, `isAllowedToLoot`, the lore-item check, team arms, **kinah → `ItemService.addItem`**, solo → `ItemService.addItem(player, itemId, count)`, remove a fully taken entry, `announceDrop`, the pet auto-sell, `resendDropList` | DropService.java:276-410 | unported (`:135-141`) | P5-09 |
| 13 | `ItemService.addItem` → kinah: `inventory.increaseKinah`; stackable: the POWER_SHARDS equipment arm, merge into stacks, then `ItemFactory.newItem` + `inventory.add`; non-stackable: `newItem`, `ExpireTimerTask.registerExpirable`, `predicate.changeItem`, `inventory.add(newItem, addType)`; `STR_MSG_DICE_INVEN_ERROR` when full | ItemService.java:70-177 | **all 11 unported** (`ItemService.cpp:26-78`); `ItemFactory` and `ExpireTimerTask` ported | **P5-07** |
| 14 | `Storage.add` / `increaseItemCount` / `increaseKinah` → `ItemPacketService.sendStorageUpdatePacket` / `sendItemPacket` → `SM_INVENTORY_ADD_ITEM` / `SM_INVENTORY_UPDATE_ITEM` + `SM_CUBE_UPDATE.cubeSize` | Storage.java; ItemPacketService.java:167-228 | `Storage` ported and calls the service (`Storage.cpp:135, 160, 186, 210, 229`); **all 9 `ItemPacketService` bodies unported** (`ItemPacketService.cpp:7-40`) | P4-13 / **P5-07** |
| 15 | `resendDropList`: a shorter `SM_LOOT_ITEMLIST`, or with nothing left `SM_LOOT_STATUS(CLOSE_DROP_LIST)`, state `ACTIVE`, `SM_EMOTION(END_LOOT)` and **`npc.getController().delete()` at once** | DropService.java:423-440 | unported (`:147-149`) | P5-09 |

### 2.2 Using, moving and equipping

| # | Step | Java | C++ today | Chunk |
|---|---|---|---|---|
| U1 | `CM_USE_ITEM(uniqueItemId, type, [targetItemId \| syncId \| indexReturn])`: stop spawn protection, find the item (and a target item or house object), cancel a cast, `notifyItemuseObservers`, **`PlayerRestrictions.canUseItem`**, `getActions().getItemActions()`, `QuestEngine.onItemUseEvent` (skipped for `QuestStartAction`), `STR_ITEM_IS_NOT_USABLE`, then **`canAct` for each action and `act` for those that can** (`DyeAction`, `MultiReturnAction`, `InstanceTimeClear` take an extra parameter) | CM_USE_ITEM.java:38-125 | **no C++ file** | **P5-16** |
| U2 | `canUseItem`: prison, about to die, `CANT_ATTACK_STATE`, transform, private store, **`hasCooldown(item)`**, item race, no actions and not a quest item, gender, class, required and max level, use area, activation race | PlayerRestrictions.java:277-369 | **`AION_UNPORTED`** (`PlayerRestrictions.cpp:200-202`); every callee is ported (`Player::hasCooldown` `Player.cpp:771`, `Creature::isInsideItemUseZone` `Creature.h:281`, `TransformModel::cantUseItems` `TransformModel.h:86`, `QuestEngine::isRegisteredQuestItem` `QuestEngine.cpp:728`) | **P5-13** |
| U3 | `AbstractItemAction.canAct(Player, Item parent, Item target, Object... params)` / `act(...)` | AbstractItemAction.java:15-35 | **not declared** — `AbstractItemAction.h:8-11` holds only the generated block (`javaClassName`); `ItemActions.h:16-27` has 3 typed lookups and **no `getItemActions()`** (the member is private in `ItemActions.xml.inc`) | **P5-07** |
| U4 | `SkillUseAction.canAct`: map check, `SkillEngine.getSkill(player, skillid, level, target, itemTemplate)`, the transform/summon refusals, `skill.canUseSkill(CAST_START)`, the full-health refusal; `act`: `setItemObjectId`, `useSkill()` | SkillUseAction.java:43-110 | **shell header, no `.cpp`**, 3 undeclared bodies | P5-07 |
| U5 | The cast of an item skill: `Skill.startCast` (`SM_ITEM_USAGE_ANIMATION` unless combat-activated), `endCast` → `payCastCosts` → `inventory.decreaseByObjectId(itemObjId, 1, DEC_ITEM_USE)` + `startCooldown(item)` → effects | Skill.java:497-525, 756-800; Player.java:1003-1009 | ported (`Skill.cpp:600, 870-908`); the decrease reaches **`ItemPacketService::sendItemPacket` (unported)** | P5-02a / P5-07 |
| U6 | The potion effects: `ProcHealInstantEffect` (HP, `TYPE.HP`), `ProcMPHealInstantEffect` (MP), then `HealEffect` / `MPHealEffect` over time | AbstractHealEffect.java:15-47; ProcHealInstantEffect.java:17-40 | `HealEffect`, `MPHealEffect` ported by M5b-2; **`ProcHealInstantEffect` 4 and `ProcMPHealInstantEffect` 4 unported** | **P5-04** |
| M1 | `CM_MOVE_ITEM(itemObjId, source, destination, slot)` → `ItemMoveService.moveItem`: same storage → slot only (**no packet**); else `ItemRestrictionService.isItemRestrictedTo/From`, trading, shutdown → `sendItemUnlockPacket`; `slot == -1` merges stacks (`ItemSplitService.mergeStacks`); full → message + unlock; else `remove`, `sendItemDeletePacket(MOVE)`, `setEquipmentSlot`, `add` | CM_MOVE_ITEM.java:25-36; ItemMoveService.java:25-84 | no C++ packet; `ItemMoveService` 3, `ItemRestrictionService` 3, `ItemSplitService` 4 all unported | P5-16, **P5-07** |
| M2 | `CM_REPLACE_ITEM` → `switchItemsInStorages` (swap two items across storages: both `SM_DELETE_*`(MOVE) first, then both adds — ItemMoveService.java:86-125); `CM_SPLIT_ITEM` → `ItemSplitService.splitItem`, which calls **`LegionService.addWHItemHistory` on every cross-storage split or merge** (ItemSplitService.java:80, :94 — unlike `moveItem`, which limits it to the legion warehouse, ItemMoveService.java:56-58); `CM_DELETE_ITEM` → `isBreakable` else `inventory.delete(item, DISCARD)` | CM_REPLACE_ITEM.java:25-36; CM_SPLIT_ITEM.java:27-40; CM_DELETE_ITEM.java:26-44 | no C++ packets; the services unported; `LegionService::addWHItemHistory` **unported** (`LegionService.cpp:420-422`); `Storage::delete_` ported but sends through `ItemPacketService` | P5-15/16, P5-07, **P5-11** |
| Q1 | `CM_EQUIP_ITEM(action, slot, itemObjId)`: `cancelUseItem`, **`PlayerRestrictions.canChangeEquip`**, 0 `equipItem`, 1 `unEquipItem` (else `STR_UI_INVENTORY_FULL`), 2 `switchHands`; `SM_UPDATE_PLAYER_APPEARANCE` broadcast | CM_EQUIP_ITEM.java:29-63 | **no C++ file**; `canChangeEquip` **unported** (`PlayerRestrictions.cpp:204-206`) | **P5-15**, **P5-13** |
| Q2 | `Equipment.equipItem`: class, level, race, gender, rank, slots, **`StigmaService.notifyEquipAction` (every item)**, soul-bind → `equip` → `ItemPacketService.updateItemAfterEquip`, `ItemEquipmentListener.onItemEquipment`, `updateCurrentStats`, `updateStatsAndSpeedVisually` | Equipment.java:59-194 | ported (`Equipment.cpp:159-305`) — **but `notifyEquipAction` (`StigmaService.cpp:30`) and `updateItemAfterEquip` (`ItemPacketService.cpp:15`) are unported**, and the soul-bind accept is (`Equipment.cpp:130-139`) | P4-12 → **P5-07** |
| Q3 | `unEquipItem` → `unEquip` → `inventory.put` (→ `ItemPacketService.sendItemUpdatePacket`) → `notifyItemUnequip` → **`ItemEquipmentListener.onItemUnequipment`** (`endEffect(item)`, stone stats, idian, bonus stats, enchant and tempering effects, buff skill, armor mastery) | Equipment.java:229-291; ItemEquipmentListener.java:86-121, 211-219 | `Equipment` ported (`:324-395`); **`onItemUnequipment` and `removeStoneStats` unported** (`ItemEquipmentListener.cpp:151-153, 259-261`) | P4-12 → **P5-01**, P5-07 |

### 2.3 Status by area (measured) — and the bodies no count shows

Sites counted with `grep -o 'AION_UNPORTED('` over the files `chunks.py files <chunk>` names; "undeclared" is a name-level comparison of every
Java method (inner and anonymous classes included) with every `name(` in the C++ header, `.cpp` and generated `.xml.inc` (script in §12).

| Area | Chunk | Chunk total | **On this milestone's path** | Undeclared bodies (lesson 1) |
|---|---|---|---|---|
| drop: `DropRegistrationService` / `DropService` / `DropDistributionService` | P5-09 | 121 + 1 partial (rest: trade 8, exchange 11, broker 11, private store 8, mail 15, craft 9, reward 12, recipe 3, passport 1) | **32 + 1 partial** required (26 + the 6 solo `DropService` bodies); 7 team bodies optional; `DropDistributionService` 4 → M5g | 0 by the plan's script; **`census.py` counts 11** (stage 1, §16) |
| quest drops | P5-06 | 163 + 2 | **4** (`getQuestDrop`, `isQuestDrop`, `allowLooting`, `regQuestDropItem`) + a signature request | 0 |
| item services | P5-07 | 109 | **32**: `ItemService` 11, `ItemPacketService` 9, `ItemMoveService` 3, `ItemRestrictionService` 3, `ItemSplitService` 4, `socketGodstone` 1, `notifyEquipAction` 1 | `ItemPacketService` enum companions 3 (`getKinahUpdateTypeFromAddType`, `ItemDeleteType.fromUpdateType`/`fromQuestStatus`; `Storage.cpp:34` holds a local copy of one), `socketGodstone`'s observer 1 |
| **item actions** | P5-07 | 0 sites (30 of 32 are headers without `.cpp`) | `SkillUseAction` 3 bodies + the API | **143 over 36 files**: `canAct`/`act` × 32 bound classes = 64, `ItemActions` lookups 9 (incl. `getItemActions`), 70 private/inner (`finishUse`, `abort`, …); `CompositionAction` has **no C++ file** (4) |
| restrictions | P5-13 | 118 + 3 | **2** (`canUseItem`, `canChangeEquip`); `canTrade`, `canChat`, `canInviteTo*` 5 optional | 0 |
| equipment listener, drop reward enum | P5-01 | 28 | **2** (`onItemUnequipment`, `removeStoneStats`) | `DropRewardEnum.dropRewardFrom` (new companion header) |
| `Equipment` soul bind | P4-12 | 5 | **1** (`Equipment_RequestResponseHandler::acceptRequest`, `Equipment.cpp:138`) | its observer and task (`Equipment$2.abort`, `$3.run`) 2 |
| legion warehouse history | P5-11 | 86 | **1** (`LegionService::addWHItemHistory`, `LegionService.cpp:420-422`; a no-op for a player without a legion, LegionService.java:1082-1092) | 0 |
| effect classes | P5-03 / P5-04 | **164 / 192 at HEAD, 122 / 134 in the working tree** (M5b-2 part 3 closing its 38 classes) | **38** in 14 classes (§2.5, §2.6) | 3 inner (`BlindEffect.checkAttackerStatus`, `FearEffect` `attacked`/`run`) |
| client packets | P5-15 / P5-16 | 43 of 190 Java files exist | **9 packets, 18 bodies** (§2.7) | 18 (no file) |
| `TemporaryTradeTimeTask` | P5-07 | — | not on the solo path (team loot, exchange) | **no C++ file**, 4 |
| **Required total** | | | **112 sites + 1 partial** | **14 bodies + 18 packet bodies** + a 64-declaration layout batch |

**Tests that pin today's behaviour and must be rewritten** (rev 2 completes the list):

| Test | What it pins | Rewritten by | When |
|---|---|---|---|
| `tests/economy/DropRegistrationServiceTest.cpp:114-160` (now `tests/economy/P5-09a/`, moved by M5c's I-01, §20) | 3 cases assert the partial and the empty maps | L-05 (loot lane) | with L-01 |
| `tests/scenario/M5bScenarioTest.cpp:2130-2136` (R3) | `ASSERT_TRUE` that the `DropRegistrationService.cpp:43` row exists, and its hit count = kills | **G-05** (gate-harness lane, stage 1) | **same commit as L-01** |
| `tests/scenario/M5bScenarioTest.cpp:2299-2305` (Q2) | `DropNpc` created **0** | **G-05** | **same commit as L-01** |
| `tests/scenario/m5b_partial_allowlist.txt:46-50` (§A) and the M5b-2 gate's §A row (m5b2-plan.md §10.1) | the partial is hit at least once | **G-05** | **same commit as L-01** |
| `tests/app/CheckOutputTest.cpp:496` | `DropNpc` is a `zeroLiveClasses()` row | **G-06** (gate-harness lane under a P5-14 lease, stage 1) | stage 1 |
| `tests/app/CheckOutputTest.cpp:552-553`, `:585` | "0 0 … a class M5b-1 never creates" — these feed fabricated counts and **stay green**; only their messages go stale | G-06 (reword) | stage 1 |
| `tests/itemsvc/ItemServicesM5aTest.cpp:161` | the login paths reach no unported body | — | still true, keep |

### 2.4 The drop data the start maps use

Derived from `global_drops/rules/**` (2,196 `gd_rule` elements; **all files, recursively** — `static_data.xml:88` is a `singleRootTag`
directory import), `custom_drop/custom_drop.xml` (174 `npc_drop`), `quest_data.xml` (`quest_drop`), `npc_templates.xml` and `item_templates.xml`,
applying DropRegistrationService.java:167-196 and 298-474 for a level-1 player of the map's race, with the map's `drop_type` (`ELYSEA` /
`ASMODAE`, world_maps.xml:11, :20) as the `gd_worlds` key.

**"Applicable rule" (rev 2, the definition Y2 and the oracle use):** a rule whose restrictions pass **and** whose candidate set is non-empty
after `collectAllowedDrops`' item-race and `min_diff..max_diff` filter. A rule that passes the restrictions but has no candidate adds nothing:
`addDropItems` only registers when `collectDrops` returns a non-empty list (DropRegistrationService.java:231-232). Rev 1 counted one such rule
for 210133 ("Armor (Common)", rules_equipment.xml:3759 and :4359, `min_diff="-2"`: the lightest armour is level 4, a diff of −3 from a
level-1 npc) and so said 11 rules and 79.1 %.

**Event drop rules.** `registerDrop` runs `addGlobalDrops` a second time over `EventService.getActiveEventDropRules()`
(DropRegistrationService.java:96-98), the `gd_rule`s of whichever timed events are active on the day (`events/timed_events/custom_events.xml`
carries 23 `<gd_rule>` elements, `retail_events.xml` 96; the review's 46 / 191 counted opening and closing tags). **Every gate profile already
switches all events off**: `ScenarioServers` merges
`gameserver.event.service.disabled_events = *` into every scenario run (`ScenarioServers.cpp:31`, asserted at `ScenarioServersTest.cpp:211`;
the same key is in `m5a.properties.example`/`m5b.properties.example:25` and in the user's `config/mygs.properties:15`), and `*` makes
`collectActiveEvents` return nothing (EventService.java:133-134). So the gate's entry count does not depend on the run date; the oracle states
the assumption and L-05 covers the event pass with a unit case. **A real-client session with events enabled gets extra entries** — that is
Java's data, not a bug (§11).

| | Poeta 210010000 | Ishalgen 220010000 |
|---|---|---|
| npc ids spawned (comments stripped) | 147 | 166 |
| with at least one applicable global rule | 77 | 91 |
| with the **Kinah** rule | 42 | 50 |
| with a **custom** drop | **0** | **0** |
| quest-drop items on the map's npcs | 20 | 25 |
| distinct droppable items | 999 | 1,077 |
| action tags of those items | none 884, `enchant` 47 (manastones), `remodel` 34, `skilluse` 31, `queststart` 2, `decompose` 1 | the same plus `read` 2 |
| `race` of the spawned ids (the Kinah rule's key) | ELYOS 55, **BEAST 33**, none 24, BROWNIE 18, MAGICALMONSTER 6, DEMIHUMANOID 6, KRALL 5 | ASMODIANS 63, **BEAST 40**, … |
| `group_drop` of the spawned ids (the `gd_npc_groups` key and the chest test) | LIGHT 43, NONE 33, CHERUBIM 12, BROWNIEM 10, BROWNIEF 6, SPAKY 5, … | DARK 51, NONE 35, SPRIGG 12, SPAKY 6, … |

**Group drops.** Two different things share the word. (a) The npc's **`group_drop`** (all 63,287 templates carry one): it selects the
`JUNK_<group>_MATERIAL` rules through `gd_npc_groups` (DropRegistrationService.java:401-409) and makes an npc a chest when it starts with
`treasure` or ends with `box` (:114). Required. (b) **Team loot**: `LootGroupRules` (round robin / free for all / leader, the quality rules,
roll and bid), `member_limit > 1` on **104** rules (DropRegistrationService.java:233-250), `drop_each_member` on quest drops
(QuestService.java:676-724), `DropDistributionService`, `SM_GROUP_LOOT`, `TemporaryTradeTimeTask`. **Deferred to M5g** (D9): every team arm
needs `LootGroupRules`' 7 unported bodies (P5-10) and a team, and none of it is reachable solo.

**The M5b gate's monster, 210663 "juvenile sparkie"** (level 2, BEAST, NORMAL, DISCIPLINED, tribe MONSTER, `group_drop="SPAKY"`,
npc_templates.xml:57809): **10 rules apply**, all from `rules_commons.xml`, `rules_equipment.xml` and `rules_junk_materials.xml`, none
npc-specific. **Every candidate of every one of them is `race="PC_ALL"`**, and so are 210133's (measured), so the player's race changes no
entry for either monster. Chances at `rates.drop = 1.0` (dynamic rules × rank 1.0 × rating 1.0; no level reduction at +1):

| Rule | Chance % | Candidates for an Elyos level 1 | Count |
|---|---|---|---|
| `JUNK_SPAKY_MATERIAL` (via `group_drop`) | **40** | 182004793 Sparkie Carapace Fragment | 1 |
| Manastones (Common) | 15 | 10 manastones 167000226-235, 167000525 (`<enchant>`) | 1 |
| Power Shards | 6 | 169000003 Minor Power Shard | 2-15 |
| Buff Food | 3.5 | 160003551, 160003557 (`skilluse` → `StatupEffect`) | 1 |
| Potions (Common) | 3.5 | 162000002, 162000007, 162000052, 162000057 | 1 |
| Armor (Common) | 2.75 | 27 Plainsman's armour pieces | 1 |
| Potions (Rare) | 2.5 | 162000012, 162000017, 162000087, 162000093 | 1 |
| Weapons (Common) | 1.3 | 24 Plainsman's weapons | 1 |
| Illusion Godstones (Legend) / (Unique) | 0.02 / 0.01 | 17 + 17 godstones 168000212-245 | 1 |

**P(at least one drop per kill) = 58.2 %** at the default rate — the number behind finding 1. **No Kinah rule** (BEAST). A kinah-dropping
neighbour: **210133 "striped kerub"** (level 1, MAGICALMONSTER, `group_drop="CHERUBIM"`, npc_templates.xml:54505), the M5b-2 plan's npc-skill
target 76.8 m from the Elyos spawn: **10 applicable rules** — Kinah 50 %, `JUNK_CHERUBIM1_MATERIAL` 40 % (182003943 Kerub Scale Fragment),
Manastones (Common) 15, Power Shards 6, Buff Food 3.5, Potions (Common) 3.5, Potions (Rare) 2.5, Weapons (Common) 1.3, the two illusion
godstone rules 0.02 / 0.01 — with **Kinah** giving 5-25 × level 1 × 1.0⁶ = **5-25 kinah**; P(some drop) = **78.5 %** (rev 1 said 11 and
79.1 %, see the definition above). It is level 1, which `isAllowedDefaultGlobalDropNpc` would exclude anywhere but Poeta and Ishalgen
(DropRegistrationService.java:174-175).

**Two picks the gate can rely on (rev 2).** "Power Shards" has **one** candidate, 169000003 Minor Power Shard, for both monsters, and
`JUNK_SPAKY_MATERIAL` has one, 182004793, so at the gate's rate the first kill makes a shard stack and a junk stack, **the 210133 kill merges
into the shard stack for certain**, and a second 210663 kill merges into both. Every other merge is a pick: "Potions (Common)" picks one of four,
two of which (162000002, 162000007) are starter stacks. None of these items has a `max_stack_count` a gate could reach (1,000 for the potions
and the junk, 10,000 for the shard).

**How the gate makes drops deterministic without touching data: the drop rate.** `calculateBoostDropRate` multiplies by
`Rates.get(killer, RatesConfig.DROP_RATES)` (DropRegistrationService.java:218; Rates.java:166-173; `gameserver.rates.drop`, default `1.0, 2.0`)
and `calculateDropChance` multiplies every rule chance by it (DropModifiers.java:53-57); a rule fires unless `Rnd.chance() >= chance`
(:189), and `Rnd.chance()` is `nextFloat(100f)`, in [0, 100) (Rnd.java:32-34). **At `gameserver.rates.drop = 1000000` every applicable rule
fires with a wide margin** (the smallest, 0.01 %, becomes 10,000 %). Rev 1's 10000 put that rule at exactly 100.0f after float rounding — it
fired only because `nextFloat(100)` never returns 100, and a different multiplication order in the port could land on 99.99999f. So the number
of entries, their indexes and which rule each came from are exact, and only the pick inside a rule (`Chance.selectElement`) and the count range
are random. The rate touches nothing else: it is read only at DropRegistrationService.java:218 (a grep of `DROP_RATES`), it multiplies
chances and never counts, and a one-value list is always index 0 whatever the membership (Rates.java:171-172). **At `0` no global or custom
rule ever fires** (custom drops too, DropGroup.java:64) and the corpse decays in 2 s exactly as today (D3, D4).

### 2.5 What the player's items need (the item subset)

**The starter inventory** (player_initial_data.xml:5-80, the same 13 entries for all six classes except the gear): 1,000 kinah, weapon, two
armour pieces, 12 × 160000001 Mercenary's Fruit Juice, 20 × 169300002 Bandage, **100 × 162000002 Minor Life Potion, 100 × 162000007 Minor Mana
Potion**, 3 × 50 event potions, 2 × 169620005 Lodas Amulet, 1 × 164002039 Administrator's Boon. Every usable one is a `skilluse` action.

**Items with a `skilluse` action** over the starter set and everything droppable on the two maps: **48 items, 9 leaf effect classes**, closed
under `extends`:

| Class | Items | M5b-2's 38 | State | Chunk |
|---|---|---|---|---|
| `StatupEffect` | 20 effects in **12 items** (juice, buff food, event potions; the 8 serums and focus agents carry two statups each — stage 1, `m5b3-item --survey`) | yes | ported | P5-04 |
| `HealEffect`, `MPHealEffect` (+ `HealOverTimeEffect`, `AbstractOverTimeEffect`, `AbstractHealEffect`, `BufEffect`) | 9 + 9 | yes | ported | P5-03/04 |
| **`ProcHealInstantEffect`** | **18** (every life potion, serum, panacea, elixir) | no | **4 unported** | P5-04 |
| **`ProcMPHealInstantEffect`** | **18** | no | **4 unported** | P5-04 |
| **`XPBoostEffect`** | 1 (starter Lodas Amulet, skill 10249) | no | 1 unported | P5-04 |
| **`NoResurrectPenaltyEffect`**, **`NoDeathPenaltyEffect`**, **`HiPassEffect`** | 1 (starter Boon, skill 10350) | no | 1 each | P5-04, P5-04, P5-03 |

**6 new classes, 12 sites, 131 Java lines** (41 + 36 + 21 + 11 + 11 + 11; rev 1 said 177). Every other effect a potion reaches is in M5b-2's set. The potions' numbers are template constants
with no randomness (HealEffectTemplate.java: `calculateSnapshotHealValue` without heal boost for `ProcHealInstantEffect`, capped at the missing
HP): **162000002 heals exactly 37 HP at once and 37 every 2 s for 20 s (skill 9889), 162000007 restores 59 MP (skill 9894)**, and both carry
`usedelay="30000" usedelayid="11"` (item_templates.xml:830728).

**Item actions.** Of the 32 bound action classes, only **`SkillUseAction`** is on this milestone's path (the starter items, 31 of the droppable
items). `EnchantItemAction` (47 manastones, 15 % per Poeta kill) needs `EnchantService` (11 unported, 591 Java lines); `RemodelAction`,
`DecomposeAction`, `QuestStartAction`, `ReadAction` need services of M5c/M5d. **They keep throwing (D6)** and §11 tells the user which.

### 2.6 Godstones and material skills (docs/deviations/P5-02a.md "Reachable after part 2")

**Godstones.** 268 items carry a `<godstone>`; their **110 proc skills need 10 leaf classes, 5 of them new**: `ProcAtkInstantEffect` (70
skills, 2 sites), `PoisonEffect` (6, 5), `SilenceEffect` (5, 4), `BlindEffect` (5, 4 + 1 inner), `ParalyzeEffect` (4, 4). **The 34 illusion
godstones that Poeta monsters drop use exactly the same ten.** What a godstone needs, end to end:

| Piece | State |
|---|---|
| **Getting one**: a drop (0.03 %/kill in Poeta; certain at `rates.drop = 1000000`) or the gate's inventory seed | this milestone's loot path |
| **Socketing it**: 4.8 does it with **`CM_MANASTONE` action 4, no npc** (`packets[91]` = `CM_GODSTONE_SOCKET` is commented out, AionClientPacketFactory.java:119) → `ItemSocketService.socketGodstone`: an `ItemUseObserver`, a 2 s `ITEM_USE` task, `decreaseByObjectId(stone)`, `weapon.addGodStone`, `updateItemAfterInfoChange` | `CM_MANASTONE` no C++ file; `socketGodstone` unported (`ItemSocketService.cpp:36`); `Item::addGodStone` ported; cycles rows exist (`cycles.toml:273-274`; rev 2 said 272-273), fieldmap callback `ItemSocketService@L193:92` exists |
| **The proc**: `CreatureController.applyGodStoneEffect` → `GodStone.tryActivate` (probability − `PROC_REDUCE_RATE`, × `gameserver.rates.godstone.activation.rate`, a 750 ms evaluation cooldown) → `getSkill` → `new Effect(skill, target).initialize(); applyEffect()` → `STR_SKILL_PROC_EFFECT_OCCURRED`; illusion godstones break after `nonbreakcount` activations | ported (`CreatureController.cpp:316`, `GodStone.cpp`); only the 5 effect classes are missing, and `updateItemAfterInfoChange` for the break |
| **The other `CM_MANASTONE` arms** (1 enchant stone, 2 manastone, 3 remove manastone, 8 amplification) | `EnchantItemAction`, `EnchantService`, `ItemSocketService` manastone bodies: **M5c** (D8); `StigmaService.chargeStigma`: **M5e** (per m5c-plan.md §3a, edited by M5c's I-01, §20); they throw until then |

A deterministic gate godstone exists: **168000116 "Fx Test Earth Godstone"** (item_templates.xml:848006): `probability="1000"`, `breakprob="0"`,
skill 8267 = `procatk_instant delta="100" element="EARTH"` — every evaluated hit **procs**, nothing breaks. The Training Sword (100000094,
`mask="138366"`) has `CAN_PROC_ENCHANT` (bit 10, ItemMask.java:17). **Two caveats (rev 2).** (a) Proc is not damage: 8267 is
`skilltype="MAGICAL"` with no `noresist`, so `EffectTemplate.calculate` can resist it at `isDodgedOrResisted` (EffectTemplate.java:314), and
`applyGodStoneEffect` sends `STR_SKILL_PROC_EFFECT_OCCURRED` after `applyEffect()` whether the effect landed or not — in Java
(CreatureController.java:278-283) and, faithfully, in C++ (`CreatureController.cpp:318-324`). The proc is evaluated only for an auto-attack
whose status is neither DODGE nor RESIST (CreatureController.java:255-256). (b) ~~Skill 8267 has two templates~~ — **wrong (stage 1,
§16): skill 8267 has one template**, skill_templates.xml:80570, `element="EARTH"`, with a weapon start condition and `move_casting
allow="false"`; the `element="WIND"` template above it (opening at :80557) is skill 8266. `SkillData.afterUnmarshal` still keeps the **last**
of duplicate ids (`skillTemplateById.put`, SkillData.java:33-39), and `oracle.py m5b3-item` does the same.

**Material skills.** `AbstractMaterialSkillActor.MaterialSkillTask` (a 1 s fixed-rate task while a creature touches a skill material, geo and
`gameserver.geodata.materials.enable` on — the default) calls `SkillEngine.applyEffectDirectly(skillId, level, creature, creature, null,
MATERIAL_SKILL)`. `material_templates.xml` names 28 skills needing 13 leaf classes, **6 new**: `ProcAtkInstantEffect` (13), `DispelEffect` (5),
`AbsoluteSnareEffect` (4, data-only), `FearEffect` (1), `MpAttackInstantEffect` (1), `ProcHealInstantEffect` (1). **Measured on the geo files:**
Poeta's 4,496 mesh placements include **98 with a skill material** (material 62 ×69, 60 ×22, 61 ×7) and Ishalgen's 5,017 include 48
(material 62 ×36, 60 ×11, 61 ×1) — **confirmed by `oracle.py m5b3-material` in stage 1** (§16); the review's 97 and 44 were wrong — **every one
of them skill 8302 *Flame Strike*, `procatk_instant value="5" element="FIRE" noresist="true"`** (skill_templates.xml:81174-81186, stack
`MATERIAL_SKILL_PROC_BONFIRE_DAMAGE`): the camp fires. Material 60 is unconditional; 61/62 need NIGHT / not-raining. The nearest material-60
placement is `pr_l_fire_semisphere_01a.cgf` at **(863.54, 1252.50, 119.50), 407 m** from the Elyos spawn; the nearest of any is a material-62
fire at (1118.42, 992.52, 131.61), 108 m. **Terrain materials (the `<map>.png` byte layer) were not read** (§12) — **settled in stage 1:
neither map has a terrain-materials file** (`210010000.png` and `220010000.png` are 16-bit heightmaps; §16). So on the start maps material
skills need **only `ProcAtkInstantEffect`**; the other five are for the rest of the world.

**The union this milestone ports: 14 classes, 38 sites + 3 inner bodies, 618 Java LOC** — P5-03: `BlindEffect`, `DispelEffect`, `FearEffect`,
`HiPassEffect`; P5-04: `MpAttackInstantEffect`, `NoDeathPenaltyEffect`, `NoResurrectPenaltyEffect`, `ParalyzeEffect`, `PoisonEffect`,
`ProcAtkInstantEffect`, `ProcHealInstantEffect`, `ProcMPHealInstantEffect`, `SilenceEffect`, `XPBoostEffect`. Their `override` declarations
already exist (M5b-2's `m5b2-f04` batch); only bodies are missing.

### 2.7 The client packets

| Packet | Java | Opcode (AionClientPacketFactory.java) | Chunk | Need |
|---|---|---|---|---|
| `CM_START_LOOT` | 55 | 154 (:182) | P5-16 | **R** |
| `CM_LOOT_ITEM` | 35 | 155 (:183) | P5-16 | **R** |
| `CM_USE_ITEM` | 126 | 37 (:65) | P5-16 | **R** (m5a-client-session.md F-2) |
| `CM_MOVE_ITEM` | 37 | 156 (:184) | P5-16 | **R** (F-2) |
| `CM_SPLIT_ITEM` | 41 | 157 (:185) | P5-16 | **R** — dragging part of a stack |
| `CM_REPLACE_ITEM` | 37 | 178 (:206) | P5-16 | **R** — dropping an item on another swaps them |
| `CM_MANASTONE` | 110 | 74 (:102) | P5-16 | **R** for action 4 (godstone); actions 1/2/3/8 reach M5c bodies and throw. Arms 1/2 construct `EnchantItemAction` and call its **non-virtual five-argument `act(player, stone, target, supplement, targetFusedSlot)`** (CM_MANASTONE.java:79-86, EnchantItemAction.java:86) — declared by h01 (§7) |
| `CM_EQUIP_ITEM` | 64 | 38 (:66) | P5-15 | **R** |
| `CM_DELETE_ITEM` | 45 | 116 (:144) | P5-15 | **R** — destroying junk |
| `CM_GROUP_LOOT`, `CM_CLIENT_COMMAND_ROLL`, `CM_DISTRIBUTION_SETTINGS`, `CM_GROUP_DISTRIBUTION` | 236 | | P5-15/16 | M5g |
| `CM_UNWRAP_ITEM`, `CM_TUNE`, `CM_TUNE_RESULT`, `CM_ITEM_REMODEL`, `CM_ITEM_PURIFICATION`, `CM_CHARGE_ITEM`, `CM_COMPOSITE_STONES`, `CM_SELECT_DECOMPOSABLE`, `CM_APPEARANCE` | 558 | | P5-15/16 | per m5c-plan.md §3a (edited by M5c's I-01, §20): `CM_TUNE`, `CM_TUNE_RESULT`, `CM_SELECT_DECOMPOSABLE` → M5c stage 1; `CM_UNWRAP_ITEM`, `CM_ITEM_REMODEL`, `CM_ITEM_PURIFICATION`, `CM_CHARGE_ITEM`, `CM_COMPOSITE_STONES` → the capital-economy milestone (m5c-plan.md D2; M5j if the user keeps the broker in M5c); `CM_APPEARANCE` → M5j |
| `CM_GODSTONE_SOCKET` | 40 | not registered in 4.8 (:119) | P5-15 | never |

**Server packets: none to write** (§1). The gate needs **independent decoders** for `SM_LOOT_ITEMLIST`, `SM_INVENTORY_ADD_ITEM`,
`SM_INVENTORY_UPDATE_ITEM`, `SM_DELETE_ITEM`, `SM_CUBE_UPDATE`, `SM_WAREHOUSE_ADD_ITEM`, `SM_DELETE_WAREHOUSE_ITEM`,
`SM_UPDATE_PLAYER_APPEARANCE`, `SM_ITEM_USAGE_ANIMATION` (G-02), reusing the item-info-blob reader behind `decodeInventoryInfo`
(`tests/scenario/decoders/PacketDecoders.h:491`) and `decodeLootStatus` (`CombatDecoders.h:292`). Wire shapes the gate reads:
`SM_LOOT_ITEMLIST` (SM_LOOT_ITEMLIST.java:37-58): `writeD(target)`, `writeC(n)`, per item `writeC(index)`, `writeD(itemId)`, `writeD(count)`,
`writeC(optionalSocket)`, `writeC(0)`, `writeC(0)`, `writeC(showLootConfirmation)`; `SM_LOOT_STATUS`: `writeD(target)`, `writeC(status)`,
`writeD(lootEffectId)` — 1003 when a listed godstone is in the drop (DropItem.java:204-212); `SM_INVENTORY_ADD_ITEM`: `writeH(mask)`,
`writeH(n)`, item infos; `SM_INVENTORY_UPDATE_ITEM`: `writeD(objId)`, `writeS(l10n)`, the blob, `writeH(updateType mask)` if sendable;
`SM_DELETE_ITEM`: `writeD(objId)`, `writeC(deleteType mask)`; `SM_ITEM_USAGE_ANIMATION`: 4 × `writeD`, `writeD(time)`, 4 × `writeC`, `writeD`.

### 2.8 Reachability from every entry point this milestone turns on (lesson 2)

Traced from each entry point to the first unported or partial body. **E-13 and E-14 are reachable before M5b-3 starts.**

| # | Entry point | Reaches first | After M5b-3's required items |
|---|---|---|---|
| E-1 | Any player kill (`doReward`, every Poeta monster) | `registerDrop` partial → **`QuestService.getQuestDrop`** (P5-06), `DropRewardEnum` (no companion, P5-01), `scheduleFreeForAll` | closed; **side effects: the M5b gate's R3/Q2/Q1 go red at once, 300 s corpses make R4/Q3 flaky** (finding 1, D4 — fixed in the same commit). One level deeper and still unreachable: `isQuestDrop` → `QuestState::getQuestVarById` (`QuestState.cpp:41`) for a quest in `START` with a collecting step — M5d |
| E-2 | `DropService.scheduleFreeForAll`'s 240 s task | `DropNpc.startFreeForAll` (ported) | closed |
| E-3 | `CM_START_LOOT` | no packet → `requestDropList` | closed |
| E-4 | `CM_LOOT_ITEM` | no packet → `requestDropItem` → `ItemService.addItem` → `ItemPacketService` | closed; team arms → `LootGroupRules` (P5-10) throw (D9) |
| E-5 | `NpcController.petLoot` (a looting pet) | `requestDropItem(…, autoLoot)` → `canAutoLoot` only with loot group rules | closed solo; pets need a pet (M5j) |
| E-6 | `CM_USE_ITEM` | no packet → `canUseItem` → `getItemActions` (undeclared) → `canAct` (undeclared) | closed for `skilluse`; **every other action throws** (D6) |
| E-7 | A potion's cast end | `decreaseByObjectId` → `ItemPacketService`; `ProcHeal/ProcMPHealInstantEffect` | closed |
| E-8 | `CM_EQUIP_ITEM` | `canChangeEquip`, **`notifyEquipAction`**, **`updateItemAfterEquip`**, **`onItemUnequipment`** — four chunks | closed; stigma items reach `StigmaService`'s other 11 bodies (throw; M5e per m5c-plan.md §3a, edited by M5c's I-01, §20) |
| E-9 | `CM_MOVE_ITEM`, `CM_REPLACE_ITEM`, `CM_SPLIT_ITEM`, `CM_DELETE_ITEM` | the item services; **and `LegionService::addWHItemHistory` (P5-11, `LegionService.cpp:420-422`) on every cross-storage `CM_SPLIT_ITEM`** — splitting part of a stack from the cube into the warehouse, legion or not (ItemSplitService.java:80, :94). `moveItem` reaches it only for the legion warehouse (ItemMoveService.java:56-58). Rev 1 missed the split case | closed by **T-08** (the 10-line body under a P5-11 lease; a no-op without a legion); a legion member moving into or out of the legion warehouse then reaches `addHistory` (`LegionService.cpp:384-386`, unported) — M5h |
| E-10 | `CM_MANASTONE` 4 | `socketGodstone` | closed; arms 1/2 → `EnchantItemAction::canAct`/five-argument `act` (stubs, D8), arm 3 → `removeManastone`, arm 8 → `amplifyItem`: all throw (M5c) |
| E-11 | A hit with a socketed godstone (`applyGodStoneEffect`) | 5 effect classes; `updateItemAfterInfoChange` on a break | closed |
| E-12 | A soul-bound item's equip (`soulBindItem` request accepted) | `Equipment.cpp:138` | closed (P-04) |
| **E-13** | **`ExpireTimerTask` (1 s), an item whose `expire_time` passes — the starter Boon, 3 days after creation** | `Item::onExpire` → `Storage::delete_` → **`sendItemDeletePacket`**. The item is already removed when it throws, so one ERROR, a ghost item in the client, and the next tick removes the entry | **closed by T-02; reachable today** |
| **E-14** | **Skill 245 *Bandage Heal* (every character) and the other skill templates with `<itemuse>` (86 elements in skill_templates.xml; rev 1 said 20)** | `ItemUseAction.act` → `Storage.decreaseByItemId` → **`sendItemPacket`** | **closed by T-02; reachable at M5b-2's end** |
| E-15 | `MaterialSkillTask` (geo on) — the ~98 Poeta camp fires | `ProcAtkInstantEffect` (2 unported) | closed (E-02) |
| E-16 | Startup, enter world, logout | nothing new: no startup, enter-world or leave-world body changes a storage (the enter-world `registerExpirables`, `PlayerEnterWorldService.cpp:590-616`, only registers; `PlayerLeaveWorldService.cpp` deletes objects, not items), and `tests/itemsvc/ItemServicesM5aTest.cpp:161` pins "the fresh-character login paths reach no unported body" | unchanged |
| E-17 | **The second `registerDrop` entry point**, `AIActions::registerDrop` (`AIActions.cpp:111-113`), which Java's `ChestAI`, `QuestItemNpcAI` and `NightmareCrateAI` call (data/handlers/ai/ChestAI.java, …/quests/QuestItemNpcAI.java, …/nightmareCircus/NightmareCrateAI.java) | none of the three has a C++ file, and no npc spawned on Poeta or Ishalgen has the `chest` AI (measured: 12 / 17 `quest_use_item` npcs, which stay on `DummyAI` under `missing_ai_handlers=warn`) | **not reachable in M5b-3**; reached by the same `registerDrop` once a handler lands (§3, O-08) |
| E-18 | Every item that lands in the cube: `Storage.add` → **`QuestEngine.onItemGet`** (Storage.java:180-182, `Storage.cpp:188`) | ported (`QuestEngine.cpp:389-401`): a registered quest handler's `onGetItemEvent` (none before M5d) and `PlayerController::updateNearbyQuests` (ported, `PlayerController.cpp:263-270`, already run by enter world) | unchanged; checked because every loot, split and move goes through it |

---

## 3. Where this plan disagrees with m5b-plan.md §9 and the roadmap

| Earlier claim | This plan | Why |
|---|---|---|
| "M5b-3 … P5-09 (123), P5-07 (109) … ~250 sites, ~5,000 LOC" (m5b-plan.md §9) | **~145 bodies required, ~3,800 Java LOC**; P5-09 is 121 today (+1 partial), of which 32 are on the path; P5-07 109, of which 32 | ~160 of the 232 sites are mail, broker, trade, craft, enchant, stigma, warehouse, cube expansion — M5c's |
| implicitly, sites == work, and only P5-09/P5-07 | **+ ~70 bodies in other chunks** (14 effect classes 38 + 3, 18 packet bodies, 4 P5-06, 2 P5-13, 2 + 1 P5-01, 1 + 2 P4-12, 1 P5-11) **+ 14 undeclared bodies** (3 `SkillUseAction`, 3 enum companions, 1 `getItemActions`, 1 `DropRewardEnum`, 1 `socketGodstone` observer, 2 soul-bind inner, 3 effect inner) **+ a 64-declaration layout batch** that no count shows | lesson 1 (§2.3) |
| "`restrictions/` contains only `fwd.h`" (m5b-plan.md §1) | `PlayerRestrictions.cpp` exists; 7 bodies left, 2 on the path | M5b-1 C-01, M5b-2 P-01 |
| O-06: "`ItemMoveService::moveItem`, `ItemActionService`, the `AbstractItemAction` `canAct`/`act` API" | `ItemActionService` (identify, tune) is `CM_TUNE`'s — **M5c**; the API is a **layout** header batch, not bodies | ItemActionService.java:23, 57 are called only from CM_TUNE.java:41 and CM_TUNE_RESULT.java:42 |
| O-07 lists `CM_GROUP_LOOT`, `CM_CLIENT_COMMAND_ROLL` with the solo loot packets | team packets → **M5g**; `CM_EQUIP_ITEM`, `CM_DELETE_ITEM`, `CM_SPLIT_ITEM`, `CM_REPLACE_ITEM`, `CM_MANASTONE` added | none of the team ones is reachable solo; the five added are what a player does with a loot bag |
| m5b2-plan.md D13: "godstone procs and material skills … wait for the item milestone" | **10 classes for godstones and the world's materials, 1 for the start maps' camp fires** — and material skills are reachable **with geo on today**, independent of items | §2.6 |
| m5b-plan.md D5: "the one thing on it that is deterministic is the unconditional `SM_LOOT_STATUS(LOOT_ENABLE)`" | still true, and it becomes the M5b gate's new R3 (G-05) | DropRegistrationService.java:104-106 runs for an empty drop set too |
| m5b-plan.md O-08: the six other root AI handlers of the start maps (`AbyssGuardSimpleAI`, `ActionItemNpcAI`, `PostboxAI`, `ResurrectAI`, `QuestItemNpcAI`, `PortalDialogAI`) wait for "M5b-3 or later" | **not M5b-3.** None is on the loot or item path; the only link is `QuestItemNpcAI`'s `AIActions::registerDrop` (§2.8 E-17), which hands out quest items. `QuestItemNpcAI` and `ActionItemNpcAI` (`quest_use_item` / `useitem`: 12 + 1 npcs on Poeta, 17 + 2 on Ishalgen) → **M5d**; `PostboxAI` → **M5c** (mail); `PortalDialogAI`, `ResurrectAI` → **M5f**; `AbyssGuardSimpleAI` (2 / 10 guards) → **M5j** | measured AI names of the spawned ids; the handlers' Java callers |

---

## 4. Decisions

| # | Decision | Why |
|---|---|---|
| **D1** | **Offered to the user, not taken by the integrator: start M5b-3 stage 1's two long lanes, loot and items, beside M5b-2 stage 2, within the six-lane cap. Their bodies and unit tests are written and merged early — except `registerDrop`'s closure (L-01), which is written early but merges only after M5b-2 has released P5-SC, together with the re-green (G-05).** | The user asked "is there anything you can potentially parallelize". M5b-2's remaining work touches P5-01/P5-03/P5-04 (part 3, in the tree now), then P5-SC, P5-05, a P5-02b lease (stage 2) and P5-14 + P5-SC (stage 3) (m5b2-plan.md §6). **The loot lane (P5-09, a P5-06 lease on `QuestService.cpp`, a P5-01 lease on the one new file `utils/stats/DropRewardEnumInfo.h`) and the items lane (P5-07, a P5-11 lease on `LegionService.cpp`) touch no file M5b-2 touches.** The new P5-01 file cannot collide with M5b-2's P5-01 edits (`AttackUtil.cpp`, `StatFunctions.cpp`) or a stage-2 fixup reopening P5-01, because nobody else writes it (rev 2: rev 1 had the loot lane wait for the player-side lane's P-03, which under D1 would not have started). **What is not disjoint is the merge** (rev 2): L-01 changes every kill, so it must land in the same commit as G-05's edits to `M5bScenarioTest.cpp`'s profile block and the allow-lists — the block and the list M5b-2's own regate lane edits (m5b2-plan.md G-05(a)-(c), e.g. removing `soulsickness.disable` at `M5bScenarioTest.cpp:906`). So under D1 L-01 waits, finished and reviewed, until M5b-2's gate, regate and stage-3 lanes have merged and P5-SC is free; everything else in the two lanes (T-02 first: it closes E-13/E-14, §8 risk 4) merges as it goes, because no gate observes it. M5b-3's player-side, effects and gate-harness lanes start when M5b-2 closes. M5b-2 stage 2 has four lanes, so these two fit under six. **What it buys:** the two lanes are the milestone's critical path at ~8-10 agent-days each (§6); starting them early hides most of that behind M5b-2's stages 2-3. **It is the user's decision** (phase5-roadmap.md "Decisions along the way"): it changes the order they asked for ("in order, one after another") and the load on their machine. If declined, nothing else in this plan changes. |
| **D2** | **Scope: a solo player loots, keeps, moves, splits, destroys, equips and uses items; kinah; potions; godstone socketing and procs; camp fires.** Out, with the homes m5c-plan.md §3a gives them (edited by M5c's I-01, 2026-09-24, §20; this row said "M5c" for all item services): team loot (M5g); manastones, enchant, amplify, extraction, decompose and remodel items, identification and tune, cube expansion, craft-learn items, `TemporaryTradeTimeTask` (M5c); stigma and skill books (M5e); quest-start and read items (M5d); multi-return and instance-time items (M5f); housing items (M5h); AP extraction (M5i); pets, mounts, cosmetics, emotes, titles, megaphone, the upgrade arcade (M5j); npc warehouses, charge, purification, the remodel service, armsfusion, tempering, polish, dye, assembly, pack and composition (the capital-economy milestone of m5c-plan.md D2, the user's; M5j if the user keeps the broker in M5c); mail (M5c). | The roadmap row: "loot a corpse; use and move items". Everything out of scope is reached only through an npc dialog, a team, a quest, or an item action other than `skilluse`. |
| **D3** | **The M5b-3 gate forces drops with `gameserver.rates.drop = 1000000`** (rev 2; rev 1 said 10000, which left the 0.01 % rules at exactly 100.0f, §2.4) and asserts the drop set **by rule**: the entry count and indexes exactly, a bijection between entries and **applicable rules** (restrictions pass *and* the candidate set is non-empty after the race and level filter, §2.4), each item as a member of its rule's candidate set, counts as ranges, all computed by the oracle. **Event rules are out** because every scenario profile carries `gameserver.event.service.disabled_events = *` (`ScenarioServers.cpp:31`); the oracle states that assumption. | A Java configuration lever (§2.4), not a C++ deviation, so no `docs/deviations` row. At the default rate the gate would see an empty 210663 corpse 41.8 % of the time. |
| **D4** | **The M5b and M5b-2 gates (and their geo variants) set `gameserver.rates.drop = 0`, and their R3/Q2/allow-list rows are rewritten — in stage 1, in the same integrator commit as L-01** (rev 2; rev 1 put this in stage 2). R3 moves from "the partial was hit once" to "`SM_LOOT_STATUS(LOOT_ENABLE)` arrived once, for the corpse, with `lootEffectId` 0"; Q2's `DropNpc` row becomes created = kills, live 0; the §A row `DropRegistrationService.cpp:43` leaves both allow-lists. The stress nightly needs no key: its clients do not fight (`tests/scenario/stress/`), and npcs killing each other register no drop (`doReward` requires a Player winner, NpcController.java:225-245). | Finding 1. With 0, `registerDrop` still runs completely (the `DropNpc`, the `LOOT_ENABLE`, the free-for-all task) but the drop set is empty, so `scheduleDecayTask` keeps `IMMEDIATE_DECAY` and R4, Q2, Q3 keep their meaning, and the corpse's despawn unregisters the `DropNpc` 2 s after the kill (`NpcController.cpp:159`). Merging L-01 without these turns R3, Q1 and Q2 red on **every** run, not by chance (§2.3's table). The M5b-1 `soulsickness.disable` key is the precedent (m5b-plan.md D1). |
| **D5** | **`DropNpc` leaves `CheckOutput::zeroLiveClasses()` for a bounded row ("corpses with an unlooted drop at the stop"), and `DropItem` joins the summary rows** — in stage 1 (G-06, gate-harness lane under a P5-14 lease). The gates assert the exact number their script leaves (0). | `DropRegistrationService` is an `Immortal` holding `Ref<DropNpc>` until the corpse despawns (`DropRegistrationService.h:30-31`), and a corpse with a drop lives 300 s: a server stopped within 5 minutes of a kill holds one **in Java too**. That is a bound, not a leak — the same argument that moved `KnownObject` (`CheckOutput.cpp:190-194`). **Not** a merge-together constraint for L-01 (under D4's rate 0 a `DropNpc` lives 2 s, so the zero row stays green in the earlier gates), but required before the M5b-3 gate and before the user's first real-client session at the default rate, where any unlooted kill in the last 5 minutes would print a false `liveLeak`. Under D1 it waits for M5b-2's stage-3 census lane (m5b2-plan.md G-07) to release P5-14. |
| **D6** | **The item-action API is declared for all 32 bound action classes; only `SkillUseAction` gets bodies; every other action stays `AION_UNPORTED` and throws.** Signature (hub-headers.md §7.4: varargs → `std::initializer_list`, default `{}`, overrides repeat it): `virtual bool canAct(player::Player& player, Item& parentItem, runtime::Ptr<Item> targetItem, std::initializer_list<std::any> params = {}) const = 0;` and the same for `void act(...)`. **Applied in stage 0 with `runtime::Ptr<Item> parentItem`, not `Item&` (§15 item 1): port against the header, not this row.** | Java declares them abstract (AbstractItemAction.java:26, 34); `shells-3` is the precedent for declaring a behaviour API across every concrete shell with stubs. m5b2-plan.md D6's argument for throwing instead of a blanket partial holds: a silently ignored item use is a lost item or a missing buff. **This is the milestone's largest invisible item: 62 declarations that stay unported** (§8 risk 3). |
| **D7** | **The effect subset is the 14 classes of §2.6.** The three not needed on the start maps (`DispelEffect`, `FearEffect`, `MpAttackInstantEffect`, 7 sites) are **W but recommended**: they close material skills for the whole world with geo on. | 247 Java lines; the alternative is a known throw per second on every material they touch elsewhere. |
| **D8** | **`CM_MANASTONE` is ported whole; its arms 1, 2, 3 and 8 reach `EnchantItemAction`, `ItemSocketService` manastone bodies, `StigmaService.chargeStigma` and `EnchantService`, which stay unported.** Arms 1/2 call `EnchantItemAction`'s **non-virtual five-argument `act(player, stone, target, supplement, targetFusedSlot)`** (CM_MANASTONE.java:86; EnchantItemAction.java:86 — the varargs override ignores its parameters and calls it with `(null, 1)`, :81-83), so **h01 declares that overload too, stubbed `AION_UNPORTED`, with a `using AbstractItemAction::act;`** so the overload does not hide the virtual (rev 2). | A faithful packet with honest throws behind it; the godstone arm (4) is the only one this milestone needs (§2.6). Without the overload in h01 the player-side lane would need a P5-07 header change in the middle of the wave to port the packet whole. |
| **D9** | **Team loot is M5g.** The solo path is ported completely; the team arms of `DropService` (7 bodies) are **O** — port them faithfully if the loot lane has time, they then throw inside `LootGroupRules` (P5-10) until M5g; `DropDistributionService` (4), `TemporaryTradeTimeTask` (no file) and the four team packets wait. | §2.4 (b). |
| **D10** | **Loot-list order is not Java's.** `SM_LOOT_ITEMLIST` iterates a Java `HashSet` (SM_LOOT_ITEMLIST.java:26-33); the C++ constructor takes `std::unordered_set<Ptr<DropItem>>` (`SM_LOOT_ITEMLIST.h:26`) and the map holds `RcHashSet<Ref<DropItem>>`. The client keys entries by index, so the gate matches by index and never by position. A `docs/deviations/P5-09.md` row. | Neither order is specified; faking Java's hash order would be a C++-only complication with no observable benefit. |
| **D11** | **`groupMembers` empty stands for Java `null`** everywhere on `registerDrop`'s path (`NpcController.cpp:278`): `getQuestDrop`'s `players != null && player.isInGroup()` (QuestService.java:676), `dropCalculator`'s and `addDropItems`' member arms. A deviation row, and a unit case per arm. | The frozen signature already decided it; the callees must agree. |
| **D12** | **The M5b-3 gate is a new `gs.scenario.m5b3` + `gs.scenario.m5b3_geo`**, same binary and `RESOURCE_LOCK`, own schema pair, output directory and allow-list. The geo variant has **its own case**: the camp fire. | m5b2-plan.md D10's argument. Unlike M5b-1 and M5b-2, geo changes this milestone's behaviour (§2.6), so the geo gate asserts something the flat run cannot. |
| **D13** | **M5b-3 does not change the stress nightly.** G-08 (looting stress clients) is a proposal for the capacity conversation, not a work item the integrator takes (rev 2). | phase5-roadmap.md "Decisions along the way": the design of the capacity tests is the user's, after M5b-2 (`docs/design/capacity-proposals.md` collects the proposals). The re-green of D4 does not touch the stress run either (its clients do not fight). |

---

## 5. Work items

Effort: **S** < 1 agent-day, **M** 1-2, **L** 2-4, **XL** > 4. Need: **R** required, **W** stub-with-warning allowed, **O** optional.

### Integrator (stage 0, day 0)

| Id | What | Deps | Need | Eff |
|---|---|---|---|---|
| **I-01** | The header batch of §7 (`m5b3-h01`..`h03`): the item-action API over 32 classes with 30 new stub `.cpp` files and `EnchantItemAction`'s five-argument `act` (D8), the `ItemActions` lookups, the `QuestService::getQuestDrop` signature. **Before any stage-1 body**; h01 is a layout change and needs the reviewer. | – | R | M |
| **I-02** | Profiles: `gameserver.rates.drop = 0` in `m5b.properties.example` and the M5b-2 example; a new `m5b3.properties.example` (the M5b-2 set, which keeps `gameserver.event.service.disabled_events = *`, + `rates.drop = 1000000` + the potion and godstone keys of §10.1 made explicit), all in the **Java** tree beside `m5a.properties.example` (m5b-plan.md I-01's location). | – | R | S |
| **I-03** | Leases (rev 2): `services/QuestService.cpp` (P5-06, the four quest-drop bodies and their tests in `tests/quest`) and the **new** file `utils/stats/DropRewardEnumInfo.h` (P5-01) to the **loot** lane; `services/LegionService.cpp` (P5-11, one body) to the **items** lane; `CheckOutput.{h,cpp}` and `tests/app/CheckOutputTest.cpp` (P5-14) to the **gate-harness** lane, which owns P5-SC in stage 1. Each lease also covers one new test file in that chunk's test directory (`tests/quest`, `tests/stats`, `tests/legionhouse`; `chunks.py owner`). | – | R | S |

### Stage 1, loot (P5-09, + the P5-06 and P5-01 leases)

| Id | What | Java refs | Deps | Need | Eff |
|---|---|---|---|---|---|
| **L-01** | `DropRegistrationService` — close the `registerDrop` partial and port the 26 bodies: `createDropModifiers`, `initDropNpc`, `isAllowedDefaultGlobalDropNpc`, `addGlobalDrops`, `getReductionDropRate`, `calculateBoostDropRate`, `calculateEffectiveChance`, `addDropItems`, `regDropItem`, `hasGlobalNpcExclusions`, `checkRuleRestrictions` and its 10 predicates, `collectDrops`, `collectAllowedDrops`, `getItemCount` (the kinah formula: `long *= int × Math.pow(float product, 6)`, a Java compound assignment that narrows through `double`), `getRankModifier`, `getRatingModifier`. **Merges only in the same integrator commit as G-05** (D4): the lane hands L-01 over finished and reviewed, and the integrator lands both. | DropRegistrationService.java:52-474 | I-01 (h03), L-06 | R | L |
| **L-02** | `DropService`, the solo path: `scheduleFreeForAll`, `requestDropList`, `requestDropItem` ×2, `resendDropList`, `announceDrop`. The two stored callbacks exist in `fieldmap.json` (`DropService@L55:44` captures the npc id, `@L502:4` the player filter); `synchronized (dropItems)` ports as the `RcHashSet`'s own lock. | DropService.java:54-136, 272-440, 496-503 | T-01 | R | M |
| **L-03** | `DropService` team arms (`canDistribute`, `canAutoLoot`, `distributeEqually`, `winningRollActions`, `winningBidActions`, `winningNormalActions`) and `TempTradeDropPredicate::changeItem`. | DropService.java:188-270, 412-421, 442-526 | L-02 | O | M |
| **L-04** | `QuestService` quest drops (lease): `getQuestDrop` (against the h03 signature), `isQuestDrop`, `allowLooting`, `regQuestDropItem`. On a solo kill the first statement returns for an npc with no quest drop, and `isQuestDrop` answers false for a character without the quest (QuestService.java:754-760) — but 20 Poeta item ids go through it. **Named, not ported (lesson 2):** for a quest in `START` whose drop has a collecting step, `isQuestDrop` calls `QuestState::getQuestVarById` → `QuestVars::getVarById`, both `AION_UNPORTED` (`QuestState.cpp:41`, `QuestVars.cpp:25`, P5-06) — unreachable until a quest can start (M5d). A unit case pins the reachable arms (no quest state, state not `START`) and documents the throw for the `START` + collecting-step arm. | QuestService.java:666-760 | I-01 | R | S |
| **L-05** | Tests in `tests/economy` (and `tests/quest` for L-04): **rewrite `DropRegistrationServiceTest.cpp`'s three partial cases**; the rule-restriction table (each predicate true, false and absent), rank × rating, the kinah count with its truncation, the boost (repose, salvation, palace, rate) and the level reduction through `DropRewardEnum`; **the item-race filter and `checkRestrictionRace`, on a shipped rule whose candidates are race-specific** (119 of the 2,196 rules have one, e.g. "Candy Essence" in `rules_crafting_materials.xml`; the gate cannot see this filter because every candidate of 210663 and 210133 is `PC_ALL`, §2.4); **`SM_LOOT_STATUS`'s `lootEffectId` = 1003 for a drop set holding a godstone of DropItem.java:207-208 and 0 otherwise** (the gate sees one only by chance, Y1); **the event pass** (a rule in `getActiveEventDropRules`, and none with `disabled_events = *`); `requestDropList` refusals (`STR_LOOT_NO_RIGHT`, `STR_LOOT_FAIL_ONLOOTING`); the solo `requestDropItem` (stack merge, kinah, entry removed, `resendDropList` closing and deleting the corpse); `scheduleFreeForAll` on a `ManualClock` at 240 s; **a lifetime case: kill, loot everything, assert `DropNpc` and every `DropItem` reclaimed**. Mutation-proven. | – | L-01..L-04, L-06 | R | L |
| **L-06** | `utils/stats/DropRewardEnumInfo.h` (new P5-01 file under the I-03 lease, the `XPRewardEnumInfo.h` pattern): `dropRewardFrom`, `rewardPercent`, with its unit case. Rev 1's P-03, moved so that the loot lane depends on no other lane (D1). | DropRewardEnum.java | – | R | S |

### Stage 1, items (P5-07, + the P5-11 lease)

| Id | What | Java refs | Deps | Need | Eff |
|---|---|---|---|---|---|
| **T-01** | `ItemService` — the 11: every `addItem`, `addNonStackableItem`, `addStackableItem` (with the POWER_SHARDS equipment arm), `copyItemInfo`, `ItemUpdatePredicate::getUpdateType`. | ItemService.java:34-208 | T-02 | R | M |
| **T-02** | `ItemPacketService` — the 9, plus a companion header for the enum methods (`getKinahUpdateTypeFromAddType`, `ItemDeleteType.fromUpdateType`, `fromQuestStatus`; generated enums take "hand-written free functions in a companion header", `ItemPacketService_ItemUpdateType.h:12`). **This item alone closes E-13 and E-14.** Optional follow-up with a P4-13 lease: delete `Storage.cpp:34`'s local copy. | ItemPacketService.java:27-235 | – | R | M |
| **T-03** | `ItemMoveService` (3), `ItemRestrictionService` (3), `ItemSplitService` (4). | ItemMoveService.java:25-126; ItemRestrictionService.java:21-79; ItemSplitService.java:29-147 | T-02 | R | M |
| **T-04** | `SkillUseAction` — new `.cpp`: `canAct`, `act`, `isIneffectiveHealSkill`; `ItemActions::getItemActions` and the 8 other typed lookups (h02). | SkillUseAction.java:43-110; ItemActions.java:38-150 | I-01 | R | S |
| **T-05** | `ItemSocketService::socketGodstone` with its `ItemUseObserver` and 2 s `ITEM_USE` task (cycles rows `cycles.toml:273-274` are the specification; rev 2 said 272-273). | ItemSocketService.java:153-207 | T-02 | R | S |
| **T-06** | `StigmaService::notifyEquipAction`, the whole body (its stigma arm reaches `getPossibleStigmaCount` and `removeStigmaSkills`, unported, and throws for stigma items only). | StigmaService.java:42-100 | – | R | S |
| **T-07** | Tests in `tests/itemsvc`: `addItem` (new stack, merge, overflow, full cube message, kinah, power shards), the packet per storage type (cube, warehouse, legion kinah) **and per path — a merge sends `SM_INVENTORY_UPDATE_ITEM` alone, a new stack `SM_INVENTORY_ADD_ITEM` + `SM_CUBE_UPDATE`** (ItemPacketService.java:191-228), `moveItem` (same storage sends nothing, restricted → unlock packet, full, merge with `slot == -1`), `switchItemsInStorages` (both deletes before both adds), split and kinah split, **a split from the cube into the regular warehouse for a player without a legion (T-08's no-op)**, `socketGodstone` on a `DeterministicExecutor` (the 2 s task, abort through the observer), `SkillUseAction::canAct`'s refusal arms. Mutation-proven. | – | T-01..T-06, T-08 | R | L |
| **T-08** | `LegionService::addWHItemHistory` (P5-11, under the I-03 lease): the 10-line body. Without a legion it does nothing; a legion member's legion-warehouse arm reaches `addHistory` (`LegionService.cpp:384-386`, unported, M5h) and throws there. | LegionService.java:1082-1092 | – | R | S |

### Stage 1, player side (P5-13, P5-01 without the loot lane's new file, P4-12, P5-15, P5-16)

| Id | What | Java refs | Deps | Need | Eff |
|---|---|---|---|---|---|
| **P-01** | `PlayerRestrictions::canUseItem`, `canChangeEquip`. **O:** `canTrade`, `canChat`, `canInviteToGroup`, `canInviteToAlliance`, `canInviteToTeam` ("the rest of the restrictions", roadmap row 2; their callees in P5-08/P5-10 stay unported). | PlayerRestrictions.java:118-205, 240-389 | – | R / O | S |
| **P-02** | `ItemEquipmentListener::onItemUnequipment`, `removeStoneStats`. | ItemEquipmentListener.java:86-121, 211-219 | – | R | S |
| ~~P-03~~ | moved to the loot lane as **L-06** (rev 2, D1) | – | – | – | – |
| **P-04** | `Equipment` soul-bind accept and its observer and task (`Equipment$1..$3`; `cycles.toml:141-144` are the rows); remove the stale "ItemUseObserver has no C++ header" comment (`Equipment.cpp:135`). | Equipment.java:698-780 | T-02 | R | S |
| **P-05** | The 9 client packets of §2.7 with their `AION_CLIENT_PACKET` markers. `CM_MANASTONE` arms 1/2 call the five-argument `EnchantItemAction::act` h01 declares (D8). | CM_*.java | T-*, L-02 (run tests only) | R | M |
| **P-06** | Tests: byte vectors per packet (`tests/cm_ak`, `tests/cm_lz`), in-process run tests over `InWorldPacketRunSupport.h`; `canUseItem`'s decision table (`tests/instance/PlayerRestrictionsTest.cpp`); **an equip round trip asserting the item's stat functions are added and then gone** (`tests/stats`), the case that catches a missing `endEffect(item)`. | – | P-01, P-02, P-04, P-05 | R | M |

### Stage 1, effects (P5-03, P5-04)

| Id | What | Need | Eff |
|---|---|---|---|
| **E-01** | Potions and starter items: `ProcHealInstantEffect` 4, `ProcMPHealInstantEffect` 4, `XPBoostEffect` 1, `NoDeathPenaltyEffect` 1, `NoResurrectPenaltyEffect` 1, `HiPassEffect` 1. | R | S |
| **E-02** | Godstones (and the camp fire): `ProcAtkInstantEffect` 2, `PoisonEffect` 5, `SilenceEffect` 4, `BlindEffect` 4 + its observer, `ParalyzeEffect` 4. | R | M |
| **E-03** | The world's other material skills: `DispelEffect` 1, `FearEffect` 4 + 2 inner, `MpAttackInstantEffect` 2. | W (D7) | S |
| **E-04** | Tests in `tests/effects_al` / `tests/effects_mz` (the M5b-2 `EffectClassTestSupport.h` fixture): per class `calculate` → `applyEffect` → `startEffect` → `endEffect`; the heal cap at missing HP/MP; `TYPE.HP` vs `REGULAR` on the wire for potions; `XPBoostEffect`'s stat. | R | M |

### Stage 1, gate harness and re-green (P5-SC, `tools/oracle`, a P5-14 lease)

| Id | What | Need | Eff |
|---|---|---|---|
| **G-01** | `oracle.py m5b3-loot --npc --map --race --level --drop-rate`: the **applicable** rules (D3's definition; world key from the map's `drop_type`; event rules excluded and the exclusion stated) with their chances, candidate items, count ranges, the expected entry count, P(no drop) at the rate, the **deterministic merges** (single-candidate rules whose item is already a stack) and the **cube-slot budget per corpse** (entries − kinah − deterministic merges, worst case); `m5b3-item --item`: actions → skill → effect classes and values (keeping the **last** of duplicate skill templates, SkillData.java:33-39), `usedelay`, mask flags; `m5b3-material --map --near x,y,z`: skill-material placements from the geo files (extends `tools/oracle/geo/geofiles.py:48, 97`, resolving `|` mesh aliases). Oracle tests. **Re-derives every number of §2.4-§2.6**, including the 98 / 48 placements the review could not reproduce. | R | M |
| **G-02** | `decoders/ItemDecoders.{h,cpp}` (+ self-tests) for the nine packets of §2.7, written from `writeImpl`; `GameSession` builders for the nine client packets; a `ScenarioDatabase` helper that seeds an `inventory` row (a high object id above the `IDFactory` cursor, `IDFactory.h:26-32`) and a `player_life_stat.hp`. | R | M |
| **G-05** | **The re-green, moved from stage 2 (rev 2, D4) and merged in the same integrator commit as L-01:** `gameserver.rates.drop = 0` in the M5b test profile (`M5bScenarioTest.cpp:903-906` is where M5b sets its keys) and in the M5b-2 gate's; R3 rewritten against `SM_LOOT_STATUS(LOOT_ENABLE)` (`:2127-2137`, decoded by `decodeLootStatus`, `CombatDecoders.h:292`); Q2's `DropNpc` row to created = kills, live 0 (`:2299-2305`); the `DropRegistrationService.cpp:43` §A rows deleted from `m5b_partial_allowlist.txt:46-50` and the M5b-2 list. Then re-run `gs.scenario.m5a`, `m5b`, `m5b_geo`, `m5b2`, `m5b2_geo`, `gs.smoke.startup(_geo)`, and record before/after in the wave report. | R | M |
| **G-06** | `CheckOutput` (P5-14 lease): `DropNpc` from `zeroLiveClasses()` to a bounded row, `DropItem` into `summaryLiveClasses()` (D5); `tests/app/CheckOutputTest.cpp:496` flipped, the stale messages at `:552-553` and `:585` reworded. | R | S |

### Stage 2

| Id | What | Deps | Need | Eff |
|---|---|---|---|---|
| **G-03** | `TEST(M5b3Scenario, Run)`, §10, registered as `gs.scenario.m5b3` in `ScenarioTests.cmake`, `tests/scenario/m5b3_partial_allowlist.txt`. | stage 1 | R | L |
| **G-04** | `gs.scenario.m5b3_geo`: the same script with geo on, plus the camp-fire case (§10.5). **First measure which coordinates the TOUCH check counts as touching the fire's mesh** (§13 item 3; how the observer is attached is now answered). | G-03 | R | M |
| **G-07** | Fixups the gate names; then the earlier gates re-run once more on the final tree. | G-03 | R | M |

### Offered to the capacity conversation, not scheduled (D13)

| Id | What | Need | Eff |
|---|---|---|---|
| **G-08** | A proposal for `docs/design/capacity-proposals.md`: stress clients that kill at a high drop rate, loot every corpse and drink potions; 0 live `DropItem` at the end, `DropNpc` within its bound. **Waits for the user's capacity-test design** (phase5-roadmap.md). | – | M |

### Deferred

| Id | What | Milestone |
|---|---|---|
| O-01 | Team loot: `DropDistributionService` (4), `LootGroupRules` (7, P5-10), `PlayerTeamDistributionService`, `CM_GROUP_LOOT`, `CM_CLIENT_COMMAND_ROLL`, `CM_DISTRIBUTION_SETTINGS`, `CM_GROUP_DISTRIBUTION`, L-03 if not taken. (`TemporaryTradeTimeTask` left this row for M5c's P-04: the exchange asks it first, m5c-plan.md §3a; edited by M5c's I-01, §20) | M5g |
| O-02 | Manastones, enchant, amplify, tempering, stigma: `EnchantService` (11), `EnchantItemAction`, `ItemSocketService` manastone bodies (7), `StigmaService` (11 more), `CM_COMPOSITE_STONES` + `CompositionAction` (no file) | per m5c-plan.md §3a (edited by M5c's I-01, §20): `EnchantService`, `EnchantItemAction`, the manastone bodies, `ExtractAction` → M5c stage 0; `StigmaService` → M5e; tempering, `CM_COMPOSITE_STONES` + `CompositionAction` → the capital-economy milestone (m5c-plan.md D2) |
| O-03 | The other 31 item actions (62 stubs + ~70 undeclared private and inner bodies): QuestStart/Read → M5d; Ride, ToyPetSpawn, AdoptPet → pets; the rest → M5c | per m5c-plan.md §3a (edited by M5c's I-01, §20): Decompose, Remodel, Tuning, ExpandInventory, CraftLearn → M5c; SkillLearn → M5e; QuestStart, Read → M5d; MultiReturn, InstanceTimeClear → M5f; Decorate, SummonHouseObject → M5h; ApExtract → M5i; AdoptPet, ToyPetSpawn, Ride, AnimationAdd, EmotionLearn, TitleAdd, CosmeticItem, FireworksUse, Megaphone, ExpExtract → M5j; Charge, Tampering, Polish, Dye (its house arm M5h), AssemblyItem, Pack → the capital-economy milestone |
| O-04 | Npc warehouse and cube expansion (`WarehouseService` 5, `CubeExpandService` 7), `ItemActionService` (identify/tune), `CM_TUNE`, `CM_TUNE_RESULT`, `CM_ITEM_REMODEL`, `CM_ITEM_PURIFICATION`, `CM_CHARGE_ITEM`, `CM_SELECT_DECOMPOSABLE`, `CM_UNWRAP_ITEM`, `CM_APPEARANCE` | per m5c-plan.md §3a (edited by M5c's I-01, §20): `CubeExpandService`, `ItemActionService`, `CM_TUNE`, `CM_TUNE_RESULT`, `CM_SELECT_DECOMPOSABLE` → M5c stage 1; `WarehouseService`, `CM_ITEM_REMODEL`, `CM_ITEM_PURIFICATION`, `CM_CHARGE_ITEM`, `CM_UNWRAP_ITEM` → the capital-economy milestone (m5c-plan.md D2); `CM_APPEARANCE` → M5j |
| O-05 | m5b-plan.md O-08, the six start-map root AI handlers (§3): `QuestItemNpcAI`, `ActionItemNpcAI` → M5d; `PostboxAI` → M5c; `PortalDialogAI`, `ResurrectAI` → M5f; `AbyssGuardSimpleAI` → M5j | as named |
| O-06 | `QuestState::getQuestVarById` / `QuestVars::getVarById` behind `isQuestDrop` (L-04) | M5d |

---

## 6. Lanes

At most six lanes per stage; chunks disjoint within a stage.

| Stage | Lane | Chunks | Items | Tests |
|---|---|---|---|---|
| 0 | integrator | manifest, headers, profiles | I-01..I-03 | header check, full build |
| 1 | **loot** | **P5-09**; leases: P5-06 `QuestService.cpp`, P5-01 new `DropRewardEnumInfo.h` | L-01, L-02, L-04..L-06 (L-03 if time) | `tests/economy`, `tests/quest`, `tests/stats` (L-06) |
| 1 | **items** | **P5-07**; lease: P5-11 `LegionService.cpp` | T-01..T-08 | `tests/itemsvc`, `tests/legionhouse` (T-08) |
| 1 | **player-side** | P5-13, P5-01 (the two listener bodies), P4-12, P5-15, P5-16 | P-01, P-02, P-04..P-06 | `tests/instance`, `tests/stats`, `tests/player`, `tests/cm_ak`, `tests/cm_lz` |
| 1 | **effects** | P5-03, P5-04 | E-01..E-04 | `tests/effects_al`, `tests/effects_mz` |
| 1 | **gate-harness** | **P5-SC**, `tools/oracle`; lease: P5-14 `CheckOutput.{h,cpp}` + `tests/app/CheckOutputTest.cpp` | G-01, G-02, **G-05, G-06** | `tools.oracle`, decoder self-tests, `tests/app`, **the six earlier gates** |
| 2 | **gate** | P5-SC | G-03, G-04 | `gs.scenario.m5b3`, `_geo` |
| 2 | **fixups** | as named | G-07 | owning tests + every gate re-run |

Five lanes in stage 1, two in stage 2 (rev 1 had four in stage 2: its regate and census lanes are now stage-1 work of the gate-harness lane).
The P5-01 split between the loot lane (one new file) and the player-side lane (`ItemEquipmentListener.cpp`) is a file lease, not shared
ownership: I-03 names the file.

**Merge order in stage 1.** I-01 (day 0) → **T-02** (it unblocks every other storage change and closes E-13/E-14 — merge it the day it is
green) → T-01 → L-06 → L-04 → L-02 → T-03..T-06, T-08 → P-01/P-02/P-04 → P-05 → E-* → G-06 → **L-01 + G-05 together, in one integrator
commit** (rev 2): L-01 alone turns the M5b gate red on every run, and G-05 alone asserts a `LOOT_ENABLE` that no body sends yet. G-01/G-02
start on day 1 with no dependency; G-05 is written against the stage-0 tree and verified on the combined commit, with the full re-run of the
six earlier gates.

**Critical path, re-budgeted (rev 2).** On §5's own scale (S < 1, M 1-2, L 2-4 agent-days) rev 1's "3-4 days per lane" did not add up. Taking
S ≈ 0.5, M ≈ 1.5, L ≈ 3: **loot** L-01 3 + L-02 1.5 + L-04 0.5 + L-05 3 + L-06 0.5 ≈ **8.5 agent-days** (range 5-11); **items** T-01..T-03 4.5 +
T-04..T-06, T-08 2 + T-07 3 ≈ **9.5** (range 6-12); player-side ≈ 5; effects ≈ 4 (14 small classes, 618 lines; it should finish first and then
help the gate harness); gate-harness ≈ 5.5. **Stage 1 is therefore ~8-10 days on the critical path, not ~4-5** — the same size as M5b-2's two
long stage-1 lanes, which were budgeted 7-9 days (m5b2-plan.md §6). The tests cannot be split off into a parallel lane: `tests/economy` is P5-09
and `tests/itemsvc` is P5-07 (`chunks.py owner`), so a separate tests lane would share its chunk. Two levers shorten it: **(a) D1** (the
user's), which starts both long lanes during M5b-2's stages 2-3; **(b)** if D1 is declined and the items lane runs long, the integrator may split
P5-07 along the seam between `services/item/**` and the item actions plus the named services (`chunks.cmake:306-315`; the P5-02 → P5-02a/b
split of `2e47bbb64` is the precedent) and give T-04, T-06 and their tests to a sixth lane — about 1.5 days off the items lane.

**If D1 is accepted:** loot and items start beside M5b-2 stage 2 (its four lanes + these two = six) once I-01 has landed (h01 and h02 touch
only P5-07 headers, h03 only `QuestService.h`: none is a file M5b-2 touches). Both lanes merge as they go **except L-01**, which waits,
reviewed, until M5b-2's gate, regate and stage-3 lanes have released P5-SC; then M5b-3's gate-harness lane writes G-05 on top of M5b-2's
final `M5bScenarioTest.cpp` and the integrator lands L-01 + G-05 together. Player-side, effects and gate-harness start when M5b-2 closes.

---

## 7. Header requests expected

| Request | Kind | For |
|---|---|---|
| **m5b3-h01** `model/templates/item/actions/AbstractItemAction.h` (P5-07): `virtual bool canAct(player::Player&, gameobjects::Item& parentItem, runtime::Ptr<gameobjects::Item> targetItem, std::initializer_list<std::any> params = {}) const = 0;` and `virtual void act(… same …) const = 0;` (D6); **`override` declarations with `AION_UNPORTED` stubs in the 32 bound action classes** (30 new `.cpp` files; `DecomposeAction.cpp` and `EmotionLearnAction.cpp` exist); **and in `EnchantItemAction.h` the non-virtual overload `void act(player::Player&, gameobjects::Item& parentItem, gameobjects::Item& targetItem, runtime::Ptr<gameobjects::Item> supplementItem, int32_t targetWeapon) const;` with `using AbstractItemAction::act;`, stubbed `AION_UNPORTED`** (rev 2, D8) | **layout** (new pure virtuals on a base every action shell derives from; the precedent is `shells-3`) | T-04, P-05; `CM_MANASTONE` constructs `EnchantItemAction` directly and calls `canAct` and the five-argument `act` (CM_MANASTONE.java:79-86, EnchantItemAction.java:86). **Applied in stage 0 with `runtime::Ptr<gameobjects::Item> parentItem` in `canAct`/`act` (§15 item 1)** |
| **m5b3-h02** `ItemActions.h` (P5-07): `const std::vector<std::unique_ptr<AbstractItemAction>>& getItemActions() const;` and the 8 typed lookups Java declares (`getEnchantAction`, `getHouseObjectAction`, `getDecorateAction`, `getDyeAction`, `getAdoptPetAction`, `getRemodelAction`, `getTuningAction`, `getRideAction`) | additive (`items-3` / `controllers-1` pattern) | T-04, P-05 |
| **m5b3-h03** `services/QuestService.h` (P5-06): `getQuestDrop(runtime::RcHashSet<runtime::Ref<model::drop::DropItem>>& dropItems, …)` instead of `const std::unordered_set<runtime::Ptr<…>>&` | **signature** (Java adds to the set, QuestService.java:682-728; the caller's set is `DropRegistrationService.h:30`'s) | L-04 |
| none for `DropService::resendDropList` / `SM_LOOT_ITEMLIST` (`std::unordered_set<Ptr<DropItem>>`): the body passes a snapshot of the live set (D10) | – | L-02 |
| none for the 14 effect classes: their overrides were declared by `m5b2-f04` | – | E-* |
| new files, no request: the `ItemPacketService` enum companion (P5-07), `DropRewardEnumInfo.h` (P5-01, written by the loot lane under the I-03 lease), the nine `CM_*.{h,cpp}` (P5-15/16), `SkillUseAction.cpp` and the other stub `.cpp`s (P5-07, inside h01), `ItemDecoders.{h,cpp}` (P5-SC) | – | – |
| none for `LegionService::addWHItemHistory` (T-08) and `CheckOutput` (G-06): bodies and a list in existing files | – | – |

---

## 8. Risks

Ordered by what is most likely to go wrong, with the evidence.

1. **The earlier gates break on the first green loot commit — always, and then sometimes** (finding 1). *Always:* R3's `ASSERT_TRUE` on the
   `DropRegistrationService.cpp:43` row, Q1's "every §A row hit at least once" and Q2's `DropNpc` created 0 fail on every run once the partial is
   gone, whatever the drop rate (§2.3's table). *Sometimes:* a 300 s corpse fails M5b R4 (no `SM_DELETE` within 45 s) and Q3 (npc live above the
   baseline once the respawn is in), and a stop within 5 minutes of the kill trips the `zeroLiveClasses` ERROR — on 58.2 % of runs for 210663,
   78.5 % for 210133. **Rev 1 scheduled the fix (G-05, G-06) for stage 2 while closing the partial in stage 1; rev 2 moves both into stage 1
   and lands L-01 + G-05 in one integrator commit** (§6), and the M5b-2 gate, which kills both monsters, gets the same key and loses its §A row
   in that commit. Under D1 the commit waits for M5b-2 to release P5-SC.
2. **A loot lifetime leak looks like a leaked Player.** `DropNpc.lootingPlayer` and `DropItem.winningPlayer` are `Field<Ref<Player>>`
   (`DropNpc.h`, `DropItem.h:28`) inside an `Immortal`'s map. Solo, `lootingPlayer` is cleared by `closeDropList` and by
   `PlayerController::onDespawn` (`PlayerController.cpp:442-445`); `winningPlayer` is set only on the team arms (`cycles.toml:127` resolves it
   "cut elsewhere: LootGroupRules"). **A `requestDropList` that throws after `setLootingPlayer` pins the looter until the corpse decays** — the
   shape of m5b-client-session.md S-2. L-05's lifetime case and Q-rows Y14 are the checks.
3. **The item-action batch hides ~130 bodies behind 62 stubs** (D6). After h01 every action class *looks* ported to a file listing and to a
   reviewer who greps for missing files; it is not. Manastones drop at 15 % per Poeta kill and a player will try one. The real-client
   checklist (§11 step 11) names the actions that throw, and `unported_trace.txt` of that session is the entry list for M5c.
4. **The reachable-today holes will show up before M5b-3 does** (E-13, E-14). If the user plays after M5b-2 on the 2026-09-21/22 characters,
   the Boon expiry and *Bandage Heal* throw in `ItemPacketService`. **T-02 is 9 small bodies with no dependency**: if D1 is declined, consider
   landing T-02 alone as an M5b-2 fixup (it is P5-07, which M5b-2 does not touch).
5. **Random item picks make a brittle gate** (D3). Every rule's pick and count are random; only the rule set, the entry count, the indexes,
   the candidate sets and the single-candidate merges (§2.4: the power shard at L4, the shard and the junk at L6c) are exact. An assertion on a
   specific item id, or on a count that a random merge can change (rev 1's "potion count 99"), is a flake: every count the gate asserts is
   derived from the packet before it. The cube has 27 slots (`StorageType.java:7`): 9 starter stacks + up to 10 new stacks from the first
   210663 + up to 8 from 210133 (kinah takes no slot, the shard merges) + the unequipped sword = 28, and the second 210663 at L6c adds up to 8
   more (rev 1 left that third corpse out). So the script destroys looted stacks it no longer needs before each kill (L3b), to the oracle's
   worst-case budget, and never loots into a full cube — where Java legally keeps the rest of the entry in the corpse and sends
   `STR_MSG_DICE_INVEN_ERROR` (ItemService.java:94-95), an outcome that would break Y3's and Y14's counts.
6. **Concurrency on the loot set.** `requestDropItem` iterates and removes under `synchronized (dropItems)` (DropService.java:286-296,
   392-396) while `SM_LOOT_STATUS`'s constructor streams the same set unsynchronized (SM_LOOT_STATUS.java:23, 33-35) and a pet may auto-loot from
   `onDie`. Port the monitor as the `RcHashSet` lock and mark Java's unsynchronized reads `// java-race`.
7. **The kinah formula is float-to-long arithmetic.** `count *= npc.getLevel() * Math.pow(rank * rating, 6)` multiplies a `long` by a `double`
   and truncates (DropRegistrationService.java:447-453); `(1.05f)^6` in `float` then `double` is not `1.05^6`. Golden vectors from the Java
   expression, in Debug and RelWithDebInfo (m5b2-plan.md risk 11).
8. **`registerDrop` now runs code on every kill in the world that runs AI** — including rules with zones (51) that call `isInsideZone`, event
   rules, and `ask(REWARD_LOOT)` arms of the registered AI handlers. A throw there is swallowed by `NpcController::onDie`'s catch
   (`NpcController.cpp:176-185`) and skips `InstanceHandler::onDie` and `DIED` — S-1's failure mode. The gates' "no ERROR line" row must not
   be relaxed.
9. **Geo gate mechanics are half verified** (G-04). Answered from the code (rev 2): a material zone attaches its actor when a creature
   **enters the zone** — `MaterialZoneHandler::onEnterZone` creates a `ZoneCollisionMaterialActor` with a TOUCH check (PASS only for materials
   14-16), adds it as an observer and calls `moved()` (`MaterialZoneHandler.cpp:47-69`), so any position update the server accepts, a fake
   client's `CM_MOVE` included, can attach it; and the task **skips a player while spawn protection is active** (`AbstractMaterialSkillActor.cpp:70`),
   so the case must first end it (any action does). Still open: which reported coordinates the TOUCH test counts as touching the fire's mesh —
   G-04 measures it before writing Y15.
10. **The header batch is a layout change across 32 classes** (h01). Every action's generated binder instantiates the concrete class; a class
    without the override is abstract and the binder stops compiling. Land it on day 0 as one reviewed commit, as `shells-3` did.
11. **A godstone proc is not a hit** (rev 2, §2.6). Skill 8267 is MAGICAL and resistible, the proc message is sent even for a resisted proc,
    and the level-2 monster dies within a few hits; rev 1's Y7 ("every hit procs with value > 0") would have failed a correct server. Y7 now
    counts proc messages per evaluated hit and asks for at least one landed proc; the residual chance that every proc of the fight is resisted
    is small and reported by the gate, not hidden. (Rev 2 added "8267 has two templates"; stage 1 found it has one, EARTH, §2.6 (b), §16.)
12. **Under D1, finished loot work sits unmerged** (rev 2). L-01 is written and reviewed early but cannot land before M5b-2 releases P5-SC; the
    longer it waits, the more `M5bScenarioTest.cpp` moves under G-05. G-05 is therefore written last, on M5b-2's final version of the file,
    and L-01 is re-verified against the tree it finally lands on.

---

## 9. The split: two implementation stages and a gate stage

| Stage | What a player can do at the end | Chunks | Bodies | Lanes |
|---|---|---|---|---|
| **0 — the declarations** | nothing new; the tree builds with the item-action API | P5-07, P5-06 headers | 0 (+ 64 stubs, + the `EnchantItemAction` overload) | integrator, ~1 day |
| **1 — loot and items** | Loot a corpse, kinah included; keep, stack, split, move to the warehouse, swap, destroy; equip and unequip with the stats following; drink a potion; socket a godstone and see it proc; stand in a camp fire and burn. **Every earlier gate green again, in the same commit that closes `registerDrop`** | P5-09, P5-06 / P5-01 / P5-11 / P5-14 (leases), P5-07, P5-13, P5-01, P4-12, P5-15, P5-16, P5-03, P5-04, P5-SC, oracle | **~145 R** (+ ~20 O) | **5 lanes**, **~8-10 days** on the critical path (rev 1: ~4-5; §6) |
| **2 — the gate proves it** | the same, proved | P5-SC, fixups | ~5 + fixups | **2 lanes**, ~3-4 days |
| — | the stress nightly loots | not scheduled: a proposal for the capacity conversation (D13) | – | – |

**Why not split stage 1.** It is about half of M5b-2's stage 1 (~145 against ~360 bodies) and its lanes are balanced by body count (loot ~44,
items ~46, player-side ~31 with the packets, effects 41) — its length comes from the two long lanes, not from its size, and splitting the stage
does not shorten a lane. **If it has to split, split at godstones and materials**: land loot, items, equip, potions and kinah with E-01 (a gate
without L6/L6b/L6c and without the geo case), and take E-02/E-03, `socketGodstone`, `CM_MANASTONE` and G-04 second. The split point is clean
because godstones touch nothing else in the milestone.

---

## 10. Gate specification (`ctest -L scenario`, `gs.scenario.m5b3`)

### 10.1 Processes, databases and profile

Identical to m5b2-plan.md §10.1 except:

| Piece | M5b-3 |
|---|---|
| Schemas / output | `aion_{ls,gs}_test_m5b3_<hash>`, `<bin>/scenario/m5b3`, same `SchemaLease`, same `RESOURCE_LOCK` |
| Profile | the M5b-2 set — which keeps the M5a set's `gameserver.event.service.disabled_events = *` (`ScenarioServers.cpp:31`), so no event drop rule applies (§2.4) — + **`gameserver.rates.drop = 1000000`** (D3), `gameserver.items.ignore_potions_at_full_health = false` (the default, CustomConfig.java:243-244), `gameserver.rates.godstone.activation.rate = 1.0` and `…evaluation.cooldown_millis = 750` (the defaults, :273-277), `gameserver.drop.announce_quality = MYTHIC` (drop.properties), `gameserver.geodata.enable = false` (`true` for `_geo`). Written out as `m5b3.properties.example` (I-02) |
| Allow-list | `tests/scenario/m5b3_partial_allowlist.txt`: **no drop row**; §A the M5a startup rows; §B nothing new; §C M5b's timing rows |
| Character | the M5b-1 Elyos Warrior (account A). **Seeds before its second enter world** (G-02): one `inventory` row of 168000116 at a high object id; `player_life_stat.hp` low before the potion case (m5b-plan.md D12) |
| Monsters | **210663** (loot, godstone proc) and **210133** (kinah), spots chosen by `m5b-monster`, rule sets by `m5b3-loot` — never hardcoded |

### 10.2 Cases

C1-C3 as M5b-1 (login, create, enter world, level ready). L0: `oracle.py m5b3-loot --npc 210663` and `--npc 210133` and `m5b3-item` for
162000002, 164002116, 168000116 answer.

| # | Case | Steps |
|---|---|---|
| **L1** | the kill | approach 210663 as M5b K4b does, attack until it dies; record from `SM_DIE` on |
| **L2** | open the corpse | `CM_START_LOOT(corpse, 0)` |
| **L3** | loot it empty | `CM_LOOT_ITEM(corpse, index)` for every listed index, in list order |
| **L3b** | make room (rev 2) | before each later kill (L4, L6b): `CM_DELETE_ITEM` on looted stacks no later case uses — never the starter potions, the shard stack, the junk stacks (L6c, L8, L10, L11) or the sword — until free slots ≥ the oracle's worst-case new-slot count for the next corpse + 1 (the unequipped sword at L5/L6). Each delete is checked with Y11's shape |
| **L4** | kinah | kill 210133, open, loot everything |
| **L5** | unequip and re-equip | `CM_EQUIP_ITEM(1, 0, sword)`, then `CM_EQUIP_ITEM(0, MAIN_HAND, sword)` |
| **L6** | socket a godstone | `CM_QUIT`, seed 168000116, re-enter; unequip the sword; `CM_MANASTONE(4, 0, sword, stone, 0)`; wait 2.5 s; equip |
| **L6b** | the proc | end spawn protection, fight the respawned 210663 **until it dies** |
| **L6c** | loot the second corpse (rev 2) | `CM_START_LOOT`, then `CM_LOOT_ITEM` for every listed index — rev 1 left this corpse unlooted, which Y14 would then have caught on a correct server |
| **L7** | a potion | quit, seed HP low, re-enter; `CM_USE_ITEM(162000002 stack, 0)`; 1 s later the same again |
| **L8** | move | `CM_MOVE_ITEM(junk stack, 0 → 1, -1)`; `CM_MOVE_ITEM(164002116, 0 → 1, -1)` (not storable in a warehouse, `mask="12352"`); `CM_MOVE_ITEM(potion stack, 0 → 0, other slot)` |
| **L9** | split | `CM_SPLIT_ITEM(life potions, n, 0, 0, 0, -1)` |
| **L10** | destroy | `CM_DELETE_ITEM(a looted junk stack)` |
| **L11** | swap | `CM_REPLACE_ITEM(0, cube item, 1, warehouse item)` |
| **L12** | persistence | `CM_QUIT(0)`, read `inventory` and `item_stones`, re-enter |
| **L13** | reports and shutdown | the M5a Q8 bar plus the rows below |

### 10.3 Assertions

| # | Case | Assertion | Proves / cannot prove | Mutation it kills |
|---|---|---|---|---|
| **Y1** | L1, L4, L6b | Per kill, exactly one `SM_LOOT_STATUS(corpse, LOOT_ENABLE)` to the killer, and its `lootEffectId` is 1003 **iff** a godstone of DropItem.java:207-208 is among that corpse's entries (L2, L4, L6c), else 0. | **Proves:** `registerDrop` ran to its end (the packet is its last statement but one), the solo `initDropNpc`, and `getLootEffect` reading the live set. **Cannot prove:** the free-for-all broadcast 240 s later (a unit case, L-05), nor a wrong `lootEffectId` reliably: only 4 of the 17 Legend and 4 of the 17 Unique illusion godstones are listed, so a listed one is in a corpse with P = 1 − (13/17)² = **41.5 %** per kill (≈ 80 % over the run's three kills) — the "hardcoded 0" mutation is L-05's (rev 2). | the partial left in place (no packet); `allowedLooters` empty |
| **Y2** | L2, L4, L6c | `SM_LOOT_ITEMLIST(corpse)` with **exactly the oracle's entry count** (10 for 210663 and 10 for 210133 at the §10.1 rate), indexes exactly `{1..n}`, **a bijection between entries and applicable rules** (D3: restrictions pass *and* a non-empty candidate set after the race and level filter) by candidate-set membership, counts inside each rule's range; then `SM_LOOT_STATUS(OPEN_DROP_LIST)` and an `SM_EMOTION(START_LOOT)` naming the corpse. | **Proves:** the whole rule evaluator (a missing predicate adds or drops a rule; a wrong `min_diff`/`max_diff` filter moves an item out of its set or adds 210133's "Armor (Common)" entry), `max_drop_rule`, `regDropItem`'s index, `requestDropList`. **Cannot prove:** the per-rule chances (at this rate all are certain; `calculateEffectiveChance` is L-05's), Java's list order (D10), or the item-race filter — every candidate of both monsters is `PC_ALL` (§2.4), so that filter is L-05's (rev 2). | `checkGlobalRuleNpcGroups` answering true (junk of every group appears: count grows); the `min_diff..max_diff` filter dropped (210133 gets 11 entries; out-of-range items appear for 210663); `isAllowedDefaultGlobalDropNpc` losing its Poeta/Ishalgen exemption (210133, level 1, drops nothing: Y2 and Y4 fail); `collectDrops` ignoring `max_drop_rule` (more entries than rules) |
| **Y3** | L3, **L4**, L6c | Each `CM_LOOT_ITEM` yields **either** a new stack — `SM_INVENTORY_ADD_ITEM` then `SM_CUBE_UPDATE` (ItemPacketService.java:214-228) — **or** a merge into an existing stack of that id — `SM_INVENTORY_UPDATE_ITEM` alone, count = old + looted, **no `SM_CUBE_UPDATE`** (:191-204) — then an `SM_LOOT_ITEMLIST` one entry shorter; after the last: `SM_LOOT_STATUS(CLOSE_DROP_LIST)`, `SM_EMOTION(END_LOOT)` and **`SM_DELETE(corpse)` within 1 s** — not 2 s, not 300 s. **The merge arm is certain at L4** (the Minor Power Shard, "Power Shards"' only candidate for both monsters, merges into L3's stack) **and at L6c** (the shard and the Sparkie Carapace Fragment); at L3 a merge happens only if "Potions (Common)" picks a starter potion (50 %). | **Proves:** `requestDropItem` → `addItem` → stack merge → `Storage.add` → `ItemPacketService`, both packet paths, entry removal, `resendDropList`'s delete. **Cannot prove:** the team arms (D9). | `addStackableItem` never merging (an ADD for the shard at L4 — **deterministic**, rev 2); an entry not removed (the list does not shrink); `resendDropList` not deleting (the corpse lives 300 s) |
| **Y4** | L4 | The kinah entry's count k ∈ [5, 25] (oracle); looting it sends **`SM_INVENTORY_UPDATE_ITEM` for the kinah item with count K + k, where K is the kinah of the last packet before it (1,000 at creation), and no `SM_INVENTORY_ADD_ITEM`**. | **Proves:** `getItemCount`'s kinah arm, `addItem`'s kinah branch, `increaseKinah`'s packet. **Cannot prove:** the `pow(…, 6)` term for anything but 1.0 (rank DISCIPLINED × rating NORMAL); L-05's golden vectors carry the rest. | kinah added as an item (an ADD packet, a second kinah stack); the count multiplied twice |
| **Y5** | L5 | Unequip: `SM_INVENTORY_UPDATE_ITEM(sword, EQUIP_UNEQUIP)`, `SM_UPDATE_PLAYER_APPEARANCE` without the sword, an `SM_STATS_INFO` whose main-hand attack is **lower**; re-equip: the same packets and an `SM_STATS_INFO` whose **stat fields** equal the ones before the unequip — the stats' current values from max HP to spell fortitude (SM_STATS_INFO.java:64-117) and the base block (from :152) — and **not** the current HP, MP, DP and FP (:65, :68, :71, :74), the fly state and movement mask, experience (:58-60), inventory size (:119), repose and salvation (:128-130), which regenerate or change with the cube between the two packets (rev 2; rev 1 compared every field). | **Proves:** `canChangeEquip`, `notifyEquipAction`, `updateItemAfterEquip`, `onItemUnequipment`'s `endEffect(item)` and the re-add — the round trip is the check. **Cannot prove:** retail stat values (self-consistency only). | `onItemUnequipment` not ending the item's functions (attack not lower); `onItemEquipment` adding twice (attack higher after the round trip) |
| **Y6** | L6 | `SM_ITEM_USAGE_ANIMATION(time 2000)` at once; ≥ 1,900 ms later the closing animation, the stone's `SM_DELETE_ITEM` (count 1), `STR_GIVE_ITEM_PROC_ENCHANTED_TARGET_ITEM` and an `SM_INVENTORY_UPDATE_ITEM(sword)` whose blob carries godstone 168000116. | **Proves:** `CM_MANASTONE` arm 4, `socketGodstone`'s task and observer, `decreaseByObjectId`, `updateItemAfterInfoChange`. **Cannot prove:** the abort path (a unit case). | the task run at once (no 2 s gap); the stone not consumed |
| **Y7** | L6b | Over the fight: **exactly one `STR_SKILL_PROC_EFFECT_OCCURRED` (skill 8267) per evaluated hit** — an auto-attack that does not kill, whose `SM_ATTACK` status is neither DODGE nor RESIST and that comes ≥ 750 ms after the previous evaluation (the godstone's `probability="1000"` makes every evaluation a proc; GodStone.java:37-49, CreatureController.java:250-256, 268-283) — and **at least one `SM_ATTACK_STATUS` on the monster with the PROCATKINSTANT log and value > 0**. Rev 2: rev 1 asked every hit for a damage packet with value > 0, but 8267 is MAGICAL and resistible and the proc message is sent for a resisted proc too (§2.6); the gate prints the number of evaluated hits k and the per-proc resist chance, so the residual "every proc resisted" case is visible, not hidden. | **Proves:** the proc path, `tryActivate` and its cooldown, and `ProcAtkInstantEffect`. **Cannot prove:** the damage number (magical arithmetic, m5b2-plan.md D8), the break path (`breakprob="0"`), or that the godstone survives a relog (no relog between L6 and L6b: Y12's). | `ProcAtkInstantEffect::applyEffect` a no-op (no damage packet at all); `tryActivate`'s cooldown never expiring (one message for k > 1 evaluations). A cooldown not applied at all is **not** visible: the Warrior's swings are already more than 750 ms apart |
| **Y8** | L7 | First use: `SM_ITEM_USAGE_ANIMATION`, an `SM_ATTACK_STATUS` of type HP (item heal) with **value exactly min(37, maxHp − hp)**, `SM_INVENTORY_UPDATE_ITEM(stack, DEC_ITEM_USE)` with **count c − 1, where c is the stack's count in L7's enter-world `SM_INVENTORY_INFO`** (rev 2: rev 1's fixed 99 ignored loot merges — "Potions (Common)" merges into the 162000002 stack with P = 1 − (3/4)³ ≈ 58 % over the run's three kills), an `SM_ABNORMAL_STATE` holding skill 9889 with ~20,000 ms; second use: `STR_ITEM_CANT_USE_UNTIL_DELAY_TIME` and **no count change**. | **Proves:** `CM_USE_ITEM`, `canUseItem` incl. the cooldown, `getItemActions`, `SkillUseAction`, `payCastCosts`' item arm, `ProcHealInstantEffect` with its cap, `startCooldown`. **Cannot prove:** the `HealEffect` tick values beyond "37 per tick" if natural regeneration interleaves (type `NATURAL_HP` is excluded). | the cooldown check dropped (second use consumes); the potion consumed on refusal; `REGULAR` instead of `HP` on the wire |
| **Y9** | L8 | Junk to the warehouse: `SM_DELETE_ITEM(type MOVE)`, `SM_CUBE_UPDATE`, `SM_WAREHOUSE_ADD_ITEM(type 1)`, `SM_CUBE_UPDATE`; the event potion: `STR_WAREHOUSE_CANT_DEPOSIT_ITEM` and an unlock `SM_INVENTORY_ADD_ITEM(ALL_SLOT)`, no delete; the same-storage move: **no packet at all**. | **Proves:** `moveItem`'s three arms and `isItemRestrictedTo`. **Cannot prove:** the slot of the same-storage move (Y12 reads it). | the restriction ignored (the event potion lands in the warehouse); a packet on the same-storage arm |
| **Y10** | L9 | With c' the stack's count in the last packet about it (Y8's): `SM_INVENTORY_UPDATE_ITEM(stack, c' − n, DEC_ITEM_SPLIT)`, `SM_CUBE_UPDATE`, then `SM_INVENTORY_ADD_ITEM` of a new object with count n and `SM_CUBE_UPDATE` (ItemSplitService.java:85-88). | **Proves:** `splitItem`. **Cannot prove:** the cross-storage arm and its `addWHItemHistory` call (T-07/T-08 unit cases). | the source not decreased (duplication) |
| **Y11** | L10 | `SM_DELETE_ITEM(obj, DISCARD)`; the object is absent from Y12's rows. | **Proves:** `CM_DELETE_ITEM` → `Storage.delete` → `sendItemDeletePacket`. **Cannot prove:** the unbreakable refusal — every starter and looted item is `BREAKABLE`. | the delete type mask wrong |
| **Y12** | L12 | After `CM_QUIT`: `inventory` holds every stack with the count of the last packet the client got about it (looted, merged, split, not deleted), kinah K + k, the warehouse junk at `item_location = 1`, the two swapped items at each other's location and slot (Y16), the same-storage move's new `slot`, the split potions as two rows of c' − n and n, the sword's `item_stones` godstone row; after re-entry `SM_INVENTORY_INFO`/`SM_WAREHOUSE_INFO` list them and the sword's blob still carries godstone 168000116. | **Proves:** persistence through `InventoryDAO` (D17 of m5b-plan.md: no mid-run save), including the socketed godstone across a relog (moved here from Y7, rev 2). | a moved item that keeps `item_location = 0`; a socketed godstone not stored or not reloaded; a swap persisted on one side only |
| **Y13** | L13 | The M5a Q8 bar: `unported_trace.txt` empty, no ERROR, `partial_trace.txt` ⊆ the M5b-3 allow-list, **no `DropRegistrationService` site in it**. | **Proves:** nothing on the path threw, including in `onDie`'s catch. **Cannot prove:** actions other than `skilluse` (D6). | any unported body left on the path |
| **Y14** | L13 | `DropNpc` created 3 (two 210663, one 210133) live 0; `DropItem` live 0; `AttackResult`, `Effect` live 0; `Item` within its account bound. | **Proves:** every corpse was looted empty (L3, L4, **L6c**) and deleted and nothing kept a drop, a looter or an effect. **Cannot prove:** the 300 s path (the script never leaves a drop). | `resendDropList` not deleting; `unregisterDrop` skipped; a looter pinned by a thrown `requestDropList` |
| **Y16** | L11 | (rev 2; rev 1 had no row for the swap) `CM_REPLACE_ITEM(0, cube item, 1, warehouse item)` yields, in this order: `SM_DELETE_ITEM(cube item, MOVE)` + `SM_CUBE_UPDATE`, `SM_DELETE_WAREHOUSE_ITEM(1, warehouse item, MOVE)` + `SM_CUBE_UPDATE`, then `SM_INVENTORY_ADD_ITEM(the former warehouse item)` + `SM_CUBE_UPDATE`, `SM_WAREHOUSE_ADD_ITEM(the former cube item)` + `SM_CUBE_UPDATE` — "delete items, then add items" (ItemMoveService.java:115-125; each delete and add goes through ItemPacketService.java:178-185 and :214-228) — and each added item carries the other's former slot. | **Proves:** `CM_REPLACE_ITEM`, `switchItemsInStorages`, its restriction checks passing for storable items. **Cannot prove:** the restricted arm (the two unlock packets; a T-07 unit case). | the adds sent before the deletes (the client drops the items it just received); the slots not exchanged (Y12's rows) |

**Corrected 2026-09-24 (stage 2, §17.2):** Y5 (the sword's stats are no stat functions of the item; parry, accuracy and critical must fall
too), Y7 and Y15 (TYPE.DAMAGE goes on the wire negated: "value > 0" is a damage, a negative int), Y8 (the heal-over-time lives duration2 +
1000 = 21,000 ms), Y9 ("no packet at all" is no item or storage packet: the potion's ticks go on), Y12 (after L11 the junk is back in the
cube and the mana potions are in the warehouse), Y14 (the run-time drops are `RuntimeDropItem`; `Effect` live equals `effectsHeld`) and the
slots and budget of L3b and L8-L11. The gate follows the Java; §10 is left as it was reviewed.

**Corrected 2026-09-24 (the review of stage 2, §18):** **Y14 does not prove "deleted"** - the counts are written after the shutdown, which
deletes a corpse that outlived its loot, so `resendDropList` without `delete()` leaves `DropNpc` live 0 and `dropNpcsHeld` 0 (the review's
mutant RK1 failed Y3 alone); Y3's "`SM_DELETE(corpse)` within 1 s" is the only proof of the delete. **Y8** checks only the 37 arm of
min(37, maxHp − hp): L7 seeds 100 of 284 HP, so the cap arm is never reached (the review's RV2, `calculateHealValue` without the cap,
survived; on the wire it is equivalent while `CreatureLifeStats.increaseHp` clamps). **Y15** gained a negative case for the TOUCH ray
(§10.5, §18.2).

### 10.4 Mutation proof (the minimum set)

| Mutation | Must fail | Must stay green |
|---|---|---|
| `DropRegistrationService::registerDrop`: restore the partial | Y1, Y2, Y13 | – |
| `checkGlobalRuleNpcGroups`: return true | **Y2** (entry count) | Y1 |
| `collectAllowedDrops`: drop the `min_diff..max_diff` test (rev 2) | **Y2** (210133: 11 entries instead of 10) | Y1 |
| `isAllowedDefaultGlobalDropNpc`: drop the Poeta/Ishalgen exemption (rev 2) | **Y2**, **Y4** (210133 drops nothing) | Y1 at L1 |
| `collectAllowedDrops`: drop the item-race test; `checkRestrictionRace`: return true | **L-05 only** — every candidate of both gate monsters is `PC_ALL` (§2.4), so the gate cannot see it (rev 2; rev 1 listed Y2) | `gs.scenario.m5b3` green |
| `DropItem::getLootEffectId`: return 0 | **L-05 only** — Y1 sees a listed godstone in 41.5 % of kills (rev 2) | `gs.scenario.m5b3` green in ~20 % of runs |
| `getItemCount`: drop the kinah arm | **Y4** only if the range moves — at level 1 × 1.0⁶ it does not; **L-05's golden vector must catch it** | `gs.scenario.m5b3` green |
| `ItemService::addStackableItem`: never merge | **Y3** at L4 (an ADD for the Minor Power Shard, which must merge — deterministic, rev 2), Y12 | Y4 |
| `DropService::resendDropList`: skip `delete()` | **Y3** (corpse lives), **Y14** | Y2 |
| `ItemEquipmentListener::onItemUnequipment`: skip `endEffect(item)` | **Y5** | Y6-Y12 |
| `PlayerRestrictions::canUseItem`: skip `hasCooldown` | **Y8** (second use consumes) | Y5 |
| `ItemRestrictionService::isItemRestrictedTo`: return false | **Y9** | Y10, Y11 |
| `ItemSocketService::socketGodstone`: run the task body synchronously | **Y6** (no 2 s gap) | Y7 |
| `ProcAtkInstantEffect::applyEffect`: no-op | **Y7** (no damage packet in the whole fight) | Y1-Y6 |
| `GodStone::tryActivate`: the cooldown never expires | **Y7** (one message for k > 1 evaluated hits) | Y6 |
| `ItemSplitService::splitItem`: don't decrease the source | **Y10**, Y12 | Y9 |
| `ItemMoveService::switchItemsInStorages`: add before deleting (rev 2) | **Y16** | Y9 |
| a socketed godstone not written by `ItemStoneListDAO` | **Y12** (moved from Y7, rev 2) | Y6, Y7 |
| a `DropItem` kept in a static | **Y14** | Y13 |

**Stage 2 (2026-09-24, §17.4):** the mutants actually run against the gate. Two rows of this table are wrong: `onItemUnequipment` without
`endEffect(item)` is an equivalent mutant for the starter sword (it has no `<modifiers>`; Y5 is killed by `Equipment.unEquip` keeping the
weapon instead), and `splitItem` without the decrease fails Y10 alone (Y12 compares the database with what the client was told, and both
carry the duplicated count).

**Corrected 2026-09-24 (the review of stage 2, §18.3-§18.4):** the row "`resendDropList`: skip `delete()` → Y3, Y14" is **Y3 alone** (RK1;
Y14's counts are written after the shutdown deleted the corpse). Rows of this table **never run as mutants**: `checkGlobalRuleNpcGroups`
answering true, `isAllowedDefaultGlobalDropNpc` without its Poeta/Ishalgen exemption, `collectDrops` ignoring `max_drop_rule`, the item-race
rows and `getLootEffectId` / `getItemCount` (L-05's by the table itself), `ItemSocketService` synchronous (M6 ran the 0 ms schedule
instead), and a `DropItem` kept in a static (M14 ran `unregisterDrop` a no-op instead). Y2's count and bijection rows got their "the others
pass" run in §18.3 (RY2: `checkGlobalRuleWorlds` never matching).

### 10.5 The geo gate (`gs.scenario.m5b3_geo`)

The whole script with `gameserver.geodata.enable = true`, `LABELS "scenario;realdata;geo"`, the same lock, **plus one case the flat run
cannot have**: walk the character (after spawn protection ended) onto the material-60 camp fire `m5b3-material` picks (today (863.54, 1252.50,
119.50), 407 m from the spawn) and stand still 12 s. **Y15:** at least two `SM_ATTACK_STATUS` on the character with the PROCATKINSTANT log
and value > 0 (skill 8302, base `value="5"`, `noresist`; the number itself goes through `AttackUtil.calculateSkillResult`'s magical
arithmetic, so it is a bound, m5b2-plan.md D8), **5 ± 1 s apart** (`frequency="5"` over a 1 s task: `secondsElapsed++ % frequency`,
AbstractMaterialSkillActor.java `MaterialSkillTask.run`), and none after it steps off. **Proves:** the material observer, the `MATERIAL_SKILL`
force type and `ProcAtkInstantEffect` through `applyEffectDirectly`. **Cannot prove:** the damage value, nor the NIGHT and not-raining
conditions of materials 61/62. **Mutations:** `ProcAtkInstantEffect::applyEffect` a no-op (no packet); the task firing every second (1 s
apart). If G-04 finds a fake client cannot trigger the collision observer, it says so and the geo gate stays a re-run (m5b-plan.md §6.4's
precedent).

**Stage 2 (2026-09-24, §17.2 items 9-10):** a fake client triggers it. The point is `oracle.py m5b3-material --stand`'s, ON the fire mesh
(the zone is a SEMISPHERE, entered only above its center) and outside the firepot's mesh, so the firepot's actor exists but is never touched;
the per-zone counting and the pinned weather of the hand-off notes are not needed (a creature has one material task). "value > 0" is a
damage: TYPE.DAMAGE is written negated.

**Corrected 2026-09-24 (the review of stage 2, §18.2):** the stand point alone cannot prove the TOUCH ray - it is on the fire mesh, touched
either way, and the step-off point is inside no zone, so a port with `isTouched = true` passed the whole geo gate (the review's RV1). L14
now first stands 7 s on the oracle's **untouched point**: inside the fire's SEMISPHERE and no other zone, 0.25 m or more beside the fire's
mesh, where every emulated TOUCH ray misses; no tick may come there. Y15 **proves** the ray's negative arm now; it still cannot prove the
damage value, the NIGHT and not-raining conditions, or either abort path alone.

---

## 11. Real-client checklist (user, after stage 2)

Prerequisites as m5b2-plan.md §11 with `mygs.properties` from `m5b3.properties.example` **but `gameserver.rates.drop` back at `1.0, 2.0`**
(the gate's 1000000 would fill the bag). Keep `gameserver.event.service.disabled_events = *` (already in `config/mygs.properties:15`): with
events on, the active events' drop rules add entries to every corpse — Java's data, not a bug (§2.4).

1. Kill a Poeta monster. The corpse **sparkles** (the loot icon) — even when nothing dropped.
2. Click the corpse. The loot window opens; about half the time it is empty for a sparkie. Loot everything: the items appear in the cube, the
   window closes and **the corpse vanishes at once**.
3. Kill a striped kerub or a brownie until one drops **kinah**: the kinah counter rises. (Sparkies, abexes and other BEAST monsters never
   drop kinah — that is Java's data, not a bug.)
4. Leave a corpse with loot on it: it stays for about 5 minutes, then vanishes.
5. Take off your weapon: the character's attack in the stats window drops; put it back: the old number returns exactly.
6. Drag a potion stack onto an empty slot with the split dialog; drag an item onto another to swap; destroy a junk item. Split part of a
   stack into the warehouse too, if one opens (step 7): that is the path that reached the unported `LegionService` body in rev 1's plan (§2.8 E-9).
7. Open a warehouse (if the npc dialog works; otherwise skip — npc dialogs are M5c) and move junk in and out.
8. Take damage and **drink a Minor Life Potion**: HP rises by 37 at once and keeps rising for 20 s; drinking again inside 30 s is refused.
9. **Use the starter Administrator's Boon and the Lodas Amulet**: their buff icons appear. (If a character is older than 3 days, the Boon has
   already expired — check that its expiry produced **no ERROR** in the log; that is E-13 closed.)
10. **Socket a godstone** (right-click a godstone onto an unequipped weapon): a 2 s bar, then the weapon's tooltip shows it. Equip and fight:
    "proc effect occurred" messages appear now and then. Godstones drop rarely, and **a GM command cannot help**: chat commands are unported
    (`ChatCommand::run`, `ChatCommand.cpp:67`; `AdminCommand::process`, `AdminCommand.cpp:24`) and no chat client packet has a C++ file. Get one
    either **through the database, as the gate does** — with the character logged out, insert an `inventory` row for 168000116 (the gate's
    `ScenarioDatabase` helper, G-02, shows the columns) — or by setting `gameserver.rates.drop = 1000000` for one kill of a juvenile sparkie or
    a striped kerub (the corpse then holds two illusion godstones, §2.4; a level-40/50 illusion godstone can be socketed at level 1, `socketGodstone` checks no level,
    ItemSocketService.java:153-207) and setting it back afterwards.
11. **Expect these to fail loudly, not silently** (D6, D8): socketing a **manastone**, enchanting, using a quest-start item, anything with a
    stigma, tuning/identifying, group loot. Note the item ids that appear in `unported_trace.txt` — they are M5c's entry list.
12. With geo on, walk into a **camp fire**: you take a little fire damage every 5 seconds (before M5b-3 each of those ticks threw inside the material task).
13. Send `game-server/log/`, `m5a_summary.txt`, `live_counts.txt`, `partial_trace.txt` and `unported_trace.txt`.

**Corrected 2026-09-24 (the regate of stage 2, §19.5):** what the gates measured for each item. Item 8's heal-over-time lasts **21 s**, not 20.
Item 12's fire burns only a character standing **on** the fire. Items 4, 7 and 9 are not covered by any gate: item 7 cannot be done at all
until M5c ports the npc dialog.

---

## 12. What was measured and what was inferred

**Measured** (grep, `chunks.py`, or a parse of the two trees at HEAD `c1edb0afb` + the M5b-2 part 3 working tree; re-runnable, scripts in the
session scratchpad under `plans/m5b3/`):

- Every `AION_UNPORTED` / `AION_PARTIAL` count in §1, §2.3 and §5: P5-09 121 + 1, P5-07 109, P5-13 118 + 3 (restrictions 7), P5-06 163 + 2,
  P5-01 28, P4-12 5, P4-11a 12, **P4-13 0**, **P5-02a 0 + 1, P5-02b 0 + 1**, P5-15 0, P5-16 0, P4-16 6, P4-17 0; the per-file lists.
- The 143 undeclared bodies of the 36 action files and the 11 of the item services, by the name-level script; 43 C++ `CM_*` classes of 190
  Java files; the opcodes of AionClientPacketFactory.java, including the commented-out `CM_GODSTONE_SOCKET`.
- Every "0 unported" packet of §1 (each file grepped) and the ported callees of §2.2 U2 (each declaration found).
- The drop data of §2.4: 2,196 rules, 174 custom drops, the map counts, the **10 applicable rules of 210663 and 10 of 210133** (rev 2; rev 1
  said 11) with chances and candidate sets, P(some drop) 58.2 % and **78.5 %**, every candidate `PC_ALL`, the single-candidate "Power Shards"
  and `JUNK_SPAKY_MATERIAL` rules, the BEAST exclusion from the Kinah rule, 104 rules with `member_limit > 1`, 51 with zones, 11 with level
  reduction, 119 rules with race-specific candidates, `group_drop` present on all 63,287 templates; 23 + 96 event `gd_rule`s and the
  `disabled_events = *` key every scenario profile carries (`ScenarioServers.cpp:31`).
- The AI names of the spawned ids — Poeta: 72 aggressive, 53 general, 12 `quest_use_item`, 3 noaction, 2 `simple_abyssguard`, 2 resurrect,
  1 each postbox, useitem, portal_dialog; Ishalgen: 79 aggressive, 50 general, 17 `quest_use_item`, 10 `simple_abyssguard`, 4 noaction,
  2 useitem, 2 resurrect, 1 each postbox, portal_dialog — and no `chest`; `handleDropRegistered`'s only override,
  data/handlers/ai/events/HalloweenPumpkinAI.java:60.
- The re-green surface of §2.3's table (the M5b gate lines, the allow-list row, `CheckOutput.cpp:215`, `CheckOutputTest.cpp:496, 552-553, 585`),
  and that the stress clients do not fight (`tests/scenario/stress/`).
- ~~Skill 8267's two templates (skill_templates.xml:80570, :80575)~~ (a misreading, corrected in stage 1: one template, :80570, EARTH; §16),
  `SkillData`'s last-wins map, and the MAGICAL resist path (EffectTemplate.java:314).
- P5-03 / P5-04: 164 / 192 `AION_UNPORTED` at HEAD, 122 / 134 in the working tree (the paths `chunks.py files` prints, read at `HEAD:cpp/game-server/…`).
- The item subset of §2.5 (48 `skilluse` items, 9 leaf classes, 6 new), the godstone and material subsets of §2.6 (110 / 28 skills, 10 / 13
  leaves), the state of each of the 14 classes, and **the geo parse**: 98 skill-material placements on Poeta, 48 on Ishalgen, all skill 8302,
  the nearest spots (mesh placements only; `models.mesh` and `<map>.geo` read big-endian as GeoWorldLoader.java:112-240 does). **Disputed:** the
  review's independent lookup found 97 / 44; G-01's `m5b3-material` settles it before any gate asserts it.
- The starter inventory, masks and `expire_time` of §2.5 and §10 (`164002039` 4,321 minutes; `164002116` not storable in a warehouse; all
  others breakable; Training Sword `CAN_PROC_ENCHANT`).
- The corpse-decay coupling: `RespawnService` constants and both the Java and C++ bodies; the M5b gate lines that assert decay, respawn,
  `DropNpc` and the partial's hit count; `CheckOutput`'s zero and summary rows.
- `cycles.toml`: 54 rows name the item-action, socket, charge, stigma, equipment and drop classes (among them `DropItem.winningPlayer`
  :127, `Equipment$1..$2` :141-144, `ItemSocketService$1` :273-274 (rev 2 said :272-273), the action observers' captures from :208); `fieldmap.json`: 51 callbacks
  of the drop, item, action and stigma classes.

**Inferred, to confirm before relying on it:**

- **That rate 1000000 makes every rule certain in the running server.** It follows from Rates.java:166-173, DropModifiers.java:53-57,
  DropRegistrationService.java:189 and Rnd.java:32-34, with a 100× margin over the smallest rule (rev 2); nobody ran it. (Rev 1's membership
  caveat is moot: a one-value list is index 0 for every membership, Rates.java:171-172.)
- ~~**That no Poeta terrain material carries a skill.**~~ **Settled in stage 1** (G-01): neither start map has a terrain-materials file, so no
  terrain material casts a skill there (§16).
- **That seeding an `inventory` row at a high object id mid-run is safe** (the `IDFactory` cursor comment, `IDFactory.h:26-32`; not tested).
- ~~**That the M5b-2 gate kills 210663 and 210133 and so needs D4's key**~~ — **confirmed** in stage 0 (`GATE_MONSTER_NPC_ID` in case S5b of
  `M5b2ScenarioTest.cpp`) and in stage 1 (X12 counts 3 kills: 210663 twice, 210133 once).
- **The effort letters and the 8-10 day critical path** (rev 2; rev 1's 3-4 days contradicted its own letters), from body counts and Java LOC
  against earlier lanes (M5b-2's long stage-1 lanes were budgeted 7-9 days).
- **That the per-proc resist chance of 8267 against 210663 is small** (Y7's residual case); the gate prints it rather than asserting it.
- **That a fake client's position on the fire's mesh passes the TOUCH test** (G-04 measures it; how the observer attaches is now read, §8 risk 9).
- **That the Boon expiry throws once and then clears** (E-13): read from `ExpireTimerTask::run` and `Storage::delete_`'s order, not run.

**Claims of earlier plans checked here:** m5b-plan.md §9's ~250 sites / ~5,000 LOC (**too big** for this scope, **too small** for what it
counted, §3); §1's "`restrictions/` contains only `fwd.h`" (**stale**); D5's "deterministic `LOOT_ENABLE`" (**confirmed**, and promoted to
the M5b gate's R3); m5b2-plan.md D13's godstone/material note (**confirmed and sized**: 10 classes, 1 for the start maps).

---

## 13. Open questions this analysis could not settle without building

1. **Whether the M5b-2 gate exists with the kills §12 assumes** when M5b-3's G-05 runs, and which allow-list rows it carries.
2. **Whether `SM_INVENTORY_UPDATE_ITEM`'s full blob for a socketed weapon decodes with the existing `decodeInventoryInfo` reader** or needs the
   godstone blob entry added (G-02).
3. ~~How the material collision observer is attached~~ — **answered (rev 2):** on zone entry, by `MaterialZoneHandler::onEnterZone`
   (`MaterialZoneHandler.cpp:47-69`), and the task skips a player under spawn protection (`AbstractMaterialSkillActor.cpp:70`). ~~**Still
   open:** which reported coordinates pass the TOUCH test on the fire's mesh~~ — **answered in stage 2 (§17.2 item 9):** the points above the
   fire mesh's own triangles, at a z above the SEMISPHERE's center; `oracle.py m5b3-material --stand` emulates the ray over a 2 cm grid.
4. ~~Whether any AI handler overrides `handleDropRegistered`~~ — **answered (rev 2):** only data/handlers/ai/events/HalloweenPumpkinAI.java:60,
   an event npc that is not on the start maps; the C++ default is the inline no-op of `AITemplate.h:102`.
5. ~~The cube budget~~ — **settled in the plan (rev 2):** three corpses, worst case 10 + 8 + 8 new stacks against 27 slots, handled by L3b and
   computed per corpse by G-01 (§8 risk 5).
6. **Whether D1 is accepted** — it changes when stage 1 starts and when L-01 merges, not what the stage contains.

---

## 14. Review, 2026-09-23

An adversarial review of rev 1 returned **needs-revision** with 22 findings (2 high, 11 medium, 9 low) and confirmed the rest of the plan's
counts (every per-chunk and per-file `AION_UNPORTED` number, the undeclared-method count, the packet opcodes, the drop data, 210663's rule
table, the starter inventory, the potion and godstone numbers, the equip path, the corpse coupling and the wire shapes). Every finding was
re-checked against the two trees before this revision; what it found and what changed:

| # | Sev. | Finding | Rev 2 |
|---|---|---|---|
| 1 | high | The re-green (G-05, G-06) was in stage 2 while L-01 closes the partial in stage 1: the M5b gate's R3 (`:2131`), Q1 (the §A row, allow-list `:50`) and Q2 (`:2302`) fail on **every** run from L-01's merge, and R4/Q3 flake; under D1 the same profile block and allow-list are M5b-2's regate lane's | **Confirmed.** G-05 and G-06 moved into stage 1 (gate-harness lane, P5-14 lease); **L-01 + G-05 merge in one integrator commit** (§1 finding 1, D4, D5, §5, §6, risk 1); D1 now says L-01 is written early but merges only after M5b-2 releases P5-SC. Two refinements: only `CheckOutputTest.cpp:496` actually fails when the row moves (`:552-553` and `:585` feed fabricated counts and stay green; their messages are reworded), and G-06 is not a merge-together constraint — under rate 0 a `DropNpc` lives 2 s — but it is still done in stage 1 |
| 2 | high | L6b's kill leaves a 10-entry corpse nobody loots, so Y14 fails on a correct server; the cube budget left that corpse out | **Confirmed.** New case **L6c** loots it; L3b makes room to the oracle's per-corpse budget; risk 5 recounted (10 + 8 + 8 new stacks) |
| 3 | medium | The event pass (DropRegistrationService.java:96-98) makes Y2's count date-dependent | **Partly.** The mechanism is real and rev 1 did not mention it, but **every scenario profile already disables all events** (`ScenarioServers.cpp:31`, `disabled_events = *`), so the gate's count does not depend on the date. §2.4, D3, §10.1 and §11 now say so; L-05 covers the event pass. The review's 46 / 191 rules counted opening and closing tags: there are 23 and 96 |
| 4 | medium | 210133 has 10 applicable rules and P(some drop) 78.5 %, not 11 and 79.1 % | **Confirmed** by re-running the evaluator with the map's `drop_type`. "Applicable rule" defined (restrictions pass *and* a non-empty candidate set), used by D3, Y2 and G-01 |
| 5 | medium | The item-race and `lootEffectId` mutations cannot be caught (all candidates `PC_ALL`) or only by chance (41.5 %) | **Confirmed.** Both moved to L-05 unit cases on shipped data (a race-specific rule; a listed godstone); Y1 and Y2 state what they cannot prove; two deterministic mutations added (the level filter: 210133 gets 11 entries; the Poeta/Ishalgen exemption) |
| 6 | medium | Y5's field-by-field `SM_STATS_INFO` equality fails on HP regeneration | **Confirmed.** Y5 compares stat fields only, with line citations of what is excluded |
| 7 | medium | Y7's "every hit procs with value > 0" is not deterministic: 8267 is MAGICAL and resistible, the message is sent regardless, the monster dies fast; the relog mutation is not observable | **Confirmed** (Java sends the message the same way, CreatureController.java:282-283, so this is a gate defect, not a port one). Y7 counts one message per evaluated hit and asks for ≥ 1 landed proc; the relog mutation moved to Y12. Also found: 8267 has **two** templates (last wins) |
| 8 | medium | Fixed potion counts ignore loot merges; Y12 contradicts Y10 | **Confirmed.** Y8, Y10 and Y12 derive every count from the packet before it |
| 9 | medium | A merge sends `SM_INVENTORY_UPDATE_ITEM` alone; the certain merge is the power shard at L4 | **Confirmed.** Y3 split into the two packet paths and applied to L4 and L6c; the "never merge" mutation is now deterministic |
| 10 | medium | Cross-storage splits reach the unported `LegionService::addWHItemHistory` | **Confirmed.** New item **T-08** (10 lines, P5-11 lease), E-9 and M2 corrected, a T-07 unit case, checklist step 6 |
| 11 | medium | h01 omits `EnchantItemAction`'s five-argument `act` that `CM_MANASTONE` calls | **Confirmed.** Added to h01 (stubbed, with a `using` declaration), D8, P-05, §2.7 |
| 12 | medium | Lane efforts sum to 5-11 days against a 3-4 day budget | **Confirmed.** Re-budgeted: loot ≈ 8.5, items ≈ 9.5 agent-days, stage 1 ≈ 8-10 days (§6, §9, §12). A separate tests lane is impossible (the tests share the lanes' chunks); the levers are D1 and an optional P5-07 split |
| 13 | medium | Under D1 the loot lane waits for P-03 in a lane that has not started | **Confirmed.** P-03 became **L-06** in the loot lane under a lease on the one new P5-01 file |
| 14 | low | The list of tests to rewrite was incomplete | **Confirmed.** §2.3 now tabulates every pinning test with its owner and timing |
| 15 | low | `AIActions::registerDrop` and m5b-plan.md O-08 were not mentioned | **Confirmed.** E-17 in §2.8, a §3 row and O-05 place the six handlers (M5c / M5d / M5f / M5j) |
| 16 | low | `isQuestDrop` reaches the unported `QuestState::getQuestVarById` | **Confirmed** (and one level deeper, `QuestVars::getVarById`). Named in §2.1, E-1, L-04 (with a unit case) and O-06 |
| 17 | low | L11 (the swap) had no assertion | **Confirmed.** New **Y16** and a mutation row |
| 18 | low | Checklist step 10 suggested a GM command that cannot work | **Confirmed.** Step 10 says to seed through the database or raise the drop rate for one kill |
| 19 | low | Wrong numbers: 131 not 177 Java lines; 86 `<itemuse>` not 20; D12 → D11; P5-03/04 164/192 and 122/134; 98/48 unconfirmed | **Confirmed** (a)-(d) and corrected; (e) the 98 / 48 placements are kept with the review's 97 / 44 beside them, for G-01 to settle |
| 20 | low | §13 Q3 and Q4 can be answered from the code | **Confirmed.** Q4 closed; Q3 closed except the TOUCH coordinates, which G-04 measures (risk 9) |
| 21 | low | Rate 10000 has no safety margin; the membership caveat is moot | **Confirmed.** Rate 1000000 everywhere (D3, I-02, §2.4, §10.1, §11, §12); the caveat removed |
| 22 | low | G-08 changes the stress run, which is the user's to design | **Confirmed.** G-08 is now a proposal for the capacity conversation (D13); the re-green does not touch the stress run (its clients do not fight) |

Also found while re-checking (not in the review): every item that lands in the cube calls `QuestEngine::onItemGet` (§2.8 E-18, ported); and
none of the chunk-local test directories can host a parallel tests lane (`tests/economy` is P5-09, `tests/itemsvc` P5-07).

**The shape after the review:** ~145 required bodies (+ a 64-declaration layout batch), stage 0 (integrator, ~1 day), stage 1 with five lanes
(loot, items, player-side, effects, gate-harness + re-green) at ~8-10 days on the critical path, stage 2 with two lanes (the gate and its geo
variant, fixups) at ~3-4 days. D1 (starting loot and items beside M5b-2) and G-08 (looting stress) are the user's.

---

## 15. Stage 0 (applied), 2026-09-24

Applied by the integrator's stage-0 lane on HEAD `27726d32c` (the plan was written at `c1edb0afb`). Since then T-02 landed (`706dc55c1`,
E-13/E-14 closed) and M5b-2 closed (stage 2, `50a9158bf`), so **D1 is moot**: P5-SC is free and every stage-1 lane can start; L-01 still
merges only together with G-05 (D4).

**I-01, the header batch** — recorded row by row in `docs/porting/header-requests.md`, "Wave 5b-3 stage 0" (`m5b3-h01`, `m5b3-h01-1..32`,
`m5b3-h02`, `m5b3-h03`, applied by the integrator; h01 and h03 are layout and signature changes and were **reviewed 2026-09-24: approve**
(hub-headers.md §14), h02 is additive): `AbstractItemAction::canAct`/`act` pure virtual,
the two overrides in all 32 bound classes in Java order with 30 new stub `.cpp` files, `EnchantItemAction`'s five-argument `act` with the
using-declaration, the nine `ItemActions` lookups, the `QuestService::getQuestDrop` signature. **74 new `AION_UNPORTED(` sites** (64 + 1 + 9;
`game-server/src` 1,744 → 1,818), no body changed. The class list re-derived from ItemActions.java:15-31 is the plan's: 32 classes, exactly the
package's `extends AbstractItemAction`; `CompositionAction` is no `AbstractItemAction` and `UseTarget` an enum.

**I-02, the profiles** (Java tree, `game-server/config/`): `gameserver.rates.drop = 0` in `m5b.properties.example` and
`m5b2.properties.example` (a new "what M5b-3 changes in this profile" block, D4); new `m5b3.properties.example`: the M5b-2 set (with
`gameserver.event.service.disabled_events = *`) + `gameserver.rates.drop = 1000000`, `gameserver.items.ignore_potions_at_full_health = false`,
`gameserver.rates.godstone.activation.rate = 1.0`, `gameserver.rates.godstone.evaluation.cooldown_millis = 750`,
`gameserver.drop.announce_quality = MYTHIC`, and geo/re-entry/shutdown under "only in the scenario gates" as in the siblings; its header tells
a player to set the drop rate back to `1.0, 2.0` (§11). The gate profiles in `M5bScenarioTest.cpp` / `M5b2ScenarioTest.cpp` are **not**
touched: that is G-05, in L-01's commit. `oracle.py m5b3-drops --npc 210663 --drop-rate 1000000` answers 10 entries with a smallest certain
effective chance of 10,000 (no margin caution).

**I-03, the leases** (one active lease per file, released at the lane's merge; recorded here, not as `aion_gs_chunk(... LEASE ...)` calls in
`chunks.cmake`, as M5b-2's were, so `chunks.py owner` still prints the owning chunk):

| Lane (chunk) | Leased file | Owner (`chunks.py owner`) | For | Test file the lease also covers |
|---|---|---|---|---|
| loot (P5-09) | `src/aion/gameserver/services/QuestService.cpp` | P5-06 | L-04, the four quest-drop bodies against h03 | one new file in `tests/quest` (P5-06; the manifest still carries a phase-4 P4-08 lease on that directory) |
| loot (P5-09) | new `src/aion/gameserver/utils/stats/DropRewardEnumInfo.h` | P5-01 (by glob, like `XPRewardEnumInfo.h`) | L-06 | one new file in `tests/stats` (P5-01) |
| items (P5-07) | `src/aion/gameserver/services/LegionService.cpp` | P5-11 | T-08, `addWHItemHistory` | one new file in `tests/legionhouse` (P5-11) |
| gate-harness (P5-SC) | `src/aion/gameserver/CheckOutput.h`, `CheckOutput.cpp`, `tests/app/CheckOutputTest.cpp` | P5-14 | G-06 (D5) | – (the test file is named) |

No other lane may edit these files while the lease is active; the player-side lane's P5-01 work stays in `ItemEquipmentListener.cpp` (§6).

**What §7 (and the counts around it) got wrong, found while applying it:**

1. **`parentItem` must be `runtime::Ptr<gameobjects::Item>`, not `gameobjects::Item&`** (h01, D6). hub-headers.md §5.1: CM_APPEARANCE.java:112-113
   passes `null` directly (`canAct(player, null, null)`, `act(player, null, item)`), and five bodies compare it with `null`
   (AnimationAddAction.java:38, EmotionLearnAction.java:40, EnchantItemAction.java:48, RideAction.java:48, TitleAddAction.java:29). Applied
   as `Ptr`; with `Item&` the M5c port of CM_APPEARANCE and those five checks would have needed the same 32-class layout change again. The
   five-argument `EnchantItemAction::act` keeps `Item&` for the parent and the target (no `null` at that arity) and `Ptr` for the supplement,
   as §7 wrote it. For T-04 this means `parentItem->getItemTemplate()` (a `Ptr` dereference throws Java's NullPointerException).
2. **h01 omits `SkillUseAction::isIneffectiveHealSkill`**, the private static helper T-04 ports (§2.3 counts it among `SkillUseAction`'s three
   bodies; SkillUseAction.java:82-97, called from `canAct` when `ignore_potions_at_full_health` is true). Not applied (it is not in §7):
   filed as **`m5b3-h04`, pending**, with the signature in header-requests.md. Decide it before T-04 starts, or T-04 writes a file-local
   function (the m5b2-s2-2 workaround). It is additive (private, static, non-virtual), so hub-headers.md §14 lets the integrator batch it
   without review; after the stage-0 review it takes the effected list read-only, as a `const std::vector<runtime::Ptr<Creature>>&` that
   `canAct` fills with `skill.getEffectedList().snapshot()` (hub-headers.md §7.1: the callee only streams it, SkillUseAction.java:94-95).
3. **`using AbstractItemAction::act;` in `EnchantItemAction` hides nothing**: the class declares the four-parameter override itself, so the
   five-argument overload hides no base overload (D8's reason). Applied as D8 says; it is redundant and harmless.
4. **"143 undeclared bodies across the 36 action files"** (§1, §2.3) is the plan's own name-level count including inner and anonymous bodies;
   `census.py --chunks P5-07` counts **119 named undeclared bodies** in the package before stage 0 and **45 after** (39 private helpers and
   non-generated getters of the 32 classes — `finishUse` ×12, `EnchantItemAction`'s `isSuccess`/`getMinLevel`/`getMaxLevel`/
   `isSupplementAction`/`checkSupplementLevel`, … — plus `CompositionAction` 4 and `UseTarget` 2). The census counts anonymous classes (the
   actions' `ItemUseObserver`s and tasks) separately: 29 in P5-07 ("Anon in open").
5. **Counts that moved since `c1edb0afb`** (census.py at `27726d32c`): P5-07 **100** unported (109 − T-02's 9), 174 after stage 0, 56
   undeclared (was 130), shells 30 → 0; **P5-11 87 + 3 partial, not 86** (87 already at `c1edb0afb`: a miscount in §12, not a change);
   P5-09 121 + 1, P5-06 163 + 2, P5-13 118 + 3, P5-01 28, P4-12 5, P5-03 122, P5-04 134 (the working-tree numbers of §2.3, now committed),
   P5-15/P5-16 0 unported with 244 / 206 undeclared bodies (the missing packet files) — unchanged.
6. **The Java-tree profiles are LF, not CRLF**: `git ls-files --eol` shows `i/lf w/lf` for `m5a/m5b/m5b2.properties.example` (the other
   Java-tree files are `w/crlf` through `core.autocrlf`), and the Java tree has no `.gitattributes`. Kept as they were; the new
   `m5b3.properties.example` is LF like its siblings.
7. **§5 I-03 names `CheckOutput.{h,cpp}` without a path**: they are `src/aion/gameserver/CheckOutput.{h,cpp}` (P5-14), recorded above.

**The stage-0 review (2026-09-24): approve**, six findings (four low, two info), closed in the same working tree: header-requests.md's
decision cells of m5b3-h01, h01-1..32 and h03 carry the review's verdict (they are layout and signature changes, hub-headers.md §14); D6
points at item 1; m5b3-h04 takes the effected list read-only (item 2) and stays pending for the integrator's additive batch; the M5b and
M5b-2 profiles say their gates get `gameserver.rates.drop = 0` only with G-05; the verification note names the 48 game-server test targets
that were built; `EnchantItemAction.h`'s class comment says why the varargs override may dereference its two items (comment only).

---

## 16. Stage 1 results (integrated), 2026-09-24

The stage-1 integration step, on the working tree over HEAD `27726d32c`: stage 0 (§15) plus the five lanes of §6 — loot, items, player side,
effects, gate harness — each ported, reviewed and fixed. Nothing is committed. **L-01 and G-05 are in the same tree, as D4 requires:**
`DropRegistrationService.cpp` has no `AION_PARTIAL`/`AION_UNPORTED` left, both earlier gate profiles set `gameserver.rates.drop = "0"`
(`M5bScenarioTest.cpp:943`, `M5b2ScenarioTest.cpp:856`), the `DropRegistrationService.cpp:43` §A row is gone from both allow-lists (HISTORY
notes in its place), M5b's R3 asserts exactly one `SM_LOOT_STATUS(LOOT_ENABLE)` naming the corpse with `lootEffectId` 0, Q2 and M5b-2's X12
assert `DropNpc` created = kills, live 0, `dropNpcsHeld`/`dropItemsHeld` 0 and both drop-item rows 0/0, and `CheckOutput` has the D5 rows
(G-06). **They land together with the loot lane's one edit outside its chunk**, which the integrator accepts: `QuestsData::afterUnmarshal`
(P4-09) now sets every `<quest_drop>`'s quest id before publication (QuestEngine.java:89 semantics; docs/deviations/P4-09.md). Without it every
kill of an npc with a quest drop (e.g. 210668 on Poeta, none of the gate npcs) throws a NullPointerException in `isQuestDrop`, inside
`registerDrop`, which `NpcController::onDie`'s catch logs as one ERROR per kill (§8 risk 8).

### 16.1 What landed

| Lane | Items | Production | Tests (binary, cases now) |
|---|---|---|---|
| loot (P5-09; P5-06, P5-01 leases) | L-01, L-02, L-04, L-05, L-06; L-03's team arms ported but untested | `DropRegistrationService` 32 of 32 bodies (the `registerDrop` partial closed), `DropService` 16 of 16 (`TempTradeDropPredicate::changeItem` stays unported: `TemporaryTradeTimeTask` has no C++ file), the four `QuestService` quest-drop bodies, new `utils/stats/DropRewardEnumInfo.h`; `QuestsData.cpp` (P4-09, above) | `aion_gs_economy_tests` 50, `aion_gs_quest_tests` 25, `DropRewardEnumTest` 3 |
| items (P5-07; P5-11 lease) | T-01, T-03..T-08 (T-02 landed before, `706dc55c1`) | `ItemService` 11, `ItemMoveService` 3, `ItemRestrictionService` 3, `ItemSplitService` 4, `SkillUseAction::canAct`/`act` (+ `isIneffectiveHealSkill` as a file-local function: m5b3-h04 pending), the 9 `ItemActions` lookups, `socketGodstone` with its observer and 2 s task, `StigmaService::notifyEquipAction`, `LegionService::addWHItemHistory`, three `ItemPacketService_Item*Info.h` companions | `aion_gs_itemsvc_tests` 69, `LegionWarehouseHistoryTest` 1 |
| player side (P5-13, P5-01, P4-12, P5-15, P5-16) | P-01, P-02, P-04..P-06 | `canUseItem`, `canChangeEquip` (+ the optional `canTrade`), `onItemUnequipment`, `removeStoneStats`, the soul-bind accept with its observer and 5 s task, the nine client packets of §2.7 | `aion_gs_instance_tests` 68, `aion_gs_player_tests` 32, `aion_gs_stats_tests` 92, `aion_gs_cm_ak_tests` 82, `aion_gs_cm_lz_tests` 85 |
| effects (P5-03, P5-04) | E-01..E-04 (D7's three included) | the 14 classes of §2.5/§2.6 | `aion_gs_effects_al_tests` 78, `aion_gs_effects_mz_tests` 88 |
| gate harness (P5-SC, `tools/oracle`; P5-14 lease) | G-01, G-02, G-05, G-06 | `CheckOutput` drop rows (D5) | `CheckOutputTest` 19 (with the integration's case), the scenario unit suites 95, `tools.oracle` 352 |

`oracle.py` now answers `m5b3-drops --inventory ITEM[:COUNT] [--cube-expansions N]` (a `cube` section: per corpse the worst- and best-case new
slots, kinah entries and deterministic merges), `m5b3-item --item ID` and `m5b3-item --survey --map ID` (§2.5-§2.6's counts, reproduced), and
`m5b3-material --map ID [--near X,Y,Z --radius R]`. Every new assertion of the five lanes was mutation-proven by the lane, and the reviews
re-ran their own mutants (the survivors became the fix passes' new cases; what is still untested is in each lane's deviation doc).

### 16.2 The integration's own edits

- **`CheckOutput::runFinalCensus` (P5-14): a leak is written only when two census checks in a row agree** — the fix for the one failure the
  integration found (16.4). New case `CheckOutputTest.FinalCensusWaitsForACountThatIsStillFalling`; with the old loop it fails (`Player 61 4`
  in `census.txt`), with the new one it passes (mutation run, restored byte for byte). docs/deviations/P5-14.md; `CheckOutput.h`'s doc of the
  function is now incomplete (comment-only request m5b3-i-1).
- The Java-tree profiles `m5b.properties.example` and `m5b2.properties.example`: the "gate profile does not carry this key yet" lines now say
  the gates carry it since this stage (G-05).
- `docs/porting/header-requests.md`, new section "Wave 5b-3 stage 1": every header request the lanes filed (none applied, 16.6).
- `docs/deviations/P4-09.md` (the quest-id fix), `docs/deviations/P5-14.md` (G-06 and the census), `docs/design/m5i-plan.md:251`
  (`XPBoostEffect::calculate` is ported now).
- This plan, in place: §2.3 (P5-09's undeclared bodies), §2.5 (`StatupEffect` is 12 items carrying 20 effects), §2.6 (skill 8267 has **one**
  template, EARTH, :80570 — the WIND one above it is 8266; the 98/48 camp-fire counts confirmed by `m5b3-material`, the review's 97/44 were
  wrong; neither start map has a terrain-materials file), `cycles.toml:273-274` (not 272-273) at §2.6 and T-05, risk 11, §12.

### 16.3 Counts, re-derived (`census.py --chunks …` and `AION_UNPORTED(` sites, working tree)

`game-server/src`: **1,744** `AION_UNPORTED(` sites at HEAD, **1,818** after stage 0's 74 stubs, **1,697** now (121 closed by stage 1);
`AION_PARTIAL(` 17 → 16 (the `registerDrop` partial).

| Chunk | Plan (`c1edb0afb`) | After stage 0 | Now | Where the plan's number moved |
|---|---|---|---|---|
| P5-09 | 121 + 1 partial, 0 undeclared | 121 + 1, **11** undeclared | **83 + 0**, 11 undeclared | the plan's name-level script counted 0 undeclared; `census.py` counts 11 (the gate-harness lane found it). Left: `DropDistributionService` 4 (M5g), `changeItem`, and the rest of the chunk (trade, exchange, broker, mail, craft … — M5c) |
| P5-06 | 163 + 2 | 163 + 2 | **159 + 2** | as planned (the four quest-drop bodies) |
| P5-07 | 109 | 174, 56 undeclared | **140**, 49 undeclared | the plan's "32 on the path" included T-02's 9 (landed before); stage 0 added 11 stubs on the path, so the lane closed 34; the three companions count as 7 undeclared in the census (accessors, enum constructors) |
| P5-11 | 86 | 87 + 3 | **86 + 3** | §12's 86 was a miscount (stage 0) |
| P5-13 | 118 + 3 | 118 + 3 | **115 + 3** | + the optional `canTrade` |
| P5-01 | 28 | 28, 27 undeclared | **26**, 24 undeclared | as planned (+ `DropRewardEnum`'s two, its enum constructor stays) |
| P4-12 | 5 | 5 | **4** | the four left are `registerExpirable` helper sites of other lists, none in `Equipment.cpp` |
| P5-03 / P5-04 | 122 / 134 | 122 / 134 | **112 / 106** | 38 sites, as §2.6 said; P5-03's undeclared 4 → 3 (`FearTask.run`) |
| P5-15 / P5-16 | 0 unported; 244 / 206 undeclared | the same | 0; **238 / 185** undeclared | the nine packet files |
| P5-14 / P5-SC | 31 / 0 | 31 / 0 | 31 / 0 | – |

### 16.4 Build, unit tests and gates, before and after

Shared `build/msvc`, configured with `-DAION_BUILD_CHAT_SERVER=ON`, Debug, `--parallel 6 -p:CL_MPCount=2`, all 177 targets: 0 errors, 0
warnings. `ctest -C Debug -j 6 -LE "scenario|geo|m4|nightly|stress|smoke"`: **3,338 of 3,338 passed in 660 s** (the M5b-2 join: 2,967 in
577 s); after the census fix `aion_gs_app_tests` 34 of 34. `lint_concurrency`, `chunks.py check` and the three `skeleton.py` checks are clean.

Gates, one at a time, exact-name regexes, `--output-on-failure`, seconds. **Before** is the M5b-2 join on `50a9158bf` (m5b2-plan.md, stage-2
results; the gate-harness lane's own G-05 runs, on a tree without the player-side and effects production code, are in its column's notes);
**first run** is this integration's tree before the census fix; **final** is the final tree.

| Gate | Before | First run | Final | Notes |
|---|---|---|---|---|
| gs.smoke.startup | passed, 30 | passed, 29 | **passed, 25** | |
| gs.smoke.startup_geo | passed, 148 | passed, 150 | **passed, 144** | |
| gs.m4.check_static_data | passed, 141 | passed, 138 | **passed, 140** | |
| gs.scenario.m5a | passed, 54 | passed, 54 | **passed, 53** | the drop rows read 0 0 (no kill) |
| gs.scenario.m5a_geo | passed, 160 | passed, 158 | **passed, 156** | |
| gs.scenario.m5b | passed, 218 (R3 then counted the partial) | **failed, 217, twice in a row**: K9 Q1, `census.txt` held `Player 103883 3` / `Player 103881 3` | **passed, 218** | the census race below; with the fix it passed at once (216 s) and again in the final run (the census logged refcount 3 and wrote nothing). R3: one `LOOT_ENABLE` for the corpse, `lootEffectId` 0; Q2 `DropNpc 0 1`. The gate-harness lane's G-05 run: passed, 216 |
| gs.scenario.m5b_geo | passed, 339 | passed, 353 | **passed, 349** | `DropNpc 0 1` |
| gs.scenario.m5b2 | S14 failed once, then passed, 235 / 197 / 168 | **failed at S14**, 236; the rerun passed, 164 | **passed, 163** | X12: 3 kills (4 in the S14 run), `DropNpc` created = kills, live 0. The gate-harness lane's G-05 run: passed, 185 |
| gs.scenario.m5b2_geo | passed, 303 | passed, 288 | **passed, 297** | `DropNpc 0 3` |

**The census race (fixed).** Both failing M5b runs logged `Leak census: … Player … refcount 86` and wrote `Player <id> 3`, while the same
shutdown logged "Runtime shut down: … 0 objects still tracked" and `live_counts.txt` read `Player 0 3`: no leak, a logout still being
reclaimed. `runFinalCensus` ran two fixed `reclaimNow()` scans and re-scanned only for a count of 0, while the census's reported count is the
one its hook saw after the last scan, and the objects a logout drops (the retired `KnownObject`s and `Effect`s holding the Player) are
destroyed epoch by epoch; the non-geo M5b run, which stops with the Warrior still connected in a crowded spot, now needed a third scan. (The
m5b_geo run of the same tree logged the same line with refcount 3 and wrote nothing; m5b-plan.md G-3's note had seen the transient line with
refcount 65 on 2026-09-23.) Why this tree needs one scan more than the M5b-2 tree did was not traced: the gate-harness lane's green G-05 runs
predate the player-side and effects production code (its lint counted 3,693 files, the final tree 3,711), which is the only candidate list.

**S14 (the known M5b-2 flake) recurred, and its new message names the cause.** Saved output: the session scratchpad `s1i/fail-r1-m5b2/`.
"monster B (object 25204): 0 SM_ATTACK at the Mage, 0 elsewhere, 1 SM_MOVE (… 0.88 m from spot B), 1 SM_ATTACK_STATUS, died": B, a 199-HP
210663 that the script never damages, **died to the Mage's first Flame Bolt**, so its HP had been drained before the pull, while the Mage was
logged out — m5b2-plan.md's reading (b), a fight with a neighbour (203055 "mercenary" and 210705 "kerub fighter" stand 22-28 m from spot B).
The script then pulled a dead npc for 75 s. It is the gate script's problem, not the server's: S14 should notice that B died to its pull and
wait for B's respawn at spot B (G-07). Stage 1 did not touch it.

**Corrected 2026-09-24 (the review of stage 2, §18.1):** the cause is not a neighbour. The tribes rule it out (210663's `MONSTER` is hostile
only to `YUN_GUARD`; 203055's `FARMER_HKERUBIM_LF1` and 210705's `KERUBIM_AFARMER_LF1` only to each other, tribe_relations.xml), and no
drained HP is needed: Flame Bolt 1282 has `apply_magical_critical`, and a magical critical multiplies its 141 by 1.5
(AttackUtil.calculateSkillResult -> calculateWeaponCritical, AttackUtil.java:191-208, 318-323), which takes all 199 HP of a full 210663. The
review's mutant RC9 (every magical-critical roll of the Mage succeeds) shows it. S14 now pulls with the spellbook's auto-attack, which cannot
kill B, and keeps the respawn branch.

### 16.5 What stage 2 (G-03, G-04, G-07) must know

- **The profile.** `game-server/config/m5b3.properties.example` has the keys (§10.1); `M5b3ScenarioTest.cpp` passes them as `-D` arguments
  the way `M5b2ScenarioTest.cpp:856` does. At rate 1,000,000 `oracle.py m5b3-drops --npc 210663` and `--npc 210133` answer **10 entries**
  each, deterministic, smallest certain chance 10,000; 210133 has one kinah entry. The cube budget comes from `m5b3-drops --inventory`
  (the fresh Warrior's 9 stacks: 10, then 8 with the shard a certain merge, then 8 with the shard and the junk merging — the plan's 10 + 8 + 8).
  Custom-group rows of `deterministicMerges` carry `customGroup`, not `ruleName` (none for the gate npcs).
- **The harness pieces:** `decoders/ItemDecoders.{h,cpp}` (the nine packets of §2.7 plus `SM_WAREHOUSE_UPDATE_ITEM`), `readItemInfoBlob`
  public in `PacketDecoders.h` — a socketed godstone travels in ENCHANT_INFO's `godStoneId`, so §13 question 2 is answered: no new blob
  entry —, the nine `GameSession` builders with a `ManastoneRequest` for every `CM_MANASTONE` arm, `ScenarioDatabase::seedInventoryItem`
  (object ids from `0x07000000` up, below the `IDFactory` wrap and skipping Java's invalid-id pattern) and `setLifeStatHp`.
- **The held rows at rate 1,000,000:** `dropNpcsHeld`/`dropItemsHeld` count what `DropRegistrationService` still holds; Y14 wants both 0
  after L6c, and a drop class alive beyond them is an ERROR line.
- **Y7 and risk 11:** 8267 has one template (EARTH); the oracle keeps the last template of an id, as `SkillData` does.
- **The camp fire (G-04, Y15).** The nearest unconditional fire is `PR_L_FIRE_SEMISPHERE_01A_103337_210010000`, material 60, a SEMISPHERE of
  r 1.76 at (863.54, 1252.50, 119.32), 407.01 m from the Elyos spawn; the nearest fire of any kind is a material-62 box 108.4 m away.
  **A material-61 firepot zone overlaps the gate's fire** (`PR_D_FIREPOT_01A_WEATHERFIRE_CHILD2_154501`, SPHERE r 1.46 at 0.18 m, conditions
  SUNNY + NIGHT) and a material-62 fire stands 8.55 m away (SUNNY): at night in fine weather a character on the fire is in two skill zones,
  each with its own 5 s task, so Y15's "5 ± 1 s apart" must count per zone or pin the game time and weather (`oracle.py m5b3-material --near
  863.54,1252.50,119.32 --radius 10` lists the three).
- **Reachable unported bodies** after stage 1 (none on the gate's script): every item action but `skilluse` (D6); `CM_MANASTONE` arms 1/2
  (`EnchantItemAction`), 3 (`removeManastone`), 8 (`amplifyItem`) and the stigma pair (`chargeStigma`); equipping a stigma
  (`getPossibleStigmaCount`, `getPossibleAdvancedStigmaCount`, `addStigmaSkills`), unequipping one (`removeStigmaSkills`), an item with
  enchant level > 0 (`EnchantService::applyEnchantEffect`); a legion member's legion-warehouse move or split (`LegionService::addHistory`);
  the team loot arms (`LootGroupRules`, D9); `isQuestDrop` for a started quest with a collecting step (`QuestState::getQuestVarById`, M5d);
  `copyItemInfo` of a source with mana stones (`ItemSocketService::addManaStone`).
- **Flakes:** the S14 cause above; the M5b-2 lane's S6/S10 flakes (the gate-harness report); the census race is fixed. Unit-test note from
  the items lane: `PricesConfig`'s atomics are 0 in unit tests, so a price test sets Java's defaults (100/100/100).

### 16.6 Left for the integrator

- **Header requests, none applied** (header-requests.md "Wave 5b-3 stage 1"): m5b3-h04 (additive; then move `isIneffectiveHealSkill` into
  the class), m5b3-loot-h01 (`QuestDrop` virtual destructor, layout), m5b3-p04-1 (`friend struct Equipment_Runnable;`), m5b3-e-1 / e-2
  (`FearEffect.h` friend and nested `FearTask`), the comment-only m5b3-p04-2, m5b3-p01-1, m5b3-i-1, and m5b2-p3-2 (approved, now also for
  `ProcAtkInstantEffect` and `PoisonEffect`).
- `Storage.cpp` (P4-13, `deleteTypeFromUpdateType`/`deleteTypeFromQuestStatus`) and `PacketSupport.h` (P4-17, the three item-type
  stand-ins) should switch to the new `ItemPacketService_Item*Info.h` companions.
- Untested and documented in the lanes' deviation docs: the palace boost, a zone rule's true arm, the event pass's chest arm and the team
  arms (P5-09); the `quest_use_item` exclusion (unobservable with the shipped data); the trading arms of move/switch/split
  (`ExchangeService::registerExchange` unported); `isIneffectiveHealSkill`'s negative-value arm (no shipped row); the stigma price ladder;
  the arguments `CM_MANASTONE` hands its unported bodies.
- Pre-existing: two `LegionHouseServicesTest` cases fail when `aion_gs_legionhouse_tests` runs in one process (order-dependent; green under
  ctest).
- Process notes: three lanes edited a `cpp/` file once with `sed -i` or Python against the Edit/Write rule (player side, effects, loot; each
  verified LF), and so did this integration once, on this plan (the two `cycles.toml` citations; LF verified). All `build/b3-*` build
  directories are deleted; their logs in `build/` can go.

---

## 17. Stage 2 results (the gate), 2026-09-24

G-03, G-04 and G-07 on the working tree over HEAD `4867fbc44`. **No production file changed**: every divergence the gate found was the
plan's, not the port's. Nothing is committed.

### 17.1 What landed

| Item | Files | What |
|---|---|---|
| G-03 | `tests/scenario/M5b3ScenarioTest.cpp` (new), `ScenarioTests.cmake`, `tests/scenario/m5b3_partial_allowlist.txt` (new) | `TEST(M5b3Scenario, Run)` as `gs.scenario.m5b3`: `<bin>/scenario/m5b3`, schemas `aion_{ls,gs}_test_m5b3_<hash>`, the shared `RESOURCE_LOCK`, TIMEOUT 900, the profile of `m5b3.properties.example` key by key; cases S-0, L0, C1-C3, L1-L3, L4 (with L3b), L5, L6, L6b-L6c (with L3b), L7, L8, L9, L10, L11, L12, L13 |
| G-04 | the same file, `ScenarioTests.cmake`; `tools/oracle/m5b3/materials.py`, `geo/probes.py`, `oracle.py`, `tests/test_m5b3_items.py` | `TEST(M5b3ScenarioGeo, Run)` as `gs.scenario.m5b3_geo` (TIMEOUT 2700, labels `scenario;realdata;geo`) with case L14, the camp fire; `oracle.py m5b3-material --near X,Y,Z --stand` emulates the TOUCH ray (AbstractCollisionObserver.java:47-76) and picks the point; `MaterialStandTest` (2 cases) on a synthetic scene |
| G-07 | `tests/scenario/M5b2ScenarioTest.cpp` | S14: when monster B dies without killing the Mage, the case waits for B's respawn at spot B (`respawnTime` + 30 s) and pulls the respawn, at most twice. S10 (it recurred here, §17.3): when monster A dies without having hit the Mage, the case waits for A's respawn at spot A and pulls it, at most twice, and a failure prints what A did (`npcActivity`: its swings, who swung at it, its HP changes by skill, its death and last attacker). The assertions are unchanged. **Superseded by §18.1** (the death is looked for from S9 on; the respawn lookup and the pulls changed) |
| harness | `tools/oracle/oracle.py` | the command line answers in UTF-8 (`sys.stdout.reconfigure`): the first gate run failed parsing "Illusion Godstone: Freyr's Esprit", whose apostrophe the Windows ANSI code page wrote as an ill-formed UTF-8 byte |

**How the gate reads the server.** Every item packet is applied in arrival order to a model of the cube, the equipment and the regular
warehouse (`InventoryModel`, decoded with ItemDecoders.h/PacketDecoders.h); that model is what a loot is expected to merge into (Y3), what
the oracle's cube budget is asked about before each corpse (`m5b3-drops --inventory`, L3b), the count before the potion and the split (Y8,
Y10) and what `inventory` must hold after the quit (Y12). The starter stacks and the two junk stacks are followed by object id, so a port
that adds a second stack fails its own row, not a later lookup. **A case that fails only non-fatally (EXPECT) lets the next case run**; an
exception or an ASSERT stops the script as in M5b-2 - so a mutant shows which rows it breaks and that the others hold.

### 17.2 Corrections to §10 (the Java and the data over the plan)

1. **Y8:** the heal-over-time's lifetime is `duration2 + 1000` = 21,000 ms, not ~20,000: `AbstractOverTimeEffect.getDuration2()` adds a
   second ("on retail these effects last one sec more", AbstractOverTimeEffect.java:64-67), and `Effect.calculateTemplateDuration` takes the
   first template with a duration, the HealEffect (Effect.java:899-910). Measured 21,000 ms in every run.
2. **Y7 and Y15, "value > 0":** `TYPE.DAMAGE` is written negated (SM_ATTACK_STATUS.java:125-133), so a proc's or a fire's damage is a
   NEGATIVE int on the wire (-102, -41 for 8267; -1 per fire tick). `TYPE.HP` shares the byte 7 and is written as it is: the potion's +37.
3. **Y5:** `ItemEquipmentListener.onItemUnequipment` without `endEffect(item)` is an **equivalent mutant** for the starter Training Sword
   (it survived, §17.4): the sword has no `<modifiers>`, and its `weapon_stats` reach SM_STATS_INFO through PlayerGameStats' BASE from
   `Equipment.getMainHandWeapon` (PlayerGameStats.java:141-147 and the critical/accuracy twins); the main-hand attack falls on the unequip
   because the weapon leaves the main hand (the sword-mastery passive checks the weapon group), 29 -> 26 -> 29. Y5 now also requires the
   parry, the main-hand accuracy and the main-hand critical to fall (247/250/52 in the clean runs); the mutant that kills it is
   `Equipment.unEquip` keeping the weapon in the equipment map.
4. **Y9, "the same-storage move: no packet at all":** no item or storage packet and no message. The character's own HP packets keep coming
   (L7's heal-over-time ticks every 2 s for 21 s, and regeneration), and they are no answer to the move - the first run failed on them.
5. **Y14:** the run-time drops are CheckOutput's `RuntimeDropItem` row (created = the 30 entries the three corpses listed, live 0); the
   static-data `DropItem` row counts custom drops (0 here). "`AttackResult`, `Effect` live 0": `AttackResult` live 0 holds; `Effect` live
   equals `effectsHeld` (309, the post-spawn statup buffs of M5b-2's X13), so the gate asserts the relation. "`Item` within its account
   bound": `Item` live is 0 at the stop (the shutdown saves and releases the online character).
6. **Y10/Y12 (§10.4):** `splitItem` without the decrease fails Y10 alone; Y12 compares the database with what the client was told, and both
   carry the duplicated count.
7. **L3b's budget:** the oracle's worst case plus the sword for L4 (as planned), **2 before L6** (the seeded stone and the unequipped sword;
   rev 2 left the stone out - `Equipment.unEquipItem` refuses on a full cube), the worst case plus **L9's split** before L6b (the sword is
   not unequipped again), and **one spare slot** each time, so that a stack that should have merged fails Y3 and not Y5 at the next unequip.
8. **Slots (L8, L9, L11, Y12, Y16):** L8 moves the starter life potions to cube slot 21 and the mana potions to 22 (the same-storage arm);
   L9 splits into slot 23 (rev 2's -1 would leave Y12's slot row nothing to check); L11 swaps the mana potions (slot 22) with the warehouse
   junk (slot -1), and each takes the other's slot. Y12 therefore finds the junk back in the cube at slot 22 and the mana potions in the
   warehouse at -1, not "the warehouse junk at `item_location = 1`"; it compares every row with the model, both directions.
9. **§13 question 3 and the hand-off's camp-fire notes.** A creature has **one** `ZONE_MATERIAL_ACTION` task: `AbstractMaterialSkillActor.act`
   schedules it only when the creature has none (AbstractMaterialSkillActor.java:37-44), with the skills of the actor touched first - and the
   actors' collision checks run on the thread pool, so on a point that touches the fire and the firepot, whose conditions apply is a race;
   overlapping zones never double the ticks (the same skill 8302 either way, so "count per zone" cannot be done from the wire either), and
   pinning the game time and the weather is unnecessary on a point that touches the fire alone. The zone is a **SEMISPHERE**, entered only
   ABOVE its center (`SemisphereArea.isInside3D`, `this.z < z`): a player on the floor beside the fire (z 119.13) is outside it; the fire
   mesh is PHYSICAL | MATERIAL and a client stands on its top (119.50). `oracle.py m5b3-material --stand` evaluates the TOUCH ray (from z +
   0.05 + 1.75 down to getZ - 0.11, a player's bound height 1.0 x 1.75, PlayerAppearance.java:1051) against every nearby zone's own
   triangles over a 2 cm grid of the fire's bound: 2,916 points, 963 touch the fire, 321 the fire alone; the point with the largest
   clearance is (863.5389, 1252.2198, 119.5043), 0.08 m from any point that does not qualify, inside the fire's and the firepot's zones and
   touching only the fire; the step-off point (865.07, 1248.81, 119.13) is 4 m away and inside no zone. (The review of stage 2: these two
   points make the ticks deterministic but give the ray no negative case - a port that ignores it passes; §18.2 adds the untouched point.)
10. **Y15's timing:** `MaterialSkillTask` runs every second from the first touched second and uses the skill when `secondsElapsed++ %
    frequency == 0`; until it has used it once (a protected player, a condition that does not match) the modulus is 1 and it tries every
    second, so only the ticks after the first are sure to be 5 s apart. The walk ends the protection before the step (CM_MOVE.java:140-141),
    so the first tick comes at once and the gaps are 5 s: measured 3 ticks at 183/5,183/10,183 ms and 228/5,229/10,230 ms. "None after it steps off" is guarded twice: the step-off's TOUCH check untouches (abort, and the task returns on
    `!isTouched`) and `onLeaveZone` aborts; a mutant of either one survives, one of both does not (§17.4).
11. **Y6:** the stone's `SM_DELETE_ITEM` carries `ItemDeleteType.USE` (`decreaseByObjectId`'s DEC_ITEM_USE, Storage.java).
12. **Y7's k:** the evaluated hits were 1 or 2 in the clean runs (a critical first hit plus the 102 of the proc leaves the second swing a
    kill), so the "one message for k > 1" clause discriminates only in runs with k >= 2; the gate prints k and the mutation run had k = 3.

### 17.3 Gate runs (Debug, one at a time, exact-name regexes, seconds)

| Gate | Runs | Notes |
|---|---|---|
| gs.scenario.m5b3 | development r1-r3 failed (the oracle's encoding; Y8's 20,000; Y14's `DropItem`; Y9's HP ticks), r4 passed 128; after the hardening c1 passed 132, c2 passed 125; **final binary: f1 passed 143, f2 passed 129** | kinah k = 5 to 25 over the runs; Y7 k = 1 or 2 |
| gs.scenario.m5b3_geo | r1 passed 299, c1 passed 288, c2 passed 288; **final binary: f1 passed 290, f2 passed 302** | 3 ticks every run, 5,000 ± 2 ms apart; startup ~140 s |
| gs.scenario.m5b2 | r1 passed 172, c1 passed 167, **c2 FAILED at S10** (128 s; kept in the scratchpad `stage2/fail-c2-gs.scenario.m5b2`); with a first S10 change d1-d4 passed 188/174/167/190; **final binary: f1 passed 192, f2 passed 174** | the S10 and S14 branches of G-07 were not needed in the passing runs; S6 did not fail |

**S10's recurrence (c2).** "monster A never damaged the Mage" after 30 s. What the saved output shows without a packet dump: S9's Flame
Bolt landed and did not kill A (X4 passed, the damage applied equal to the announced 141-base bolt, so A had more HP than it took), S10
attacked nothing, and yet X12's kill list and `DropNpc` (created 3, live 0) hold A's respawn (object 103919): **A died inside S10's window
without having hit the Mage** - the S14 pattern at spot A. Its `SM_EMOTION(DIE)` names the Mage as the last attacker, which nothing in the
saved output explains; the new failure message prints A's activity so the next occurrence says who damaged it.

**Corrected 2026-09-24 (the review of stage 2, §18.1):** the inference "X4 passed, so A had more HP than the bolt took" is wrong. X4 expects
min(announced, maxHp), and `reduceHp` sends the HP actually taken (CreatureLifeStats.java:100-110), so a Flame Bolt that kills passes X4. S9's
bolt was a **magical critical** (141 x 1.5 > 199) and killed A in S9, credited to the Mage - ordinary Java behaviour. S10 waited 30 s for a
dead npc, and its branch only looked for a death after S10's start. The review's mutant RC9 reproduced c2 exactly. The death is
explained, and the G-07 branch above did not cover it.

Besides the gates: `tools.oracle` passed (with `MaterialStandTest`), the 141 harness unit tests of `aion_gs_scenario_tests` (decoders,
GameSession, Oracle, the database and server harness) passed, `lint_concurrency --werror --cycles=core game-server/src` reports 3,711 files,
0 errors, 0 warnings, 0 advisories and `chunks.py check` 0 problems; only `aion_gs_scenario_tests` (and `aion_game_server` for the mutants)
was built, `--parallel 6 -p:CL_MPCount=2`.

### 17.4 Mutation runs (each a real gate run of a production mutant; the file restored byte for byte - sha256 and an empty `git diff` - before the gate started)

| # | Row | Mutant | Result |
|---|---|---|---|
| M1 | Y1 | `registerDrop` sends no `LOOT_ENABLE` | Y1 failed (the three corpses, both rows); everything else passed |
| M2 | Y2 | `collectAllowedDrops` without the `min_diff..max_diff` test | Y2 failed (43 entries instead of 10, 8 rules without an entry); the cube then overflowed at entry 16 (Y3) and a fatal lookup ended the script |
| M2b | Y2 | `registerDrop`'s running index from 0 | Y2's index row failed for the three corpses; everything else passed |
| M3r | Y3 | `addStackableItem` never merges | Y3 failed (L3's two potion merges, L4's shard, L6c's shard and junk); everything else passed |
| M4 | Y4 | `Storage.increaseKinah` adds twice the amount | Y4 failed (1000 + 11 -> 1022); everything else passed |
| M5 | Y5 | `onItemUnequipment` without `endEffect(item)` | **survived**: equivalent for the starter sword (item 3) |
| M5br2 | Y5 | `Equipment.unEquip` keeps the weapon in the map | Y5 failed (attack, parry, accuracy, critical unchanged; the appearance does not decode) in L5 and L6; everything else passed |
| M6 | Y6 | the socketing task at 0 ms | Y6 failed (0 ms); everything else passed |
| M7 | Y7 | `ProcAtkInstantEffect::applyEffect` a no-op | Y7 failed (5 procs, no damage packet); everything else passed |
| M7b | Y7 | `GodStone.tryActivate`'s cooldown never expires | Y7 failed (1 message for 3 evaluated hits); everything else passed |
| M8 | Y8 | `canUseItem` without `hasCooldown` | Y8 failed (the second use consumed and was animated); everything else passed |
| M9r | Y9 | `isItemRestrictedTo` lets anything into the warehouse | Y9 failed (no refusal, no unlock, the event potion moved); everything else passed |
| M10 | Y10 | `splitItem` does not decrease the source | Y10 failed; everything else passed (item 6) |
| M11 | Y11 | `CM_DELETE_ITEM` deletes with DEFAULT | Y11 failed (L3b's deletes and L10); everything else passed |
| M12 | Y12 | `ItemStoneListDAO.save` skips the godstones | Y12 failed (no `item_stones` row); everything else passed |
| M13 | Y13 | an `AION_PARTIAL` left in `registerDrop` | Y13 failed (not allowed; a DropRegistrationService site); everything else passed |
| M14 | Y14 | `DropService.unregisterDrop` a no-op | Y14 failed (`DropNpc` 3 live, `dropNpcsHeld` 3); everything else passed |
| M15 | Y16 | `switchItemsInStorages` adds before it deletes | Y16 failed, and Y12 (the client dropped the two items it had just received); everything else passed |
| M16 | Y15 | the material task uses the skill every second | Y15 failed (12 ticks 1 s apart); everything else of the geo gate passed |
| M17 | Y15 | `onLeaveZone` does not abort | **survived**: the untouch aborts too (item 10) |
| M17b | Y15 | the actor never untouches and `onLeaveZone` does not abort | Y15's step-off row failed (a tick after stepping off); everything else passed |

Three first attempts (M3, M5b, M9) showed the targeted row failing but an ASSERT or a decode exception of the gate ending the script;
the gate was hardened (EXPECT inside the loot loop, the equip rows and L8's unlock; a decode failure of an equip packet is a Y5 failure)
and the three were rerun (M3r, M5br2, M9r). The runs up to M4 used a gate binary before that hardening; the rows they target did not change.

### 17.5 Left for a regate

- **S14 (M5b-2)** did not recur in the stage-2 runs, so its new respawn branch has not run against the server; the saved failure of the
  stage-1 integration (the scratchpad's `s1i/fail-r1-m5b2`) is the case it is written for. The same holds for S10's branch (c2).
- **S10's attribution:** in c2 monster A died with the Mage named its last attacker although S10 made no attack after S9's bolt, which had
  not killed it; the next occurrence prints A's swings, attackers and HP changes (`npcActivity`) and should settle whether that is a
  neighbour's fight credited oddly, a delayed hit or a port defect in the death attribution (`CreatureLifeStats::onHpChanged` passes the
  killing damage's effector, as Java does). **Settled 2026-09-24 (§18.1):** S9's bolt did kill it, as a magical critical. No port defect.
- **Y1's `lootEffectId`** is checked on every corpse, but a mutant that hardcodes 0 is caught only when a listed godstone has 1003 (about 40
  % of corpses; it happened in some runs and not in others) - L-05's unit case stays the proof.
- **Y7's cooldown clause** discriminates only in runs with k >= 2 (item 12).
- The oracle's stand point relies on the double-precision emulation of the TOUCH ray with a 0.08 m clearance; a geometry change in the
  client data (a new `models.mesh`) moves it, and the oracle recomputes it on every run.

## 18. The review of stage 2, and its fixes, 2026-09-24

The review of stage 2 (verdict "changes requested": one high, three medium, two low findings and one info) ran its own mutants and kept
the evidence in the session scratchpad `review2/`. This section records the fixes. Each fix has a real gate run of a production mutant: the
file was saved, mutated, built into `aion_game_server`, and restored byte for byte (sha256 and an empty `git diff`) **before** the gate
started. The mutant runs' logs are in the scratchpad `fix3/logs/`. **No production file changed.** Files: `M5b2ScenarioTest.cpp`
(S9, S10, S14, and S6's level-ready pattern, §18.5), `M5b3ScenarioTest.cpp`, `tools/oracle/m5b3/materials.py`, `tools/oracle/oracle.py`
(help text) and `tools/oracle/tests/test_m5b3_items.py`.

### 18.1 M5b-2's S10 and S14: the cause is a magical critical (the high finding and two medium ones)

**The cause.** Flame Bolt 1282 has `apply_magical_critical="true"`. A magical critical multiplies the damage by 1.5
(AttackUtil.calculateSkillResult -> calculateWeaponCritical, AttackUtil.java:191-208, 318-323). The bolt's normal damage to a fresh 210663
is already 197 of 199 HP (the runs below print "197 by skill 1282"), so a critical bolt takes all of a full 210663's HP. X4 still passes:
it expects min(announced, maxHp), and `reduceHp` sends the HP actually taken (CreatureLifeStats.java:100-110). The review's mutant **RC9**
makes every magical-critical roll of the Mage succeed; it reproduced c2 exactly (S10 failed after 30,000 ms, A dead in S9, credited to the
Mage). So:
- **S10 (c2):** A died to S9's bolt. §17.3's "X4 passed, so A had HP left" and §17.5's "unexplained attribution" are corrected there.
  G-07's branch did not cover this case: it only looked for a death after S10's start.
- **S14 (s1i/fail-r1-m5b2):** B died to the pull's Flame Bolt. It was not HP drained by a neighbour: the tribes rule that out (§16.4,
  corrected there).

**The fixes (`M5b2ScenarioTest.cpp`).**
- `s9From`: S9 records where its recording starts. S10 looks for A's death from there, and its wait for A's swing also ends at A's death
  (`isDeathOf`, `deathIndexOf`). S9's X4 line says when its bolt killed A.
- `waitForRespawnAt(spot, diedAt, timeout)`: the respawn is the first `SM_NPC_INFO` at the spot recorded after the death whose object was
  never announced at the spot before it. The old `waitForNpcAt(spot, current)` answered the npc an earlier case had killed at the same spot
  until the respawn was announced. That was the second finding: the review's RS10b pulled S5's corpse, and S14's second round would have
  pulled the original B. S10 and S14 both use the new lookup.
- `pullWithSwing(npc)`: one auto-attack of the Training Spellbook (100600034: 20-23 damage, attack_range 15000), sent again only while it has
  not gone out, at most three times. It cannot kill a 199-HP npc. S10 pulls A's respawn with it. S14 pulls B with it (a Flame Bolt only
  when no swing went out), so a critical can no longer kill B at the pull. S14 keeps its respawn branch and now prints its steps on every
  run.
- The comments of S10 and S14 give the critical-hit cause and cite RC9.

**Evidence** (`gs.scenario.m5b2`, each a real gate run):

| Run | Mutant | Result |
|---|---|---|
| RC9 | `StatFunctions::calculateMagicalCriticalRate` returns true for a Mage (the review's c2 reproducer) | **passed, 193 s**. "X4: … landed; it killed monster A". "S10 (G-07): A (object 103928) died in S9 without hitting the Mage: … 1 HP changes (199 by skill 1282) … died, last attacker the Mage; A respawned as object 103935; the Mage's swing went out; then A hit the Mage". S14: "the Mage's swing at B went out; the Mage died". The kill list 26048, 35183, 103928, 103935 shows that the old lookup would have answered 26048, S3's A, which S5 killed. The review's run of the same mutant against the stage-2 script failed at S10 |
| RS | `CreatureController::attackTarget`: a 210663's first swing at a Mage kills the npc instead (a delayed hit credited to the Mage); the 1-HP Mage's first two swings at a 210663 take all its HP | **passed, 214 s**. S10: "A (object 103923) died in S10 without hitting the Mage … respawned as object 103941 … then A hit the Mage" (the in-window death of RS10b). S14's half did not fire: the Mage had regenerated above 1 HP |
| RS2 | the same, with the Mage's half taken below 10 % HP | **passed, 212 s**. S14: one round, "B (object 25014) died without killing the Mage … B respawned as object 103942 … the Mage died". The second kill did not fire (the Mage was above 10 % by then) |
| RS3 | the same, with the Mage's second and third swings at a 210663 killing it | **passed, 236 s**. S14 went two rounds: "B (object 25334) died … respawned as object 103944 … B (object 103944) died … respawned as object 103950 … the Mage died". The old lookup would have answered 25334 in the second round |

### 18.2 Y15's negative case: the TOUCH ray (medium)

The review's RV1 (`ZoneCollisionMaterialActor::onMoved` with `isTouched = true`) passed the whole geo gate. The stand point is on the fire
mesh, so it is touched either way, and the step-off point is inside no zone.

**Oracle.** `oracle.py m5b3-material --stand` now also answers `stand.untouched` (`materials.py untouched_point`). It searches a 5 cm grid over
the fire zone's disc, at least 0.25 m outside the fire geometry's world bound. The heights are the PHYSICAL surface, plus heights 5 cm apart
above the zone's center within 1 m of that surface. The zone is a SEMISPHERE whose center stands 0.19 m above the floor, so the client must
report a z above the floor, which CM_MOVE takes as the client sends it. A candidate's margin is the smaller of its depth inside the fire's
area and its distance outside every other nearby zone's area (`area_depth`). The chosen point has the largest margin among candidates whose
emulated TOUCH checks all miss. For the Poeta fire it is **(864.2429, 1251.1038, 119.5162)**: 0.38 m above the floor, margin 0.185 m,
0.88 m outside the mesh's bound, inside the fire's zone only (the firepot's sphere is 0.185 m away), with no ray hitting. Out of 2,948
columns and 8,771 candidates, none was rejected. `MaterialStandTest.test_the_untouched_point` checks it on the synthetic scene: inside the fire's
area alone, no ray hitting, at least 0.25 m outside the bound, not below the floor, on the side away from the pot, with a margin above
0.33 m (the analytic optimum is 0.354 m) and, without the pot, above 0.95 m.

**Gate (`M5b3ScenarioTest.cpp` L14).** After the 3 s on the step-off point, the character stands 7 s on the untouched point. Entering the
zone creates the fire's actor and runs its check at once (MaterialZoneHandler.onEnterZone -> actor.moved()). It then goes back to the
step-off point, which leaves the zone, and on to the fire as before. The new Y15 row: no tick of 8302 in that window.

| Run | Gate | Result |
|---|---|---|
| g1 (clean) | gs.scenario.m5b3_geo | **passed, 325 s**; 3 ticks on the fire at 209 / 5,210 / 10,210 ms, none at the untouched point |
| RV1 | gs.scenario.m5b3_geo | **failed, 310 s**, only in the new row: "2 tick(s) at the untouched point" (at -8,514 and -3,513 ms from the step onto the fire). Standing, the 5 s gaps and the step-off rows passed, and so did every other case (S-0 … L13) |

### 18.3 Y2's count and bijection rows (low)

Stage 2's M2 overflowed the cube and M2b covered only the index row. The new mutant **RY2** is `checkGlobalRuleWorlds`: a `gd_worlds` rule
never matches. It removes "Weapons (Common)" and "Armor (Common)" from 210663 and "Weapons (Common)" from 210133 (the only applicable rules
with a `gd_worlds` restriction, rules_equipment.xml). No later case uses those items. `gs.scenario.m5b3` **failed, 177 s**, only in Y2's
count row (M5b3ScenarioTest.cpp:1608: 8, 9 and 8 entries against the oracle's 10) and its bijection row (:1630: "2 applicable rule(s) of npc
210663 have no entry", "1 … of npc 210133"), on all three corpses. Every other row passed, including L4's kinah, the merges, Y12, and L13's
Y14 (`RuntimeDropItem` created = the 25 entries listed). The §10.4 rows that were never run are listed there.

### 18.4 Documentation only (low, info)

- **Y14 and `resendDropList`** (the review's RK1: `resendDropList` without `delete()` failed Y3 alone): §10.3 and §10.4 are corrected. Y14's
  counts are written after the shutdown, which deletes a corpse that outlived its loot, so Y3's `SM_DELETE` within 1 s is the only proof.
- **Y8's min()**: L7 seeds 100 of 284 HP, so only the 37 arm is checked. The review's RV2 (`calculateHealValue` without the cap) survived,
  equivalent on the wire while `increaseHp` clamps. Noted in §10.3 and next to the row.

### 18.5 Gate runs on the final sources (Debug, one at a time, exact-name regexes, seconds)

`aion_game_server` was rebuilt from the restored sources after the last mutant: four files, each with its sha256 checked and an empty
`git diff -- game-server/src`. Every gate below ran on that binary.

| Gate | Runs | Notes |
|---|---|---|
| gs.scenario.m5b3 | **f1 passed 155, f2 passed 138** | the M5b3 script is unchanged since these runs |
| gs.scenario.m5b3_geo | **g1 passed 325, f2 passed 330** | 3 ticks each (209/5,210/10,210 and 376/5,376/10,377 ms), none at the untouched point |
| gs.scenario.m5b2 | f1 passed 179; **f2 FAILED at S6** (92 s; output kept in the scratchpad `fix3/fail-f2-gs.scenario.m5b2`); after the S6 fix below **f3 passed 172, f4 passed 174** | S14 pulled with the swing in every run ("the Mage's swing at B went out; the Mage died") |
| gs.scenario.m5b2_geo | f1 passed 292 (before the S6 fix); **f2 passed 293** | the same script with geodata |

**S6 (f2): the cause and the fix.** The Mage's CM_LEVEL_READY burst matched §5.8 up to its `SM_CUBE_UPDATE`, and then one more
`SM_NPC_INFO` arrived inside the burst's quiet window: "the packets match the sequence up to packet #35 SM_NPC_INFO, where it expected the
end of the packets". The M5b-2 lane saw the same kind of failure before, with a trailing `SM_ATTACK_STATUS` (its runs 5 and 9). An object that
spawns or comes into view while the burst is still being read is announced by the known-list update, after the answer. The Mage enters about
20 s after S5 killed monster A, and A's respawnTime is 20 s, which makes A's respawn the likely one. The saved output has no packet dump, so
the npc is not named. M5b-2's `levelReadyPattern` now accepts `(SM_NPC_INFO | SM_GATHERABLE_INFO)*` after `SM_CUBE_UPDATE`; the order up to
it is asserted as before. `levelReady` prints each such trailing npc ("level ready: after the answer, SM_NPC_INFO of npc …"), and none came
in f3, f4 or the geo run. The M5a, M5b and M5b-3 copies of the pattern are unchanged: M5b-3 asserts only its first level ready, before any
kill.

Besides the gates: `tools.oracle` passed (154 s, with the new `MaterialStandTest` case). The harness unit tests of
`aion_gs_scenario_tests` (every test but the gates and `M5aStress`) passed: 154, plus `OracleRunTest` once `AION_TEST_PYTHON` was set.
`lint_concurrency --werror
--cycles=core game-server/src` reported 3,711 files with 0 errors, 0 warnings and 0 advisories, and `chunks.py check` reported 0 problems.

### 18.6 Left for a regate

- The respawn branches of S10 and S14 have run only under mutants (RC9, RS, RS2, RS3). A natural critical Flame Bolt in S9 (c2's case) now
  takes the S10 branch. That costs the respawn wait (20 s) plus a pull, well inside TIMEOUT 900.
- The untouched point floats 0.38 m above the floor. A server that snapped a player's z to the ground would move it out of the SEMISPHERE,
  and the row would pass without proving anything. Java does not snap (CM_MOVE takes the client's z), and RV1 shows that the port's
  character is inside the zone there.
- Two other lanes were building in `build/leak-a` and `build/leak-b` during the final runs. Their load was on the same machine.
- S6's trailing `SM_NPC_INFO` (f2) is the only new failure. The fix relaxes the burst's end, not its order. Which npc it was stays
  unnamed: the saved run has no packet dump, and the new log line will name the next one.

## 19. Stage 2 results (the regate), 2026-09-24

The regate ran after §18's fixes, on the working tree over HEAD `4867fbc44`: a full build, the unit suite, and **all eleven gates, one at a
time. Each gate passed on its first run.** Stage 2 changed no production file. Every gate ran on binaries built at 21:24 from a tree with an
empty `git diff -- game-server/src`, and the diff was checked again before each gate started. Nothing is committed. **M5b-3 is complete**
(phase5-roadmap.md, row 2). The logs are in the session scratchpad under `regate3/logs/`.

### 19.1 §18.6's list, item by item

- **The respawn branches of S10 and S14.** No natural critical came in either M5b-2 run. S9's bolt landed without killing A (X4 "landed"),
  and S14 printed "the Mage's swing at B went out; the Mage died". Neither branch ran, so they are still proven only by mutants (RC9, RS,
  RS2 and RS3, §18.1).
- **The height of the untouched point.** Unchanged, and documented in §18.6. No packet tells the client that the server placed the
  character inside the zone. The run RV1 remains the evidence.
- **S6's trailing npc.** No npc arrived after the level-ready answer in either M5b-2 run: neither log has an "after the answer" line.
- **Y8's cap arm.** Still documented only (§10.3, §18.4). To check it, the gate would need a second potion use at an HP within 37 of the
  maximum. That use must wait out the 30 s `usedelay`, which `ItemCooldownsDAO` keeps across a relog, and natural regeneration races the
  HP during the wait. A regate does not add a case like that.
- **The limits of Y1, Y7 and Y15 (§17.5).** Unchanged. In this run both clauses that depend on chance discriminated: Y1 saw `lootEffectId`
  1003 on both 210663 corpses, and Y7 had k = 2.
- None of these items needed a change to `tests/scenario` or `tools/oracle`.

### 19.2 Build, unit tests, lint

- `cmake --preset msvc -DAION_BUILD_CHAT_SERVER=ON`, then all targets, Debug, `--parallel 6 -p:CL_MPCount=2`: **0 errors, 0 warnings**, in
  54 s. The build was incremental: the only new objects were two lifetime tests that another lane had added and not yet tracked
  (`effects_mz/NpcEffectLifetimeTest.cpp`, `handlers_ai_core/NpcCastDeathLifetimeTest.cpp`).
- `ctest -C Debug -j 6 -LE "scenario|geo|m4|nightly|stress|smoke"`: **3,360 of 3,361 passed in 717 s.** The one failure was `tools.oracle`,
  in `test_m5b2.M5b2JavaRulesShapeTest.test_the_constant_is_read_from_the_override`, which found no `SkillEngine.java` in its temporary
  tree. Another lane was editing `tools/oracle/m5b2/skills.py` and `tests/test_m5b2.py` during the run; neither file is in stage 2's list.
  Its new `_read_launch_rules` reads `SkillEngine.java`, and the test's helper, which copies part of the Java tree, did not copy that
  file yet. After that lane's next edit of the test (21:51), **`tools.oracle` passed on its own in 146 s** (355 tests, stage 2's
  `MaterialStandTest` included).
- `lint_concurrency --werror --cycles=core game-server/src`: 0 errors, 0 warnings, 0 advisories. `chunks.py check`: 0 problems. Both
  ran again at the end, after the leak lane's in-progress production edits (§19.6), so they cover those edits too. The gates do not.
- **Process note.** The first attempt at the unit run stopped while listing the tests. A `ctest -N` had been started beside it. Discovery
  runs before the tests (PRE_TEST), so both processes rewrote the `*[1]_tests-Debug.cmake` files at the same moment and interleaved them
  (`aion_gs_legionhouse_tests`, `…controllers…`, `…player…`, `…handlers_ai_core…`). Deleting the regenerated files and running one
  `ctest -N` alone repaired them. **Two `ctest` processes in the same build directory can corrupt its discovery files**: one more reason to
  run one `ctest` at a time.

### 19.3 Gates (Debug, one at a time, exact-name regexes, `--output-on-failure`, seconds as ctest reports them)

| Gate | Result | Seconds | Earlier (§16.4 final; §18.5) | Notes |
|---|---|---|---|---|
| gs.smoke.startup | **passed** | 34 | 25 | |
| gs.smoke.startup_geo | **passed** | 150 | 144 | 83,885 npc spawns, 0 spawn failures, 0 unexpected ERROR entries |
| gs.m4.check_static_data | **passed** | 149 | 140 | |
| gs.scenario.m5a | **passed** | 65 | 53 | |
| gs.scenario.m5a_geo | **passed** | 170 | 156 | |
| gs.scenario.m5b | **passed** | 227 | 218 | R3: one `LOOT_ENABLE`, `lootEffectId` 0; Q2 `DropNpc 0 1` |
| gs.scenario.m5b_geo | **passed** | 351 | 349 | |
| gs.scenario.m5b2 | **passed** | 172 | 163; 172, 174 | S9's bolt landed and did not kill A (no S10 branch); S14 pulled with the swing; X12: 3 kills, `DropNpc 0 3`; no npc after the level-ready answer |
| gs.scenario.m5b2_geo | **passed** | 301 | 297; 293 | the same as m5b2 |
| gs.scenario.m5b3 | **passed** | 146 | -; 155, 138 | below |
| gs.scenario.m5b3_geo | **passed** | 314 | -; 325, 330 | Y15: 3 ticks of 1 HP at 127, 5,127 and 10,127 ms after the step onto the fire; none at the untouched point (0.382 m above the floor, margin 0.185 m) and none after the step off |

Most gates took 5 to 20 s longer than in §16.4. Other lanes were working on the same machine at the time (the lifetime and leak lanes,
and the oracle edits above). No gate came close to its timeout.

**What `gs.scenario.m5b3` saw.** L0: 10 entries for each monster (210663 at 53.4 m, 210133 at 76.8 m). Y1: `lootEffectId` 1003, 0 and 1003,
as each corpse's entries predict. Y4: kinah 1,000 + 17. Y5: the main-hand attack went 29 -> 26 -> 29. Y6: 2,001 ms. Y7: k = 2, two proc
messages, damage -102 and -39, none resisted. Y8: +37 at 104 of 284 HP, the stack 100 -> 99, a heal-over-time of 21,000 ms. Y9, Y10 and
Y16: the exact packet orders. Y12: 29 `inventory` rows and one `item_stones` row. Y14: `DropNpc 0 3`, and `Effect` live 309, equal to
`effectsHeld`. No gate log has a leak-census line.

### 19.4 The corrections, in one place

Where the plan and the Java or the data disagreed, the gate follows the Java and the data. Each correction is written where it was found,
and §10's dated notes point to most of them:

1. **Y5:** the Training Sword's stats reach the player through the base, because the sword has no `<modifiers>`. So `onItemUnequipment`
   without `endEffect` is an equivalent mutant for it. The parry, accuracy and critical fall too (§17.2 item 3).
2. **Y7 and Y15:** a damage goes on the wire as a negative value (§17.2 item 2).
3. **Y8:** the heal-over-time lasts 21,000 ms (§17.2 item 1). Only the 37 arm of min(37, maxHp - hp) is checked (§18.4).
4. **Y9:** "no packet" means no item or storage packet (§17.2 item 4).
5. **Y10 and Y12:** a split that does not decrease the source fails Y10 alone (§17.2 item 6).
6. **Y14:** the run-time drops are counted in the `RuntimeDropItem` row, and live `Effect` equals `effectsHeld`. **Y14 does not prove that
   a corpse is deleted; only Y3 does** (§17.2 item 5, §18.4).
7. **Y6:** the stone's `SM_DELETE_ITEM` carries `USE` (§17.2 item 11).
8. **L3b's budget and the slots of L8 to L11** (§17.2 items 7 and 8).
9. **§10.5 and §13 question 3:** a creature has one material task. The SEMISPHERE is entered only above its center, and the character
   stands on the fire's mesh. The untouched point is the TOUCH ray's negative case (§17.2 items 9 and 10, §18.2).
10. **M5b-2's S10 and S14:** the cause is a magical critical Flame Bolt, not a neighbour. S6's trailing `SM_NPC_INFO` is §18.5's (§16.4,
    §17.3, §18.1).
11. **§10.4:** the rows that never ran as mutants are listed in §10.4's note.

The regate adds no correction of its own. It adds a note to §11 that points to §19.5.

### 19.5 What the real-client checklist (§11) should expect

For §11's profile: Java's drop rates and events disabled. Each numbered item below is the same item of §11.

1. Every kill sends the killer `LOOT_ENABLE` (Y1), so **every corpse sparkles**, even with nothing on it. When an illusion godstone from
   the list is on the corpse, the packet carries loot effect 1003.
2. The loot window lists the drop. Each item either adds a stack or merges into one of the same id (Y3). After the last item the window
   closes and the corpse **vanishes within a second** (Y3's `SM_DELETE`).
3. For 210133, a kinah entry at level 1 is 5 to 25; the counter rises by that much, and no item is added (Y4).
4. **No gate covers the 5-minute decay** of a corpse left with loot (§18.4). Only the client check shows it.
5. With the Warrior's Training Sword, the main-hand attack goes **29 -> 26 -> 29**. Parry, accuracy and critical fall with it and return
   (Y5). Other gear shows other numbers, but the same round trip.
6. Split, swap and destroy are covered by Y10, Y16 and Y11, and a swap exchanges the two slots. **No gate covers a split into the
   warehouse.**
7. **The warehouse cannot be opened from the client yet.** The npc dialog (`CM_SHOW_DIALOG`) is M5c's. The gate sends `CM_MOVE_ITEM`
   without a dialog, which a real client never does. Skip this item.
8. Minor Life Potion: **+37 at once**, then up to 37 every 2 s for **21 s** (the buff shows 21 s, not 20). A second use inside 30 s is
   refused with STR_ITEM_CANT_USE_UNTIL_DELAY_TIME, and no potion is consumed (Y8). Near full HP the instant heal is capped at the missing
   HP; no gate covers that.
9. The Administrator's Boon and the Lodas Amulet are not in any gate script. Only the client check covers them.
10. The godstone: a 2 s bar, and the weapon still has the godstone after a relog (Y6, Y12). 168000116 (Fx Test Earth Godstone) has
    probability 1000 on a main hand, so **every** auto-attack that lands and does not kill shows "proc effect occurred", at most one every
    750 ms (Y7). Skill 8267's damage is magical: -102 and -39 on a 210663 in this run. A resisted proc shows the message without damage.
11. The loud failures are unchanged.
12. **The camp fire burns only a character standing on it.** The zone is a semisphere that is entered only above its center, and the
    TOUCH ray must hit the fire's own mesh. On the fire: one tick at once, then one every 5 s (1 HP each for the gate's level-1 Warrior).
    Next to the fire, even inside its zone: nothing. Stepping off stops it (Y15).
13. The same files. After a clean shutdown with every corpse looted or decayed, `live_counts.txt` shows `DropNpc` live 0 and created equal
    to the kills.

**A real-client session on `4867fbc44` has already happened.** It is `docs/design/m5b3-client-session.md`, written by another lane and not
committed. It confirmed items 1-3, 5, 6, 8-10 and 12. It also found one leaked Npc (its S-1), which the leak lane is fixing (§19.6).

### 19.6 Left for the integrator

- **Commit stage 2.** Its files: `tests/scenario/M5b3ScenarioTest.cpp` and `m5b3_partial_allowlist.txt` (both new),
  `tests/scenario/ScenarioTests.cmake`, `tests/scenario/M5b2ScenarioTest.cpp`, `tools/oracle/m5b3/materials.py`, `tools/oracle/geo/probes.py`,
  `tools/oracle/oracle.py`, `tools/oracle/tests/test_m5b3_items.py`, this plan and phase5-roadmap.md (row 2's status note).
- **Not stage 2's work, though it is in the same tree:** `tools/oracle/m5b2/skills.py`, `tools/oracle/tests/test_m5b2.py` and
  `tools/oracle/README.md` (the launcher work for m5e-plan.md), `docs/design/m5c-plan.md`, `docs/design/m5b3-client-session.md`,
  `docs/design/phase6-questgen-prototype.md`, `tools/gen/**`, the untracked lifetime and probe tests
  (`NpcEffectLifetimeTest.cpp`, `NpcCastDeathLifetimeTest.cpp`, `world/WorldContainerLifetimeTest.cpp`,
  `runtime/services/LeakCensusHolderProbeTest.cpp`), and **the leak lane's production edits**: `world/World.cpp`,
  `world/zone/ZoneInstance.cpp`, `GameServer.cpp`, `runtime/services/LeakCensus.cpp`, the header `LeakCensus.h`, and the new
  `world/WorldLeakProbe.{h,cpp}`. That lane made them from 22:12 on, after the last gate had started, and was still making them
  when this section was written. No gate binary contains them. **If they are committed with stage 2, run the gates again on a binary built from both.** The
  M5b-2 and M5b-3 gates call `oracle.py` at run time, so they did run with the other lane's `m5b2/skills.py` as it was at 21:29.
- Still open from §17.5 and §18.6: Y8's cap arm; Y1's `lootEffectId` check depends on chance, and so does Y7's cooldown clause; Y15 cannot
  catch a port that breaks only one of the two abort paths; the untouched point's height; and the S10 and S14 branches, which only
  mutants have run.

---

## 20. M5c's hand-off edits (m5c-plan.md §3a, item I-01), 2026-09-24

Applied by M5c's integrator lane (stage 0, HEAD `5fbb03a08`, after this milestone's last commit). m5c-plan.md §3a gives every item service
this plan sent to "M5c" one home, and says that this plan must change to match and that the integrator records the change in both plans. It
is recorded in m5c-plan.md §17 as well.

| Row | Was | Is |
|---|---|---|
| D2, "Out" | manastones/enchant/amplify/tempering/stigma/tune/remodel/purify/charge/dye/pack/decompose (M5c), quest-start and read items (M5d), npc warehouses and cube expansion (M5c), mail (M5c) | the homes of m5c-plan.md §3a: M5c takes what a start-map player reaches (manastones, enchant, amplify, extraction, decompose, remodel, identification and tune, cube expansion, craft-learn items, `TemporaryTradeTimeTask`, mail); stigma and skill books go to M5e, quest items to M5d, multi-return and instance-time items to M5f, housing items to M5h, AP extraction to M5i, pets, mounts, cosmetics and the rest to M5j, and the capital-only services (warehouses, charge, purification, the remodel service, armsfusion, tempering, polish, dye, assembly, pack, composition) to the capital-economy milestone of m5c-plan.md D2 (M5j if the user keeps the broker in M5c) |
| O-01 | team loot incl. `TemporaryTradeTimeTask` (no file) → M5g | without `TemporaryTradeTimeTask`, which M5c's P-04 creates: `ExchangeService.addItem` asks it for every untradeable item (ExchangeService.java:108), so the exchange needs it before team loot does |
| O-02 | → M5c | per m5c-plan.md §3a: `EnchantService`, `EnchantItemAction`, the manastone bodies and `ExtractAction` M5c stage 0; `StigmaService` M5e; tempering and composition the capital-economy milestone |
| O-03 | QuestStart/Read → M5d; Ride, ToyPetSpawn, AdoptPet → pets; the rest → M5c | per action, as m5c-plan.md §3a lists them (the 29 actions besides `EnchantItemAction` and `ExtractAction`) |
| O-04 | → M5c | per m5c-plan.md §3a: cube expansion, identification, `CM_TUNE`, `CM_TUNE_RESULT`, `CM_SELECT_DECOMPOSABLE` M5c stage 1; the warehouse and the remodel, purification, charge and unwrap packets the capital-economy milestone; `CM_APPEARANCE` M5j |
| §2.6, "the other `CM_MANASTONE` arms" (added by the review fix of I-01) | `StigmaService.chargeStigma`: M5c (D8) | `StigmaService.chargeStigma` M5e; the enchant and manastone bodies stay M5c (stage 0) |
| §2.8, E-8 (added by the review fix) | stigma items reach `StigmaService`'s other 11 bodies (throw, M5c) | the same bodies, M5e |
| §2.7, the packet table's item-service row (added by the review fix) | `CM_UNWRAP_ITEM`, `CM_TUNE`, `CM_TUNE_RESULT`, `CM_ITEM_REMODEL`, `CM_ITEM_PURIFICATION`, `CM_CHARGE_ITEM`, `CM_COMPOSITE_STONES`, `CM_SELECT_DECOMPOSABLE`, `CM_APPEARANCE` → M5c | `CM_TUNE`, `CM_TUNE_RESULT`, `CM_SELECT_DECOMPOSABLE` M5c stage 1; `CM_UNWRAP_ITEM`, `CM_ITEM_REMODEL`, `CM_ITEM_PURIFICATION`, `CM_CHARGE_ITEM`, `CM_COMPOSITE_STONES` the capital-economy milestone; `CM_APPEARANCE` M5j |
| §2.3, the table of tests to rewrite (added by the review fix) | `tests/economy/DropRegistrationServiceTest.cpp:114-160` | the same citation, with the file's new directory `tests/economy/P5-09a/` (I-01's move; the line numbers are the rev-2 ones) |

**The three recommendations of m5c-plan.md §3a are moot.** They asked this milestone to take identification (A-13), `RemodelAction`'s two
trivial bodies and `CM_QUESTION_RESPONSE` (D-04). M5b-3 closed at `5fbb03a08` without them (m5c-plan.md §0, §15.2), so M5c keeps P-07, E-04's
`RemodelAction` part and D-04. Nothing else in this plan changed (besides the rows of the review fix below): its bodies, gates and allow-lists are as §19 left them. The D2 "Out" homes
that point to the capital-economy milestone depend on the user's answer to m5c-plan.md D2, which is still open.

**Review fix (2026-09-25).** The review of I-01 found three more rows that still sent a §3a item to "M5c" (§2.6's other `CM_MANASTONE`
arms, §2.8's E-8, §2.7's packet row) and one citation of a moved test (§2.3). They now carry the same "per m5c-plan.md §3a" pointer as O-02..O-04,
and the last four rows of the table above record them. §1's prose ("… which are M5c's", the pre-§3a analysis) is left as it was written.
