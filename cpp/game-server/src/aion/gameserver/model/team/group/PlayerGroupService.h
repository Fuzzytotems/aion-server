#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/common/legacy/fwd.h"
#include "aion/gameserver/model/team/fwd.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/model/team/group/fwd.h"

namespace aion::gameserver::model::team::group {

/**
 * C++: a static-only class (hub-headers.md §11.1). Written for M5a (plan E1-05): onPlayerLogin and onPlayerLogout are ported for a player
 * without a group; every other body stays AION_UNPORTED until the team work of M5b. The Runnable OfflinePlayerChecker is used only by the
 * bodies and defined in the .cpp (hub-headers.md §9.3).
 *
 * @author ATracer
 */
class PlayerGroupService {
public:
	class OfflinePlayerChecker;

private:
	static inline runtime::ConcurrentHashMap<int32_t, runtime::Ref<PlayerGroup>> groups{AION_LOCK_CLASS(PlayerGroupService::groups#stripe)}; // Java: = new ConcurrentHashMap<>()
	static inline runtime::AtomicBoolean offlineCheckStarted{AION_LOCK_CLASS(PlayerGroupService::offlineCheckStarted)}; // Java: = new AtomicBoolean()

public:
	PlayerGroupService() = delete; // Java: a class with static methods only
	static void inviteToGroup(gameobjects::player::Player& inviter, gameobjects::player::Player& invited);
	static runtime::Ptr<PlayerGroup> createGroup(gameobjects::player::Player& leader, gameobjects::player::Player& invited, TeamType type, int32_t id);
private:
	static void initializeOfflineCheck();
public:
	static void addPlayerToGroup(PlayerGroup& group, gameobjects::player::Player& invited);
	/** Change group's loot rules and notify team members */
	static void changeGroupRules(PlayerGroup& group, common::legacy::LootGroupRules& lootRules);
	/** Player entered world - search for non expired group */
	static void onPlayerLogin(gameobjects::player::Player& player);
	/** Player leaved world - set last online on member */
	static void onPlayerLogout(gameobjects::player::Player& player);
	/** Update group members to some event of player */
	static void updateGroup(gameobjects::player::Player& player, common::legacy::GroupEvent groupEvent);
	static void updateGroupEffects(gameobjects::player::Player& player, int32_t slot);
	/** Add player to group */
	static void addPlayer(PlayerGroup& group, gameobjects::player::Player& player);
	/** Remove player from group (normal leave, or kick offline player) */
	static void removePlayer(gameobjects::player::Player& player);
	/** Remove player from group (ban) */
	static void banPlayer(gameobjects::player::Player& bannedPlayer, gameobjects::player::Player& banGiver);
	/** Disband group by removing all players one by one */
	static void disband(PlayerGroup& group);
	/** Share specific amount of kinah between group members */
	static void distributeKinah(gameobjects::player::Player& player, int64_t kinah);
	static void changeLeader(gameobjects::player::Player& player);
	/** Start mentoring in group */
	static void startMentoring(gameobjects::player::Player& player);
	/** Stop mentoring in group */
	static void stopMentoring(gameobjects::player::Player& player);
	static runtime::Ptr<PlayerGroup> searchGroup(int32_t playerObjId);
};

} // namespace aion::gameserver::model::team::group
