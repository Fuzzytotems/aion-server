#include "aion/gameserver/network/aion/serverpackets/SM_OBJECT_USE_UPDATE.h"

#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/model/templates/housing/UseItemAction.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_OBJECT_USE_UPDATE::SM_OBJECT_USE_UPDATE(int32_t usingPlayerIdValue, int32_t ownerPlayerIdValue, int32_t useCountValue,
	model::gameobjects::HouseObject& objectValue)
	: AionServerPacket(opcodeOf<SM_OBJECT_USE_UPDATE>), usingPlayerId(usingPlayerIdValue), ownerPlayerId(ownerPlayerIdValue),
	  useCount(useCountValue), object(objectValue) {
	AION_UNPORTED(); // action = UseableItemObject's template action
}

SM_OBJECT_USE_UPDATE::~SM_OBJECT_USE_UPDATE() = default;

void SM_OBJECT_USE_UPDATE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
