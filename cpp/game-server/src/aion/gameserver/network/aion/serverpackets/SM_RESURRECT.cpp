#include "aion/gameserver/network/aion/serverpackets/SM_RESURRECT.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_RESURRECT::SM_RESURRECT(model::gameobjects::Creature& creature)
	: SM_RESURRECT(creature, 0) {
}

SM_RESURRECT::SM_RESURRECT(model::gameobjects::Creature& creature, int32_t skillIdValue)
	: AionServerPacket(opcodeOf<SM_RESURRECT>), skillId(skillIdValue) {
	name = creature.getName();
}

void SM_RESURRECT::writeImpl(AionConnection* con) {
	writeS(name);
	writeH(skillId); // unk
	writeD(0);
}

} // namespace aion::gameserver::network::aion::serverpackets
