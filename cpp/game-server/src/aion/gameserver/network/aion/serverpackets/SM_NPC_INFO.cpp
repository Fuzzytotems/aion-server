#include "aion/gameserver/network/aion/serverpackets/SM_NPC_INFO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_NPC_INFO::SM_NPC_INFO(model::gameobjects::Npc& npcValue, model::gameobjects::player::Player& player)
	: AionServerPacket(opcodeOf<SM_NPC_INFO>), npc(npcValue) {
	AION_UNPORTED();
}

SM_NPC_INFO::SM_NPC_INFO(model::gameobjects::Summon& summon, model::gameobjects::player::Player& player)
	: AionServerPacket(opcodeOf<SM_NPC_INFO>), npc(summon) {
	AION_UNPORTED();
}

SM_NPC_INFO::~SM_NPC_INFO() = default;

void SM_NPC_INFO::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
