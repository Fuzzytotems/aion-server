#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/alliance/events/AssignViceCaptainEvent_AssignType.h"
#include "aion/gameserver/model/team/alliance/fwd.h"
#include "aion/gameserver/model/team/common/events/fwd.h"
#include "aion/gameserver/model/team/common/legacy/fwd.h"
#include "aion/gameserver/model/team/fwd.h"

namespace aion::gameserver::model::team::alliance {

/**
 * C++: a static-only class (hub-headers.md §11.1). Written for M5a (plan E1-05): onPlayerLogin and onPlayerLogout are ported for a player
 * without an alliance; every other body stays AION_UNPORTED until the team work of M5b. The Runnable OfflinePlayerAllianceChecker is used only
 * by the bodies and defined in the .cpp (hub-headers.md §9.3).
 *
 * @author ATracer
 */
class PlayerAllianceService {
public:
	class OfflinePlayerAllianceChecker;

private:
	static inline runtime::ConcurrentHashMap<int32_t, runtime::Ref<PlayerAlliance>> alliances{AION_LOCK_CLASS(PlayerAllianceService::alliances#stripe)}; // Java: = new ConcurrentHashMap<>()
	static inline runtime::AtomicBoolean offlineCheckStarted{AION_LOCK_CLASS(PlayerAllianceService::offlineCheckStarted)}; // Java: = new AtomicBoolean()

public:
	PlayerAllianceService() = delete; // Java: a class with static methods only
	static void inviteToAlliance(gameobjects::player::Player& inviter, gameobjects::player::Player& invited);
	static runtime::Ptr<PlayerAlliance> createAlliance(gameobjects::player::Player& leader, gameobjects::player::Player& invited, TeamType type);
private:
	static void initializeOfflineCheck();
public:
	static runtime::Ptr<PlayerAllianceMember> addPlayerToAlliance(PlayerAlliance& alliance, gameobjects::player::Player& invited);
	/** Change alliance's loot rules and notify team members */
	static void changeGroupRules(PlayerAlliance& alliance, common::legacy::LootGroupRules& lootRules);
	/** Player entered world - search for non expired alliance */
	static void onPlayerLogin(gameobjects::player::Player& player);
	/** Player leaved world - set last online on member */
	static void onPlayerLogout(gameobjects::player::Player& player);
	/** Update alliance members to some event of player */
	static void updateAlliance(gameobjects::player::Player& player, common::legacy::PlayerAllianceEvent allianceEvent);
	static void updateAllianceEffects(gameobjects::player::Player& player, int32_t slot);
	/** Add player to alliance */
	static void addPlayer(PlayerAlliance& alliance, gameobjects::player::Player& player);
	/** Remove player from alliance (normal leave, or kick offline player) */
	static void removePlayer(gameobjects::player::Player& player);
	/** Remove player from alliance (ban) */
	static void banPlayer(gameobjects::player::Player& bannedPlayer, gameobjects::player::Player& banGiver);
	/** Disband alliance after minimum of members has been reached */
	static void disband(PlayerAlliance& alliance, bool onBefore);
	static void changeLeader(gameobjects::player::Player& player);
	/** Change vice captain position of player (promote, demote) */
	static void changeViceCaptain(gameobjects::player::Player& player, events::AssignViceCaptainEvent_AssignType assignType);
	static runtime::Ptr<PlayerAlliance> searchAlliance(int32_t playerObjId);
	/** Move members between alliance groups */
	static void changeMemberGroup(gameobjects::player::Player& player, int32_t firstPlayer, int32_t secondPlayer, int32_t allianceGroupId);
	/** Check that alliance is ready */
	static void checkReady(gameobjects::player::Player& player, common::events::TeamCommand eventCode);
	/** Share specific amount of kinah between alliance members */
	static void distributeKinah(gameobjects::player::Player& player, int64_t amount);
	static void distributeKinahInGroup(gameobjects::player::Player& player, int64_t amount);
};

} // namespace aion::gameserver::model::team::alliance
