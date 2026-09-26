#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_OBJECTS.h"

#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/model/templates/housing/PlaceableHouseObject.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_HOUSE_OBJECTS::SM_HOUSE_OBJECTS(const std::vector<runtime::Ptr<model::gameobjects::HouseObject>>& objectsValue)
	: AionServerPacket(opcodeOf<SM_HOUSE_OBJECTS>), objects(objectsValue.begin(), objectsValue.end()) {
}

SM_HOUSE_OBJECTS::~SM_HOUSE_OBJECTS() = default;

void SM_HOUSE_OBJECTS::writeImpl(AionConnection* con) {
	writeH(static_cast<int32_t>(objects.size()));
	for (const runtime::Ref<model::gameobjects::HouseObject>& obj : objects) {
		writeD(obj->getObjectTemplate()->getTemplateId());
		writeF(obj->getX());
		writeF(obj->getY());
		writeF(obj->getZ());
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
