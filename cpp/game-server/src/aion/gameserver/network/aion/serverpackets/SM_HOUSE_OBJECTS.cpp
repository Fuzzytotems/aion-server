#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_OBJECTS.h"

#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_HOUSE_OBJECTS::SM_HOUSE_OBJECTS(const std::vector<runtime::Ptr<model::gameobjects::HouseObject>>& objectsValue)
	: AionServerPacket(opcodeOf<SM_HOUSE_OBJECTS>), objects(objectsValue.begin(), objectsValue.end()) {
}

SM_HOUSE_OBJECTS::~SM_HOUSE_OBJECTS() = default;

void SM_HOUSE_OBJECTS::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
