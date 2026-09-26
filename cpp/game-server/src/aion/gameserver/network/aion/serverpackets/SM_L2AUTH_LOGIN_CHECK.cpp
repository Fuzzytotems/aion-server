#include "aion/gameserver/network/aion/serverpackets/SM_L2AUTH_LOGIN_CHECK.h"

#include <cstddef>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_L2AUTH_LOGIN_CHECK::SM_L2AUTH_LOGIN_CHECK(bool okValue, std::string_view accountNameValue)
	: AionServerPacket(opcodeOf<SM_L2AUTH_LOGIN_CHECK>), ok(okValue), accountName(accountNameValue) {
}

void SM_L2AUTH_LOGIN_CHECK::writeImpl(AionConnection* con) {
	writeD(ok ? 0x00 : 0x01);
	writeC(0); // server ID override (added in 4.7)
	writeC(0); // 1 on Fast-Track Server: makes the client send C_REQUEST_DIRECT_ENTER_WORLD
	writeC(0); // 1 on Fast-Track Server: displays the origin server's name above the minimap and as system message
	writeC(0); // 1 on Fast-Track Server
	for (size_t i = 0; i < serverIdByIndex.size(); i++) {
		int8_t serverId = serverIdByIndex[i];
		writeC(serverId == 0 ? 0 : static_cast<int32_t>(i));
		writeC(serverId);
		writeC(serverId);
	}
	for (size_t serverId = 0; serverId < serverIndexById.size(); serverId++) {
		int8_t i = serverIndexById[serverId];
		writeC(i);
		writeC(i == 0 ? 0 : static_cast<int32_t>(serverId));
		writeC(i == 0 ? 0 : static_cast<int32_t>(serverId));
	}
	const dataholders::WorldMapsData& worldMapsData = *dataholders::DataManager::WORLD_MAPS_DATA;
	writeH(worldMapsData.size());
	for (const model::templates::world::WorldMapTemplate* worldMapTemplate : worldMapsData) {
		writeD(worldMapTemplate->getMapId());
		writeH(worldMapTemplate->isInstance() ? 0 : worldMapTemplate->getTwinCount()); // for Fast-Track Server it's getBeginnerTwinCount()
	}
	writeS(accountName);
}

} // namespace aion::gameserver::network::aion::serverpackets
