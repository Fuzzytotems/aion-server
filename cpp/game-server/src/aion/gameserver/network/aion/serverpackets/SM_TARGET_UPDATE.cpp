#include "aion/gameserver/network/aion/serverpackets/SM_TARGET_UPDATE.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_TARGET_UPDATE::SM_TARGET_UPDATE(model::gameobjects::player::Player& playerValue)
	: AionServerPacket(opcodeOf<SM_TARGET_UPDATE>), player(playerValue) {
}

SM_TARGET_UPDATE::~SM_TARGET_UPDATE() = default;

void SM_TARGET_UPDATE::writeImpl(AionConnection* con) {
	writeD(player->getObjectId());
	runtime::Ptr<model::gameobjects::VisibleObject> target = player->getTarget();
	writeD(target == nullptr ? 0 : target->getObjectId());
}

} // namespace aion::gameserver::network::aion::serverpackets
