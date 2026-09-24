# M5c work plan (vendors and economy)

> **Status:** plan **rev 2**, 2026-09-23, revised after an adversarial review (§14 lists what the review found and what changed). Rev 1 was
> a **read-only** analysis over HEAD `c1edb0afb` ("M5b-2 stage 1 part 2: the cast engine and the effect core") plus the uncommitted M5b-2
> stage 1 part 3 lanes in the working tree (P5-01, P5-03, P5-04 — none of them a chunk this plan touches), against the Java 4.8 tree; rev 2
> re-measured over the same tree and read the M5b-3 plan, which was written in parallel (`m5b3-plan.md`). **Nothing was compiled, built or run
> for this plan.** Every C++ statement comes from reading the two trees, from `game-server/chunks.cmake`, from counting `AION_UNPORTED(` /
> `AION_PARTIAL(` call sites and from throw-away scripts that parse the Java sources with `tools/gen/javasrc.py` and compare their methods with
> the C++ declarations. Every number names the file it came from, and §12 lists which claims were **measured** and which were **inferred**.
>
> It follows the shape of [m5b-plan.md](m5b-plan.md) and [m5b2-plan.md](m5b2-plan.md): the paths end to end with file:line, numbered work
> items with owners and dependencies, lanes, a gate specification, risks and an honest split. Ownership follows `tools/porting/chunks.py`;
> header changes follow [hub-headers.md](hub-headers.md) §14 through `docs/porting/header-requests.md`.
>
> **M5c starts after M5b-3 (loot and items).** Rev 1 was written before the M5b-3 plan existed; rev 2 checked every assumption against
> `m5b3-plan.md` (§0 names the M5b-3 item that delivers each one), took over the part of the item services that plan sends to M5c which a
start-map player reaches, and gave every other part a named milestone (§3a). Every
> work item, case and checklist step that stands on an assumption carries its id (`A-01` … `A-13`), so the plan can be re-verified in one pass
> when M5c branches.

---

## 0. What this plan assumes M5b-3 delivers

The roadmap gives M5b-3 "loot a corpse; use and move items" over P5-09 (drop), P5-07 and P5-13 (rest of restrictions). The economy is built
on the same item machinery, and **every inventory change in this milestone goes through two P5-07 classes that are unported today**:
`Storage` itself is ported (`model/items/storage/Storage.cpp`, 0 `AION_UNPORTED`), but every mutation with an actor sends its packet through
`ItemPacketService` (`Storage.cpp:135, 160, 186, 210, 229`; Java Storage.java:121, 147, 180, 213, 240), and every item a player receives is
created by `ItemService::addItem`.

| Id | Assumed delivered by M5b-3 | Where it is today | What `m5b3-plan.md` says (rev 2 check) | Why M5c needs it | If M5b-3 did not deliver it |
|---|---|---|---|---|---|
| **A-01** | `ItemPacketService` — all 9 bodies | `services/item/ItemPacketService.cpp:7-39`, 9 `AION_UNPORTED` | **in scope**: T-02 (+ the enum companion) | every kinah change (`Storage::increaseKinah`, `tryDecreaseKinah`), every stack change, every add and delete in buy, sell, exchange, mail, store, craft and the stage-0 item services | **M5c cannot start.** Nothing in §2.2-§2.6 completes without it |
| **A-02** | `ItemService::addItem` (7 overloads), `addNonStackableItem`, `addStackableItem`, `copyItemInfo`, `ItemUpdatePredicate::getUpdateType` | `services/item/ItemService.cpp:26-77`, 11 `AION_UNPORTED` | **in scope**: T-01 | buying (TradeService.java:151), buy-back (RepurchaseService.java:61), private store (PrivateStoreService.java:166), crafting (CraftService.java:72), gathering (`GatheringTask.cpp:138`), extraction (EnchantService.java:74) | M5c cannot start |
| **A-03** | The item-action dispatch: `CM_USE_ITEM`, and the **`canAct`/`act` virtuals on `AbstractItemAction`** | `AbstractItemAction.h` declares **no virtual at all** (Java AbstractItemAction.java:26, 34 are abstract); `CM_USE_ITEM` has no C++ file | **in scope**: I-01 header batch `m5b3-h01` declares the API on all 32 bound action classes with **stub `.cpp` files that throw** (m5b3 D6); only `SkillUseAction` gets bodies (T-04); `CM_USE_ITEM` is P-05. **`ItemActionService` is not in it** (m5b3 §3 moves it to M5c: A-13) | using a bought potion; `CraftLearnAction` (C-05); the stage-0 actions (`EnchantItemAction`, `ExtractAction`, `DecomposeAction`, `RemodelAction`) fill stubs M5b-3 declared | the checklist says "do not drink"; C-05 and E-02..E-04 need a header request of their own |
| **A-04** | `SkillUseAction` plus the two potion effect classes `ProcHealInstantEffect` and `ProcMPHealInstantEffect` | 4 `AION_UNPORTED` each (`skillengine/effect/Proc*HealInstantEffect.cpp`); neither is in m5b2-plan.md §2.4's 34 classes | **in scope**: T-04, E-01 (m5b3 §2.5 "U6") | the Poeta potions: skill 9889 and 10202 are `prochealinstant` + `heal`, 9894 and 10207 are `procmphealinstant` + `mpheal` (`skill_templates.xml`) | buying works, **drinking** throws — checklist step 4 changes |
| **A-05** | `CM_EQUIP_ITEM` and `PlayerRestrictions::canChangeEquip` | no C++ file / `restrictions/PlayerRestrictions.cpp:204` | **in scope**: P-01, P-05 (m5b3 §2.2 Q1-Q3) | wearing the armour bought from Mune; equipping the identified item of C15 | the checklist drops "equip it" |
| **A-06** | `CM_MOVE_ITEM`, `CM_SPLIT_ITEM`, `CM_DELETE_ITEM`, `ItemMoveService`, `ItemSplitService` | no C++ files / 3 + 4 `AION_UNPORTED` | **in scope**: T-03, P-05 | nothing on the gate's path; a real client drags items all the time (m5a-client-session.md F-2 lists `CM_MOVE_ITEM`) | real-client noise only |
| **A-07** | Loot: `CM_START_LOOT`, `CM_LOOT_ITEM`, `DropService`, `DropRegistrationService::registerDrop` (the M5b-1 partial, `m5b_partial_allowlist.txt` row `DropRegistrationService.cpp:43`) | 13 + 26 `AION_UNPORTED`, 1 partial | **in scope**: L-01, L-02 (solo path) | "sell loot" in the real-client checklist | the checklist sells starter items only; the gate never needs loot (D5) |
| **A-08** | `TemporaryTradeTimeTask` (no C++ file; DropService.java:519 registers looted items, ExchangeService.java:108 asks it) | **no `.h`, no `.cpp`**; `fieldmap.json` already has its row | **not in scope, and assigned to M5g** (m5b3 D9, O-01) — **a conflict**: the exchange needs it first | ExchangeService::addItem compiles only against it | **item P-04 creates it in M5c (settled).** The M5b-3 plan's O-01 row must drop it (§3a) |
| **A-09** | `PlayerRestrictions::canTrade` ("the rest of restrictions") | `restrictions/PlayerRestrictions.cpp:192`, `AION_UNPORTED` | **optional** (m5b3 P-01 "O") | every trade body calls it first (TradeService.java:81, 188, 257, 289; ExchangeService.java:55; RepurchaseService.java:48) | item P-01 closes it (drop P-01 if M5b-3 took the O) |
| **A-10** | `CM_QUESTION_RESPONSE` | no C++ file | **not in scope** — although m5b3 P-04 ports the soul-bind accept handler (`Equipment$1`), which a real client can then not answer | accepting an exchange, soul healing, learning a craft, the cube expander's question | item D-04 ports it. **Recommendation to the integrator:** move D-04 into M5b-3's player-side lane so its soul-bind handler is reachable; M5c then drops D-04 |
| **A-11** | Scenario decoders for `SM_INVENTORY_ADD_ITEM`, `SM_INVENTORY_UPDATE_ITEM`, `SM_DELETE_ITEM`, `SM_CUBE_UPDATE`, `SM_ITEM_USAGE_ANIMATION` (from Java `writeImpl`), and an oracle command for item-template constants | not in `tests/scenario/decoders/` (measured: `PacketDecoders.h:487-582`, `CombatDecoders.h`, `SkillDecoders.h`) | **in scope**: G-01 (`m5b3-item`), G-02 (`ItemDecoders`) | every ledger assertion of §10 reads kinah and stack counts from them | item G-02 writes them |
| **A-12** | `gs.scenario.m5b3` and `gs.scenario.m5b3_geo` green, with `m5a`, `m5a_geo`, `m5b`, `m5b_geo`, `m5b2`, `m5b2_geo` re-greened | – | **in scope**: m5b3 D12, G-03..G-05 | §10's gate re-runs all of them | G-04 grows |
| **A-13** | **Item identification**: `CM_TUNE` (arm "not identified"), `ItemActionService::identifyItem` (+ its `ItemUseObserver` and task), and `TuningAction::getRandomStatBonusIdFor`, which `identifyItem` calls (ItemActionService.java:23-53, the call at :45) | `CM_TUNE` no C++ file; `ItemActionService.cpp` 2 `AION_UNPORTED`; `TuningAction` a shell | **not in scope** (m5b3 §3 row O-06, O-04 → M5c) — but **reachable from M5b-3's own loot**: all 51 Plainsman's weapons and armour pieces that Poeta monsters drop carry `option_slot_bonus="1"` and no `rnd_count` (`item_templates.xml`), so `ItemTemplate` leaves `maxTuneCount` at −1 (ItemTemplate.java:155-160), the item is created **unidentified** (Item.java:84-85), and `Equipment.equip` refuses it with a warning (Equipment.java:163-167) | a looted weapon cannot be equipped before it is identified; C15 | item **P-07** ports it in stage 1. **Recommendation to the integrator:** M5b-3 should take it (its checklist equips loot); then P-07 drops |

**Re-verification at branch time:** for each row, grep the named file for `AION_UNPORTED(` and check the manifest owner. A-01 and A-02 are
blocking; A-03 to A-05 change the real-client checklist and the stage-0 header requests only; A-08 to A-11 and A-13 move an item between the
two milestones. m5b2-plan.md O-02 assigned `skillengine/task/CraftingTask` to M5b-3 ("with the item path"); **the M5b-3 plan did not take it**
(measured: no row names it), so item C-02 stays. The M5b-3 plan also does not take `CM_GATHER` (D10 stays) and does not split P5-09 (D1 stays).

---

## 1. Summary

**The economy's frame is ported and its engine is not.** M5a had to port the enter-world and logout halves of every economy service, so the
database, the models and every server packet already exist:

| Already ported, 0 `AION_UNPORTED` | Evidence |
|---|---|
| **Every DAO the milestone touches** | `MailDAO` (10 bodies: `storeLetter` :120, `deleteLetter` :181, `updateOfflineMailCounter` :188, `loadPlayerMailbox` :88, `storeMailbox` :110), `InventoryDAO` (21, incl. `store(Item, ownerId)`), `ItemStoneListDAO` (12), `PlayerRecipesDAO` (3), `CraftCooldownsDAO` (3), `BlockListDAO` (4), `PlayerDAO::loadPlayerCommonDataByName` :141 — `dao/*.cpp`, measured over all 56 DAOs: 2 unported sites in the whole DAO tree, both in `CustomInstancePlayerModelEntryDAO` |
| **Every server packet the milestone sends** | the 41 packets of §2.8 — `SM_DIALOG_WINDOW`, `SM_TRADELIST`, `SM_SELL_ITEM`, `SM_REPURCHASE`, `SM_QUESTION_WINDOW`, `SM_EXCHANGE_*` (4), `SM_MAIL_SERVICE`, `SM_PRIVATE_STORE(_NAME)`, `SM_CRAFT_UPDATE`, `SM_CRAFT_ANIMATION`, `SM_LEARN_RECIPE`, `SM_RECIPE_DELETE`, `SM_INVENTORY_*`, `SM_GATHERABLE_INFO`, … — 0 `AION_UNPORTED` in each. **But two of them construct through unported model bodies** (§2.9 W-03) |
| The models | `Letter`, `Mailbox`, `PrivateStore`, `RecipeList`, `RequestResponseHandler`, `ResponseRequester`, `PlayerSettings`, `Exchange`, `ExchangeItem`, `TradeList`, `TradeItem`, `TradePSItem`, `RepurchaseList`, `Storage`, `PlayerStorage`, `ItemStorage`, `LimitedItem`, `PlayerSkillList`, `PlayerSkillEntry`, `BrokerItem` — 0 unported each |
| The enter-world / logout hooks of the services | `MailService::onPlayerLogin` (`MailService.cpp:41`), `BrokerService::onPlayerLogin`, `removePlayerCache`, `ExchangeService::cancelExchange` + `returnItems` + `cleanUpExchanges`, `RepurchaseService` except `repurchaseFromShop`, `LimitedItemTradeService` (all 5), `PricesService` (all 9), `RelinquishCraftStatus` (all 9) — called from `PlayerEnterWorldService.cpp:546, 556` and `PlayerLeaveWorldService.cpp:107-109` today |
| The dialog frame | `NpcController::onDialogRequest` / `onDialogSelect` (`NpcController.cpp:284-305`, Java NpcController.java:249-272), `GeneralNpcAI::handleDialogStart` → `TalkEventHandler::onTalk` (`TalkEventHandler.cpp:28-58`), `model::getStartPageId` (`DialogPageInfo.cpp:17-29`), `PlayerController::onDialogSelect` (the private-store BUY arm, `PlayerController.cpp:648-652`), `QuestEngine::onDialog` (`QuestEngine.cpp:158-190`) |
| Gathering, except its client packet | `GatherableController` (9 bodies), `GatheringTask` (11), `AbstractInteractionTask` (6), `AbstractCraftTask` (2) — `skillengine/task/*.cpp`, `controllers/GatherableController.cpp` |

**What is empty is the service bodies, the client packets, three classes that have no C++ file and a handful of methods no header declares:**

| # | Hole | Size (measured) |
|---|---|---|
| 1 | **Client packets.** 23 of the milestone's packets have no C++ file (of 188 Java `CM_*`, 42 have a `.cpp` in `network/aion/clientpackets/`). The opcodes are all registered already (`ClientPacketInfo.gen.inc:35-127`), so each is an `.h` + `.cpp` + byte-vector test | 23 files, 46 bodies, 1,169 Java lines |
| 2 | **Service bodies.** `DialogService` 7 (P5-08), `TradeService` 8, `ExchangeService` 11, `PrivateStoreService` 8, `MailService` 7, `SystemMailService` 2, `RecipeService` 3, `CraftService` 5, `CraftSkillUpdateService` 4 (P5-09); **from the M5b-3 hand-off (§3a)** `EnchantService` 11, `ItemSocketService` 7 (the manastone bodies), `CubeExpandService` 7, `ItemActionService` 2 (P5-07); and six one- or two-body prerequisites in P4-11a, P5-00, P5-07, P5-08, P5-13 | **90** `AION_UNPORTED` sites |
| 3 | **The invisible work** (lesson 1). No `AION_UNPORTED` count sees it: `CraftingTask` (P5-02a, no C++ file, 9 bodies), `TemporaryTradeTimeTask` (P5-07, no C++ file, 4 — A-08), `PostboxAI` (P5-05, no C++ file, 2 — **the Poeta mailbox is dead without it**), six `Profession` methods `ProfessionInfo.h` never declared, `StatEnum.getModifier`, `CraftLearnAction.canAct`/`act`, 5 methods of anonymous `RequestResponseHandler`/`ItemUpdatePredicate` subclasses; **from the hand-off** the item actions `EnchantItemAction` 9, `DecomposeAction` 7, `TuningAction` 4, `ExtractAction` 3, `RemodelAction` 2, `ExpandInventoryAction` 2, and 3 anonymous bodies (`ItemActionService`'s observer and task, `CubeExpandService`'s handler). M5b-3's `m5b3-h01` turns the `canAct`/`act` pairs into declared stubs; the private helpers stay undeclared | **59** bodies |
| 4 | **The database.** Every DAO is ported, but **seven write paths have never run in any gate**: `MailDAO.storeLetter` / `deleteLetter` / `updateOfflineMailCounter`, `InventoryDAO.store(Item, ownerId)` (the owner change of a mailed item), `PlayerRecipesDAO.addRecipe` / `delRecipe`, `ItemStoneListDAO.storeManaStones` (socketing and removal, ItemStoneListDAO.java:134; ItemSocketService.java:129), and the `npc_expands` column of `PlayerDAO.storePlayer` (PlayerDAO.java:50, 64). This is verification work in the gate (D11), not porting | 0 bodies, 7 first uses |

**Total: ~195 bodies (90 sites + 59 invisible + 46 packet bodies), ~4,750 Java lines of bodies.** Rev 1 said ~132 and ~3,100: the
difference is the M5b-3 hand-off this plan now owns (§3a) — the manastone, enchantment, extraction, bundle, identification and cube-expansion
work a start-map player reaches, ~63 bodies and ~1,650 Java lines. The rest of what `m5b3-plan.md` O-02..O-04 sent to M5c, plus two P5-07
services no plan had named (`UpgradeArcadeService`, `HouseObjectFactory`), ~170 bodies, is **given another home** in §3a, each with its
reason. **The size is not the risk; the shape is**: M5c is the first milestone whose every
feature moves items and kinah between two storages, often between two players, and writes the result to the database on the packet thread.
Its failure modes are item loss and item duplication, not a wrong number.

**Five findings shape the plan.**

1. **Dialog belongs to M5c, not M5e** (§3). Nothing in the economy is reachable without `CM_SHOW_DIALOG`, `CM_DIALOG_SELECT` and
   `DialogService`, and the roadmap lists "dialog" under M5e's P5-08. Porting it wakes every talkable npc on the start maps (§2.9), which is
   why it is the first stage (stage 0), with a green point before a single trade; the stage-0 item services run beside it in P5-07.
2. **Two "fully ported" server packets throw on construction**: `SM_TRADELIST` and `SM_SELL_ITEM` call `Npc::canSell` / `canPurchase`
   (`SM_TRADELIST.cpp:27`, `SM_SELL_ITEM.cpp:19-20`), which are `AION_UNPORTED` (`model/gameobjects/Npc.cpp:354, 366`). A packet inventory
   that counts `AION_UNPORTED` in `serverpackets/` says "0" and is wrong — the same trap as lesson 1, one layer down.
3. **Crafting cannot be reached from the start maps, and level 10 means a Daeva.** All 28 npcs with `COMBINE_SKILL_LEVELUP` (dialog
   action 46) spawn in Sanctum, Pandaemonium, Oriel, Pernon or 900020000; all static crafting stations spawn in those four maps
   (`spawns/Statics/`, 4 files); learning a craft needs character level 10 (CraftSkillUpdateService.java:83-84); and the cheapest autolearn
   recipe needs a gathered material (Inina, gatherable only in Verteron, `spawns/Gather/210030000_Verteron.xml`). **A character of a starting
   class never reaches level 10**: `PlayerCommonData.setExp` caps a non-Daeva at level 9 online (PlayerCommonData.java:276-281, ported
   faithfully at `PlayerCommonData.cpp:187-205`), and at load it gives level 10 only above the level-10 threshold or to a Daeva (the
   ascension quest 1006/2008 COMPLETE and an advanced class, PlayerCommonData.java:588-610). Rev 1's seed (`exp = 126,069` on a Warrior) gives
   **level 9**, and Hestia would silently refuse. The gate therefore seeds a **Daeva** (D5), and that seed is also what wakes W-06 and W-20.
   The real client needs the same seed by SQL until M5d/M5e/M5f (§11).
4. **The broker is the one piece that does not fit** (D2). It is the largest single service (707 Java lines, 11 sites, 9 client packets,
   363 Java lines of packets), and its npc (`OPEN_VENDOR = 33`) spawns in **20 maps, none of them a starting map**: the two capitals, Oriel,
   Pernon, Reshanta, and two per field map from Verteron and Altgard on (§2.7). No new character reaches one before travel (M5f), and a
   market needs two players and the settlement timer. The plan recommends a **capital-economy milestone after M5f** holding the broker,
   express mail, trade-in, AP vendors, the warehouse and the capital-only item services of §3a. **This is the user's decision** (the roadmap
   row names the broker); stage 3 carries the broker as an optional lane if the user wants it now.
5. **M5b-3 hands M5c ~230 bodies of item services, and a start-map player reaches about a quarter of them** (§3a). Manastones drop on 15 %
   of Poeta kills (m5b3-plan.md §2.4), the manastone-removal npcs Seril (203336, 11.1 m from the plan's merchant) and Dobar (203692) stand on
   the start maps, the plan's own merchant 798007 sells **Extraction Tools** (165000001, `<extract/>`, goods list 132), and every Plainsman's
   item a Poeta monster drops is created unidentified (A-13). Those paths are ported here (stage 0 and stage 1); the capital-only and
   other-system items get an explicit home elsewhere.

---

## 2. The paths end to end

"ported" means the C++ body exists and contains no `AION_UNPORTED`; **U** means `AION_UNPORTED`; **P** means `AION_PARTIAL`; **no file** means
the Java class has no C++ counterpart. Chunk owners are from `chunks.py owner`.

### 2.1 Talking to an npc

| # | Step | Java | C++ today | Chunk |
|---|---|---|---|---|
| 1 | Client clicks an npc: `CM_SHOW_DIALOG(targetObjectId)` → stop spawn protection, `isTrading` bail-out, remove hide effects unless `can_talk_invisible`, `npc.getController().onDialogRequest(player)` | CM_SHOW_DIALOG.java:23-42 | **no file** | **P5-16** |
| 2 | `NpcController.onDialogRequest`: `canInteract`, `isInTalkRange` (talk distance + 1, PositionUtil.java:306-309) else `STR_DIALOG_TOO_FAR_TO_TALK`, then `DIALOG_START` to the AI | NpcController.java:249-262 | ported (`NpcController.cpp:284-297`) | P4-11b |
| 3 | `GeneralNpcAI.handleDialogStart` → `TalkEventHandler.onTalk`: `QuestEngine.onDialog(USE_OBJECT)`, then `SM_DIALOG_WINDOW(npc, DialogPage.getStartPageId(npc, player))` | GeneralNpcAI.java:50; TalkEventHandler.java:22-46 | ported (`GeneralNpcAI.cpp:42-44`, `TalkEventHandler.cpp:28-58`); `QuestEngine::onDialog` ported, no quest registered (`QuestEngine.cpp:111` **P**, M5a §A row) | P5-05, P5-06 |
| 4 | `DialogPage.getStartPageId` → **`DialogService.isInteractionAllowed`** → `isSummonOwner`, `isSubDialogRestricted`; page 10 for a function npc | DialogPage.java:113-125; DialogService.java:298-377 | `DialogPageInfo.cpp:21` calls it; **U** (`DialogService.cpp:27, 31, 35`) | **P5-08** |
| 4b | The **mailbox** npc 700000 has `ai="postbox"` (`npc_templates.xml:439517`). `PostboxAI.handleDialogStart`: `mailBoxState = REGULAR`, `SM_DIALOG_WINDOW(page 18 = MAIL)` | data/handlers/ai/PostboxAI.java:22-26 | **no file**: `AIEngine::newAI` substitutes a `DummyNpcAI` whose hooks are empty (`AIEngine.cpp:158-168`, `gameserver.dev.missing_ai_handlers = warn`), so the click does **nothing, silently** | **P5-05** |
| 5 | Client picks a function: `CM_DIALOG_SELECT(target, dialogActionId, extendedRewardIndex, lastPage, questId, unk)`: unknown-action warning, `isFunctionDialog && !supportsAction` audit, `isInteractionAllowed` audit, `controller.onDialogSelect` | CM_DIALOG_SELECT.java:47-124 | **no file** | **P5-15** |
| 6 | `NpcController.onDialogSelect` → `ai.onDialogSelect` (false for `general`, AbstractAI.java:385-387) → **`DialogService.onDialogSelect`**: the 212-line switch (BUY → `SM_TRADELIST`; SELL/`TRADE_SELL_LIST` → `SM_SELL_ITEM`; `BUY_AGAIN` → `SM_REPURCHASE`; `RECOVERY` → a question; `COMBINE_SKILL_LEVELUP` → `CraftSkillUpdateService.learnSkill`; …; default → `handleQuestDialogueOrSendNextPage`) | NpcController.java:265-272; DialogService.java:69-291 | `NpcController.cpp:299-305` ported; **U** (`DialogService.cpp:15, 19, 23`) | **P5-08** |
| 7 | `SM_TRADELIST` constructor: tabs filtered by legion level, **`npc.canSell()`**, `npc.canBuy()`; `SM_SELL_ITEM`: **`canSell`, `canBuy() \|\| canPurchase()`** | SM_TRADELIST.java:33-55; SM_SELL_ITEM.java:27-35; Npc.java:361-386 | packets ported, but **`Npc::canSell`, `canTradeIn`, `canPurchase` are U** (`Npc.cpp:354, 362, 366`; `canBuy` at :358 is ported and calls `canSell` for an npc without SELL) | **P4-11a** |
| 8 | Player target (private store): `PlayerController.onDialogSelect(BUY)` → `SM_PRIVATE_STORE` | PlayerController.java:553-556 | ported (`PlayerController.cpp:648-652`) | P4-11b |
| 9 | `CM_CLOSE_DIALOG` → `DialogService.onCloseDialog`: `DIALOG_FINISH`, legion-warehouse release, mailbox `CLOSED`; `SM_LOOKATOBJECT` | CM_CLOSE_DIALOG.java:24-36; DialogService.java:53-67 | **no file**; **U** (`DialogService.cpp:10`) | P5-15, P5-08 |
| 10 | Answering a question: `CM_QUESTION_RESPONSE(questionId, response, …, senderId)`: cancel an exchange on "yes", `ResponseRequester.respond` | CM_QUESTION_RESPONSE.java:27-45 | **no file**; `ResponseRequester::respond` / `denyAll` ported (`ResponseRequester.cpp:19, 28`) | **P5-16** (A-10) |

### 2.2 Buying and selling at a merchant

| # | Step | Java | C++ today | Chunk |
|---|---|---|---|---|
| 1 | `CM_BUY_ITEM(sellerObjId, tradeActionId, amount ≤ 36, [itemId\|index\|objId, count ≤ 20000]…)`: 0 private store, 1 sell, 2 buy back, 13-16 buy, 17 sell to a pet; `isInteractionAllowed` for an npc | CM_BUY_ITEM.java:47-143 | **no file** | **P5-15** |
| 2 | Buy: `performBuyFromShop` → `performBuyTransaction`: `canTrade`, `validateBuyItems` (the npc's goods lists), `calculateBuyListPrice` (`PricesService.getBuyPrice` × count × `sell_price_rate` / 100), `calculateAbyssRewardBuyList`, free slots, limited items, `tryDecreaseKinah`, `ItemService.addItem(BUY, INC_ITEM_BUY)` | TradeService.java:62-181; TradeList.java:48-107 | **U** (`TradeService.cpp:10-26`); `TradeList` ported; `canTrade` **U** (A-09); `addItem` **U** (A-02) | **P5-09** |
| 3 | Sell: `performSellToShop`: `canTrade`, `isSellable` else `STR_BUY_SELL_ITEM_CAN_NOT_BE_SELLED_TO_NPC`, `getSellReward(price, 20)`, **`PlayerLimitService.updateSellLimit`**, `delete(SELL)` or `decreaseItemCount` + a new repurchase item, `RepurchaseService.addRepurchaseItems` (replaces the player's set), `increaseKinah(INC_KINAH_SELL)` | TradeService.java:183-249 | **U** (`TradeService.cpp:27-36`); `updateSellLimit` **U** (`PlayerLimitService.cpp:15`) — the whole body, so it throws **before** its `LIMITS_ENABLED` early return (PlayerLimitService.java:23) | P5-09, **P5-08** |
| 4 | Buy back: `CM_DIALOG_SELECT(BUY_AGAIN = 70)` → `SM_REPURCHASE`; `CM_BUY_ITEM(2)` → `RepurchaseService.repurchaseFromShop`: `tryDecreaseKinah(repurchasePrice)`, `ItemService.addItem(player, item)` | DialogService.java:232-234; RepurchaseService.java:47-69 | `SM_REPURCHASE` ported; **U** (`RepurchaseService.cpp:41`) | **P5-07** |
| 5 | AP / abyss / reward vendors: `AbyssPointsService.addAp` | TradeService.java:133-134, 251-286 | **U** (`AbyssPointsService.cpp:11-23`); reached only when `requiredAp > 0` or for an `ABYSS` purchase template — **no such vendor on the start maps** (§2.10) | P5-08 — **W** |
| 6 | Trade-in: `CM_BUY_TRADE_IN_TRADE` → `performBuyFromTradeInTrade` | TradeService.java:288-387 | **no file**; U | P5-15 — deferred (D2) |

### 2.3 Player-to-player exchange

| # | Step | Java | C++ today | Chunk |
|---|---|---|---|---|
| 1 | `CM_EXCHANGE_REQUEST(target)`: range 5, hide, same race, `DeniedStatus.TRADE`, then an **anonymous `RequestResponseHandler`** (`acceptRequest` → `registerExchange`, `denyRequest` → `STR_EXCHANGE_HE_REJECTED_EXCHANGE`) put on the target with `SM_QUESTION_WINDOW(90001)` | CM_EXCHANGE_REQUEST.java:34-99 | **no file**; the handler is a callback struct to write (hub-headers.md §7.3; the pattern exists in `AIActions.cpp:34-60`) | **P5-15** |
| 2 | Target answers → `CM_QUESTION_RESPONSE` (§2.1 row 10) → `registerExchange`: `canTrade` both, two `Exchange` objects in `exchanges`, `SM_EXCHANGE_REQUEST` to both | ExchangeService.java:43-56 | **U** (`ExchangeService.cpp:33, 37`) | **P5-09** |
| 3 | `CM_EXCHANGE_ADD_ITEM(objId, count)` / `CM_EXCHANGE_ADD_KINAH(count)`: tradeable check (`isTradeable` \|\| **`TemporaryTradeTimeTask.canTrade`** \|\| legion-tradeable), `AdminService.canOperate`, split stacks via `ItemFactory.newItem`, `SM_DELETE_ITEM` / fake `SM_INVENTORY_UPDATE_ITEM`, `SM_EXCHANGE_ADD_ITEM(0/1)`, `SM_EXCHANGE_ADD_KINAH(0/1)` | ExchangeService.java:76-171 | **U** (`ExchangeService.cpp:59, 63`); `TemporaryTradeTimeTask` **no file** (A-08) | P5-09, P5-07 |
| 4 | `CM_EXCHANGE_LOCK` → `SM_EXCHANGE_CONFIRMATION(3)` to the partner; `CM_EXCHANGE_OK` → `confirm`, `(2)` to the partner, and **if the partner is confirmed**, `performTrade`: `validateExchange` (free slots), `removeItemsFromInventory` ×2 (incl. `tryDecreaseKinah`), `(0)` to both, `putItemToInventory` ×2 (`Storage.add(PLAYER_EXCHANGE_GET)`, `increaseKinah`), **`InventoryDAO.store` ×2 on the packet thread**, `cleanUpExchanges` | ExchangeService.java:173-346 | **U** (`ExchangeService.cpp:67, 108-143`) | P5-09 |
| 5 | `CM_EXCHANGE_CANCEL` / logout / a "yes" to another question → `cancelExchange` → `returnItems` (`SM_INVENTORY_ADD_ITEM(GET_BACK)`, `SM_CUBE_UPDATE`), `SM_EXCHANGE_CONFIRMATION(1)` to the partner | ExchangeService.java:182-214; PlayerLeaveWorldService.java:85 | **ported** (`ExchangeService.cpp:71-106`; called from `PlayerLeaveWorldService.cpp:108`) | P5-09 |

### 2.4 Mail

| # | Step | Java | C++ today | Chunk |
|---|---|---|---|---|
| 1 | Open the postbox: §2.1 row 4b → `SM_DIALOG_WINDOW(page 18)` whose last `writeH` is the mailbox state (SM_DIALOG_WINDOW.java:35-36); the client then asks `CM_CHECK_MAIL_LIST(expressOnly)` → `MailService.sendMailList` (split with `DynamicServerPacketBodySplitList`, ported in `utils/collections/`) | PostboxAI.java:22-26; CM_CHECK_MAIL_LIST.java:22-32; MailService.java:270-281 | PostboxAI **no file**; `CM_CHECK_MAIL_LIST` **no file**; **U** (`MailService.cpp:46`) | P5-05, P5-15, P5-09 |
| 2 | `CM_SEND_MAIL(recipient, title, message, itemObjId, itemCount, kinah, letterType)` → `sendMail`: length checks, **`PlayerService.getOrLoadPlayerCommonData(name)`**, `validateRecipient` (race, 100 letters, `BlockListDAO.load`), commission (`price × qualityRate × count × costFactor`, float) + `PricesService.getPriceForService`, the disposition arm for untradeable items, `remove` / `decreaseItemCount`, `setItemLocation(MAILBOX)`, `decreaseKinah`, a `Letter` with `IDFactory.nextId`, **`InventoryDAO.store(item, recipientId)`**, `ItemStoneListDAO.save`, **`MailDAO.storeLetter`**, `SM_MAIL_SERVICE(1)`, `SystemMailService.updateRecipientMailbox` | CM_SEND_MAIL.java:29-43; MailService.java:56-199 | **no file**; **U** (`MailService.cpp:15-28`); **`PlayerService::getOrLoadPlayerCommonData` ×2 U** (`PlayerService.cpp:323, 327`) — a hidden prerequisite | P5-16, P5-09, **P5-00** |
| 3 | `updateRecipientMailbox`: offline → `mailboxLetters + 1` + **`MailDAO.updateOfflineMailCounter`**; online → `putLetterToMailbox`, `SM_MAIL_SERVICE(0)`, a list refresh if the recipient has the mailbox open, `STR_POSTMAN_NOTIFY` for express | SystemMailService.java:116-138 | **U** (`SystemMailService.cpp:15`) | P5-09 |
| 4 | `CM_READ_MAIL` → `readMail` (`SM_MAIL_SERVICE(3)`, `setReadLetter`); `CM_GET_MAIL_ATTACHMENT(id, 0\|1)` → `getAttachments` (item: `Storage.add(MAIL)` + `ExpireTimerTask.registerExpirable`; kinah: `MailDAO.storeLetter` first, then `increaseKinah`); `CM_DELETE_MAIL(ids)` → `deleteMail` (**`MailDAO.deleteLetter`** each) | MailService.java:204-263 | all three packets **no file**; **U** (`MailService.cpp:29-40`); `ExpireTimerTask::registerExpirable` ported | P5-16, P5-15, P5-09 |
| 5 | Express mail: `CM_READ_EXPRESS_MAIL(1)` → `VisibleObjectSpawner.spawnPostman` → `DeliveryManAI` (extends `FollowingNpcAI`) | CM_READ_EXPRESS_MAIL.java:31-69; DeliveryManAI.java | **no file** for the packet and both AIs (`handlers/ai/` has 3 of 43 root handlers) | deferred (D9) |
| 6 | Logout persistence: `PlayerService.storePlayer` → `MailDAO.storeMailbox` | PlayerService.java:92 | ported | – |

### 2.5 Private store

| # | Step | Java | C++ today | Chunk |
|---|---|---|---|---|
| 1 | `CM_PRIVATE_STORE([objId, itemId, count, price]…)` → `createStoreWithItems`: `canOpenPrivateStore` (fly, move, combat, trading, ride, hide, dead, chair, existing store), `validateItem` per entry (≤ 10, tradeable, not equipped, no duplicate), `setStore`, `PRIVATE_SHOP` state, `RecallService.cancel`, broadcast `SM_EMOTION(OPEN_PRIVATESHOP = 33)`; an empty list closes the store | CM_PRIVATE_STORE.java:23-42; PrivateStoreService.java:35-122 | **no file**; **U** (`PrivateStoreService.cpp:10-27`); `RecallService::cancel` ported | P5-16, **P5-09** |
| 2 | `CM_PRIVATE_STORE_NAME(name)` → `openPrivateStore` → broadcast `SM_PRIVATE_STORE_NAME` | CM_PRIVATE_STORE_NAME.java:27-35; PrivateStoreService.java:228-231 | **no file**; **U** (`PrivateStoreService.cpp:43`) | P5-16, P5-09 |
| 3 | Buyer: `CM_DIALOG_SELECT(seller, BUY)` → `SM_PRIVATE_STORE` (§2.1 row 8); `CM_BUY_ITEM(seller, 0, [index, count])` → `sellStoreItem`: **the item "id" is the index into the store's insertion order** (PrivateStoreService.java:209), free slots, price, `decreaseItemFromPlayer`, `ItemService.addItem(buyer, item, count)`, `decreaseKinah` / `increaseKinah`, close when empty | PrivateStoreService.java:127-223 | **U** (`PrivateStoreService.cpp:28-42`) | P5-09 |

### 2.6 Crafting and gathering

| # | Step | Java | C++ today | Chunk |
|---|---|---|---|---|
| 1 | Learn a craft at a master: `CM_DIALOG_SELECT(COMBINE_SKILL_LEVELUP = 46)` → `CraftSkillUpdateService.learnSkill`: level ≥ 10, `Profession.getUpgradeCost(level)` (3,500 kinah for 0 → 1), an **anonymous `RequestResponseHandler`** with `SM_QUESTION_WINDOW(900852, name, price)`; on yes `tryDecreaseKinah(DEC_KINAH_LEARN)` + `PlayerSkillList.addSkill` → `SkillLearnService.onLearnSkill` → `SM_SKILL_LIST(1330061)` + `RecipeService.autoLearnRecipes` → `RecipeList.addRecipe` → `PlayerRecipesDAO.addRecipe` + `SM_LEARN_RECIPE` | DialogService.java:199-202; CraftSkillUpdateService.java:83-127; SkillLearnService.java:25-45; RecipeService.java:70-73 | **U** (`CraftSkillUpdateService.cpp:67, 72`); **`Profession.getUpgradeCost` / `getMaxUpgradableLevel` / `getClientName` ×2 / `getSkillGrade` / `getBySkillId` are not declared** (`model/craft/ProfessionInfo.h` has `getSkillId`, `isCrafting`, `PROFESSION_VALUES`); `SkillLearnService` (7 bodies) and `PlayerSkillList` (14) ported; **`RecipeService::autoLearnRecipes` U** (`RecipeService.cpp:15`) — and `SkillLearnService.cpp:65` already calls it | **P5-09** |
| 2 | `CM_CRAFT(unk, targetTemplateId, recipeId, targetObjId, materials, craftType)`: shutdown check, the station within 10 m unless morph (`unk == 129`) → `CraftService.startCrafting`: `checkCraft` (5 m to a `StaticObject`, DP, stance, full inventory, recipe known, cooldown, skill level, materials, **consumes the materials at its end**), interval `2500 − 60 × Δlevel` capped by quality, `new CraftingTask(…).start()` | CM_CRAFT.java:32-61; CraftService.java:97-238 | **no file**; **U** (`CraftService.cpp:11-30`) | P5-15, P5-09 |
| 3 | `CraftingTask`: `onInteractionStart` (two `SM_CRAFT_UPDATE`, two `SM_CRAFT_ANIMATION`), `analyzeInteraction` per tick (`Rnd` success vs `gameserver.craft.fail.chance × failReduction`, CRIT_BLUE 15 % + Δ/3, steps to 1000), `onSuccessFinish` → crit chain (`calculateCrit`: `gameserver.rates.crafting.crit_chances`) or `CraftService.finishCrafting` | skillengine/task/CraftingTask.java:21-173; AbstractCraftTask.java:11, 46-49 | **no file** (`skillengine/task/` has `AbstractCraftTask`, `AbstractInteractionTask`, `GatheringTask`); `fieldmap.json` has its row | **P5-02a** |
| 4 | `finishCrafting`: limited-production recipes, xp `(int)(0.008 × (lvl + 100)² + 60)` × `Rates.SKILL_XP_CRAFTING` × **`StatEnum.getModifier(skillId)`** boost, `addSkillXp` (level-up when `current + xp ≥ (int)(0.23 × (lvl + 17.2)²)`, PlayerSkillList.java:118-125), `addExp(XP_CRAFTING)`, `ItemService.addItem(CRAFTED_ITEM)` with an **anonymous `ItemUpdatePredicate.changeItem`** (creator name), craft cooldowns | CraftService.java:43-95; StatEnum.java:250-262 | **U**; `StatEnum.getModifier` has **no declaration**, only a P4-11b stand-in (`controllers/ControllerSupport.h:265-290`, used by `GatherableController.cpp:216`) | P5-09, P5-01 |
| 5 | `CM_RECIPE_DELETE(recipeId)` → `RecipeList.deleteRecipe` | CM_RECIPE_DELETE.java:21-30 | **no file**; `RecipeList::deleteRecipe` ported | P5-16 |
| 6 | A recipe item: `CM_USE_ITEM` → `CraftLearnAction.act` / `canAct` → `RecipeService.addRecipe` / `validateNewRecipe` | CraftLearnAction.java | shell header with neither method (`CraftLearnAction.h`); A-03 | **P5-07** |
| 7 | Gathering: `CM_GATHER(actionId)` → `GatherableController.startGathering` / `GatheringTask.abort` | CM_GATHER.java:26-50 | **no file** — the only missing piece; the rest is ported (§1) but ends in `ItemService::addItem` (A-02) | P5-15 — conditional (D10) |

### 2.7 The broker (deferred, D2)

`CM_BROKER_LIST`, `_SEARCH`, `_REGISTERED`, `_CANCEL_REGISTERED`, `_SELL_WINDOW`, `_SETTLE_ACCOUNT`, `_SETTLE_LIST`, `CM_BUY_BROKER_ITEM`,
`CM_REGISTER_BROKER_ITEM` — 9 files, 363 Java lines, none in C++ — call 11 unported `BrokerService` bodies (`BrokerService.cpp:145, 149, 153,
157, 183, 229, 233, 237, 253, 257, 292`; ~388 Java lines of the 586 in the file's bodies). The 28 ported bodies are the startup, login,
logout, deletion and periodic-settlement halves, which already run. `SM_BROKER_SERVICE` (266 Java lines) is ported. The npc that opens it
(`OPEN_VENDOR = 33`) spawns as 48 spawns in **20 maps** (measured over `spawns/**` with an XML parser; rev 1 said "a handful", which was
wrong): 400010000 Reshanta (4), 120010000 (4), 700010000 (4), 710010000 (4), 110010000 (3), two each in Verteron 210030000 (798001
gaurinerk, 798002 toroonerk, `spawns/Npcs/210030000_Verteron.xml`), Altgard 220030000 (798028, 798029), Eltnen, Heiron, Inggison,
Cygnea, Idian Depths (both), Morheim, Beluslan, Brusthonin, Gelkmaros, Enshar, one in Theobomos, and two on the custom GM isle 900110000.
**None spawns on Poeta or Ishalgen**; the nearest are in the second maps, which a new character reaches only by travel (M5f).

### 2.8 Status by area (measured)

**Unported sites per file, the milestone's scope** (grep over the C++ files `chunks.py files <chunk>` selects):

| Chunk | File | Sites | Stage | | Chunk | File | Sites | Stage |
|---|---|---|---|---|---|---|---|---|
| P5-08 | `services/DialogService.cpp` | 7 | 0 | | P5-09 | `services/craft/CraftService.cpp` | 5 | 2 |
| P4-11a | `model/gameobjects/Npc.cpp` (`canSell`, `canTradeIn`, `canPurchase`) | 3 | 0 | | P5-09 | `services/craft/CraftSkillUpdateService.cpp` | 4 | 2 |
| P5-09 | `services/TradeService.cpp` | 8 | 1 | | P5-09 | `services/RecipeService.cpp` | 3 | 2 |
| P5-09 | `services/ExchangeService.cpp` | 11 | 1 | | P5-00 | `services/player/PlayerService.cpp` (`getOrLoadPlayerCommonData` ×2) | 2 | 1 |
| P5-09 | `services/PrivateStoreService.cpp` | 8 | 1 | | P5-13 | `restrictions/PlayerRestrictions.cpp` (`canTrade`) | 1 | 1 (A-09) |
| P5-09 | `services/mail/MailService.cpp` | 7 | 1 | | P5-08 | `services/player/PlayerLimitService.cpp` | 1 | 1 |
| P5-09 | `services/mail/SystemMailService.cpp` | 2 | 1 | | P5-07 | `services/RepurchaseService.cpp` | 1 | 1 |
| **P5-07** | `services/EnchantService.cpp` (hand-off, §3a) | 11 | 0 | | **P5-07** | `services/CubeExpandService.cpp` (rev 1: optional) | 7 | 1 |
| **P5-07** | `services/item/ItemSocketService.cpp` (the manastone bodies; `socketGodstone` is M5b-3's T-05) | 7 | 0 | | **P5-07** | `services/item/ItemActionService.cpp` (A-13) | 2 | 1 |
| | | | | | | **Total** | **90** | |

Optional in scope: `reward/StarterKitService.cpp:55` (P5-09, 1 — reachable only with `gameserver.custom.starter_kit.enable`, default false).
`CubeExpandService` is **required** in rev 2: the cube expanders 798008 (Poeta, 12.8 m from the plan's merchant) and 798037 (Ishalgen) are
on the start maps and the dialog of stage 0 makes them loud (W-09). Out of scope in the same chunks: `BrokerService` 11 (D2), `MailFormatter`
6 (siege, house and abyss mails — their milestones), `AbyssPointsService` 4 (W), the drop services 43 (M5b-3, A-07), the other reward
services 11 and `AtreianPassportService` 1; in P5-07 the 49 sites §3a gives to other milestones.

**P5-09's "121" of the roadmap is right and is not M5c's number**: 121 = drop 43 + trade/exchange/store 27 + broker 11 + mail 15 + craft/recipe 12
+ rewards 12 + passport 1 (`chunks.py files P5-09`, grep). M5c's share is **48**, and 43 of the rest are M5b-3's. **P5-07's 109** (the
18 files of `chunks.py files P5-07` with a site): M5b-3 takes 32 (m5b3-plan.md §2.3), M5c 28 (the four rows above plus `RepurchaseService`),
and §3a gives the other 49 to named milestones.

**The invisible work** (lesson 1, measured with a script that compares `javasrc` method lists with the identifiers the C++ headers and xmlgen
member blocks declare; the script and its limits are in §12):

| Class | Chunk | Java bodies | What the C++ tree has | Bodies to write |
|---|---|---|---|---|
| `skillengine/task/CraftingTask` | P5-02a | 8 methods + constructor (174 lines) | nothing (`fieldmap.json` has the row, so `skeleton.py` can emit the shell) | **9** |
| `taskmanager/tasks/TemporaryTradeTimeTask` | P5-07 | 4 + singleton (63 lines) | nothing | **4** (A-08) |
| `data/handlers/ai/PostboxAI` | P5-05 | 2 (32 lines) | nothing; the AI name falls back to `DummyNpcAI` | **2** |
| `model/craft/Profession` (enum) | P5-09 | 8 | `ProfessionInfo.h` declares 2 (`getSkillId`, `isCrafting`) | **6** |
| `model/stats/container/StatEnum` (enum) | P5-01 | `getModifier`, `getSign`, `getItemStoneMask` | none declared; `getModifier` exists only as a P4-11b stand-in | **1** |
| `model/templates/item/actions/CraftLearnAction` | P5-07 | `canAct`, `act` | shell with no methods; the base declares no virtual (A-03) | **2** |
| anonymous subclasses | P5-15, P5-08, P5-09 | `CM_EXCHANGE_REQUEST$1` (2), `DialogService$1` RECOVERY (1), `CraftSkillUpdateService$1` (1), `CraftService$1.changeItem` (1) | callback structs to write inside the owning `.cpp` | **5** |
| `model/templates/item/actions/EnchantItemAction` (hand-off) | P5-07 | `canAct`, `act` ×2, `isSuccess`, `getMaxLevel`, `getMinLevel`, `isSupplementAction`, `checkSupplementLevel`, the `ItemUseObserver.abort` (222 lines) | shell; `canAct`/`act` become stubs with `m5b3-h01` | **9** |
| `…/DecomposeAction` (hand-off) | P5-07 | `canAct`, `act`, `postValidate`, `finishUse`, `filterItemsByLevel`, `containsSpecialCubeItems`, `isValidItemId`, the observer (423 lines) | a `.cpp` with the static-data validation only (`DecomposeAction.cpp:15-80`) | **7** |
| `…/TuningAction` (A-13) | P5-07 | `canAct`, `act`, `getRandomStatBonusIdFor`, the observer (114 lines) | shell | **4** |
| `…/ExtractAction` (hand-off) | P5-07 | `canAct`, `act`, the observer (70 lines) | shell | **3** |
| `…/RemodelAction`, `…/ExpandInventoryAction` (hand-off) | P5-07 | 2 + 2 (`RemodelAction` is `return false` and an empty `act`, RemodelAction.java:19-26) | shells | **4** |
| anonymous subclasses (hand-off) | P5-07 | `ItemActionService$1.abort`, `$2.run`, `CubeExpandService$1.acceptRequest` | callback structs in the owning `.cpp` | **3** |
| | | | **Total** | **59** |

Everything else on the path declares every Java method: `DialogService`, `TradeService`, `ExchangeService`, `MailService`,
`SystemMailService`, `PrivateStoreService`, `RecipeService`, `CraftService`, `CraftSkillUpdateService`, `RepurchaseService`, the trade models,
`Storage`, `Mailbox`, `Letter`, `RecipeList` — 0 undeclared (measured). The shells the script flagged in `ItemPacketService` (the three nested
enums' `getMask`, `isSendable`, `getKinahUpdateTypeFromAddType`, `fromUpdateType`) are A-01's; the packets carry their own mask tables
(`serverpackets/detail/PacketSupport.h:90-112`).

**Client packets** (all opcodes registered in `ClientPacketInfo.gen.inc`, measured):

| Need | Packets | Chunk | Java lines |
|---|---|---|---|
| **R** stage 0 | `CM_SHOW_DIALOG` (0x0117), `CM_QUESTION_RESPONSE` (0x0115) | P5-16 | 89 |
| **R** stage 0 | `CM_DIALOG_SELECT` (0x0119), `CM_CLOSE_DIALOG` (0x0118) | P5-15 | 162 |
| **R** stage 1 | `CM_BUY_ITEM` (0x0116), `CM_EXCHANGE_REQUEST` (0x0102), `CM_EXCHANGE_ADD_ITEM` (0x0103), `CM_EXCHANGE_ADD_KINAH` (0x02E5), `CM_EXCHANGE_LOCK` (0x02E6), `CM_EXCHANGE_OK` (0x02E7), `CM_EXCHANGE_CANCEL` (0x02E8), `CM_CHECK_MAIL_LIST` (0x0128), `CM_GET_MAIL_ATTACHMENT` (0x012B), `CM_DELETE_MAIL` (0x012C) | P5-15 | 494 |
| **R** stage 1 | `CM_SEND_MAIL` (0x0127), `CM_READ_MAIL` (0x0129), `CM_PRIVATE_STORE` (0x015A), `CM_PRIVATE_STORE_NAME` (0x015B) | P5-16 | 154 |
| **R** stage 1 | `CM_TUNE` (0x018E, A-13), `CM_TUNE_RESULT` (0x01B1), `CM_SELECT_DECOMPOSABLE` (0x018F) (hand-off) | P5-16 | 177 |
| **R** stage 2 | `CM_CRAFT` (0x0150) / `CM_RECIPE_DELETE` (0x013C) | P5-15 / P5-16 | 93 |
| **O** stage 2 | `CM_GATHER` (0x00F6) | P5-15 | 51 |
| deferred | `CM_READ_EXPRESS_MAIL`, `CM_BUY_TRADE_IN_TRADE`, the 9 broker packets | P5-15/16 | 482 |
| elsewhere (§3a) | `CM_CHARGE_ITEM`, `CM_ITEM_PURIFICATION`, `CM_ITEM_REMODEL`, `CM_FUSION_WEAPONS`, `CM_BREAK_WEAPONS`, `CM_UNWRAP_ITEM`, `CM_COMPOSITE_STONES`, `CM_APPEARANCE`, `CM_MEGAPHONE`, `CM_UPGRADE_ARCADE` | P5-15/16 | – |

`CM_MANASTONE` (0x02ED) is M5b-3's (m5b3-plan.md D8 ports it whole, with only arm 4 behind it); stage 0 here fills arms 1, 2, 3 and 8
(CM_MANASTONE.java:65-94, 104-106). `CM_USE_ITEM` (0x00C8) is M5b-3's P-05; stage 0 fills the `ExtractAction`, `DecomposeAction`, `RemodelAction`
and `ExpandInventoryAction` stubs it dispatches to.

**Server packets: none to write.** The 41 of the scope (`SM_BROKER_SERVICE` … `SM_WAREHOUSE_UPDATE_ITEM`, the list in §12) each have a
`.cpp` with 0 `AION_UNPORTED`; `SM_BROKER_SERVICE`'s one undeclared method is its nested enum's `getId`.

### 2.9 What wakes up (lesson 2)

Traced from every entry point the milestone turns on — the 23 client packets, the `CM_MANASTONE` arms and `CM_USE_ITEM` actions stage 0
fills, the tasks they schedule, the enter-world and logout calls, and the enter world of the gate's seeded characters — through to the first
unported or partial body. **W** rows are reached by M5c's own code and must be closed or deliberately left loud;
**D** rows are dormant (reached only by data or states the start maps and the gate do not produce) and are named so that the next milestone
does not rediscover them.

| # | Reached from | First unported / partial body | Kind | Resolution |
|---|---|---|---|---|
| W-01 | every talkable npc's `CM_SHOW_DIALOG` → `getStartPageId` | `DialogService::isInteractionAllowed` (`DialogService.cpp:27`) | **W** | D-02 |
| W-02 | the two Poeta/Ishalgen mailboxes (700000, 700079) | `PostboxAI` — no file; `DummyNpcAI` answers nothing (`AIEngine.cpp:158-168`) | **W**, silent | D-05 |
| W-03 | `SM_TRADELIST` / `SM_SELL_ITEM` constructors | `Npc::canSell`, `canPurchase` (`Npc.cpp:354, 366`) | **W** — a "0-unported" packet that throws | D-01 |
| W-04 | `performSellToShop` | `PlayerLimitService::updateSellLimit` (`PlayerLimitService.cpp:15`) — the whole body, the `LIMITS_ENABLED` early return included | **W** | P-02 |
| W-05 | `MailService.sendMail`, `SystemMailService.sendMail` | `PlayerService::getOrLoadPlayerCommonData` ×2 (`PlayerService.cpp:323, 327`) | **W** | M-02 |
| W-06 | `SkillLearnService::onLearnSkill` for any crafting **or morph** skill (`SkillLearnService.cpp:64-65`, SkillLearnService.java:40-41): the learn-a-craft dialog, **and every character that reaches level 10**, because `craft_skill_tree.xml:4-5` autolearns 30003 (Aethertapping) and **40009 (Morph Substances)** at level 10 for every class and race, and `learnNewSkills` (`SkillLearnService.cpp:80`) runs on each level change (PlayerController.java:594) — **including the enter world of a character whose level rose while offline** (PlayerEnterWorldService.java:204, `PlayerEnterWorldService.cpp:413`, W-20) | `RecipeService::autoLearnRecipes` (`RecipeService.cpp:15`) | **W, live** (rev 1 called it dormant, which was wrong). An online level-up past 9 needs Daeva status (§1 finding 3), so today it is reached by a class change (M5e), by an ascension quest (M5d/phase 6), or by a database edit — the gate's C19 seed and checklist step 11 | C-01; asserted by X21a (the three Elyos morph recipes 155000001, 155000002, 155000005, `recipe_templates.xml:3-27`) |
| W-07 | `ExchangeService.addItem` for an untradeable item | `TemporaryTradeTimeTask` — no file | **W** | P-04 (A-08) |
| W-08 | flight masters 203070, 203083 (Poeta) and 203513, 203545 (Ishalgen) — `func_dialogs="44"`, not in the two-teleporter arm of DialogService.java:188-195 | `TeleportService::showMap` (`TeleportService.cpp:284`) | **W**, loud | stays **U** until M5f; the checklist says so |
| W-09 | cube expanders 798008 (Poeta), 798037 (Ishalgen) — `EXTEND_INVENTORY = 47` | `CubeExpandService::expandCube` (`CubeExpandService.cpp:11`) | **W**, loud | P-05 (**R** in rev 2); case C17 |
| W-10 | the soul healers 203064 and 203084 (Poeta), 203512, 203680 (Ishalgen) — `RECOVERY = 35` | the accept calls `Storage.decreaseKinah` (DialogService.java:143) → `ItemPacketService` (A-01); nothing else once D-02 and D-04 land (`EffectController::removeByDispelSlotType` is ported since M5b-2 part 2) | covered by A-01 | case C13 |
| W-11 | `CM_QUESTION_RESPONSE` answers every pending request in the tree | the four existing `putRequest` sites: `AIActions.cpp:125` (ported), `Equipment.cpp:791` soul-bound equip (A-05), `NpcFactions.cpp:221` (`FACTION_JOIN`, no such npc on the start maps), `RVController.cpp:151, 158` (rifts, disabled by `gameserver.rift.enable = false`) | D | – |
| W-12 | `CM_DIALOG_SELECT` with `targetObjectId == 0` (quest report) | `QuestService::finishQuest` (`QuestService.cpp:82`), `ClassChangeService::changeClassToSelection` (`ClassChangeService.cpp:11`) | D until quests register (M5d) and M5e | – |
| W-13 | `onCloseDialog` at a legion-warehouse npc for a legion member | `LegionWarehouse::unsetInUse` (`LegionWarehouse.cpp:108`) | D (no legions until M5h) | – |
| W-14 | `SystemMailService.sendMail` becomes live for `BonusPackService` / `FactionPackService` (level 65), `VeteranRewardService.tryReward` (level 65, account ≥ 1 month), `StarterKitService.onLevelUp` | `StarterKitService::onLevelUp` (`StarterKitService.cpp:55`) behind `CustomConfig.ENABLE_STARTER_KIT` (default false) | D | optional R-02 |
| W-15 | talkable npcs whose AI handler is not ported: on Poeta 12 `quest_use_item`, 2 `resurrect` (the obelisks), 2 `simple_abyssguard`, 1 `useitem`, 1 `portal_dialog`; on Ishalgen 17, 2, **10** (among them 203524 megin and 203543 alfrigh, which `hasAlternativeDialogAfterAscension` lists as Ishalgen dialog npcs, `DialogPageInfo.cpp:31-80`), 2, 1 | `DummyNpcAI` — a click does nothing and logs nothing | D, silent | M5d/M5j; the checklist names it |
| W-16 | a character entering **Sanctum** (110010000) for the first time in any automated run (crafting, D5) | unknown — no gate has spawned a player in a capital | **unknown** | the gate lane measures it first (§13 item 2) |
| W-17 | **Extraction Tools** (165000001, `<extract/>`, `item_templates.xml:836407-836411`) are sold by the plan's own merchant 798007 (goods list 132) and by 203080 lonian (Poeta), 203542 denma and 798038 crizpinerk (Ishalgen) — every start-map npc with `BUY` and a list holding them; using one: `CM_USE_ITEM` → `ExtractAction` → a 5 s task → `EnchantService.breakItem` (ExtractAction.java:43-68) | the `ExtractAction` stub `m5b3-h01` declares, then `EnchantService::breakItem` (`EnchantService.cpp`) | **W** — M5c's own shop sells the entry point (rev 1 missed it) | E-01, E-03; case C18 |
| W-18 | the **manastone-removal npcs** 203336 Seril (Poeta, (862.714, 1251.59, 119.134), 11.1 m from 798007) and 203692 Dobar (Ishalgen) — `func_dialogs="42"` = `REMOVE_ITEM_OPTION`: `sendDialogWindow` → `SM_DIALOG_WINDOW(page 20 = REMOVE_MANASTONE, DialogPage.java:37)`; the client answers `CM_MANASTONE` arm 3 (CM_MANASTONE.java:90-94) | `ItemSocketService::removeManastone` (`ItemSocketService.cpp`) | **W**, loud (rev 1 missed it) | E-02; case C16 |
| W-19 | **every Plainsman's weapon and armour piece a Poeta monster drops** (51 items, all `option_slot_bonus="1"`, no `rnd_count`) is created unidentified (Item.java:84-85, ItemTemplate.java:155-160); `Equipment.equip` refuses it with a warning (Equipment.java:163-167) and the client identifies it with `CM_TUNE` | `CM_TUNE` no file; `ItemActionService::identifyItem` (`ItemActionService.cpp`) | **W** from M5b-3's loot on | P-07 (A-13); case C15 |
| W-20 | the **enter world of a character whose level changed offline**: `onLevelChange(PlayerDAO.getOldCharacterLevel(id), level)` (PlayerEnterWorldService.java:204, `PlayerEnterWorldService.cpp:413`) → `updateStatsTemplate`, `upgradePlayer`, `NpcFactions::onLevelUp`, `QuestEngine::onLevelChanged`, `learnNewSkills` (all ported, `PlayerController.cpp:670-702`). No gate has run it: in every earlier gate `old_level` equals the level | for the C19 seed (a Gladiator at level 10): the Warrior skills of levels 2-9 and the Gladiator skills of 9-10, whose passives are `statboost`, `wpnmastery`, `armormastery`, `shieldmastery` (skills 169, 348, 138, 44-46, 48-54, 139, `skill_tree.xml` × `skill_templates.xml`; all four classes are in M5b-2's subset, m5b2-plan.md §2.4), the 30001 → 30002 swap (SkillLearnService.java:70-74), then 30003 and 40009 → **W-06** | **W** for the seeded run (new in rev 2) | C-01 closes W-06; the rest is ported; X21a asserts it |
| W-21 | `CM_MANASTONE` arms 1/2 with a **stigma stone on a stigma** (CM_MANASTONE.java:76-77) | `StigmaService::chargeStigma` (`StigmaService.cpp`) | D (no stigma on the start maps) | M5e (§3a) |
| W-22 | `CM_USE_ITEM` on an item whose only action is `<remodel>` (15,105 templates, most equipment) → `RemodelAction.canAct` | the `m5b3-h01` stub, which throws where Java answers `false` (RemodelAction.java:19-22) | **unknown**: whether the 4.8 client sends `CM_USE_ITEM` for equipment was not measured | E-04 ports the two trivial bodies; **recommended to M5b-3** (§3a) |
| W-23 | `CM_USE_ITEM` on a bundle (`<decompose>`, 4,125 templates; one in Poeta's drop set, m5b3-plan.md §2.4) → `DecomposeAction` → `CM_SELECT_DECOMPOSABLE` for a selectable box | the `m5b3-h01` stubs; `CM_SELECT_DECOMPOSABLE` no file | **W** from M5b-3's loot on; common once M5d's quests reward bundles | E-04, K-02 |

### 2.10 The shipped data the gate stands on (lesson 3)

Every number here comes from the Java data files and the Java arithmetic, computed for this plan with the oracle's own float helpers
(`tools/oracle/m5a/javafloat.py`); **G-01 re-derives all of them** and the gate asserts the oracle's output, never these constants.

**The merchants.** The Poeta vendors cluster in Akarios village, 417 m from the Elyos spawn point (1212.94, 1044.85, 140.76):

| npc | name | spot | `func_dialogs` | trade tabs → goods (`npc_trade_list.xml`, `goodslists/goodslists.xml`, parsed with an XML parser) |
|---|---|---|---|---|
| **798007** | minalinerk (general goods) | (851.671, 1252.67, 118.833) | 2 3 | 132 → 169000003 Minor Power Shard, 165000001 Extraction Tools, 169300002 Bandage; 720 → **162000052 Minor Life Elixir**, 162000057 Minor Mana Elixir |
| 203060 | mune (armour) | 17.6 m away | 2 3 | 129, 130, 131, 450 |
| 203061 / 203063 | uno (food) / amus (weapons) | < 20 m | 2 3 | 133 / 127, 128 |
| 203080 | lonian (general goods; rev 1 left it out) | Poeta | 2 3 | includes 132 (Extraction Tools) |
| 700000 | mailbox (`ai="postbox"`, talk distance 5) | (827.231, 1243.21, 118.876), 26.2 m | – | – |
| 203064 | fulla (soul healer) | (851.51, 1208.55, 117.815), 44.1 m | 35 | – |
| 798008 | baevrunerk (cube) | (839.719, 1257.19, 118.875), 12.8 m | 47 | first npc expansion **1,000 kinah**, raw (`storage_expander/cube_expander.xml:4-6`; CubeExpandService.java:44 applies no price modifier) |
| 203336 | seril (manastone removal) | (862.714, 1251.59, 119.134), 11.1 m | 42 | – |

Ishalgen's equivalents are 798038 crizpinerk (tabs 264, 721 — the same goods), 203514/203515/203526/203542, 203692 dobar (42) and mailbox
700079 at (562.239, 2426.29, 278.47). Every tradelist template on both maps is `npc_type` NORMAL — **written on the wire as 1**, the enum's
constructor argument (`NORMAL(1)`, TradeNpcType.java:12; `writeC(tradeNpcType.index())`, SM_TRADELIST.java:59, SM_SELL_ITEM.java:40) — with
`sell_price_rate` 100 (the defaults, TradeListTemplate.java:25-28), and **none has a `purchase_template`**, so selling always takes the
`purchaseTemplate == null` arm (TradeService.java:215-221) and `SM_SELL_ITEM` falls back to NORMAL (SM_SELL_ITEM.java:30).

**Prices.** With `gameserver.siege.enable = false` (the M5a/M5b profiles and the user's `mygs.properties`) `SiegeService` holds no location
(SiegeService.java:74-91), so `Influence` computes 0 for every race (Influence.java:36-42), and `PricesService` gives **global prices 125 %**
and **taxes 113 %** (`Math.round(100 + 0.25 × 100)`, `Math.round(100 + 0.125 × 100)`, PricesService.java:21-53) — the three bytes of the
enter-world `SM_PRICES` (SM_PRICES.java:13-18). Then:

| Formula | Java | Value |
|---|---|---|
| buy price, template price 250 (the elixir) | `getBuyPrice`: four truncations through 100 / 125 / 100 / 113 % (PricesService.java:95-98) | **352** each; 50 → 70 (Salt); 5 → 6 |
| sell reward, price 250 (Minor Life Potion) | `getSellReward(250, VENDOR_SELL_MODIFIER = 20)` | **50** each |
| mail cost, 5 × 162000002 + 200 kinah, NORMAL | `10 + (long)(200 × 0.01f) + (long)(250 × 0.02f × 5 × 1)` = 37, then `getPriceForService` | **51**, i.e. **251** with the kinah |
| soul healing, `recoverexp` 1000 | `(int)(1000 × (0.25 − 0.00000015 × 1000))` (DialogService.java:132-133) | **249** |
| cooking 0 → 1 | `Profession.getUpgradeCost(0)` (Profession.java:37-49), no price modifier | **3,500** |
| craft xp, recipe skillpoint 1 | `(int)(0.008 × 101² + 60)` | **141**; level-up needs `(int)(0.23 × 18.2²)` = **76**, so one craft makes cooking 1 → 2 |
| Extraction Tools, template price 1000 | `getBuyPrice` as above | **1,412** |
| manastone removal | `getPriceForService(650)` (ItemSocketService.java:123): 650 → 812 → 812 → 917 | **917** |
| C19's exact-kinah seed | `getUpgradeCost(0)` + 2 × Salt | **3,640**, so the Salt purchase spends the last kinah (X6's `>=` mutation, TradeList.java:56 and Storage.java:83) |

**The character.** A new Elyos Warrior or Mage owns 1,000 kinah (item 182400001), 100 × 162000002 Minor Life Potion (mask 12414:
TRADEABLE and SELLABLE, ItemMask.java:8-9), 100 × 162000007, 20 × 169300002 Bandage (12414) and 12 × 160000001 Mercenary's Fruit Juice
(mask 12360: **neither tradeable nor sellable**) — `player_initial_data.xml`, confirmed by `oracle.py m5a-creation --race ELYOS --class WARRIOR`
(run read-only for this plan). So the gate can sell, exchange, mail and store without any loot, and has a built-in negative case.

**Crafting.** The only Elyos cooking autolearn recipe at skill 1 is **155001381** (`recipe_templates.xml:12124-12130`): 1 × 152001001 Inina +
2 × 169400096 Salt → 2 × 160001001 Roast Inina (combo 160001051). Salt is sold by 203785 Luelas in Sanctum (tab 68); Inina is not sold by any
spawned npc: goods list 119 is the only list holding it, and of its three templates the `tradelist_template` of 203234 (the seller) has no
spawn, 832782 nallo (Reshanta, `spawns/Sieges/400010000_Reshanta.xml:181`) holds it in a **`purchase_template`** — nallo *buys* it
(npc_trade_list.xml:10534-10540) — and 831585 in a trade-in list, unspawned. It is gathered only in Verteron (gatherable 400901); the
level-1 morph recipe 155000002 also makes 3 Inina from one 152000901 (`recipe_templates.xml:8-12`). The Sanctum cluster: master **203784
Hestia** (1848.07, 1543.97, 590.158), **Luelas** 7.7 m away, and four **Ovens** (static template 150000009, `spawns/Statics/110010000_Sanctum.xml:48-53`)
5.4-6.6 m from Hestia. The level-10 threshold is `experience[9]` of `player_experience_table.xml`, **126,069** (measured by the review;
`getStartExpForLevel(10)`, PlayerExperienceTable.java:29-34). **At that exp a Warrior or Mage is level 9, not 10**: a starting class loads
with `maxLevel` 10 unless the exp is *above* the threshold or `updateDaeva()` finds quest 1006/2008 COMPLETE on an advanced class
(PlayerCommonData.java:276-281, 588-610), and online it stays capped at 9. The C19 seed is therefore a Daeva (D5).

---

## 3. Where this plan disagrees with the roadmap

| phase5-roadmap.md | This plan | Why |
|---|---|---|
| M5c "chunks mainly P5-09 (trade, mail, craft, broker), P5-07" | **P5-09 48 sites, P5-07 28 sites + 8 invisible classes, P5-08 8, P4-11a 3, P5-00 2, P5-13 1, P5-02a 1 class, P5-05 1 handler, P5-15/P5-16 23 packets** | rev 1 said "P5-07 contributes little"; with the M5b-3 hand-off (§3a) P5-07 is the second-largest chunk of the milestone |
| M5e "P5-08 (skill learn, class change, **dialog**)" | **`DialogService` moves to M5c** (stage 0) | no vendor, mailbox, master or soul healer is reachable without it (§2.1). It also opens the way for M5d: every XML quest dialog goes through `CM_DIALOG_SELECT` and `handleQuestDialogueOrSendNextPage` |
| "P5-09 … 121" unported | 121 is right; **48** are M5c's, 43 are M5b-3's drop | §2.8 |
| M5c includes the broker | **recommend a capital-economy milestone after M5f** (user's decision, D2) | no broker npc on either start map (20 maps, §2.7), so nothing is reachable before travel; 11 sites + 9 packets (~750 Java lines); needs a two-player market and the settlement timer |
| M5c includes crafting, "a player can … craft" | **kept, but no real player on the start maps can reach it**; the gate seeds a Daeva in Sanctum, the checklist does the same by SQL | §1 finding 3 |
| m5b2-plan.md O-02: `CraftingTask` "M5b-3 (with the item path)" | **M5c** (item C-02); the M5b-3 plan did not take it | the roadmap's rows |
| m5b3-plan.md D2, D8, O-02..O-04: manastones, enchant, amplify, tempering, stigma, tune, remodel, purify, charge, dye, pack, decompose, the other item actions, npc warehouse, cube expansion → "M5c" | **§3a gives every one of them a named home**: M5c takes what a start-map player reaches (~63 bodies); the rest goes to M5d, M5e, M5f, M5h, M5i, M5j or the capital-economy milestone of D2 | rev 1 took none of it, so ~230 bodies belonged to no milestone |
| m5b3-plan.md D9, O-01: `TemporaryTradeTimeTask` → M5g | **M5c** (P-04) | `ExchangeService.addItem` asks it for every untradeable item (ExchangeService.java:108); the exchange is M5c's. Team loot in M5g then finds it ported |

### 3a. The M5b-3 hand-off, reconciled

`m5b3-plan.md` sends to "M5c" everything item-shaped that its solo loot path does not need (D2 at :332; D8 at :338; O-02..O-04 at :428-430;
the packet table at :275). Rev 1 of this plan took none of it. Rev 2 measured every class (`javasrc` bodies against the C++ declarations,
the same script as §2.8, over the working tree) and gives each one **one** home. The rule: **M5c takes what a player on Poeta or Ishalgen
reaches once M5b-3 and M5c are in; an item that needs another milestone's system goes to that milestone; an item reachable only in the
capitals or the field maps goes with the capital economy of D2** (or to M5j if the user keeps the broker in M5c).

| Item (Java) | Bodies (sites + undeclared) | Reached on the start maps by | Home | Why |
|---|---|---|---|---|
| `EnchantService` (591 lines), `EnchantItemAction` (222), `ItemSocketService` manastone bodies (`addManaStone` ×2, `insertManaStoneIntoNextPossibleSlot`, `insertManastoneIntoSlot`, `copyFusionStones`, `removeManastone`, `removeAllManastone`), `ExtractAction` (70); `CM_MANASTONE` arms 1, 2, 3, 8 go live | 11 + 7 + 9 + 3 = **30** | manastones on 15 % of Poeta kills (m5b3-plan.md §2.4); Seril/Dobar (W-18); Extraction Tools at 798007 (W-17) | **M5c stage 0** (E-01..E-03) | the three reachable paths share one service; amplification (arm 8, `amplifyItem`) is in the same class and is ported with it, unit-tested only |
| `DecomposeAction` (423), `CM_SELECT_DECOMPOSABLE` (70), `RemodelAction` (36, trivial) | 7 + 2 + 2 = **11** | one Poeta drop (m5b3-plan.md §2.4); W-22 | **M5c** (E-04 stage 0; K-02 stage 1) | bundles are items a start-map player owns; M5d's quest rewards make them common. `RemodelAction`'s two bodies are recommended to M5b-3 (they are `return false` and `{}`) |
| `ItemActionService` (identify, apply tune result), `TuningAction`, `CM_TUNE` (55), `CM_TUNE_RESULT` (52) | 2 + 2 + 4 + 4 = **12** | every Plainsman's drop (A-13, W-19) | **M5c stage 1** (P-07, K-02) — **recommended to M5b-3** | M5b-3's own checklist equips loot, which needs identification first |
| `CubeExpandService` (+ its handler), `ExpandInventoryAction` | 7 + 1 + 2 = **10** | 798008 / 798037 (W-09) | **M5c stage 1** (P-05, now R) | a loud npc on both start maps |
| `CraftLearnAction` (already in rev 1) | 2 | recipe items | **M5c** (C-05) | crafting |
| `TemporaryTradeTimeTask` (already in rev 1) | 4 | every exchange of an untradeable item (W-07) | **M5c** (P-04) | the exchange |
| `StigmaService` (11 + 2 inner), `SkillLearnAction` (3); `CM_MANASTONE` 1/2 on a stigma | **16** | nothing (stigma masters in 18 maps, none a start map; W-21) | **M5e** training and progression | stigmas and skill books are skill learning |
| `QuestStartAction`, `ReadAction` | **8** | 2 + 2 items in the drop sets (m5b3-plan.md §2.4) | **M5d** | quests |
| `MultiReturnAction`, `InstanceTimeClear` | **8** | – | **M5f** | teleport, instances |
| `DecorateAction`, `SummonHouseObjectAction`, `HouseObjectFactory` (2 sites; P5-07 but no plan named it), `DyeAction`'s house arm | **7** | – | **M5h** | housing |
| `ApExtractAction` | **5** | – | **M5i** | abyss points |
| `AdoptPetAction`, `ToyPetSpawnAction`, `RideAction`; `AnimationAddAction`, `EmotionLearnAction`, `TitleAddAction`, `CosmeticItemAction`, `FireworksUseAction`, `MegaphoneAction`, `ExpExtractAction`; `CM_APPEARANCE` (rename coupons and cosmetics, CM_APPEARANCE.java:54-67), `CM_MEGAPHONE`; `UpgradeArcadeService` (12 sites; P5-07, no plan named it) + `CM_UPGRADE_ARCADE` | 15 + 23 + 4 + 14 = **56** | – | **M5j** the rest | pets, mounts, cosmetics, emotes, titles, chat, an event |
| **Group K**: `WarehouseService` (5 + 1; the warehouse npcs, `DEPOSIT_CHAR_WAREHOUSE = 26`, spawn in 30 maps, none a start map, and no npc carries `EXTEND_CHAR_WAREHOUSE = 48`), `ItemChargeService` (12 + 1) + `ChargeAction` + `CM_CHARGE_ITEM`, `ItemPurificationService` + `CM_ITEM_PURIFICATION`, `ItemRemodelService` + `CM_ITEM_REMODEL`, `ArmsfusionService` + `CM_FUSION_WEAPONS` + `CM_BREAK_WEAPONS`, `TamperingAction`, `PolishAction`, `DyeAction`, `AssemblyItemAction`, `PackAction` + `CM_UNWRAP_ITEM`, `CompositionAction` (no C++ file) + `CM_COMPOSITE_STONES` | **70** | nothing: their npcs (functions 26, 43, 66/67, 75/76, 94/95, 109) spawn only in capitals, Reshanta and field maps (measured over `spawns/**`), and their consumables are not sold on the start maps (the 106 goods of the start-map vendors carry only `skilluse`, `extract` and `polish` actions; the six idians belong to 798037, which has no `BUY`) | **the capital-economy milestone of D2**; **M5j** if the user keeps the broker in M5c | none is reachable before travel (M5f) |
| | **~63 new to M5c (+6 rev 1 already had), ~170 elsewhere** | | | |

**What the M5b-3 plan must change to match** (this plan cannot edit it; the integrator does, and records it in both plans' review
sections): O-01 drops `TemporaryTradeTimeTask` (M5c P-04); O-02, O-03 and O-04 point to this table instead of "M5c"; D2's "Out" list names
the homes above; and three recommendations — take A-13 (identification), `RemodelAction`'s two trivial bodies, and D-04
(`CM_QUESTION_RESPONSE`, which its own soul-bind handler needs). If M5b-3 takes them, P-07, E-04's `RemodelAction` part and D-04 drop here.

---

## 4. Decisions

Decisions the integrator takes under the standing instruction unless marked **user**.

| # | Decision | Why |
|---|---|---|
| **D1** | **P5-09 is split in the manifest into three parts sharing `aion_gs_economy`**: **P5-09a** drop, rewards, passport, bonus and faction packs, guide (`services/{drop,reward}/**`, `AtreianPassportService`, `BonusPackService`, `FactionPackService`, `model/guide/**`) — M5b-3's side, 56 sites; **P5-09b** trade and market (`TradeService`, `ExchangeService`, `PrivateStoreService`, `BrokerService`, `services/trade/**`) — 38 sites, 27 in scope; **P5-09c** mail and craft (`services/{mail,craft}/**`, `RecipeService`, `model/craft/**`) — 27 sites + 6 undeclared, 21 in scope. Tests follow into `tests/economy/P5-09{a,b,c}` as P5-02a/b did. **The M5b-3 plan does not split P5-09** (its loot lane owns the chunk whole, m5b3-plan.md §6), so the split lands at M5c's branch (I-01), after M5b-3's last merge. | a chunk is the unit of ownership: unsplit, the trade services and mail would be one serial lane through stages 1 and 2 (~1,400 Java lines of bodies). The P5-02a/b precedent (`chunks.cmake:258-279`) is exactly this shape |
| **D2** | **user** — **A capital-economy milestone scheduled after M5f** takes the broker, express mail (+ `DeliveryManAI`, `FollowingNpcAI`, `CM_READ_EXPRESS_MAIL`), trade-in (`CM_BUY_TRADE_IN_TRADE`), AP vendors (`AbyssPointsService` 4), the warehouse and its expansion, and §3a's group K (the capital-only item services, ~70 bodies). **If the user wants the broker now**, stage 3 keeps an optional broker lane (B-01, B-02, with B-03 in the stage's one P5-SC lane) and group K goes to M5j. | **Measured:** the broker npc spawns in 20 maps, none a starting map — the capitals, Oriel, Pernon, Reshanta, and two per field map from Verteron and Altgard on (§2.7); group K's npcs and consumables likewise (§3a). So nothing in D2 is reachable before travel (M5f), a market needs two players and the settlement timer, and the broker alone is ~750 Java lines with its own 266-line packet decoder. Rev 1's reason ("capital-only") was wrong in fact but not in effect |
| **D3** | **Crafting stays in M5c** (stage 2). The gate reaches Sanctum by seeding a Daeva (D5); the real-client checklist gives the SQL (§11 step 11). | the user's roadmap row asks for crafting; changing that is the user's call, and the cost of keeping it is one stage |
| **D4** | **`DialogService` is ported whole and faithfully, including the arms that reach unported services.** `TeleportService::showMap` (W-08) stays `AION_UNPORTED` and loud until M5f; `CubeExpandService` (W-09) is **required** in stage 1; the arms whose services §3a sends elsewhere (`WarehouseService::expandWarehouse`, `ItemChargeService`, `LegionService`, `HousingService`) stay loud | a partial switch would be an invented behaviour; a loud arm is the m5b2-plan.md D6 rule applied to dialogs |
| **D5** | **The gate seeds rather than plays what M5c does not own**: positions (`players.x/y/z/world_id`), `players.recoverexp` for soul healing, the kinah row's `item_count`, item rows whose `item_unique_id` lies **above `gameserver.idfactory.wrap_at` (2²⁷)**, which the monotone cursor never reaches in a run (`IDFactory.h`, "Monotone cursor") — one Inina for C19, and for C15-C18 one unidentified Plainsman's armour piece B can wear (`tune_count = -1`, the value Item.java:85 gives a fresh one), one Plainsman's weapon to extract, one manastone the start maps drop — and **for C19 a Daeva**: `players.player_class` = the Warrior's advanced class `GLADIATOR`, a `player_quests` row (1006, `COMPLETE`) (`sql/aion_gs.sql:786-798`), and `players.exp` = 126,069. Rev 1 seeded only the exp, which loads a Warrior at **level 9** (§2.10) and made C15's learn a silent no-op. All seeds are written with `ScenarioDatabase::execute` while the character is offline, as M5b-1 seeded `player_life_stats.hp` (`M5bScenarioTest.cpp:1786`). | walking 417 m past aggressive monsters, looting a specific drop, gathering in Verteron, the ascension quest and levelling to 10 are other milestones' features; the precedents are m5b-plan.md D12 and m5b2-plan.md D3. The Daeva seed is also the only way any automated run reaches W-06 and W-20 before M5d/M5e |
| **D6** | **The gate profile removes the randomness it can**: `gameserver.craft.fail.chance = 0` (CraftConfig.java:28-29), `gameserver.rates.crafting.crit_chances = 0, 0` (RatesConfig.java:11-12) and `gameserver.rates.manastone_chances = 200, 200` (RatesConfig.java:17-18), beside the M5b profile's `gameserver.siege.enable = false` (prices 125/113) and `gameserver.limits.enable = false` (sell limits: W in the gate, unit-tested) | `analyzeInteraction` rolls `Rnd` every tick; with failure 0 what is left is CRIT_BLUE and `multi = Rnd.nextFloat(1f, 2f)` (CraftingTask.java:131, rev 1 omitted it), which change the number of ticks (bounded, X20) and never the product. `socketManastone` has no cap on its chance (EnchantService.java:344-395), so 200 makes a socket certain. **Two things stay random and are asserted as sets**: enchanting is capped at 80 % (EnchantService.java:126-127), and `breakItem` rolls the stone grade and count (EnchantService.java:52, 74) |
| **D7** | **The exchange's double-confirm race is ported as Java has it and marked `// java-race`**: two `CM_EXCHANGE_OK` processed at once on two connection threads can both see the partner confirmed and both call `performTrade` (ExchangeService.java:225-232; `Exchange.confirmed` is a plain field). A unit test on a deterministic executor names the interleaving; **a fix is a behaviour change and is offered to the user** as a deviation proposal, not taken | faithfulness (m5b2-plan.md D9); the consequence in Java is a failed second `removeItemsFromInventory` and an audit line, but it must be measured, not argued |
| **D8** | **`StatEnum.getModifier` gets a real home**: a new companion `model/stats/container/StatEnumInfo.h` (P5-01, a new file, no request) with `getModifier`; `CraftService` uses it. The P4-11b stand-in (`ControllerSupport.h:265`) is left for its owner to switch | including another chunk's `detail::` helper from P5-09 would be a layering shortcut |
| **D9** | **Express mail is not ported** (D2): an EXPRESS letter is still stored, listed and readable at a postbox; clicking the client's express-mail icon sends `CM_READ_EXPRESS_MAIL`, which stays an unknown packet | porting the packet without `DeliveryManAI` would spawn a postman with a `DummyNpcAI` that never despawns (its despawn task is scheduled by `DeliveryManAI.handleSpawned`), pinned by `Player.postman` until logout (`cycles.toml:159`) |
| **D10** | **`CM_GATHER` is ported in stage 2** (the M5b-3 plan does not take it, §0). | it is the one missing piece of gathering (§2.6 row 7), and gathering is where crafting materials come from |
| **D11** | **The gate asserts the database, not only the wire**: after each quit it reads `mail`, `inventory` (owner, location, count per item id, `tune_count`, `enchant`), `item_stones`, `players.mailbox_letters`, `players.recoverexp`, `players.npc_expands`, `player_recipes` and `player_skills`; and it keeps, per client, an inventory model built from every item packet | seven DAO write paths run for the first time (§1 hole 4). **A duplicated item cannot show as one `item_unique_id` under two owners** — `inventory` has `PRIMARY KEY (item_unique_id)` (`sql/aion_gs.sql:380`), so rev 1's claim was impossible; a duplication shows instead as per-item-id counts that sum above the oracle's ledger, as a DAO error in `gs_log` (a duplicate-key insert), or **before any write** as one object id present in both clients' inventory models |
| **D12** | **No `gs.scenario.m5c_geo`.** | measured: no Java class on the gate's paths calls `GeoService` (grep over the services, packets and `PositionUtil.isInTalkRange` / `isInRange`); the one `GeoService` use in `NpcController.java` is in `onDie` (:165). **Gathering does** — `GatherableController.startGathering` checks `GeoService.canSee(player, gatherable)` (GatherableController.java:56), reached by `CM_GATHER` (C-04, D10) — but gathering is not in the gate, so that check is covered only by the real-client session. m5b-plan.md §6.4 is the precedent for saying so instead of inventing a row. Geo coverage of Sanctum comes from the real-client session too |
| **D13** | **user** — **The stress run (G-05) is not on the required path.** It is written as a proposal: 10 pairs of `FakeGameClient`s that buy, sell, exchange and mail in a loop, under ASan, with a **ledger** invariant (below). It runs only in a slot the user chooses, and its shape joins the capacity conversation (`capacity-proposals.md` §4.10 is the user's table, §11 the questions; the integrator adds it there — this plan cannot edit that file). | The standing resource rule after 2026-09-21: "no stress, soak or ASan run without asking" (`capacity-proposals.md:654-656`); the roadmap reserves to the user "anything that loads their machine beyond the resource rules, and the design of the capacity tests" (phase5-roadmap.md:62-66). Rev 1 had it as a required nightly. **Its invariant was also false**: npc trade creates and destroys items (TradeService.java:151 `addItem`; :238-246 `delete` / `decreaseItemCount`), so per-item-id counts over the database are not conserved. Restated: each client keeps a ledger of what it bought (+), sold (−), bought back (+), crafted (+/−) and paid; at the end, per item id, **database total = starter items + Σ ledger deltas**, and kinah likewise with the mail commissions as a sink; only the player-to-player transfers (exchange, mail, private store) must conserve per item id on their own, which T-04 already proves in a unit test |
| **D14** | **The M5b-3 hand-off is reconciled by §3a**, taken by the integrator: M5c ports the start-map-reachable item services; each other item has a named milestone. | lesson 2: a body that belongs to no milestone is a throw a real player finds. The one user-shaped part — group K's home — rides on D2 |

---

## 5. Work items

Effort is **size, not time** (rev 1's agent-days were not calibrated): **S** ≤ 10 bodies or ≤ 250 Java lines, **M** ≤ 30 / ≤ 700,
**L** ≤ 60 / ≤ 1,500, **XL** beyond. The measured pace to set it against (git log): M5b-1 went from its plan commit `5f65cb14f`
(2026-09-22 02:29) through stage 1 `340c05c5e` (16:40; 121 sites removed, 11 added, 15,053 insertions) to its gate `23c4e6485` (2026-09-23
02:32), about 24 h; M5b-2 part 2 `c1edb0afb` closed 295 sites in about 4 h after part 1 `29009d778` (14:36 → 18:49), so a lane of that wave
closed roughly 50 bodies. Need: **R** required, **W** stub-with-warning allowed, **O** optional. "A-xx" in Deps is an assumption of §0.

### Integrator

| Id | What | Deps | Need | Eff |
|---|---|---|---|---|
| I-01 | D1's manifest split of P5-09 and the three test directories (the M5b-3 plan does not split P5-09, so M5c does it at branch time); the §3a edits to `m5b3-plan.md` (O-01, O-02..O-04, D2) recorded in both plans | – | R | S |
| I-02 | **Before stage 0**: the header batch of §7 (the `AbstractItemAction` virtuals and the `canAct`/`act` stubs are M5b-3's `m5b3-h01`; this batch adds the private helpers of `EnchantItemAction` and `DecomposeAction`, `ProfessionInfo.h`'s six functions), and **the `CraftingTask` shell** (`skeleton.py`, from the `fieldmap.json` row that already exists) with `fwd.h` regenerated, so that C-01 (which constructs a `CraftingTask`, CraftService.java:123, 131) and C-02 compile against the same header from stage 2's first day | A-03 | R | S |
| I-03 | `game-server/config/m5c.properties.example` in the **Java** tree beside `m5b.properties.example` (m5b-plan.md I-01's location), with D6's keys | – | R | S |
| I-04 | Allow-list bookkeeping: an edit **above line 268** of `PlayerService.cpp` shifts the `PlayerService.cpp:268` row of both `m5a_partial_allowlist.txt` and `m5b_partial_allowlist.txt` (and the m5b2/m5b3 lists that copy it). M-02's bodies are below it (:323-330), but a new `#include` at the top is not | M-02 | R | S |

### Stage 0 — talking to npcs (P5-08, P4-11a, P5-05, P5-15, P5-16) and the start-map item services (P5-07)

| Id | What | Java refs | Deps | Need | Eff |
|---|---|---|---|---|---|
| **E-01** | `EnchantService` — all 11 (`breakItem`, `calculateEffectiveLevel` ×2, `enchantItem`, `enchantItemAct`, `setEnchantLevel`, `applyEnchantEffect`, `socketManastone`, `socketManastoneAct`, `getEquipBuff`, `amplifyItem`) (P5-07; §3a) | EnchantService.java:37-591 | A-01, A-02 | R | L |
| **E-02** | `ItemSocketService` — the 7 manastone bodies (`socketGodstone` is M5b-3's T-05) | ItemSocketService.java:30-151 | A-01 | R | M |
| **E-03** | `EnchantItemAction` (9, incl. the second `act` overload and the observer struct) and `ExtractAction` (3) | EnchantItemAction.java; ExtractAction.java:25-68 | A-03, I-02, E-01 | R | M |
| **E-04** | `DecomposeAction` (7 + the static reward tables the existing `.cpp` already validates, `DecomposeAction.cpp:15-80`) and `RemodelAction` (2 trivial; drop if M5b-3 took them) | DecomposeAction.java:39-423; RemodelAction.java:19-26 | A-02, A-03, I-02 | R | M |
| **E-05** | Tests in `tests/itemsvc`: `breakItem`'s grade and count over a seeded `Rnd` (EnchantService.java:48-74) and its refusals; `socketManastone`'s float chance (EnchantService.java:344-395) and slot limits; `enchantItem`'s 80 % cap and both outcome arms; `removeManastone`'s price (`getPriceForService(650)`), refusals and the `DELETED` persistent state (ItemSocketService.java:102-138); `amplifyItem`; `ExtractAction`'s refusals and its 5 s task with the observer's abort on a `DeterministicExecutor`; `DecomposeAction`'s fixed, random and selectable reward arms; `RemodelAction.canAct == false`. Mutation-proven | – | E-01..E-04 | R | L |
| **D-01** | `Npc::canSell`, `canTradeIn`, `canPurchase` (`Npc.cpp:354, 362, 366`; `DataManager.h` is already included) | Npc.java:361-386 | – | R | S |
| **D-02** | `DialogService` — all 7 bodies, the whole switch, and the RECOVERY `RequestResponseHandler` callback struct | DialogService.java:53-377 | D-01 | R | M |
| **D-03** | `CM_SHOW_DIALOG` (P5-16), `CM_DIALOG_SELECT` and `CM_CLOSE_DIALOG` (P5-15), byte-vector tests in `tests/cm_ak`, `tests/cm_lz`, run tests over `InWorldPacketRunSupport.h` | CM_SHOW_DIALOG.java, CM_DIALOG_SELECT.java, CM_CLOSE_DIALOG.java | D-02 | R | S |
| **D-04** | `CM_QUESTION_RESPONSE` (P5-16) and its tests — **drop if M5b-3 ported it** (A-10; recommended there) | CM_QUESTION_RESPONSE.java:27-45 | – | R | S |
| **D-05** | `PostboxAI` (P5-05 `handlers_ai_core`): `handleDialogStart`, `handleDialogFinish`, the `AION_AI` registration; a `tests/handlers_ai_core` case that `AIEngine::newAI("postbox")` is no longer a `DummyNpcAI` | data/handlers/ai/PostboxAI.java | – | R | S |
| **D-06** | Tests in `tests/playersvc`: `isSubDialogRestricted` per `SubDialogType` (the 14 in the data), `isInteractionAllowed` for a summoned npc, `getStartPageId` (0 / 1011 / 10 / 1352), `onDialogSelect` BUY → `SM_TRADELIST` fields, SELL → `SM_SELL_ITEM`, a function action the npc does not support → nothing, RECOVERY price arithmetic | – | D-01..D-02 | R | M |

### Stage 1 — shop, exchange, mail, private store, cube, identification (A-01, A-02 required)

| Id | What | Java refs | Deps | Need | Eff |
|---|---|---|---|---|---|
| **T-01** | `TradeService` — all 8 (P5-09b). The AP arms call `AbyssPointsService::addAp`, which stays **U** (W) | TradeService.java:51-387 | A-01, A-02, P-01, P-02 | R | L |
| **T-02** | `ExchangeService` — the 11 unported bodies, with D7's `// java-race` marks | ExchangeService.java:43-346 | A-01, P-01, P-04 | R | L |
| **T-03** | `PrivateStoreService` — all 8 | PrivateStoreService.java:35-231 | A-01, A-02 | R | M |
| **T-04** | Tests in `tests/economy/P5-09b`: the buy/sell/buy-back ledgers against §2.10's formulas, `validateBuyItems`, an exact-kinah purchase (the `>=` of TradeList.java:56 and Storage.java:83), the private-store index semantics (PrivateStoreService.java:209), the exchange state machine incl. cancel and logout, **the double-confirm interleaving on a `DeterministicExecutor`** (D7), and a **no-duplication invariant**: after any interleaving of player-to-player transfers the sum of each item id's counts over both players is conserved | – | T-01..T-03 | R | L |
| **M-01** | `MailService` 7 + `SystemMailService` 2 (P5-09c) | MailService.java:56-281; SystemMailService.java:38-138 | A-01, M-02 | R | L |
| **M-02** | `PlayerService::getOrLoadPlayerCommonData` ×2 (P5-00; I-04) | PlayerService.java:235-247 | – | R | S |
| **M-03** | Tests in `tests/economy/P5-09c`: commission float arithmetic (Java `float`), `validateRecipient` table, online/offline recipient, attachment take order (kinah stored before it is added, MailService.java:241-251) | – | M-01 | R | M |
| **P-01** | `PlayerRestrictions::canTrade` (P5-13) — **drop if M5b-3 closed it** (A-09) | PlayerRestrictions.java:240-252 | – | R | S |
| **P-02** | `PlayerLimitService::updateSellLimit` (P5-08) + a `tests/playersvc` case with limits on | PlayerLimitService.java:22-49 | – | R | S |
| **P-03** | `RepurchaseService::repurchaseFromShop` (P5-07) | RepurchaseService.java:47-69 | A-02 | R | S |
| **P-04** | `TemporaryTradeTimeTask` (P5-07): shell from `skeleton.py`, 4 bodies, the `fieldmap.toml` / `cycles.toml` rows for `items` (it holds `Ref<Item>` until the exchange time runs out) — **drop if M5b-3 created it** (A-08) | TemporaryTradeTimeTask.java:18-63 | – | R | S |
| **P-05** | `CubeExpandService` (P5-07, 7 bodies + the handler struct) and `ExpandInventoryAction` (2) — W-09; **R** in rev 2 | CubeExpandService.java:29-123; ExpandInventoryAction.java | A-01, A-03 | R | S |
| **P-06** | Tests in `tests/itemsvc` for P-03..P-05 and P-07: `repurchaseFromShop`'s price and refusals, `TemporaryTradeTimeTask`'s expiry on a `ManualClock`, `expandCube`'s refusals (min/max level, `NPC_CUBE_EXPANDS_SIZE_LIMIT`), `identifyItem`'s 5 s task, abort, and the rolled ranges (sockets `0..option_slot_bonus`, enchant bonus `0..max_enchant_bonus`) | – | P-03..P-05, P-07 | R | M |
| **P-07** | **Identification (A-13)**: `ItemActionService` 2 + its observer and task structs, `TuningAction` 4 (P5-07) — **drop if M5b-3 took it** | ItemActionService.java:23-69; TuningAction.java | A-01, A-03 | R | S |
| **K-01** | The stage-1 client packets: `CM_BUY_ITEM`, the 6 `CM_EXCHANGE_*` (with the `CM_EXCHANGE_REQUEST` handler struct), `CM_CHECK_MAIL_LIST`, `CM_GET_MAIL_ATTACHMENT`, `CM_DELETE_MAIL` (P5-15); `CM_SEND_MAIL`, `CM_READ_MAIL`, `CM_PRIVATE_STORE`, `CM_PRIVATE_STORE_NAME` (P5-16); byte vectors incl. every audit bound (`amount > 36`, `count > 20000`, negative counts) | §2.8 | – | R | M |
| **K-02** | `CM_TUNE`, `CM_TUNE_RESULT` (with P-07) and `CM_SELECT_DECOMPOSABLE` (with E-04) (P5-16), byte vectors and run tests | CM_TUNE.java:25-55; CM_TUNE_RESULT.java; CM_SELECT_DECOMPOSABLE.java | – | R | S |
| **R-02** | `StarterKitService::onLevelUp` (P5-09a, 1 body) | StarterKitService.java:63-75 | M-01 | O | S |

### Stage 2 — crafting (+ gathering)

| Id | What | Java refs | Deps | Need | Eff |
|---|---|---|---|---|---|
| **C-01** | `CraftService` 5 (+ the `ItemUpdatePredicate` callback), `RecipeService` 3, `CraftSkillUpdateService` 4 (+ the handler struct), the 6 `Profession` functions in `ProfessionInfo.h` (P5-09c) | CraftService.java:43-258; RecipeService.java:15-73; CraftSkillUpdateService.java:78-175; Profession.java:37-87 | A-02, C-03, **I-02 (the `CraftingTask` shell)** | R | M |
| **C-02** | `CraftingTask` (P5-02a): the 9 bodies in the shell I-02 generated | CraftingTask.java:21-173 | I-02; merges **after** C-01 (its `onSuccessFinish` calls `CraftService.finishCrafting`) | R | S |
| **C-03** | `StatEnumInfo.h` with `getModifier` (P5-01, D8) | StatEnum.java:250-262 | – | R | S |
| **C-04** | `CM_CRAFT` (P5-15), `CM_RECIPE_DELETE` (P5-16), `CM_GATHER` (P5-15, D10) | CM_CRAFT.java:32-61; CM_RECIPE_DELETE.java; CM_GATHER.java | – | R / O | S |
| **C-05** | `CraftLearnAction::canAct` / `act` (P5-07) | CraftLearnAction.java | A-03, I-02 | R | S |
| **C-06** | Tests: `CraftingTask.analyzeInteraction` over a seeded `Rnd` (success/failure steps, CRIT_BLUE, speed, the morph and `skillLvlDiff < 0` arms), `calculateCrit`, `checkCraft`'s table **and its order** (materials are consumed last, CraftService.java:222-230), `finishCrafting`'s xp and level-up arithmetic, `learnSkill`'s price table, `autoLearnRecipes`' race filter | – | C-01..C-05 | R | L |

### The gate (P5-SC, `tools/oracle`, P5-14)

| Id | What | Deps | Need | Eff |
|---|---|---|---|---|
| **G-01** | `tools/oracle` command **`m5c-economy`**: vendor, postbox, Seril and cube-expander spots; **the X2 spot, whose 3D distance to 798007 lies in `[talk + R_npc + R_player, talk + 1 + R_npc + R_player)`** — the band only the "+ 1" admits (`isInTalkRange` → `isInRange(npc, player, talk + 1, false)`, which adds both bound radii and compares with a strict `<`, PositionUtil.java:243-261, 306-309) — and a second spot outside every range; each vendor's tabs and goods with template price, buy price, sell reward (from the profile's siege setting through the Java arithmetic of §2.10, using `javafloat.py`); **the trade npc type byte from `TradeNpcType`'s constructor argument** (TradeNpcType.java:12-16), never an ordinal; the starter inventory (from `m5a/creation.py`); mail commission for a given attachment; `SM_PRICES`; RECOVERY price; the cube price; the Extraction Tools price and `breakItem`'s stone-grade set and count range for the seeded weapon (EnchantService.java:48-74); the removal price; the seeded Plainsman's items' `option_slot_bonus`, `max_enchant_bonus`, manastone slots and B's class mask; the Daeva seed (advanced class, quest 1006, exp 126,069) and what its enter world must learn (the level 2-10 autolearn skills, 30002 instead of 30001, the three morph recipes); the Sanctum spots, oven static ids, Luelas' goods; recipe 155001381's components, product, xp, and the tick bounds of X20; C19's exact kinah; valid item ids above `wrap_at`. Plus `tools/oracle` tests. **It must parse the XML** — this plan's own first pass read goods list 259 as empty because of the self-closing `<list id="258"/>` before it (§12) | – | R | M |
| **G-02** | Decoders `tests/scenario/decoders/EconomyDecoders.{h,cpp}` from the Java `writeImpl`: `SM_DIALOG_WINDOW` (SM_DIALOG_WINDOW.java:29-41), `SM_PRICES` (:13-18), `SM_TRADELIST` (:57-73), `SM_SELL_ITEM` (:38-47), `SM_REPURCHASE` (:29-46), `SM_QUESTION_WINDOW` (:305-313), `SM_EXCHANGE_REQUEST`/`ADD_ITEM`/`ADD_KINAH`/`CONFIRMATION`, `SM_MAIL_SERVICE` (all six service ids, :87-178), `SM_PRIVATE_STORE`, `SM_PRIVATE_STORE_NAME`, `SM_CRAFT_UPDATE` (:37-71), `SM_CRAFT_ANIMATION`, `SM_LEARN_RECIPE`, `SM_RECIPE_DELETE`, `SM_RECIPE_LIST`, `SM_SKILL_LIST`'s full form if M5a's decoder does not cover it; the A-11 inventory decoders are M5b-3's (G-02 there); `GameSession` builders for the 23 packets and `CM_MANASTONE` / `CM_USE_ITEM` if M5b-3's builders do not take a target and a supplement; `EconomyDecodersTest.cpp` | – | R | L |
| **G-03** | `TEST(M5cScenario, Run)` — the cases of §10, `<bin>/scenario/m5c`, schema pair `aion_{ls,gs}_test_m5c_<hash>`, the shared `RESOURCE_LOCK`, `tests/scenario/m5c_partial_allowlist.txt`, `gs.scenario.m5c` in `ScenarioTests.cmake`. Written in two parts: C0-C18 and C20 (stages 0-1) in stage 2, C19 (crafting) in stage 3 | G-01, G-02, stages 0-1 (then 2) | R | L |
| **G-04** | **Re-green `gs.scenario.m5a`, `m5a_geo`, `m5b`, `m5b_geo`, `m5b2`, `m5b2_geo`, `m5b3`, `m5b3_geo`, and `gs.smoke.startup` / `startup_geo`** (A-12; `ScenarioTests.cmake:64-129` registers the first four today, m5b2-plan.md G-04 and m5b3-plan.md D12 add the geo variants). Expected movement: none of their allow-list rows closes in M5c; the risks are I-04's line shift and a §2.9 wake-up on a path they send (none is known: they send no dialog, trade, mail, manastone or tune packet). Record before/after in the wave report | G-03 | R | M |
| **G-05** | **user (D13), not on the required path.** The stress extension, written only if the user approves: 10 pairs of `FakeGameClient`s that exchange, mail and buy/sell in a loop, under ASan, in a slot the user picks; asserts **the ledger of D13** (per item id: database total = starter + Σ each client's recorded deltas; kinah with the commission sink), 0 reused-id warnings, an empty final census, 0 live `Exchange`, `ExchangeItem`, `TradeList`, `RepurchaseList`, `PrivateStore`, `TradePSItem`, `RequestResponseHandler`, `Letter` after everyone quits. Registered DISABLED like `gs.scenario.m5a_stress` (`StressTests.cmake:20-42`) | G-03, G-06, the user | O | M |
| **G-06** | `CheckOutput` (P5-14): the transfer classes above and `CraftingTask` in `zeroLiveClasses()` and the summary — **assigned once**, to stage 2's gate-1 lane (X22 needs it) | – | R | S |

### Stage 3 optional — the broker (only if the user answers D2 "now")

| Id | What | Need | Eff |
|---|---|---|---|
| B-01 | `BrokerService` 11 (P5-09b) + tests | O | M |
| B-02 | The 9 broker packets (P5-15/16) | O | M |
| B-03 | `SM_BROKER_SERVICE` decoder + gate cases in Sanctum (register, search, buy by the second character, settle) — **P5-SC work, so it goes to stage 3's single gate lane, after G-03 part 2 and G-04** | O | M |

---

## 6. Lanes

At most six lanes per stage, **chunks disjoint within a stage** (phase5-roadmap.md:50) — checked row by row below: no chunk appears in two
lanes of one stage, and each stage has **exactly one P5-SC lane**. A lane may own several chunks. A follow-up **part** (after a stage merges)
is not a lane of that stage.

| Stage | Lane | Chunks | Items | Tests | Size |
|---|---|---|---|---|---|
| **0** | **dialog** | P5-08, P4-11a, P5-05, P5-15, P5-16 | D-01..D-06 | `tests/playersvc`, `tests/objects`, `tests/handlers_ai_core`, `tests/cm_ak`, `tests/cm_lz` | ~21 bodies |
| 0 | **enhance** | P5-07 | E-01..E-05 | `tests/itemsvc` | ~39 bodies, ~1,230 Java lines — **the stage's long pole** |
| 0 | **harness-a** | P5-SC, `tools/oracle` | G-01, G-02 (dialog decoders first) | `tools.oracle`, decoder self-tests | – |
| **1** | **trade** | **P5-09b** | T-01..T-04 | `tests/economy/P5-09b` | 27 sites, ~1,050 Java lines — **the milestone's critical path** |
| 1 | **mail** | **P5-09c**, P5-00 (+ P5-09a only for the optional R-02) | M-01..M-03, R-02 | `tests/economy/P5-09c`, `tests/login_slice` | ~11 bodies |
| 1 | **packets** | P5-15, P5-16 | K-01, K-02 | `tests/cm_ak`, `tests/cm_lz` | 17 packets |
| 1 | **player-items** | P5-13, P5-08, P5-07 | P-01..P-07 | `tests/instance`, `tests/playersvc`, `tests/itemsvc` | ~25 bodies |
| 1 | **harness-b** | P5-SC, `tools/oracle` | G-01, G-02 (the rest) | as above | – |
| **2** | **craft** | P5-09c | C-01 | `tests/economy/P5-09c` | ~20 bodies |
| 2 | **craft-task** | P5-02a, P5-01 | C-02, C-03 | `tests/skills/P5-02a`, `tests/stats` | 10 bodies |
| 2 | **craft-edges** | P5-15, P5-16, P5-07 | C-04, C-05 | `tests/cm_ak`, `tests/cm_lz`, `tests/itemsvc` | ~8 bodies |
| 2 | **gate-1** | P5-SC, `tools/oracle`, P5-14 (for G-06) | G-03 part 1 (C0-C18, C20), **G-06** | `gs.scenario.m5c` | – |
| 2 | *(C-06 tests ride in the lanes above)* | | | | |
| 2+ | **fixups** — a follow-up **part after stage 2 merges**, not a concurrent lane: it owns whatever chunks gate-1's findings name, then gate-1 reruns | | findings | owning tests + gate rerun | – |
| **3** | **gate-2** — the stage's only P5-SC lane, in this order: G-03 part 2 (C19), G-04, then B-03 if D2 = now, then G-05 if the user approved it (D13) | P5-SC, `tools/oracle` | G-03 part 2, G-04, (B-03), (G-05) | `gs.scenario.m5c`, the earlier gates | – |
| 3 | *broker (optional, D2 = now)* | P5-09b, P5-15, P5-16 | B-01, B-02 | `tests/economy/P5-09b`, `tests/cm_ak`, `tests/cm_lz` | 11 + 18 |
| all | integrator | manifest, leases, profile | I-01..I-04 | full verification | – |

**Notes.**

- **Stage 0 needs A-01..A-03 from M5b-3** (the enhance lane moves items and fills `m5b3-h01` stubs); the dialog lane needs only A-10's
  answer. Rev 1's stage 0 had two lanes; rev 2 runs the enhance lane beside it because P5-07 is free in stage 0 and every one of its paths is
  reachable without a merchant — this is where the milestone gains parallelism.
- **Critical path: the trade lane** (T-01..T-04, ~1,050 Java lines of bodies, the concurrency tests), then gate-1. Rev 1 budgeted it in
  agent-days; by the measured pace (§5) the whole milestone is of the order of M5b-1 (~24 h from plan commit to gate) — an inference, not a
  measurement.
- **Merge order in stage 0:** E-01 → E-02 → E-03/E-04 → E-05; D-01 → D-02 → D-03..D-06. **In stage 1:** P-01, P-02, P-04, M-02 (small) → T-03
  and M-01 → P-05, P-07 → T-01 → T-02 → K-01, K-02 (the packets compile against the frozen headers from the first day and merge last, so no
  packet reaches an unported body in a merged tree) → T-04's concurrency test. **In stage 2:** C-03 → C-01 → C-02 (I-02 generated the
  `CraftingTask` shell before the stage, so both compile from the first day) → C-04, C-05.
- **Stage 2's gate-1 lane runs beside the crafting lanes**, as m5b2-plan.md §6 ran the gate beside npc abilities: the crafting case (C19) is
  written in stage 3 once C-* merged. G-06 is gate-1's and nobody else's.

---

## 7. Header requests expected

| Request | Kind | For |
|---|---|---|
| `AbstractItemAction`: `canAct` / `act` pure virtuals and the overrides with stubs on all 32 bound classes — **M5b-3's `m5b3-h01`** (m5b3-plan.md D6, I-01); the C++ base declares no virtual today | layout (vtable) | A-03 |
| `CraftLearnAction.h`: nothing beyond `m5b3-h01` (the script finds only `canAct`/`act` undeclared); rev 1's request drops | – | C-05 |
| `EnchantItemAction.h`: the second `act` overload's helpers `isSuccess`, `getMaxLevel`, `getMinLevel`, `isSupplementAction`, `checkSupplementLevel` (private) | additive, filed in I-02's batch before stage 0 | E-03 |
| `DecomposeAction.h`: `postValidate`, `finishUse`, `filterItemsByLevel`, `containsSpecialCubeItems`, `isValidItemId` (private) and the static reward maps the existing `.cpp` comment defers (`DecomposeAction.cpp:15-16`) | additive, same batch | E-04 |
| `TuningAction.h`: `static getRandomStatBonusIdFor` (public; `ItemActionService` calls it) | additive, same batch | P-07 |
| `model/craft/ProfessionInfo.h`: `getUpgradeCost`, `getMaxUpgradableLevel`, `getClientName` ×2, `getSkillGrade`, `getBySkillId` as free functions | additive; the chunk's own companion — **the integrator confirms whether it is frozen** (hub-headers.md §14: a hand-written enum companion is a new file only when it does not exist) | C-01 |
| New files, no request: `model/stats/container/StatEnumInfo.h` (C-03), `skillengine/task/CraftingTask.{h,cpp}` (generated by I-02), `taskmanager/tasks/TemporaryTradeTimeTask.{h,cpp}` (P-04), `handlers/ai/PostboxAI.{h,cpp}` (D-05), the `.cpp` files of the action shells M5b-3 did not already give one, 23 client packets; `fwd.h` regeneration with `skeleton.py --fwd` in the directories touched | new files | – |
| `CheckOutput` (P5-14): `Exchange`, `ExchangeItem`, `TradeList`, `RepurchaseList`, `PrivateStore`, `TradePSItem`, `RequestResponseHandler`, `CraftingTask` in `zeroLiveClasses()`, `Letter` in the summary | additive | G-06 |
| Manifest: D1 | build | I-01 |
| **None expected** in `Npc.h`, `DialogService.h`, `TradeService.h`, `ExchangeService.h`, `PrivateStoreService.h`, `MailService.h`, `SystemMailService.h`, `RecipeService.h`, `CraftService.h`, `CraftSkillUpdateService.h`, `RepurchaseService.h`, `PlayerService.h`, `PlayerRestrictions.h`, `PlayerLimitService.h`, `EnchantService.h`, `ItemSocketService.h`, `CubeExpandService.h`, `ItemActionService.h` | – | measured: 0 undeclared named methods in each (§2.8; the anonymous classes are callback structs inside the `.cpp`) |

`SM_RECIPE_LIST` iterating an unordered container is a known layout candidate (hub-headers.md §14); the gate does not assert its order, so
no request is needed.

---

## 8. Risks

Ordered by what is most likely to go wrong, with the evidence.

**Item and kinah integrity.**

1. **A throw in the middle of a transfer loses or duplicates items.** `performTrade` removes from both inventories, sends `(0)`, then adds
   to both and stores both (ExchangeService.java:248-262); `sendMail` removes the item before it stores the letter (MailService.java:130-161);
   `sellStoreItem` moves item by item and settles kinah at the end (PrivateStoreService.java:152-177). Java never throws there; a C++ body
   that reaches an unported callee (A-01, A-02 or anything §2.9 missed) throws **between the halves**. The item is then gone, or, where the
   add ran and the remove did not, doubled — and `exchanges` keeps both `Player`s alive. The same shape recurs in stage 0: `breakItem`
   deletes the weapon and then adds the stones (EnchantService.java:67-74), `removeManastone` takes the kinah before it stores the removal
   (ItemSocketService.java:124-134). **Mitigation:** K-01 merges last (§6), T-04's conservation invariant, X16's ledger and object-id checks,
   the Q8 "no `unported_trace`" bar (X22), and — only if the user approves it — G-05's ledger under load (D13).
2. **The double-confirm race (D7)** and its siblings: two buyers of one private store on two threads (`sellStoreItem` mutates the seller's
   storage from the buyers' threads); a mail recipient deleting while a sender's thread puts a letter (`Mailbox.mails` is a
   `ConcurrentHashMap` in both trees, `Mailbox.h:24`). The gate cannot produce these interleavings; T-04's deterministic test is the required
   place that can, and G-05 the optional one (D13).
3. **First writes (D11).** `InventoryDAO.store(item, recipientId)` re-owns an item row; `MailDAO.storeLetter` stores the letter row after it
   in Java's order (MailService.java:153-162) — **not for a foreign key**: the `mail` table's only foreign key is `mail_recipient_id` →
   `players` (`sql/aion_gs.sql:529-543`; rev 1 claimed one to `inventory`, which does not exist) — so the order matters only for what a crash
   between the two leaves behind. `updateOfflineMailCounter` writes `players.mailbox_letters`; `ItemStoneListDAO.storeManaStones` writes and
   deletes `item_stones` rows by persistent state. None of these has ever run against MariaDB in a gate. A wrong column order or a missing
   `UPDATE` shows up only as a character whose inventory is wrong after a relog — which is why X16 relogs both characters and reads the rows.

**Wake-up and scope.**

4. **The dialog wake-up is wide.** Stage 0 makes every talkable npc on both start maps reach `DialogService`; W-08 and W-09 are loud arms a
   real player hits within a minute of arriving in Akarios village, W-15 is 50 silent npc ids (18 on Poeta, 32 on Ishalgen). The gate does not click them; §11 says what the
   user will see.
5. **M5b-3 does not deliver A-01/A-02.** Then M5c cannot start, and the plan's stage 0 is the only part that can. Re-verify §0 at branch time.
6. **More "0-unported" packets that throw.** W-03 was found by scanning each packet's `.cpp` for callees whose definitions are unported
   (§12). The scan is name-based and noisy; every lane that constructs a packet must repeat it for that packet before claiming a path works.
7. **Sanctum is new ground (W-16), and so is a level-10 Daeva (W-20).** No automated run has ever spawned a player in a capital, or entered
   the world with a level that changed offline. Capital-only code (npcs with unported AIs, zones, the geo-disabled z of a seeded spot) and
   the enter-world `onLevelChange(old_level, 10)` (A's `old_level` is whatever C13's +1,000 exp left it at; a Gladiator's skills up to
   level 10 and their passives, the 30001 → 30002 swap, 40009 → the morph
   recipes) run for the first time in stage 3. The gate lane enters Sanctum with the seeded idle character first and reads the log before
   scripting crafting. If the passives reach an effect class M5b-2 did not port after all, that is a W-row for this plan, not a reason to
   seed differently: a real level-10 character carries the same skills.
7a. **The stage-0 item services are the least-measured part of this plan.** `EnchantService` (591 lines) was sized, not traced callee by
   callee as §2.9 did for the trade path; `enchantItemAct` and `applyEnchantEffect` reach the equipment listeners and item-set stats. The
   enhance lane runs the §12 callee scan on each of its bodies first and reports new W-rows before porting.

**Gate construction.**

8. **Prices depend on the siege setting.** 352 is 250 × 125 % × 113 % only because `gameserver.siege.enable = false` leaves every influence
   at 0. A later milestone that turns sieges on in a gate profile moves every price; G-01 therefore reads the profile, and X1 asserts
   `SM_PRICES` first so a moved premise fails one row, not twenty.
9. **Two clients, one choreography.** The exchange and the private store need A and B in each other's known list, within 5 m, same race,
   neither moving. `SM_EMOTION(OPEN_PRIVATESHOP)` reaches B only through `broadcastPacket` to sighted players; m5b-plan.md T1's
   `SM_TARGET_UPDATE` is the precedent for a broadcast a client never sees. Read `PacketSendUtility::broadcastPacket` before writing X12.
10. **The Inina seed (D5)** relies on `wrap_at` and on `InventoryDAO` loading an id above it. If either changes, the seeded item collides or
    is dropped; G-01 checks the id against the profile's `wrap_at` and the invalid-id bit pattern.
11. **`gs.scenario.m5c` is at least the ninth serialized server run** under the shared `RESOURCE_LOCK` (after `m5a`, `m5a_geo`, `m5b`,
    `m5b_geo` registered today, `ScenarioTests.cmake:64-129`, and `m5b2`, `m5b2_geo`, `m5b3`, `m5b3_geo` to come; rev 1 said sixth). Its
    own script: two characters, two first enter-worlds, **seven relogs** (B in C9, C12 and C16, A in C13, both in C14, A before C19; rev 1
    said three, the review counted six before C16's was added), two 5 s item-use tasks (C15, C18), a craft of 10-36 s (X20). Inferred
    budget: M5a's 32 s for the two-account setup (m5b-plan.md risk 15) + seven enter-worlds at ~5 s + the tasks and the craft + ~40 s of
    scripted steps ≈ **120-200 s**; the first run
    measures it, and `TIMEOUT 2700` (§10.5) leaves room. Nine serialized runs of 30-200 s each put a full `ctest -L scenario` at the
    order of 10-20 minutes (inferred; G-04 records the real figure).

---

## 9. The split: four stages, and why in this order

| Stage | What a player can do at the end | Chunks | Sites | Bodies | Lanes |
|---|---|---|---|---|---|
| **0 — talking, and the item services** | talk to every `general` npc on the start maps, open a shop's buy and sell windows, open the mailbox; the soul healer asks its price; socket a manastone, have Seril remove it, extract a weapon into enchantment stones, enchant, open a bundle (the last four without a gate case until stage 2) | P5-08, P4-11a, P5-05, P5-15, P5-16, **P5-07**, P5-SC | 28 | ~60 (dialog ~21 + enhance ~39) | **3** |
| **1 — shop, trade, mail, store** | buy potions, sell, buy back, heal soul sickness, trade with another player, send and receive mail with items and kinah, run and buy from a private store, **expand the cube, identify loot** | P5-09b, P5-09c, P5-00, P5-13, P5-08, P5-07, P5-15, P5-16, P5-SC | 50 (+1 O) | ~99 (50 + 17 packets + `TemporaryTradeTimeTask` + the exchange handler + cube and identification invisible bodies) | **5** |
| **2 — crafting** | learn cooking, learn its recipe, buy Salt, craft Roast Inina at an Oven, delete a recipe, gather; the gate proves stages 0-1 | P5-09c, P5-02a, P5-01, P5-15, P5-16, P5-07, P5-SC, P5-14 | 12 | ~36 (+4 with `CM_GATHER`) | **4** + a fixups part |
| **3 — proof** | the crafting case, every earlier gate green; the broker if the user wants it; the stress run only in a slot the user picks | P5-SC (+ P5-09b, P5-15/16 for the broker) | 0 (+11) | ~2 | **1-2** |

1. **Stage 0 first because it has a green point without a single trade** and because everything else stands on it. Its dialog lane is the
   only work M5b-3's state does not decide; its enhance lane needs A-01..A-03 and nothing from M5c.
2. **Stage 1 before crafting** because crafting needs buying (Salt), the question window (the master), `ItemService` and the item packets —
   stage 1 proves all four on cheaper paths. The reverse order would debug a crafting task against an unproven shop.
3. **Stage 1 is one stage, not three,** because its four features share the packets lane and the harness, and the exchange and the store need
   the same two-client choreography. Six lanes would fit; five are enough.
4. **The gate is split across stages 2 and 3** so that a trade or mail bug is found while the crafting lanes run, not after them.
5. **The M5b-3 hand-off rides in stages 0 and 1, not in a stage of its own**, because P5-07 is free in stage 0 and its paths need no
   merchant; a separate stage would have added a serial step for work that can run beside the dialog lane. The price is a stage 0 whose
   enhance lane is longer than its dialog lane.

**What this plan does not claim.** It does not claim the milestone is small because it has ~195 bodies: M5b-1 had fewer and found eight
hidden prerequisites. §2.9 names twenty-three wake-ups; W-03, W-05, W-06, W-17, W-19 and W-20 are ones no count would have found. Rev 1
missed W-06's live path and W-18 (the review found them) and W-17, W-19, W-20, W-22, W-23 (this revision found them while reconciling the
hand-off) — the same lesson as M5b-1's eight, one plan later.

---

## 10. Gate specification (`ctest -L scenario`, `gs.scenario.m5c`)

### 10.1 Processes, databases and profile

Identical to m5b2-plan.md §10.1 except:

| Piece | M5c |
|---|---|
| Schemas | `aion_ls_test_m5c_<hash>` / `aion_gs_test_m5c_<hash>`, same `SchemaLease` and sweep |
| Output directory | `<bin>/scenario/m5c`, its own `gs_log` and `ls_run` |
| `RESOURCE_LOCK` | the shared `"aion_game_server_log;aion_login_server_log"` |
| Profile | the M5b-3 set (M5b-2's: `siege`, `autogroup`, `rift`, `vortex`, `worldraid`, `cp` disabled, `limits` disabled, `missing_ai_handlers = warn`, events off, npc shouts off; M5b-3's drop keys as m5b3-plan.md D4 sets them for gates other than its own, i.e. `gameserver.rates.drop = 0`) **plus** `gameserver.craft.fail.chance = 0`, `gameserver.rates.crafting.crit_chances = 0, 0` and `gameserver.rates.manastone_chances = 200, 200` (D6). Written out as `game-server/config/m5c.properties.example` (I-03). **Verify first that the `float[]` property accepts "0, 0"** |
| Allow-list | `tests/scenario/m5c_partial_allowlist.txt`: **§A is copied at branch time from the §A of `m5b2_partial_allowlist.txt` and `m5b3_partial_allowlist.txt`** (the rows every gate's startup and enter world hit), not listed here — rev 1 listed six rows, but the M5a list already has ten (with `GeneralNpcAI.cpp:122`, `NpcSkillList.cpp:91`, `SkillEngine.cpp:137`) and the M5b list more, and lines move (`SkillEngine`'s partial sits at :137 in today's tree); §B nothing new; §C the timing rows |
| Characters | **A** an Elyos WARRIOR on account A, **B** an Elyos MAGE on account B — both online at once (the M5a gate already runs two accounts concurrently, `M5aScenarioTest.cpp:1298-1307`) |
| Seeds (D5) | before the first enter world: both characters at the oracle's Akarios gate spot (the X2 spot); before C13: A's `recoverexp`; in C14's offline window: B's kinah (the oracle's sum of C16-C18's prices) and B's three items for C15-C18 (an unidentified Plainsman's armour piece B can wear, with a manastone slot and `max_enchant` > 0; a Plainsman's weapon; a start-map manastone); before C19: **A as a Daeva** (`player_class = 'GLADIATOR'`, a `player_quests` row (1006, `COMPLETE`), `exp` = 126,069), Sanctum position, kinah = **3,640** (exact, §2.10), and the Inina row |

### 10.2 Cases

| # | Case | Steps |
|---|---|---|
| **C0** | the oracle answers | `oracle.py m5c-economy` returns every constant below |
| **C1** | setup | M5a cases 1-4 for both accounts (login, create A and B, seed positions, enter world, level ready) |
| **C2** | prices | read `SM_PRICES` from both enter-world bursts |
| **C3** | talking | A: `CM_SHOW_DIALOG(798007)` from the X2 spot (inside the "+ 1" band, G-01), then from 10 m, `CM_CLOSE_DIALOG`; B: walk to the postbox, `CM_SHOW_DIALOG(700000)`, then walk back beside A (C8 needs 5 m). B does not need to keep the postbox open: C9's relog resets it anyway (C11) |
| **C4** | a shop's windows | A: `CM_DIALOG_SELECT(798007, BUY = 2)`, then `(SELL = 3)` |
| **C5** | buying | A: `CM_BUY_ITEM(798007, 13, [(162000052, 2)])`; then an unlisted item `(162000002, 1)`; then one more elixir it cannot afford |
| **C6** | selling | A: `CM_BUY_ITEM(798007, 1, [(potion stack, 10)])`; then the juice |
| **C7** | buying back | A: `CM_DIALOG_SELECT(798007, BUY_AGAIN = 70)`; `CM_BUY_ITEM(798007, 2, [(repurchase object, 10)])` |
| **C8** | an exchange | A: `CM_EXCHANGE_REQUEST(B)`; B: `CM_QUESTION_RESPONSE(90001, 1)`; A adds 10 potions and 100 kinah; B adds 5 bandages; A tries the juice; both LOCK; A OK; B OK |
| **C9** | a cancelled exchange, and a partner who quits | a second exchange; A adds 10 potions; B `CM_EXCHANGE_CANCEL`. A third one; B `CM_QUIT(0)` mid-exchange; B re-enters |
| **C10** | a private store | A (the seller — B holds the kinah after C8): `CM_PRIVATE_STORE([(potion stack, 162000002, 5, 100)])`, `CM_PRIVATE_STORE_NAME("m5c")`; B: `CM_DIALOG_SELECT(A, BUY)`, `CM_BUY_ITEM(A, 0, [(0, 3)])`, then `[(0, 2)]` |
| **C11** | mail, online | A and B walk to the postbox. **B opens it first** — `CM_SHOW_DIALOG(700000)` — because C9's relog gave B a new `Mailbox` whose state is 0 (MailService.java:265-267 → MailDAO.java:34 `new Mailbox(player)`; Mailbox.java:25 `mailBoxState = 0`), and without it `updateRecipientMailbox` would skip the refresh (SystemMailService.java:129). Then A opens it and sends `CM_SEND_MAIL(B, "m5c", "gate", potion stack, 5, 200, NORMAL)`; B: `CM_CHECK_MAIL_LIST(0)`, `CM_READ_MAIL`, `CM_GET_MAIL_ATTACHMENT(0)`, `(1)`, `CM_CHECK_MAIL_LIST(0)` again, `CM_DELETE_MAIL`, `CM_CLOSE_DIALOG(700000)`; A sends a second letter carrying only 10 kinah |
| **C12** | mail, offline, and a wrong name | B quits; A sends a third 10-kinah letter to B, then one to a name that does not exist; B re-enters. (The oracle's ledger keeps A above the 249 kinah C13 needs: 1,000 → 296 → 196 → 696 → 399 with §2.10's prices) |
| **C13** | soul healing | A quits; seed `recoverexp = 1000`; A re-enters, walks to 203064, `CM_DIALOG_SELECT(203064, RECOVERY = 35)`, `CM_QUESTION_RESPONSE(160011, 1)` |
| **C14** | persistence | both quit; read the database; seed B for C15-C18; both re-enter; read `SM_INVENTORY_INFO` |
| **C15** | identification (P-07, A-13) | B: `CM_TUNE(armour, 0)`; wait 5 s; `CM_EQUIP_ITEM(armour)` (A-05), then unequip it |
| **C16** | a manastone, and Seril (E-01, E-02, E-03) | B: `CM_MANASTONE(2, 0, armour, stone, 0)`; **B quits and re-enters** (so the socket is stored as a row before it is removed — otherwise the removal deletes a row that never existed); walk to Seril (11.1 m), target it (`CM_TARGET_SELECT`, as M5b's gate targets), `CM_SHOW_DIALOG(203336)`, `CM_DIALOG_SELECT(203336, REMOVE_ITEM_OPTION = 42)`, `CM_MANASTONE(3, 0, armour, slot 0, 203336)` |
| **C17** | the cube (P-05) | B: walk to 798008, `CM_SHOW_DIALOG(798008)`, `CM_DIALOG_SELECT(798008, EXTEND_INVENTORY = 47)`, `CM_QUESTION_RESPONSE(900686, 1)` (`STR_WAREHOUSE_EXPAND_WARNING`, SM_QUESTION_WINDOW.java:194; the cube reuses the warehouse question, CubeExpandService.java:60-63) |
| **C18** | extraction and enchanting (E-01, E-03) | B: back at 798007, `CM_DIALOG_SELECT(798007, 2)`, `CM_BUY_ITEM(798007, 13, [(165000001, 1)])` — the last kinah B has; `CM_USE_ITEM(tools, target = weapon)`; wait 5 s; `CM_MANASTONE(1, 0, armour, one of the new stones, 0)` |
| **C19** | crafting | A quits; seed the Daeva, Sanctum, kinah 3,640, one Inina; A enters Sanctum (the enter world runs `onLevelChange(old_level, 10)` with the level C14's quit stored, W-20); `CM_SHOW_DIALOG(203784)`, `CM_DIALOG_SELECT(203784, 46)`, `CM_QUESTION_RESPONSE(900852, 1)`; walk to Luelas, buy 2 Salt (exactly the last 140 kinah); walk back; `CM_CRAFT(0, 150000009, 155001381, oven, {152001001: 1, 169400096: 2}, 0)` from 7 m, from 12 m, then from 3 m; wait for the end; `CM_RECIPE_DELETE(155001381)`; quit and read the rows |
| **C20** | reports and shutdown | the M5a Q8 bar plus the M5c rows |

### 10.3 Assertions

Every row states what it proves and what it cannot; the constants are the oracle's (§2.10 shows the values it is expected to produce).

| # | Case | Assertion | Proves / cannot prove | Mutation it kills |
|---|---|---|---|---|
| **X1** | C2 | `SM_PRICES` is (125, 100, 113) for both characters | **Proves** `Influence` and `PricesService` under the gate's siege setting — the premise of every price below. **Cannot** prove siege-influenced prices | `getTaxes` rounding half down (112); an influence default of 0.5 (100/100) |
| **X2** | C3 | From the X2 spot — whose distance to 798007 G-01 puts in the band only the "+ 1" admits (`[talk + R_npc + R_player, talk + 1 + R_npc + R_player)`, PositionUtil.java:243-261, 306-309): `SM_DIALOG_WINDOW(798007, page 10, quest 0, 0, 0)` and nothing else; from 10 m: `STR_DIALOG_TOO_FAR_TO_TALK` and no `SM_DIALOG_WINDOW` | **Proves** `CM_SHOW_DIALOG` → `onDialogRequest` → `TalkEventHandler` → `getStartPageId` → `isInteractionAllowed` for a function npc, and the talk-range arithmetic at its edge. **Cannot** prove the client draws the buttons, nor quest pages (no quest registered) | `isInteractionAllowed` → false (page 1011); `isInTalkRange` without its "+ 1" (the band spot is refused); `centerToCenter = true` (the bound radii dropped: refused too) |
| **X3** | C3, C11 | The postbox answers `SM_DIALOG_WINDOW(700000, page 18, …)` whose last `writeH` is **1** (REGULAR) — in C3, and again when B reopens it in C11. In C11, A's first letter reaches B (postbox reopened after the C9 relog) as `SM_MAIL_SERVICE(0)` **followed by a list refresh `SM_MAIL_SERVICE(2)`**; A's second letter, sent after B's `CM_CLOSE_DIALOG`, reaches B as `SM_MAIL_SERVICE(0)` **only** (SystemMailService.java:126-132) | **Proves** `PostboxAI` is registered and sets the state, and `onCloseDialog` clears it (DialogService.java:64-66). **Cannot** prove the client opens its mail window, nor that a relog resets the state (a faithful port does, MailDAO.java:34; the gate only reopens) | `PostboxAI` unregistered (no packet at all); the state left 0 (no refresh for the first letter); `onCloseDialog` not clearing it (a refresh for the second) |
| **X4** | C4 | `SM_TRADELIST`: npc type **1** (`NORMAL.index()`, the oracle's byte, TradeNpcType.java:12; SM_TRADELIST.java:59), modifier 100, 100, buy tab 1, sell tab 1, tabs = the oracle's (132, 720), 0 limited items. `SM_SELL_ITEM`: type **1** (the NORMAL fallback, SM_SELL_ITEM.java:30, 40), rate 20, 1, 1, no tabs | **Proves** `Npc::canSell`/`canBuy`/`canPurchase` (W-03), tab filtering, the vendor modifiers, the npc type byte. **Cannot** prove the client's price display | `canSell` → false (buy tab 0); SELL answered with `SM_TRADELIST`; `ordinal()` written instead of `index()` (0) |
| **X5** | C5 | After the buy: exactly one kinah update to 1000 − 2 × 352 = **296**, and `SM_INVENTORY_ADD_ITEM` for 162000052 count **2** | **Proves** `getBuyPrice` through all four factors, `performBuyTransaction`, `ItemService.addItem(BUY)`. **Cannot** prove AP or reward vendors | taxes skipped (624 → 376 left); the count multiply dropped (648); kinah charged twice |
| **X6** | C5 | Unlisted item: `STR_BUY_SELL_USER_BUY_FAILED`, no kinah or item packet. Unaffordable elixir: `STR_MSG_NOT_ENOUGH_MONEY`, no change | **Proves** `validateBuyItems` and the kinah check precede any change. **Cannot** catch `>=` → `>` in the kinah checks (TradeList.java:56, Storage.java:83): C5's buys are never exact; **X17 and X27 are the rows that kill it**, with an exact-kinah Salt and tools purchase | `validateBuyItems` → true |
| **X7** | C6 | Selling 10 potions: kinah **+500**, the stack 100 → 90 by `SM_INVENTORY_UPDATE_ITEM`; the juice: `STR_BUY_SELL_ITEM_CAN_NOT_BE_SELLED_TO_NPC`, no change | **Proves** `performSellToShop`, `getSellReward(…, 20)`, `updateSellLimit`'s disabled arm (W-04), `isSellable`. **Cannot** prove the sell-limit arithmetic (limits off; P-02's unit test) | the vendor-sell modifier 100 (+2,500); `delete` instead of `decreaseItemCount` (stack gone); `isSellable` skipped |
| **X8** | C7 | `SM_REPURCHASE` lists **exactly the last sale**: one entry, 162000002, count 10, price 500. After the buy-back: kinah −500, the stack back to 100 | **Proves** `RepurchaseService` (the set is replaced per sale, RepurchaseService.java:28-30), `repurchaseFromShop`, `ItemService.addItem(player, item)`. **Cannot** prove the list across two sales unless G-01 adds a second sale | repurchase price = template price (250); repurchase without paying |
| **X9** | C8 | The whole choreography, in order: B `SM_QUESTION_WINDOW(90001, A's name)`; `SM_EXCHANGE_REQUEST` to both; A's fake stack update to 90 and `SM_EXCHANGE_ADD_ITEM(0, 162000002)`, B's `(1, 162000002)`; `SM_EXCHANGE_ADD_KINAH(100, 0/1)`; **no** `SM_EXCHANGE_ADD_ITEM` for the juice to either; `CONFIRMATION(3)` to each partner on LOCK; **`(2)` to the partner on each OK** — on A's OK to B, on B's OK to A, sent unconditionally before the partner check (ExchangeService.java:227-231) — and on the second OK, after its `(2)`, `(0)` to both | **Proves** `CM_EXCHANGE_REQUEST` → the handler struct → `CM_QUESTION_RESPONSE` → `registerExchange` → the add/lock/confirm state machine and the tradeable check (W-07). **Cannot** prove the double-confirm race (D7) | `isTradeable` check dropped (juice appears); `(2)` sent to self; `performTrade` on the first OK |
| **X10** | C8 | The ledgers after the trade, from packets: A potions 90, bandages 25, kinah −100; B potions 110, bandages 15, kinah +100 | **Proves** `removeItemsFromInventory` and `putItemToInventory` on both sides. **Cannot** prove persistence (X16) | giver and partner swapped in `putItemToInventory`; kinah added twice; items not removed (a dupe: B +10 and A still 100) |
| **X11** | C9 | Cancel: A's items come back (`GET_BACK` packets), B gets nothing, A gets `CONFIRMATION(1)`, the ledgers are unchanged. Quit: A gets `CONFIRMATION(1)`; after B re-enters, a new exchange can be requested | **Proves** `cancelExchange` from both entry points and that `exchanges` was cleaned (a stale entry makes `isTrading` refuse the next request). **Cannot** prove the census; that is X22 | `returnItems` skipped; `cleanUpExchanges` skipped on the logout path |
| **X12** | C10 | B (and A) receive `SM_EMOTION(A, OPEN_PRIVATESHOP = 33)` and `SM_PRIVATE_STORE_NAME(A, "m5c")`; `SM_PRIVATE_STORE`: seller A, 1 entry, count 5, price 100; after `[(0, 3)]`: B −300 kinah +3 potions, A +300 −3 and `STR_MSG_PERSONAL_SHOP_SELL_ITEM_MULTI`; after `[(0, 2)]`: `SM_EMOTION(A, CLOSE_PRIVATESHOP = 34)` | **Proves** the store's life cycle and the index semantics. **Cannot** prove two concurrent buyers (risk 2) | index read as an object id (nothing bought); the store not closed when empty; the price not multiplied by the count |
| **X13** | C11 | A: `SM_MAIL_SERVICE(1, MAIL_SEND_SUCCESS)`, kinah −**251**, the stack −5. B: `SM_MAIL_SERVICE(0)` total 1 unread 1. B's list: one letter, sender A, unread, attachment 162000002, kinah 200, type 0; read → `(3)`; item → +5 potions and `(5, id, 0)`; kinah → +200 and `(5, id, 1)`; a second `CM_CHECK_MAIL_LIST` shows the letter read, with attachment 0 and kinah 0; delete → `(6)` with 0 letters | **Proves** `sendMail`'s commission (float + service price), `updateRecipientMailbox` online, list, read, both attachments, delete. **Cannot** prove express mail (D9) | quality rate 0.2 (commission 250); `getPriceForService` skipped (37 + 200); the attachment not removed from the letter (taken twice) |
| **X14** | C12 | With B offline: the third letter stores a `mail` row and raises `players.mailbox_letters` for B by 1; B's enter-world burst has `SM_MAIL_SERVICE(0)` with the second and third letters unread (2) and the character list's unread flag 1. The wrong name: `SM_MAIL_SERVICE(1, NO_SUCH_CHARACTER_NAME)`, no kinah change | **Proves** `getOrLoadPlayerCommonData` for an offline name (W-05), `updateOfflineMailCounter`, `loadPlayerMailbox`, `MailDAO.haveUnread`. **Cannot** prove the 100-letter limit | the offline counter not written; `getOrLoadPlayerCommonData` online-only (the letter bounces) |
| **X15** | C13 | `SM_QUESTION_WINDOW(160011, "249")`; after yes: kinah −**249**, exp +1000, `STR_SUCCESS_RECOVER_EXPERIENCE`; after the next quit `players.recoverexp` = 0 | **Proves** the RECOVERY arm, its callback struct and the npc-requester question path. **Cannot** prove the soul-sickness removal unless A is sick (M5b-2's X10 has the death) | the factor's sign; `resetRecoverableExp` not called |
| **X16** | C14 (and every later quit) | The database after both quit: **per item id, the sum of A's and B's `inventory` counts equals the oracle's ledger** (starter items + every buy, sale, buy-back, exchange, store sale, mail and commission of C5-C13); the mailed potions' row belongs to B; `mail` holds only the two unread kinah letters; **no ERROR from `InventoryDAO` or `MailDAO` in `gs_log`** (a duplicate `item_unique_id` insert fails there — the key is the table's primary key, `sql/aion_gs.sql:380`); and throughout C8-C13 **no object id is ever in both clients' inventory models** (built from every item packet each client received); after re-entry `SM_INVENTORY_INFO` equals the same ledger | **Proves** the first writes (D11) and that no transfer duplicated or lost an item. **Cannot** prove a concurrent duplication (T-04's deterministic test; G-05 if the user approves it) | a transfer that re-adds to the recipient without removing from the giver (the object id in both models; the per-id sum one stack too high); `InventoryDAO.store(item, recipientId)` given the giver's id (the potions back with A after relog: the ledger fails for both); the recipient's store skipped (the potions vanish after relog) |
| **X17** | C19 | Learning: `SM_QUESTION_WINDOW(900852, profession name, "3500")`; after yes kinah −3,500, `SM_SKILL_LIST` with 40001 level 1, **exactly one** `SM_LEARN_RECIPE(155001381)`; the Salt: −2 × 70, **leaving 0** (the seed is exact, §2.10) | **Proves** the `COMBINE_SKILL_LEVELUP` arm (which a level-9 character would silently skip, CraftSkillUpdateService.java:83-84), `Profession.getUpgradeCost`, `addSkill` → `onLearnSkill` → `autoLearnRecipes` (W-06) with its race filter, and a purchase of exactly the last kinah. **Cannot** prove the expert/master arms | `getUpgradeCost(0)` wrong; the race filter dropped (a second `SM_LEARN_RECIPE(155006386)`); **`>=` → `>` in `calculateBuyListPrice` or `tryDecreaseKinah`** (the Salt refused with `STR_MSG_NOT_ENOUGH_MONEY`) |
| **X18** | C19 | From 12 m: nothing (the packet's 10 m check). From 7 m: `STR_COMBINE_TOO_FAR_FROM_TOOL` and `SM_CRAFT_UPDATE(action 4)`, **Inina and Salt unchanged** | **Proves** both range checks and that `checkCraft` consumes materials only at its end. **Cannot** prove the other `checkCraft` refusals (C-06) | the 5 m check dropped; materials consumed before the checks |
| **X19** | C19 | From 3 m: `SM_CRAFT_UPDATE(0)` then `(1)`, `SM_CRAFT_ANIMATION(…, 40001, 0)` and `(…, 1)`, progress updates, `SM_CRAFT_UPDATE(5)` and `SM_CRAFT_ANIMATION(…, 2)`; then +2 × 160001001, Inina −1, Salt −2, `SM_SKILL_LIST` 40001 **level 2**, exp +141 × the crafting rate | **Proves** `CraftingTask` end to end with `finishCrafting`'s arithmetic. **Cannot** prove crits (disabled, D6) or failure (C-06) | the product from `getComboProduct(0)`; materials consumed twice; the level-up threshold |
| **X20** | C19 | Between the start and `(5)` there are **between 4 and 14** progress updates, at 2,500 ms intervals | **Proves** the interval (`2500 − 60 × Δ`, cap 1200) and the step arithmetic (70 + bonus, CRIT_BLUE up to +280, and `multi` in [1, 2), CraftingTask.java:131). **Cannot** prove the exact count (random CRIT_BLUE and `multi`) | interval 200 (the morph value) for cooking; the step without its 70 minimum (> 14 updates) |
| **X21** | C19 | `CM_RECIPE_DELETE` → `SM_RECIPE_DELETE(155001381)`; after the quit `player_recipes` has no 155001381 row and `player_skills` has 40001 at level 2 | **Proves** `RecipeList.deleteRecipe` → `PlayerRecipesDAO.delRecipe`, and the skill level's persistence | the DAO call skipped (the recipe returns after relog) |
| **X21a** | C19 (enter world) | A enters as a **level-10** Gladiator (the level of `SM_STATS_INFO` / `SM_STATUPDATE_EXP`, the oracle's); the enter-world skill list holds 30003, 40009 and 30002 and **not** 30001, plus the oracle's Warrior (levels 1-9) and Gladiator (9-10) autolearn skills; the burst carries **exactly three** `SM_LEARN_RECIPE` — 155000001, 155000002, 155000005 (the Elyos morph recipes, `recipe_templates.xml:3-27`); after the quit `player_recipes` holds those three (and 155001381 is gone, X21) | **Proves** the Daeva level rule (PlayerCommonData.java:276-281, 588-610), the enter-world `onLevelChange` for an offline level change (W-20), and `learnNewSkills` → `onLearnSkill` → `autoLearnRecipes` for a **morph** skill (W-06). **Cannot** prove an online level-up past 9 (M5e's class change) | `updateDaeva` not consulting the quest list (level 9: this row and X17 fail); `isMorphSkill` dropped from `onLearnSkill`'s condition (no morph recipe); the 30001 → 30002 swap skipped; `autoLearnRecipes`' race filter dropped (three Asmodian morph recipes 155005001, 155005002, 155005005 as well) |
| **X22** | C20 | The Q8 bar: `unported_trace.txt` empty, `partial_trace.txt` ⊆ the allow-list with §A hit ≥ 1 and §B 0, no ERROR in either log, lockdep empty; `live_counts.txt`: `Exchange`, `ExchangeItem`, `TradeList`, `RepurchaseList`, `PrivateStore`, `TradePSItem`, `CraftingTask`, `RequestResponseHandler` live 0 with `created > 0`, `Letter` live 0 (nobody online at the end) | **Proves** nothing on the scripted path fell outside the port, and every transfer object was reclaimed. **Cannot** prove paths off the script (W-08, W-15, W-21 are not clicked; the bundle of W-23 is unit-tested only, E-05) | a `cleanUpExchanges` that forgets one side; a `RequestResponseHandler` left in `activeRequests` |
| **X23** | C15 | `SM_ITEM_USAGE_ANIMATION(B, armour, id, 5000, 9, 0)`; ~5 s later `(…, 0, 10, 0)`, `SM_INVENTORY_UPDATE_ITEM(armour)` whose sockets are in `[0, option_slot_bonus]` and enchant bonus in `[0, max_enchant_bonus]` (the oracle's, from the template) and tune count **0**, `STR_MSG_ITEM_IDENTIFY_SUCCEED`; then the equip succeeds (A-05's equip packets), where before identification it would have been refused (Equipment.java:163-167); after C20's quit `inventory.tune_count` = 0 | **Proves** `CM_TUNE` → `identifyItem` → its task → `TuningAction.getRandomStatBonusIdFor` (A-13, W-19), and that identification unlocks equipping. **Cannot** prove tuning with a scroll or `CM_TUNE_RESULT` (P-06's unit tests) | `setTuneCount` not incremented (the equip refused); the task scheduled with delay 0 (the two animations together); the sockets rolled from `max_enchant_bonus` |
| **X24** | C16 | `CM_MANASTONE(2)`: the stone's stack −1, `SM_INVENTORY_UPDATE_ITEM(armour)` carrying the stone in slot 0, the success message; no randomness (D6: chance 200, uncapped); after B's quit **one `item_stones` row** for the armour, slot 0, and B's re-entry shows the stone in the armour's item info | **Proves** `EnchantItemAction` (arm 2) → `socketManastone` → `socketManastoneAct` → `ItemSocketService.addManaStone`, and the insert path of `ItemStoneListDAO`. **Cannot** prove the failure arm, fusion sockets or amplification (E-05) | the stone not consumed; the stone put in slot 1; a cap on the chance (a random refusal); the stone never stored (no row) |
| **X25** | C16 | At Seril: `SM_DIALOG_WINDOW(203336, page 10)`; after `CM_DIALOG_SELECT(42)` `SM_DIALOG_WINDOW(203336, page **20**)` (`REMOVE_MANASTONE`, DialogPage.java:37); after `CM_MANASTONE(3)`: kinah −**917**, `STR_REMOVE_ITEM_OPTION_SUCCEED`, the armour updated without its stone; **at once** (the removal writes immediately, ItemSocketService.java:125-129) the `item_stones` row is gone | **Proves** `sendDialogWindow` for a function npc (W-18), arm 3's target and talk-range check (CM_MANASTONE.java:90-94), `removeManastone`'s price and the `DELETED` path of `ItemStoneListDAO.storeManaStones`. **Cannot** prove the fusion-socket arm | the page id sent as the action id (42); the price without taxes (812); the `DELETED` state not written (the stone back after relog) |
| **X26** | C17 | `SM_QUESTION_WINDOW(900686, "1000")`; after yes: kinah −**1,000**, `SM_CUBE_UPDATE` with the npc expansion count 1; after C20's quit `players.npc_expands` = 1 | **Proves** `EXTEND_INVENTORY` → `expandCube` → its handler struct → `npcExpand` (W-09) and the `npc_expands` write. **Cannot** prove ticket or quest expansion (P-06) | the price read from the wrong level (`getPrice(0)`: no price, a refusal message); `npc_expands` not stored |
| **X27** | C18 | The tools: kinah −**1,412**, **leaving 0** (C14's seed is exact); `CM_USE_ITEM`: `SM_ITEM_USAGE_ANIMATION(…, 5000, 0, 0)`; ~5 s later `SM_DELETE_ITEM(weapon)`, the tools −1, `STR_DECOMPOSE_ITEM_SUCCEED`, `SM_INVENTORY_ADD_ITEM` of **one** stone id in the oracle's grade set with a count in **[2, 5]** (EnchantService.java:52-74), the animation's result 1 | **Proves** `ExtractAction` (W-17) → its task → `breakItem`, and a second exact-kinah purchase. **Cannot** prove the exact grade and count (random) | the weapon not deleted (a duplication: X16's model check catches it too); the armour count range `[1, 3]` used for a weapon; the tools not consumed; `>=` → `>` in the kinah checks |
| **X28** | C18 | `CM_MANASTONE(1)` on the armour: the stone −1 and **exactly one of** Java's two outcomes — success: enchant level in {1, 2, 3} (capped at `max_enchant` + the enchant bonus) and `STR_MSG_ENCHANT_ITEM_SUCCEED_NEW`; failure: level 0 and `STR_ENCHANT_ITEM_FAILED`, the armour kept (enchant type 0, the oracle checks) (EnchantService.java:173-231); after C20's quit `inventory.enchant` equals the level seen | **Proves** arm 1 → `enchantItem` → `enchantItemAct` → `setEnchantLevel` and its persistence. **Cannot** prove either arm deterministically (the chance is capped at 80 %, EnchantService.java:126-127) — E-05 does | the stone not consumed on failure; success without `setEnchantLevel` (the level 0 after relog); the item deleted on a type-0 failure |

### 10.4 Mutation proof (the standard)

Each row above must be watched failing, with the mutation and both outputs quoted. The minimum set, including what the gate cannot catch:

| Mutation | Must fail | Must stay green |
|---|---|---|
| `PricesService::getTaxes`: `Math.round` → truncation (113 → 112) | X1, X5 (349 instead of 352), X17 (Salt 69 instead of 70, so kinah is left over), X25 (909 instead of 917), X27 (1,400 instead of 1,412) | X7, X8, X15, X26 (the cube price is raw), and **X13** — the mail cost truncates to 51 either way (46 × 1.12 = 51.52), so X13 cannot be the row that catches it |
| `TradeList::calculateBuyListPrice` or `Storage::tryDecreaseKinah`: `>=` → `>` | X17, X27 (the exact-kinah purchases) | X5, X6 |
| `SM_TRADELIST` / `SM_SELL_ITEM`: `ordinal()` written for `index()` | X4 | X2 |
| `PositionUtil::isInTalkRange`: drop the "+ 1" | X2 (the band spot) | X1 |
| `Npc::canSell` → `false` | X4 | X2 |
| `DialogService::isInteractionAllowed` → `false` | X2, X4 | X1 |
| `PlayerCommonData::updateDaeva`: ignore the quest list | X21a (level 9), X17 (Hestia silent) | X1-X16 |
| `SkillLearnService::onLearnSkill`: drop `isMorphSkill()` from the recipe condition | X21a | X17 |
| `ItemActionService::identifyItem`: do not increment the tune count | X23 (the equip refused) | X24 |
| `ItemSocketService::removeManastone`: skip `storeManaStones` | X25 (the row survives) | X24 |
| `EnchantService::breakItem`: skip `inventory.delete(targetItem)` | X27, X16 (the weapon's object id stays) | X26 |
| `CubeExpandService`: expand without storing the npc expansion count | X26 | X25 |
| `TradeService::performSellToShop`: `delete` for every sale | X7, X8, X16 | X5 |
| `ExchangeService::addItem`: drop the tradeable check | X9 | X10 |
| `ExchangeService::removeItemsFromInventory`: skip the giver's removal | X10, X16 | X9 |
| `ExchangeService::confirmExchange`: call `performTrade` without checking the partner | X9 | X11 |
| `MailService::getAttachments`: leave the item on the letter | X13 (the second list still shows it) and X16 | X14 |
| `SystemMailService::updateRecipientMailbox`: skip `updateOfflineMailCounter` | X14 | X13 |
| `CraftService::checkCraft`: consume materials first | X18 | X19 |
| `CraftingTask::analyzeInteraction`: drop the 70 minimum step | X20 | X19 |
| `RecipeService::autoLearnRecipes`: drop the race filter | X17 | X19 |
| **the double-confirm interleaving** (D7) | **nothing in the gate** — T-04's deterministic test (and G-05, if the user approves it, D13) | `gs.scenario.m5c` green |
| `EnchantService::enchantItem`: the 80 % cap removed | **nothing in the gate** (X28 accepts both outcomes) — E-05 | green |
| `PlayerLimitService::updateSellLimit` arithmetic | **nothing in the gate** (limits off) — P-02's test | green |
| `CraftingTask::calculateCrit` | **nothing in the gate** (crits off) — C-06 | green |
| an `Exchange` kept in `exchanges` deliberately | X22 (and X11's second request) | X10 |

### 10.5 CTest wiring

`gs.scenario.m5c`, `LABELS "scenario;realdata"`, `TIMEOUT 2700`, the shared `RESOURCE_LOCK`, in `ScenarioTests.cmake`; no geo variant (D12).

---

## 11. Real-client checklist (user, after stage 3)

Prerequisites as m5b-plan.md §10 steps 1-6 with `mygs.properties` from `m5c.properties.example` **minus** the two crafting keys and the
manastone key (keep defaults for a real session: sockets can then fail) and with geo on. Everything below assumes M5b-3's
checklist passed (§0).

1. Make an Elyos character and walk about 420 m from the start to Akarios village, where the vendors stand around (850, 1250).
   **Clicking the obelisks and the quest objects on the way still does nothing** — their AIs are not ported (W-15); that is not a
   regression.
2. Talk to **Minalinerk** (a Shugo next to the other merchants). The dialog shows Buy and Sell.
3. **Buy**: the list has Minor Power Shard, Extraction Tools, Bandage, Minor Life Elixir and Minor Mana Elixir. The elixir should cost
   **352 kinah** (sieges are off: 125 % prices, 113 % tax). Buy two: your kinah drops by exactly 704.
4. Drink one (only if M5b-3 delivered A-03/A-04; otherwise skip — it will log an ERROR).
5. **Sell** ten Minor Life Potions: +500 kinah. Try to sell *Mercenary's Fruit Juice*: the client says it cannot be sold.
6. **Buy back** the potions from the buy-back tab: −500.
7. Visit Mune, Amus and Uno: their lists open. Buy a Mercenary Shield and equip it (equip is A-05).
8. **Do not expect** the flight master (Kustanon) to work: its click logs an `UnportedException` (W-08, until M5f). **The cube expander
   (Baevrunerk, 13 m from Minalinerk) does**: it asks 1,000 kinah and your cube grows by a row.
8a. **Loot and items** (with M5b-3's loot): a looted Plainsman's weapon or armour piece arrives **unidentified** — identify it (the
   client's identify button, 5 s), then equip it (W-19). Right-click a looted **manastone** onto an item with a free slot: it sockets
   or fails (the default chance is 75 % plus a level term, EnchantService.java:344-357). **Seril** (11 m from Minalinerk) removes it for 917 kinah. Buy **Extraction Tools** from
   Minalinerk (1,412 kinah), use them on a spare weapon: after 5 s the weapon is gone and 2-5 enchantment stones arrive; use one on your
   armour — it may fail (that is Java's 80 % cap). If a monster drops a bundle, open it. **Stigma stones** and anything that needs a capital
   npc still throw (§3a: M5e and D2).
9. **Mail**: click the mailbox, 26 m from Minalinerk. Send a letter with five potions and 200 kinah to a second character on another
   account; the cost is 251 kinah. Log the second character in (second client or relog): the mail icon lights, the letter is there, take
   the item and the kinah, delete the letter. Try the express icon: nothing happens (D9).
10. **Trade** (two clients, two accounts, same race, side by side): right-click → Trade, accept, add items and kinah, lock, OK. Items and
    kinah swap. Start another, cancel it: everything returns. Then **open a private store** on one and buy from it with the other.
11. **Crafting** needs Sanctum and level 10, and **level 10 needs a Daeva**: a Warrior or Mage stays at level 9 whatever its exp (§1
    finding 3), so rev 1's one-line SQL would leave Hestia silent. No player can reach it before M5d/M5e/M5f. With the character logged out
    (a Warrior shown; a Mage becomes `'SORCERER'`, an Asmodian needs quest 2008 instead of 1006):
    `UPDATE players SET player_class = 'GLADIATOR', world_id = 110010000, x = 1849.0, y = 1546.5, z = 590.2, exp = 126069 WHERE name = '<name>';`,
    `INSERT INTO player_quests (player_id, quest_id, status) VALUES (<id>, 1006, 'COMPLETE');` and give it kinah
    (`UPDATE inventory SET item_count = 10000 WHERE item_owner = <id> AND item_id = 182400001;`). Log in: you are a level-10 Gladiator
    standing between Hestia and the Ovens, with Aethertapping and Morph Substances learned and three morph recipes in the recipe book
    (W-06, W-20). Talk to Hestia → learn Cooking (3,500 kinah). Buy two Salt from Luelas. You also need one Inina, gathered in Verteron —
    or skip the craft and check only that the recipe list shows *Roast Inina*. With the materials, use an Oven and craft: the bar runs, two
    Roast Inina arrive, Cooking becomes 2. (The SQL is the gate's C19 seed; the gate proves it first. The Gladiator's other skills are the
    class's autolearn set, not a bug.)
12. Soul healing: after a death with experience loss, talk to Fulla (44 m from Minalinerk) → recover experience for kinah; the soul
    sickness icon goes (with M5b-2).
13. Log out and back in on both characters: every change above is still there.
14. Send `game-server/log/`, `m5a_summary.txt`, `live_counts.txt`, `partial_trace.txt`, `unported_trace.txt`, and note every click that did
    nothing — each is the entry list of a later milestone, as m5a-client-session.md F-2 was.

---

## 12. What was measured and what was inferred

**Measured** (grep or a parse over the two trees at HEAD `c1edb0afb` plus the working tree; re-runnable):

- Every `AION_UNPORTED` / `AION_PARTIAL` count in §2.8, per file and per chunk, including P5-09 = 121 split as drop 43 / trade 27 / broker 11 /
  mail 15 / craft 12 / rewards 12 / passport 1, and every U row of §2.1-§2.6 and §2.9 at its line.
- The per-body state of every file named (a script that lists each top-level C++ definition and whether its body holds `AION_UNPORTED`).
- **The invisible work of §2.8**: a script parsed each Java class with `tools/gen/javasrc.py` (named types and anonymous classes) and
  compared its method names, with multiplicity, against the identifiers followed by `(` in the class's C++ headers and xmlgen member blocks
  (`*.xml.inc`), enum companions (`*Info.h`) included. **Limits**: a name that appears in a header for another reason counts as declared (the
  count can only be too low), constructors are not compared, and renamed classes were mapped by hand (`DialogPage` → `DialogPageInfo`,
  `ExpertQuestsList`/`MasterQuestsList` → `CraftQuestsLists`). The 29 bodies were then checked by reading each header. **Rev 2 re-ran the
  same comparison** over the twelve P5-07 services and the 32 item-action classes plus `CompositionAction` (§3a, the 30 hand-off bodies of
  §2.8): body counts per class, Java lines, C++ `AION_UNPORTED` sites, undeclared names; the counts are name-level, so a helper whose name
  happens to appear in a header is missed (the per-action totals, 122 over the 31 classes other than `SkillUseAction`, are a floor).
- **The reconciliation's reach data (§3a)**: every npc function id's spawn maps (functions 4, 26, 33, 41, 42, 43, 47, 48, 66, 75/76, 94/95,
  109) over all `spawns/**` files with an XML parser; the action tags of the 106 goods the start-map vendors list (`skilluse` 24, `polish` 6
  — all six on 798037, which has no `BUY` — and `extract` 1: Extraction Tools); the 51 Plainsman's items' tuning attributes; the item-action
  tag counts over the 102,009 item templates (`remodel` 15,105, `decompose` 4,125, `enchant` 1,247, … `extract` 1; no `composition` tag —
  `CompositionAction` is built only by `CM_COMPOSITE_STONES`, CM_COMPOSITE_STONES.java:69).
- **The Daeva trace (W-20)**: `learnNewSkills(2..10)` for GLADIATOR/ELYOS over `skill_tree.xml` and `craft_skill_tree.xml` (autolearn,
  class or all-class rows, race or `PC_ALL`), each skill's activation and effect tags from `skill_templates.xml`, and the C++ state of the
  four passive effect classes (`WeaponMasteryEffect.cpp`, `ArmorMasteryEffect.cpp`, `ShieldMasteryEffect.cpp` 0 sites; `StatboostEffect` is
  data-only, m5b2-plan.md §2.4); the morph autolearn recipes per race (`recipe_templates.xml`, 3 per race at skill level 1).
- The pace of §5 (git log timestamps and `git show` counts of removed `AION_UNPORTED(` lines per commit).
- The 188 Java client packets, 42 C++ `CM_*.cpp` in `network/aion/clientpackets/`, the absence of the 23 + 12 packets of §2.8, and every
  opcode in `ClientPacketInfo.gen.inc`.
- The 14 tradelist templates of npcs spawned on Poeta and Ishalgen: every one has only `buy_price_rate` set (so `npc_type` NORMAL and
  `sell_price_rate` 100 by default), and no `purchase_template` or `trade_in_list_template` belongs to a start-map npc (XML parse).
- The 41 server packets of the scope each have a `.cpp` with 0 `AION_UNPORTED` (`SM_BROKER_SERVICE`, `SM_CRAFT_ANIMATION`, `SM_CRAFT_UPDATE`,
  `SM_CUBE_UPDATE`, `SM_DELETE_ITEM`, `SM_DIALOG_WINDOW`, `SM_DP_INFO`, `SM_EXCHANGE_ADD_ITEM`, `SM_EXCHANGE_ADD_KINAH`,
  `SM_EXCHANGE_CONFIRMATION`, `SM_EXCHANGE_REQUEST`, `SM_GATHERABLE_INFO`, `SM_GATHER_ANIMATION`, `SM_GATHER_UPDATE`, `SM_INVENTORY_ADD_ITEM`,
  `SM_INVENTORY_INFO`, `SM_INVENTORY_UPDATE_ITEM`, `SM_ITEM_USAGE_ANIMATION`, `SM_LEARN_RECIPE`, `SM_LOOKATOBJECT`, `SM_MAIL_SERVICE`,
  `SM_OBJECT_USE_UPDATE`, `SM_PLAYER_STATE`, `SM_PRIVATE_STORE`, `SM_PRIVATE_STORE_NAME`, `SM_QUESTION_WINDOW`, `SM_RECIPE_COOLDOWN`,
  `SM_RECIPE_DELETE`, `SM_RECIPE_LIST`, `SM_REPURCHASE`, `SM_SELL_ITEM`, `SM_SKILL_LIST`, `SM_STATS_INFO`, `SM_STATUPDATE_DP`,
  `SM_TITLE_INFO`, `SM_TRADELIST`, `SM_TRADE_IN_LIST`, `SM_UPDATE_PLAYER_APPEARANCE`, `SM_WAREHOUSE_ADD_ITEM`, `SM_WAREHOUSE_INFO`,
  `SM_WAREHOUSE_UPDATE_ITEM`), and **W-03**: a second script listed, per packet `.cpp`, the called names whose C++ definitions are unported;
  after discarding namesakes by hand, `Npc::canSell` / `canPurchase` were the only real hits.
- All 56 DAOs: 2 unported sites (`CustomInstancePlayerModelEntryDAO`), and each DAO function of §1 ported at its line.
- The dialog frame's C++ state (`NpcController.cpp:284-305`, `TalkEventHandler.cpp:28-58`, `DialogPageInfo.cpp:17-29`,
  `PlayerController.cpp:648-652`), the `DummyNpcAI` fallback (`AIEngine.cpp:153-181`), and that `handlers/ai/` holds only `AggressiveNpcAI`,
  `GeneralNpcAI`, `NoActionAI`.
- The data of §2.10: npc spots and `func_dialogs` on Poeta and Ishalgen (spawn files with XML comments removed), the trade and goods lists
  (re-parsed with an XML parser after the first pass's defect below), the starter inventory (and `oracle.py m5a-creation`, run read-only),
  the item masks, the potions' skill effects, recipe 155001381, the autolearn recipe list (26), Salt's vendors, Inina's gatherable and
  vendor, the Sanctum spots and Ovens, the 28 craft masters' maps, the broker npcs' maps, the absence of `subdialog_type` npcs on the start
  maps and capitals, the function-dialog id set (57 ids; `BUY_AGAIN = 70` is not among them).
- **The arithmetic of §2.10**, computed with `tools/oracle/m5a/javafloat.py` from the Java formulas: 125/113, 352/70/6, 50, 25 + 2 → 51 → 251,
  249, 141 and 76; rev 2 added 1,412, 917 and 3,640 by the same truncation chains (not run through `javafloat.py`: they are integer
  `(long)` chains over `double`, PricesService.java:86-99). **Its premise is inferred** (below).
- **The level rule** (PlayerCommonData.java:273-290, 588-610, and its faithful C++ port `PlayerCommonData.cpp:187-205, 265-290`) — read,
  not run.
- The P5-09 manifest row, the P5-02a/b split precedent, `fieldmap.json` rows for `CraftingTask` and `TemporaryTradeTimeTask`, and the
  `cycles.toml` rows for `ResponseRequester`, `AbstractInteractionTask`, `Player.postman`, `Mailbox`.
- **A defect in this analysis's own first pass, recorded as m5b2-plan.md §12 recorded its own:** a regex over `goodslists.xml` read list
  259 (Hephe's weapons in Ishalgen) as empty, because the self-closing `<list id="258"/>` before it swallowed its start. The re-parse with an
  XML parser fixed it; no Poeta number moved. **G-01 must use an XML parser.**

**Inferred, and a lane should confirm before relying on it:**

- **§0 as far as M5b-3's execution goes.** Rev 2 checked each row against `m5b3-plan.md`, which is a plan, not a delivery; the §3a edits to
  it are proposed, not made.
- **That `gameserver.siege.enable = false` gives influence 0** (read from SiegeService.java:74-91 and Influence.java:36-42; the C++
  `Influence.cpp` has 0 unported sites but was not read line by line). X1 settles it in the first run.
- **That the Daeva seed loads as level 10 in the C++ port** — read from the faithful port, not run; X21a settles it. (The threshold itself,
  126,069 = `experience[9]`, the review measured.)
- **That `EnchantService`'s other callees are ported** (risk 7a): sized, not traced.
- **That the 4.8 client sends `CM_USE_ITEM` for equipment** (W-22) — unknown; the real-client session shows it.
- **That §2.9 is complete.** It was traced by reading Java callees and checking each C++ counterpart; a callee reached through a virtual or a
  lambda can be missed. M5b-1 predicted two hidden prerequisites and found eight.
- **That the two-client broadcasts (X12's `SM_EMOTION`) reach a fake client** (risk 9).
- **That a player can enter Sanctum cleanly** (W-16).
- **That the `float[]` config parser accepts "0, 0"** (§10.1).
- **The wall-clock expectations**: the effort letters are sizes (§5); any time estimate is an extrapolation from the git-log pace.
- **The gate's time budget** (risk 11).

---

## 13. Open questions this analysis could not settle without building

1. **What M5b-3 actually delivers** (§0) — the first thing to check when M5c branches.
2. **What the log says when a character enters Sanctum** (W-16). Enter with an idle character in the gate lane's first run.
3. **Whether the real client sends `CM_DIALOG_SELECT(2)` before it opens the buy window**, or asks for the list another way, and what it
   sends when the mail window opens (it may send `CM_CHECK_MAIL_UNK`, already ported and empty, before `CM_CHECK_MAIL_LIST`). Only the
   real-client session can tell; the gate follows the Java handlers.
4. **Whether the double-confirm race (D7) duplicates anything in Java** or only logs an audit line: T-04's deterministic test decides it, and
   its answer decides whether D7's deviation proposal goes to the user.
5. **Whether `SM_RECIPE_LIST`'s unordered iteration matters to the client** (hub-headers.md §14's layout candidate) once a character has
   more than one recipe — C19's character now has four.
6. **Whether the user wants the broker in M5c** (D2), and with it where group K goes.
7. **Whether the user wants the stress run at all, and in which slot** (D13).
8. **Whether the M5b-3 plan accepts §3a's edits** — in particular taking identification (A-13), `RemodelAction` and `CM_QUESTION_RESPONSE`
   (A-10) itself. The integrator decides that when both plans are accepted.

---

## 14. Review, 2026-09-23

An adversarial review of rev 1 returned **needs-revision** with 4 high, 4 medium and 7 low findings, and verified the rest of rev 1's
counts, citations, data and arithmetic (its list: the `AION_UNPORTED` counts, the 63-site split, the DAO and packet inventories, the lesson-1
comparison, the prices and ledgers, the level-10 threshold, the data rows). Each finding was re-checked against the two trees before
anything changed.

| # | Sev. | Finding | Verdict | What changed |
|---|---|---|---|---|
| 1 | high | X3 expects a list refresh that a faithful port never sends: B's C9 relog builds a new `Mailbox` with state 0 | **confirmed** (MailService.java:265-267, MailDAO.java:34, Mailbox.java:25, SystemMailService.java:129) | C3 no longer keeps the postbox open; C11 has B reopen it before A's first letter; X3 asserts the page-18 state byte at both openings and says what it cannot prove |
| 2 | high | X4 asserts npc type 0; Java writes `NORMAL.index()` = 1 | **confirmed** (TradeNpcType.java:12; SM_TRADELIST.java:59; SM_SELL_ITEM.java:30, 40) | X4 asserts 1 from the oracle; G-01 emits the byte from the enum's constructor argument; a new mutation row (`ordinal()`) |
| 3 | high | M5b-3 sends ~180 item-service bodies to M5c that rev 1 neither took nor re-homed; the two plans disagreed on `TemporaryTradeTimeTask` | **confirmed, and larger**: measured ~233 bodies including two P5-07 services no plan named | New §3a gives every item a home: ~63 bodies to M5c (manastones, enchant, extraction, bundles, identification, cube expansion) in stages 0 and 1 with a new enhance lane; the rest to M5d, M5e, M5f, M5h, M5i, M5j or D2's capital-economy group K. `TemporaryTradeTimeTask` settled as M5c (P-04). Totals re-sized (~195 bodies, ~4,750 lines); the edits `m5b3-plan.md` needs are listed for the integrator (this revision cannot edit that file) |
| 4 | high | G-05 is a required ASan stress run and a capacity design, both the user's; its conservation invariant is false | **confirmed** (capacity-proposals.md:654-656; phase5-roadmap.md:62-66; TradeService.java:151, 238-246) | New D13 (user): G-05 is optional, runs only in a user-chosen slot, joins the capacity conversation; the invariant is a per-client ledger; stage 3 no longer schedules it. The entry in `capacity-proposals.md` §4.10/§11 is left to the integrator (not this plan's file) |
| 5 | medium | Half of X16 cannot fail: `item_unique_id` is the primary key | **confirmed** (`sql/aion_gs.sql:380`) | X16 rewritten (per-item-id ledger, DAO errors in `gs_log`, one object id in two clients' packet-built inventory models); D11 corrected |
| 6 | medium | W-06 is not dormant: level 10 learns 40009 and calls `autoLearnRecipes` | **confirmed in substance, but the mechanism and the proposed test are wrong**: a starting class never levels past 9 online (PlayerCommonData.java:276-281), so "9 → 10 by kills" cannot happen and a soul-healing crossing cannot cross; the live paths are the enter world after an offline level change (PlayerEnterWorldService.java:204) — exactly rev 1's own seed — a class change, and the ascension quest | W-06 reworded as live; new W-20; D5's seed is now a **Daeva** (rev 1's exp-only seed loaded a Warrior at level 9 and would have made the learn of rev 1's C15, now C19, a silent no-op — a defect the review did not catch); new X21a asserts 30003, 40009, 30001 → 30002 and the three morph recipes from the oracle; checklist step 11's SQL fixed the same way |
| 7 | medium | Missed wake-up: func 42 npcs (Seril, Dobar) open `REMOVE_MANASTONE`, whose `CM_MANASTONE` arm 3 is unported | **confirmed** (spawn scan; DialogPage.java:37; CM_MANASTONE.java:90-94) | New W-18; `ItemSocketService`'s manastone bodies ported in stage 0 (E-02); case C16 and X25 |
| 8 | medium | Lanes not chunk-disjoint in stages 2 and 3; G-06 assigned twice | **confirmed** | §6 rewritten: one P5-SC lane per stage (stage 3's gate-2 does G-03 part 2, G-04, then B-03, then G-05 if approved); fixups is a follow-up part after stage 2; G-06 belongs to gate-1 only |
| 9 | medium | The broker is not capital-only; D2's rationale is inaccurate | **confirmed** (20 maps, measured) | §1, §2.7, §3 and D2 carry the measured map list; the reason is now "not reachable before travel (M5f), a two-player market, the settlement timer" |
| 10 | low | C-01 needs `CraftingTask.h`, created by C-02, which depends on C-01 | **confirmed** (CraftService.java:123, 131) | I-02 generates the `CraftingTask` shell before stage 2; C-01 and C-02 depend on I-02; the merge order is stated |
| 11 | low | X2's "+ 1" and X6's `>=` mutations are not killed as specified | **confirmed**, and X2's band is finer than the review's "5 < d ≤ 6": `isInRange(…, false)` adds both bound radii and compares strictly (PositionUtil.java:243-261) | G-01 computes the band spot; the `>=` mutation moves to X17 and X27, which now make exact-kinah purchases (seeds 3,640 and C14's sum); X6 says it cannot catch it |
| 12 | low | X9 omits the second `(2)` | **confirmed** (ExchangeService.java:227-231) | X9: `(2)` to the partner on each OK, then `(0)` to both |
| 13 | low | §A allow-list rows are not the rows every gate has | **confirmed** (the M5a list has ten rows; `SkillEngine`'s partial sat at :137 when this revision read the tree and at :132 when the review did — the line moves, which is the point) | §10.1 derives §A from the m5b2/m5b3 lists at branch time |
| 14 | low | G-04 misses `m5b2_geo`; run count and relog budget off | **confirmed** | G-04 lists all eight earlier gates plus the smoke tests; risk 11 says ninth run and counts seven relogs (C16 added one) with an inferred 120-200 s |
| 15 | low | D12's "no M5c path calls `GeoService`" is wrong for gathering | **confirmed** (GatherableController.java:56) | D12 names it and leaves gathering's geo check to the real-client session |
| 16 | low | Five smaller inaccuracies | **four confirmed, one rejected** | (1) risk 3's foreign key removed (`sql/aion_gs.sql:529-543`); (3) W-10 names the `decreaseKinah` → A-01 path; (4) D6 names `multi = Rnd.nextFloat(1f, 2f)`; (5) 203080 lonian added. **(2) rejected**: 832782 nallo holds list 119 in a **`purchase_template`** (npc_trade_list.xml:10534-10540) — it *buys* Inina, it does not sell it — so "Inina is not sold by any spawned npc" stands; §2.10 now says so explicitly |
| 17 | low | Effort budgets not calibrated to the measured pace | **confirmed** | §5 redefines the effort letters as sizes and quotes the git-log pace (M5b-1 ~24 h plan to gate; M5b-2 part 2 295 sites in ~4 h); the day estimates are gone |

**Found by this revision, beyond the review:** the Daeva level rule and rev 1's level-9 seed (above, row 6); **W-17**, the Extraction Tools
that the plan's own merchant sells and whose `ExtractAction` reaches `EnchantService`; **W-19 / A-13**, every Plainsman's drop created
unidentified, so M5b-3's loot cannot be equipped without `CM_TUNE`; **W-22 / W-23**, the `RemodelAction` and `DecomposeAction` stubs M5b-3's
header batch declares; **risk 7a**, `EnchantService` sized but not traced; and that `CompositionAction` has no item-data tag at all (only
`CM_COMPOSITE_STONES` builds it).

**What the revision did not do:** it did not edit `m5b3-plan.md` or `capacity-proposals.md` (read-only here); it did not trace
`EnchantService`'s callees one by one; it did not run the oracle for the new seeds.
