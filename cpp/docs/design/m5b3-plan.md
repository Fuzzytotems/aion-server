# M5b-3 work plan (loot and items)

> **Status:** plan **rev 2**, 2026-09-23 — rev 1 after its adversarial review (22 findings, 2 high; §14 lists them and what changed). A
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
| drop: `DropRegistrationService` / `DropService` / `DropDistributionService` | P5-09 | 121 + 1 partial (rest: trade 8, exchange 11, broker 11, private store 8, mail 15, craft 9, reward 12, recipe 3, passport 1) | **32 + 1 partial** required (26 + the 6 solo `DropService` bodies); 7 team bodies optional; `DropDistributionService` 4 → M5g | 0 |
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
| `tests/economy/DropRegistrationServiceTest.cpp:114-160` | 3 cases assert the partial and the empty maps | L-05 (loot lane) | with L-01 |
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
| `StatupEffect` | 20 (juice, buff food, event potions) | yes | ported | P5-04 |
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
| **Socketing it**: 4.8 does it with **`CM_MANASTONE` action 4, no npc** (`packets[91]` = `CM_GODSTONE_SOCKET` is commented out, AionClientPacketFactory.java:119) → `ItemSocketService.socketGodstone`: an `ItemUseObserver`, a 2 s `ITEM_USE` task, `decreaseByObjectId(stone)`, `weapon.addGodStone`, `updateItemAfterInfoChange` | `CM_MANASTONE` no C++ file; `socketGodstone` unported (`ItemSocketService.cpp:36`); `Item::addGodStone` ported; cycles rows exist (`cycles.toml:272-273`), fieldmap callback `ItemSocketService@L193:92` exists |
| **The proc**: `CreatureController.applyGodStoneEffect` → `GodStone.tryActivate` (probability − `PROC_REDUCE_RATE`, × `gameserver.rates.godstone.activation.rate`, a 750 ms evaluation cooldown) → `getSkill` → `new Effect(skill, target).initialize(); applyEffect()` → `STR_SKILL_PROC_EFFECT_OCCURRED`; illusion godstones break after `nonbreakcount` activations | ported (`CreatureController.cpp:316`, `GodStone.cpp`); only the 5 effect classes are missing, and `updateItemAfterInfoChange` for the break |
| **The other `CM_MANASTONE` arms** (1 enchant stone, 2 manastone, 3 remove manastone, 8 amplification) | `EnchantItemAction`, `EnchantService`, `ItemSocketService` manastone bodies, `StigmaService.chargeStigma`: **M5c** (D8); they throw until then |

A deterministic gate godstone exists: **168000116 "Fx Test Earth Godstone"** (item_templates.xml:848006): `probability="1000"`, `breakprob="0"`,
skill 8267 = `procatk_instant delta="100" element="EARTH"` — every evaluated hit **procs**, nothing breaks. The Training Sword (100000094,
`mask="138366"`) has `CAN_PROC_ENCHANT` (bit 10, ItemMask.java:17). **Two caveats (rev 2).** (a) Proc is not damage: 8267 is
`skilltype="MAGICAL"` with no `noresist`, so `EffectTemplate.calculate` can resist it at `isDodgedOrResisted` (EffectTemplate.java:314), and
`applyGodStoneEffect` sends `STR_SKILL_PROC_EFFECT_OCCURRED` after `applyEffect()` whether the effect landed or not — in Java
(CreatureController.java:278-283) and, faithfully, in C++ (`CreatureController.cpp:318-324`). The proc is evaluated only for an auto-attack
whose status is neither DODGE nor RESIST (CreatureController.java:255-256). (b) **Skill 8267 has two templates**
(skill_templates.xml:80570, `element="WIND"`, no properties; :80575, `element="EARTH"`, a weapon start condition and `move_casting
allow="false"`); `SkillData.afterUnmarshal` keeps the **last** (`skillTemplateById.put`, SkillData.java:33-39), and the oracle must do the same.

**Material skills.** `AbstractMaterialSkillActor.MaterialSkillTask` (a 1 s fixed-rate task while a creature touches a skill material, geo and
`gameserver.geodata.materials.enable` on — the default) calls `SkillEngine.applyEffectDirectly(skillId, level, creature, creature, null,
MATERIAL_SKILL)`. `material_templates.xml` names 28 skills needing 13 leaf classes, **6 new**: `ProcAtkInstantEffect` (13), `DispelEffect` (5),
`AbsoluteSnareEffect` (4, data-only), `FearEffect` (1), `MpAttackInstantEffect` (1), `ProcHealInstantEffect` (1). **Measured on the geo files:**
Poeta's 4,496 mesh placements include **98 with a skill material** (material 62 ×69, 60 ×22, 61 ×7) and Ishalgen's 5,017 include 48 (the
review's independent lookup, which ignores the `|` aliases of mesh names, found 97 and 44: unconfirmed either way until `m5b3-material`
re-derives it, G-01) — **every one
of them skill 8302 *Flame Strike*, `procatk_instant value="5" element="FIRE" noresist="true"`** (skill_templates.xml:81174-81186, stack
`MATERIAL_SKILL_PROC_BONFIRE_DAMAGE`): the camp fires. Material 60 is unconditional; 61/62 need NIGHT / not-raining. The nearest material-60
placement is `pr_l_fire_semisphere_01a.cgf` at **(863.54, 1252.50, 119.50), 407 m** from the Elyos spawn; the nearest of any is a material-62
fire at (1118.42, 992.52, 131.61), 108 m. **Terrain materials (the `<map>.png` byte layer) were not read** (§12). So on the start maps material
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
| `CM_UNWRAP_ITEM`, `CM_TUNE`, `CM_TUNE_RESULT`, `CM_ITEM_REMODEL`, `CM_ITEM_PURIFICATION`, `CM_CHARGE_ITEM`, `CM_COMPOSITE_STONES`, `CM_SELECT_DECOMPOSABLE`, `CM_APPEARANCE` | 558 | | P5-15/16 | M5c |
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
| E-8 | `CM_EQUIP_ITEM` | `canChangeEquip`, **`notifyEquipAction`**, **`updateItemAfterEquip`**, **`onItemUnequipment`** — four chunks | closed; stigma items reach `StigmaService`'s other 11 bodies (throw, M5c) |
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
| **D2** | **Scope: a solo player loots, keeps, moves, splits, destroys, equips and uses items; kinah; potions; godstone socketing and procs; camp fires.** Out: team loot (M5g), manastones/enchant/amplify/tempering/stigma/tune/remodel/purify/charge/dye/pack/decompose (M5c), quest-start and read items (M5d), npc warehouses and cube expansion (M5c), mail (M5c). | The roadmap row: "loot a corpse; use and move items". Everything out of scope is reached only through an npc dialog, a team, a quest, or an item action other than `skilluse`. |
| **D3** | **The M5b-3 gate forces drops with `gameserver.rates.drop = 1000000`** (rev 2; rev 1 said 10000, which left the 0.01 % rules at exactly 100.0f, §2.4) and asserts the drop set **by rule**: the entry count and indexes exactly, a bijection between entries and **applicable rules** (restrictions pass *and* the candidate set is non-empty after the race and level filter, §2.4), each item as a member of its rule's candidate set, counts as ranges, all computed by the oracle. **Event rules are out** because every scenario profile carries `gameserver.event.service.disabled_events = *` (`ScenarioServers.cpp:31`); the oracle states that assumption. | A Java configuration lever (§2.4), not a C++ deviation, so no `docs/deviations` row. At the default rate the gate would see an empty 210663 corpse 41.8 % of the time. |
| **D4** | **The M5b and M5b-2 gates (and their geo variants) set `gameserver.rates.drop = 0`, and their R3/Q2/allow-list rows are rewritten — in stage 1, in the same integrator commit as L-01** (rev 2; rev 1 put this in stage 2). R3 moves from "the partial was hit once" to "`SM_LOOT_STATUS(LOOT_ENABLE)` arrived once, for the corpse, with `lootEffectId` 0"; Q2's `DropNpc` row becomes created = kills, live 0; the §A row `DropRegistrationService.cpp:43` leaves both allow-lists. The stress nightly needs no key: its clients do not fight (`tests/scenario/stress/`), and npcs killing each other register no drop (`doReward` requires a Player winner, NpcController.java:225-245). | Finding 1. With 0, `registerDrop` still runs completely (the `DropNpc`, the `LOOT_ENABLE`, the free-for-all task) but the drop set is empty, so `scheduleDecayTask` keeps `IMMEDIATE_DECAY` and R4, Q2, Q3 keep their meaning, and the corpse's despawn unregisters the `DropNpc` 2 s after the kill (`NpcController.cpp:159`). Merging L-01 without these turns R3, Q1 and Q2 red on **every** run, not by chance (§2.3's table). The M5b-1 `soulsickness.disable` key is the precedent (m5b-plan.md D1). |
| **D5** | **`DropNpc` leaves `CheckOutput::zeroLiveClasses()` for a bounded row ("corpses with an unlooted drop at the stop"), and `DropItem` joins the summary rows** — in stage 1 (G-06, gate-harness lane under a P5-14 lease). The gates assert the exact number their script leaves (0). | `DropRegistrationService` is an `Immortal` holding `Ref<DropNpc>` until the corpse despawns (`DropRegistrationService.h:30-31`), and a corpse with a drop lives 300 s: a server stopped within 5 minutes of a kill holds one **in Java too**. That is a bound, not a leak — the same argument that moved `KnownObject` (`CheckOutput.cpp:190-194`). **Not** a merge-together constraint for L-01 (under D4's rate 0 a `DropNpc` lives 2 s, so the zero row stays green in the earlier gates), but required before the M5b-3 gate and before the user's first real-client session at the default rate, where any unlooted kill in the last 5 minutes would print a false `liveLeak`. Under D1 it waits for M5b-2's stage-3 census lane (m5b2-plan.md G-07) to release P5-14. |
| **D6** | **The item-action API is declared for all 32 bound action classes; only `SkillUseAction` gets bodies; every other action stays `AION_UNPORTED` and throws.** Signature (hub-headers.md §7.4: varargs → `std::initializer_list`, default `{}`, overrides repeat it): `virtual bool canAct(player::Player& player, Item& parentItem, runtime::Ptr<Item> targetItem, std::initializer_list<std::any> params = {}) const = 0;` and the same for `void act(...)`. | Java declares them abstract (AbstractItemAction.java:26, 34); `shells-3` is the precedent for declaring a behaviour API across every concrete shell with stubs. m5b2-plan.md D6's argument for throwing instead of a blanket partial holds: a silently ignored item use is a lost item or a missing buff. **This is the milestone's largest invisible item: 62 declarations that stay unported** (§8 risk 3). |
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
| **T-05** | `ItemSocketService::socketGodstone` with its `ItemUseObserver` and 2 s `ITEM_USE` task (cycles rows `cycles.toml:272-273` are the specification). | ItemSocketService.java:153-207 | T-02 | R | S |
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
| O-01 | Team loot: `DropDistributionService` (4), `LootGroupRules` (7, P5-10), `TemporaryTradeTimeTask` (no file), `PlayerTeamDistributionService`, `CM_GROUP_LOOT`, `CM_CLIENT_COMMAND_ROLL`, `CM_DISTRIBUTION_SETTINGS`, `CM_GROUP_DISTRIBUTION`, L-03 if not taken | M5g |
| O-02 | Manastones, enchant, amplify, tempering, stigma: `EnchantService` (11), `EnchantItemAction`, `ItemSocketService` manastone bodies (7), `StigmaService` (11 more), `CM_COMPOSITE_STONES` + `CompositionAction` (no file) | M5c |
| O-03 | The other 31 item actions (62 stubs + ~70 undeclared private and inner bodies): QuestStart/Read → M5d; Ride, ToyPetSpawn, AdoptPet → pets; the rest → M5c | per action |
| O-04 | Npc warehouse and cube expansion (`WarehouseService` 5, `CubeExpandService` 7), `ItemActionService` (identify/tune), `CM_TUNE`, `CM_TUNE_RESULT`, `CM_ITEM_REMODEL`, `CM_ITEM_PURIFICATION`, `CM_CHARGE_ITEM`, `CM_SELECT_DECOMPOSABLE`, `CM_UNWRAP_ITEM`, `CM_APPEARANCE` | M5c |
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
| **m5b3-h01** `model/templates/item/actions/AbstractItemAction.h` (P5-07): `virtual bool canAct(player::Player&, gameobjects::Item& parentItem, runtime::Ptr<gameobjects::Item> targetItem, std::initializer_list<std::any> params = {}) const = 0;` and `virtual void act(… same …) const = 0;` (D6); **`override` declarations with `AION_UNPORTED` stubs in the 32 bound action classes** (30 new `.cpp` files; `DecomposeAction.cpp` and `EmotionLearnAction.cpp` exist); **and in `EnchantItemAction.h` the non-virtual overload `void act(player::Player&, gameobjects::Item& parentItem, gameobjects::Item& targetItem, runtime::Ptr<gameobjects::Item> supplementItem, int32_t targetWeapon) const;` with `using AbstractItemAction::act;`, stubbed `AION_UNPORTED`** (rev 2, D8) | **layout** (new pure virtuals on a base every action shell derives from; the precedent is `shells-3`) | T-04, P-05; `CM_MANASTONE` constructs `EnchantItemAction` directly and calls `canAct` and the five-argument `act` (CM_MANASTONE.java:79-86, EnchantItemAction.java:86) |
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
    is small and reported by the gate, not hidden. And 8267 has **two** templates; an oracle that keeps the first would predict WIND.
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
- Skill 8267's two templates (skill_templates.xml:80570, :80575), `SkillData`'s last-wins map, and the MAGICAL resist path (EffectTemplate.java:314).
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
  :127, `Equipment$1..$2` :141-144, `ItemSocketService$1` :272-273, the action observers' captures from :208); `fieldmap.json`: 51 callbacks
  of the drop, item, action and stigma classes.

**Inferred, to confirm before relying on it:**

- **That rate 1000000 makes every rule certain in the running server.** It follows from Rates.java:166-173, DropModifiers.java:53-57,
  DropRegistrationService.java:189 and Rnd.java:32-34, with a 100× margin over the smallest rule (rev 2); nobody ran it. (Rev 1's membership
  caveat is moot: a one-value list is index 0 for every membership, Rates.java:171-172.)
- **That no Poeta terrain material carries a skill.** Only mesh placements were parsed; the `<map>.png` byte layer was not.
- **That seeding an `inventory` row at a high object id mid-run is safe** (the `IDFactory` cursor comment, `IDFactory.h:26-32`; not tested).
- **That the M5b-2 gate kills 210663 and 210133 and so needs D4's key** — from m5b2-plan.md §10.2 C5-C12; the gate file does not exist yet.
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
   (`MaterialZoneHandler.cpp:47-69`), and the task skips a player under spawn protection (`AbstractMaterialSkillActor.cpp:70`). **Still open:**
   which reported coordinates pass the TOUCH test on the fire's mesh (G-04 measures it).
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
