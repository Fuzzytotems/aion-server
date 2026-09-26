#include "aion/gameserver/network/aion/serverpackets/SM_TELEPORT_LOC.h"

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/model/animations/TeleportAnimationInfo.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_TELEPORT_LOC::SM_TELEPORT_LOC(int32_t mapIdValue, int32_t instanceIdValue, float xValue, float yValue, float zValue, int8_t headingValue,
	model::animations::TeleportAnimation portAnimationValue)
	: AionServerPacket(opcodeOf<SM_TELEPORT_LOC>), mapId(mapIdValue), instanceId(instanceIdValue), x(xValue), y(yValue), z(zValue),
	  heading(headingValue) {
	const model::templates::world::WorldMapTemplate* worldMapTemplate = dataholders::DataManager::WORLD_MAPS_DATA->getTemplate(mapIdValue);
	if (worldMapTemplate == nullptr) // Java: DataManager.WORLD_MAPS_DATA.getTemplate(mapId).isInstance() on null
		throw runtime::NullPointerException("SM_TELEPORT_LOC: no world map template " + std::to_string(mapIdValue));
	isInstance = worldMapTemplate->isInstance();
	portAnimation = model::animations::getId(portAnimationValue);
}

void SM_TELEPORT_LOC::writeImpl(AionConnection* con) {
	writeC(portAnimation);
	writeD(mapId); // new 4.3 NA -->old //writeH(mapId & 0xFFFF);
	writeD(isInstance ? instanceId : mapId); // mapId | instanceId
	writeF(x);
	writeF(y);
	writeF(z);
	writeC(heading);
}

} // namespace aion::gameserver::network::aion::serverpackets
