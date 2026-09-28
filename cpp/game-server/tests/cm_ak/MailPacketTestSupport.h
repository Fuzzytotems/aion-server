#pragma once

// Shared fixture of the mail client packets' run cases (m5c-plan.md K-01): P5-15's tests/cm_ak/MailPacketsTest.cpp (CM_CHECK_MAIL_LIST,
// CM_GET_MAIL_ATTACHMENT, CM_DELETE_MAIL) and P5-16's tests/cm_lz/MailPacketsTest.cpp (CM_SEND_MAIL, CM_READ_MAIL), which includes this header by
// relative path as it includes EconomyPacketTestSupport.h. Each runImpl is one call into the mail lane's MailService (P5-09c); the cases drive it
// through the packet and observe what MailService does with the fields the packet hands over.
//
// - MailPacketTest is EconomyPacketTest with a Mailbox for the holder ("Holder", 710101: SM_MAIL_SERVICE's writeImpl reads the receiving
//   connection's mailbox) and, on demand, the recipient "Partner" (710202) online: in the World (PlayerService.getOrLoadPlayerCommonData and
//   MailService.validateRecipient find him there), his common data online (SystemMailService.updateRecipientMailbox's getPlayer) and with a
//   Mailbox of his own.
// - Letters are put into a mailbox as MailDAO.loadPlayerMailbox builds them (state UPDATED, MailDAO.java loadPlayerMailbox).
// - Without a database every DAO write returns false (DB.insertUpdate and InventoryDAO.store catch the SQLException), so MailService stops at
//   its first store: sendMail after the sender paid (MailService.java:153-161), getAttachments' kinah arm at storeLetter (:243-246). That is
//   Java's flow on a failed store, and it is what the cases without a database observe; they assert that no database is open first.
// - A case whose Java flow needs its store to succeed calls MAIL_PACKET_REQUIRE_DATABASE(): the economy test database of
//   tests/economy/P5-09a/EconomyTestSupport.h (aion_gs_test_economy, recreated once per process under its named lock; the case is skipped without
//   AION_TEST_GS_DATABASE_URL). The case empties the rows these cases write and inserts the players rows of the holder and the recipient (the
//   mail table's foreign key), and TearDown shuts DatabaseFactory down again, so a case that follows in one process (a whole-executable run) has no
//   database, as it has none under ctest (one process per case).
// - Like EconomyPacketTestSupport.h, it must not be mixed with tests/world/WorldTestSupport.h in one executable run (m5c-plan.md §18.7).

#include "EconomyPacketTestSupport.h"
#include "../economy/P5-09a/EconomyTestSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/gameserver/model/gameobjects/Letter.h"
#include "aion/gameserver/model/gameobjects/LetterType.h"
#include "aion/gameserver/model/gameobjects/LetterTypeInfo.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/taskmanager/tasks/ExpireTimerTask.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::items {

namespace maildb = ::aion::gameserver::economy::test;

using model::gameobjects::Letter;
using model::gameobjects::LetterType;

// ServerPacketsOpcodes.java: SM_MAIL_SERVICE
inline constexpr int32_t SM_MAIL_SERVICE_OPCODE = 161;

inline constexpr int32_t HOLDER_ID = 710101; // ItemPacketTest's player
inline constexpr int32_t HOLDER_ACCOUNT = 9901;
inline constexpr int32_t PARTNER_ID = 710202; // the recipient
inline constexpr std::string_view PARTNER_NAME = "Partner";

// MailMessage.java: the ids SM_MAIL_SERVICE(1, message) writes
inline constexpr int32_t MAIL_SEND_SUCCESS = 0;

/** SM_MAIL_SERVICE(1, message) (SM_MAIL_SERVICE.java writeMailMessage): C(1), C(message id) */
inline std::vector<uint8_t> mailMessage(int32_t messageId) {
	return javaPacket(SM_MAIL_SERVICE_OPCODE, PacketWriter().C(1).C(messageId));
}

/** SM_MAIL_SERVICE() (SM_MAIL_SERVICE.java writeMailboxState): C(0), H(total), H(unread), H(unread express), H(unread black cloud) */
inline std::vector<uint8_t> mailboxState(int32_t total, int32_t unread, int32_t express, int32_t blackCloud) {
	return javaPacket(SM_MAIL_SERVICE_OPCODE, PacketWriter().C(0).H(total).H(unread).H(express).H(blackCloud));
}

/** SM_MAIL_SERVICE(letterId, attachmentType) (SM_MAIL_SERVICE.java writeLetterState): C(5), D(letterId), C(attachmentType), C(1) */
inline std::vector<uint8_t> letterState(int32_t letterId, int32_t attachmentType) {
	return javaPacket(SM_MAIL_SERVICE_OPCODE, PacketWriter().C(5).D(letterId).C(attachmentType).C(1));
}

/** One letter of SM_MAIL_SERVICE(2)'s list (SM_MAIL_SERVICE.java writeLettersList), appended to `body` */
inline void listedLetter(PacketWriter& body, int32_t letterId, std::string_view sender, std::string_view title, bool isRead, int32_t itemObjId,
	int32_t itemId, int64_t kinah, LetterType type) {
	body.D(letterId).S(sender).S(title).C(isRead ? 1 : 0).D(itemObjId).D(itemId).Q(kinah).C(model::gameobjects::getId(type));
}

/** A Letter as MailDAO.loadPlayerMailbox builds one: state UPDATED, received at `millis` */
inline runtime::Ref<Letter> loadedLetter(int32_t letterId, int32_t recipientId, runtime::Ptr<Item> item, int64_t kinah, std::string_view title,
	std::string_view message, std::string_view sender, int64_t millis, bool unread, LetterType type) {
	runtime::Ref<Letter> letter = Letter::create(letterId, recipientId, item, kinah, title, message, sender,
		commons::database::Timestamp(std::chrono::milliseconds(millis)), unread, type);
	letter->setPersistentState(model::gameobjects::Persistable_PersistentState::UPDATED);
	return letter;
}

/** The first column of the first row of a query as text, std::nullopt for NULL or no row */
inline std::optional<std::string> queryText(std::string_view sql) {
	auto con = commons::database::DatabaseFactory::getConnection();
	auto rs = con->prepareStatement(sql)->executeQuery();
	if (!rs->next())
		return std::nullopt;
	return rs->getObject<std::string>(1);
}

class MailPacketTest : public EconomyPacketTest {
protected:
	void SetUp() override {
		EconomyPacketTest::SetUp();
		player().setMailbox(std::make_unique<model::gameobjects::player::Mailbox>(player()));
	}

	void TearDown() override {
		if (f.player)
			taskmanager::tasks::ExpireTimerTask::getInstance().unregisterExpirables(*f.player); // a taken item registers its holder
		for (OtherPlayer& other : others)
			other.f.commonData->setOnline(false);
		EconomyPacketTest::TearDown();
		if (databaseOpened)
			commons::database::DatabaseFactory::shutdown();
	}

	/** "Partner" online in the World with a Mailbox (PlayerEnterWorldService for the mail's purposes) */
	OtherPlayer& partner() {
		OtherPlayer& other = otherPlayer(PARTNER_ID, PARTNER_NAME);
		other.player().setMailbox(std::make_unique<model::gameobjects::player::Mailbox>(other.player()));
		other.f.commonData->setOnline(true);
		return other;
	}

	/** A letter put into the owner's mailbox as the DAO loads it */
	runtime::Ref<Letter> putLetter(model::gameobjects::player::Player& owner, int32_t letterId, runtime::Ptr<Item> item, int64_t kinah,
		std::string_view title, int64_t millis, bool unread = true, LetterType type = LetterType::NORMAL, std::string_view message = "",
		std::string_view sender = "Sender") {
		runtime::Ref<Letter> letter = loadedLetter(letterId, owner.getObjectId(), item, kinah, title, message, sender, millis, unread, type);
		owner.getMailbox()->putLetterToMailbox(*letter);
		return letter;
	}

	/** @return false (and the case skips) without the test database; see the header comment */
	bool requireDatabase() {
		if (!maildb::isDatabaseEnabled())
			return false;
		maildb::setUpDatabaseOnce();
		if (!commons::database::DatabaseFactory::isInitialized()) // shut down by an earlier case of this process
			commons::database::DatabaseFactory::init(maildb::urlWithDatabase(maildb::TEST_DATABASE), maildb::user(), maildb::password(), 10, 5000);
		databaseOpened = true;
		maildb::execute("DELETE FROM inventory"); // item_stones cascade
		maildb::execute("DELETE FROM players WHERE id IN (" + std::to_string(HOLDER_ID) + ", " + std::to_string(PARTNER_ID) + ")"); // mail cascades
		maildb::insertPlayer(HOLDER_ID, "Holder", HOLDER_ACCOUNT);
		maildb::insertPlayer(PARTNER_ID, PARTNER_NAME, PARTNER_ID + 1000); // otherPlayer's account id
		return true;
	}

	bool databaseOpened = false;
};

/** Skips the case without the test database (use at the start of a TEST_F body of MailPacketTest) */
#define MAIL_PACKET_REQUIRE_DATABASE()                                                                                                               \
	if (!requireDatabase())                                                                                                                            \
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL to run the database tests";

} // namespace aion::gameserver::network::aion::clientpackets::testing::items
