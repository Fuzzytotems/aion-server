#include "aion/gameserver/network/aion/serverpackets/SM_TRANSFORM.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_TRANSFORM::SM_TRANSFORM(model::gameobjects::Creature& creatureValue)
	: AionServerPacket(opcodeOf<SM_TRANSFORM>), creature(creatureValue) {
}

SM_TRANSFORM::~SM_TRANSFORM() = default;

void SM_TRANSFORM::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
