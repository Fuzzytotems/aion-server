#pragma once

#include <cstdint>
#include <initializer_list>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/model/templates/siegelocation/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"
#include "aion/gameserver/services/panesterra/ahserion/fwd.h"
#include "aion/gameserver/services/panesterra/fwd.h"

namespace aion::gameserver::services::panesterra {

/**
 * Workflow for Panesterra sieges:
 * 1. Stop all outer bases
 * 2. Start artifacts as PEACE
 * 3. Start camps as PEACE or
 * Gab1_StartTimeCheck_03 + _04
 * STR_MSG_LDF5_Gab1_End01
 * TODO-List:
 * - Teleportation to other bases should be deactivated, if the target location is not occupied by the same faction
 * - Basic matchmaking, regarding priority slots
 * - Announcements during prep time
 * - Siege tests
 *
 * @author Estrayl
 */
class PanesterraService : public runtime::Immortal {
private:
	runtime::ConcurrentHashMap<ahserion::PanesterraFaction, runtime::Ref<ahserion::PanesterraTeam>> activeFactionTeams{
		AION_LOCK_CLASS(PanesterraService::activeFactionTeams#stripe)};
public:
	/**
	 * 1. Stop outer bases
	 * 2. Start faction camps
	 * 3. Despawn default corridors
	 * 4. Spawn corridors
	 */
	void prepareFortressSiege(model::siege::FortressLocation& loc);
private:
	void prepareBases(const model::templates::siegelocation::SiegeRelatedBases* relatedBases);
public:
	void startFortressSiege(model::siege::FortressLocation& loc);
	void stopFortressSiege(model::siege::FortressLocation& loc);
private:
	/**
	 * Up to 100 players from each faction, with a rank of 1-Star Officer or higher, can apply for the fortress siege.
	 * Out of those 100 slots, 1 will be reserved for the Governor, 50 will be reserved for 5-Star Officers and above, and
	 * the remaining 49 slots will be reserved for 1-Star Officers and above.
	 * The preparation time is set to 10 minutes, with the first 5 minutes being dedicated to the ranked applications
	 * and the remaining 5 minutes being open to all eligible applications.
	 * <p/>
	 * While some sources suggest that every slot except the Governor is randomly assigned, the matchmaking system has made
	 * sure to guarantee basic group formations by assigning at least one tank and one healer per theoretical group.
	 * <br/>
	 * <br/>
	 * Note: Starting as of v4.9 Rank-1 players were also allowed to apply after the initial 5 minutes.
	 */
	void spawnAdvanceCorridors();
	void spawnCorridor(model::templates::spawns::SpawnTemplate& template_, int32_t staticId);
public:
	void startAhserionRaid();
	void stopAhserionRaid();
	runtime::Ptr<ahserion::PanesterraTeam> handleTeamElimination(ahserion::PanesterraFaction faction);
private:
	void createTeams(int32_t siegeId);
	void removeTeams(std::initializer_list<ahserion::PanesterraFaction> factions = {});
	void spawnAhserionCorridors(int32_t fortressId);
public:
	void onEnterPanesterra(model::gameobjects::player::Player& player);
private:
	int32_t getSiegeId(int32_t worldId);
public:
	bool isAhserionRaidStarted();
	int32_t getTeamMemberCount(ahserion::PanesterraFaction faction);
	runtime::Ptr<ahserion::PanesterraTeam> getTeam(ahserion::PanesterraFaction faction);
	runtime::Ptr<ahserion::PanesterraTeam> getTeam(model::gameobjects::player::Player& player);
	bool teleportToStartPosition(model::gameobjects::player::Player& player);
	bool reviveInEventLocation(model::gameobjects::player::Player& player);
	void teleportToEventLocation(model::gameobjects::player::Player& player);
private:
	void teleport(model::gameobjects::player::Player& player);
public:
	static PanesterraService& getInstance(); // Java singleton
};

} // namespace aion::gameserver::services::panesterra
