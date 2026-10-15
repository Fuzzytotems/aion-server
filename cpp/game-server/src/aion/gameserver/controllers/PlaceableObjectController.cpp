#include "aion/gameserver/controllers/PlaceableObjectController.h"

#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/model/gameobjects/UseableHouseObject.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::controllers {

using model::gameobjects::HouseObject;
using model::gameobjects::player::Player;
using runtime::Ptr;

void PlaceableObjectController::onDespawn() {
	VisibleObjectController::onDespawn();
	static_cast<HouseObject&>(getOwner()).onDespawn(); // Java: getOwner() is HouseObject<T> (erased generic)
}

void PlaceableObjectController::onDialogRequest(model::gameobjects::player::Player& player) {
	HouseObject& houseObject = static_cast<HouseObject&>(getOwner());
	if (!utils::PositionUtil::isInTalkRange(player, houseObject)) {
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_OBJECT_TOO_FAR_TO_USE());
		return;
	}
	houseObject.onDialogRequest(player);
}

void PlaceableObjectController::notKnow(model::gameobjects::VisibleObject& object) {
	VisibleObjectController::notKnow(object);
	Ptr<model::gameobjects::UseableHouseObject> useableHouseObject = runtime::as<model::gameobjects::UseableHouseObject>(getOwner());
	if (Ptr<Player> player = runtime::as<Player>(object); useableHouseObject && player)
		useableHouseObject->releaseOccupant(*player);
}

} // namespace aion::gameserver::controllers
