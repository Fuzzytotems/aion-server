// The G-02 dialog and shop decoders (m5c-plan.md §2.1-§2.2, G-02) against byte vectors written from the Java writeImpl, in the style of
// ItemDecodersTest.cpp: a hand-built body with the offset of every field where the layout is flat, a builder written out field by field in
// Java order where a packet carries an item info blob or a string, and every case asserts the body size Java produces. The values are the
// gate's own (m5c-plan.md §10.3 X1-X4, X8, X15, X25, X26): merchant 798007 and its goods lists 132 and 720 (npc_trade_list), the Minor Life
// Potion, item_templates.xml:830724 (id 162000002, mask 12414, desc 702583, price 250), the RECOVERY question for 1,000 recoverable exp and
// the cube expander's question. No case includes or consults a C++ serverpackets header (m5a-plan.md D9).

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "decoders/EconomyDecoders.h"

#include "NetworkTestSupport.h"

namespace aion::gameserver::scenario::decoders {
namespace {

using network::test::PacketWriter;

/** SM_QUESTION_WINDOW ids of the dialog path (SM_QUESTION_WINDOW.java:107, 123, 194) */
constexpr int32_t STR_EXCHANGE_DO_YOU_ACCEPT_EXCHANGE = 90001;
constexpr int32_t STR_ASK_RECOVER_EXPERIENCE = 160011;
constexpr int32_t STR_WAREHOUSE_EXPAND_WARNING = 900686;

/** ChatUtil.l10n(desc) (ChatUtil.java:95-101) as writeS puts it on the wire: "$", the two chars of `desc << 1 | 1` (low half first), NUL */
std::string l10n(PacketWriter& writer, int32_t desc) {
	const uint32_t id = static_cast<uint32_t>(desc) << 1 | 1;
	const std::u16string chars{u'$', static_cast<char16_t>(id & 0xFFFF), static_cast<char16_t>(id >> 16)};
	for (const char16_t c : chars)
		writer.H(c);
	writer.H(0);
	return commons::utils::StringUtils::toUtf8(chars);
}

/**
 * ItemInfoBlob.writeMe of an item whose group has no equipment slot (a potion): GENERAL_INFO alone (GeneralInfoBlobEntry: the mask, the count,
 * an empty creator and the constants PacketDecoders.cpp verifies), prefixed with the entries' size - ItemDecodersTest.cpp's plainBlob
 */
void plainBlob(PacketWriter& w, uint16_t itemMask, int64_t count) {
	PacketWriter entries;
	entries.C(0x00);
	entries.H(itemMask).Q(count).S("").C(0).D(0).D(0).D(0).H(0).D(0).H(18);
	w.H(static_cast<int32_t>(entries.data.size())).B(entries.data);
}

// ---- SM_DIALOG_WINDOW -------------------------------------------------------------------------------------------------------------------

TEST(EconomyDecodersTest, DialogWindowOfAFunctionNpc) {
	// X2: TalkEventHandler.onTalk -> DialogPage.getStartPageId answers 10 for an npc with func_dialogs (DialogPage.java:119-121), sent with
	// the two-argument constructor, so the quest id is 0 (SM_DIALOG_WINDOW.java:18-20)
	const std::vector<uint8_t> body = {
		0x2A, 0x00, 0x0B, 0x40, // 0: writeD(targetObjectId) = 798007's object id  (:31)
		0x0A, 0x00,             // 4: writeH(dialogPageId) = 10                     (:32)
		0x00, 0x00, 0x00, 0x00, // 6: writeD(questId) = 0                           (:33)
		0x00, 0x00,             // 10: writeH(0)                                    (:34)
		0x00, 0x00,             // 12: writeH(0), neither MAIL nor TOWN_CHALLENGE_TASK (:39-40)
	};
	ASSERT_EQ(body.size(), 14u);
	EXPECT_EQ(decodeDialogWindow(body), (DialogWindow{0x400B002A, 10, 0, 0}));

	// DialogService.handleQuestDialogueOrSendNextPage echoes an action as the next page, with the quest id (DialogService.java:289)
	PacketWriter quest;
	quest.D(0x400B002A).H(1011).D(1101).H(0).H(0);
	EXPECT_EQ(decodeDialogWindow(quest.data), (DialogWindow{0x400B002A, 1011, 1101, 0}));

	std::vector<uint8_t> changedConstant = body;
	changedConstant[10] = 1;
	EXPECT_THROW(decodeDialogWindow(changedConstant), DecodeError) << "writeH(0) at :34 is a constant";
	std::vector<uint8_t> valueOnAnotherPage = body;
	valueOnAnotherPage[12] = 1;
	EXPECT_THROW(decodeDialogWindow(valueOnAnotherPage), DecodeError) << "only MAIL and TOWN_CHALLENGE_TASK write a value in the last short";
	std::vector<uint8_t> trailing = body;
	trailing.push_back(0);
	EXPECT_THROW(decodeDialogWindow(trailing), DecodeError);
	EXPECT_THROW(decodeDialogWindow(std::span<const uint8_t>(body).first(12)), DecodeError) << "a body without its last short";
}

TEST(EconomyDecodersTest, DialogWindowOfThePostboxAndOfATownTask) {
	// X3: PostboxAI.handleDialogStart sets the mailbox state to REGULAR and sends DialogPage.MAIL (PostboxAI.java:23-26): the last short is
	// player.getMailbox().mailBoxState (SM_DIALOG_WINDOW.java:35-36)
	PacketWriter postbox;
	postbox.D(0x400B0100).H(DIALOG_PAGE_MAIL).D(0).H(0).H(MAILBOX_STATE_REGULAR);
	ASSERT_EQ(postbox.data.size(), 14u);
	const DialogWindow mail = decodeDialogWindow(postbox.data);
	EXPECT_EQ(mail.dialogPageId, 18);
	EXPECT_EQ(mail.pageValue, MAILBOX_STATE_REGULAR) << "the mailbox state, 1 for REGULAR (PlayerMailboxState.java:9)";

	PacketWriter closed; // a MAIL page may carry 0 too: the state is data there, not a constant
	closed.D(0x400B0100).H(DIALOG_PAGE_MAIL).D(0).H(0).H(MAILBOX_STATE_CLOSED);
	EXPECT_EQ(decodeDialogWindow(closed.data).pageValue, 0);

	PacketWriter town; // TOWN_CHALLENGE_TASK writes TownService.getTownIdByPosition(player) (:37-38)
	town.D(0x400B0200).H(DIALOG_PAGE_TOWN_CHALLENGE_TASK).D(0).H(0).H(7);
	EXPECT_EQ(decodeDialogWindow(town.data).pageValue, 7);
}

// ---- SM_PRICES --------------------------------------------------------------------------------------------------------------------------

TEST(EconomyDecodersTest, PricesAreThreeBytes) {
	// X1: with sieges off both influences are 0, so getGlobalPrices = round(100 + 0.25 * 100) = 125, the modifier is DEFAULT_MODIFIER 100 and
	// getTaxes = round(100 + 0.125 * 100) = 113 (PricesService.java:21-53; oracle.py m5c-trade `prices`)
	const std::vector<uint8_t> body = {125, 100, 113}; // SM_PRICES.java:14, 16, 17
	EXPECT_EQ(decodePrices(body), (Prices{125, 100, 113}));
	EXPECT_THROW(decodePrices(std::vector<uint8_t>{125, 100}), DecodeError);
	EXPECT_THROW(decodePrices(std::vector<uint8_t>{125, 100, 113, 0}), DecodeError);
}

// ---- SM_TRADELIST, SM_SELL_ITEM ---------------------------------------------------------------------------------------------------------

TEST(EconomyDecodersTest, TradeListOfMerchant798007) {
	// X4: npc_trade_list: 798007 is NORMAL, sell_price_rate 100, tabs 132 and 720, no limited items; the modifier is
	// VENDOR_BUY_MODIFIER 100 * 100 / 100 (DialogService.java:91-92)
	const std::vector<uint8_t> body = {
		0x2A, 0x00, 0x0B, 0x40, // 0: writeD(targetObjId)                            (:58)
		0x01,                   // 4: writeC(tradeNpcType.index()) = NORMAL's 1       (:59)
		0x64, 0x00, 0x00, 0x00, // 5: writeD(buyPriceModifier) = 100                  (:60)
		0x64, 0x00, 0x00, 0x00, // 9: writeD(100), "new aion 4.5"                     (:61)
		0x01,                   // 13: writeC(showBuyTab ? 1 : 0)                     (:62)
		0x01,                   // 14: writeC(showSellTab ? 1 : 0)                    (:63)
		0x02, 0x00,             // 15: writeH(tradeTablist.size())                    (:64)
		0x84, 0x00, 0x00, 0x00, // 17: writeD(132)                                    (:66)
		0xD0, 0x02, 0x00, 0x00, // 21: writeD(720)
		0x00, 0x00,             // 25: writeH(limitedItems.size())                    (:67)
	};
	ASSERT_EQ(body.size(), 27u);
	const TradeList list = decodeTradeList(body);
	EXPECT_EQ(list.npcObjectId, 0x400B002A);
	EXPECT_EQ(list.tradeNpcType, 1) << "TradeNpcType.NORMAL.index() is 1, its ordinal would be 0";
	EXPECT_EQ(list.buyPriceModifier, 100);
	EXPECT_TRUE(list.showBuyTab);
	EXPECT_TRUE(list.showSellTab);
	EXPECT_EQ(list.tabs, (std::vector<int32_t>{132, 720}));
	EXPECT_TRUE(list.limitedItems.empty());

	std::vector<uint8_t> changedConstant = body;
	changedConstant[9] = 0x65;
	EXPECT_THROW(decodeTradeList(changedConstant), DecodeError) << "writeD(100) at :61 is a constant";
	std::vector<uint8_t> notAFlag = body;
	notAFlag[13] = 2;
	EXPECT_THROW(decodeTradeList(notAFlag), DecodeError) << "showBuyTab is `? 1 : 0`";
	std::vector<uint8_t> sellNotAFlag = body;
	sellNotAFlag[14] = 2;
	EXPECT_THROW(decodeTradeList(sellNotAFlag), DecodeError) << "showSellTab is `? 1 : 0` too";
	std::vector<uint8_t> moreTabs = body;
	moreTabs[15] = 3;
	EXPECT_THROW(decodeTradeList(moreTabs), DecodeError) << "a third tab that is not there";
	std::vector<uint8_t> trailing = body;
	trailing.push_back(0);
	EXPECT_THROW(decodeTradeList(trailing), DecodeError);
}

TEST(EconomyDecodersTest, TradeListWithALimitedItem) {
	// SM_TRADELIST.java:67-72: per limited item writeD(itemId), writeH(getBuyCount(player)), writeH(getSellLimit())
	PacketWriter w;
	w.D(0x400B0300).C(1).D(100).D(100).C(1).C(0).H(1).D(132);
	w.H(2).D(162000052).H(3).H(10).D(169000003).H(0).H(40000);
	ASSERT_EQ(w.data.size(), 4u + 1u + 4u + 4u + 2u + 2u + 4u + 2u + 2u * 8u);
	const TradeList list = decodeTradeList(w.data);
	EXPECT_FALSE(list.showSellTab);
	ASSERT_EQ(list.limitedItems.size(), 2u);
	EXPECT_EQ(list.limitedItems[0], (LimitedTradeItem{162000052, 3, 10}));
	EXPECT_EQ(list.limitedItems[1], (LimitedTradeItem{169000003, 0, 40000})) << "the shorts are unsigned: 40000 is not negative";
}

TEST(EconomyDecodersTest, SellWindowWithoutAPurchaseTemplate) {
	// X4: 798007 has no purchase template, so SM_SELL_ITEM writes NORMAL's index, VENDOR_SELL_MODIFIER (20) and no tabs (SM_SELL_ITEM.java:30-34)
	const std::vector<uint8_t> body = {
		0x2A, 0x00, 0x0B, 0x40, // 0: writeD(targetObjectId)          (:39)
		0x01,                   // 4: writeC(tradeNpcType.index())    (:40)
		0x14, 0x00, 0x00, 0x00, // 5: writeD(buyPriceRate) = 20       (:41)
		0x01,                   // 9: writeC(showBuyTab)              (:42)
		0x01,                   // 10: writeC(showSellTab)            (:43)
		0x00, 0x00,             // 11: writeH(tradeTabs.size()) = 0   (:44)
	};
	ASSERT_EQ(body.size(), 13u);
	const SellItemWindow window = decodeSellItem(body);
	EXPECT_EQ(window.npcObjectId, 0x400B002A);
	EXPECT_EQ(window.tradeNpcType, 1);
	EXPECT_EQ(window.buyPriceRate, 20);
	EXPECT_TRUE(window.showBuyTab);
	EXPECT_TRUE(window.showSellTab);
	EXPECT_TRUE(window.tabs.empty());

	PacketWriter purchase; // a purchase template: its npc type, buy_price_rate and tabs
	purchase.D(0x400B0400).C(2).D(35).C(0).C(1).H(1).D(9001);
	const SellItemWindow withTemplate = decodeSellItem(purchase.data);
	EXPECT_EQ(withTemplate.tradeNpcType, 2);
	EXPECT_EQ(withTemplate.buyPriceRate, 35);
	EXPECT_FALSE(withTemplate.showBuyTab);
	EXPECT_EQ(withTemplate.tabs, (std::vector<int32_t>{9001}));

	std::vector<uint8_t> notAFlag = body;
	notAFlag[10] = 2;
	EXPECT_THROW(decodeSellItem(notAFlag), DecodeError) << "showSellTab is a flag";
	std::vector<uint8_t> buyNotAFlag = body;
	buyNotAFlag[9] = 2;
	EXPECT_THROW(decodeSellItem(buyNotAFlag), DecodeError) << "showBuyTab is a flag too";
	EXPECT_THROW(decodeSellItem(std::span<const uint8_t>(body).first(11)), DecodeError) << "no tab count";
	std::vector<uint8_t> trailing = body;
	trailing.push_back(0);
	EXPECT_THROW(decodeSellItem(trailing), DecodeError);
}

// ---- SM_REPURCHASE ----------------------------------------------------------------------------------------------------------------------

TEST(EconomyDecodersTest, RepurchaseListOfOneSale) {
	// X8: after selling ten Minor Life Potions the list holds exactly that stack at its repurchase price, the whole reward 500
	// (TradeService.java:241-243; oracle.py m5c-trade `sellBack`)
	PacketWriter w;
	w.D(0x400B002A).D(1).H(1);                     // SM_REPURCHASE.java:30-32
	w.D(0x7000010).D(162000002);                   // :37-38
	const std::string name = l10n(w, 702583);      // :39
	const size_t blobAt = w.data.size();
	plainBlob(w, 12414, 10);                       // :41-42
	const size_t blobSize = w.data.size() - blobAt;
	w.Q(500);                                      // :44
	ASSERT_EQ(w.data.size(), 10u + 8u + 8u + blobSize + 8u);

	const Repurchase list = decodeRepurchase(w.data);
	EXPECT_EQ(list.targetObjectId, 0x400B002A);
	ASSERT_EQ(list.items.size(), 1u);
	const RepurchaseEntry& entry = list.items[0];
	EXPECT_EQ(entry.item.objectId, 0x7000010);
	EXPECT_EQ(entry.item.templateId, 162000002);
	EXPECT_EQ(entry.item.l10n, name);
	ASSERT_TRUE(entry.item.general);
	EXPECT_EQ(entry.item.general->count, 10);
	EXPECT_EQ(entry.item.general->itemMask, 12414);
	EXPECT_EQ(entry.repurchasePrice, 500) << "a long after the blob, with no slot or cloth byte between";

	PacketWriter empty; // before any sale the list is empty (RepurchaseService.getRepurchaseItems answers an empty collection)
	empty.D(0x400B002A).D(1).H(0);
	EXPECT_TRUE(decodeRepurchase(empty.data).items.empty());

	std::vector<uint8_t> changedConstant = w.data;
	changedConstant[4] = 2;
	EXPECT_THROW(decodeRepurchase(changedConstant), DecodeError) << "writeD(1) at :31 is a constant";
	std::vector<uint8_t> noPrice(w.data.begin(), w.data.end() - 8);
	EXPECT_THROW(decodeRepurchase(noPrice), DecodeError) << "the price long is missing";
	std::vector<uint8_t> trailing = w.data;
	trailing.push_back(0);
	EXPECT_THROW(decodeRepurchase(trailing), DecodeError);
}

// ---- SM_QUESTION_WINDOW -----------------------------------------------------------------------------------------------------------------

TEST(EconomyDecodersTest, QuestionWindowOfTheSoulHealer) {
	// X15: DialogService's RECOVERY arm for 1,000 recoverable exp: price (int) (1000 * (0.25 - 0.00000015 * 1000)) = 249, sent as
	// new SM_QUESTION_WINDOW(STR_ASK_RECOVER_EXPERIENCE, 0, 0, String.valueOf(price)) (DialogService.java:132-133, 154-155)
	PacketWriter w;
	w.D(STR_ASK_RECOVER_EXPERIENCE); // SM_QUESTION_WINDOW.java:306
	w.S("249").H(0).H(0);            // :307-308: the price, then two writeS(null), a lone NUL char each
	w.D(0);                          // :309
	w.C(0).D(0).D(0);                // :310-312: no range, sender 0
	ASSERT_EQ(w.data.size(), 4u + 8u + 2u + 2u + 4u + 1u + 4u + 4u);

	const QuestionWindow window = decodeQuestionWindow(w.data);
	EXPECT_EQ(window.code, STR_ASK_RECOVER_EXPERIENCE);
	EXPECT_EQ(window.params[0], "249");
	EXPECT_EQ(window.params[1], "") << "writeS(null) is one NUL char";
	EXPECT_EQ(window.params[2], "");
	EXPECT_FALSE(window.rangeOrCooldown);
	EXPECT_EQ(window.senderId, 0);
	EXPECT_EQ(window.rangeOrCooldownSeconds, 0);

	PacketWriter cube; // X26: CubeExpandService.expandCube's question carries the raw price (CubeExpandService.java:62)
	cube.D(STR_WAREHOUSE_EXPAND_WARNING).S("1000").H(0).H(0).D(0).C(0).D(0).D(0);
	EXPECT_EQ(decodeQuestionWindow(cube.data).params[0], "1000");

	std::vector<uint8_t> changedConstant = w.data;
	changedConstant[16] = 1; // the first byte of writeD(0x00) at :309
	EXPECT_THROW(decodeQuestionWindow(changedConstant), DecodeError) << "writeD(0x00) at :309 is a constant";
	std::vector<uint8_t> twoParams(w.data.begin(), w.data.end());
	twoParams.erase(twoParams.begin() + 14, twoParams.begin() + 16); // the third writeS removed: every later field shifts
	EXPECT_THROW(decodeQuestionWindow(twoParams), DecodeError) << "the packet always writes MAX_PARAM_COUNT = 3 strings";
	std::vector<uint8_t> trailing = w.data;
	trailing.push_back(0);
	EXPECT_THROW(decodeQuestionWindow(trailing), DecodeError);
}

TEST(EconomyDecodersTest, QuestionWindowWithAParameterASenderAndARange) {
	// C8's exchange request: CM_EXCHANGE_REQUEST asks the partner STR_EXCHANGE_DO_YOU_ACCEPT_EXCHANGE with the requester's name, sender 0 and
	// no range (CM_EXCHANGE_REQUEST.java:94-95)
	PacketWriter exchange;
	exchange.D(STR_EXCHANGE_DO_YOU_ACCEPT_EXCHANGE).S("Mfivecwarrior").H(0).H(0).D(0).C(0).D(0).D(0);
	const QuestionWindow asked = decodeQuestionWindow(exchange.data);
	EXPECT_EQ(asked.code, STR_EXCHANGE_DO_YOU_ACCEPT_EXCHANGE);
	EXPECT_EQ(asked.params[0], "Mfivecwarrior");
	EXPECT_FALSE(asked.rangeOrCooldown);

	// a rift's question has a sender and a range and no parameter at all: new SM_QUESTION_WINDOW(904304, getOwner().getObjectId(), 5)
	// (RVController.java:126), so all three strings are writeS(null)
	PacketWriter rift;
	rift.D(904304).H(0).H(0).H(0).D(0).C(1).D(0x400B0500).D(5);
	ASSERT_EQ(rift.data.size(), 4u + 6u + 4u + 1u + 4u + 4u);
	const QuestionWindow window = decodeQuestionWindow(rift.data);
	EXPECT_EQ(window.params, (std::array<std::string, QUESTION_WINDOW_PARAMS>{"", "", ""}));
	EXPECT_TRUE(window.rangeOrCooldown);
	EXPECT_EQ(window.senderId, 0x400B0500);
	EXPECT_EQ(window.rangeOrCooldownSeconds, 5);

	PacketWriter flagWithoutRange; // `rangeOrCooldownSeconds > 0 ? 1 : 0` cannot be 1 with 0 seconds
	flagWithoutRange.D(904304).H(0).H(0).H(0).D(0).C(1).D(0x400B0500).D(0);
	EXPECT_THROW(decodeQuestionWindow(flagWithoutRange.data), DecodeError);
	PacketWriter rangeWithoutFlag;
	rangeWithoutFlag.D(904304).H(0).H(0).H(0).D(0).C(0).D(0x400B0500).D(5);
	EXPECT_THROW(decodeQuestionWindow(rangeWithoutFlag.data), DecodeError);
	PacketWriter notAFlag;
	notAFlag.D(904304).H(0).H(0).H(0).D(0).C(2).D(0x400B0500).D(5);
	EXPECT_THROW(decodeQuestionWindow(notAFlag.data), DecodeError);
}

} // namespace
} // namespace aion::gameserver::scenario::decoders
