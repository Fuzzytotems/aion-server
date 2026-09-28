#include "aion/gameserver/network/aion/serverpackets/SM_FORCED_MOVE.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_FORCED_MOVE::SM_FORCED_MOVE(model::gameobjects::Creature& creatureValue, model::gameobjects::Creature& target)
	: SM_FORCED_MOVE(creatureValue, target.getObjectId(), target.getX(), target.getY(), target.getZ()) {
}

SM_FORCED_MOVE::SM_FORCED_MOVE(model::gameobjects::Creature& creatureValue, int32_t objectIdValue, float xValue, float yValue, float zValue)
	: AionServerPacket(opcodeOf<SM_FORCED_MOVE>), creature(creatureValue), objectId(objectIdValue), x(xValue), y(yValue), z(zValue) {
}

SM_FORCED_MOVE::~SM_FORCED_MOVE() = default;

void SM_FORCED_MOVE::writeImpl(AionConnection* con) {
	writeD(creature->getObjectId());
	writeD(objectId); // targets objectId
	writeC(16); // unk
	writeF(x);
	writeF(y);
	writeF(z);
}

} // namespace aion::gameserver::network::aion::serverpackets
