#include "aion/gameserver/network/aion/serverpackets/SM_OBJECT_USE_UPDATE.h"

#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/model/gameobjects/PostboxObject.h"
#include "aion/gameserver/model/gameobjects/StorageObject.h"
#include "aion/gameserver/model/gameobjects/UseableItemObject.h"
#include "aion/gameserver/model/templates/housing/HousingUseableItem.h"
#include "aion/gameserver/model/templates/housing/PlaceableHouseObject.h"
#include "aion/gameserver/model/templates/housing/UseItemAction.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_OBJECT_USE_UPDATE::SM_OBJECT_USE_UPDATE(int32_t usingPlayerIdValue, int32_t ownerPlayerIdValue, int32_t useCountValue,
	model::gameobjects::HouseObject& objectValue)
	: AionServerPacket(opcodeOf<SM_OBJECT_USE_UPDATE>), usingPlayerId(usingPlayerIdValue), ownerPlayerId(ownerPlayerIdValue),
	  useCount(useCountValue), object(objectValue) {
	if (auto* useableItemObject = dynamic_cast<model::gameobjects::UseableItemObject*>(&objectValue))
		action = useableItemObject->getObjectTemplate()->getAction();
}

SM_OBJECT_USE_UPDATE::~SM_OBJECT_USE_UPDATE() = default;

void SM_OBJECT_USE_UPDATE::writeImpl(AionConnection* con) {
	writeC(object->getObjectTemplate()->getTypeId());
	if (runtime::as<model::gameobjects::PostboxObject>(runtime::Ptr<model::gameobjects::HouseObject>(object)) != nullptr
		|| runtime::as<model::gameobjects::StorageObject>(runtime::Ptr<model::gameobjects::HouseObject>(object)) != nullptr) {
		writeD(usingPlayerId);
		writeC(1); // unk
		writeD(object->getObjectId());
	} else if (runtime::as<model::gameobjects::UseableItemObject>(runtime::Ptr<model::gameobjects::HouseObject>(object)) != nullptr) {
		writeD(usingPlayerId);
		writeD(ownerPlayerId);
		writeD(object->getObjectId());
		writeD(useCount);
		int32_t checkType = 0;
		if (action != nullptr && action->getCheckType().has_value())
			checkType = *action->getCheckType();
		writeC(checkType);
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
