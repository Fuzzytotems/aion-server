#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_RANK_UPDATE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ABYSS_RANK_UPDATE::SM_ABYSS_RANK_UPDATE(int32_t actionValue, model::gameobjects::player::Player& playerValue)
	: AionServerPacket(opcodeOf<SM_ABYSS_RANK_UPDATE>), player(playerValue), action(actionValue) {
}

SM_ABYSS_RANK_UPDATE::~SM_ABYSS_RANK_UPDATE() = default;

void SM_ABYSS_RANK_UPDATE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
