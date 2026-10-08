#include "aion/gameserver/services/abyss/AbyssService.h"

#include <cstdint>

#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/stats/AbyssRankEnum.h"

namespace aion::gameserver::services::abyss {

using model::gameobjects::player::Player;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

bool AbyssService::shouldAnnounceHighRankedDeath(Player& victim) {
	// Java: victim.getAbyssRank().getRank().getId() >= AbyssRankEnum.GRADE1_SOLDIER.getId(). getId() is the ordinal + 1 for every constant of
	// the enum, so the ordinals are compared (the shape AbyssSkillService.cpp:72 already uses for the same enum).
	if (static_cast<int32_t>(victim.getAbyssRank()->getRank()) >= static_cast<int32_t>(utils::stats::AbyssRankEnum::GRADE1_SOLDIER)) {
		for (int32_t map : killAnnounceMaps) {
			if (map == victim.getWorldId())
				return true;
		}
	}
	return false;
}

void AbyssService::announceHighRankedDeath(Player& victim) {
	if (!shouldAnnounceHighRankedDeath(victim))
		return;
	// Java: p -> p != victim && victim.getWorldType() == p.getWorldType() && !p.isInInstance() - `p != victim` is a reference comparison, so it
	// is spelled with the addresses and not with AionObject::equals (which compares object ids)
	utils::PacketSendUtility::broadcastToWorld(SM_SYSTEM_MESSAGE::STR_ABYSS_ORDER_RANKER_DIE(victim),
		[&victim](Player& p) { return &p != &victim && victim.getWorldType() == p.getWorldType() && !p.isInInstance(); });
}

// Java AbyssService.java:33-36. `p != player` is a reference comparison (the addresses), as announceHighRankedDeath's
void AbyssService::announceAbyssSkillUsage(model::gameobjects::player::Player& player, std::string_view skillL10n) {
	utils::PacketSendUtility::broadcastToWorld(SM_SYSTEM_MESSAGE::STR_SKILL_ABYSS_SKILL_IS_FIRED(player, skillL10n),
		[&player](Player& p) { return &p != &player && player.getWorldType() == p.getWorldType() && !p.isInInstance(); });
}

} // namespace aion::gameserver::services::abyss
