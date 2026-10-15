// M5c K-01 (m5c-plan.md §5, P5-16): the mailbox packets of the L-Z range - CM_SEND_MAIL (C_MAIL_WRITE) and CM_READ_MAIL (C_MAIL_READ).
// CM_CHECK_MAIL_LIST, CM_GET_MAIL_ATTACHMENT and CM_DELETE_MAIL are P5-15's (tests/cm_ak/MailPacketsTest.cpp).
//
// Java: CM_SEND_MAIL.java:29-43, CM_READ_MAIL.java:22-30. The read cases lay each body out from the Java readImpl (three UTF-16 strings, the
// attached item's id and count, the kinah and the letter type as an unsigned byte). The run cases (../cm_ak/MailPacketTestSupport.h) drive each
// runImpl into the mail lane's MailService (P5-09c) and observe what it does with the fields the packet hands over: CM_SEND_MAIL's
// `LetterType.getLetterTypeById` argument, which throws for a type the enum does not have before sendMail is called (Java evaluates the arguments
// first); the recipient, item, count, kinah and letter type through what the sender pays (without a database, MailService.java:56-156), and the
// title and message through the letter the online recipient receives (with the database, :157-169 and SystemMailService.updateRecipientMailbox);
// CM_READ_MAIL's letter through the letter MailService.readMail (:204-213) sends and marks read.

#include "../cm_ak/MailPacketTestSupport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/network/aion/clientpackets/CM_READ_MAIL.h"
#include "aion/gameserver/network/aion/clientpackets/CM_SEND_MAIL.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

std::unique_ptr<AionClientPacket> CM_SEND_MAIL_clientPacketFactory(int32_t opcode, const StateSet& validStates);
std::unique_ptr<AionClientPacket> CM_READ_MAIL_clientPacketFactory(int32_t opcode, const StateSet& validStates);

/** The friends the two headers declare: the fields readImpl decoded, which Java keeps private */
struct CM_SEND_MAILTestAccess {
	static std::string recipientName(const CM_SEND_MAIL& p) { return p.recipientName; }
	static std::string title(const CM_SEND_MAIL& p) { return p.title; }
	static std::string message(const CM_SEND_MAIL& p) { return p.message; }
	static int32_t itemObjId(const CM_SEND_MAIL& p) { return p.itemObjId; }
	static int64_t itemCount(const CM_SEND_MAIL& p) { return p.itemCount; }
	static int64_t kinahCount(const CM_SEND_MAIL& p) { return p.kinahCount; }
	static int32_t idLetterType(const CM_SEND_MAIL& p) { return p.idLetterType; }
};

struct CM_READ_MAILTestAccess {
	static int32_t mailObjId(const CM_READ_MAIL& p) { return p.mailObjId; }
};

namespace testing::items {
namespace {

using network::test::LogCapture;
using Send = CM_SEND_MAILTestAccess;
using serverpackets::SM_SYSTEM_MESSAGE;

/** the decoded opcodes of ClientPacketInfo.gen.inc (Java AionClientPacketFactory.java:160, :162; State.IN_GAME) */
constexpr int32_t CM_SEND_MAIL_OPCODE = 132;
constexpr int32_t CM_READ_MAIL_OPCODE = 134;

/** C_MAIL_WRITE: S recipient, S title, S message, D item, Q item count, Q kinah, C letter type (CM_SEND_MAIL.java:29-37) */
std::vector<uint8_t> sendBody(std::string_view recipient, std::string_view title, std::string_view text, int32_t itemObjId, int64_t itemCount,
	int64_t kinah, int32_t letterType) {
	return PacketWriter().S(recipient).S(title).S(text).D(itemObjId).Q(itemCount).Q(kinah).C(letterType).data;
}

TEST(MailPacketsLzReadTest, SendReadsThreeStringsTheItemTheKinahAndAnUnsignedLetterType) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	int32_t unread = -1;
	auto p = readAlone<CM_SEND_MAIL>(CM_SEND_MAIL_OPCODE,
		sendBody("Partner", "Tides", "Five potions and 200 kinah, \xC3\xA9t\xC3\xA9", 0x0A0B0C0D, 0x100000005LL, 0x7000000000000200LL, 0xFE), unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(Send::recipientName(*p), "Partner");
	EXPECT_EQ(Send::title(*p), "Tides");
	EXPECT_EQ(Send::message(*p), "Five potions and 200 kinah, \xC3\xA9t\xC3\xA9");
	EXPECT_EQ(Send::itemObjId(*p), 0x0A0B0C0D);
	EXPECT_EQ(Send::itemCount(*p), 0x100000005LL);
	EXPECT_EQ(Send::kinahCount(*p), 0x7000000000000200LL);
	EXPECT_EQ(Send::idLetterType(*p), 254) << "readUC";
	EXPECT_EQ(unread, 0);
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
}

TEST(MailPacketsLzReadTest, ReadReadsTheLetter) {
	int32_t unread = -1;
	auto p = readAlone<CM_READ_MAIL>(CM_READ_MAIL_OPCODE, PacketWriter().D(-0x0A0B0C0D).data, unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(CM_READ_MAILTestAccess::mailObjId(*p), -0x0A0B0C0D);
	EXPECT_EQ(unread, 0);
}

TEST(MailPacketsLzReadTest, TheMarkersRegisterTheClassesUnderTheirJavaOpcodes) {
	const StateSet inGame{AionConnection_State::IN_GAME};
	EXPECT_NE(dynamic_cast<CM_SEND_MAIL*>(CM_SEND_MAIL_clientPacketFactory(CM_SEND_MAIL_OPCODE, inGame).get()), nullptr);
	EXPECT_NE(dynamic_cast<CM_READ_MAIL*>(CM_READ_MAIL_clientPacketFactory(CM_READ_MAIL_OPCODE, inGame).get()), nullptr);
	EXPECT_EQ(economyTableEntries("CM_SEND_MAIL", CM_SEND_MAIL_OPCODE), 1);
	EXPECT_EQ(economyTableEntries("CM_READ_MAIL", CM_READ_MAIL_OPCODE), 1);
}

class MailPacketsLzRunTest : public MailPacketTest {
protected:
	static constexpr int32_t POTIONS = 500001;    // the holder's stack of Minor Life Potions (COMMON, price 250)
	static constexpr int32_t KINAH_ITEM = 500002; // the holder's kinah
};

// CM_SEND_MAIL.java:42: LetterType.getLetterTypeById throws for 3 (NORMAL 0, EXPRESS 1, BLACKCLOUD 2) before MailService.sendMail runs - here on
// a connection without a player, so a sendMail that ran first would throw NullPointerException instead
TEST_F(MailPacketsLzRunTest, AnUnknownLetterTypeThrowsBeforeTheLetterIsSent) {
	TestClient loggedOut;
	EconomyDriver<CM_SEND_MAIL> packet(CM_SEND_MAIL_OPCODE);
	ASSERT_TRUE(packet.readOn(sendBody("Partner", "t", "m", 0, 0, 10, 3), loggedOut.get()));

	try {
		packet.runNow();
		ADD_FAILURE() << "no exception";
	} catch (const runtime::IllegalArgumentException& e) {
		EXPECT_EQ(std::string(e.what()), "Unsupported revive type: 3") << "Java's message (LetterType.java)";
	}
}

// The named letter is sent in full and marked read (MailService.java:204-213: SM_MAIL_SERVICE(player, letter, time), then setReadLetter); a letter
// the mailbox does not hold is logged with the reader's object id and sends nothing
TEST_F(MailPacketsLzRunTest, ReadSendsTheNamedLetterAndMarksItRead) {
	runtime::Ref<Letter> letter = putLetter(player(), 800001, nullptr, 300, "Hello", 1'700'000'123'456, true, LetterType::NORMAL, "Body text");
	runtime::Ref<Letter> other = putLetter(player(), 800002, nullptr, 0, "Other", 1'700'000'000'000, true, LetterType::EXPRESS);
	LogCapture mailLog({"MAIL_LOG"});

	readAndRun<CM_READ_MAIL>(CM_READ_MAIL_OPCODE, PacketWriter().D(800009).data);
	EXPECT_TRUE(sent().empty());
	EXPECT_TRUE(mailLog.contains("Cannot read mail 710101 800009")) << mailLog.dump();

	readAndRun<CM_READ_MAIL>(CM_READ_MAIL_OPCODE, PacketWriter().D(800001).data);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(opcodesOf(packets), (std::vector<int32_t>{SM_MAIL_SERVICE_OPCODE}));
	// SM_MAIL_SERVICE.java writeLetterRead
	PacketReader reader(bodyOf(packets[0]));
	EXPECT_EQ(reader.C(), 3);
	EXPECT_EQ(reader.D(), HOLDER_ID); // the recipient
	reader.D();                       // total + unread * 0x10000: the counts when the packet is written (MailService's order, not the packet's)
	EXPECT_EQ(reader.D(), 1);         // unread express + black cloud: the other letter
	EXPECT_EQ(reader.D(), 800001);
	EXPECT_EQ(reader.D(), HOLDER_ID);
	EXPECT_EQ(reader.S(), "Sender");
	EXPECT_EQ(reader.S(), "Hello");
	EXPECT_EQ(reader.S(), "Body text");
	EXPECT_EQ(reader.Q(), 0); // no item
	EXPECT_EQ(reader.Q(), 0);
	EXPECT_EQ(reader.D(), 0);
	EXPECT_EQ(reader.D(), 300); // the kinah
	EXPECT_EQ(reader.D(), 0);
	EXPECT_EQ(reader.C(), 0);
	EXPECT_EQ(reader.D(), 1'700'000'123); // the time stamp in seconds
	EXPECT_EQ(reader.C(), 0);             // NORMAL
	EXPECT_EQ(reader.remaining(), 0u);
	EXPECT_FALSE(letter->isUnread());
	EXPECT_TRUE(other->isUnread());
}

// The packet hands MailService.sendMail the recipient, the item, its count, the kinah and the letter type in Java's order: the sender pays
// what the oracle computes for that letter (`oracle.py m5c-economy --no-profile --set gameserver.siege.enable=false --mail 162000002:3:200`
// -> 237, `...:express` -> 1026) and the stack loses the count. Without a database the new item's store fails after the payment
// (MailService.java:149-156), so no letter is written, no SM_MAIL_SERVICE is sent and the recipient gets nothing
TEST_F(MailPacketsLzRunTest, SendChargesTheLetterOfTheNamedItemCountKinahAndType) {
	ASSERT_FALSE(commons::database::DatabaseFactory::isInitialized()) << "this case runs without a database";
	OtherPlayer& partnerB = partner();
	giveTo(player(), POTIONS, MINOR_LIFE_POTION, 10);
	giveTo(player(), KINAH_ITEM, KINAH, 5000);

	readAndRun<CM_SEND_MAIL>(CM_SEND_MAIL_OPCODE, sendBody(PARTNER_NAME, "Tides", "Five potions", POTIONS, 3, 200, 0));
	EXPECT_EQ(countOf(player(), MINOR_LIFE_POTION), 7);
	EXPECT_EQ(kinahOf(player()), 5000 - 237);
	EXPECT_TRUE(packetsOf(sent(), SM_MAIL_SERVICE_OPCODE).empty());
	EXPECT_TRUE(packetsOf(sent(), SM_SYSTEM_MESSAGE_OPCODE).empty()) << "no refusal";
	EXPECT_TRUE(partnerB.sent().empty());
	EXPECT_EQ(partnerB.player().getMailbox()->size(), 0);

	clearSent();
	readAndRun<CM_SEND_MAIL>(CM_SEND_MAIL_OPCODE, sendBody(PARTNER_NAME, "Tides", "Five potions", POTIONS, 3, 200, 1));
	EXPECT_EQ(countOf(player(), MINOR_LIFE_POTION), 4);
	EXPECT_EQ(kinahOf(player()), 5000 - 237 - 1026);
	EXPECT_TRUE(packetsOf(sent(), SM_MAIL_SERVICE_OPCODE).empty());
	EXPECT_TRUE(packetsOf(sent(), SM_SYSTEM_MESSAGE_OPCODE).empty()) << "no refusal";
	EXPECT_TRUE(partnerB.sent().empty());
}

// With the database the letter reaches the online recipient with the title and the message the packet read: the sender pays 991
// (`oracle.py ... --mail 162000002:2:200:express`) and gets SM_MAIL_SERVICE(MAIL_SEND_SUCCESS); the recipient's mailbox holds the letter and he
// gets SM_MAIL_SERVICE() and, for an express letter, STR_POSTMAN_NOTIFY (SystemMailService.updateRecipientMailbox, SystemMailService.java:116-138)
TEST_F(MailPacketsLzRunTest, SendDeliversTheLetterWithItsTitleAndMessage) {
	MAIL_PACKET_REQUIRE_DATABASE();
	OtherPlayer& partnerB = partner();
	giveTo(player(), POTIONS, MINOR_LIFE_POTION, 10);
	giveTo(player(), KINAH_ITEM, KINAH, 5000);

	readAndRun<CM_SEND_MAIL>(CM_SEND_MAIL_OPCODE, sendBody(PARTNER_NAME, "Tides", "Five potions and 200 kinah", POTIONS, 2, 200, 1));
	EXPECT_EQ(countOf(player(), MINOR_LIFE_POTION), 8);
	EXPECT_EQ(kinahOf(player()), 5000 - 991);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_FALSE(packets.empty());
	EXPECT_EQ(packets.back(), mailMessage(MAIL_SEND_SUCCESS));
	EXPECT_EQ(partnerB.sent(), exactly({mailboxState(1, 1, 1, 0), partnerB.serializedFor(SM_SYSTEM_MESSAGE::STR_POSTMAN_NOTIFY())}));

	std::vector<runtime::Ptr<Letter>> letters = partnerB.player().getMailbox()->getLetters();
	ASSERT_EQ(letters.size(), 1u);
	const Letter& letter = *letters[0];
	EXPECT_EQ(letter.getRecipientId(), PARTNER_ID);
	EXPECT_EQ(letter.getSenderName(), "Holder");
	EXPECT_EQ(letter.getTitle(), "Tides");
	EXPECT_EQ(letter.getMessage(), "Five potions and 200 kinah");
	EXPECT_EQ(letter.getAttachedKinah(), 200);
	EXPECT_EQ(letter.getLetterType(), LetterType::EXPRESS);
	EXPECT_TRUE(letter.isUnread());
	ASSERT_TRUE(letter.getAttachedItem());
	EXPECT_EQ(letter.getAttachedItem()->getItemId(), MINOR_LIFE_POTION);
	EXPECT_EQ(letter.getAttachedItem()->getItemCount(), 2);
	EXPECT_NE(letter.getAttachedItem()->getObjectId(), POTIONS) << "part of a stack is a new item";
	const std::string row = " FROM mail WHERE mail_unique_id = " + std::to_string(letter.getObjectId());
	EXPECT_EQ(queryText("SELECT mail_title" + row), "Tides");
	EXPECT_EQ(queryText("SELECT mail_message" + row), "Five potions and 200 kinah");
	EXPECT_EQ(queryText("SELECT sender_name" + row), "Holder");
	EXPECT_EQ(maildb::queryLong("SELECT mail_recipient_id" + row), PARTNER_ID);
	EXPECT_EQ(maildb::queryLong("SELECT attached_kinah_count" + row), 200);
	EXPECT_EQ(maildb::queryLong("SELECT express" + row), 1);
	EXPECT_EQ(maildb::queryLong("SELECT item_count FROM inventory WHERE item_unique_id = " + std::to_string(letter.getAttachedItem()->getObjectId())),
		2);
}

} // namespace
} // namespace testing::items
} // namespace aion::gameserver::network::aion::clientpackets
