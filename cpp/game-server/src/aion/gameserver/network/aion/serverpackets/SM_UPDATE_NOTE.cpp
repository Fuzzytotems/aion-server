#include "aion/gameserver/network/aion/serverpackets/SM_UPDATE_NOTE.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_UPDATE_NOTE::SM_UPDATE_NOTE(model::gameobjects::player::Player& player)
	: AionServerPacket(opcodeOf<SM_UPDATE_NOTE>) {
	targetObjId = player.getObjectId();
	note = player.getCommonData()->getNote();
}

void SM_UPDATE_NOTE::writeImpl(AionConnection* con) {
	writeD(targetObjId);
	writeS(note);
}

} // namespace aion::gameserver::network::aion::serverpackets
