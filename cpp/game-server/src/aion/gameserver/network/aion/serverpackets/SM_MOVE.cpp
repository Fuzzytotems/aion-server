#include "aion/gameserver/network/aion/serverpackets/SM_MOVE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_MOVE::SM_MOVE(model::gameobjects::Creature& creatureValue)
	: AionServerPacket(opcodeOf<SM_MOVE>) {
	AION_UNPORTED();
}

SM_MOVE::SM_MOVE(model::gameobjects::Creature& creatureValue, int8_t movementMaskValue)
	: AionServerPacket(opcodeOf<SM_MOVE>), creature(creatureValue), movementMask(movementMaskValue) {
}

SM_MOVE::~SM_MOVE() = default;

void SM_MOVE::writeImpl(AionConnection* client) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
