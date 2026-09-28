// SystemMailService (m5c-plan.md M-01, M-03): sendMail's refusals (each named by its SYSMAIL_LOG line), the letter and its new item for an
// offline recipient (MailDAO.storeLetter, then InventoryDAO.store, then MailDAO.updateOfflineMailCounter) and updateRecipientMailbox for an
// online one. The fixture is MailTestSupport.h's.

#include "MailTestSupport.h"

#include <cstdint>
#include <string>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/mail/SystemMailService.h"

namespace aion::gameserver::economy::test::mail {
namespace {

using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using services::mail::SystemMailService;

const int32_t MAILBOX_LOCATION = model::items::storage::getId(StorageType::MAILBOX);

class SystemMailServiceTest : public MailTest {};

// SystemMailService.java:41-50: an item needs a positive count (refused silently) and a template (refused with a warning)
TEST_F(SystemMailServiceTest, AnItemWithoutACountOrATemplateIsRefused) {
	MAIL_REQUIRE_DATABASE();
	LogCapture log("SYSMAIL_LOG");
	EXPECT_FALSE(SystemMailService::sendMail("Beyond Aion", B_NAME, "T", "M", MINOR_LIFE_POTION, 0, 0, LetterType::NORMAL));
	EXPECT_FALSE(SystemMailService::sendMail("Beyond Aion", B_NAME, "T", "M", MINOR_LIFE_POTION, -1, 0, LetterType::NORMAL));
	EXPECT_TRUE(log.lines().empty());
	EXPECT_FALSE(SystemMailService::sendMail("Beyond Aion", B_NAME, "T", "M", 999999999, 1, 7, LetterType::NORMAL));
	EXPECT_EQ(log.lines(), (std::vector<std::string>{"[SYSMAILSERVICE] > [SenderName: Beyond Aion] [RecipientName: Partner] RETURN ITEM ID:999999999"
													" ITEM COUNT 1 KINAH COUNT 7 ITEM TEMPLATE IS MISSING "}));
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM mail"), 0);

	EXPECT_TRUE(SystemMailService::sendMail("Beyond Aion", B_NAME, "T", "M", MINOR_LIFE_POTION, 1, 0, LetterType::NORMAL));
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM mail"), 1);
}

// SystemMailService.java:52-62: the names count UTF-16 units; a sender that starts with "$$" (a system sender) may be longer than 16
TEST_F(SystemMailServiceTest, TooLongNamesAreRefused) {
	LogCapture log("SYSMAIL_LOG");
	EXPECT_FALSE(SystemMailService::sendMail("Beyond Aion", "Abcdefghijklmnopq", "T", "M", 0, 0, 0, LetterType::NORMAL));
	EXPECT_FALSE(SystemMailService::sendMail("Abcdefghijklmnopq", B_NAME, "T", "M", 0, 0, 0, LetterType::NORMAL));
	EXPECT_EQ(log.lines(), (std::vector<std::string>{
		"[SYSMAILSERVICE] > [SenderName: Beyond Aion] [RecipientName: Abcdefghijklmnopq] ITEM RETURN0 ITEM COUNT 0 KINAH COUNT 0 RECIPIENT NAME LENGTH > 16 ",
		"[SYSMAILSERVICE] > [SenderName: Abcdefghijklmnopq] [RecipientName: Partner] ITEM RETURN0 ITEM COUNT 0 KINAH COUNT 0 SENDER NAME LENGTH > 16 "}));

	// past both checks: without a database the recipient is unknown (PlayerDAO finds no row), which the next line says
	const std::string sixteen = "\xC3\x84\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4";
	EXPECT_FALSE(SystemMailService::sendMail("$$FACTION_PACK_MAIL_SENDER", sixteen, "T", "M", 0, 0, 0, LetterType::NORMAL));
	std::vector<std::string> lines = log.lines();
	ASSERT_EQ(lines.size(), 3u);
	EXPECT_EQ(lines[2], "[SYSMAILSERVICE] > [RecipientName: " + sixteen + "] NO SUCH CHARACTER NAME.");

	// the sender's length counts UTF-16 units too: 16 two-byte letters (32 UTF-8 bytes) are not too long
	EXPECT_FALSE(SystemMailService::sendMail(sixteen, "Nobody", "T", "M", 0, 0, 0, LetterType::NORMAL));
	lines = log.lines();
	ASSERT_EQ(lines.size(), 4u);
	EXPECT_EQ(lines[3], "[SYSMAILSERVICE] > [RecipientName: Nobody] NO SUCH CHARACTER NAME.");
}

// SystemMailService.java:64-68: title.substring(0, 20) and message.substring(0, 1000) in UTF-16 units, as in MailService
TEST_F(SystemMailServiceTest, TheTitleAndTheMessageAreCutToJavasLengths) {
	MAIL_REQUIRE_DATABASE();
	goOnline(partner());
	std::string title;
	for (int i = 0; i < 21; i++)
		title += "\xC3\xA4"; // 21 x U+00E4
	std::string message(999, 'm');
	message += "\xC3\xA4\xC3\xA4"; // 1001 units

	EXPECT_TRUE(SystemMailService::sendMail("Beyond Aion", B_NAME, title, message, 0, 0, 1, LetterType::NORMAL));
	std::vector<runtime::Ptr<Letter>> letters = partner().getMailbox()->getLetters();
	ASSERT_EQ(letters.size(), 1u);
	EXPECT_EQ(letters[0]->getTitle(), title.substr(0, 40)); // 20 x U+00E4
	EXPECT_EQ(letters[0]->getMessage(), message.substr(0, 1001)); // 999 'm' + one U+00E4
	EXPECT_EQ(queryString("SELECT mail_title FROM mail"), title.substr(0, 40));
	EXPECT_EQ(queryString("SELECT mail_message FROM mail"), message.substr(0, 1001));
}

// SystemMailService.java:77-81: more than 199 letters (the mail's 100 + the reserve)
TEST_F(SystemMailServiceTest, AMailboxOfTwoHundredLettersIsFull) {
	goOnline(partner());
	LogCapture log("SYSMAIL_LOG");
	b.commonData->setMailboxLetters(200);
	EXPECT_FALSE(SystemMailService::sendMail("Beyond Aion", B_NAME, "T", "M", MINOR_LIFE_POTION, 2, 3, LetterType::NORMAL));
	EXPECT_EQ(log.lines(), (std::vector<std::string>{
		"[SYSMAILSERVICE] > [SenderName: Beyond Aion] [RecipientName: Partner] ITEM RETURN162000002 ITEM COUNT 2 KINAH COUNT 3 MAILBOX FULL "}));

	// 199 passes; without a database the letter's store fails and ends the send, silently: no mailbox update
	b.commonData->setMailboxLetters(199);
	EXPECT_FALSE(SystemMailService::sendMail("Beyond Aion", B_NAME, "T", "M", 0, 0, 3, LetterType::NORMAL));
	EXPECT_EQ(log.lines().size(), 1u);
	EXPECT_TRUE(sentB().empty());
	EXPECT_TRUE(partner().getMailbox()->getLetters().empty());
	EXPECT_EQ(b.commonData->getMailboxLetters(), 199);
}

TEST_F(SystemMailServiceTest, AnUnknownRecipientIsRefused) {
	MAIL_REQUIRE_DATABASE();
	LogCapture log("SYSMAIL_LOG");
	EXPECT_FALSE(SystemMailService::sendMail("Beyond Aion", "Nobody", "T", "M", 0, 0, 10, LetterType::NORMAL));
	EXPECT_EQ(log.lines(), (std::vector<std::string>{"[SYSMAILSERVICE] > [RecipientName: Nobody] NO SUCH CHARACTER NAME."}));
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM mail"), 0);
}

// The letter row, then the new item's row owned by the recipient in the mailbox (not equipped, slot 0), then the offline counter
TEST_F(SystemMailServiceTest, AnOfflineRecipientGetsTheLetterAndANewItem) {
	MAIL_REQUIRE_DATABASE();
	EXPECT_TRUE(SystemMailService::sendMail("$$FACTION_PACK", B_NAME, "Title", "Message", MINOR_LIFE_POTION, 3, 50, LetterType::NORMAL));

	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM mail WHERE mail_recipient_id = 710202"), 1);
	EXPECT_EQ(queryString("SELECT sender_name FROM mail"), "$$FACTION_PACK");
	EXPECT_EQ(queryString("SELECT mail_title FROM mail"), "Title");
	EXPECT_EQ(queryString("SELECT mail_message FROM mail"), "Message");
	EXPECT_EQ(queryLong("SELECT attached_kinah_count FROM mail"), 50);
	EXPECT_EQ(queryLong("SELECT unread FROM mail"), 1);
	EXPECT_EQ(queryLong("SELECT express FROM mail"), 0);
	std::optional<int64_t> itemId = queryLong("SELECT attached_item_id FROM mail");
	ASSERT_TRUE(itemId);
	std::string row = " FROM inventory WHERE item_unique_id = " + std::to_string(*itemId);
	EXPECT_EQ(queryLong("SELECT item_id" + row), MINOR_LIFE_POTION);
	EXPECT_EQ(queryLong("SELECT item_count" + row), 3);
	EXPECT_EQ(queryLong("SELECT item_owner" + row), B_ID);
	EXPECT_EQ(queryLong("SELECT item_location" + row), MAILBOX_LOCATION);
	EXPECT_EQ(queryLong("SELECT is_equipped" + row), 0);
	EXPECT_EQ(queryLong("SELECT slot" + row), 0);
	EXPECT_EQ(queryLong("SELECT mailbox_letters FROM players WHERE id = 710202"), 1);
}

// SystemMailService.java:95-96: only a positive kinah count is attached; ItemFactory.newItem cuts a count above the maximum stack
TEST_F(SystemMailServiceTest, NegativeKinahIsNotAttachedAndAStackIsCut) {
	MAIL_REQUIRE_DATABASE();
	EXPECT_TRUE(SystemMailService::sendMail("Beyond Aion", B_NAME, "T", "M", MINOR_LIFE_POTION, 5000, -5, LetterType::NORMAL));
	EXPECT_EQ(queryLong("SELECT attached_kinah_count FROM mail"), 0);
	std::optional<int64_t> itemId = queryLong("SELECT attached_item_id FROM mail");
	ASSERT_TRUE(itemId);
	EXPECT_EQ(queryLong("SELECT item_count FROM inventory WHERE item_unique_id = " + std::to_string(*itemId)), 1000);
}

// updateRecipientMailbox for an online recipient with a closed mailbox: the letter in memory, SM_MAIL_SERVICE(0), and for an express letter
// STR_POSTMAN_NOTIFY; the players row keeps its counter. The letter is stamped with the time of the send (SystemMailService.java:99)
TEST_F(SystemMailServiceTest, AnOnlineRecipientIsToldOfAnExpressLetter) {
	MAIL_REQUIRE_DATABASE();
	goOnline(partner());
	const int64_t before = commons::utils::currentTimeMillis();
	EXPECT_TRUE(SystemMailService::sendMail("Beyond Aion", B_NAME, "Express", "M", MINOR_LIFE_POTION, 2, 0, LetterType::EXPRESS));
	const int64_t after = commons::utils::currentTimeMillis();

	EXPECT_EQ(sentB(), cp::exactly({mailboxState(1, 1, 1, 0), serializedForB(SM_SYSTEM_MESSAGE::STR_POSTMAN_NOTIFY())}));
	std::vector<runtime::Ptr<Letter>> letters = partner().getMailbox()->getLetters();
	ASSERT_EQ(letters.size(), 1u);
	EXPECT_EQ(letters[0]->getSenderName(), "Beyond Aion");
	EXPECT_EQ(letters[0]->getLetterType(), LetterType::EXPRESS);
	ASSERT_TRUE(letters[0]->getTimeStamp());
	EXPECT_GE(letters[0]->getTimeStamp()->time_since_epoch().count(), before);
	EXPECT_LE(letters[0]->getTimeStamp()->time_since_epoch().count(), after);
	ASSERT_TRUE(letters[0]->getAttachedItem());
	EXPECT_EQ(letters[0]->getAttachedItem()->getItemId(), MINOR_LIFE_POTION);
	EXPECT_EQ(letters[0]->getAttachedItem()->getItemCount(), 2);
	EXPECT_EQ(b.commonData->getMailboxLetters(), 1);
	EXPECT_EQ(queryLong("SELECT mailbox_letters FROM players WHERE id = 710202"), 0);
	EXPECT_EQ(queryLong("SELECT express FROM mail"), 1);

	// a normal letter: no postman
	clearSentB();
	EXPECT_TRUE(SystemMailService::sendMail("Beyond Aion", B_NAME, "Normal", "M", 0, 0, 5, LetterType::NORMAL));
	EXPECT_EQ(sentB(), cp::exactly({mailboxState(2, 2, 1, 0)}));
}

// An online recipient whose mailbox is not loaded yet (before MailService.onPlayerLogin sets it) is left alone
// (`else if (recipient.getMailbox() != null)`, SystemMailService.java:121): the letter is stored, and his counter, packets and players row stay
// as they are; onPlayerLogin loads the letter from its row
TEST_F(SystemMailServiceTest, AnOnlineRecipientWithoutAMailboxIsLeftAlone) {
	MAIL_REQUIRE_DATABASE();
	goOnline(partner());
	partner().setMailbox(nullptr);
	EXPECT_TRUE(SystemMailService::sendMail("Beyond Aion", B_NAME, "T", "M", 0, 0, 5, LetterType::EXPRESS));

	EXPECT_TRUE(sentB().empty());
	EXPECT_EQ(b.commonData->getMailboxLetters(), 0);
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM mail WHERE mail_recipient_id = 710202"), 1);
	EXPECT_EQ(queryLong("SELECT mailbox_letters FROM players WHERE id = 710202"), 0);
}

} // namespace
} // namespace aion::gameserver::economy::test::mail
