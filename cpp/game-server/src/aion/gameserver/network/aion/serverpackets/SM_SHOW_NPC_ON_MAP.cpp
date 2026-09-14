#include "aion/gameserver/network/aion/serverpackets/SM_SHOW_NPC_ON_MAP.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_SHOW_NPC_ON_MAP::SM_SHOW_NPC_ON_MAP(model::gameobjects::player::Player& playerValue, int32_t npcidValue, int32_t worldidValue, float xValue,
	float yValue, float zValue)
	: AionServerPacket(opcodeOf<SM_SHOW_NPC_ON_MAP>), player(playerValue), npcid(npcidValue), worldid(worldidValue), x(xValue), y(yValue), z(zValue) {
}

SM_SHOW_NPC_ON_MAP::~SM_SHOW_NPC_ON_MAP() = default;

void SM_SHOW_NPC_ON_MAP::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
