#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_RANK_UPDATE.h"

#include "aion/gameserver/model/gameobjects/detail/ObjectsData.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ABYSS_RANK_UPDATE::SM_ABYSS_RANK_UPDATE(int32_t actionValue, model::gameobjects::player::Player& playerValue)
	: AionServerPacket(opcodeOf<SM_ABYSS_RANK_UPDATE>), player(playerValue), action(actionValue) {
}

SM_ABYSS_RANK_UPDATE::~SM_ABYSS_RANK_UPDATE() = default;

void SM_ABYSS_RANK_UPDATE::writeImpl(AionConnection* con) {
	writeC(action);
	writeD(player->getObjectId());
	switch (action) {
		case 0: // Abyss rank change
			writeD(model::gameobjects::detail::abyssRankId(player->getAbyssRank()->getRank()));
			break;
		case 1: // Team objectId
			writeD(player->getCurrentTeamId());
			break;
		case 2: // Mentor status change
			if (player->isMentor())
				writeD(1);
			else
				writeD(0);
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
