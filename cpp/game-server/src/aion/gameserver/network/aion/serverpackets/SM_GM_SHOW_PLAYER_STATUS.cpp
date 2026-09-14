#include "aion/gameserver/network/aion/serverpackets/SM_GM_SHOW_PLAYER_STATUS.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_GM_SHOW_PLAYER_STATUS::SM_GM_SHOW_PLAYER_STATUS(model::gameobjects::player::Player& playerValue)
	: AionServerPacket(opcodeOf<SM_GM_SHOW_PLAYER_STATUS>) {
	AION_UNPORTED();
}

SM_GM_SHOW_PLAYER_STATUS::~SM_GM_SHOW_PLAYER_STATUS() = default;

void SM_GM_SHOW_PLAYER_STATUS::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
