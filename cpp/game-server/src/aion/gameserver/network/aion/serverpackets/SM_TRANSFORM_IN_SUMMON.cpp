#include "aion/gameserver/network/aion/serverpackets/SM_TRANSFORM_IN_SUMMON.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_TRANSFORM_IN_SUMMON::SM_TRANSFORM_IN_SUMMON(model::gameobjects::player::Player& playerValue, model::gameobjects::Creature& creature)
	: SM_TRANSFORM_IN_SUMMON(playerValue, creature.getObjectId()) {
}

SM_TRANSFORM_IN_SUMMON::SM_TRANSFORM_IN_SUMMON(model::gameobjects::player::Player& playerValue, int32_t creatureObjectId)
	: AionServerPacket(opcodeOf<SM_TRANSFORM_IN_SUMMON>), player(playerValue), summonObject(creatureObjectId) {
}

SM_TRANSFORM_IN_SUMMON::~SM_TRANSFORM_IN_SUMMON() = default;

void SM_TRANSFORM_IN_SUMMON::writeImpl(AionConnection* con) {
	writeD(summonObject);
	writeS(player->getName());
	writeD(player->getObjectId());
}

} // namespace aion::gameserver::network::aion::serverpackets
