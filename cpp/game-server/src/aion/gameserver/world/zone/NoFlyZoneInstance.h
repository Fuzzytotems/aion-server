#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/templates/zone/fwd.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::world::zone {

/** Zone in which flying is not allowed: sets the NO_FLY inside zone type while a creature is inside. */
class NoFlyZoneInstance : public ZoneInstance {
	AION_MAKE_REF_FRIEND
protected:
	NoFlyZoneInstance(int32_t mapId, model::templates::zone::ZoneInfo& template_);
	~NoFlyZoneInstance() override;

public:
	/** Java: new NoFlyZoneInstance(mapId, template) */
	static runtime::Ref<NoFlyZoneInstance> create(int32_t mapId, model::templates::zone::ZoneInfo& template_);

	bool onEnter(model::gameobjects::Creature& creature) override; // synchronized

	bool onLeave(model::gameobjects::Creature& creature) override; // synchronized
};

} // namespace aion::gameserver::world::zone
