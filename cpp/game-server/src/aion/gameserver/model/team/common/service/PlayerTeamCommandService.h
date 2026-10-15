#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/common/events/fwd.h"
#include "aion/gameserver/model/team/common/service/fwd.h"
#include "aion/gameserver/model/team/fwd.h"
#include "aion/gameserver/model/team/league/fwd.h"

namespace aion::gameserver::model::team::common::service {

/**
 * C++: a static-only class (hub-headers.md §11.1). The alliance and league arms call their services, whose bodies are the alliance and league
 * lanes' (m5g-plan.md D10: they fail loudly until then).
 *
 * @author ATracer
 */
class PlayerTeamCommandService {
public:
	PlayerTeamCommandService() = delete; // Java: a class with static methods only

	static void executeCommand(gameobjects::player::Player& player, events::TeamCommand command, int32_t memberObjId);

private:
	static league::LeagueMember& findLeagueAlliance(TemporaryPlayerTeam& team, gameobjects::player::Player& player, int32_t leagueAllianceId);

	static gameobjects::player::Player& findMember(TemporaryPlayerTeam& team, gameobjects::player::Player& player, int32_t memberObjId);
};

} // namespace aion::gameserver::model::team::common::service
