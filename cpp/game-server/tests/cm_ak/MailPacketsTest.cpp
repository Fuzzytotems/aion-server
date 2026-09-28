// M5c K-01 (m5c-plan.md §5, P5-15): the mailbox packets of the A-K range - CM_CHECK_MAIL_LIST (C_MAIL_LIST), CM_GET_MAIL_ATTACHMENT
// (C_MAIL_GETITEM) and CM_DELETE_MAIL (C_MAIL_DELETE). CM_SEND_MAIL and CM_READ_MAIL are P5-16's (tests/cm_lz/MailPacketsTest.cpp).
//
// Java: CM_CHECK_MAIL_LIST.java:22-31, CM_GET_MAIL_ATTACHMENT.java:23-32, CM_DELETE_MAIL.java:22-34. The read cases lay each body out from the
// Java readImpl (the express flag is `readC() == 1`; the attachment type a signed byte; the letter count an unsigned short, each letter an id
// and an unknown byte). The run cases (MailPacketTestSupport.h) drive each runImpl into the mail lane's MailService (P5-09c) and observe what it
// does with the fields the packet hands over: the list request's null guard (CM_CHECK_MAIL_LIST.java:29) and its express flag
// (MailService.sendMailList(player, expressOnly, false), MailService.java:270-281: every letter newest first, or only the unread express ones,
// never the refresh packet); the attachment request's letter and type (MailService.getAttachments, :215-254: 0 takes the item of the named
// letter, 1 its kinah - stored before it is paid, so without a database the kinah is audited and never paid, with one it is paid); the
// delete request's letters in the client's order (MailService.deleteMail, :256-263).

#include "MailPacketTestSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

#include "aion/gameserver/dao/MailDAO.h"
#include "aion/gameserver/network/aion/clientpackets/CM_CHECK_MAIL_LIST.h"
#include "aion/gameserver/network/aion/clientpackets/CM_DELETE_MAIL.h"
#include "aion/gameserver/network/aion/clientpackets/CM_GET_MAIL_ATTACHMENT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_ADD_ITEM.h"
#include "aion/gameserver/services/item/ItemPacketService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

std::unique_ptr<AionClientPacket> CM_CHECK_MAIL_LIST_clientPacketFactory(int32_t opcode, const StateSet& validStates);
std::unique_ptr<AionClientPacket> CM_GET_MAIL_ATTACHMENT_clientPacketFactory(int32_t opcode, const StateSet& validStates);
std::unique_ptr<AionClientPacket> CM_DELETE_MAIL_clientPacketFactory(int32_t opcode, const StateSet& validStates);

/** The friends the two headers declare: the fields readImpl decoded, which Java keeps private */
struct CM_GET_MAIL_ATTACHMENTTestAccess {
	static int32_t mailObjId(const CM_GET_MAIL_ATTACHMENT& p) { return p.mailObjId; }
	static int8_t attachmentType(const CM_GET_MAIL_ATTACHMENT& p) { return p.attachmentType; }
};

struct CM_DELETE_MAILTestAccess {
	static std::vector<int32_t> mailObjIds(const CM_DELETE_MAIL& p) { return p.mailObjIds; }
};

namespace testing::items {
namespace {

using network::test::LogCapture;
using serverpackets::SM_INVENTORY_ADD_ITEM;
using services::item::ItemPacketService;

/** the decoded opcodes of ClientPacketInfo.gen.inc (Java AionClientPacketFactory.java:161, :164-165; State.IN_GAME) */
constexpr int32_t CM_CHECK_MAIL_LIST_OPCODE = 133;
constexpr int32_t CM_GET_MAIL_ATTACHMENT_OPCODE = 136;
constexpr int32_t CM_DELETE_MAIL_OPCODE = 137;

TEST(MailPacketsReadTest, TheListRequestIsExpressOnlyForExactlyOne) {
	int32_t unread = -1;
	auto one = readAlone<CM_CHECK_MAIL_LIST>(CM_CHECK_MAIL_LIST_OPCODE, PacketWriter().C(1).data, unread);
	ASSERT_NE(one, nullptr);
	EXPECT_TRUE(one->expressOnly);
	EXPECT_EQ(unread, 0);
	for (int32_t flag : {0, 2, 0xFF}) {
		auto other = readAlone<CM_CHECK_MAIL_LIST>(CM_CHECK_MAIL_LIST_OPCODE, PacketWriter().C(flag).data, unread);
		ASSERT_NE(other, nullptr);
		EXPECT_FALSE(other->expressOnly) << flag;
	}
}

TEST(MailPacketsReadTest, TheAttachmentRequestReadsTheLetterAndASignedByteType) {
	int32_t unread = -1;
	auto p = readAlone<CM_GET_MAIL_ATTACHMENT>(CM_GET_MAIL_ATTACHMENT_OPCODE, PacketWriter().D(0x0A0B0C0D).C(0x81).data, unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(CM_GET_MAIL_ATTACHMENTTestAccess::mailObjId(*p), 0x0A0B0C0D);
	EXPECT_EQ(CM_GET_MAIL_ATTACHMENTTestAccess::attachmentType(*p), -127) << "readC: a Java byte";
	EXPECT_EQ(unread, 0);

	p = readAlone<CM_GET_MAIL_ATTACHMENT>(CM_GET_MAIL_ATTACHMENT_OPCODE, PacketWriter().D(7).C(1).data, unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(CM_GET_MAIL_ATTACHMENTTestAccess::attachmentType(*p), 1) << "1: the kinah";
}

TEST(MailPacketsReadTest, DeleteReadsAnUnsignedCountAndPerLetterItsIdAndAnUnknownByte) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	int32_t unread = -1;
	auto p = readAlone<CM_DELETE_MAIL>(CM_DELETE_MAIL_OPCODE, PacketWriter().H(3).D(0x01020304).C(0x7F).D(-2).C(0).D(55).C(0xFF).data, unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(CM_DELETE_MAILTestAccess::mailObjIds(*p), (std::vector<int32_t>{0x01020304, -2, 55}));
	EXPECT_EQ(unread, 0);
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();

	p = readAlone<CM_DELETE_MAIL>(CM_DELETE_MAIL_OPCODE, PacketWriter().H(0).D(1).C(0).data, unread);
	ASSERT_NE(p, nullptr);
	EXPECT_TRUE(CM_DELETE_MAILTestAccess::mailObjIds(*p).empty());
	EXPECT_EQ(unread, 5) << "a count of 0 reads no letter";
}

TEST(MailPacketsReadTest, TheLetterCountIsUnsigned) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	// 0x8000 letters is 32,768 (readUH), not a negative array size
	PacketWriter body;
	body.H(0x8000);
	for (int32_t i = 1; i <= 0x8000; i++)
		body.D(i).C(0);
	int32_t unread = -1;
	auto p = readAlone<CM_DELETE_MAIL>(CM_DELETE_MAIL_OPCODE, body.data, unread);
	ASSERT_NE(p, nullptr);
	std::vector<int32_t> ids = CM_DELETE_MAILTestAccess::mailObjIds(*p);
	ASSERT_EQ(ids.size(), 32768u);
	EXPECT_EQ(ids.front(), 1);
	EXPECT_EQ(ids.back(), 0x8000);
	EXPECT_EQ(unread, 0);
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
}

TEST(MailPacketsReadTest, TheMarkersRegisterTheClassesUnderTheirJavaOpcodes) {
	const StateSet inGame{AionConnection_State::IN_GAME};
	EXPECT_NE(dynamic_cast<CM_CHECK_MAIL_LIST*>(CM_CHECK_MAIL_LIST_clientPacketFactory(CM_CHECK_MAIL_LIST_OPCODE, inGame).get()), nullptr);
	EXPECT_NE(dynamic_cast<CM_GET_MAIL_ATTACHMENT*>(CM_GET_MAIL_ATTACHMENT_clientPacketFactory(CM_GET_MAIL_ATTACHMENT_OPCODE, inGame).get()),
		nullptr);
	EXPECT_NE(dynamic_cast<CM_DELETE_MAIL*>(CM_DELETE_MAIL_clientPacketFactory(CM_DELETE_MAIL_OPCODE, inGame).get()), nullptr);
	EXPECT_EQ(economyTableEntries("CM_CHECK_MAIL_LIST", CM_CHECK_MAIL_LIST_OPCODE), 1);
	EXPECT_EQ(economyTableEntries("CM_GET_MAIL_ATTACHMENT", CM_GET_MAIL_ATTACHMENT_OPCODE), 1);
	EXPECT_EQ(economyTableEntries("CM_DELETE_MAIL", CM_DELETE_MAIL_OPCODE), 1);
}

class MailPacketsRunTest : public MailPacketTest {
protected:
	static constexpr int32_t POTIONS = 600001;       // a letter's attached stack
	static constexpr int32_t OTHER_POTIONS = 600002; // another letter's
	static constexpr int32_t KINAH_ITEM = 500002;    // the holder's kinah
	const int32_t MAILBOX_LOCATION = model::items::storage::getId(StorageType::MAILBOX);
	const int32_t CUBE_LOCATION = model::items::storage::getId(StorageType::CUBE);

	/** SM_MAIL_SERVICE(2) of the holder's letters as Java writes them (SM_MAIL_SERVICE.java writeLettersList): the last part's count negative */
	std::vector<uint8_t> letterList(int32_t signedCount, const PacketWriter& letters) {
		return javaPacket(SM_MAIL_SERVICE_OPCODE, PacketWriter().C(2).D(HOLDER_ID).C(0).H(signedCount).B(letters.data));
	}
};

// CM_CHECK_MAIL_LIST.java:29: no active player, no MailService call (sendMailList dereferences the player)
TEST_F(MailPacketsRunTest, TheListRequestWithoutAPlayerDoesNothing) {
	TestClient loggedOut;
	EconomyDriver<CM_CHECK_MAIL_LIST> packet(CM_CHECK_MAIL_LIST_OPCODE);
	ASSERT_TRUE(packet.readOn(PacketWriter().C(1).data, loggedOut.get()));

	EXPECT_NO_THROW(packet.runNow());
	EXPECT_TRUE((*loggedOut).sentBytes().empty());
}

// C(0) lists every letter, newest first; C(1) only the unread letters whose isExpress() is set (EXPRESS and BLACKCLOUD, Letter.java:37), newest
// first. Either way one SM_MAIL_SERVICE(2) and no refresh packet before it (the packet passes sendRefreshPacket false, CM_CHECK_MAIL_LIST.java:30)
TEST_F(MailPacketsRunTest, TheListRequestListsEveryLetterOrOnlyTheUnreadExpressOnes) {
	putLetter(player(), 800001, loadedItem(POTIONS, MINOR_LIFE_POTION, 5, StorageType::MAILBOX), 0, "Oldest", 1000);
	putLetter(player(), 800002, nullptr, 300, "Newest", 3000, true, LetterType::EXPRESS);
	putLetter(player(), 800003, nullptr, 0, "Middle", 2000, false, LetterType::EXPRESS);
	putLetter(player(), 800004, nullptr, 0, "Cloud", 1500, true, LetterType::BLACKCLOUD);

	readAndRun<CM_CHECK_MAIL_LIST>(CM_CHECK_MAIL_LIST_OPCODE, PacketWriter().C(0).data);
	PacketWriter all;
	listedLetter(all, 800002, "Sender", "Newest", false, 0, 0, 300, LetterType::EXPRESS);
	listedLetter(all, 800003, "Sender", "Middle", true, 0, 0, 0, LetterType::EXPRESS);
	listedLetter(all, 800004, "Sender", "Cloud", false, 0, 0, 0, LetterType::BLACKCLOUD);
	listedLetter(all, 800001, "Sender", "Oldest", false, POTIONS, MINOR_LIFE_POTION, 0, LetterType::NORMAL);
	EXPECT_EQ(sent(), exactly({letterList(-4, all)}));

	clearSent();
	readAndRun<CM_CHECK_MAIL_LIST>(CM_CHECK_MAIL_LIST_OPCODE, PacketWriter().C(1).data);
	PacketWriter express;
	listedLetter(express, 800002, "Sender", "Newest", false, 0, 0, 300, LetterType::EXPRESS);
	listedLetter(express, 800004, "Sender", "Cloud", false, 0, 0, 0, LetterType::BLACKCLOUD);
	EXPECT_EQ(sent(), exactly({letterList(-2, express)}));
	EXPECT_EQ(player().getMailbox()->size(), 4) << "listing reads the letters and leaves them";
}

// Type 0 takes the item of the named letter into the cube (MailService.java:222-240: Storage.add with ItemAddType.MAIL, then
// SM_MAIL_SERVICE(letter, 0)); the letter keeps its kinah, the other letter its item
TEST_F(MailPacketsRunTest, TheAttachmentRequestTakesTheItemOfTheNamedLetter) {
	runtime::Ref<Item> potions = loadedItem(POTIONS, MINOR_LIFE_POTION, 5, StorageType::MAILBOX);
	runtime::Ref<Item> otherPotions = loadedItem(OTHER_POTIONS, MINOR_LIFE_POTION, 7, StorageType::MAILBOX);
	runtime::Ref<Letter> letter = putLetter(player(), 800001, potions, 300, "Potions", 1000);
	runtime::Ref<Letter> other = putLetter(player(), 800002, otherPotions, 0, "More", 2000);

	readAndRun<CM_GET_MAIL_ATTACHMENT>(CM_GET_MAIL_ATTACHMENT_OPCODE, PacketWriter().D(800001).C(0).data);
	// ItemPacketService.sendStorageUpdatePacket (ItemPacketService.java:214-228): the item, then the cube's size (the potions alone)
	EXPECT_EQ(sent(), exactly({serializedFor(SM_INVENTORY_ADD_ITEM({runtime::Ptr<Item>(potions)}, player(), ItemPacketService::ItemAddType::MAIL)),
						  cubeSize(StorageType::CUBE, 1), letterState(800001, 0)}));
	EXPECT_EQ(countOf(player(), MINOR_LIFE_POTION), 5);
	EXPECT_EQ(player().getInventory().getItemByObjId(POTIONS).get(), potions.get());
	EXPECT_EQ(potions->getItemLocation(), CUBE_LOCATION);
	EXPECT_FALSE(letter->getAttachedItem());
	EXPECT_EQ(letter->getAttachedKinah(), 300) << "type 0 leaves the kinah";
	EXPECT_EQ(other->getAttachedItem().get(), otherPotions.get());
	EXPECT_EQ(otherPotions->getItemLocation(), MAILBOX_LOCATION);

	// the item is gone from the letter: a second request finds none (MailService.java:223-225) and sends nothing
	clearSent();
	readAndRun<CM_GET_MAIL_ATTACHMENT>(CM_GET_MAIL_ATTACHMENT_OPCODE, PacketWriter().D(800001).C(0).data);
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(countOf(player(), MINOR_LIFE_POTION), 5);
}

// Type 1 asks for the kinah of the named letter, which is taken off the letter and stored before it is paid (MailService.java:241-252). Without a
// database the store fails: the attempt is audited with the letter's kinah and nothing is paid or sent; the item stays in the letter
TEST_F(MailPacketsRunTest, TheKinahRequestWithoutADatabaseIsAuditedAndPaysNothing) {
	ASSERT_FALSE(commons::database::DatabaseFactory::isInitialized()) << "this case runs without a database";
	giveTo(player(), KINAH_ITEM, KINAH, 50);
	runtime::Ref<Item> potions = loadedItem(POTIONS, MINOR_LIFE_POTION, 5, StorageType::MAILBOX);
	runtime::Ref<Letter> letter = putLetter(player(), 800001, potions, 300, "Kinah", 1000);
	LogCapture audit({AUDIT_LOGGER});

	readAndRun<CM_GET_MAIL_ATTACHMENT>(CM_GET_MAIL_ATTACHMENT_OPCODE, PacketWriter().D(800001).C(1).data);
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(kinahOf(player()), 50);
	EXPECT_EQ(letter->getAttachedKinah(), 0);
	EXPECT_EQ(letter->getAttachedItem().get(), potions.get()) << "type 1 leaves the item";
	EXPECT_EQ(countOf(player(), MINOR_LIFE_POTION), 0);
	// "Location: " + player.getPosition() (WorldPosition.java toString: the holder's spawned position)
	EXPECT_TRUE(audit.contains("tried to use kinah mail exploit. Location: WorldPosition [mapId=210010000, x=100.0, y=100.0, z=50.0, heading=0, "
							   "isSpawned=true], kinah count: 300"))
		<< audit.dump();
}

// With the database the stored letter pays its kinah: the letter's row loses the kinah first, then the kinah is added and SM_MAIL_SERVICE(letter,
// 1) sent (MailService.java:241-252); the item stays in the letter
TEST_F(MailPacketsRunTest, TheKinahRequestPaysTheKinahOfTheNamedLetter) {
	MAIL_PACKET_REQUIRE_DATABASE();
	giveTo(player(), KINAH_ITEM, KINAH, 50);
	runtime::Ref<Item> potions = loadedItem(POTIONS, MINOR_LIFE_POTION, 5, StorageType::MAILBOX);
	runtime::Ref<Letter> letter = Letter::create(800001, HOLDER_ID, potions, 300, "Kinah", "", "Sender",
		commons::database::Timestamp(std::chrono::milliseconds(1'700'000'000'000)), true, LetterType::NORMAL);
	ASSERT_TRUE(dao::MailDAO::storeLetter(*letter)); // the row, as MailService.sendMail stored it
	player().getMailbox()->putLetterToMailbox(*letter);

	readAndRun<CM_GET_MAIL_ATTACHMENT>(CM_GET_MAIL_ATTACHMENT_OPCODE, PacketWriter().D(800001).C(1).data);
	EXPECT_EQ(kinahOf(player()), 350);
	EXPECT_EQ(letter->getAttachedKinah(), 0);
	EXPECT_EQ(letter->getAttachedItem().get(), potions.get()) << "type 1 leaves the item";
	EXPECT_EQ(countOf(player(), MINOR_LIFE_POTION), 0);
	EXPECT_EQ(maildb::queryLong("SELECT attached_kinah_count FROM mail WHERE mail_unique_id = 800001"), 0);
	EXPECT_EQ(maildb::queryLong("SELECT attached_item_id FROM mail WHERE mail_unique_id = 800001"), POTIONS);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_FALSE(packets.empty());
	EXPECT_EQ(packets.back(), letterState(800001, 1));
	EXPECT_EQ(packetsOf(packets, SM_MAIL_SERVICE_OPCODE), exactly({letterState(800001, 1)}));
}

// The listed letters leave the mailbox (MailService.java:256-263; the DAO delete fails quietly without a database) and SM_MAIL_SERVICE(6)
// answers with their ids in the client's order, after the mailbox counts of the moment it is written
TEST_F(MailPacketsRunTest, DeleteRemovesTheListedLettersAndAnswersWithTheirIds) {
	putLetter(player(), 800001, nullptr, 0, "First", 1000);
	putLetter(player(), 800002, nullptr, 0, "Second", 2000, true, LetterType::EXPRESS);
	putLetter(player(), 800003, nullptr, 0, "Third", 3000, false);
	f.commonData->setMailboxLetters(3);

	readAndRun<CM_DELETE_MAIL>(CM_DELETE_MAIL_OPCODE, PacketWriter().H(2).D(800003).C(0).D(800001).C(0).data);
	std::vector<runtime::Ptr<Letter>> left = player().getMailbox()->getLetters();
	ASSERT_EQ(left.size(), 1u);
	EXPECT_EQ(left[0]->getObjectId(), 800002);
	EXPECT_EQ(f.commonData->getMailboxLetters(), 1);
	// SM_MAIL_SERVICE.java writeLetterDelete: C(6), D(total + unread * 0x10000), D(unread express + black cloud), H(count), the ids
	EXPECT_EQ(sent(), exactly({javaPacket(SM_MAIL_SERVICE_OPCODE, PacketWriter().C(6).D(1 + 0x10000).D(1).H(2).D(800003).D(800001))}));
}

} // namespace
} // namespace testing::items
} // namespace aion::gameserver::network::aion::clientpackets
