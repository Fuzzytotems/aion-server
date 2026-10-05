#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/alliance/fwd.h"
#include "aion/gameserver/model/team/common/legacy/fwd.h"
#include "aion/gameserver/model/team/league/League.h"
#include "aion/gameserver/model/team/league/fwd.h"

namespace aion::gameserver::model::team::league {

/**
 * C++: a static-only class (hub-headers.md §11.1). Declaration header and `AION_UNPORTED` shell written by the M5g parties lane (m5g-plan.md
 * I-02a, header request m5g-12) so the party packets and PlayerTeamCommandService link against the league arms; the bodies are the league
 * lane's (P5-10d, LG-02). Java passes a possibly null alliance to removeAlliance (PlayerTeamCommandService.java:69, checked at :104).
 *
 * @author ATracer
 */
class LeagueService {
private:
	static inline runtime::ConcurrentHashMap<int32_t, runtime::Ref<League>> leagues{AION_LOCK_CLASS(LeagueService::leagues#stripe)}; // Java: = new ConcurrentHashMap<>()

public:
	LeagueService() = delete; // Java: a class with static methods only
	static void inviteToLeague(gameobjects::player::Player& inviter, gameobjects::player::Player& invited);
	static bool canInvite(gameobjects::player::Player& inviter, gameobjects::player::Player& invited);
	static runtime::Ptr<League> createLeague(gameobjects::player::Player& leader);
	/** Add alliance to league */
	static void addAlliance(League& league, alliance::PlayerAlliance& alliance);
	/** Remove alliance from league (normal leave) */
	static void removeAlliance(runtime::Ptr<alliance::PlayerAlliance> alliance);
	/** Remove alliance from league (expel) */
	static void expelAlliance(LeagueMember& leagueAlliance, gameobjects::player::Player& leagueLeader);
	static void setLeader(gameobjects::player::Player& player, gameobjects::player::Player& allianceLeader);
	/** Disband league after minimum of members has been reached */
	static void disband(League& league);
	/** Java: an unmodifiable view of the leagues - a snapshot */
	static std::vector<runtime::Ptr<League>> getLeagues();
	static void moveAlliance(gameobjects::player::Player& player, int32_t selectedId, int32_t targetId);
	static void changeGroupRules(League& league, common::legacy::LootGroupRules& lootRules);
	static void distributeKinah(gameobjects::player::Player& player, int64_t amount);
};

} // namespace aion::gameserver::model::team::league
