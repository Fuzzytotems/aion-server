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

// ---- the exchange (stage 1, harness-b; §10.3 X9-X11) --------------------------------------------------------------------------------

TEST(EconomyDecodersTest, ExchangeRequestCarriesTheOtherPlayersName) {
	// X9: registerExchange sends each player the OTHER one's name (ExchangeService.java:50-51; SM_EXCHANGE_REQUEST.java:19)
	PacketWriter w;
	w.S("Mfivecmage");
	ASSERT_EQ(w.data.size(), 22u) << "ten UTF-16 chars and the NUL";
	EXPECT_EQ(decodeExchangeRequest(w.data), "Mfivecmage");
	EXPECT_EQ(decodeExchangeRequest(std::vector<uint8_t>{0, 0}), "");
	std::vector<uint8_t> trailing = w.data;
	trailing.push_back(0);
	EXPECT_THROW(decodeExchangeRequest(trailing), DecodeError);
	EXPECT_THROW(decodeExchangeRequest(std::span<const uint8_t>(w.data).first(20)), DecodeError) << "a name without its NUL char";
}

TEST(EconomyDecodersTest, ExchangeAddItemWritesTheTemplateIdBeforeTheObjectId) {
	// X9: A adds 10 of the 100 Minor Life Potions; the partner gets action 1 with the exchange's item (ExchangeService.java:169-170). For a part
	// of a stack addItem makes that item with ItemFactory.newItem(itemId, 10) (:139-144), so its object id is a NEW one, not the stack's
	// 0x7000010: a gate must not match this id against the stack
	PacketWriter w;
	w.C(EXCHANGE_OTHER);                    // SM_EXCHANGE_ADD_ITEM.java:29
	w.D(162000002).D(0x7000011);            // :31-32, the template id FIRST, then the new item's object id
	const std::string name = l10n(w, 702583); // :33
	const size_t blobAt = w.data.size();
	plainBlob(w, 12414, 10);                // :35-36, the exchange item's blob (its count is the exchanged count)
	ASSERT_EQ(w.data.size(), 1u + 8u + 8u + (w.data.size() - blobAt));

	const ExchangeAddItem added = decodeExchangeAddItem(w.data);
	EXPECT_EQ(added.action, EXCHANGE_OTHER);
	EXPECT_EQ(added.item.templateId, 162000002) << "writeD(templateId) comes first (:31)";
	EXPECT_EQ(added.item.objectId, 0x7000011) << "the partial add's new item, not the stack";
	EXPECT_EQ(added.item.l10n, name);
	ASSERT_TRUE(added.item.general);
	EXPECT_EQ(added.item.general->count, 10);
	EXPECT_EQ(added.item.equipmentSlot, 0) << "no slot follows the blob";

	std::vector<uint8_t> trailing = w.data;
	trailing.push_back(0);
	EXPECT_THROW(decodeExchangeAddItem(trailing), DecodeError) << "nothing follows the blob, not even a slot";
	EXPECT_THROW(decodeExchangeAddItem(std::span<const uint8_t>(w.data).first(w.data.size() - 1)), DecodeError);
}

TEST(EconomyDecodersTest, ExchangeAddKinahAndConfirmation) {
	// X9: A adds 100 kinah: (100, 0) to A, (100, 1) to B (ExchangeService.java:94-95)
	const std::vector<uint8_t> body = {
		0x01,                                           // 0: writeC(action) = 1, the partner's copy (SM_EXCHANGE_ADD_KINAH.java:21)
		0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 1: writeQ(kinahCount) = 100                          (:22)
	};
	EXPECT_EQ(decodeExchangeAddKinah(body), (ExchangeAddKinah{EXCHANGE_OTHER, 100}));
	PacketWriter large; // a long: an amount above 2^31 survives
	large.C(EXCHANGE_SELF).Q(5'000'000'000LL);
	EXPECT_EQ(decodeExchangeAddKinah(large.data), (ExchangeAddKinah{EXCHANGE_SELF, 5'000'000'000LL}));
	EXPECT_THROW(decodeExchangeAddKinah(std::span<const uint8_t>(body).first(5)), DecodeError) << "an int instead of the long";
	std::vector<uint8_t> trailing = body;
	trailing.push_back(0);
	EXPECT_THROW(decodeExchangeAddKinah(trailing), DecodeError);

	// X9: LOCK answers (3) to the partner, OK (2), the second OK (0) to both; a cancel (1) (ExchangeService.java:178, 188, 228, 254-255)
	EXPECT_EQ(decodeExchangeConfirmation(std::vector<uint8_t>{3}), EXCHANGE_CONFIRMATION_PARTNER_LOCKED);
	EXPECT_EQ(decodeExchangeConfirmation(std::vector<uint8_t>{2}), EXCHANGE_CONFIRMATION_PARTNER_CONFIRMED);
	EXPECT_EQ(decodeExchangeConfirmation(std::vector<uint8_t>{1}), EXCHANGE_CONFIRMATION_CANCELLED);
	EXPECT_EQ(decodeExchangeConfirmation(std::vector<uint8_t>{0}), EXCHANGE_CONFIRMATION_DONE);
	EXPECT_THROW(decodeExchangeConfirmation(std::vector<uint8_t>{}), DecodeError);
	EXPECT_THROW(decodeExchangeConfirmation(std::vector<uint8_t>{2, 0}), DecodeError);
}

// ---- SM_MAIL_SERVICE (§10.3 X3, X13, X14) -----------------------------------------------------------------------------------------------

TEST(EconomyDecodersTest, MailServiceMailboxStateAndMessage) {
	// X13: B's "new mail" notice after A's letter: one letter, one unread, no express, no black cloud (SM_MAIL_SERVICE.java:95, 124-129)
	const std::vector<uint8_t> state = {
		0x00,       // 0: writeC(serviceId) = 0    (:93)
		0x01, 0x00, // 1: writeH(totalCount)       (:125)
		0x01, 0x00, // 3: writeH(unreadCount)      (:126)
		0x00, 0x00, // 5: writeH(expressCount)     (:127)
		0x02, 0x00, // 7: writeH(blackCloudCount)  (:128), 2 to tell it from the express count
	};
	const MailService notice = decodeMailService(state);
	EXPECT_EQ(notice.serviceId, MAIL_SERVICE_MAILBOX_STATE);
	ASSERT_TRUE(notice.mailboxState);
	EXPECT_EQ(*notice.mailboxState, (MailboxCounts{1, 1, 0, 2}));
	EXPECT_FALSE(notice.mailMessage || notice.letterList || notice.letterRead || notice.attachmentTaken || notice.lettersDeleted);
	EXPECT_THROW(decodeMailService(std::span<const uint8_t>(state).first(7)), DecodeError);
	// three letters, one of them unread, one unread express: the four shorts in Java's order total, unread, express, black cloud (:125-128)
	const std::vector<uint8_t> three = {0x00, 0x03, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00};
	EXPECT_EQ(*decodeMailService(three).mailboxState, (MailboxCounts{3, 1, 1, 0})) << "the total comes before the unread count";

	// X13 / X14: sendMail answers the sender with a MailMessage (MailService.java:74, 168)
	const MailService sent = decodeMailService(std::vector<uint8_t>{1, MAIL_MESSAGE_SEND_SUCCESS});
	ASSERT_TRUE(sent.mailMessage);
	EXPECT_EQ(*sent.mailMessage, MAIL_MESSAGE_SEND_SUCCESS);
	EXPECT_FALSE(sent.mailboxState);
	EXPECT_EQ(*decodeMailService(std::vector<uint8_t>{1, 1}).mailMessage, MAIL_MESSAGE_NO_SUCH_CHARACTER_NAME);
	EXPECT_THROW(decodeMailService(std::vector<uint8_t>{1, 0, 0}), DecodeError);

	EXPECT_THROW(decodeMailService(std::vector<uint8_t>{4}), DecodeError) << "no constructor sets service 4";
	EXPECT_THROW(decodeMailService(std::vector<uint8_t>{7}), DecodeError);
	EXPECT_THROW(decodeMailService(std::vector<uint8_t>{}), DecodeError);
}

TEST(EconomyDecodersTest, MailServiceLetterList) {
	// X13: B's list after A's first letter - one unread NORMAL letter with the potions and 200 kinah - as the last (and only) part
	PacketWriter w;
	w.C(MAIL_SERVICE_LETTER_LIST).D(0x400B0600).C(0).H(-1); // SM_MAIL_SERVICE.java:93, 105-107: one letter, negated for the last part
	w.D(0x7000100).S("Mfivecwarrior").S("m5c").C(0);         // :109-112: unread
	w.D(0x7000010).D(162000002).Q(200).C(LETTER_TYPE_NORMAL); // :113-116
	const MailService mail = decodeMailService(w.data);
	ASSERT_TRUE(mail.letterList);
	EXPECT_EQ(mail.letterList->playerObjectId, 0x400B0600);
	EXPECT_TRUE(mail.letterList->lastPacket);
	ASSERT_EQ(mail.letterList->letters.size(), 1u);
	EXPECT_EQ(mail.letterList->letters[0],
	          (LetterListEntry{0x7000100, "Mfivecwarrior", "m5c", false, 0x7000010, 162000002, 200, LETTER_TYPE_NORMAL}));

	// X13's second list: the letter read, both attachments taken; a part that is not the last writes the count as it is
	PacketWriter taken;
	taken.C(2).D(0x400B0600).C(0).H(1).D(0x7000100).S("Mfivecwarrior").S("m5c").C(1).D(0).D(0).Q(0).C(0);
	const LetterList second = *decodeMailService(taken.data).letterList;
	EXPECT_FALSE(second.lastPacket);
	EXPECT_EQ(second.letters[0], (LetterListEntry{0x7000100, "Mfivecwarrior", "m5c", true, 0, 0, 0, LETTER_TYPE_NORMAL}));

	PacketWriter empty; // an empty mailbox: -0 == 0, so the last (and only) part of an empty list decodes as NOT the last one
	empty.C(2).D(0x400B0600).C(0).H(0);
	const LetterList none = *decodeMailService(empty.data).letterList;
	EXPECT_TRUE(none.letters.empty());
	EXPECT_FALSE(none.lastPacket) << "lastPacket is `count < 0`: an empty part cannot say it is the last";

	std::vector<uint8_t> changedConstant = w.data;
	changedConstant[5] = 1;
	EXPECT_THROW(decodeMailService(changedConstant), DecodeError) << "writeC(0) at :106 is a constant";
	PacketWriter notAFlag;
	notAFlag.C(2).D(0x400B0600).C(0).H(1).D(0x7000100).S("a").S("b").C(2).D(0).D(0).Q(0).C(0);
	EXPECT_THROW(decodeMailService(notAFlag.data), DecodeError) << "the read flag is `isUnread() ? 0 : 1`";
	PacketWriter halfAnItem;
	halfAnItem.C(2).D(0x400B0600).C(0).H(1).D(0x7000100).S("a").S("b").C(0).D(0x7000010).D(0).Q(0).C(0);
	EXPECT_THROW(decodeMailService(halfAnItem.data), DecodeError) << "an item has both ids or neither";
	PacketWriter templateOnly; // the other half: a template id without an object id (:113-114 write both from the same item or 0 for both)
	templateOnly.C(2).D(0x400B0600).C(0).H(1).D(0x7000100).S("a").S("b").C(0).D(0).D(162000002).Q(0).C(0);
	EXPECT_THROW(decodeMailService(templateOnly.data), DecodeError) << "a template id needs its object id";
	PacketWriter twoAnnounced;
	twoAnnounced.C(2).D(0x400B0600).C(0).H(-2).D(0x7000100).S("a").S("b").C(0).D(0).D(0).Q(0).C(0);
	EXPECT_THROW(decodeMailService(twoAnnounced.data), DecodeError) << "-2 announces two letters";
}

TEST(EconomyDecodersTest, MailServiceLetterReadWithAndWithoutAnItem) {
	// X13: B reads A's first letter: the potions attached, 200 kinah; the counts are packed as total + unread * 0x10000 (:133)
	PacketWriter w;
	w.C(MAIL_SERVICE_LETTER_READ);                          // :93
	w.D(0x400B0600).D(1 + 1 * 0x10000).D(0);                // :132-134: recipient, 1 letter 1 unread, no express
	w.D(0x7000100).D(0x400B0600);                           // :135-136
	w.S("Mfivecwarrior").S("m5c").S("gate");                // :137-139
	w.D(0x7000010).D(162000002).D(1).D(0);                  // :145-148
	const std::string name = l10n(w, 702583);               // :149
	plainBlob(w, 12414, 5);                                 // :151-152
	w.D(200).D(0).C(0).D(1'790'000'000).C(LETTER_TYPE_NORMAL); // :159-163
	const MailService mail = decodeMailService(w.data);
	ASSERT_TRUE(mail.letterRead);
	const LetterRead& read = *mail.letterRead;
	EXPECT_EQ(read.recipientObjectId, 0x400B0600);
	EXPECT_EQ(read.counts, (MailboxCounts{1, 1, 0, 0}));
	EXPECT_EQ(read.letterObjectId, 0x7000100);
	EXPECT_EQ(read.senderName, "Mfivecwarrior");
	EXPECT_EQ(read.title, "m5c");
	EXPECT_EQ(read.message, "gate");
	ASSERT_TRUE(read.attachedItem);
	EXPECT_EQ(read.attachedItem->objectId, 0x7000010);
	EXPECT_EQ(read.attachedItem->templateId, 162000002);
	EXPECT_EQ(read.attachedItem->l10n, name);
	ASSERT_TRUE(read.attachedItem->general);
	EXPECT_EQ(read.attachedItem->general->count, 5);
	EXPECT_EQ(read.attachedKinah, 200);
	EXPECT_EQ(read.timeSeconds, 1'790'000'000);
	EXPECT_EQ(read.letterType, LETTER_TYPE_NORMAL);

	// A's second letter carries 10 kinah and no item: writeQ(0), writeQ(0), writeD(0) instead (:154-156)
	PacketWriter kinahOnly;
	kinahOnly.C(3).D(0x400B0600).D(2 + 2 * 0x10000).D(1).D(0x7000101).D(0x400B0600).S("Mfivecwarrior").S("m5c").S("");
	kinahOnly.Q(0).Q(0).D(0);
	kinahOnly.D(10).D(0).C(0).D(1'790'000'100).C(LETTER_TYPE_NORMAL);
	ASSERT_EQ(kinahOnly.data.size(), 1u + 20u + 28u + 8u + 2u + 20u + 14u);
	const LetterRead plain = *decodeMailService(kinahOnly.data).letterRead;
	EXPECT_FALSE(plain.attachedItem);
	EXPECT_EQ(plain.counts, (MailboxCounts{2, 2, 1, 0})) << "the express and black cloud counts arrive as one sum";
	EXPECT_EQ(plain.attachedKinah, 10);

	std::vector<uint8_t> otherRecipient = kinahOnly.data;
	otherRecipient[17] = 0x01; // the first byte of the second recipient id (:136)
	EXPECT_THROW(decodeMailService(otherRecipient), DecodeError) << "the recipient id is written twice";
	PacketWriter apReward;
	apReward.C(3).D(0x400B0600).D(1).D(0).D(0x7000101).D(0x400B0600).S("").S("").S("").Q(0).Q(0).D(0).D(10).D(5).C(0).D(0).C(0);
	EXPECT_THROW(decodeMailService(apReward.data), DecodeError) << "writeD(0) at :160 is a constant";
	PacketWriter apByte; // the byte after the AP reward, :161's writeC(0), set to 1; everything else as `plain`
	apByte.C(3).D(0x400B0600).D(1).D(0).D(0x7000101).D(0x400B0600).S("").S("").S("").Q(0).Q(0).D(0).D(10).D(0).C(1).D(0).C(0);
	EXPECT_THROW(decodeMailService(apByte.data), DecodeError) << "writeC(0) at :161 is a constant";
	PacketWriter apByteZero; // the same body with the byte 0 decodes: only the byte differs
	apByteZero.C(3).D(0x400B0600).D(1).D(0).D(0x7000101).D(0x400B0600).S("").S("").S("").Q(0).Q(0).D(0).D(10).D(0).C(0).D(0).C(0);
	EXPECT_EQ(decodeMailService(apByteZero.data).letterRead->attachedKinah, 10);
	PacketWriter manyExpress; // services 3 and 6 write `express + blackCloud` as an int; a mailbox count above a short's range is no count
	manyExpress.C(3).D(0x400B0600).D(1).D(0x10000).D(0x7000101).D(0x400B0600).S("").S("").S("").Q(0).Q(0).D(0).D(10).D(0).C(0).D(0).C(0);
	EXPECT_THROW(decodeMailService(manyExpress.data), DecodeError) << "0x10000 unread express and black cloud letters";
	PacketWriter mostExpress; // 0xFFFF still fits the uint16_t of MailboxCounts
	mostExpress.C(3).D(0x400B0600).D(1).D(0xFFFF).D(0x7000101).D(0x400B0600).S("").S("").S("").Q(0).Q(0).D(0).D(10).D(0).C(0).D(0).C(0);
	EXPECT_EQ(decodeMailService(mostExpress.data).letterRead->counts.unreadExpress, 0xFFFF);
	PacketWriter notZero;
	notZero.C(3).D(0x400B0600).D(1).D(0).D(0x7000101).D(0x400B0600).S("").S("").S("").D(0).D(0).D(1).D(0).D(0).D(10).D(0).C(0).D(0).C(0);
	EXPECT_THROW(decodeMailService(notZero.data), DecodeError) << "a letter without an item writes 20 zero bytes";
	PacketWriter itemConstant;
	itemConstant.C(3).D(0x400B0600).D(1).D(0).D(0x7000100).D(0x400B0600).S("").S("").S("").D(0x7000010).D(162000002).D(2).D(0);
	l10n(itemConstant, 702583);
	plainBlob(itemConstant, 12414, 5);
	itemConstant.D(0).D(0).C(0).D(0).C(0);
	EXPECT_THROW(decodeMailService(itemConstant.data), DecodeError) << "writeD(1) at :147 is a constant";
	PacketWriter secondConstant; // writeD(1), then writeD(5) where :148 writes writeD(0)
	secondConstant.C(3).D(0x400B0600).D(1).D(0).D(0x7000100).D(0x400B0600).S("").S("").S("").D(0x7000010).D(162000002).D(1).D(5);
	l10n(secondConstant, 702583);
	plainBlob(secondConstant, 12414, 5);
	secondConstant.D(0).D(0).C(0).D(0).C(0);
	EXPECT_THROW(decodeMailService(secondConstant.data), DecodeError) << "writeD(0) at :148 is a constant";
}

TEST(EconomyDecodersTest, MailServiceAttachmentTakenAndLettersDeleted) {
	// X13: the item, then the kinah (MailService.java:238, 251; SM_MAIL_SERVICE.java:166-170)
	const std::vector<uint8_t> item = {
		0x05,                   // 0: writeC(serviceId) = 5
		0x00, 0x01, 0x00, 0x07, // 1: writeD(letterId)
		0x00,                   // 5: writeC(attachmentType) = item
		0x01,                   // 6: writeC(1)
	};
	const MailService taken = decodeMailService(item);
	ASSERT_TRUE(taken.attachmentTaken);
	EXPECT_EQ(*taken.attachmentTaken, (AttachmentTaken{0x7000100, MAIL_ATTACHMENT_ITEM}));
	EXPECT_EQ(decodeMailService(std::vector<uint8_t>{5, 0x00, 0x01, 0x00, 0x07, 1, 1}).attachmentTaken->attachmentType, MAIL_ATTACHMENT_KINAH);
	std::vector<uint8_t> changedConstant = item;
	changedConstant[6] = 0;
	EXPECT_THROW(decodeMailService(changedConstant), DecodeError) << "writeC(1) at :169 is a constant";

	// X13: B deletes the letter; the counts are the mailbox's after the delete, packed as in service 3 (:172-178)
	PacketWriter w;
	w.C(MAIL_SERVICE_LETTERS_DELETED).D(0).D(0).H(1).D(0x7000100);
	ASSERT_EQ(w.data.size(), 15u);
	const MailService deleted = decodeMailService(w.data);
	ASSERT_TRUE(deleted.lettersDeleted);
	EXPECT_EQ(deleted.lettersDeleted->counts, (MailboxCounts{}));
	EXPECT_EQ(deleted.lettersDeleted->letterObjectIds, (std::vector<int32_t>{0x7000100}));
	PacketWriter two;
	two.C(6).D(3 + 1 * 0x10000).D(0).H(2).D(0x7000101).D(0x7000102);
	const LettersDeleted both = *decodeMailService(two.data).lettersDeleted;
	EXPECT_EQ(both.counts, (MailboxCounts{3, 1, 0, 0}));
	EXPECT_EQ(both.letterObjectIds, (std::vector<int32_t>{0x7000101, 0x7000102}));
	EXPECT_THROW(decodeMailService(std::span<const uint8_t>(w.data).first(11)), DecodeError) << "the id announced is missing";
	PacketWriter negativeExpress;
	negativeExpress.C(6).D(0).D(-1).H(0);
	EXPECT_THROW(decodeMailService(negativeExpress.data), DecodeError) << "a mailbox count is never negative";
}

// ---- SM_PRIVATE_STORE, SM_PRIVATE_STORE_NAME (§10.3 X12) --------------------------------------------------------------------------------

TEST(EconomyDecodersTest, PrivateStoreOfOneEntry) {
	// X12: A sells 5 of its potion stack at 100 each; the blob is the seller's whole stack (SM_PRIVATE_STORE.java:31-38)
	PacketWriter w;
	w.D(0x400B0600).H(1);                           // :31-32
	w.D(0x7000010).D(162000002).H(5).Q(100);        // :34-37
	plainBlob(w, 12414, 90);                        // :38, the seller's item
	const PrivateStore store = decodePrivateStore(w.data);
	EXPECT_TRUE(store.present);
	EXPECT_EQ(store.sellerObjectId, 0x400B0600);
	ASSERT_EQ(store.items.size(), 1u);
	const PrivateStoreEntry& entry = store.items[0];
	EXPECT_EQ(entry.itemObjectId, 0x7000010);
	EXPECT_EQ(entry.itemId, 162000002);
	EXPECT_EQ(entry.count, 5);
	EXPECT_EQ(entry.price, 100) << "the price of one item";
	EXPECT_EQ(entry.item.objectId, 0x7000010);
	EXPECT_EQ(entry.item.templateId, 162000002);
	EXPECT_TRUE(entry.item.l10n.empty()) << "no l10n is written";
	ASSERT_TRUE(entry.item.general);
	EXPECT_EQ(entry.item.general->count, 90);

	const PrivateStore closed = decodePrivateStore(std::vector<uint8_t>{});
	EXPECT_FALSE(closed.present) << "a null store writes nothing (:27)";
	EXPECT_TRUE(closed.items.empty());

	std::vector<uint8_t> trailing = w.data;
	trailing.push_back(0);
	EXPECT_THROW(decodePrivateStore(trailing), DecodeError);
	EXPECT_THROW(decodePrivateStore(std::span<const uint8_t>(w.data).first(6)), DecodeError) << "one entry announced, none there";
	EXPECT_THROW(decodePrivateStore(std::vector<uint8_t>{0, 0}), DecodeError) << "a seller id cut short";
}

TEST(EconomyDecodersTest, PrivateStoreName) {
	// X12: CM_PRIVATE_STORE_NAME("m5c") -> SM_PRIVATE_STORE_NAME(A, "m5c") (SM_PRIVATE_STORE_NAME.java:23-24)
	PacketWriter w;
	w.D(0x400B0600).S("m5c");
	ASSERT_EQ(w.data.size(), 4u + 8u);
	EXPECT_EQ(decodePrivateStoreName(w.data), (PrivateStoreName{0x400B0600, "m5c"}));
	PacketWriter none;
	none.D(0x400B0600).H(0);
	EXPECT_EQ(decodePrivateStoreName(none.data).name, "") << "writeS(null) for a store without a message";
	std::vector<uint8_t> trailing = w.data;
	trailing.push_back(0);
	EXPECT_THROW(decodePrivateStoreName(trailing), DecodeError);
}

// ---- crafting (§10.3 X17-X21a) ----------------------------------------------------------------------------------------------------------

TEST(EconomyDecodersTest, CraftUpdateMessagesFollowTheAction) {
	// X19: recipe 155001381 makes Roast Inina (160001001, desc 729414) with cooking (40001). INIT carries 1330048 and the product's l10n
	// (SM_CRAFT_UPDATE.java:38-51); the progress values here are hand-picked, the layout is the point
	PacketWriter init;
	init.H(40001).C(CRAFT_UPDATE_INIT).D(160001001).D(1000).D(1000).D(100).D(2500).D(CRAFT_MESSAGE_START);
	const std::string name = l10n(init, 729414);
	ASSERT_EQ(init.data.size(), 2u + 1u + 20u + 4u + 8u) << "the short, the action, five ints, the message id and the l10n";
	const CraftUpdate start = decodeCraftUpdate(init.data);
	EXPECT_EQ(start.skillId, 40001);
	EXPECT_EQ(start.action, CRAFT_UPDATE_INIT);
	EXPECT_EQ(start.itemId, 160001001);
	EXPECT_EQ(start.success, 1000);
	EXPECT_EQ(start.failure, 1000);
	EXPECT_EQ(start.executionSpeed, 100);
	EXPECT_EQ(start.delay, 2500);
	EXPECT_TRUE(start.hasMessage);
	EXPECT_EQ(start.messageId, CRAFT_MESSAGE_START);
	EXPECT_EQ(start.itemNameL10n, name);

	PacketWriter progress; // X20: a progress update, 0 and writeS(null) (:52-56)
	progress.H(40001).C(CRAFT_UPDATE_NORMAL).D(160001001).D(140).D(0).D(100).D(2500).D(0).H(0);
	const CraftUpdate step = decodeCraftUpdate(progress.data);
	EXPECT_EQ(step.success, 140);
	EXPECT_EQ(step.messageId, 0);
	EXPECT_EQ(step.itemNameL10n, "");

	PacketWriter cancel; // X18: the cancel pair of CraftService.sendCancelCraft, action 4 with 1330051 (:57-60)
	cancel.H(40001).C(CRAFT_UPDATE_CANCELLED).D(160001001).D(0).D(0).D(0).D(0).D(CRAFT_MESSAGE_CANCELLED).H(0);
	EXPECT_EQ(decodeCraftUpdate(cancel.data).messageId, CRAFT_MESSAGE_CANCELLED);

	PacketWriter success; // X19: SUCCESS, 1330049 with the name (:61-64)
	success.H(40001).C(CRAFT_UPDATE_SUCCESS).D(160001001).D(1000).D(0).D(100).D(2500).D(CRAFT_MESSAGE_SUCCESS);
	l10n(success, 729414);
	EXPECT_EQ(decodeCraftUpdate(success.data).itemNameL10n, name);

	PacketWriter failed; // FAILED and FAILURE share 1330050 (:65-69)
	failed.H(40001).C(CRAFT_UPDATE_FAILURE).D(160001001).D(0).D(1000).D(100).D(2500).D(CRAFT_MESSAGE_FAILED);
	l10n(failed, 729414);
	EXPECT_EQ(decodeCraftUpdate(failed.data).messageId, CRAFT_MESSAGE_FAILED);
	PacketWriter failedEnd; // action 6, "failed (end)": the same 1330050 and the product's name (:65-69)
	failedEnd.H(40001).C(CRAFT_UPDATE_FAILED).D(160001001).D(0).D(1000).D(100).D(2500).D(CRAFT_MESSAGE_FAILED);
	l10n(failedEnd, 729414);
	const CraftUpdate failure = decodeCraftUpdate(failedEnd.data);
	EXPECT_EQ(failure.action, CRAFT_UPDATE_FAILED);
	EXPECT_TRUE(failure.hasMessage);
	EXPECT_EQ(failure.messageId, CRAFT_MESSAGE_FAILED);
	EXPECT_EQ(failure.itemNameL10n, name);

	PacketWriter purple; // action 3, the purple critical (a proc): INIT's 1330048 and the name (:47-51), not a progress update's 0
	purple.H(40001).C(CRAFT_UPDATE_CRIT_PURPLE).D(160001001).D(500).D(0).D(100).D(2500).D(CRAFT_MESSAGE_START);
	l10n(purple, 729414);
	const CraftUpdate proc = decodeCraftUpdate(purple.data);
	EXPECT_EQ(proc.messageId, CRAFT_MESSAGE_START);
	EXPECT_EQ(proc.itemNameL10n, name);
	PacketWriter purpleWithoutName;
	purpleWithoutName.H(40001).C(CRAFT_UPDATE_CRIT_PURPLE).D(160001001).D(500).D(0).D(100).D(2500).D(0).H(0);
	EXPECT_THROW(decodeCraftUpdate(purpleWithoutName.data), DecodeError) << "action 3 writes 1330048, not a progress update's 0";

	PacketWriter noCase; // an action without a case writes nothing after the delay
	noCase.H(40001).C(8).D(160001001).D(0).D(0).D(0).D(0);
	const CraftUpdate bare = decodeCraftUpdate(noCase.data);
	EXPECT_FALSE(bare.hasMessage);
	EXPECT_EQ(bare.messageId, 0);

	PacketWriter morph; // the morph skill's delay is always 1000 (:29-30)
	morph.H(CRAFT_SKILL_MORPH).C(CRAFT_UPDATE_NORMAL).D(152000901).D(0).D(0).D(0).D(1000).D(0).H(0);
	EXPECT_EQ(decodeCraftUpdate(morph.data).delay, 1000);
	PacketWriter morphDelay;
	morphDelay.H(CRAFT_SKILL_MORPH).C(CRAFT_UPDATE_NORMAL).D(152000901).D(0).D(0).D(0).D(200).D(0).H(0);
	EXPECT_THROW(decodeCraftUpdate(morphDelay.data), DecodeError) << "the constructor replaces the morph skill's delay by 1000";

	PacketWriter wrongMessage;
	wrongMessage.H(40001).C(CRAFT_UPDATE_CANCELLED).D(160001001).D(0).D(0).D(0).D(0).D(CRAFT_MESSAGE_FAILED).H(0);
	EXPECT_THROW(decodeCraftUpdate(wrongMessage.data), DecodeError) << "action 4 writes 1330051";
	PacketWriter nameOnCancel;
	nameOnCancel.H(40001).C(CRAFT_UPDATE_CANCELLED).D(160001001).D(0).D(0).D(0).D(0).D(CRAFT_MESSAGE_CANCELLED).S("x");
	EXPECT_THROW(decodeCraftUpdate(nameOnCancel.data), DecodeError) << "CANCELLED writes writeS(null) (:59)";
	PacketWriter nameOnProgress;
	nameOnProgress.H(40001).C(CRAFT_UPDATE_CRIT_BLUE).D(160001001).D(0).D(0).D(0).D(0).D(0).S("x");
	EXPECT_THROW(decodeCraftUpdate(nameOnProgress.data), DecodeError) << "CRIT_BLUE writes writeS(null)";
	std::vector<uint8_t> trailing = noCase.data;
	trailing.push_back(0);
	EXPECT_THROW(decodeCraftUpdate(trailing), DecodeError) << "no message follows an action without a case";
	EXPECT_THROW(decodeCraftUpdate(std::span<const uint8_t>(init.data).first(23)), DecodeError) << "INIT without its message";
	EXPECT_THROW(decodeCraftUpdate(std::span<const uint8_t>(init.data).first(27)), DecodeError) << "INIT without its parameter";
}

TEST(EconomyDecodersTest, CraftAnimationAndTheRecipePackets) {
	// X19: SM_CRAFT_ANIMATION(player, oven, 40001, 0) at the start (SM_CRAFT_ANIMATION.java:25-28)
	const std::vector<uint8_t> body = {
		0x00, 0x06, 0x0B, 0x40, // 0: writeD(playerObjId)
		0x00, 0x07, 0x0B, 0x40, // 4: writeD(targetObjectId), the oven
		0x41, 0x9C,             // 8: writeH(skillId) = 40001
		0x00,                   // 10: writeC(action)
	};
	EXPECT_EQ(decodeCraftAnimation(body), (CraftAnimation{0x400B0600, 0x400B0700, 40001, 0}));
	EXPECT_THROW(decodeCraftAnimation(std::span<const uint8_t>(body).first(10)), DecodeError);
	std::vector<uint8_t> trailing = body;
	trailing.push_back(0);
	EXPECT_THROW(decodeCraftAnimation(trailing), DecodeError);

	// X17: learning cooking teaches exactly one recipe, 155001381 (SM_LEARN_RECIPE.java:19-20)
	PacketWriter learn;
	learn.D(155001381).C(0);
	EXPECT_EQ(decodeLearnRecipe(learn.data), 155001381);
	PacketWriter learnConstant;
	learnConstant.D(155001381).C(1);
	EXPECT_THROW(decodeLearnRecipe(learnConstant.data), DecodeError) << "writeC(0) at :20 is a constant";
	EXPECT_THROW(decodeLearnRecipe(std::span<const uint8_t>(learn.data).first(4)), DecodeError) << "the byte after the id is written";

	// X21: CM_RECIPE_DELETE -> SM_RECIPE_DELETE(155001381) (SM_RECIPE_DELETE.java:19)
	PacketWriter deleted;
	deleted.D(155001381);
	EXPECT_EQ(decodeRecipeDelete(deleted.data), 155001381);
	EXPECT_THROW(decodeRecipeDelete(learn.data), DecodeError) << "five bytes are SM_LEARN_RECIPE's, not SM_RECIPE_DELETE's";

	// X21a: the three Elyos morph recipes of the Daeva's enter world (SM_RECIPE_LIST.java:21-24; the Set's order is not asserted)
	PacketWriter list;
	list.H(3).D(155000001).C(0).D(155000002).C(0).D(155000005).C(0);
	ASSERT_EQ(list.data.size(), 2u + 3u * 5u);
	EXPECT_EQ(decodeRecipeList(list.data), (std::vector<int32_t>{155000001, 155000002, 155000005}));
	EXPECT_TRUE(decodeRecipeList(std::vector<uint8_t>{0, 0}).empty());
	PacketWriter listConstant;
	listConstant.H(1).D(155000001).C(2);
	EXPECT_THROW(decodeRecipeList(listConstant.data), DecodeError) << "writeC(0) at :24 is a constant";
	EXPECT_THROW(decodeRecipeList(std::span<const uint8_t>(list.data).first(12)), DecodeError) << "three announced, two there";
}

} // namespace
} // namespace aion::gameserver::scenario::decoders
