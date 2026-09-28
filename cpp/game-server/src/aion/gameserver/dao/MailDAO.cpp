#include "aion/gameserver/dao/MailDAO.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "aion/commons/database/DB.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dao/InventoryDAO.h"
#include "aion/gameserver/dao/ItemStoneListDAO.h"
#include "aion/gameserver/dao/detail/UsedIds.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Letter.h"
#include "aion/gameserver/model/gameobjects/LetterTypeInfo.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/StorageType.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::DB;
using commons::database::PreparedStatement;
using commons::database::ResultSet;
using model::gameobjects::Item;
using model::gameobjects::Letter;
using model::gameobjects::Persistable;
using model::gameobjects::player::Mailbox;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.MailDAO");

namespace {

/**
 * The Java body of loadPlayerMailbox. Mailbox is an OwnedPart (Player::setMailbox takes a std::unique_ptr), so the result is a
 * std::unique_ptr: a Ref cannot hold a part (header request dao-1).
 */
std::unique_ptr<Mailbox> loadPlayerMailboxPart(model::gameobjects::player::Player& player) {
	auto mailbox = std::make_unique<Mailbox>(player);
	// Java: Map<Letter, Integer> letters = new HashMap<>() (AionObject equality is the object id, the primary key of the rows, so every row is its
	// own key; the iteration order does not matter, the mailbox keys the letters by id)
	std::vector<std::pair<runtime::Ref<Letter>, int32_t>> letters;
	std::optional<std::vector<runtime::Ref<Item>>> mailboxItems;

	DB::select(
		"SELECT * FROM mail WHERE mail_recipient_id = ?", [&](PreparedStatement& stmt) { stmt.setInt(1, player.getObjectId()); },
		[&](ResultSet& rset) {
			while (rset.next()) {
				int32_t mailUniqueId = rset.getInt("mail_unique_id");
				int32_t recipientId = rset.getInt("mail_recipient_id");
				std::string senderName = rset.getString("sender_name");
				std::string mailTitle = rset.getString("mail_title");
				std::string mailMessage = rset.getString("mail_message");
				bool unread = rset.getInt("unread") == 1;
				int32_t attachedItemObjId = rset.getInt("attached_item_id");
				int64_t attachedKinahCount = rset.getLong("attached_kinah_count");
				model::gameobjects::LetterType letterType = model::gameobjects::getLetterTypeById(rset.getInt("express"));
				std::optional<commons::database::Timestamp> receivedTime = rset.getTimestamp("recieved_time");
				letters.emplace_back(Letter::create(mailUniqueId, recipientId, nullptr, attachedKinahCount, mailTitle, mailMessage, senderName,
										 receivedTime, unread, letterType),
					attachedItemObjId);
			}
		});

	for (const auto& [letter, attachedItemObjId] : letters) {
		if (attachedItemObjId > 0) {
			if (!mailboxItems) { // lazy initialization to minimize DB io
				mailboxItems = InventoryDAO::loadItems(player.getObjectId(), model::items::storage::StorageType::MAILBOX);
				ItemStoneListDAO::load(std::vector<runtime::Ptr<Item>>(mailboxItems->begin(), mailboxItems->end()));
			}
			for (const runtime::Ref<Item>& item : *mailboxItems)
				if (item->getObjectId() == attachedItemObjId)
					letter->setAttachedItem(item);
		}
		letter->setPersistentState(Persistable::PersistentState::UPDATED);
		mailbox->putLetterToMailbox(*letter);
	}

	return mailbox;
}

} // namespace

std::unique_ptr<model::gameobjects::player::Mailbox> MailDAO::loadPlayerMailbox(model::gameobjects::player::Player& player) {
	return loadPlayerMailboxPart(player);
}

bool MailDAO::haveUnread(int32_t playerId) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement("SELECT * FROM mail WHERE mail_recipient_id = ? ORDER BY recieved_time");
		stmt->setInt(1, playerId);
		auto rset = stmt->executeQuery();
		while (rset->next()) {
			int32_t unread = rset->getInt("unread");
			if (unread == 1) {
				return true;
			}
		}
	} catch (const std::exception& e) {
		log.error("Could not read mail for player: " + std::to_string(playerId) + " from DB: " + e.what(), e);
	}
	return false;
}

void MailDAO::storeMailbox(model::gameobjects::player::Player& player) {
	runtime::Ptr<Mailbox> mailbox = player.getMailbox();
	if (!mailbox)
		return;
	std::vector<runtime::Ptr<Letter>> letters = mailbox->getLetters();
	for (const runtime::Ptr<Letter>& letter : letters) {
		storeLetter(*letter);
	}
}

bool MailDAO::storeLetter(model::gameobjects::Letter& letter) {
	bool result = false;
	switch (letter.getPersistentState()) {
		case Persistable::PersistentState::NEW:
			result = saveLetter(letter);
			break;
		case Persistable::PersistentState::UPDATE_REQUIRED:
			result = updateLetter(letter);
			break;
		default:
			break;
	}
	letter.setPersistentState(Persistable::PersistentState::UPDATED);

	return result;
}

bool MailDAO::saveLetter(model::gameobjects::Letter& letter) {
	int32_t attachedItemId = 0;
	if (letter.getAttachedItem())
		attachedItemId = letter.getAttachedItem()->getObjectId();

	const int32_t fAttachedItemId = attachedItemId;

	return DB::insertUpdate(
		"INSERT INTO `mail` (`mail_unique_id`, `mail_recipient_id`, `sender_name`, `mail_title`, `mail_message`, `unread`, `attached_item_id`, `attached_kinah_count`, `express`, `recieved_time`) VALUES(?,?,?,?,?,?,?,?,?,?)",
		[&](PreparedStatement& stmt) {
			stmt.setInt(1, letter.getObjectId());
			stmt.setInt(2, letter.getRecipientId());
			stmt.setString(3, letter.getSenderName());
			stmt.setString(4, letter.getTitle());
			stmt.setString(5, letter.getMessage());
			stmt.setBoolean(6, letter.isUnread());
			stmt.setInt(7, fAttachedItemId);
			stmt.setLong(8, letter.getAttachedKinah());
			stmt.setInt(9, model::gameobjects::getId(letter.getLetterType()));
			stmt.setTimestamp(10, letter.getTimeStamp());
			stmt.execute();
		});
}

bool MailDAO::updateLetter(model::gameobjects::Letter& letter) {
	int32_t attachedItemId = 0;
	if (letter.getAttachedItem())
		attachedItemId = letter.getAttachedItem()->getObjectId();

	const int32_t fAttachedItemId = attachedItemId;

	return DB::insertUpdate(
		"UPDATE mail SET  unread=?, attached_item_id=?, attached_kinah_count=?, `express`=?, recieved_time=? WHERE mail_unique_id=?",
		[&](PreparedStatement& stmt) {
			stmt.setBoolean(1, letter.isUnread());
			stmt.setInt(2, fAttachedItemId);
			stmt.setLong(3, letter.getAttachedKinah());
			stmt.setInt(4, model::gameobjects::getId(letter.getLetterType()));
			stmt.setTimestamp(5, letter.getTimeStamp());
			stmt.setInt(6, letter.getObjectId());
			stmt.execute();
		});
}

bool MailDAO::deleteLetter(int32_t letterId) {
	return DB::insertUpdate("DELETE FROM mail WHERE mail_unique_id=?", [&](PreparedStatement& stmt) {
		stmt.setInt(1, letterId);
		stmt.execute();
	});
}

void MailDAO::updateOfflineMailCounter(model::gameobjects::player::PlayerCommonData& recipientCommonData) {
	DB::insertUpdate("UPDATE players SET mailbox_letters=? WHERE name=?", [&](PreparedStatement& stmt) {
		stmt.setInt(1, recipientCommonData.getMailboxLetters());
		stmt.setString(2, recipientCommonData.getName());
		stmt.execute();
	});
}

std::vector<int32_t> MailDAO::getUsedIDs() {
	return detail::getUsedIDs(log, "SELECT mail_unique_id FROM mail", "mail_unique_id", "Can't get list of IDs from mail table");
}

bool MailDAO::cleanMail(std::string_view recipient) {
	return DB::insertUpdate(
		"DELETE FROM mail WHERE mail_recipient_id=(SELECT id FROM players WHERE name=?) AND attached_item_id=0 AND attached_kinah_count=0",
		[&](PreparedStatement& stmt) {
			stmt.setString(1, recipient);
			stmt.execute();
		});
}

} // namespace aion::gameserver::dao
