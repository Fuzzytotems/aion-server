#pragma once

// The economy packets the M5c gate reads (m5c-plan.md §2.1-§2.2, G-02; the §10.3 rows X1-X8, X15, X25, X26). Stage 0 (harness-a) wrote the
// packets of talking to an npc and of a shop's windows: SM_DIALOG_WINDOW, SM_PRICES, SM_TRADELIST, SM_SELL_ITEM, SM_REPURCHASE and
// SM_QUESTION_WINDOW. The exchange, mail, private store and crafting packets of G-02 join this file in stage 1 (harness-b).
//
// **m5a-plan.md D9:** every layout is written from the Java `writeImpl` under game-server/src/com/aionemu/gameserver/network/aion/serverpackets/
// and the helpers it calls (iteminfo/ItemInfoBlob for SM_REPURCHASE's items, AionServerPacket / BaseServerPacket.writeS), the page ids from
// model/DialogPage.java and the mailbox states from services/player/PlayerMailboxState.java. Nothing here includes, calls or mirrors a C++
// serverpackets header. SM_REPURCHASE's item info blob is PacketDecoders.cpp's reader (readItemInfoBlob), the one SM_INVENTORY_INFO uses.
//
// Every decode function consumes the body exactly and throws DecodeError otherwise; the Java constants that carry no data are verified.

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "decoders/PacketDecoders.h" // BodyReader, DecodeError, InventoryItem, readItemInfoBlob

namespace aion::gameserver::scenario::decoders {

// ---- the constants the packets carry ----------------------------------------------------------------------------------------------------

/**
 * DialogPage.id() of the two pages whose SM_DIALOG_WINDOW carries a value in its last short (SM_DIALOG_WINDOW.java:35-40): MAIL writes the
 * player's mailbox state, TOWN_CHALLENGE_TASK the town id at the player's position (DialogPage.java:35, 57); every other page writes 0
 */
constexpr uint16_t DIALOG_PAGE_MAIL = 18;
constexpr uint16_t DIALOG_PAGE_TOWN_CHALLENGE_TASK = 43;

/** PlayerMailboxState (PlayerMailboxState.java:8-10), the last short of SM_DIALOG_WINDOW's MAIL page */
constexpr uint16_t MAILBOX_STATE_CLOSED = 0x00;
constexpr uint16_t MAILBOX_STATE_REGULAR = 0x01;
constexpr uint16_t MAILBOX_STATE_EXPRESS = 0x02;

/** SM_QUESTION_WINDOW.MAX_PARAM_COUNT (SM_QUESTION_WINDOW.java:276): the packet always writes three strings */
constexpr size_t QUESTION_WINDOW_PARAMS = 3;

// ---- SM_DIALOG_WINDOW -------------------------------------------------------------------------------------------------------------------

/** SM_DIALOG_WINDOW (SM_DIALOG_WINDOW.java:29-41), 14 bytes */
struct DialogWindow {
	int32_t targetObjectId = 0;
	/** writeH(dialogPageId): a DialogPage id, or the action id DialogService.handleQuestDialogueOrSendNextPage echoes as the next page */
	uint16_t dialogPageId = 0;
	int32_t questId = 0;
	/**
	 * the last short (:35-40): the mailbox state for DIALOG_PAGE_MAIL, the town id for DIALOG_PAGE_TOWN_CHALLENGE_TASK; for any other page a
	 * literal writeH(0), which the decoder verifies
	 */
	uint16_t pageValue = 0;

	bool operator==(const DialogWindow&) const = default;
};

DialogWindow decodeDialogWindow(std::span<const uint8_t> body);

// ---- SM_PRICES --------------------------------------------------------------------------------------------------------------------------

/** SM_PRICES (SM_PRICES.java:13-18), 3 bytes: PricesService.getGlobalPrices(race), getGlobalPricesModifier(), getTaxes(race) */
struct Prices {
	uint8_t globalPrices = 0;
	uint8_t globalPricesModifier = 0;
	uint8_t taxes = 0;

	bool operator==(const Prices&) const = default;
};

Prices decodePrices(std::span<const uint8_t> body);

// ---- SM_TRADELIST, SM_SELL_ITEM ---------------------------------------------------------------------------------------------------------

/** one limited item of SM_TRADELIST (SM_TRADELIST.java:68-72) */
struct LimitedTradeItem {
	int32_t itemId = 0;
	/** writeH(limitedItem.getBuyCount(playerObjId)) */
	uint16_t buyCount = 0;
	/** writeH(limitedItem.getSellLimit()) */
	uint16_t sellLimit = 0;

	bool operator==(const LimitedTradeItem&) const = default;
};

/** SM_TRADELIST (SM_TRADELIST.java:57-73), the BUY window */
struct TradeList {
	int32_t npcObjectId = 0;
	/** writeC(tradeNpcType.index()): the TradeNpcType constructor argument (TradeNpcType.java:12-16), NORMAL is 1 - never the ordinal */
	uint8_t tradeNpcType = 0;
	/** writeD(buyPriceModifier): VENDOR_BUY_MODIFIER * sell_price_rate / 100 (DialogService.java:91-92) */
	int32_t buyPriceModifier = 0;
	// writeD(100) "new aion 4.5" (:61) is a literal the decoder verifies
	bool showBuyTab = false;
	bool showSellTab = false;
	/** the tab (goods list) ids the player's legion level may see, in the template's order */
	std::vector<int32_t> tabs;
	std::vector<LimitedTradeItem> limitedItems;
};

TradeList decodeTradeList(std::span<const uint8_t> body);

/** SM_SELL_ITEM (SM_SELL_ITEM.java:38-47), the SELL window */
struct SellItemWindow {
	int32_t npcObjectId = 0;
	/** TradeNpcType.index() of the purchase template, NORMAL (1) without one (:30) */
	uint8_t tradeNpcType = 0;
	/** the purchase template's buy_price_rate, PricesService.getVendorSellModifier() without one (:31) */
	int32_t buyPriceRate = 0;
	bool showBuyTab = false;
	bool showSellTab = false;
	/** the purchase template's tabs, none without one (:34) */
	std::vector<int32_t> tabs;
};

SellItemWindow decodeSellItem(std::span<const uint8_t> body);

// ---- SM_REPURCHASE ----------------------------------------------------------------------------------------------------------------------

/** one entry of SM_REPURCHASE (SM_REPURCHASE.java:34-45) */
struct RepurchaseEntry {
	/** object id, template id, l10n and the full blob; no slot and no cloth byte are written, so equipmentSlot and cloth keep their defaults */
	InventoryItem item;
	/** writeQ(item.getRepurchasePrice()) */
	int64_t repurchasePrice = 0;
};

/** SM_REPURCHASE (SM_REPURCHASE.java:29-46): the buy-back list RepurchaseService holds for the player */
struct Repurchase {
	/** the npc's object id (DialogService.java:233 passes npc.getObjectId() to the constructor's `npcId`) */
	int32_t targetObjectId = 0;
	// writeD(1) (:31) is a literal the decoder verifies
	std::vector<RepurchaseEntry> items;
};

Repurchase decodeRepurchase(std::span<const uint8_t> body);

// ---- SM_QUESTION_WINDOW -----------------------------------------------------------------------------------------------------------------

/** SM_QUESTION_WINDOW (SM_QUESTION_WINDOW.java:305-313) */
struct QuestionWindow {
	/** the client_strings id of the question, e.g. STR_ASK_RECOVER_EXPERIENCE 160011 */
	int32_t code = 0;
	/**
	 * the three writeS of the parameters (String.valueOf of each, null past the given ones): a null and an empty string are the same lone NUL
	 * char on the wire, so both decode as ""
	 */
	std::array<std::string, QUESTION_WINDOW_PARAMS> params;
	// writeD(0x00) (:309) is a literal the decoder verifies
	/** writeC(rangeOrCooldownSeconds > 0 ? 1 : 0) (:310); the decoder verifies it agrees with rangeOrCooldownSeconds */
	bool rangeOrCooldown = false;
	int32_t senderId = 0;
	int32_t rangeOrCooldownSeconds = 0;
};

QuestionWindow decodeQuestionWindow(std::span<const uint8_t> body);

} // namespace aion::gameserver::scenario::decoders
