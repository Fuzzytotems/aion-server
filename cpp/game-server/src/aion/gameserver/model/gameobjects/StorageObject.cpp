#include "aion/gameserver/model/gameobjects/StorageObject.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/house/HouseRegistry.h"
#include "aion/gameserver/model/templates/housing/HousingStorage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_OBJECT_USE_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::gameobjects {

StorageObject::StorageObject(CreateKey key, house::HouseRegistry& registry, int32_t objId, int32_t templateId)
	: UseableHouseObject(key, registry, objId, templateId) {
}

StorageObject::~StorageObject() = default;

const templates::housing::HousingStorage* StorageObject::getObjectTemplate() const {
	return static_cast<const templates::housing::HousingStorage*>(HouseObject::getObjectTemplate());
}

void StorageObject::onUse(player::Player& player) {
	if (player.getObjectId() != getOwnerHouse()->getOwnerId()) {
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_OBJECT_IS_ONLY_FOR_OWNER_VALID());
		return;
	}
	if (!setOccupant(player)) {
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_OBJECT_OCCUPIED_BY_OTHER());
		return;
	}
	utils::PacketSendUtility::sendPacket(player,
		network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_OBJECT_USE(getObjectTemplate()->getL10n()));
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_OBJECT_USE_UPDATE(player.getObjectId(), 0, 0, *this));
}

} // namespace aion::gameserver::model::gameobjects
