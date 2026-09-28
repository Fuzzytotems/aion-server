#include "aion/gameserver/services/mail/MailService.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/dao/BlockListDAO.h"
#include "aion/gameserver/dao/InventoryDAO.h"
#include "aion/gameserver/dao/ItemStoneListDAO.h"
#include "aion/gameserver/dao/MailDAO.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Letter.h"
#include "aion/gameserver/model/gameobjects/LetterType.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/player/BlockList.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/model/templates/item/ItemQuality.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/mail/MailMessage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DELETE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MAIL_SERVICE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/AdminService.h"
#include "aion/gameserver/services/item/ItemFactory.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemAddType.h"
#include "aion/gameserver/services/mail/SystemMailService.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/services/trade/PricesService.h"
#include "aion/gameserver/taskmanager/tasks/ExpireTimerTask.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/utils/collections/DynamicServerPacketBodySplitList.h"
#include "aion/gameserver/utils/collections/ListPart.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::services::mail {

static const auto log = commons::logging::LoggerFactory::getLogger("MAIL_LOG");

namespace {

using model::gameobjects::Item;
using model::gameobjects::Letter;
using model::gameobjects::LetterType;
using model::gameobjects::player::Mailbox;
using model::gameobjects::player::Player;
using model::gameobjects::player::PlayerCommonData;
using model::items::storage::Storage;
using model::items::storage::StorageType;
using model::templates::item::ItemQuality;
using model::templates::mail::MailMessage;
using network::aion::serverpackets::SM_DELETE_ITEM;
using network::aion::serverpackets::SM_MAIL_SERVICE;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;
using runtime::Ref;
using utils::PacketSendUtility;

/** Java: (long) a of a float - NaN 0, saturating (a C++ out-of-range conversion is undefined) */
int64_t javaFloatToLong(float a) noexcept {
	if (a != a)
		return 0;
	if (a >= 9223372036854775808.0f)
		return std::numeric_limits<int64_t>::max();
	if (a <= -9223372036854775808.0f)
		return std::numeric_limits<int64_t>::min();
	return static_cast<int64_t>(a);
}

/** Java long addition (wraps) */
int64_t javaAdd(int64_t a, int64_t b) noexcept {
	return static_cast<int64_t>(static_cast<uint64_t>(a) + static_cast<uint64_t>(b));
}

/** Java `new Timestamp(System.currentTimeMillis())` */
commons::database::Timestamp now() {
	return commons::database::Timestamp(std::chrono::milliseconds(commons::utils::currentTimeMillis()));
}

} // namespace

void MailService::sendMail(model::gameobjects::player::Player& sender, std::string_view recipientName, std::string_view titleValue,
	std::string_view messageValue, int32_t attachedItemObjId, int64_t attachedItemCount, int64_t attachedKinah, model::gameobjects::LetterType letterType) {
	// Java String.length() and substring count UTF-16 code units (CONVENTIONS.md "Strings")
	if (sender.isTrading() || commons::utils::StringUtils::utf16Length(recipientName) > 16)
		return;
	if (letterType == LetterType::BLACKCLOUD || attachedKinah < 0) {
		utils::audit::AuditLogger::log(sender, "tried to send letter of type " + std::string(xml::enumName(letterType)) + " with " +
			std::to_string(attachedKinah) + " Kinah");
		return;
	}

	std::string title(titleValue);
	if (commons::utils::StringUtils::utf16Length(title) > 20)
		title = commons::utils::StringUtils::substring(title, 0, 20);

	std::string message(messageValue);
	if (commons::utils::StringUtils::utf16Length(message) > 1000)
		message = commons::utils::StringUtils::substring(message, 0, 1000);

	Ref<PlayerCommonData> recipientCommonData = player::PlayerService::getOrLoadPlayerCommonData(recipientName);
	MailMessage status = validateRecipient(sender, recipientCommonData);
	if (status != MailMessage::MAIL_SEND_SUCCESS) {
		PacketSendUtility::sendPacket(sender, SM_MAIL_SERVICE(status));
		return;
	}

	Ref<Item> senderItem; // Java: a local, a strong reference also after the inventory lets the item go
	int32_t baseCost = letterType == LetterType::EXPRESS ? 500 : 10;
	int32_t costFactor = letterType == LetterType::EXPRESS ? 5 : 1;
	int64_t kinahMailCommission = 0;
	int64_t itemMailCommission = 0;

	Storage& senderInventory = sender.getInventory();

	if (attachedItemObjId != 0 && attachedItemCount > 0) {
		senderItem = senderInventory.getItemByObjId(attachedItemObjId);

		if (!senderItem || senderItem->getItemCount() < attachedItemCount) {
			PacketSendUtility::sendPacket(sender, SM_SYSTEM_MESSAGE::STR_MAIL_SEND_USED_ITEM());
			return;
		}

		if (senderItem->isEquipped()) {
			PacketSendUtility::sendPacket(sender, SM_SYSTEM_MESSAGE::STR_MAIL_SEND_CAN_NOT_SEND_EQUIPPED_ITEM());
			return;
		}

		if (!AdminService::getInstance().canOperate(sender, nullptr, *senderItem, "mail"))
			return;

		// Java: (long) (price * getQualityPriceRate(item) * attachedItemCount * costFactor) - a long times a float is a float, and so is every
		// later product; each product is rounded to float here as Java's float multiplication rounds it
		float itemCommission = static_cast<float>(senderItem->getItemTemplate()->getPrice()) * getQualityPriceRate(*senderItem);
		itemCommission = itemCommission * static_cast<float>(attachedItemCount);
		itemCommission = itemCommission * static_cast<float>(costFactor);
		itemMailCommission = javaFloatToLong(itemCommission);
	}

	if (attachedKinah > 0) {
		// Java: (long) (attachedKinah * 0.01f * costFactor), float arithmetic as above
		float kinahCommission = static_cast<float>(attachedKinah) * 0.01f;
		kinahCommission = kinahCommission * static_cast<float>(costFactor);
		kinahMailCommission = javaFloatToLong(kinahCommission);
	}

	int64_t finalMailKinah = javaAdd(
		trade::PricesService::getPriceForService(javaAdd(javaAdd(baseCost, kinahMailCommission), itemMailCommission), sender.getRace()), attachedKinah);

	if (senderInventory.getKinah() < finalMailKinah) {
		PacketSendUtility::sendPacket(sender, SM_SYSTEM_MESSAGE::STR_NOT_ENOUGH_MONEY());
		return;
	}

	Ref<Item> attachedItem;
	if (senderItem) {
		// Check Mailing untradables with Cash items (Special courier passes)
		if (senderItem->getPackCount() <= 0 && !senderItem->isTradeable()) {
			const model::templates::item::Disposition* dispo = senderItem->getItemTemplate()->getDisposition();
			if (dispo == nullptr || dispo->getId() == 0 || dispo->getCount() == 0) // can not be traded, hack
				return;

			if (senderInventory.getItemCountByItemId(dispo->getId()) >= dispo->getCount()) {
				senderInventory.decreaseByItemId(dispo->getId(), dispo->getCount());
			} else
				return;
		}

		// reuse item in case of full decrease of count
		if (senderItem->getItemCount() == attachedItemCount) {
			senderInventory.remove(*senderItem);
			PacketSendUtility::sendPacket(sender, SM_DELETE_ITEM(attachedItemObjId));
			attachedItem = senderItem;
		} else if (senderItem->getItemCount() > attachedItemCount) {
			attachedItem = item::ItemFactory::newItem(senderItem->getItemTemplate()->getTemplateId(), attachedItemCount);
			senderInventory.decreaseItemCount(*senderItem, attachedItemCount);
		}

		if (!attachedItem)
			return;

		// unpack
		if (attachedItem->getPackCount() > 0)
			attachedItem->setPackCount(attachedItem->getPackCount() * -1);

		attachedItem->setItemLocation(model::items::storage::getId(StorageType::MAILBOX));
	}

	senderInventory.decreaseKinah(finalMailKinah);
	Ref<Letter> newLetter = Letter::create(utils::idfactory::IDFactory::getInstance().nextId(), recipientCommonData->getPlayerObjId(), attachedItem,
		attachedKinah, title, message, sender.getName(), now(), true, letterType);

	// first save attached item for FK consistency
	if (attachedItem) {
		if (!dao::InventoryDAO::store(*attachedItem, recipientCommonData->getPlayerObjId()))
			return;
		// save item stones too
		dao::ItemStoneListDAO::save(std::vector<Ptr<Item>>{Ptr<Item>(attachedItem)});
	}
	// save letter
	if (!dao::MailDAO::storeLetter(*newLetter))
		return;

	if (attachedItem && configs::main::LoggingConfig::LOG_MAIL.load())
		log.info("Player: " + sender.getName() + " sent item " + std::to_string(attachedItem->getItemId()) + " [" + attachedItem->getItemName() +
			"] (count: " + std::to_string(attachedItem->getItemCount()) + ") to player " + std::string(recipientName));

	PacketSendUtility::sendPacket(sender, SM_MAIL_SERVICE(status));
	SystemMailService::updateRecipientMailbox(*recipientCommonData, *newLetter);
}

model::templates::mail::MailMessage MailService::validateRecipient(model::gameobjects::player::Player& sender,
	runtime::Ptr<model::gameobjects::player::PlayerCommonData> recipientCommonData) {
	if (!recipientCommonData)
		return MailMessage::NO_SUCH_CHARACTER_NAME;
	if (recipientCommonData->getRace() != sender.getRace() && !sender.isStaff())
		return MailMessage::MAIL_IS_ONE_RACE_ONLY;
	if (recipientCommonData->getMailboxLetters() >= 100)
		return MailMessage::RECIPIENT_MAILBOX_FULL;
	Ptr<Player> p = world::World::getInstance().getPlayer(recipientCommonData->getPlayerObjId());
	Ref<model::gameobjects::player::BlockList> blockList =
		p ? Ref<model::gameobjects::player::BlockList>(p->getBlockList()) : dao::BlockListDAO::load(recipientCommonData->getPlayerObjId());
	if (blockList->contains(sender.getObjectId()))
		return MailMessage::YOU_ARE_IN_RECIPIENT_IGNORE_LIST;
	return MailMessage::MAIL_SEND_SUCCESS;
}

float MailService::getQualityPriceRate(model::gameobjects::Item& senderItem) {
	std::optional<ItemQuality> quality = senderItem.getItemTemplate()->getItemQuality();
	if (!quality) // Java: a switch on a null enum
		throw runtime::NullPointerException("senderItem.getItemTemplate().getItemQuality()");
	switch (*quality) {
		case ItemQuality::MYTHIC:
		case ItemQuality::EPIC:
			return 0.05f;
		case ItemQuality::UNIQUE:
		case ItemQuality::LEGEND:
			return 0.04f;
		case ItemQuality::RARE:
			return 0.03f;
		default:
			return 0.02f;
	}
}

void MailService::readMail(model::gameobjects::player::Player& player, int32_t letterId) {
	Ptr<Letter> letter = player.getMailbox()->getLetterFromMailbox(letterId);
	if (!letter) {
		log.warn("Cannot read mail " + std::to_string(player.getObjectId()) + " " + std::to_string(letterId));
		return;
	}

	// Java: letter.getTimeStamp().getTime() (every letter carries its time stamp, Letter.cpp)
	PacketSendUtility::sendPacket(player, SM_MAIL_SERVICE(player, *letter, letter->getTimeStamp().value().time_since_epoch().count()));
	letter->setReadLetter();
}

void MailService::getAttachments(model::gameobjects::player::Player& player, int32_t letterId, int8_t attachmentType) {
	Ptr<Letter> letter = player.getMailbox()->getLetterFromMailbox(letterId);

	if (!letter)
		return;

	switch (attachmentType) {
		case 0: {
			Ref<Item> attachedItem = letter->getAttachedItem(); // Java: a local, a strong reference also after setAttachedItem(null)
			if (!attachedItem)
				return;
			if (player.getInventory().isFull()) {
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MAIL_TAKE_ALL_CANCEL());
				return;
			}
			if (attachedItem->getExpireTime() != 0 && attachedItem->getExpireTime() <= commons::utils::currentTimeMillis() / 1000) {
				attachedItem->setPersistentState(model::gameobjects::Persistable_PersistentState::DELETED);
				dao::InventoryDAO::store(*attachedItem, player);
			} else {
				if (!player.getInventory().add(*attachedItem, item::ItemPacketService_ItemAddType::MAIL))
					return;
				taskmanager::tasks::ExpireTimerTask::getInstance().registerExpirable(*attachedItem, player);
			}
			PacketSendUtility::sendPacket(player, SM_MAIL_SERVICE(letterId, attachmentType));
			letter->setAttachedItem(nullptr);
			break;
		}
		case 1: {
			// TODO normal fix
			// fix for kinah dupe
			int64_t attachedKinahCount = letter->getAttachedKinah();
			letter->removeAttachedKinah();
			if (!dao::MailDAO::storeLetter(*letter)) {
				Ptr<world::WorldPosition> position = player.getPosition();
				utils::audit::AuditLogger::log(player, "tried to use kinah mail exploit. Location: " + (position ? position->toString() : std::string("null")) +
					", kinah count: " + std::to_string(attachedKinahCount));
				return;
			}
			player.getInventory().increaseKinah(attachedKinahCount);
			PacketSendUtility::sendPacket(player, SM_MAIL_SERVICE(letterId, attachmentType));
			break;
		}
		default:
			break;
	}
}

void MailService::deleteMail(model::gameobjects::player::Player& player, std::span<const int32_t> mailObjId) {
	Ptr<Mailbox> mailbox = player.getMailbox();
	for (int32_t letterId : mailObjId) {
		mailbox->removeLetter(letterId);
		dao::MailDAO::deleteLetter(letterId);
	}
	PacketSendUtility::sendPacket(player, SM_MAIL_SERVICE(mailObjId));
}

void MailService::onPlayerLogin(model::gameobjects::player::Player& player) {
	player.setMailbox(dao::MailDAO::loadPlayerMailbox(player));
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_MAIL_SERVICE());
}

void MailService::sendMailList(model::gameobjects::player::Player& player, bool isExpress, bool sendRefreshPacket) {
	if (sendRefreshPacket)
		PacketSendUtility::sendPacket(player, SM_MAIL_SERVICE());

	std::vector<Ptr<Letter>> letterStream = player.getMailbox()->getLetters();
	if (isExpress)
		std::erase_if(letterStream, [](const Ptr<Letter>& letter) { return !(letter->isExpress() && letter->isUnread()); });
	// Java: sorted(Comparator.comparing(Letter::getTimeStamp).reversed()) - a stable sort, newest first
	std::stable_sort(letterStream.begin(), letterStream.end(),
		[](const Ptr<Letter>& a, const Ptr<Letter>& b) { return b->getTimeStamp().value() < a->getTimeStamp().value(); });
	std::vector<Ref<Letter>> letters(letterStream.begin(), letterStream.end());
	utils::collections::DynamicServerPacketBodySplitList<Letter> mailSplitList(std::move(letters), true, SM_MAIL_SERVICE::STATIC_BODY_SIZE,
		SM_MAIL_SERVICE::DYNAMIC_BODY_PART_SIZE_CALCULATOR);
	for (utils::collections::ListPart<Letter>& part : mailSplitList)
		PacketSendUtility::sendPacket(player, SM_MAIL_SERVICE(player, part.borrowed(), part.isLast()));
}

} // namespace aion::gameserver::services::mail
