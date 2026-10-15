#include "aion/gameserver/network/aion/serverpackets/SM_MAIL_SERVICE.h"

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Letter.h"
#include "aion/gameserver/model/gameobjects/LetterTypeInfo.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/mail/MailMessageInfo.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/iteminfo/ItemInfoBlob.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"

namespace aion::gameserver::network::aion::serverpackets {

// callback key com.aionemu.gameserver.network.aion.serverpackets.SM_MAIL_SERVICE@L23:84 (captureless, ported)
const runtime::PinnedCallback<int32_t(model::gameobjects::Letter&)> SM_MAIL_SERVICE::DYNAMIC_BODY_PART_SIZE_CALCULATOR(
	[](model::gameobjects::Letter& letter) -> int32_t {
		return 22 + byteLengthForString(letter.getSenderName()) + byteLengthForString(letter.getTitle());
	});

SM_MAIL_SERVICE::SM_MAIL_SERVICE()
	: AionServerPacket(opcodeOf<SM_MAIL_SERVICE>), serviceId(0) {
}

SM_MAIL_SERVICE::SM_MAIL_SERVICE(model::templates::mail::MailMessage mailMessageValue)
	: AionServerPacket(opcodeOf<SM_MAIL_SERVICE>), serviceId(1) {
	mailMessage = model::templates::mail::getId(mailMessageValue);
}

SM_MAIL_SERVICE::SM_MAIL_SERVICE(model::gameobjects::player::Player& playerValue,
	const std::vector<runtime::Ptr<model::gameobjects::Letter>>& lettersValue, bool isLastPacketValue)
	: AionServerPacket(opcodeOf<SM_MAIL_SERVICE>), player(playerValue), serviceId(2), letters(lettersValue.begin(), lettersValue.end()),
	  isLastPacket(isLastPacketValue) {
}

SM_MAIL_SERVICE::SM_MAIL_SERVICE(model::gameobjects::player::Player& playerValue, model::gameobjects::Letter& letterValue, int64_t timeValue)
	: AionServerPacket(opcodeOf<SM_MAIL_SERVICE>), player(playerValue), serviceId(3), letter(letterValue), time(timeValue) {
}

SM_MAIL_SERVICE::SM_MAIL_SERVICE(int32_t letterIdValue, int8_t attachmentTypeValue)
	: AionServerPacket(opcodeOf<SM_MAIL_SERVICE>), serviceId(5), letterId(letterIdValue), attachmentType(attachmentTypeValue) {
}

SM_MAIL_SERVICE::SM_MAIL_SERVICE(std::span<const int32_t> letterIdsValue)
	: AionServerPacket(opcodeOf<SM_MAIL_SERVICE>), serviceId(6), letterIds(letterIdsValue.begin(), letterIdsValue.end()) {
}

SM_MAIL_SERVICE::~SM_MAIL_SERVICE() = default;

void SM_MAIL_SERVICE::writeImpl(AionConnection* con) {
	runtime::Ptr<model::gameobjects::player::Mailbox> mailbox = detail::requireConnection(con, "SM_MAIL_SERVICE").getActivePlayer()->getMailbox();
	int32_t totalCount = mailbox->size();
	int32_t unreadCount = mailbox->getUnreadCount();
	int32_t unreadExpressCount = mailbox->getUnreadCountByType(model::gameobjects::LetterType::EXPRESS);
	int32_t unreadBlackCloudCount = mailbox->getUnreadCountByType(model::gameobjects::LetterType::BLACKCLOUD);
	writeC(serviceId);
	switch (serviceId) {
		case 0:
			writeMailboxState(totalCount, unreadCount, unreadExpressCount, unreadBlackCloudCount);
			break;
		case 1:
			writeMailMessage(mailMessage);
			break;
		case 2:
			writeLettersList(std::vector<runtime::Ptr<model::gameobjects::Letter>>(letters.begin(), letters.end()));
			break;
		case 3:
			writeLetterRead(*letter, time, totalCount, unreadCount, unreadExpressCount, unreadBlackCloudCount);
			break;
		case 5:
			writeLetterState(letterId, attachmentType);
			break;
		case 6:
			writeLetterDelete(totalCount, unreadCount, unreadExpressCount, unreadBlackCloudCount, std::span<const int32_t>(letterIds));
			break;
	}
}

void SM_MAIL_SERVICE::writeLettersList(const std::vector<runtime::Ptr<model::gameobjects::Letter>>& value) {
	writeD(player->getObjectId());
	writeC(0);
	writeH(isLastPacket ? static_cast<int32_t>(value.size()) * -1 : static_cast<int32_t>(value.size()));
	for (const runtime::Ptr<model::gameobjects::Letter>& entry : value) {
		writeD(entry->getObjectId());
		writeS(entry->getSenderName());
		writeS(entry->getTitle());
		writeC(entry->isUnread() ? 0 : 1); // isRead
		runtime::Ptr<model::gameobjects::Item> attachedItem = entry->getAttachedItem(); // Java reads getAttachedItem() per use
		writeD(attachedItem == nullptr ? 0 : attachedItem->getObjectId());
		writeD(attachedItem == nullptr ? 0 : attachedItem->getItemTemplate()->getTemplateId());
		writeQ(entry->getAttachedKinah());
		writeC(model::gameobjects::getId(entry->getLetterType()));
	}
}

void SM_MAIL_SERVICE::writeMailMessage(int32_t messageId) {
	writeC(messageId);
}

void SM_MAIL_SERVICE::writeMailboxState(int32_t totalCount, int32_t unreadCount, int32_t expressCount, int32_t blackCloudCount) {
	writeH(totalCount);
	writeH(unreadCount);
	writeH(expressCount);
	writeH(blackCloudCount);
}

void SM_MAIL_SERVICE::writeLetterRead(model::gameobjects::Letter& value, int64_t timeValue, int32_t totalCount, int32_t unreadCount,
	int32_t expressCount, int32_t blackCloudCount) {
	writeD(value.getRecipientId());
	writeD(totalCount + unreadCount * 0x10000); // total count + unread hex
	writeD(expressCount + blackCloudCount); // unread express + BC letters count
	writeD(value.getObjectId());
	writeD(value.getRecipientId());
	writeS(value.getSenderName());
	writeS(value.getTitle());
	writeS(value.getMessage());
	runtime::Ptr<model::gameobjects::Item> item = value.getAttachedItem();
	if (item != nullptr) {
		const model::templates::item::ItemTemplate* itemTemplate = item->getItemTemplate();
		writeD(item->getObjectId());
		writeD(itemTemplate->getTemplateId());
		writeD(1); // unk
		writeD(0); // unk
		writeS(itemTemplate->getL10n());
		runtime::Ref<iteminfo::ItemInfoBlob> itemInfoBlob = iteminfo::ItemInfoBlob::getFullBlob(player, *item);
		itemInfoBlob->writeMe(getBuf());
	} else {
		writeQ(0);
		writeQ(0);
		writeD(0);
	}
	writeD(static_cast<int32_t>(value.getAttachedKinah()));
	writeD(0); // AP reward for castle assault/defense (in future)
	writeC(0);
	writeD(static_cast<int32_t>(timeValue / 1000));
	writeC(model::gameobjects::getId(value.getLetterType())); // mail type
}

void SM_MAIL_SERVICE::writeLetterState(int32_t value, int8_t attachmentTypeValue) {
	writeD(value);
	writeC(attachmentTypeValue);
	writeC(1);
}

void SM_MAIL_SERVICE::writeLetterDelete(int32_t totalCount, int32_t unreadCount, int32_t expressCount, int32_t blackCloudCount,
	std::initializer_list<int32_t> value) {
	writeLetterDelete(totalCount, unreadCount, expressCount, blackCloudCount, std::span<const int32_t>(value.begin(), value.size()));
}

void SM_MAIL_SERVICE::writeLetterDelete(int32_t totalCount, int32_t unreadCount, int32_t expressCount, int32_t blackCloudCount,
	std::span<const int32_t> value) {
	writeD(totalCount + unreadCount * 0x10000); // total count + unread hex
	writeD(expressCount + blackCloudCount); // unread express + BC letters count
	writeH(static_cast<int32_t>(value.size()));
	for (int32_t id : value)
		writeD(id);
}

} // namespace aion::gameserver::network::aion::serverpackets
