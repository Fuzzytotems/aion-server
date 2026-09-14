#include "aion/gameserver/network/aion/serverpackets/SM_DELETE_HOUSE_OBJECT.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_DELETE_HOUSE_OBJECT::SM_DELETE_HOUSE_OBJECT(int32_t itemObjectIdValue)
	: AionServerPacket(opcodeOf<SM_DELETE_HOUSE_OBJECT>), itemObjectId(itemObjectIdValue) {
}

void SM_DELETE_HOUSE_OBJECT::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
