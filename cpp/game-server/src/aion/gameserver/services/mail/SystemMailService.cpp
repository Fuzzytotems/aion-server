#include "aion/gameserver/services/mail/SystemMailService.h"

#include <chrono>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/dao/InventoryDAO.h"
#include "aion/gameserver/dao/MailDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Letter.h"
#include "aion/gameserver/model/gameobjects/LetterType.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MAIL_SERVICE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/item/ItemFactory.h"
#include "aion/gameserver/services/mail/MailService.h"
#include "aion/gameserver/services/player/PlayerMailboxState.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

namespace aion::gameserver::services::mail {

static const auto log = commons::logging::LoggerFactory::getLogger("SYSMAIL_LOG");

namespace {

using model::gameobjects::Item;
using model::gameobjects::Letter;
using model::gameobjects::LetterType;
using model::gameobjects::player::Mailbox;
using model::gameobjects::player::Player;
using model::gameobjects::player::PlayerCommonData;
using network::aion::serverpackets::SM_MAIL_SERVICE;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;
using runtime::Ref;
using utils::PacketSendUtility;

/** Java's log text `" ITEM COUNT " + attachedItemCount + " KINAH COUNT " + attachedKinahCount` */
std::string counts(int64_t attachedItemCount, int64_t attachedKinahCount) {
	return " ITEM COUNT " + std::to_string(attachedItemCount) + " KINAH COUNT " + std::to_string(attachedKinahCount);
}

} // namespace

bool SystemMailService::sendMail(std::string_view sender, std::string_view recipientName, std::string_view titleValue, std::string_view messageValue,
	int32_t attachedItemId, int64_t attachedItemCount, int64_t attachedKinahCount, model::gameobjects::LetterType letterType) {

	if (attachedItemId != 0) {
		if (attachedItemCount <= 0)
			return false;
		const model::templates::item::ItemTemplate* itemTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(attachedItemId);
		if (itemTemplate == nullptr) {
			log.warn("[SYSMAILSERVICE] > [SenderName: " + std::string(sender) + "] [RecipientName: " + std::string(recipientName) +
				"] RETURN ITEM ID:" + std::to_string(attachedItemId) + counts(attachedItemCount, attachedKinahCount) + " ITEM TEMPLATE IS MISSING ");
			return false;
		}
	}

	// Java String.length() and substring count UTF-16 code units (CONVENTIONS.md "Strings")
	if (commons::utils::StringUtils::utf16Length(recipientName) > 16) {
		log.warn("[SYSMAILSERVICE] > [SenderName: " + std::string(sender) + "] [RecipientName: " + std::string(recipientName) + "] ITEM RETURN" +
			std::to_string(attachedItemId) + counts(attachedItemCount, attachedKinahCount) + " RECIPIENT NAME LENGTH > 16 ");
		return false;
	}

	if (!sender.starts_with("$$") && commons::utils::StringUtils::utf16Length(sender) > 16) {
		log.warn("[SYSMAILSERVICE] > [SenderName: " + std::string(sender) + "] [RecipientName: " + std::string(recipientName) + "] ITEM RETURN" +
			std::to_string(attachedItemId) + counts(attachedItemCount, attachedKinahCount) + " SENDER NAME LENGTH > 16 ");
		return false;
	}

	std::string title(titleValue);
	if (commons::utils::StringUtils::utf16Length(title) > 20)
		title = commons::utils::StringUtils::substring(title, 0, 20);

	std::string message(messageValue);
	if (commons::utils::StringUtils::utf16Length(message) > 1000)
		message = commons::utils::StringUtils::substring(message, 0, 1000);

	Ref<PlayerCommonData> recipientCommonData = player::PlayerService::getOrLoadPlayerCommonData(recipientName);

	if (!recipientCommonData) {
		log.info("[SYSMAILSERVICE] > [RecipientName: " + std::string(recipientName) + "] NO SUCH CHARACTER NAME.");
		return false;
	}

	if (recipientCommonData->getMailboxLetters() > 199) {
		log.info("[SYSMAILSERVICE] > [SenderName: " + std::string(sender) + "] [RecipientName: " + recipientCommonData->getName() + "] ITEM RETURN" +
			std::to_string(attachedItemId) + counts(attachedItemCount, attachedKinahCount) + " MAILBOX FULL ");
		return false;
	}
	Ref<Item> attachedItem;
	int64_t finalAttachedKinahCount = 0;

	if (attachedItemId != 0) {
		Ref<Item> senderItem = item::ItemFactory::newItem(attachedItemId, attachedItemCount);
		if (senderItem) {
			senderItem->setEquipped(false);
			senderItem->setEquipmentSlot(0);
			senderItem->setItemLocation(model::items::storage::getId(model::items::storage::StorageType::MAILBOX));
			attachedItem = senderItem;
		}
	}

	if (attachedKinahCount > 0)
		finalAttachedKinahCount = attachedKinahCount;

	Ref<Letter> newLetter = Letter::create(utils::idfactory::IDFactory::getInstance().nextId(), recipientCommonData->getPlayerObjId(), attachedItem,
		finalAttachedKinahCount, title, message, sender, commons::database::Timestamp(std::chrono::milliseconds(commons::utils::currentTimeMillis())),
		true, letterType);

	if (!dao::MailDAO::storeLetter(*newLetter))
		return false;

	if (attachedItem)
		if (!dao::InventoryDAO::store(*attachedItem, recipientCommonData->getPlayerObjId()))
			return false;

	if (configs::main::LoggingConfig::LOG_SYSMAIL.load())
		log.info("[SYSMAILSERVICE] > [SenderName: " + std::string(sender) + "] [RecipientName: " + std::string(recipientName) + "] RETURN ITEM ID:" +
			std::to_string(attachedItemId) + counts(attachedItemCount, attachedKinahCount) + " MESSAGE SUCCESSFULLY SENDED ");

	updateRecipientMailbox(*recipientCommonData, *newLetter);
	return true;
}

void SystemMailService::updateRecipientMailbox(model::gameobjects::player::PlayerCommonData& recipientCommonData,
	model::gameobjects::Letter& newLetter) {
	Ptr<Player> recipient = recipientCommonData.getPlayer();
	if (!recipient) {
		recipientCommonData.setMailboxLetters(recipientCommonData.getMailboxLetters() + 1);
		dao::MailDAO::updateOfflineMailCounter(recipientCommonData);
	} else if (recipient->getMailbox()) { // Send mail update packets
		Ptr<Mailbox> mailbox = recipient->getMailbox();
		mailbox->putLetterToMailbox(newLetter);
		recipientCommonData.setMailboxLetters(mailbox->size());

		PacketSendUtility::sendPacket(*recipient, SM_MAIL_SERVICE());

		// refresh letters if recipient is currently looking into his mailbox
		int8_t mailBoxState = mailbox->mailBoxState.get();
		if (mailBoxState != 0) {
			bool isPostman = (mailBoxState & player::PlayerMailboxState::EXPRESS) == player::PlayerMailboxState::EXPRESS;
			MailService::sendMailList(*recipient, isPostman, false);
		}

		if (newLetter.getLetterType() == LetterType::EXPRESS)
			PacketSendUtility::sendPacket(*recipient, SM_SYSTEM_MESSAGE::STR_POSTMAN_NOTIFY());
		// else if (newLetter.getLetterType() == LetterType.BLACKCLOUD) PacketSendUtility.sendPacket(recipient, SM_SYSTEM_MESSAGE.STR_MAIL_CASHITEM_BUY(itemId));
	}
}

} // namespace aion::gameserver::services::mail
