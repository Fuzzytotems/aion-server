#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_EDIT.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_HOUSE_EDIT::SM_HOUSE_EDIT(int32_t actionValue) : AionServerPacket(opcodeOf<SM_HOUSE_EDIT>), action(actionValue) {
}

SM_HOUSE_EDIT::SM_HOUSE_EDIT(int32_t actionValue, int32_t storeIdValue, int32_t itemObjectIdValue) : SM_HOUSE_EDIT(actionValue) {
	this->itemObjectId = itemObjectIdValue;
	this->storeId = storeIdValue;
}

SM_HOUSE_EDIT::SM_HOUSE_EDIT(int32_t actionValue, int32_t itemObjectIdValue, float xValue, float yValue, float zValue, int32_t rotationValue)
	: AionServerPacket(opcodeOf<SM_HOUSE_EDIT>), action(actionValue), itemObjectId(itemObjectIdValue), x(xValue), y(yValue), z(zValue),
	  rotation(rotationValue) {
}

void SM_HOUSE_EDIT::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
