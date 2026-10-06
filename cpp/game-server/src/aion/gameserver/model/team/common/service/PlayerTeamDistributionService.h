#pragma once

#include "aion/gameserver/controllers/attack/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/team/common/service/fwd.h"
#include "aion/gameserver/model/team/fwd.h"

namespace aion::gameserver::model::team::common::service {

/**
 * C++: a static-only class (hub-headers.md §11.1). The private Consumer PlayerTeamRewardStats is used only by doReward and defined in the .cpp
 * (hub-headers.md §9.3); it is a K5 local of doReward.
 *
 * @author ATracer, nrg
 */
class PlayerTeamDistributionService {
public:
	class PlayerTeamRewardStats;

	PlayerTeamDistributionService() = delete; // Java: a class with static methods only

	/** This method will send a reward if a player is in a team */
	static void doReward(TemporaryPlayerTeam& team, float damagePercent, gameobjects::Npc& owner, gameobjects::AionObject& winner,
		controllers::attack::TeamDamageList& teamDamageList);
};

} // namespace aion::gameserver::model::team::common::service
