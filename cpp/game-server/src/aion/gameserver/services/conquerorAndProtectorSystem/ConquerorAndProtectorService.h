#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/cp/fwd.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::services::conquerorAndProtectorSystem {

/**
 * Since 4.8 there is no serial killer system anymore, but a conqueror and protector system. See
 * https://aionpowerbook.com/powerbook/Conqueror_and_Protector_System
 * <p>
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 *
 * @author Source, Dtem, ginho, Yeats, Neon
 */
class ConquerorAndProtectorService : public runtime::Immortal {
private:
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<CPInfo>> conquerors{AION_LOCK_CLASS(ConquerorAndProtectorService::conquerors#stripe)}; // Java: = new ConcurrentHashMap<>()
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<CPInfo>> protectors{AION_LOCK_CLASS(ConquerorAndProtectorService::protectors#stripe)}; // Java: = new ConcurrentHashMap<>()
	runtime::ConcurrentHashMap<int32_t, int64_t> intruderScanCooldowns{AION_LOCK_CLASS(ConquerorAndProtectorService::intruderScanCooldowns#stripe)}; // Java: = new ConcurrentHashMap<>()
	runtime::HashMap<int32_t, model::Race> handledWorlds{AION_LOCK_CLASS(ConquerorAndProtectorService::handledWorlds)}; // Java: = new HashMap<>()
	ConquerorAndProtectorService();
	~ConquerorAndProtectorService();
public:
	void init();
	runtime::Ptr<CPInfo> getCPInfoForCurrentMap(model::gameobjects::player::Player& player);
	runtime::Ptr<CPInfo> getCPInfoForCurrentMap(model::gameobjects::player::Player& player, bool createIfNotExists);
	void onEnterMap(model::gameobjects::player::Player& player);
	void onLeaveMap(model::gameobjects::player::Player& player);
	void onEnterZone(model::gameobjects::player::Player& player, world::zone::ZoneInstance& zone);
	void onLeaveZone(model::gameobjects::player::Player& player, world::zone::ZoneInstance& zone);
	void onLeaveLegion(model::gameobjects::player::Player& player);
private:
	void resetLegionDominionRank(model::gameobjects::player::Player& player);
	bool isOccupiedLegionDominionZone(model::gameobjects::player::Player& player, world::zone::ZoneInstance& zone);
public:
	void onKill(model::gameobjects::player::Player& killer, model::gameobjects::player::Player& victim);
	void sendDetectCooldown(model::gameobjects::player::Player& player);
private:
	void addVictims(runtime::Ptr<model::gameobjects::player::Player> player, CPInfo& info, int32_t count);
	void updateBuffAndNotifyNearbyPlayers(model::gameobjects::player::Player& player, CPInfo& cpInfo);
public:
	void intruderScan(model::gameobjects::player::Player& player);
private:
	int32_t getOrRemoveCooldown(int32_t objectId);
	std::vector<runtime::Ptr<model::gameobjects::player::Player>> findIntruders(model::gameobjects::player::Player& player);
	bool canSee(CPInfo& protector, CPInfo& intruder);
	int32_t getRank(int32_t kills);
	/** @return null outside the handled worlds (getCPInfoForCurrentMap tests it) */
	std::optional<model::templates::cp::CPType> getCPTypeForCurrentMap(model::gameobjects::player::Player& player);
public:
	static ConquerorAndProtectorService& getInstance(); // Java singleton
};

} // namespace aion::gameserver::services::conquerorAndProtectorSystem
