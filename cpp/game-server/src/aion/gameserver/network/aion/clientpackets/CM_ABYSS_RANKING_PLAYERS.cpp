#include "aion/gameserver/network/aion/clientpackets/CM_ABYSS_RANKING_PLAYERS.h"

#include <any>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_RANKING_PLAYERS.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/abyss/AbyssRankingCache.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_ABYSS_RANKING_PLAYERS::CM_ABYSS_RANKING_PLAYERS(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_ABYSS_RANKING_PLAYERS.java:33-36
void CM_ABYSS_RANKING_PLAYERS::readImpl() {
	raceId = readC();
}

// Java CM_ABYSS_RANKING_PLAYERS.java:38-67
void CM_ABYSS_RANKING_PLAYERS::runImpl() {
	using model::gameobjects::player::AbyssRank_AbyssRankUpdateType;
	runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	model::Race queriedRace;
	AbyssRank_AbyssRankUpdateType updateType;
	switch (raceId) {
		case 0:
			queriedRace = model::Race::ELYOS;
			updateType = AbyssRank_AbyssRankUpdateType::PLAYER_ELYOS;
			break;
		case 1:
			queriedRace = model::Race::ASMODIANS;
			updateType = AbyssRank_AbyssRankUpdateType::PLAYER_ASMODIANS;
			break;
		default:
			commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.clientpackets.CM_ABYSS_RANKING_PLAYERS")
				.warn("Received invalid raceId (" + std::to_string(raceId) + ") from player " + player->toString());
			return;
	}
	if (player->isAbyssRankListUpdated(updateType)) {
		sendPacket(serverpackets::SM_ABYSS_RANKING_PLAYERS(services::abyss::AbyssRankingCache::getInstance().getLastUpdate(), queriedRace));
	} else {
		runtime::Ptr<runtime::RcArrayList<std::shared_ptr<serverpackets::SM_ABYSS_RANKING_PLAYERS>>> results =
			services::abyss::AbyssRankingCache::getInstance().getPlayers(queriedRace);
		for (const std::shared_ptr<serverpackets::SM_ABYSS_RANKING_PLAYERS>& packet : results->snapshot())
			sendPacket(*packet);
		player->setAbyssRankListUpdated(updateType);
	}
}

AION_CLIENT_PACKET(CM_ABYSS_RANKING_PLAYERS);

} // namespace aion::gameserver::network::aion::clientpackets
