#include "aion/gameserver/network/aion/clientpackets/CM_ABYSS_RANKING_LEGIONS.h"

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
#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_RANKING_LEGIONS.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/abyss/AbyssRankingCache.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_ABYSS_RANKING_LEGIONS::CM_ABYSS_RANKING_LEGIONS(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_ABYSS_RANKING_LEGIONS.java:33-36
void CM_ABYSS_RANKING_LEGIONS::readImpl() {
	raceId = readC();
}

// Java CM_ABYSS_RANKING_LEGIONS.java:38-68
void CM_ABYSS_RANKING_LEGIONS::runImpl() {
	using model::gameobjects::player::AbyssRank_AbyssRankUpdateType;
	runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	model::Race queriedRace;
	AbyssRank_AbyssRankUpdateType updateType;
	switch (raceId) {
		case 0:
			queriedRace = model::Race::ELYOS;
			updateType = AbyssRank_AbyssRankUpdateType::LEGION_ELYOS;
			break;
		case 1:
			queriedRace = model::Race::ASMODIANS;
			updateType = AbyssRank_AbyssRankUpdateType::LEGION_ASMODIANS;
			break;
		default:
			commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.clientpackets.CM_ABYSS_RANKING_LEGIONS")
				.warn("Received invalid raceId (" + std::to_string(raceId) + ") from player " + player->toString());
			return;
	}

	// calculate rankings and send packet
	std::shared_ptr<serverpackets::SM_ABYSS_RANKING_LEGIONS> legionRanking;
	if (player->isAbyssRankListUpdated(updateType)) {
		legionRanking = std::make_shared<serverpackets::SM_ABYSS_RANKING_LEGIONS>(services::abyss::AbyssRankingCache::getInstance().getLastUpdate(), queriedRace);
	} else {
		legionRanking = services::abyss::AbyssRankingCache::getInstance().getLegions(queriedRace);
		player->setAbyssRankListUpdated(updateType);
	}
	if (legionRanking == nullptr) // Java: sendPacket(null) of a race without a cached ranking: NullPointerException when it is written
		throw runtime::NullPointerException("SM_ABYSS_RANKING_LEGIONS");
	sendPacket(*legionRanking);
}

AION_CLIENT_PACKET(CM_ABYSS_RANKING_LEGIONS);

} // namespace aion::gameserver::network::aion::clientpackets
