#include "aion/gameserver/model/team/league/LeagueService.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"
#include "aion/gameserver/model/team/league/League.h"
#include "aion/gameserver/model/team/league/LeagueMember.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::team::league {

// The league lane's bodies (m5g-plan.md LG-02); shells of the M5g parties lane's stage-0 headers (I-02a).

void LeagueService::inviteToLeague(gameobjects::player::Player& inviter, gameobjects::player::Player& invited) {
	AION_UNPORTED();
}

bool LeagueService::canInvite(gameobjects::player::Player& inviter, gameobjects::player::Player& invited) {
	AION_UNPORTED();
}

runtime::Ptr<League> LeagueService::createLeague(gameobjects::player::Player& leader) {
	AION_UNPORTED();
}

void LeagueService::addAlliance(League& league, alliance::PlayerAlliance& alliance) {
	AION_UNPORTED();
}

void LeagueService::removeAlliance(runtime::Ptr<alliance::PlayerAlliance> alliance) {
	AION_UNPORTED();
}

void LeagueService::expelAlliance(LeagueMember& leagueAlliance, gameobjects::player::Player& leagueLeader) {
	AION_UNPORTED();
}

void LeagueService::setLeader(gameobjects::player::Player& player, gameobjects::player::Player& allianceLeader) {
	AION_UNPORTED();
}

void LeagueService::disband(League& league) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<League>> LeagueService::getLeagues() {
	AION_UNPORTED();
}

void LeagueService::moveAlliance(gameobjects::player::Player& player, int32_t selectedId, int32_t targetId) {
	AION_UNPORTED();
}

void LeagueService::changeGroupRules(League& league, common::legacy::LootGroupRules& lootRules) {
	AION_UNPORTED();
}

void LeagueService::distributeKinah(gameobjects::player::Player& player, int64_t amount) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::team::league
