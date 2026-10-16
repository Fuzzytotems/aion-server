#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_STATE.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualStateInfo.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_PLAYER_STATE::SM_PLAYER_STATE(model::gameobjects::Creature& creature)
	: AionServerPacket(opcodeOf<SM_PLAYER_STATE>) {
	playerObjId = creature.getObjectId();
	visualState = creature.getVisualState();
	seeState = creature.getSeeState();
}

void SM_PLAYER_STATE::writeImpl(AionConnection* con) {
	using model::gameobjects::state::CreatureVisualState;
	writeD(playerObjId);
	writeC(visualState);
	writeC(seeState);
	writeC(visualState == model::gameobjects::state::getId(CreatureVisualState::BLINKING) ? 0x01 : 0x00);
}

} // namespace aion::gameserver::network::aion::serverpackets
