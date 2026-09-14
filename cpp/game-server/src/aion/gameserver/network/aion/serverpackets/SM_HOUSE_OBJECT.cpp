#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_OBJECT.h"

#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_HOUSE_OBJECT::SM_HOUSE_OBJECT(model::gameobjects::HouseObject& owner) : AionServerPacket(opcodeOf<SM_HOUSE_OBJECT>), houseObject(owner) {
}

SM_HOUSE_OBJECT::~SM_HOUSE_OBJECT() = default;

void SM_HOUSE_OBJECT::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
