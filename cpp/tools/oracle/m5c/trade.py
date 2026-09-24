"""m5c-trade: what a merchant npc sells to a player and at what price, and what it pays for what a player sells (m5c-plan.md §2.2, §2.10, G-01).

Java rules, each with the method it comes from (game-server/src/com/aionemu/gameserver):
- the price factors: PricesService.getGlobalPrices (PricesService.java:21-33) and getTaxes (:45-53) over the influence rate of the player's race
  (:55-63, Influence.java:36-42: getInfluence(race) / 100f). With gameserver.siege.enable = false SiegeService holds no location
  (SiegeService.java:75-92), so Influence computes 0 for both races: globalPrices = Math.round(DEFAULT_PRICES + ((0.5f / 2) * 100)) and taxes =
  Math.round(DEFAULT_TAXES + ((0.5f / 4) * 100)), in float arithmetic - 125 and 113 with the default 100s. SM_PRICES writes the three as bytes
  (SM_PRICES.java:13-18). With sieges on, the influence is the sum of the influence values of the fortresses the race holds, which comes from
  the database (SiegeDAO.loadSiegeLocations): the oracle then refuses unless --influence RACE=N gives it;
- the buy price of one unit: PricesService.getBuyPrice (:95-99), four `(long) (x * factor / 100D)` truncations over the template price with
  VENDOR_BUY_MODIFIER, the global prices, DEFAULT_MODIFIER and the taxes;
- a purchase: TradeService.performBuyFromShop (TradeService.java:62-75: NORMAL and ABYSS_KINAH pay kinah, ABYSS and REWARD do not, every other
  type is "Unhandled" and fails), performBuyTransaction (:80-164) with the kinah modifier `sell_price_rate` (`sell_price_rate2` for ABYSS_KINAH,
  :95) and the AP modifier `sell_price_rate` (`ap_sell_price_rate2` for ABYSS_KINAH, :96); TradeList.calculateBuyListPrice (TradeList.java:48-57):
  getBuyPrice * count * modifier / 100 in long arithmetic, per item; calculateAbyssRewardBuyList (:59-98): an item whose <acquisition> type is
  AP or ABYSS costs (int) ((ap * count * modifier / 100.0D) * VENDOR_BUY_MODIFIER) / 100 abyss points, and an acquisition item id costs
  acquisition count * count of that item; validateBuyItems (:166-181): every tab's goods list, WITHOUT the legion level filter the window
  applies; canBuyLimitItem (:51-60) for the limited items of LimitedItemTradeService (LimitedItemTradeService.java:29-61, GoodsList.java:50-60:
  an item is limited only when it has BOTH buy_limit and sell_limit; a fresh server has sold nothing, LimitedItem.java);
- the npc functions: Npc.canSell / canBuy / canTradeIn / canPurchase (Npc.java:361-386) over NpcTemplate.supportsAction (NpcTemplate.java:283-286,
  talk_info func_dialogs) and DialogAction BUY, SELL, TRADE_IN, TRADE_SELL_LIST; CM_BUY_ITEM (CM_BUY_ITEM.java:93-143) runs a buy (13-16) only if
  canSell() and a sale (1) only if canBuy() || canPurchase(), and rejects a count above 20000 (:69);
- the dialog path (dialog_path): CM_DIALOG_SELECT.runImpl (CM_DIALOG_SELECT.java:110-122) audits and sends NOTHING for a dialog id that any npc
  template lists in its func_dialogs (NpcData.isFunctionDialog, NpcData.java:89-90, 107-109) when this npc does not support it - BUY (2), SELL
  (3) and TRADE_SELL_LIST (103) all are in the data; then NpcController.onDialogSelect (NpcController.java:266-272) asks the npc's AI
  (AIEngine.newAI over the @AIName classes, Creature.java:64-67 with a spot's `ai`; AbstractAI.onDialogSelect answers false, AbstractAI.java:384-387)
  and only then DialogService. An AI class that overrides onDialogSelect is not modelled: the window is reported as None (no trade npc of the real
  data has one), and an npc_template ai no class carries is refused (AIEngine.validateScripts, AIEngine.java:111-114);
- the windows: DialogService.onDialogSelect's BUY arm (DialogService.java:74-95: STR_BUY_SELL_HE_DOES_NOT_SELL_ITEM without a trade list or
  without a tab the player's legion level may see, else SM_TRADELIST with VENDOR_BUY_MODIFIER * sell_price_rate / 100; past the gate the npc
  supports BUY, so the buy tab of a real SM_TRADELIST is on) and its SELL / TRADE_SELL_LIST arm (:251-254, SM_SELL_ITEM, reachable only through
  a dialog the npc supports); SM_TRADELIST (SM_TRADELIST.java:33-73) and SM_SELL_ITEM (SM_SELL_ITEM.java:27-47). A CM_BUY_ITEM buy or sale does
  not pass the dialog gate: an npc with BUY but without SELL still buys items through SM_TRADELIST's sell tab (canBuy() is canSell() too);
- a sale: TradeService.performSellToShop (:183-249). Without a purchase_template (every Poeta and Ishalgen merchant): Item.isSellable
  (Item.java:651-653, the SELLABLE bit of the template mask after ItemData.cleanup applied item_restriction_cleanups, ItemData.java:70-86) else
  STR_BUY_SELL_ITEM_CAN_NOT_BE_SELLED_TO_NPC, and PricesService.getSellReward(price, VENDOR_SELL_MODIFIER) (:104-106). With a
  purchase_template: the item must be in one of its purchase lists (no message otherwise, and no sellable check) and pays
  (long) (price * buy_price_rate / 100D) (:202-214); an ABYSS purchase template takes performSellForAPToShop (:251-286) and pays
  Math.round((ap * buy_price_rate) / 100F) * count abyss points. Then PlayerLimitService.updateSellLimit (PlayerLimitService.java:22-49):
  nothing when gameserver.limits.enable is false or the reward is 0, else the account's daily limit (SellLimit.java over
  Rates.SELL_LIMIT, Rates.java:143-149 and :166-173) caps the count; the oracle computes it for the FIRST sale of an account since the server
  started (or since gameserver.limits.update last cleared it) - an account's earlier sales are in-memory state. The item's repurchase price is
  the whole reward (TradeService.java:241-243, RepurchaseService.java:47-69).

Java behaviour a gate should know (reported, not modelled away): a multi-item CM_BUY_ITEM sale that meets an unsellable item, an item missing
from the purchase lists or a count above the stack AFTER a sellable one returns false having already removed the earlier items, and pays
nothing for them (TradeService.java:216-218, 229-231); a sale whose limit leaves 0 items `break`s and still replaces the repurchase list; a buy
whose AP is short sends STR_MSG_NOT_ENOUGH_ABYSSPOINT twice (TradeList.java:86-88 and TradeService.java:105-107); the SM_TRADELIST / SM_SELL_ITEM
npc type byte is TradeNpcType.index(), 1 for NORMAL (not 0).

The static data is the one gameserver.country.code selects (XmlMerger.applyCountryOverride, XmlMerger.java:233-243: goodslists_<region>.xml for
the codes 1, 2, 4, 5, 6 and 7); trade_report refuses data read with another code.

What the oracle does NOT model, and raises OracleError for: a goods item without an item template (NullPointerException in
calculateBuyListPrice), an <acquisition> without a known type in a purchase, a purchase list id that is missing before the item is found
(NullPointerException in performSellToShop / performSellForAPToShop, after the apitems switch for the latter), long or int overflow in the price
arithmetic, sieges without --influence, a config key an event may override, data the game server cannot start with (a limited item whose goods
list has no or an unmodelled <salestime>, a cleanup that sets or clears a bit of an item without a template, a missing trade or goods list
kind, an npc_template ai without an AI class), and Java code that is not the code this oracle models: every modelled member (method,
constructor, class, enum constant body, switch arm) is fingerprinted WHOLE in MODELLED_MEMBERS, with every comment and all white space removed,
so an edit anywhere inside one is a refusal; MODELLED_STATEMENTS name the key statements for a precise message. Player state is out of scope:
PlayerRestrictions.canTrade, the kinah, AP, items and free slots a purchase needs (reported as requirements), and what a dialog needs from the
player (not trading, the npc known, in talk range - the report gives talkRange - and DialogService.isInteractionAllowed, whose subdialog_type
the report gives).
"""

from __future__ import annotations

import hashlib
import re
import weakref
from dataclasses import dataclass
from pathlib import Path

from staticdata_oracle import OracleError

from m5a.creation import RACES, enum_constants
from m5a.data import StaticData, java_int
from m5a.javafloat import f32, round_to_int, to_int, to_long
from m5a.spawns import GameClock, NpcInfo as SpawnNpcInfo, evaluate, load_groups

from .trade_config import ConfigValue

LONG_MIN, LONG_MAX = -2**63, 2**63 - 1
NPC_TYPES_WITH_KINAH = ("NORMAL", "ABYSS_KINAH")  # TradeService.performBuyFromShop: performBuyTransaction(..., true)
NPC_TYPES_WITHOUT_KINAH = ("ABYSS", "REWARD")     # ... (..., false); every other TradeNpcType is "Unhandled"
AP_ACQUISITIONS = ("AP", "ABYSS")                     # TradeList.calculateAbyssRewardBuyList
CLEANUP_MASKS = (("trade", "TRADEABLE"), ("sell", "SELLABLE"), ("wh", "STORABLE_IN_WH"), ("awh", "STORABLE_IN_AWH"), ("lwh", "STORABLE_IN_LWH"))
NO_AI = "__NO_AI__"  # SpawnTemplate.NO_AI (a MODELLED_STATEMENTS entry)

# The key statements the oracle models, as they stand in the Java sources (file below com/aionemu/gameserver, statement). read() looks for each
# one with every comment and all white space removed, which names the statement that changed; the whole members around them are checked by
# MODELLED_MEMBERS below, which also catches code inserted before, after or between these statements.
MODELLED_STATEMENTS = (
	("services/trade/PricesService.java",
	 "public static int getGlobalPrices(Race playerRace) { int defaultPrices = PricesConfig.DEFAULT_PRICES; float influenceValue = "
	 "getPriceInfluenceRate(playerRace); if (influenceValue == 0.5f) { return defaultPrices; } else if (influenceValue > 0.5f) { float diff = "
	 "influenceValue - 0.5f; return Math.round(defaultPrices - ((diff / 2) * 100)); } else { float diff = 0.5f - influenceValue; return "
	 "Math.round(defaultPrices + ((diff / 2) * 100)); } }"),
	("services/trade/PricesService.java", "public static int getGlobalPricesModifier() { return PricesConfig.DEFAULT_MODIFIER; }"),
	("services/trade/PricesService.java",
	 "public static int getTaxes(Race playerRace) { int defaultTax = PricesConfig.DEFAULT_TAXES; float influenceValue = "
	 "getPriceInfluenceRate(playerRace); if (influenceValue >= 0.5f) { return defaultTax; } float diff = 0.5f - influenceValue; return "
	 "Math.round(defaultTax + ((diff / 4) * 100)); }"),
	("services/trade/PricesService.java",
	 "switch (playerRace) { case ASMODIANS: return Influence.getInstance().getAsmodianInfluenceRate(); case ELYOS: return "
	 "Influence.getInstance().getElyosInfluenceRate(); }"),
	("services/trade/PricesService.java", "public static int getVendorBuyModifier() { return PricesConfig.VENDOR_BUY_MODIFIER; }"),
	("services/trade/PricesService.java", "public static int getVendorSellModifier() { return PricesConfig.VENDOR_SELL_MODIFIER; }"),
	("services/trade/PricesService.java",
	 "public static long getBuyPrice(long requiredKinah, Race playerRace) { return (long) ((long) ((long) ((long) (requiredKinah * "
	 "getVendorBuyModifier() / 100D) * getGlobalPrices(playerRace) / 100D) * getGlobalPricesModifier() / 100D) * getTaxes(playerRace) / 100D); }"),
	("services/trade/PricesService.java",
	 "public static long getSellReward(long kinahValue, int sellModifier) { return (long) (kinahValue * sellModifier / 100D); }"),
	("model/siege/Influence.java", "elyosInfluenceRate = getInfluence(SiegeRace.ELYOS) / 100f;"),
	("model/siege/Influence.java", "asmoInfluenceRate = getInfluence(SiegeRace.ASMODIANS) / 100f;"),
	("model/siege/Influence.java", "for (SiegeLocation sLoc : SiegeService.getInstance().getSiegeLocations().values()) {"),
	("model/siege/Influence.java", "public int getInfluence(SiegeRace race) { return globalInfluences.getOrDefault(race, 0); }"),
	("services/SiegeService.java", "if (SiegeConfig.SIEGE_ENABLED) {"),
	("services/SiegeService.java", "outposts = Collections.emptyMap(); locations = Collections.emptyMap();"),
	("model/trade/TradeList.java",
	 "requiredKinah += PricesService.getBuyPrice(tradeItem.getItemTemplate().getPrice(), player.getRace()) * tradeItem.getCount() * modifier / 100;"),
	("model/trade/TradeList.java", "return availableKinah >= requiredKinah;"),
	("model/trade/TradeList.java",
	 "if (aquisition.getType().equals(AcquisitionType.AP) || aquisition.getType().equals(AcquisitionType.ABYSS)) requiredAp += (int) "
	 "((aquisition.getRequiredAp() * tradeItem.getCount() * modifier / 100.0D) * PricesService.getVendorBuyModifier()) / 100;"),
	("model/trade/TradeList.java", "int rewardItemId = aquisition.getItemId(); if (rewardItemId == 0) continue;"),
	("model/trade/TradeList.java", "requiredItems.put(rewardItemId, aquisition.getItemCount() * tradeItem.getCount());"),
	("model/trade/TradeList.java", "if (ap < requiredAp) { PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_MSG_NOT_ENOUGH_ABYSSPOINT());"),
	("model/trade/TradeList.java", "if (requiredItems.get(itemId) < 1 || count < requiredItems.get(itemId)) return false;"),
	("services/TradeService.java",
	 "switch (npcType) { case NORMAL: case ABYSS_KINAH: return performBuyTransaction(npc, player, tradeList, true); case ABYSS: case REWARD: return "
	 "performBuyTransaction(npc, player, tradeList, false); default: log.warn(\"Unhandled TradeNpcType:\" + npcType.name()); } return false;"),
	("services/TradeService.java",
	 "int sellModifier = template.getTradeNpcType().equals(TradeNpcType.ABYSS_KINAH) ? template.getSellPriceRate2() : template.getSellPriceRate();"),
	("services/TradeService.java",
	 "int apSellModifier = template.getTradeNpcType().equals(TradeNpcType.ABYSS_KINAH) ? template.getApSellPriceRate2() : "
	 "template.getSellPriceRate();"),
	("services/TradeService.java", "if (useKinah && !tradeList.calculateBuyListPrice(player, sellModifier)) {"),
	("services/TradeService.java", "if (!tradeList.calculateAbyssRewardBuyList(player, apSellModifier)) {"),
	("services/TradeService.java", "if (tradeList.getRequiredAp() < 0) {"),
	("services/TradeService.java", "if (freeSlots < tradeList.size()) {"),
	("services/TradeService.java", "if (goodsList != null && goodsList.getItemIdList() != null) allowedItems.addAll(goodsList.getItemIdList());"),
	("services/TradeService.java", "if (tradeItem.getCount() < 1 || !allowedItems.contains(tradeItem.getItemId())) return false;"),
	("services/TradeService.java",
	 "if (item.getDefaultSellLimit() > 0 && item.getSellLimit() - tradeItem.getCount() < 0) return false; if (item.getBuyLimit() > 0 && "
	 "item.getBuyCount(player.getObjectId()) + tradeItem.getCount() > item.getBuyLimit()) return false;"),
	("services/TradeService.java", "return performSellToShop(player, tradeList, purchaseTemplate, PricesService.getVendorSellModifier());"),
	("services/TradeService.java", "GoodsList goodList = goodsListData.getGoodsPurchaseListById(tab.getId()); if (goodList.getItemIdList().contains(itemId)) {"),
	("services/TradeService.java", "sellReward = (long) (item.getItemTemplate().getPrice() * purchaseTemplate.getBuyPriceRate() / 100D);"),
	("services/TradeService.java",
	 "} else { if (!item.isSellable()) { PacketSendUtility.sendPacket(player, SM_SYSTEM_MESSAGE.STR_BUY_SELL_ITEM_CAN_NOT_BE_SELLED_TO_NPC("
	 "item.getL10n())); return false; } sellReward = PricesService.getSellReward(item.getItemTemplate().getPrice(), sellModifier); }"),
	("services/TradeService.java", "count = PlayerLimitService.updateSellLimit(player, sellReward, count); if (count == 0) break; long realReward = "
	                               "sellReward * count;"),
	("services/TradeService.java", "kinahReward += realReward; repurchaseItem.setRepurchasePrice(realReward);"),
	("services/TradeService.java", "if (!CustomConfig.SELLING_APITEMS_ENABLED) {"),
	("services/TradeService.java",
	 "int requiredAp = item.getItemTemplate().getAcquisition().getRequiredAp(); int apToAdd = Math.round((requiredAp * "
	 "purchaseTemplate.getBuyPriceRate()) / 100F); AbyssPointsService.addAp(player, apToAdd * (int) count);"),
	("network/aion/clientpackets/CM_BUY_ITEM.java", "if (count < 0 || (itemId <= 0 && tradeActionId != 0) || count > 20000) {"),
	("network/aion/clientpackets/CM_BUY_ITEM.java",
	 "case 1: if (npc.canBuy() || npc.canPurchase()) { tradeTemplate = DataManager.TRADE_LIST_DATA.getPurchaseTemplate(npc.getNpcId()); if "
	 "(tradeTemplate != null && tradeTemplate.getTradeNpcType() == TradeNpcType.ABYSS) TradeService.performSellForAPToShop(player, tradeList, "
	 "tradeTemplate); else TradeService.performSellToShop(player, tradeList, tradeTemplate); } break;"),
	("network/aion/clientpackets/CM_BUY_ITEM.java",
	 "case 13: case 14: case 15: case 16: if (npc.canSell()) TradeService.performBuyFromShop(npc, player, tradeList); break;"),
	("services/DialogService.java", "int tradeModifier = tradeListTemplate.getSellPriceRate();"),
	("services/DialogService.java", "if (goodsList == null || goodsList.getLegionLevel() > legionLevel) continue; hasAnythingToSell = true; break;"),
	("services/DialogService.java", "new SM_TRADELIST(player, npc, tradeListTemplate, PricesService.getVendorBuyModifier() * tradeModifier / 100)"),
	("services/DialogService.java", "case SELL: case TRADE_SELL_LIST: PacketSendUtility.sendPacket(player, new SM_SELL_ITEM(npc)); break;"),
	("services/DialogService.java", "if (questId == 0) { switch (dialogActionId) { case BUY: {"),
	("services/DialogService.java", "if (talkInfo == null || talkInfo.getSubDialogType() == null) return false;"),
	("network/aion/clientpackets/CM_DIALOG_SELECT.java",
	 "if (target instanceof Npc npc) { boolean isFunctionDialog = DataManager.NPC_DATA.isFunctionDialog(dialogActionId); if (isFunctionDialog && "
	 "!npc.getObjectTemplate().supportsAction(dialogActionId)) { AuditLogger.log(player, \"tried to use unsupported dialog action \" + "
	 "dialogActionName + \" on \" + npc); return; }"),
	("controllers/NpcController.java",
	 "if (!PositionUtil.isInTalkRange(player, getOwner())) return; if (!getOwner().getAi().onDialogSelect(player, dialogActionId, questId, "
	 "extendedRewardIndex)) { DialogService.onDialogSelect(dialogActionId, player, getOwner(), questId, extendedRewardIndex); }"),
	("dataholders/NpcData.java", "if (npc.getFuncDialogIds() != null) functionDialogIds.addAll(npc.getFuncDialogIds());"),
	("dataholders/NpcData.java", "public boolean isFunctionDialog(int functionDialogId) { return functionDialogIds.contains(functionDialogId); }"),
	("ai/AbstractAI.java", "public boolean onDialogSelect(Player player, int dialogActionId, int questId, int extendedRewardIndex) { return false; }"),
	("ai/AIEngine.java",
	 "if (name == null) { aiInstance = new DummyAI<>(owner); } else { Class<? extends AbstractAI<? extends Creature>> aiClass = aiHandlers.get(name); "
	 "if (aiClass == null) throw new IllegalArgumentException(\"No AI found for name \" + name);"),
	("model/gameobjects/Creature.java",
	 "String aiName = objectTemplate.getAiName(); if (spawnTemplate != null && spawnTemplate.getAiName() != null) aiName = "
	 "SpawnTemplate.NO_AI.equals(spawnTemplate.getAiName()) ? null : spawnTemplate.getAiName(); this.ai = AIEngine.getInstance().newAI(aiName, this);"),
	("model/templates/spawns/SpawnTemplate.java", "public static final String NO_AI = \"__NO_AI__\";"),
	("model/gameobjects/Npc.java",
	 "public boolean canSell() { return DataManager.TRADE_LIST_DATA.getTradeListTemplate(getNpcId()) != null && "
	 "getObjectTemplate().supportsAction(DialogAction.BUY); }"),
	("model/gameobjects/Npc.java", "public boolean canBuy() { return getObjectTemplate().supportsAction(DialogAction.SELL) || canSell(); }"),
	("model/gameobjects/Npc.java",
	 "public boolean canTradeIn() { return DataManager.TRADE_LIST_DATA.getTradeInListTemplate(getNpcId()) != null && "
	 "getObjectTemplate().supportsAction(DialogAction.TRADE_IN); }"),
	("model/gameobjects/Npc.java",
	 "public boolean canPurchase() { return DataManager.TRADE_LIST_DATA.getPurchaseTemplate(getNpcId()) != null && "
	 "getObjectTemplate().supportsAction(DialogAction.TRADE_SELL_LIST); }"),
	("model/templates/npc/NpcTemplate.java",
	 "public boolean supportsAction(int dialogActionId) { List<Integer> dialogIds = getFuncDialogIds(); return dialogIds != null && "
	 "dialogIds.contains(dialogActionId); }"),
	("network/aion/serverpackets/SM_TRADELIST.java", "this.showBuyTab = npc.canSell(); this.showSellTab = npc.canBuy();"),
	("network/aion/serverpackets/SM_TRADELIST.java", "if (goodsList == null || goodsList.getLegionLevel() > legionLevel) continue; this.tradeTablist.add(tab);"),
	("network/aion/serverpackets/SM_TRADELIST.java",
	 "writeD(targetObjId); writeC(tradeNpcType.index()); writeD(buyPriceModifier); writeD(100); writeC(showBuyTab ? 1 : 0); writeC(showSellTab ? 1 "
	 ": 0); writeH(tradeTablist.size()); for (TradeTab tradeTabl : tradeTablist) writeD(tradeTabl.getId()); writeH(limitedItems.size()); for "
	 "(LimitedItem limitedItem : limitedItems) { writeD(limitedItem.getItemId()); writeH(limitedItem.getBuyCount(playerObjId)); "
	 "writeH(limitedItem.getSellLimit()); }"),
	("network/aion/serverpackets/SM_SELL_ITEM.java",
	 "this.tradeNpcType = tradeList != null ? tradeList.getTradeNpcType() : TradeNpcType.NORMAL; this.buyPriceRate = tradeList != null ? "
	 "tradeList.getBuyPriceRate() : PricesService.getVendorSellModifier(); this.showBuyTab = npc.canSell(); this.showSellTab = npc.canBuy() || "
	 "npc.canPurchase(); this.tradeTabs = tradeList != null ? tradeList.getTradeTablist() : new ArrayList<>();"),
	("network/aion/serverpackets/SM_SELL_ITEM.java",
	 "writeD(targetObjectId); writeC(tradeNpcType.index()); writeD(buyPriceRate); writeC(showBuyTab ? 1 : 0); writeC(showSellTab ? 1 : 0); "
	 "writeH(tradeTabs.size()); for (TradeTab tradeTab : tradeTabs) writeD(tradeTab.getId());"),
	("network/aion/serverpackets/SM_PRICES.java",
	 "writeC(PricesService.getGlobalPrices(con.getActivePlayer().getRace())); writeC(PricesService.getGlobalPricesModifier()); "
	 "writeC(PricesService.getTaxes(con.getActivePlayer().getRace()));"),
	("model/gameobjects/Item.java", "public boolean isSellable() { return (getItemMask() & ItemMask.SELLABLE) == ItemMask.SELLABLE; }"),
	("model/gameobjects/Item.java", "public int getItemMask() { return itemTemplate.getMask(); }"),
	("dataholders/ItemData.java", "applyCleanup(template, ict.resultSell(), ItemMask.SELLABLE);"),
	("dataholders/ItemData.java", "case 1 -> item.modifyMask(true, mask); case 0 -> item.modifyMask(false, mask);"),
	("dataholders/ItemData.java", "items.put(it.getTemplateId(), it);"),
	("dataholders/TradeListData.java",
	 "for (TradeListTemplate npc : tlist) { npctlistData.put(npc.getNpcId(), npc); } for (TradeListTemplate npc : tInlist) { "
	 "npcTradeInlistData.put(npc.getNpcId(), npc); } for (TradeListTemplate npc : plist) { npcPurchaseTemplateData.put(npc.getNpcId(), npc); }"),
	("dataholders/GoodsListData.java",
	 "for (GoodsList it : list) { goodsListData.put(it.getId(), it); } for (GoodsList it : inList) { goodsInListData.put(it.getId(), it); } for "
	 "(GoodsList it : purchaseList) { goodsPurchaseListData.put(it.getId(), it); }"),
	("model/templates/goods/GoodsList.java",
	 "if (item.getBuyLimit() != null && item.getSellLimit() != null) { limitedItems.add(new LimitedItem(item.getId(), item.getSellLimit(), "
	 "item.getBuyLimit(), salesTime)); }"),
	("services/LimitedItemTradeService.java",
	 "List<LimitedItem> limitedItems = goodsList.getLimitedItems(); if (!limitedItems.isEmpty()) { limitedTradeNpcs.computeIfAbsent(npcId, k -> new "
	 "LimitedTradeNpc()).addLimitedItems(limitedItems); }"),
	("services/LimitedItemTradeService.java",
	 "for (LimitedItem limitedItem : limitedTradeNpcs.get(npcId).getLimitedItems()) { if (limitedItem.getItemId() == itemId) { return limitedItem; } }"),
	("services/LimitedItemTradeService.java", "CronService.getInstance().schedule(limitedItem::setToDefault, limitedItem.getSalesTime());"),
	("services/cron/CronExpressions.java",
	 "return cronExpressions.computeIfAbsent(cronExpression, _ -> { try { return new CronExpression(cronExpression); } catch (ParseException e) { "
	 "throw new RuntimeException(e); } });"),
	("dataholders/ItemData.java", "ItemTemplate template = getItemTemplate(ict.getId());"),
	("services/player/PlayerLimitService.java",
	 "if (!CustomConfig.LIMITS_ENABLED || itemPrice == 0) return itemCount; int accountId = player.getAccount().getId(); Long limit = "
	 "sellLimit.get(accountId); if (limit == null) { limit = SellLimit.getSellLimit(player); sellLimit.putIfAbsent(accountId, limit); } if "
	 "(itemPrice < 0 || itemCount <= 0) return 0; long possibleCount = Math.max(0, limit / itemPrice); if (CustomConfig.LIMITS_ENABLE_DYNAMIC_CAP && "
	 "possibleCount < itemCount) possibleCount += 1; if (possibleCount == 0 || limit == 0) { PacketSendUtility.sendPacket(player, "
	 "SM_SYSTEM_MESSAGE.STR_MSG_DAY_CANNOT_SELL_NPC(limit)); return 0; } else { long useCount = Math.min(possibleCount, itemCount); limit -= "
	 "Math.min(limit, itemPrice * useCount); sellLimit.put(accountId, limit); return useCount; }"),
	("model/SellLimit.java",
	 "int playerLevel = player.getAccount().getMaxPlayerLevel(); for (SellLimit sellLimit : values()) { if (sellLimit.playerMinLevel <= playerLevel "
	 "&& sellLimit.playerMaxLevel >= playerLevel) { return Rates.SELL_LIMIT.calcResult(player, sellLimit.limit); } }"),
	("model/gameobjects/player/Rates.java",
	 "SELL_LIMIT { @Override public long calcResult(Player player, long sellLimit) { return (long) (sellLimit * get(player, "
	 "RatesConfig.SELL_LIMIT_RATES)); } };"),
	("model/gameobjects/player/Rates.java",
	 "if (membershipRates.length == 0) { LoggerFactory.getLogger(Rates.class).warn(\"Missing rates\", new IllegalStateException()); return 1; } int "
	 "membershipLevel = player.getAccount().getMembership(); return membershipRates[Math.min(membershipRates.length - 1, membershipLevel)];"),
)


# Every member (method, constructor, class, enum constant body, switch arm) whose code the oracle models, with the fingerprint of its whole text
# as it stands in the Java sources: comments and all white space removed, sha256, the first 16 hex digits. read() recomputes each one, so ANY
# change inside a modelled member - a statement inserted before, after or between the MODELLED_STATEMENTS, a changed operand, a `break` - is a
# refusal. A member is `method NAME` (unique by name) or `method NAME(PARAMETERS)`, `class NAME`, `block NAME` (an enum constant's body) or
# `case NAME` (a switch arm, from its labels to the next label of the same switch).
MODELLED_MEMBERS = (
	# the price factors and SM_PRICES
	("services/trade/PricesService.java", "class PricesService", "3add002d1eaa0afe"),
	("model/siege/Influence.java", "method Influence()", "76478d1332d04499"),
	("model/siege/Influence.java", "method recalculateInfluence", "704c3f40c4691c31"),
	("model/siege/Influence.java", "method calculateFortressWorldInfluences", "4d061b078db2ee97"),
	("model/siege/Influence.java", "method calculateGlobalInfluences", "f859c18385a91026"),
	("model/siege/Influence.java", "method getElyosInfluenceRate", "175fb7e55d2507ec"),
	("model/siege/Influence.java", "method getAsmodianInfluenceRate", "7cd69abc1a052dd5"),
	("model/siege/Influence.java", "method getInfluence(SiegeRace race)", "2db7707eedfcb077"),
	("services/SiegeService.java", "method SiegeService()", "beb555eda31c5740"),
	("services/SiegeService.java", "method getSiegeLocations()", "9356e5002193d27a"),
	("network/aion/serverpackets/SM_PRICES.java", "class SM_PRICES", "c564293d77ed1665"),
	# a purchase
	("model/trade/TradeList.java", "class TradeList", "f06277669cdf0ed0"),
	("model/trade/TradeItem.java", "class TradeItem", "260c763d911b3907"),
	("services/TradeService.java", "method canBuyLimitItem", "b5e6a8c4181d5b3f"),
	("services/TradeService.java", "method performBuyFromShop", "1ab545e819449bdd"),
	("services/TradeService.java", "method performBuyTransaction", "425084f2aea08e00"),
	("services/TradeService.java", "method validateBuyItems", "c54b2dadf3e31e58"),
	("network/aion/clientpackets/CM_BUY_ITEM.java", "method readImpl", "3afa1564d8591c6b"),
	("network/aion/clientpackets/CM_BUY_ITEM.java", "method runImpl", "76bfdb6a844c1000"),
	# a sale
	("services/TradeService.java", "method performSellToShop(Player player, TradeList tradeList, TradeListTemplate purchaseTemplate)", "6a79a2cba41435a1"),
	("services/TradeService.java", "method performSellToShop(Player player, TradeList tradeList, TradeListTemplate purchaseTemplate, int sellModifier)",
	 "7a3fa264e017df9a"),
	("services/TradeService.java", "method performSellForAPToShop", "413cd445bfb2d633"),
	("services/RepurchaseService.java", "method addRepurchaseItems", "442785a3fcc88749"),
	("services/RepurchaseService.java", "method repurchaseFromShop", "1aae38322ddd74c4"),
	("services/player/PlayerLimitService.java", "method updateSellLimit", "db592340c111078c"),
	("model/SellLimit.java", "method getSellLimit", "19388a630c061191"),
	("model/gameobjects/player/Rates.java", "block SELL_LIMIT", "a57539cf5207dbc5"),
	("model/gameobjects/player/Rates.java", "method get(Player player, float[] membershipRates)", "fceb69c865b0fc64"),
	("model/gameobjects/Item.java", "method isSellable", "be51638887064a22"),
	("model/gameobjects/Item.java", "method getItemMask", "c7a4fe4ce49f02e0"),
	# the dialogs and the windows
	("network/aion/clientpackets/CM_DIALOG_SELECT.java", "method runImpl", "bbf0b981e721f6a1"),
	("controllers/NpcController.java", "method onDialogSelect", "2b4babbadae56781"),
	("ai/AbstractAI.java", "method onDialogSelect", "9fa91dca2996810e"),
	("ai/AIEngine.java", "method newAI", "d626ef0f4dcc5c3f"),
	("ai/AIEngine.java", "method validateScripts", "f49c488d2db05975"),
	("ai/AIEngine.java", "class DummyAI", "56944f5c202a193b"),
	("model/gameobjects/Creature.java", "method Creature", "bb89200104d6a250"),
	("services/DialogService.java", "case BUY", "a0e2ebb8ecfb3e8d"),
	("services/DialogService.java", "case SELL", "1323684df51b3104"),
	("services/DialogService.java", "method isInteractionAllowed", "50b0bc09944cefc1"),
	("model/gameobjects/Npc.java", "method canSell", "11226ddc2ba6bf48"),
	("model/gameobjects/Npc.java", "method canBuy", "467f38109c67d07e"),
	("model/gameobjects/Npc.java", "method canTradeIn", "45c7c586d1e4ca19"),
	("model/gameobjects/Npc.java", "method canPurchase", "7b57bccf62c67470"),
	("model/templates/npc/NpcTemplate.java", "method getFuncDialogIds", "0492042900d188ab"),
	("model/templates/npc/NpcTemplate.java", "method supportsAction", "62726a83b31e47a4"),
	("model/templates/npc/NpcTemplate.java", "method getAiName", "ea4521cc79685505"),
	("model/templates/npc/NpcTemplate.java", "method getTalkDistance", "9e4b01452f321734"),
	("utils/PositionUtil.java", "method isInTalkRange(Creature creature, Npc npc)", "e82076de9b588ded"),
	("model/templates/npc/TalkInfo.java", "class TalkInfo", "f1b9c8609047b988"),
	("dataholders/NpcData.java", "method init", "5fadfcd4a6253a4f"),
	("dataholders/NpcData.java", "method isFunctionDialog", "b88f0851a575a818"),
	("network/aion/serverpackets/SM_TRADELIST.java", "class SM_TRADELIST", "0ca195f8620a7b25"),
	("network/aion/serverpackets/SM_SELL_ITEM.java", "class SM_SELL_ITEM", "091332e2c127d183"),
	# the static data holders
	("model/templates/item/ItemTemplate.java", "method getMask", "914474bb0270c2f4"),
	("model/templates/item/ItemTemplate.java", "method getPrice", "c08e7538aeebcf84"),
	("model/templates/item/ItemTemplate.java", "method getAcquisition", "23e4bda82411d1b7"),
	("model/templates/item/Acquisition.java", "class Acquisition", "ffc75056ddf33262"),
	("model/templates/restriction/ItemCleanupTemplate.java", "class ItemCleanupTemplate", "84a016cb98c9d2f4"),
	("dataholders/ItemData.java", "method afterUnmarshal", "3d7a5eaed8e2a6af"),
	("dataholders/ItemData.java", "method cleanup", "fd8dae09cd1f60f9"),
	("dataholders/ItemData.java", "method applyCleanup", "bc24a7e1fb190157"),
	("dataholders/ItemData.java", "method getItemTemplate", "2e4fc0ab6672fd3b"),
	("dataholders/TradeListData.java", "method afterUnmarshal", "0c5f69252f96f459"),
	("dataholders/TradeListData.java", "method getTradeListTemplate(int id)", "3af2ffa3346f9672"),
	("dataholders/TradeListData.java", "method getTradeListTemplate()", "c8da1660ae7a0766"),
	("dataholders/TradeListData.java", "method getTradeInListTemplate", "938920e72f17d5a8"),
	("dataholders/TradeListData.java", "method getPurchaseTemplate", "ec1c13f51ee5f2a7"),
	("dataholders/GoodsListData.java", "method afterUnmarshal", "0fdc825cadbef9e1"),
	("dataholders/GoodsListData.java", "method getGoodsListById", "5e1779939ce1f6da"),
	("dataholders/GoodsListData.java", "method getGoodsPurchaseListById", "538cea72c199b56a"),
	("model/templates/goods/GoodsList.java", "class GoodsList", "0df4e2f805a51ddb"),
	# TradeListTemplate: the getters (the field defaults are literals JavaTradeRules.read reads)
	("model/templates/tradelist/TradeListTemplate.java", "method getTradeTablist", "ddbeeff1fa9fac6a"),
	("model/templates/tradelist/TradeListTemplate.java", "method getNpcId", "6bd180a96e685307"),
	("model/templates/tradelist/TradeListTemplate.java", "method getTradeNpcType", "325602b02001fc59"),
	("model/templates/tradelist/TradeListTemplate.java", "method getSellPriceRate", "b61daa04c02da65b"),
	("model/templates/tradelist/TradeListTemplate.java", "method getSellPriceRate2", "caef86079aa587f0"),
	("model/templates/tradelist/TradeListTemplate.java", "method getApSellPriceRate2", "e42c965ae84d0240"),
	("model/templates/tradelist/TradeListTemplate.java", "method getBuyPriceRate", "4de31691250f19fb"),
	("model/templates/tradelist/TradeListTemplate.java", "class TradeTab", "91c1df378fb0d858"),
	# limited items
	("services/LimitedItemTradeService.java", "method start", "57e02123ea435ca7"),
	("services/LimitedItemTradeService.java", "method getLimitedItem", "3665594518ece588"),
	("services/LimitedItemTradeService.java", "method getLimitedTradeNpc", "3d311f15e05e28a6"),
	("services/cron/CronExpressions.java", "method getOrCreate", "d2483becf1ec615e"),
	("model/limiteditems/LimitedItem.java", "class LimitedItem", "fda0e6f6c9f52001"),
	("model/limiteditems/LimitedTradeNpc.java", "class LimitedTradeNpc", "21d6594463c396b0"),
)

# Every Java file JavaTradeRules.read and trade_config.property_defaults read, below com/aionemu/gameserver
JAVA_SOURCES = tuple(sorted({relative for relative, _ in MODELLED_STATEMENTS} | {relative for relative, _, _ in MODELLED_MEMBERS} | {
	"model/DialogAction.java", "model/templates/tradelist/TradeNpcType.java", "model/templates/item/AcquisitionType.java", "model/items/ItemMask.java",
	"model/SellLimit.java", "model/templates/tradelist/TradeListTemplate.java", "network/aion/clientpackets/CM_BUY_ITEM.java",
	"configs/main/PricesConfig.java", "configs/main/SiegeConfig.java", "configs/main/CustomConfig.java", "configs/main/RatesConfig.java",
	"configs/main/GSConfig.java"}))

# a comment, or a literal whose text a comment marker inside must not end: text blocks, strings and chars
_JAVA_TOKEN = re.compile(r'//[^\n]*|/\*.*?\*/|"""(?:\\.|[^\\])*?"""|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', re.DOTALL)
_JAVA_LITERAL = re.compile(r'"""(?:\\.|[^\\])*?"""|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', re.DOTALL)


def _read(path: Path) -> str:
	try:
		return path.read_text(encoding="utf-8")
	except OSError as e:
		raise OracleError(f"{path}: {e}") from e


def _strip_comments(text: str) -> str:
	"""The Java text with every comment replaced by one space; string, text block and char literals are kept, so a `//` or `/*` inside one is
	no comment."""
	return _JAVA_TOKEN.sub(lambda m: " " if m.group(0).startswith("/") else m.group(0), text)


def normalize_java(text: str) -> str:
	"""Comments and all white space removed: the form MODELLED_STATEMENTS and MODELLED_MEMBERS are compared in."""
	return re.sub(r"\s+", "", _strip_comments(text))


def _mask_literals(code: str) -> str:
	"""The code with the inside of every literal replaced by x (same length), so brackets inside a string do not count."""
	return _JAVA_LITERAL.sub(lambda m: m.group(0)[0] + "x" * (len(m.group(0)) - 2) + m.group(0)[-1], code)


def _closing(masked: str, index: int, what: str) -> int:
	"""The index of the bracket that closes the one at `index`."""
	opener = masked[index]
	closer = {"{": "}", "(": ")"}[opener]
	depth = 0
	for i in range(index, len(masked)):
		if masked[i] == opener:
			depth += 1
		elif masked[i] == closer:
			depth -= 1
			if depth == 0:
				return i
	raise OracleError(f"{what}: unbalanced {opener}")


def _member_start(masked: str, index: int) -> int:
	"""Where the declaration that contains `index` starts: after the previous `;`, `{` or `}` (its annotations and modifiers included)."""
	return max(masked.rfind(c, 0, index) for c in ";{}") + 1


def java_member(text: str, member: str, what: str) -> str:
	"""The comment-free text of one member of a Java source (see MODELLED_MEMBERS); refused unless exactly one declaration matches."""
	code = _strip_comments(text)
	masked = _mask_literals(code)
	kind, _, spec = member.partition(" ")
	found: list[tuple[int, int]] = []
	if kind == "method":
		name, paren, params = spec.partition("(")
		wanted = re.sub(r"\s+", "", params[:-1]) if paren else None
		for match in re.finditer(rf"\b{re.escape(name)}\s*\(", masked):
			before = masked[:match.start()].rstrip()
			if before.endswith(".") or re.search(r"\bnew$", before):
				continue  # a call or an instance creation
			open_paren = match.end() - 1
			close_paren = _closing(masked, open_paren, what)
			body = re.match(r"\s*(?:throws\s+[\w.,\s]+?)?\s*\{", masked[close_paren + 1:])
			if not body:
				continue  # a call
			if wanted is not None and re.sub(r"\s+", "", code[open_paren + 1:close_paren]) != wanted:
				continue
			brace = close_paren + body.end()
			found.append((_member_start(masked, match.start()), _closing(masked, brace, what)))
	elif kind == "class":
		for match in re.finditer(rf"\b(?:class|enum|interface|record)\s+{re.escape(spec)}\b", masked):
			brace = masked.find("{", match.end())
			if brace < 0:
				raise OracleError(f"{what}: {member} has no body")
			found.append((_member_start(masked, match.start()), _closing(masked, brace, what)))
	elif kind == "block":
		for match in re.finditer(rf"\b{re.escape(spec)}\s*\{{", masked):
			found.append((match.start(), _closing(masked, match.end() - 1, what)))
	elif kind == "case":
		label = re.compile(r"\s*(?:case\s+[\w.]+\s*:|default\s*:)")
		for match in re.finditer(rf"\bcase\s+{re.escape(spec)}\s*:", masked):
			i = match.end()
			while (more := label.match(masked, i)) is not None:
				i = more.end()
			depth = 0
			while i < len(masked):
				c = masked[i]
				if c == "{":
					depth += 1
				elif c == "}":
					if depth == 0:
						break
					depth -= 1
				elif depth == 0 and (c == "c" or c == "d") and not (masked[i - 1].isalnum() or masked[i - 1] == "_") and label.match(masked, i):
					break
				i += 1
			found.append((match.start(), i - 1))
	else:
		raise OracleError(f"{what}: unknown member kind {member!r}")
	if len(found) != 1:
		raise OracleError(f"{what}: {len(found)} declarations of `{member}`, expected exactly one: the Java source does not have the shape this oracle "
		                  "was written against")
	start, end = found[0]
	return code[start:end + 1]


def member_fingerprint(text: str, member: str, what: str) -> str:
	"""The fingerprint MODELLED_MEMBERS stores: sha256 of the member's text without comments and white space, the first 16 hex digits."""
	return hashlib.sha256(re.sub(r"\s+", "", java_member(text, member, what)).encode("utf-8")).hexdigest()[:16]


def _int32(value: int) -> int:
	"""Java int arithmetic: the value wrapped to 32 bits two's complement."""
	return (value + 2**31) % 2**32 - 2**31


def _long(value: int, what: str) -> int:
	if not LONG_MIN <= value <= LONG_MAX:
		raise OracleError(f"{what}: {value} overflows a Java long, which the oracle does not model")
	return value


def _java_div(a: int, b: int) -> int:
	"""Java integer division: truncation toward zero."""
	q = abs(a) // abs(b)
	return q if (a >= 0) == (b > 0) else -q


def times_div_100d(value: int, factor: int, what: str) -> int:
	"""Java `(long) (value * factor / 100D)` for a long value and an int factor: the long product, converted to double, divided by 100.0, truncated."""
	return to_long(float(_long(value * factor, what)) / 100.0)


def influence_rate(influence: int) -> float:
	"""Influence.recalculateInfluence: getInfluence(race) / 100f (an int divided by a float)."""
	return f32(f32(influence) / f32(100.0))


def global_prices(default_prices: int, rate: float) -> int:
	"""PricesService.getGlobalPrices (PricesService.java:21-33), float arithmetic and Math.round(float)."""
	half = f32(0.5)
	if rate == half:
		return default_prices
	if rate > half:
		diff = f32(rate - half)
		return round_to_int(f32(f32(default_prices) - f32(f32(diff / 2) * 100)))
	diff = f32(half - rate)
	return round_to_int(f32(f32(default_prices) + f32(f32(diff / 2) * 100)))


def taxes(default_tax: int, rate: float) -> int:
	"""PricesService.getTaxes (PricesService.java:45-53)."""
	half = f32(0.5)
	if rate >= half:
		return default_tax
	diff = f32(half - rate)
	return round_to_int(f32(f32(default_tax) + f32(f32(diff / 4) * 100)))


def buy_price(price: int, vendor_buy_modifier: int, prices: int, prices_modifier: int, tax: int) -> int:
	"""PricesService.getBuyPrice (PricesService.java:95-99): four truncations, in this order."""
	value = times_div_100d(price, vendor_buy_modifier, "getBuyPrice price * VENDOR_BUY_MODIFIER")
	value = times_div_100d(value, prices, "getBuyPrice * getGlobalPrices")
	value = times_div_100d(value, prices_modifier, "getBuyPrice * getGlobalPricesModifier")
	return times_div_100d(value, tax, "getBuyPrice * getTaxes")


def buy_list_price(unit_price: int, count: int, modifier: int) -> int:
	"""TradeList.calculateBuyListPrice's term (TradeList.java:53): getBuyPrice * count * modifier / 100, long arithmetic."""
	return _java_div(_long(_long(unit_price * count, "buy price * count") * modifier, "buy price * count * modifier"), 100)


def required_ap(ap: int, count: int, modifier: int, vendor_buy_modifier: int) -> int:
	"""TradeList.calculateAbyssRewardBuyList's term (TradeList.java:71): (int) ((ap * count * modifier / 100.0D) * VENDOR_BUY_MODIFIER) / 100."""
	value = float(_long(_long(ap * count, "ap * count") * modifier, "ap * count * modifier")) / 100.0
	return _java_div(to_int(value * vendor_buy_modifier), 100)


def sell_reward(price: int, modifier: int) -> int:
	"""PricesService.getSellReward (PricesService.java:104-106)."""
	return times_div_100d(price, modifier, "getSellReward price * sellModifier")


def purchase_reward(price: int, buy_price_rate: int) -> int:
	"""TradeService.performSellToShop with a purchase template (TradeService.java:214)."""
	return times_div_100d(price, buy_price_rate, "price * buy_price_rate")


def ap_sale(ap: int, buy_price_rate: int, count: int) -> int:
	"""TradeService.performSellForAPToShop (TradeService.java:280-282): the argument of AbyssPointsService.addAp."""
	ap_to_add = round_to_int(f32(f32(_int32(ap * buy_price_rate)) / f32(100.0)))
	return _int32(ap_to_add * _int32(count))


def fresh_sell_limit(base: int, rate: float) -> int:
	"""Rates.SELL_LIMIT.calcResult: (long) (sellLimit * rate) - a long times a float is a float product."""
	return to_long(f32(f32(base) * rate))


def limited_sale(limit: int, reward: int, count: int, dynamic_cap: bool) -> dict:
	"""PlayerLimitService.updateSellLimit (PlayerLimitService.java:22-49) past its early return, for an account whose limit is `limit`."""
	if reward < 0 or count <= 0:
		return {"soldCount": 0, "possibleCount": None, "remainingLimit": limit, "message": None}
	possible = max(0, _java_div(limit, reward))
	if dynamic_cap and possible < count:
		possible += 1
	if possible == 0 or limit == 0:
		return {"soldCount": 0, "possibleCount": possible, "remainingLimit": limit, "message": "STR_MSG_DAY_CANNOT_SELL_NPC"}
	sold = min(possible, count)
	return {"soldCount": sold, "possibleCount": possible, "remainingLimit": limit - min(limit, _long(reward * sold, "reward * count")), "message": None}


@dataclass(frozen=True)
class JavaTradeRules:
	"""The literals and tables of the trade rules, read from the Java sources; read() also checks MODELLED_STATEMENTS."""

	dialog_actions: dict[str, int]                 # DialogAction BUY, SELL, TRADE_IN, TRADE_SELL_LIST
	npc_type_index: dict[str, int]                 # TradeNpcType constant -> index() (what SM_TRADELIST and SM_SELL_ITEM write)
	acquisition_types: tuple[str, ...]             # AcquisitionType constants
	item_masks: dict[str, int]                     # ItemMask constants
	sell_limits: tuple[tuple[int, int, int], ...]  # SellLimit (playerMinLevel, playerMaxLevel, limit)
	tradelist_defaults: dict[str, object]          # TradeListTemplate JAXB field defaults
	max_count: int                                 # CM_BUY_ITEM: `count > N` is an audit

	@staticmethod
	def read(java_src: Path) -> "JavaTradeRules":
		base = Path(java_src) / "com" / "aionemu" / "gameserver"
		texts: dict[str, str] = {}
		normalized: dict[str, str] = {}
		for relative, statement in MODELLED_STATEMENTS:
			if relative not in normalized:
				texts[relative] = _read(base / relative)
				normalized[relative] = normalize_java(texts[relative])
			if normalize_java(statement) not in normalized[relative]:
				raise OracleError(f"{relative} no longer contains `{statement[:120]}{'...' if len(statement) > 120 else ''}`: the Java source does not "
				                  "have the shape this oracle was written against")
		for relative, member, fingerprint in MODELLED_MEMBERS:
			if relative not in texts:
				texts[relative] = _read(base / relative)
			actual = member_fingerprint(texts[relative], member, relative)
			if actual != fingerprint:
				raise OracleError(f"{relative}: `{member}` changed (fingerprint {actual}, the oracle models {fingerprint}): the Java code this oracle models "
				                  "is not the one it was written against")

		dialog_java = _strip_comments(_read(base / "model" / "DialogAction.java"))
		actions = {}
		for name in ("BUY", "SELL", "TRADE_IN", "TRADE_SELL_LIST"):
			match = re.search(rf"public\s+static\s+final\s+int\s+{name}\s*=\s*(\d+)\s*;", dialog_java)
			if not match:
				raise OracleError(f"DialogAction.java: no `public static final int {name} = N;`")
			actions[name] = int(match.group(1))

		npc_types = {}
		for name, args in enum_constants(base / "model" / "templates" / "tradelist" / "TradeNpcType.java", "TradeNpcType"):
			npc_types[name] = java_int((args or "").strip(), f"TradeNpcType.{name} index")
		acquisitions = tuple(name for name, _ in enum_constants(base / "model" / "templates" / "item" / "AcquisitionType.java", "AcquisitionType"))

		masks = {}
		for name, value in re.findall(r"public\s+static\s+final\s+int\s+(\w+)\s*=\s*([^;]+);", _strip_comments(_read(base / "model" / "items" /
		                                                                                                              "ItemMask.java"))):
			shift = re.fullmatch(r"\(?\s*1\s*<<\s*(\d+)\s*\)?", value.strip())
			masks[name] = 1 << int(shift.group(1)) if shift else java_int(value.strip(), f"ItemMask.{name}")

		limits = []
		for name, args in enum_constants(base / "model" / "SellLimit.java", "SellLimit"):
			parts = [p.strip() for p in (args or "").split(",")]
			if len(parts) != 3:
				raise OracleError(f"SellLimit.{name}: expected (playerMinLevel, playerMaxLevel, limit)")
			limits.append((java_int(parts[0], f"SellLimit.{name}"), java_int(parts[1], f"SellLimit.{name}"), _long(int(parts[2]), f"SellLimit.{name}")))

		template_java = _strip_comments(_read(base / "model" / "templates" / "tradelist" / "TradeListTemplate.java"))
		defaults: dict[str, object] = {}
		for attribute, pattern in (("npc_type", r"TradeNpcType\s+tradeNpcType\s*=\s*TradeNpcType\.(\w+)\s*;"),
		                           ("sell_price_rate", r"int\s+sellPriceRate\s*=\s*(\d+)\s*;"),
		                           ("sell_price_rate2", r"int\s+sellPriceRate2\s*=\s*(\d+)\s*;"),
		                           ("ap_sell_price_rate2", r"int\s+apSellPriceRate2\s*=\s*(\d+)\s*;"),
		                           ("buy_price_rate", r"int\s+buyPriceRate\s*(?:=\s*(\d+)\s*)?;")):
			match = re.search(rf'@XmlAttribute\(name\s*=\s*"{attribute}"\)\s*private\s+{pattern}', template_java)
			if not match:
				raise OracleError(f"TradeListTemplate.java: the field of {attribute} does not have the shape this oracle was written against")
			value = match.group(1)
			defaults[attribute] = value if attribute == "npc_type" else int(value or 0)

		buy_item = _strip_comments(_read(base / "network" / "aion" / "clientpackets" / "CM_BUY_ITEM.java"))
		max_count = re.search(r"\|\|\s*count\s*>\s*(\d+)\s*\)", buy_item)
		if not max_count:
			raise OracleError("CM_BUY_ITEM.java: the count cap was not found")
		for mask in ("SELLABLE", *(name for _, name in CLEANUP_MASKS)):
			if mask not in masks:
				raise OracleError(f"ItemMask.java: no {mask}")
		for npc_type in NPC_TYPES_WITH_KINAH + NPC_TYPES_WITHOUT_KINAH:
			if npc_type not in npc_types:
				raise OracleError(f"TradeNpcType has no {npc_type}")
		return JavaTradeRules(actions, npc_types, acquisitions, masks, tuple(limits), defaults, int(max_count.group(1)))


@dataclass(frozen=True)
class TradeListInfo:
	npc_id: int
	npc_type: str
	sell_price_rate: int
	sell_price_rate2: int
	ap_sell_price_rate2: int
	buy_price_rate: int
	tabs: tuple[int, ...]


@dataclass(frozen=True)
class GoodsListInfo:
	list_id: int
	legion_level: int
	items: tuple[tuple[int, int | None, int | None], ...]  # (item id, sell_limit, buy_limit); the limits are Integer, null when absent
	sales_time: str | None


@dataclass(frozen=True)
class ItemInfo:
	item_id: int
	name: str | None
	price: int
	template_mask: int
	mask: int                         # after ItemData.cleanup
	acquisition: dict[str, str] | None
	cleanup: dict[str, int] | None


@dataclass(frozen=True)
class NpcInfo:
	npc_id: int
	name: str | None
	race: str
	level: int
	npc_type: str
	func_dialogs: tuple[int, ...] | None  # TalkInfo.funcDialogIds: null without <talk_info> or without func_dialogs
	ai: str | None = None                 # NpcTemplate.getAiName: the `ai` attribute
	talk_distance: int = 2                # NpcTemplate.getTalkDistance: 2 without <talk_info>, else TalkInfo's distance (default 2)
	sub_dialog_type: str | None = None    # TalkInfo.subDialogType


# Quartz CronExpression shapes the oracle accepts for a limited item's <salestime>, which LimitedItemTradeService.start schedules at startup
# (CronExpressions.getOrCreate: a null expression is a NullPointerException, an invalid one a RuntimeException, and the server does not start):
# second minute hour of plain values and ranges, `?` for the day of the month, `*` for the month, `*` or day names for the day of the week.
# Every other shape is refused as not modelled.
_CRON_DAYS = ("SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT")


def check_sales_time(sales_time: str | None, what: str) -> None:
	if sales_time is None:
		raise OracleError(f"{what}: a limited item's goods list has no <salestime>: LimitedItemTradeService.start schedules null "
		                  "(NullPointerException in CronExpressions.getOrCreate) and the game server does not start")
	fields = [f for f in re.split(r"[ \t]+", sales_time.upper()) if f]  # CronExpression: upper case, StringTokenizer over " \t"
	bounds = ((0, 59), (0, 59), (0, 23))
	ok = len(fields) == 6 and fields[3] == "?" and fields[4] == "*"
	if ok:
		for text, (low, high) in zip(fields[:3], bounds):
			for part in text.split(","):
				ends = re.fullmatch(r"(\d{1,2})(?:-(\d{1,2}))?", part)
				if part == "*" and text == "*":
					continue
				if not ends or not low <= int(ends.group(1)) <= (int(ends.group(2)) if ends.group(2) else int(ends.group(1))) <= high:
					ok = False
		ok = ok and (fields[5] == "*" or all(day in _CRON_DAYS for day in fields[5].split(",")))
	if not ok:
		raise OracleError(f"{what}: <salestime> {sales_time!r} is not a cron shape the oracle models (Quartz CronExpression; an invalid one stops the "
		                  "game server at startup)")


class TradeData:
	"""The static data of the trade rules: trade lists, goods lists, item and npc templates (the fields the rules read), cleanups."""

	def __init__(self, data: StaticData, rules: JavaTradeRules):
		self.data = data
		self.rules = rules
		self.tradelists: dict[int, TradeListInfo] = {}
		self.tradeins: dict[int, TradeListInfo] = {}
		self.purchases: dict[int, TradeListInfo] = {}
		holders = {"tradelist_template": self.tradelists, "trade_in_list_template": self.tradeins, "purchase_template": self.purchases}
		seen: set[str] = set()
		for element in data.children("npc_trade_list"):
			if element.tag in holders:
				seen.add(element.tag)
				info = self._tradelist(element)
				holders[element.tag][info.npc_id] = info  # TradeListData.afterUnmarshal: a later template of the npc replaces the earlier one
		self.goods: dict[int, GoodsListInfo] = {}
		self.in_goods: dict[int, GoodsListInfo] = {}
		self.purchase_goods: dict[int, GoodsListInfo] = {}
		lists = {"list": self.goods, "in_list": self.in_goods, "purchase_list": self.purchase_goods}
		for element in data.children("goodslists"):
			if element.tag in lists:
				seen.add(element.tag)
				info = self._goods_list(element)
				lists[element.tag][info.list_id] = info  # GoodsListData.afterUnmarshal: a later list of the id replaces the earlier one
		missing = [tag for tag in (*holders, *lists) if tag not in seen]
		if missing:
			raise OracleError(f"no <{missing[0]}> in the static data: TradeListData / GoodsListData.afterUnmarshal iterate that list, which JAXB leaves "
			                  "null (NullPointerException at startup)")
		for npc_id, template in self.tradelists.items():  # LimitedItemTradeService.start schedules every limited item's sales time
			for tab in template.tabs:
				goods = self.goods.get(tab)
				if goods is not None and any(s is not None and b is not None for _, s, b in goods.items):
					check_sales_time(goods.sales_time, f"<list id={tab}> (a tab of npc {npc_id})")
		cleanups: dict[int, list[dict[str, int]]] = {}
		for element in data.children("item_restriction_cleanups", "cleanup"):
			item_id = java_int(element.get("id"), "cleanup id")
			values = {}
			for attribute, _ in CLEANUP_MASKS:
				value = java_int(element.get(attribute), f"cleanup {item_id} {attribute}", -1)
				if not -128 <= value <= 127:
					raise OracleError(f"cleanup {item_id} {attribute}={value} is out of the byte range")
				values[attribute] = value
			cleanups.setdefault(item_id, []).append(values)
		self.items = self._items(data, cleanups)
		self.npcs = self._npcs(data)

	def _tradelist(self, element) -> TradeListInfo:
		npc_id = java_int(element.get("npc_id"), f"<{element.tag}> npc_id")
		what = f"<{element.tag} npc_id={npc_id}>"
		defaults = self.rules.tradelist_defaults
		npc_type = element.get("npc_type", defaults["npc_type"])
		if npc_type not in self.rules.npc_type_index:
			raise OracleError(f"{what}: npc_type {npc_type} is not a TradeNpcType constant (JAXB would leave the field null)")
		tabs = tuple(java_int(tab.get("id"), f"{what} tradelist id", 0) for tab in element.findall("tradelist"))
		return TradeListInfo(npc_id, npc_type, java_int(element.get("sell_price_rate"), f"{what} sell_price_rate", defaults["sell_price_rate"]),
		                     java_int(element.get("sell_price_rate2"), f"{what} sell_price_rate2", defaults["sell_price_rate2"]),
		                     java_int(element.get("ap_sell_price_rate2"), f"{what} ap_sell_price_rate2", defaults["ap_sell_price_rate2"]),
		                     java_int(element.get("buy_price_rate"), f"{what} buy_price_rate", defaults["buy_price_rate"]), tabs)

	@staticmethod
	def _goods_list(element) -> GoodsListInfo:
		list_id = java_int(element.get("id"), f"<{element.tag}> id", 0)
		what = f"<{element.tag} id={list_id}>"
		sales = element.findall("salestime")
		if len(sales) > 1:
			raise OracleError(f"{what}: more than one <salestime>, which the oracle does not model")
		items = []
		for item in element.findall("item"):
			item_id = java_int(item.get("id"), f"{what} item id", 0)
			sell_limit = java_int(item.get("sell_limit"), f"{what} item {item_id} sell_limit") if item.get("sell_limit") is not None else None
			buy_limit = java_int(item.get("buy_limit"), f"{what} item {item_id} buy_limit") if item.get("buy_limit") is not None else None
			items.append((item_id, sell_limit, buy_limit))
		return GoodsListInfo(list_id, java_int(element.get("legion_lvl"), f"{what} legion_lvl", 0), tuple(items),
		                     (sales[0].text or "") if sales else None)  # JAXB: an empty <salestime/> is "", a missing one null

	def _items(self, data: StaticData, cleanups: dict[int, list[dict[str, int]]]) -> dict[int, ItemInfo]:
		raw: dict[int, tuple] = {}
		for element in data.stream("item_templates", "item_template"):
			item_id = java_int(element.get("id"), "item_template id")
			acquisition = element.find("acquisition")
			raw[item_id] = (element.get("name"), java_int(element.get("price"), f"item {item_id} price", 0),
			                java_int(element.get("mask"), f"item {item_id} mask", 0), dict(acquisition.attrib) if acquisition is not None else None)
		items = {}
		for item_id, (name, price, mask, acquisition) in raw.items():
			items[item_id] = ItemInfo(item_id, name, price, mask, mask, acquisition, None)
		for item_id, entries in cleanups.items():
			if item_id not in items:
				# ItemData.cleanup passes the null template to applyCleanup, whose switch dereferences it only for a result of 1 or 0
				if any(values[attribute] in (0, 1) for values in entries for attribute, _ in CLEANUP_MASKS):
					raise OracleError(f"item_restriction_cleanups names item {item_id}, which has no template, with a result 0 or 1 (ItemData.applyCleanup: "
					                  "NullPointerException at startup)")
				continue
			info = items[item_id]
			mask = info.mask
			for values in entries:  # ItemData.cleanup: in document order, each result 1 sets and 0 clears its bit, anything else leaves it
				for attribute, mask_name in CLEANUP_MASKS:
					bit = self.rules.item_masks[mask_name]
					if values[attribute] == 1:
						mask |= bit
					elif values[attribute] == 0:
						mask &= ~bit
			items[item_id] = ItemInfo(item_id, info.name, info.price, info.template_mask, _int32(mask), info.acquisition, entries[-1])
		return items

	def _npcs(self, data: StaticData) -> dict[int, NpcInfo]:
		npcs = {}
		self.function_dialogs: set[int] = set()  # NpcData.init: the func_dialogs of EVERY template, a replaced duplicate too (NpcData.java:89-90)
		for element in data.stream("npc_templates", "npc_template"):
			npc_id = java_int(element.get("npc_id"), "npc_template npc_id")
			talk = element.find("talk_info")
			funcs = None
			if talk is not None and talk.get("func_dialogs") is not None:
				funcs = tuple(java_int(token, f"npc {npc_id} func_dialogs") for token in talk.get("func_dialogs").split())
				self.function_dialogs.update(funcs)
			npcs[npc_id] = NpcInfo(npc_id, element.get("name"), element.get("race", "NONE"), java_int(element.get("level"), f"npc {npc_id} level", 0),
			                       element.get("type", "NONE"), funcs, element.get("ai"),
			                       java_int(talk.get("distance"), f"npc {npc_id} talk_info distance", 2) if talk is not None else 2,
			                       talk.get("subdialog_type") if talk is not None else None)
		return npcs

	def npc(self, npc_id: int) -> NpcInfo:
		if npc_id not in self.npcs:
			raise OracleError(f"no npc_template with npc_id {npc_id}")
		return self.npcs[npc_id]

	def item(self, item_id: int, why: str) -> ItemInfo:
		if item_id not in self.items:
			raise OracleError(f"item {item_id} has no item_template ({why})")
		return self.items[item_id]

	def supports(self, npc: NpcInfo, action: str) -> bool:
		"""NpcTemplate.supportsAction."""
		return npc.func_dialogs is not None and self.rules.dialog_actions[action] in npc.func_dialogs

	def functions(self, npc: NpcInfo) -> dict[str, bool]:
		"""Npc.canSell, canBuy, canTradeIn and canPurchase (Npc.java:361-386)."""
		can_sell = npc.npc_id in self.tradelists and self.supports(npc, "BUY")
		return {"canSell": can_sell, "canBuy": self.supports(npc, "SELL") or can_sell,
		        "canTradeIn": npc.npc_id in self.tradeins and self.supports(npc, "TRADE_IN"),
		        "canPurchase": npc.npc_id in self.purchases and self.supports(npc, "TRADE_SELL_LIST")}

	def dialog_gate(self, npc: NpcInfo, action: str) -> dict:
		"""CM_DIALOG_SELECT.runImpl's npc gate (CM_DIALOG_SELECT.java:112-116): a dialog id that any npc template lists in its func_dialogs
		(NpcData.isFunctionDialog) and this npc does not support is an audit, and nothing is sent."""
		action_id = self.rules.dialog_actions[action]
		function = action_id in self.function_dialogs
		supported = self.supports(npc, action)
		return {"action": action, "id": action_id, "functionDialog": function, "supported": supported, "passes": supported or not function}

	def limited_items(self, npc_id: int) -> list[dict]:
		"""LimitedItemTradeService.start: the npc's LimitedTradeNpc list - every tab of its tradelist_template in order (a missing goods list is
		skipped), every item with both limits, as a fresh server holds them (sell limit at its default, no player has bought any)."""
		template = self.tradelists.get(npc_id)
		result = []
		for tab in template.tabs if template else ():
			goods = self.goods.get(tab)
			if goods is None:
				continue
			for item_id, sell_limit, buy_limit in goods.items:
				if sell_limit is not None and buy_limit is not None:
					result.append({"itemId": item_id, "sellLimit": sell_limit, "buyLimit": buy_limit, "salesTime": goods.sales_time, "list": tab})
		return result


class DialogAi:
	"""The npc's AI, which NpcController.onDialogSelect asks before DialogService (NpcController.java:266-272): AIEngine.newAI (AIEngine.java:64-85)
	over the @AIName classes of src/.../ai and data/handlers/ai (the M5b-3 lane's AiCatalog), DummyAI without a name, and whether a class of its
	extends chain overrides onDialogSelect - AbstractAI's answers false (AbstractAI.java:384-387), an override is not modelled."""

	def __init__(self, java_src: Path, handlers_dir: Path):
		from m5b3.drops import AiCatalog
		self.catalog = AiCatalog(Path(java_src), Path(handlers_dir))
		self._resolved: dict[str | None, dict] = {}
		self._checked: list[dict[int, NpcInfo]] = []

	def check_template_names(self, npcs: dict[int, NpcInfo]) -> None:
		"""AIEngine.validateScripts (AIEngine.java:111-114): an npc_template ai that no AI class carries stops the game server at startup."""
		if any(npcs is checked for checked in self._checked):
			return
		missing = sorted({n.ai for n in npcs.values() if n.ai is not None} - set(self.catalog.by_ai_name) - set(self.catalog.nested_ai_names))
		if missing:
			raise OracleError(f"no AI class is named {', '.join(map(repr, missing[:5]))} (npc_template ai): AIEngine.validateScripts throws a "
			                  "GameServerError at startup")
		self._checked.append(npcs)

	def resolve(self, name: str | None) -> dict:
		if name not in self._resolved:
			if name is None:  # AIEngine.newAI(null): DummyAI (fingerprinted in MODELLED_MEMBERS) extends AITemplate
				classes, chain = self.catalog.by_class["AITemplate"], ["DummyAI", "AITemplate"]
			else:
				if name in self.catalog.nested_ai_names:
					raise OracleError(f"AI {name!r} is a nested class ({self.catalog.nested_ai_names[name].name}), which the oracle does not model")
				cls = self.catalog.by_ai_name.get(name)
				if cls is None:
					raise OracleError(f"no AI class is named {name!r}: AIEngine.newAI throws IllegalArgumentException and the npc does not spawn")
				classes = self.catalog.chain(cls)
				chain = [c.name for c in classes]
			overrides = [c.name for c in classes if re.search(r"\bboolean\s+onDialogSelect\s*\(", _mask_literals(_strip_comments(_read(c.path))))]
			self._resolved[name] = {"aiName": name, "classChain": chain, "overridesOnDialogSelect": overrides}
		return self._resolved[name]


@dataclass
class TradeContext:
	rules: JavaTradeRules
	trade: TradeData
	config: dict[str, ConfigValue]
	races: tuple[str, ...]
	prices: dict[str, dict]
	count: int
	legion_level: int
	account_max_level: int
	membership: int
	java_src: Path | None = None
	handlers_dir: Path | None = None
	dialog_ai: DialogAi | None = None

	def cfg(self, key: str):
		return self.config[key].value

	def ai(self) -> DialogAi:
		"""The AI catalog, built on first use (the dialog paths of --npc and --map), with AIEngine.validateScripts' startup check."""
		if self.dialog_ai is None:
			if self.java_src is None:
				raise OracleError("the dialog path needs the Java sources and data/handlers (the npc's AI answers the dialog first)")
			self.dialog_ai = DialogAi(self.java_src, self.handlers_dir or Path(self.java_src).parent / "data" / "handlers")
		self.dialog_ai.check_template_names(self.trade.npcs)
		return self.dialog_ai


def race_prices(config: dict[str, ConfigValue], race: str, influence: int | None) -> dict:
	"""SM_PRICES and the factors of getBuyPrice for a player of `race`."""
	if config["gameserver.siege.enable"].value:
		if influence is None:
			raise OracleError(f"gameserver.siege.enable is true: the {race} influence comes from the siege locations' owners in the database "
			                  f"(SiegeDAO.loadSiegeLocations), which is not static data; pass --influence {race}=N (Influence.getInfluence)")
	elif influence is not None:
		raise OracleError("--influence with gameserver.siege.enable false: SiegeService holds no location, the influence is 0")
	else:
		influence = 0
	rate = influence_rate(influence)
	values = (global_prices(config["gameserver.prices.default.prices"].value, rate), config["gameserver.prices.default.modifier"].value,
	          taxes(config["gameserver.prices.default.taxes"].value, rate))
	return {"influence": influence, "influenceRate": rate, "globalPrices": values[0], "globalPricesModifier": values[1], "taxes": values[2],
	        "vendorBuyModifier": config["gameserver.prices.vendor.buymod"].value, "vendorSellModifier": config["gameserver.prices.vendor.sellmod"].value,
	        "smPrices": list(values), "smPricesBytes": [v & 0xFF for v in values]}


def unit_prices(ctx: TradeContext, price: int) -> dict[str, int]:
	return {race: buy_price(price, p["vendorBuyModifier"], p["globalPrices"], p["globalPricesModifier"], p["taxes"]) for race, p in ctx.prices.items()}


def _item_block(ctx: TradeContext, item: ItemInfo) -> dict:
	sellable_bit = ctx.rules.item_masks["SELLABLE"]
	return {"itemId": item.item_id, "name": item.name, "templatePrice": item.price, "templateMask": item.template_mask, "mask": item.mask,
	        "cleanup": item.cleanup, "sellable": (item.mask & sellable_bit) == sellable_bit, "acquisition": item.acquisition,
	        "unitPrice": unit_prices(ctx, item.price)}


def _acquisition(ctx: TradeContext, item: ItemInfo) -> tuple[str | None, int, int, int] | None:
	"""(type, ap, item, count) of the item's <acquisition> with the JAXB defaults; a missing or unknown type is refused (NullPointerException in
	calculateAbyssRewardBuyList's aquisition.getType().equals)."""
	if item.acquisition is None:
		return None
	attrs = item.acquisition
	what = f"item {item.item_id} <acquisition>"
	kind = attrs.get("type")
	if kind not in ctx.rules.acquisition_types:
		raise OracleError(f"{what}: type {kind!r} is not an AcquisitionType (JAXB leaves it null: NullPointerException in "
		                  "TradeList.calculateAbyssRewardBuyList)")
	return kind, java_int(attrs.get("ap"), f"{what} ap", 0), java_int(attrs.get("item"), f"{what} item", 0), java_int(attrs.get("count"), f"{what} count", 0)


def good_entry(ctx: TradeContext, npc: NpcInfo, template: TradeListInfo, item_id: int, count: int, sell_back: bool = True) -> dict:
	"""One item bought from the npc: TradeService.performBuyFromShop for a list holding `count` of it and nothing else."""
	trade = ctx.trade
	functions = trade.functions(npc)
	item = trade.item(item_id, "TradeItem.getItemTemplate() is null: NullPointerException in TradeList.calculateBuyListPrice")
	allowed = set()
	tabs = []
	shown = False
	for tab in template.tabs:
		goods = trade.goods.get(tab)
		if goods is not None and item_id in (i for i, _, _ in goods.items):
			allowed.add(item_id)
			tabs.append(tab)
			shown = shown or goods.legion_level <= ctx.legion_level
	npc_type = template.npc_type
	use_kinah = npc_type in NPC_TYPES_WITH_KINAH
	kinah_modifier = template.sell_price_rate2 if npc_type == "ABYSS_KINAH" else template.sell_price_rate
	ap_modifier = template.ap_sell_price_rate2 if npc_type == "ABYSS_KINAH" else template.sell_price_rate
	vendor_buy_modifier = ctx.cfg("gameserver.prices.vendor.buymod")

	failures: list[dict] = []
	if not functions["canSell"]:
		failures.append({"reason": "npc.canSell() is false: CM_BUY_ITEM.java:130 ignores the buy", "message": None})
	if npc_type not in NPC_TYPES_WITH_KINAH + NPC_TYPES_WITHOUT_KINAH:
		failures.append({"reason": f"TradeService.performBuyFromShop: unhandled TradeNpcType {npc_type}", "message": None})
	if item_id not in allowed:
		failures.append({"reason": "validateBuyItems: the item is in none of the npc's goods lists", "message": "STR_BUY_SELL_USER_BUY_FAILED"})

	acquisition = _acquisition(ctx, item)
	ap = 0
	required_items = []
	if acquisition is not None:
		kind, acquisition_ap, acquisition_item, acquisition_count = acquisition
		if kind in AP_ACQUISITIONS:
			ap = required_ap(acquisition_ap, count, ap_modifier, vendor_buy_modifier)
		if acquisition_item != 0:
			needed = _long(acquisition_count * count, "acquisition count * count")
			required_items.append({"itemId": acquisition_item, "count": needed})
			if needed < 1:
				failures.append({"reason": "calculateAbyssRewardBuyList: a required item count below 1", "message": "STR_MSG_NOT_ENOUGH_ABYSSPOINT"})
	if ap < 0:
		failures.append({"reason": "TradeService.java:111: getRequiredAp() < 0", "message": "STR_MSG_NOT_ENOUGH_ABYSSPOINT"})
	limited = next((entry for entry in trade.limited_items(npc.npc_id) if entry["itemId"] == item_id), None)
	if limited is not None:
		if (limited["sellLimit"] > 0 and limited["sellLimit"] - count < 0) or (limited["buyLimit"] > 0 and count > limited["buyLimit"]):
			failures.append({"reason": "canBuyLimitItem on a fresh server: the count exceeds the item's sell or buy limit",
			                 "message": "STR_MSG_LIMITED_BUYING_CANT_SELECT_NO_ITEMS"})

	units = unit_prices(ctx, item.price)
	entry = {
		"itemId": item_id,
		"name": item.name,
		"tabs": tabs,
		"shown": shown,
		"templatePrice": item.price,
		"acquisition": item.acquisition,
		"count": count,
		"unitPrice": units,
		# TradeList.calculateBuyListPrice's term for `count` of the item; 0 for the npc types that do not charge kinah
		"kinah": {race: buy_list_price(unit, count, kinah_modifier) if use_kinah else 0 for race, unit in units.items()},
		"requiredAp": ap,
		"requiredItems": required_items,
		"limited": limited,
		# the first failure in Java's order that no player state can avoid; `kinah`, `requiredAp` and `requiredItems` are what a buy that passes
		# it needs (plus PlayerRestrictions.canTrade and one free slot per list entry)
		"buyable": not failures,
		"failure": failures[0] if failures else None,
	}
	if sell_back:
		entry["sellBack"] = sale_entry(ctx, npc, item, count)
	return entry


def sale_entry(ctx: TradeContext, npc: NpcInfo | None, item: ItemInfo, count: int) -> dict:
	"""What selling `count` of the item to the npc pays: CM_BUY_ITEM action 1 (npc None: a vendor without a purchase_template)."""
	trade = ctx.trade
	sellable_bit = ctx.rules.item_masks["SELLABLE"]
	entry = {"itemId": item.item_id, "arm": None, "accepted": False, "failure": None, "sellable": (item.mask & sellable_bit) == sellable_bit,
	         "templatePrice": item.price, "sellModifier": None, "unitReward": None, "count": count, "soldCount": 0, "kinah": 0, "repurchasePrice": None,
	         "ap": None, "sellLimit": None}
	purchase = None
	if npc is not None:
		functions = trade.functions(npc)
		if not (functions["canBuy"] or functions["canPurchase"]):
			entry["failure"] = {"reason": "npc.canBuy() || npc.canPurchase() is false: CM_BUY_ITEM.java:114 ignores the sale", "message": None}
			return entry
		purchase = trade.purchases.get(npc.npc_id)

	if purchase is not None:
		# CM_BUY_ITEM passes the npc's purchase template whether or not canPurchase() holds (CM_BUY_ITEM.java:115-119)
		ap_arm = purchase.npc_type == "ABYSS"
		entry["arm"] = "PURCHASE_AP" if ap_arm else "PURCHASE"
		entry["sellModifier"] = purchase.buy_price_rate
		if ap_arm and not ctx.cfg("gameserver.selling.apitems.enabled"):  # performSellForAPToShop asks it first (TradeService.java:252-255)
			entry["failure"] = {"reason": "gameserver.selling.apitems.enabled is false: \"This feature is disabled\"", "message": None}
			return entry
		valid = False
		for tab in purchase.tabs:  # TradeService.java:205-211 / 270-276
			goods = trade.purchase_goods.get(tab)
			if goods is None:
				raise OracleError(f"npc {npc.npc_id}: purchase list {tab} does not exist and is read before the item is found (NullPointerException in "
				                  f"TradeService.{'performSellForAPToShop' if ap_arm else 'performSellToShop'})")
			if item.item_id in (i for i, _, _ in goods.items):
				valid = True
				break
		if not valid:
			entry["failure"] = {"reason": "the item is in none of the purchase lists (no message)", "message": None}
			return entry
		if ap_arm:
			if item.acquisition is None:
				raise OracleError(f"item {item.item_id} has no <acquisition>: NullPointerException in TradeService.performSellForAPToShop")
			# only getRequiredAp() is read (TradeService.java:280-282): the acquisition's type does not matter here
			ap = java_int(item.acquisition.get("ap"), f"item {item.item_id} <acquisition> ap", 0)
			entry.update(accepted=True, soldCount=count, ap=ap_sale(ap, purchase.buy_price_rate, count))
			return entry
		reward = purchase_reward(item.price, purchase.buy_price_rate)
	else:
		entry["arm"] = "VENDOR"
		entry["sellModifier"] = ctx.cfg("gameserver.prices.vendor.sellmod")
		if not entry["sellable"]:
			entry["failure"] = {"reason": "Item.isSellable() is false", "message": "STR_BUY_SELL_ITEM_CAN_NOT_BE_SELLED_TO_NPC"}
			return entry
		reward = sell_reward(item.price, entry["sellModifier"])

	sold = count
	if ctx.cfg("gameserver.limits.enable") and reward != 0:
		limit = account_sell_limit(ctx)
		result = limited_sale(limit, reward, count, ctx.cfg("gameserver.limits.enable_dynamic_cap"))
		entry["sellLimit"] = {"freshAccountLimit": limit, **result}
		sold = result["soldCount"]
	kinah = _long(reward * sold, "sellReward * count")
	entry.update(accepted=True, unitReward=reward, soldCount=sold, kinah=kinah, repurchasePrice=kinah if sold > 0 else None)
	return entry


def account_sell_limit(ctx: TradeContext) -> int:
	"""SellLimit.getSellLimit for an account whose highest character level is --account-max-level and whose membership is --membership."""
	rates = ctx.cfg("gameserver.rates.sell_limit")
	rate = f32(1.0) if not rates else rates[min(len(rates) - 1, ctx.membership)]  # Rates.get
	for low, high, base in ctx.rules.sell_limits:
		if low <= ctx.account_max_level <= high:
			return fresh_sell_limit(base, rate)
	raise OracleError(f"account max level {ctx.account_max_level} is in no SellLimit band (Java: NoSuchElementException)")


def _tabs(ctx: TradeContext, template: TradeListInfo, holder: dict[int, GoodsListInfo], window_filter: bool = True) -> list[dict]:
	"""The template's tabs in order; `shown` is SM_TRADELIST's filter (the goods list exists and the player's legion level reaches it), which
	SM_SELL_ITEM does not apply to a purchase template's tabs (window_filter False: no `shown`)."""
	result = []
	for tab in template.tabs:
		goods = holder.get(tab)
		entry = {"id": tab, "exists": goods is not None, "legionLevel": goods.legion_level if goods else None}
		if window_filter:
			entry["shown"] = goods is not None and goods.legion_level <= ctx.legion_level
		entry["items"] = [item_id for item_id, _, _ in goods.items] if goods else []
		result.append(entry)
	return result


AUDIT_NO_PACKET = "none (CM_DIALOG_SELECT audit: unsupported dialog action)"


def dialog_path(ctx: TradeContext, npc: NpcInfo, action: str) -> dict:
	"""Where a CM_DIALOG_SELECT of `action` (questId 0) at the npc ends: the gate of CM_DIALOG_SELECT.java:112-116 (outcome "audit": nothing is
	sent), then NpcController.onDialogSelect asks the npc template's AI (outcome None: the AI overrides onDialogSelect, not modelled), else
	DialogService.onDialogSelect (outcome "DialogService"). A spawn spot's `ai` replaces the template's (Creature.java:64-67; --map reports it)."""
	path = ctx.trade.dialog_gate(npc, action)
	if not path["passes"]:
		return {**path, "outcome": "audit"}
	ai = ctx.ai().resolve(npc.ai)
	if ai["overridesOnDialogSelect"]:
		return {**path, "ai": ai, "outcome": None,
		        "notModelled": f"AI {npc.ai!r}: {ai['overridesOnDialogSelect'][0]} overrides onDialogSelect, which NpcController asks before DialogService"}
	return {**path, "ai": ai, "outcome": "DialogService"}


def buy_side(ctx: TradeContext, npc: NpcInfo) -> dict:
	"""The BUY dialog (dialog_path, then DialogService.java:74-95), SM_TRADELIST and every item of the npc's goods lists (a CM_BUY_ITEM buy does
	not pass the dialog gate: CM_BUY_ITEM.java:126-132 asks canSell)."""
	template = ctx.trade.tradelists.get(npc.npc_id)
	functions = ctx.trade.functions(npc)
	path = dialog_path(ctx, npc, "BUY")
	if path["outcome"] == "audit":
		gated = AUDIT_NO_PACKET
	elif path["outcome"] is None:
		gated = None
	else:
		gated = "STR_BUY_SELL_HE_DOES_NOT_SELL_ITEM" if template is None else ""
	if template is None:
		return {"tradeList": None, "dialogPath": path, "dialog": gated, "smTradeList": None, "goods": []}
	tabs = _tabs(ctx, template, ctx.trade.goods)
	shown = [tab["id"] for tab in tabs if tab["shown"]]
	goods: list[dict] = []
	seen: set[int] = set()
	for tab in tabs:
		for item_id in tab["items"]:
			if item_id not in seen:
				seen.add(item_id)
				goods.append(good_entry(ctx, npc, template, item_id, ctx.count))
	sm_tradelist = None
	if gated == "" and shown:  # hasAnythingToSell: a tab whose goods list exists and whose legion level the player reaches
		sm_tradelist = {
			"npcType": template.npc_type,
			"npcTypeIndex": ctx.rules.npc_type_index[template.npc_type],
			"buyPriceModifier": _java_div(_int32(ctx.cfg("gameserver.prices.vendor.buymod") * template.sell_price_rate), 100),
			"constant": 100,
			"showBuyTab": functions["canSell"],
			"showSellTab": functions["canBuy"],
			"tabs": shown,
			"limitedItems": [{"itemId": e["itemId"], "buyCount": 0, "sellLimit": e["sellLimit"]} for e in ctx.trade.limited_items(npc.npc_id)],
		}
	return {
		"tradeList": {"npcType": template.npc_type, "sellPriceRate": template.sell_price_rate, "sellPriceRate2": template.sell_price_rate2,
		              "apSellPriceRate2": template.ap_sell_price_rate2, "buyPriceRate": template.buy_price_rate,
		              "useKinah": template.npc_type in NPC_TYPES_WITH_KINAH,
		              "kinahModifier": template.sell_price_rate2 if template.npc_type == "ABYSS_KINAH" else template.sell_price_rate,
		              "apModifier": template.ap_sell_price_rate2 if template.npc_type == "ABYSS_KINAH" else template.sell_price_rate,
		              "tabs": tabs},
		"dialogPath": path,
		"dialog": gated if gated != "" else "SM_TRADELIST" if shown else "STR_BUY_SELL_HE_DOES_NOT_SELL_ITEM",
		"smTradeList": sm_tradelist,
		"goods": goods,
	}


def sell_side(ctx: TradeContext, npc: NpcInfo) -> dict:
	"""The SELL and TRADE_SELL_LIST dialogs (dialog_path, then DialogService.java:251-254: SM_SELL_ITEM) and the rule CM_BUY_ITEM action 1
	applies to any item (a sale does not pass the dialog gate: CM_BUY_ITEM.java:113-121 asks canBuy() || canPurchase())."""
	functions = ctx.trade.functions(npc)
	purchase = ctx.trade.purchases.get(npc.npc_id)
	buys = functions["canBuy"] or functions["canPurchase"]
	arm = None
	if buys:
		arm = "VENDOR" if purchase is None else "PURCHASE_AP" if purchase.npc_type == "ABYSS" else "PURCHASE"
	paths = {action: dialog_path(ctx, npc, action) for action in ("SELL", "TRADE_SELL_LIST")}
	dialogs = {action: "SM_SELL_ITEM" if p["outcome"] == "DialogService" else AUDIT_NO_PACKET if p["outcome"] == "audit" else None
	           for action, p in paths.items()}
	reachable_by = [action for action, dialog in dialogs.items() if dialog == "SM_SELL_ITEM"]
	return {
		"npcBuys": buys,
		"arm": arm,
		"sellModifier": (purchase.buy_price_rate if purchase else ctx.cfg("gameserver.prices.vendor.sellmod")) if buys else None,
		"purchaseTemplate": None if purchase is None else {"npcType": purchase.npc_type, "buyPriceRate": purchase.buy_price_rate,
		                                                   "tabs": _tabs(ctx, purchase, ctx.trade.purchase_goods, window_filter=False)},
		"dialogPaths": paths,
		"dialogs": dialogs,
		# whether a SELL or TRADE_SELL_LIST dialog opens SM_SELL_ITEM (None: an AI that overrides onDialogSelect decides, not modelled)
		"reachable": True if reachable_by else None if None in dialogs.values() else False,
		"reachableBy": reachable_by,
		"smSellItem": None if not reachable_by else {
			"npcType": purchase.npc_type if purchase else "NORMAL",
			"npcTypeIndex": ctx.rules.npc_type_index[purchase.npc_type if purchase else "NORMAL"],
			"buyPriceRate": purchase.buy_price_rate if purchase else ctx.cfg("gameserver.prices.vendor.sellmod"),
			"showBuyTab": functions["canSell"],
			"showSellTab": buys,
			"tabs": list(purchase.tabs) if purchase else [],
		},
	}


def _npc_block(ctx: TradeContext, npc: NpcInfo) -> dict:
	return {"npcId": npc.npc_id, "name": npc.name, "race": npc.race, "level": npc.level,
	        "funcDialogs": list(npc.func_dialogs) if npc.func_dialogs is not None else None, **ctx.trade.functions(npc),
	        "hasTradeList": npc.npc_id in ctx.trade.tradelists, "hasPurchaseTemplate": npc.npc_id in ctx.trade.purchases,
	        "hasTradeInList": npc.npc_id in ctx.trade.tradeins, "ai": npc.ai,
	        # what a dialog needs from the player (not modelled): CM_DIALOG_SELECT - not trading, the npc in his known list;
	        # DialogService.isInteractionAllowed - no summon owner on a regular spawn, and isSubDialogRestricted, false without a subdialog_type;
	        # NpcController.onDialogSelect - PositionUtil.isInTalkRange, within getTalkDistance() + 1
	        "talkDistance": npc.talk_distance, "talkRange": npc.talk_distance + 1, "subDialogType": npc.sub_dialog_type}


def npc_report(ctx: TradeContext, npc_id: int, item_id: int | None = None) -> dict:
	npc = ctx.trade.npc(npc_id)
	report = {"npc": _npc_block(ctx, npc), "buy": buy_side(ctx, npc), "sell": sell_side(ctx, npc)}
	if npc.npc_id in ctx.trade.tradeins:
		report["tradeIn"] = {"tabs": list(ctx.trade.tradeins[npc.npc_id].tabs), "notModelled": "trade-in prices (performBuyFromTradeInTrade)"}
	if item_id is not None:
		item = ctx.trade.item(item_id, "--item")
		template = ctx.trade.tradelists.get(npc_id)
		report["item"] = {
			**_item_block(ctx, item),
			"buy": good_entry(ctx, npc, template, item_id, ctx.count, sell_back=False) if template is not None else
			{"failure": {"reason": "the npc has no tradelist_template: npc.canSell() is false, CM_BUY_ITEM.java:130 ignores the buy", "message": None}},
			"sell": sale_entry(ctx, npc, item, ctx.count),
		}
	return report


def item_report(ctx: TradeContext, item_id: int) -> dict:
	"""One item: its prices, what a vendor without a purchase_template pays for it, and every npc that sells or purchases it."""
	item = ctx.trade.item(item_id, "--item")
	sold_by = []
	for npc_id, template in sorted(ctx.trade.tradelists.items()):
		if not any(item_id in (i for i, _, _ in ctx.trade.goods[tab].items) for tab in template.tabs if tab in ctx.trade.goods):
			continue
		npc = ctx.trade.npcs.get(npc_id)
		if npc is None:
			sold_by.append({"npcId": npc_id, "npcTemplate": False})  # a trade list of an npc that does not exist
			continue
		entry = good_entry(ctx, npc, template, item_id, ctx.count, sell_back=False)
		sold_by.append({"npcId": npc_id, "name": npc.name, "npcTemplate": True, **ctx.trade.functions(npc), "npcType": template.npc_type,
		                "sellPriceRate": template.sell_price_rate, "tabs": entry["tabs"], "shown": entry["shown"], "kinah": entry["kinah"],
		                "requiredAp": entry["requiredAp"], "requiredItems": entry["requiredItems"], "limited": entry["limited"], "buyable": entry["buyable"],
		                "failure": entry["failure"]})
	purchased_by = []
	for npc_id, template in sorted(ctx.trade.purchases.items()):
		if not any(item_id in (i for i, _, _ in ctx.trade.purchase_goods[tab].items) for tab in template.tabs if tab in ctx.trade.purchase_goods):
			continue
		npc = ctx.trade.npcs.get(npc_id)
		if npc is None:
			purchased_by.append({"npcId": npc_id, "npcTemplate": False})
			continue
		purchased_by.append({"npcId": npc_id, "name": npc.name, "npcTemplate": True, **ctx.trade.functions(npc),
		                     "sale": sale_entry(ctx, npc, item, ctx.count)})
	return {"item": _item_block(ctx, item), "vendorSale": sale_entry(ctx, None, item, ctx.count), "soldBy": sold_by, "purchasedBy": purchased_by}


def map_report(ctx: TradeContext, map_id: int) -> dict:
	"""Every npc with a regular spawn on the map that has a trade, trade-in or purchase template or a BUY, SELL, TRADE_IN or TRADE_SELL_LIST
	function, with its spots (m5a/spawns.py evaluate at an unknown game time: `spawned` None for a pool or a temporary spawn)."""
	spawn_npcs = {npc_id: SpawnNpcInfo(npc.level, npc.npc_type) for npc_id, npc in ctx.trade.npcs.items()}
	rows = evaluate(load_groups(ctx.trade.data, map_id), spawn_npcs, GameClock())
	by_npc: dict[int, list[dict]] = {}
	for row in rows:
		by_npc.setdefault(row["npcId"], []).append(row)
	merchants = []
	for npc_id in sorted(by_npc):
		npc = ctx.trade.npcs.get(npc_id)
		if npc is None:
			continue
		functions = ctx.trade.functions(npc)
		interesting = (npc_id in ctx.trade.tradelists or npc_id in ctx.trade.purchases or npc_id in ctx.trade.tradeins
		               or any(ctx.trade.supports(npc, a) for a in ("BUY", "SELL", "TRADE_IN", "TRADE_SELL_LIST")))
		spots = [{"x": r["x"], "y": r["y"], "z": r["z"], "h": r["h"], "staticId": r["staticId"], "spawned": r["spawned"], "flags": r["flags"],
		          # Creature.java:64-67: the spot's ai replaces the template's, NO_AI means none
		          "ai": npc.ai if r.get("spotAi") is None else None if r["spotAi"] == NO_AI else r["spotAi"]}
		         for r in by_npc[npc_id]]
		if not interesting or all(s["spawned"] is False for s in spots):
			continue
		for spot in spots:
			spot["aiOverridesOnDialogSelect"] = bool(ctx.ai().resolve(spot["ai"])["overridesOnDialogSelect"])
		template = ctx.trade.tradelists.get(npc_id)
		tabs = _tabs(ctx, template, ctx.trade.goods) if template else []
		merchants.append({**_npc_block(ctx, npc), "npcType": template.npc_type if template else None,
		                  "sellPriceRate": template.sell_price_rate if template else None, "tabs": [t["id"] for t in tabs],
		                  "shownTabs": [t["id"] for t in tabs if t["shown"]],
		                  "goodsCount": len({i for t in tabs for i in t["items"]}), "spots": spots})
	return {
		"map": map_id,
		"merchants": merchants,
		"sellers": [m["npcId"] for m in merchants if m["canSell"]],
		"buyers": [m["npcId"] for m in merchants if m["canBuy"] or m["canPurchase"]],
	}


COUNTRY_HOLDERS = ("npc_trade_list", "goodslists", "item_templates", "npc_templates", "item_restriction_cleanups", "spawns", "timed_events")
_COUNTRY_CHECKED: "weakref.WeakKeyDictionary[StaticData, set[int]]" = weakref.WeakKeyDictionary()


def check_country_code(data: StaticData, country_code: int) -> None:
	"""XmlMerger.applyCountryOverride (XmlMerger.java:233-243): gameserver.country.code picks the region variant of an imported file
	(goodslists_europe.xml for 2, ...). The static data the oracle reads must have been resolved with the configured code."""
	if country_code in _COUNTRY_CHECKED.get(data, ()):
		return
	expected = StaticData(data.dir, country_code)
	for tag in COUNTRY_HOLDERS:
		try:
			actual = data.files(tag)
		except OracleError:
			continue
		if actual != expected.files(tag):
			raise OracleError(f"<{tag}>: the static data was read with another country code than gameserver.country.code={country_code} "
			                  f"({[p.name for p in actual]} instead of {[p.name for p in expected.files(tag)]}); build it with StaticData(dir, {country_code})")
	_COUNTRY_CHECKED.setdefault(data, set()).add(country_code)


def trade_report(data: StaticData, java_src: Path, config: dict[str, ConfigValue], npc_id: int | None = None, item_id: int | None = None,
                 map_id: int | None = None, races: tuple[str, ...] = RACES, count: int = 1, influences: dict[str, int] | None = None,
                 legion_level: int = 0, account_max_level: int = 1, membership: int = 0, trade: TradeData | None = None,
                 handlers_dir: Path | None = None, dialog_ai: DialogAi | None = None) -> dict:
	"""`data` must be read with the configured gameserver.country.code (StaticData(dir, code)); `handlers_dir` is data/handlers (default: beside
	`java_src`), whose AI classes answer a dialog first."""
	rules = JavaTradeRules.read(java_src) if trade is None else trade.rules
	check_country_code(trade.data if trade is not None else data, config["gameserver.country.code"].value)
	if map_id is not None and (npc_id is not None or item_id is not None):
		raise OracleError("--map lists the merchants of a map; ask for an --npc or an --item in a second call")
	if map_id is None and npc_id is None and item_id is None:
		raise OracleError("pass --npc, --item or --map")
	if not 1 <= count <= rules.max_count:
		raise OracleError(f"count {count}: CM_BUY_ITEM accepts 1..{rules.max_count} (validateBuyItems refuses a count below 1)")
	if membership < 0:
		raise OracleError("a negative membership indexes no rate (Rates.get: ArrayIndexOutOfBoundsException)")
	if legion_level < 0:
		raise OracleError("the legion level is 0 without a legion and positive in one")
	for race in races:
		if race not in RACES:
			raise OracleError(f"race must be one of {RACES}")
	influences = dict(influences or {})
	for race in influences:
		if race not in RACES:
			raise OracleError(f"--influence {race}: race must be one of {RACES}")
	trade = trade if trade is not None else TradeData(data, rules)
	prices = {race: race_prices(config, race, influences.get(race)) for race in races}
	ctx = TradeContext(rules, trade, config, tuple(races), prices, count, legion_level, account_max_level, membership, Path(java_src),
	                   Path(handlers_dir) if handlers_dir is not None else None, dialog_ai)
	report = {
		"format": "aion-m5c-trade",
		"version": 1,
		"mode": "map" if map_id is not None else "npc" if npc_id is not None else "item",
		"config": {key: value.as_json() for key, value in config.items()},
		"prices": prices,
		"count": count,
		"legionLevel": legion_level,
		"account": {"maxLevel": account_max_level, "membership": membership},
	}
	if map_id is not None:
		report.update(map_report(ctx, map_id))
	elif npc_id is not None:
		report.update(npc_report(ctx, npc_id, item_id))
	else:
		report.update(item_report(ctx, item_id))
	return report


def parse_influences(values: list[str]) -> dict[str, int]:
	result = {}
	for text in values:
		race, sep, number = text.partition("=")
		if not sep:
			raise OracleError(f"--influence {text!r}: expected RACE=N")
		result[race.strip()] = java_int(number, f"--influence {race}")
	return result
