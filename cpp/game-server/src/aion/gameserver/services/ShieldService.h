#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/geoEngine/scene/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/model/templates/shield/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * @author xavier, Rolandas, SVDNESS
 */
class ShieldService : public runtime::Immortal {
private:
	static inline runtime::HashMap<int32_t, runtime::Ref<runtime::RcHashSet<std::string>>> IGNORED_SHIELDS_BY_MAP_ID{
		AION_LOCK_CLASS(ShieldService::IGNORED_SHIELDS_BY_MAP_ID)};
	runtime::ConcurrentHashMap<int32_t, const model::templates::shield::ShieldTemplate*> sphereShields{
		AION_LOCK_CLASS(ShieldService::sphereShields#stripe)};
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<runtime::RcArrayList<runtime::Ref<model::siege::SiegeShield>>>> registeredShields{
		AION_LOCK_CLASS(ShieldService::registeredShields#stripe)};
	ShieldService();
public:
	void logDetachedShields();
	runtime::Ref<controllers::observer::ShieldObserver> createShieldObserver(model::siege::FortressLocation& location,
		model::gameobjects::Creature& observed);
	/** Registers geo shield for zone lookup */
	runtime::Ptr<model::siege::SiegeShield> tryRegisterShield(int32_t worldId, geoEngine::scene::Spatial& geometry);
	/**
	 * Attaches geo shield and removes obsolete sphere shield if such exists. Should be called when geo shields and SiegeZoneInstance were created.
	 */
	void attachShield(model::siege::SiegeLocation& location);
private:
	bool isShieldInsideLocation(model::siege::SiegeShield& shield, model::siege::SiegeLocation& location);
	bool isIgnored(int32_t mapId, std::string_view geometryName);
public:
	static ShieldService& getInstance(); // Java singleton
};

} // namespace aion::gameserver::services
