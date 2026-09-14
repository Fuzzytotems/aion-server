#include "aion/gameserver/network/aion/serverpackets/SM_FORCED_MOVE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/gameobjects/Creature.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_FORCED_MOVE::SM_FORCED_MOVE(model::gameobjects::Creature& creatureValue, model::gameobjects::Creature& target)
	: AionServerPacket(opcodeOf<SM_FORCED_MOVE>) {
	AION_UNPORTED();
}

SM_FORCED_MOVE::SM_FORCED_MOVE(model::gameobjects::Creature& creatureValue, int32_t objectIdValue, float xValue, float yValue, float zValue)
	: AionServerPacket(opcodeOf<SM_FORCED_MOVE>), creature(creatureValue), objectId(objectIdValue), x(xValue), y(yValue), z(zValue) {
}

SM_FORCED_MOVE::~SM_FORCED_MOVE() = default;

void SM_FORCED_MOVE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
