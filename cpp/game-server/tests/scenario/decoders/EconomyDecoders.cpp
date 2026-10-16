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

// ---- the exchange -----------------------------------------------------------------------------------------------------------------------

std::string decodeExchangeRequest(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_EXCHANGE_REQUEST");
	std::string receiver = reader.S(); // SM_EXCHANGE_REQUEST.java:19
	reader.expectFullyConsumed();
	return receiver;
}

ExchangeAddItem decodeExchangeAddItem(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_EXCHANGE_ADD_ITEM");
	ExchangeAddItem added;
	added.action = reader.C();                    // SM_EXCHANGE_ADD_ITEM.java:29
	added.item.templateId = reader.D();           // :31, the template id first
	added.item.objectId = reader.D();             // :32
	added.item.l10n = reader.S();                 // :33
	readItemInfoBlob(reader, added.item);         // :35-36, ItemInfoBlob.getFullBlob
	reader.expectFullyConsumed();
	return added;
}

ExchangeAddKinah decodeExchangeAddKinah(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_EXCHANGE_ADD_KINAH");
	ExchangeAddKinah added;
	added.action = reader.C();     // SM_EXCHANGE_ADD_KINAH.java:21
	added.kinahCount = reader.Q(); // :22
	reader.expectFullyConsumed();
	return added;
}

uint8_t decodeExchangeConfirmation(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_EXCHANGE_CONFIRMATION");
	const uint8_t action = reader.C(); // SM_EXCHANGE_CONFIRMATION.java:19
	reader.expectFullyConsumed();
	return action;
}

// ---- SM_MAIL_SERVICE --------------------------------------------------------------------------------------------------------------------

namespace {

/** writeD(totalCount + unreadCount * 0x10000), writeD(expressCount + blackCloudCount): the packed counts of services 3 and 6 */
MailboxCounts readPackedCounts(BodyReader& reader) {
	MailboxCounts counts;
	const auto packed = static_cast<uint32_t>(reader.D());
	counts.total = static_cast<uint16_t>(packed & 0xFFFFu);
	counts.unread = static_cast<uint16_t>(packed >> 16);
	const int32_t expressAndBlackCloud = reader.D();
	if (expressAndBlackCloud < 0 || expressAndBlackCloud > 0xFFFF)
		reader.fail("the unread express + black cloud count " + std::to_string(expressAndBlackCloud) + " is no mailbox count");
	counts.unreadExpress = static_cast<uint16_t>(expressAndBlackCloud);
	return counts;
}

LetterList readLetterList(BodyReader& reader) {
	LetterList list;
	list.playerObjectId = reader.D();                           // SM_MAIL_SERVICE.java:105
	reader.expectC(0, "the byte after the player's object id"); // :106
	const int16_t count = reader.Hs();                          // :107, negated for the last part
	list.lastPacket = count < 0;
	const int32_t letters = count < 0 ? -static_cast<int32_t>(count) : count;
	for (int32_t i = 0; i < letters; i++) {
		LetterListEntry letter;
		letter.letterObjectId = reader.D();                // :109
		letter.senderName = reader.S();                    // :110
		letter.title = reader.S();                         // :111
		const uint8_t read = reader.C();                   // :112, `isUnread() ? 0 : 1`
		if (read > 1)
			reader.fail("the read flag is `isUnread() ? 0 : 1`, got " + std::to_string(read));
		letter.read = read == 1;
		letter.attachedItemObjectId = reader.D();          // :113
		letter.attachedItemTemplateId = reader.D();        // :114
		if ((letter.attachedItemObjectId == 0) != (letter.attachedItemTemplateId == 0))
			reader.fail("an attached item has both an object id and a template id, a letter without one writes 0 for both (:113-114)");
		letter.attachedKinah = reader.Q();                 // :115
		letter.letterType = reader.C();                    // :116
		list.letters.push_back(std::move(letter));
	}
	return list;
}

LetterRead readLetterRead(BodyReader& reader) {
	LetterRead read;
	read.recipientObjectId = reader.D();    // SM_MAIL_SERVICE.java:132
	read.counts = readPackedCounts(reader); // :133-134
	read.letterObjectId = reader.D();       // :135
	const int32_t recipientAgain = reader.D(); // :136
	if (recipientAgain != read.recipientObjectId)
		reader.fail("the recipient id is written twice (:132, :136), but " + std::to_string(read.recipientObjectId) + " and " +
		            std::to_string(recipientAgain) + " differ");
	read.senderName = reader.S(); // :137
	read.title = reader.S();      // :138
	read.message = reader.S();    // :139
	const int32_t itemObjectId = reader.D();
	if (itemObjectId != 0) { // :142-152, an attached item
		InventoryItem item;
		item.objectId = itemObjectId;                               // :145
		item.templateId = reader.D();                               // :146
		reader.expectD(1, "the int after the attached item's template id"); // :147, writeD(1)
		reader.expectD(0, "the second int after the attached item's template id"); // :148, writeD(0)
		item.l10n = reader.S();                                     // :149
		readItemInfoBlob(reader, item);                             // :151-152
		read.attachedItem = std::move(item);
	} else {
		reader.expectZeros(16, "the rest of the writeQ(0), writeQ(0), writeD(0) of a letter without an item"); // :154-156
	}
	read.attachedKinah = reader.D();                          // :159
	reader.expectD(0, "the AP reward int");                   // :160
	reader.expectC(0, "the byte after the AP reward");        // :161
	read.timeSeconds = reader.D();                            // :162
	read.letterType = reader.C();                             // :163
	return read;
}

} // namespace

MailService decodeMailService(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_MAIL_SERVICE");
	MailService mail;
	mail.serviceId = reader.C(); // SM_MAIL_SERVICE.java:93
	switch (mail.serviceId) {
		case MAIL_SERVICE_MAILBOX_STATE: { // :95, writeMailboxState (:124-129)
			MailboxCounts counts;
			counts.total = reader.H();
			counts.unread = reader.H();
			counts.unreadExpress = reader.H();
			counts.unreadBlackCloud = reader.H();
			mail.mailboxState = counts;
			break;
		}
		case MAIL_SERVICE_MESSAGE: // :96, writeMailMessage (:120-122)
			mail.mailMessage = reader.C();
			break;
		case MAIL_SERVICE_LETTER_LIST: // :97
			mail.letterList = readLetterList(reader);
			break;
		case MAIL_SERVICE_LETTER_READ: // :98
			mail.letterRead = readLetterRead(reader);
			break;
		case MAIL_SERVICE_ATTACHMENT_TAKEN: { // :99, writeLetterState (:166-170)
			AttachmentTaken taken;
			taken.letterObjectId = reader.D();
			taken.attachmentType = reader.C();
			reader.expectC(1, "the byte after the attachment type"); // :169, writeC(1)
			mail.attachmentTaken = taken;
			break;
		}
		case MAIL_SERVICE_LETTERS_DELETED: { // :100, writeLetterDelete (:172-178)
			LettersDeleted deleted;
			deleted.counts = readPackedCounts(reader);
			const uint16_t count = reader.H();
			for (uint16_t i = 0; i < count; i++)
				deleted.letterObjectIds.push_back(reader.D());
			mail.lettersDeleted = std::move(deleted);
			break;
		}
		default: // only the six constructors set serviceId (:37-84)
			reader.fail("service " + std::to_string(mail.serviceId) + " is none of the six SM_MAIL_SERVICE constructors (0, 1, 2, 3, 5, 6)");
	}
	reader.expectFullyConsumed();
	return mail;
}

// ---- SM_PRIVATE_STORE, SM_PRIVATE_STORE_NAME --------------------------------------------------------------------------------------------

PrivateStore decodePrivateStore(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_PRIVATE_STORE");
	PrivateStore store;
	if (reader.remaining() == 0) // :27, a null store writes nothing
		return store;
	store.present = true;
	store.sellerObjectId = reader.D();  // SM_PRIVATE_STORE.java:31
	const uint16_t count = reader.H(); // :32
	for (uint16_t i = 0; i < count; i++) {
		PrivateStoreEntry entry;
		entry.itemObjectId = reader.D(); // :34
		entry.itemId = reader.D();       // :35
		entry.count = reader.H();        // :36
		entry.price = reader.Q();        // :37
		entry.item.objectId = entry.itemObjectId;
		entry.item.templateId = entry.itemId;
		readItemInfoBlob(reader, entry.item); // :38
		store.items.push_back(std::move(entry));
	}
	reader.expectFullyConsumed();
	return store;
}

PrivateStoreName decodePrivateStoreName(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_PRIVATE_STORE_NAME");
	PrivateStoreName name;
	name.playerObjectId = reader.D(); // SM_PRIVATE_STORE_NAME.java:23
	name.name = reader.S();           // :24
	reader.expectFullyConsumed();
	return name;
}

// ---- crafting ---------------------------------------------------------------------------------------------------------------------------

CraftUpdate decodeCraftUpdate(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_CRAFT_UPDATE");
	CraftUpdate update;
	update.skillId = reader.H();        // SM_CRAFT_UPDATE.java:38
	update.action = reader.C();         // :39
	update.itemId = reader.D();         // :40
	update.success = reader.D();        // :41
	update.failure = reader.D();        // :42
	update.executionSpeed = reader.D(); // :43
	update.delay = reader.D();          // :44
	if (update.skillId == CRAFT_SKILL_MORPH && update.delay != 1000)
		reader.fail("the constructor sets the delay of the morph skill to 1000 (:29-30), got " + std::to_string(update.delay));
	int32_t expectedMessage = 0;
	bool withName = false;
	switch (update.action) {
		case CRAFT_UPDATE_INIT:
		case CRAFT_UPDATE_CRIT_PURPLE:
			expectedMessage = CRAFT_MESSAGE_START; // :47-51
			withName = true;
			break;
		case CRAFT_UPDATE_NORMAL:
		case CRAFT_UPDATE_CRIT_BLUE:
			expectedMessage = 0; // :52-56, writeS(null)
			break;
		case CRAFT_UPDATE_CANCELLED:
			expectedMessage = CRAFT_MESSAGE_CANCELLED; // :57-60, writeS(null)
			break;
		case CRAFT_UPDATE_SUCCESS:
			expectedMessage = CRAFT_MESSAGE_SUCCESS; // :61-64
			withName = true;
			break;
		case CRAFT_UPDATE_FAILED:
		case CRAFT_UPDATE_FAILURE:
			expectedMessage = CRAFT_MESSAGE_FAILED; // :65-69
			withName = true;
			break;
		default: // no case: nothing after the delay
			reader.expectFullyConsumed();
			return update;
	}
	update.hasMessage = true;
	update.messageId = reader.D();
	if (update.messageId != expectedMessage)
		reader.fail("action " + std::to_string(update.action) + " writes the message id " + std::to_string(expectedMessage) + ", got " +
		            std::to_string(update.messageId));
	update.itemNameL10n = reader.S();
	if (!withName && !update.itemNameL10n.empty())
		reader.fail("action " + std::to_string(update.action) + " writes writeS(null), got \"" + update.itemNameL10n + "\"");
	reader.expectFullyConsumed();
	return update;
}

CraftAnimation decodeCraftAnimation(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_CRAFT_ANIMATION");
	CraftAnimation animation;
	animation.playerObjectId = reader.D(); // SM_CRAFT_ANIMATION.java:25
	animation.targetObjectId = reader.D(); // :26
	animation.skillId = reader.H();        // :27
	animation.action = reader.C();         // :28
	reader.expectFullyConsumed();
	return animation;
}

int32_t decodeLearnRecipe(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_LEARN_RECIPE");
	const int32_t recipeId = reader.D();                // SM_LEARN_RECIPE.java:19
	reader.expectC(0, "the byte after the recipe id"); // :20, writeC(0) "4.0"
	reader.expectFullyConsumed();
	return recipeId;
}

int32_t decodeRecipeDelete(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_RECIPE_DELETE");
	const int32_t recipeId = reader.D(); // SM_RECIPE_DELETE.java:19
	reader.expectFullyConsumed();
	return recipeId;
}

std::vector<int32_t> decodeRecipeList(std::span<const uint8_t> body) {
	BodyReader reader(body, "SM_RECIPE_LIST");
	const uint16_t count = reader.H(); // SM_RECIPE_LIST.java:21
	std::vector<int32_t> recipes;
	recipes.reserve(count);
	for (uint16_t i = 0; i < count; i++) {
		recipes.push_back(reader.D());                     // :23
		reader.expectC(0, "the byte after a recipe id"); // :24
	}
	reader.expectFullyConsumed();
	return recipes;
}

} // namespace aion::gameserver::scenario::decoders
