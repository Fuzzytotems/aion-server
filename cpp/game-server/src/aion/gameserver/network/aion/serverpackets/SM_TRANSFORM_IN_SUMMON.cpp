#include "aion/gameserver/network/aion/serverpackets/SM_TRANSFORM_IN_SUMMON.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_TRANSFORM_IN_SUMMON::SM_TRANSFORM_IN_SUMMON(model::gameobjects::player::Player& playerValue, model::gameobjects::Creature& creature)
	: AionServerPacket(opcodeOf<SM_TRANSFORM_IN_SUMMON>) {
	AION_UNPORTED();
}

SM_TRANSFORM_IN_SUMMON::SM_TRANSFORM_IN_SUMMON(model::gameobjects::player::Player& playerValue, int32_t creatureObjectId)
	: AionServerPacket(opcodeOf<SM_TRANSFORM_IN_SUMMON>), player(playerValue), summonObject(creatureObjectId) {
}

SM_TRANSFORM_IN_SUMMON::~SM_TRANSFORM_IN_SUMMON() = default;

void SM_TRANSFORM_IN_SUMMON::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
