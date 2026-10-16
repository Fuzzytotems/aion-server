#include "aion/gameserver/services/abyss/GloryPointsService.h"

#include "aion/gameserver/dao/AbyssRankDAO.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_RANK.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::services::abyss {

using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

// Java GloryPointsService.java:18-35
void GloryPointsService::addGp(int32_t playerObjId, int32_t amount) {
	if (amount == 0)
		return;
	runtime::Ptr<model::gameobjects::player::Player> player = world::World::getInstance().getPlayer(playerObjId);
	bool addToStats = amount > 0;
	if (!player) {
		dao::AbyssRankDAO::addGp(playerObjId, amount, addToStats);
	} else {
		// java-race: AbyssRank.addGp is an unsynchronized read-modify-write (AbyssRank.cpp), so `added` can include another thread's grant
		int32_t oldGp = player->getAbyssRank()->getCurrentGP();
		player->getAbyssRank()->addGp(amount, addToStats);
		int32_t added = player->getAbyssRank()->getCurrentGP() - oldGp;

		PacketSendUtility::sendPacket(*player, amount >= 0 ? SM_SYSTEM_MESSAGE::STR_MSG_GLORY_POINT_GAIN(added) : SM_SYSTEM_MESSAGE::STR_MSG_GLORY_POINT_LOSE(-added));
		if (added != 0)
			PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_ABYSS_RANK(*player));
	}
}

} // namespace aion::gameserver::services::abyss
