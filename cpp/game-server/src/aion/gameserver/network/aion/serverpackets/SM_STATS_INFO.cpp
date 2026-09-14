#include "aion/gameserver/network/aion/serverpackets/SM_STATS_INFO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_STATS_INFO::SM_STATS_INFO(model::gameobjects::player::Player& playerValue)
	: AionServerPacket(opcodeOf<SM_STATS_INFO>), player(playerValue) {
	AION_UNPORTED();
}

SM_STATS_INFO::~SM_STATS_INFO() = default;

void SM_STATS_INFO::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
