#include "decoders/EconomyDecoders.h"

#include <string>
#include <string_view>
#include <utility>

namespace aion::gameserver::scenario::decoders {

namespace {

/** reads a byte and fails unless it is 0 or 1 (a Java `flag ? 1 : 0`) */
bool readFlag(BodyReader& reader, std::string_view what) {
	const uint8_t value = reader.C();
	if (value > 1)
		reader.fail(std::string(what) + ": expected 0 or 1, got " + std::to_string(value));
	return value == 1;
}

/** writeH(list.size()) followed by one writeD per entry: the tab lists of SM_TRADELIST and SM_SELL_ITEM */
std::vector<int32_t> readTabs(BodyReader& reader) {
	const uint16_t count = reader.H();
	std::vector<int32_t> tabs;
	tabs.reserve(count);
	for (uint16_t i = 0; i < count; i++)
		tabs.push_back(reader.D());
	return tabs;
}

} // namespace

// ---- SM_DIALOG_WINDOW -------------------------------------------------------------------------------------------------------------------

DialogWindow decodeDialogWindow(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_DIALOG_WINDOW");
	DialogWindow window;
	window.targetObjectId = reader.D(); // SM_DIALOG_WINDOW.java:31
	window.dialogPageId = reader.H();   // :32
	window.questId = reader.D();        // :33
	reader.expectH(0, "the short after the quest id"); // :34, writeH(0)
	if (window.dialogPageId == DIALOG_PAGE_MAIL || window.dialogPageId == DIALOG_PAGE_TOWN_CHALLENGE_TASK)
		window.pageValue = reader.H(); // :35-38, the mailbox state or the town id
	else
		reader.expectH(0, "the last short of a page that is neither MAIL nor TOWN_CHALLENGE_TASK"); // :39-40, writeH(0)
	reader.expectFullyConsumed();
	return window;
}

// ---- SM_PRICES --------------------------------------------------------------------------------------------------------------------------

Prices decodePrices(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_PRICES");
	Prices prices;
	prices.globalPrices = reader.C();         // SM_PRICES.java:14
	prices.globalPricesModifier = reader.C(); // :16
	prices.taxes = reader.C();                // :17
	reader.expectFullyConsumed();
	return prices;
}

// ---- SM_TRADELIST, SM_SELL_ITEM ---------------------------------------------------------------------------------------------------------

TradeList decodeTradeList(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_TRADELIST");
	TradeList list;
	list.npcObjectId = reader.D();                         // SM_TRADELIST.java:58
	list.tradeNpcType = reader.C();                        // :59, tradeNpcType.index()
	list.buyPriceModifier = reader.D();                    // :60
	reader.expectD(100, "the 4.5 int after the modifier"); // :61, writeD(100)
	list.showBuyTab = readFlag(reader, "showBuyTab");      // :62
	list.showSellTab = readFlag(reader, "showSellTab");    // :63
	list.tabs = readTabs(reader);                          // :64-66
	const uint16_t limitedCount = reader.H();              // :67
	for (uint16_t i = 0; i < limitedCount; i++) {
		LimitedTradeItem item;
		item.itemId = reader.D();    // :69
		item.buyCount = reader.H();  // :70
		item.sellLimit = reader.H(); // :71
		list.limitedItems.push_back(item);
	}
	reader.expectFullyConsumed();
	return list;
}

SellItemWindow decodeSellItem(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_SELL_ITEM");
	SellItemWindow window;
	window.npcObjectId = reader.D();                      // SM_SELL_ITEM.java:39
	window.tradeNpcType = reader.C();                     // :40, tradeNpcType.index()
	window.buyPriceRate = reader.D();                     // :41
	window.showBuyTab = readFlag(reader, "showBuyTab");   // :42
	window.showSellTab = readFlag(reader, "showSellTab"); // :43
	window.tabs = readTabs(reader);                       // :44-46
	reader.expectFullyConsumed();
	return window;
}

// ---- SM_REPURCHASE ----------------------------------------------------------------------------------------------------------------------

Repurchase decodeRepurchase(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_REPURCHASE");
	Repurchase repurchase;
	repurchase.targetObjectId = reader.D();                   // SM_REPURCHASE.java:30
	reader.expectD(1, "the int after the target object id"); // :31, writeD(1)
	const uint16_t count = reader.H();                        // :32
	for (uint16_t i = 0; i < count; i++) {
		RepurchaseEntry entry;
		entry.item.objectId = reader.D();        // :37
		entry.item.templateId = reader.D();      // :38
		entry.item.l10n = reader.S();            // :39
		readItemInfoBlob(reader, entry.item);    // :41-42, ItemInfoBlob.getFullBlob
		entry.repurchasePrice = reader.Q();      // :44
		repurchase.items.push_back(std::move(entry));
	}
	reader.expectFullyConsumed();
	return repurchase;
}

// ---- SM_QUESTION_WINDOW -----------------------------------------------------------------------------------------------------------------

QuestionWindow decodeQuestionWindow(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_QUESTION_WINDOW");
	QuestionWindow window;
	window.code = reader.D(); // SM_QUESTION_WINDOW.java:306
	for (std::string& param : window.params)
		param = reader.S(); // :307-308, writeS(String.valueOf(params[i])) or writeS(null)
	reader.expectD(0, "the int after the parameters"); // :309, writeD(0x00)
	window.rangeOrCooldown = readFlag(reader, "the range or cooldown flag"); // :310
	window.senderId = reader.D();                                            // :311
	window.rangeOrCooldownSeconds = reader.D();                              // :312
	if (window.rangeOrCooldown != (window.rangeOrCooldownSeconds > 0))
		reader.fail("the flag at :310 is `rangeOrCooldownSeconds > 0 ? 1 : 0`, but the flag is " + std::to_string(window.rangeOrCooldown) +
		            " and the seconds are " + std::to_string(window.rangeOrCooldownSeconds));
	reader.expectFullyConsumed();
	return window;
}

} // namespace aion::gameserver::scenario::decoders
