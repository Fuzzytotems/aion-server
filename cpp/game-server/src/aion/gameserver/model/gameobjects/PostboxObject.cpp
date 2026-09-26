#include "aion/gameserver/model/gameobjects/PostboxObject.h"

#include "aion/gameserver/model/DialogPage.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/HouseRegistry.h"
#include "aion/gameserver/model/templates/housing/HousingPostbox.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_OBJECT_USE_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/player/PlayerMailboxState.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::gameobjects {

PostboxObject::PostboxObject(CreateKey key, house::HouseRegistry& registry, int32_t objId, int32_t templateId)
	: UseableHouseObject(key, registry, objId, templateId) {
}

PostboxObject::~PostboxObject() = default;

const templates::housing::HousingPostbox* PostboxObject::getObjectTemplate() const {
	return static_cast<const templates::housing::HousingPostbox*>(HouseObject::getObjectTemplate());
}

void PostboxObject::onUse(player::Player& player) {
	if (!setOccupant(player)) {
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_OBJECT_OCCUPIED_BY_OTHER());
		return;
	}
	player.getMailbox()->mailBoxState.set(services::player::PlayerMailboxState::REGULAR);
	utils::PacketSendUtility::sendPacket(player,
		network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_OBJECT_USE(getObjectTemplate()->getL10n()));
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_DIALOG_WINDOW(getObjectId(), id(DialogPage::MAIL)));
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_OBJECT_USE_UPDATE(player.getObjectId(), 0, 0, *this));
}

} // namespace aion::gameserver::model::gameobjects
