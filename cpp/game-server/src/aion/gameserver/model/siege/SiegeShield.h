#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/geoEngine/scene/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"
#include "aion/gameserver/world/zone/handler/ZoneHandler.h"

namespace aion::gameserver::model::siege {

/**
 * Shields have material ID 11 in geo.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author Rolandas
 */
class SiegeShield : public runtime::RefCounted, public world::zone::handler::ZoneHandler {
	AION_MAKE_REF_FRIEND
private:
	// Java: = new ConcurrentHashMap<>()
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<controllers::observer::ActionObserver>> observed{AION_LOCK_CLASS(SiegeShield::observed#stripe)};
	const runtime::Ref<geoEngine::scene::Spatial> geometry;
	runtime::Field<int32_t> siegeLocationId{};

protected:
	explicit SiegeShield(geoEngine::scene::Spatial& geometry);

public:
	static runtime::Ref<SiegeShield> create(geoEngine::scene::Spatial& value);

	runtime::Ptr<geoEngine::scene::Spatial> getGeometry() const { return this->geometry; }

	void onEnterZone(gameobjects::Creature& creature, world::zone::ZoneInstance& zone) override;

	void onLeaveZone(gameobjects::Creature& creature, world::zone::ZoneInstance& zone) override;

	void setSiegeLocationId(int32_t siegeLocationId);

	std::string toString();

	/** C++ only: ZoneHandler retain the object itself. */
	void retain() const noexcept override { runtime::RefCounted::retain(); }

	void release() const noexcept override { runtime::RefCounted::release(); }

protected:
	~SiegeShield() override;
};

} // namespace aion::gameserver::model::siege
