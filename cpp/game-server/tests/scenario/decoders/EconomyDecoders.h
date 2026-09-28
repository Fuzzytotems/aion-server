#pragma once

// The economy packets the M5c gate reads (m5c-plan.md §2.1-§2.6, G-02; the §10.3 rows X1-X21a, X25, X26). Stage 0 (harness-a) wrote the
// packets of talking to an npc and of a shop's windows: SM_DIALOG_WINDOW, SM_PRICES, SM_TRADELIST, SM_SELL_ITEM, SM_REPURCHASE and
// SM_QUESTION_WINDOW. Stage 1 (harness-b) added the rest of G-02: the exchange (SM_EXCHANGE_REQUEST, SM_EXCHANGE_ADD_ITEM,
// SM_EXCHANGE_ADD_KINAH, SM_EXCHANGE_CONFIRMATION), the mail (SM_MAIL_SERVICE, all six service ids), the private store (SM_PRIVATE_STORE,
// SM_PRIVATE_STORE_NAME) and crafting (SM_CRAFT_UPDATE, SM_CRAFT_ANIMATION, SM_LEARN_RECIPE, SM_RECIPE_DELETE, SM_RECIPE_LIST).
// SM_SKILL_LIST's full form (the list and the one-skill form with a message) is M5a's decodeSkillList (PacketDecoders.h), SM_CUBE_UPDATE
// M5b-3's decodeCubeUpdate (ItemDecoders.h) and SM_EMOTION M5b's decodeEmotion (CombatDecoders.h).
//
// **m5a-plan.md D9:** every layout is written from the Java `writeImpl` under game-server/src/com/aionemu/gameserver/network/aion/serverpackets/
// and the helpers it calls (iteminfo/ItemInfoBlob for SM_REPURCHASE's, SM_EXCHANGE_ADD_ITEM's, SM_PRIVATE_STORE's and SM_MAIL_SERVICE's items,
// AionServerPacket / BaseServerPacket.writeS), the page ids from model/DialogPage.java, the mailbox states from
// services/player/PlayerMailboxState.java, the mail messages from model/templates/mail/MailMessage.java and the letter types from
// model/gameobjects/LetterType.java. Nothing here includes, calls or mirrors a C++ serverpackets header. The item info blob is
// PacketDecoders.cpp's reader (readItemInfoBlob), the one SM_INVENTORY_INFO uses.
//
// Every decode function consumes the body exactly and throws DecodeError otherwise; the Java constants that carry no data are verified.

#include <array>
#include <cstdint>
#include <optional>
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

// ---- the exchange: SM_EXCHANGE_REQUEST, SM_EXCHANGE_ADD_ITEM, SM_EXCHANGE_ADD_KINAH, SM_EXCHANGE_CONFIRMATION --------------------------

/** the `action` byte of SM_EXCHANGE_ADD_ITEM and SM_EXCHANGE_ADD_KINAH ("0 -self 1-other", SM_EXCHANGE_ADD_ITEM.java:29, SM_EXCHANGE_ADD_KINAH.java:21) */
constexpr uint8_t EXCHANGE_SELF = 0;
constexpr uint8_t EXCHANGE_OTHER = 1;

/**
 * The SM_EXCHANGE_CONFIRMATION actions ExchangeService sends: 0 to both once performTrade removed the items (ExchangeService.java:254-255), 1
 * to the partner of a cancel (:188), 2 to the partner of every OK, unconditionally and before the partner's state is checked (:228), 3 to the
 * partner of a lock (:178)
 */
constexpr uint8_t EXCHANGE_CONFIRMATION_DONE = 0;
constexpr uint8_t EXCHANGE_CONFIRMATION_CANCELLED = 1;
constexpr uint8_t EXCHANGE_CONFIRMATION_PARTNER_CONFIRMED = 2;
constexpr uint8_t EXCHANGE_CONFIRMATION_PARTNER_LOCKED = 3;

/** SM_EXCHANGE_REQUEST (SM_EXCHANGE_REQUEST.java:18-20): writeS(receiver), the name of the OTHER player (ExchangeService.java:50-51) */
std::string decodeExchangeRequest(std::span<const uint8_t> body);

/** SM_EXCHANGE_ADD_ITEM (SM_EXCHANGE_ADD_ITEM.java:26-37) */
struct ExchangeAddItem {
	/** writeC(action): EXCHANGE_SELF to the player who added it, EXCHANGE_OTHER to the partner (ExchangeService.java:169-170) */
	uint8_t action = 0;
	/**
	 * writeD(templateId) THEN writeD(objectId) - the reverse of SM_REPURCHASE's order -, writeS(l10n) and ItemInfoBlob.getFullBlob(player, item);
	 * no slot and no cloth byte follow, so equipmentSlot and cloth keep their defaults
	 */
	InventoryItem item;
};

ExchangeAddItem decodeExchangeAddItem(std::span<const uint8_t> body);

/** SM_EXCHANGE_ADD_KINAH (SM_EXCHANGE_ADD_KINAH.java:20-23), 9 bytes */
struct ExchangeAddKinah {
	uint8_t action = 0;
	/** writeQ(kinahCount): the amount this call added (ExchangeService.addKinah's countToAdd), not the running total */
	int64_t kinahCount = 0;

	bool operator==(const ExchangeAddKinah&) const = default;
};

ExchangeAddKinah decodeExchangeAddKinah(std::span<const uint8_t> body);

/** SM_EXCHANGE_CONFIRMATION (SM_EXCHANGE_CONFIRMATION.java:18-19): one byte, the action */
uint8_t decodeExchangeConfirmation(std::span<const uint8_t> body);

// ---- SM_MAIL_SERVICE --------------------------------------------------------------------------------------------------------------------

/** the service ids of SM_MAIL_SERVICE's six constructors (SM_MAIL_SERVICE.java:37-84); there is no service 4 */
constexpr uint8_t MAIL_SERVICE_MAILBOX_STATE = 0;
constexpr uint8_t MAIL_SERVICE_MESSAGE = 1;
constexpr uint8_t MAIL_SERVICE_LETTER_LIST = 2;
constexpr uint8_t MAIL_SERVICE_LETTER_READ = 3;
constexpr uint8_t MAIL_SERVICE_ATTACHMENT_TAKEN = 5;
constexpr uint8_t MAIL_SERVICE_LETTERS_DELETED = 6;

/** MailMessage ids (model/templates/mail/MailMessage.java), service 1's byte */
constexpr uint8_t MAIL_MESSAGE_SEND_SUCCESS = 0;
constexpr uint8_t MAIL_MESSAGE_NO_SUCH_CHARACTER_NAME = 1;
constexpr uint8_t MAIL_MESSAGE_RECIPIENT_MAILBOX_FULL = 2;
constexpr uint8_t MAIL_MESSAGE_ONE_RACE_ONLY = 3;
constexpr uint8_t MAIL_MESSAGE_IN_RECIPIENT_IGNORE_LIST = 4;
constexpr uint8_t MAIL_MESSAGE_RECIPIENT_IGNORING_LOWER_LEVEL = 5;
constexpr uint8_t MAIL_MESSAGE_SPAM_WAIT = 6;

/** LetterType.getId() (LetterType.java:8-10) */
constexpr uint8_t LETTER_TYPE_NORMAL = 0;
constexpr uint8_t LETTER_TYPE_EXPRESS = 1;
constexpr uint8_t LETTER_TYPE_BLACKCLOUD = 2;

/** CM_GET_MAIL_ATTACHMENT's attachment type, echoed by service 5 (CM_GET_MAIL_ATTACHMENT.java:24, "0 - item , 1 - kinah") */
constexpr uint8_t MAIL_ATTACHMENT_ITEM = 0;
constexpr uint8_t MAIL_ATTACHMENT_KINAH = 1;

/**
 * The mailbox counts of the connection's player (SM_MAIL_SERVICE.java:89-92: Mailbox.size, getUnreadCount, getUnreadCountByType(EXPRESS),
 * getUnreadCountByType(BLACKCLOUD)). Service 0 writes all four as shorts; services 3 and 6 pack them into two ints (`total + unread * 0x10000`
 * and `express + blackCloud`), so there only `total`, `unread` and the sum `unreadExpress + unreadBlackCloud` (kept in unreadExpress, with
 * unreadBlackCloud 0) are on the wire
 */
struct MailboxCounts {
	uint16_t total = 0;
	uint16_t unread = 0;
	uint16_t unreadExpress = 0;
	uint16_t unreadBlackCloud = 0;

	bool operator==(const MailboxCounts&) const = default;
};

/** one letter of service 2 (SM_MAIL_SERVICE.java:109-116) */
struct LetterListEntry {
	int32_t letterObjectId = 0;
	std::string senderName;
	std::string title;
	/** writeC(letter.isUnread() ? 0 : 1) */
	bool read = false;
	/** 0 without an attached item */
	int32_t attachedItemObjectId = 0;
	int32_t attachedItemTemplateId = 0;
	int64_t attachedKinah = 0;
	uint8_t letterType = 0;

	bool operator==(const LetterListEntry&) const = default;
};

/** service 2 (SM_MAIL_SERVICE.java:104-118), one part of MailService.sendMailList's split list */
struct LetterList {
	int32_t playerObjectId = 0;
	// writeC(0) (:106) is a literal the decoder verifies
	/**
	 * writeH(isLastPacket ? letters.size() * -1 : letters.size()): a negative count marks the last part. An EMPTY last part writes -0 == 0, so a
	 * part without letters decodes with lastPacket false whichever it was
	 */
	bool lastPacket = false;
	std::vector<LetterListEntry> letters;
};

/** service 3, a letter read (SM_MAIL_SERVICE.java:131-164) */
struct LetterRead {
	/** writeD(letter.getRecipientId()), written twice (:132, :136); the decoder verifies the second equals the first */
	int32_t recipientObjectId = 0;
	/** :133-134: total and unread (the low and high shorts of the first int), unread express + black cloud (the second) */
	MailboxCounts counts;
	int32_t letterObjectId = 0;
	std::string senderName;
	std::string title;
	std::string message;
	/**
	 * the attached item: object id, template id, writeD(1), writeD(0), l10n and the full blob (:145-152); without one the packet writes
	 * writeQ(0), writeQ(0), writeD(0) (:154-156), 20 zero bytes, which the decoder tells apart by the object id (never 0 for an item)
	 */
	std::optional<InventoryItem> attachedItem;
	/** writeD((int) letter.getAttachedKinah()) */
	int32_t attachedKinah = 0;
	// writeD(0) "AP reward" and writeC(0) (:160-161) are literals the decoder verifies
	/** writeD((int) (time / 1000)): the letter's time stamp in seconds */
	int32_t timeSeconds = 0;
	uint8_t letterType = 0;
};

/** service 5, an attachment taken (SM_MAIL_SERVICE.java:166-170): writeD(letterId), writeC(attachmentType), writeC(1) (verified) */
struct AttachmentTaken {
	int32_t letterObjectId = 0;
	uint8_t attachmentType = 0;

	bool operator==(const AttachmentTaken&) const = default;
};

/** service 6, letters deleted (SM_MAIL_SERVICE.java:172-178) */
struct LettersDeleted {
	/** the counts AFTER the delete: total and unread, and unread express + black cloud in unreadExpress (MailboxCounts) */
	MailboxCounts counts;
	std::vector<int32_t> letterObjectIds;
};

/** SM_MAIL_SERVICE: writeC(serviceId), then exactly one of the six bodies (SM_MAIL_SERVICE.java:93-101) */
struct MailService {
	uint8_t serviceId = 0;
	/** service 0: SystemMailService.updateRecipientMailbox's "new mail" notice and MailService.sendMailList's refresh */
	std::optional<MailboxCounts> mailboxState;
	/** service 1: a MailMessage id (MailService.sendMail's answer to the sender) */
	std::optional<uint8_t> mailMessage;
	std::optional<LetterList> letterList;
	std::optional<LetterRead> letterRead;
	std::optional<AttachmentTaken> attachmentTaken;
	std::optional<LettersDeleted> lettersDeleted;
};

MailService decodeMailService(std::span<const uint8_t> body);

// ---- SM_PRIVATE_STORE, SM_PRIVATE_STORE_NAME --------------------------------------------------------------------------------------------

/** one item of SM_PRIVATE_STORE (SM_PRIVATE_STORE.java:33-38) */
struct PrivateStoreEntry {
	/** writeD(tradeItem.getItemObjId()), writeD(tradeItem.getItemId()) */
	int32_t itemObjectId = 0;
	int32_t itemId = 0;
	/** writeH((int) tradeItem.getCount()) */
	uint16_t count = 0;
	/** writeQ(tradeItem.getPrice()): the price of ONE item */
	int64_t price = 0;
	/** ItemInfoBlob.getFullBlob of the seller's item; objectId and templateId are the two ids above, no l10n, slot or cloth is written */
	InventoryItem item;
};

/** SM_PRIVATE_STORE (SM_PRIVATE_STORE.java:26-40) */
struct PrivateStore {
	/** the packet writes nothing at all for a null store (:27, a store that closed in between) */
	bool present = false;
	int32_t sellerObjectId = 0;
	/** store.getSoldItems().values() in the LinkedHashMap's order - the index CM_BUY_ITEM(seller, 0, ...) names (PrivateStoreService.java:209) */
	std::vector<PrivateStoreEntry> items;
};

PrivateStore decodePrivateStore(std::span<const uint8_t> body);

/** SM_PRIVATE_STORE_NAME (SM_PRIVATE_STORE_NAME.java:22-25) */
struct PrivateStoreName {
	int32_t playerObjectId = 0;
	/** writeS(store.getStoreMessage()): "" for a store without a message (writeS(null) is the lone NUL char) */
	std::string name;

	bool operator==(const PrivateStoreName&) const = default;
};

PrivateStoreName decodePrivateStoreName(std::span<const uint8_t> body);

// ---- crafting: SM_CRAFT_UPDATE, SM_CRAFT_ANIMATION, SM_LEARN_RECIPE, SM_RECIPE_DELETE, SM_RECIPE_LIST ------------------------------------

/** SM_CRAFT_UPDATE's actions (SM_CRAFT_UPDATE.java:46-70) */
constexpr uint8_t CRAFT_UPDATE_INIT = 0;
constexpr uint8_t CRAFT_UPDATE_NORMAL = 1;
constexpr uint8_t CRAFT_UPDATE_CRIT_BLUE = 2;
constexpr uint8_t CRAFT_UPDATE_CRIT_PURPLE = 3;
constexpr uint8_t CRAFT_UPDATE_CANCELLED = 4;
constexpr uint8_t CRAFT_UPDATE_SUCCESS = 5;
constexpr uint8_t CRAFT_UPDATE_FAILED = 6;
constexpr uint8_t CRAFT_UPDATE_FAILURE = 7;

/** the message ids SM_CRAFT_UPDATE writes per action (:47-70) */
constexpr int32_t CRAFT_MESSAGE_START = 1330048;
constexpr int32_t CRAFT_MESSAGE_SUCCESS = 1330049;
constexpr int32_t CRAFT_MESSAGE_FAILED = 1330050;
constexpr int32_t CRAFT_MESSAGE_CANCELLED = 1330051;

/** the morph skill, whose SM_CRAFT_UPDATE always carries the delay 1000 (the constructor, SM_CRAFT_UPDATE.java:29-33) */
constexpr uint16_t CRAFT_SKILL_MORPH = 40009;

/** SM_CRAFT_UPDATE (SM_CRAFT_UPDATE.java:37-71) */
struct CraftUpdate {
	uint16_t skillId = 0;
	uint8_t action = 0;
	/** the product's template id (item.getTemplateId()) */
	int32_t itemId = 0;
	/** writeD(success), writeD(failure): the progress bars' current values, the maxima on INIT */
	int32_t success = 0;
	int32_t failure = 0;
	int32_t executionSpeed = 0;
	/** 1000 for the morph skill whatever the caller passed (verified) */
	int32_t delay = 0;
	/**
	 * the message id and its parameter: INIT and CRIT_PURPLE 1330048 with the product's l10n, NORMAL and CRIT_BLUE 0 with writeS(null),
	 * CANCELLED 1330051 with writeS(null), SUCCESS 1330049 and FAILED/FAILURE 1330050 with the l10n; the decoder verifies the id and the empty
	 * parameter against the action. Any other action writes nothing after the delay: messageId 0 and no parameter, hasMessage false
	 */
	bool hasMessage = false;
	int32_t messageId = 0;
	std::string itemNameL10n;
};

CraftUpdate decodeCraftUpdate(std::span<const uint8_t> body);

/** SM_CRAFT_ANIMATION (SM_CRAFT_ANIMATION.java:24-29), 11 bytes */
struct CraftAnimation {
	int32_t playerObjectId = 0;
	int32_t targetObjectId = 0;
	uint16_t skillId = 0;
	/** 0 start, 1 in progress, 2 end or cancel (CraftingTask, CraftService.sendCancelCraft) */
	uint8_t action = 0;

	bool operator==(const CraftAnimation&) const = default;
};

CraftAnimation decodeCraftAnimation(std::span<const uint8_t> body);

/** SM_LEARN_RECIPE (SM_LEARN_RECIPE.java:18-21): writeD(recipeId), then writeC(0) "4.0", verified */
int32_t decodeLearnRecipe(std::span<const uint8_t> body);

/** SM_RECIPE_DELETE (SM_RECIPE_DELETE.java:18-20): writeD(recipeId) */
int32_t decodeRecipeDelete(std::span<const uint8_t> body);

/**
 * SM_RECIPE_LIST (SM_RECIPE_LIST.java:20-26): writeH(size), then per recipe writeD(id), writeC(0) (verified). The order is the Set's iteration
 * order, which the gate must not assert (m5c-plan.md §7: a HashSet in Java)
 */
std::vector<int32_t> decodeRecipeList(std::span<const uint8_t> body);

} // namespace aion::gameserver::scenario::decoders
