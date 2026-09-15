#include "aion/gameserver/network/aion/serverpackets/SM_SHOW_NPC_ON_MAP.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SHOW_NPC_ON_MAP::SM_SHOW_NPC_ON_MAP(model::gameobjects::player::Player& playerValue, int32_t npcidValue, int32_t worldidValue, float xValue,
	float yValue, float zValue)
	: AionServerPacket(opcodeOf<SM_SHOW_NPC_ON_MAP>), player(playerValue), npcid(npcidValue), worldid(worldidValue), x(xValue), y(yValue), z(zValue) {
}

SM_SHOW_NPC_ON_MAP::~SM_SHOW_NPC_ON_MAP() = default;

void SM_SHOW_NPC_ON_MAP::writeImpl(AionConnection* con) {
	writeD(npcid);
	writeD(worldid);
	// default value: mapid + channelId(0)
	int32_t instanceId = worldid;
	if (player->getPosition()->getMapId() == worldid) {
		if (player->isInInstance())
			instanceId = player->getInstanceId();
		else
			instanceId = worldid + player->getInstanceId() - 1; // mapid + channelId (instanceId-1)
	}
	writeD(instanceId);
	writeF(x);
	writeF(y);
	writeF(z);
}

} // namespace aion::gameserver::network::aion::serverpackets
