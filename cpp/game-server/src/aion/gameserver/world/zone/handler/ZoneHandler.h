#pragma once

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::world::zone::handler {

/**
 * Handler of a zone instance (ZoneInstance.handlers): notified when a creature enters or leaves the zone.
 * <p>
 * Hub header (docs/design/hub-headers.md §9.2): an interface held by `runtime::Ref<ZoneHandler>` (HandlerRegistry.h ZoneFactory,
 * ZoneInstance.handlers), so it declares the reference count operations; the first implementor with a runtime base forwards them
 * (GeneralZoneHandler, MaterialZoneHandler, SiegeLocation, SiegeShield, VortexLocation). Zone handlers are RefCounted (amendment §2:
 * PvPZone schedules tasks capturing `this`); registered handlers provide `static Ref<C> create()` (QuestZoneHandler: `create(int32_t questId)`).
 *
 * @author MrPoke
 */
class ZoneHandler {
public:
	virtual void onEnterZone(model::gameobjects::Creature& player, ZoneInstance& zone) = 0;

	virtual void onLeaveZone(model::gameobjects::Creature& player, ZoneInstance& zone) = 0;

	/** C++ only: Ref<ZoneHandler> retains the implementing object (hub-headers.md §9.2). */
	virtual void retain() const noexcept = 0;
	/** C++ only: Ref<ZoneHandler> releases the implementing object (hub-headers.md §9.2). */
	virtual void release() const noexcept = 0;

	virtual ~ZoneHandler() = default;

protected:
	ZoneHandler() = default;
	ZoneHandler(const ZoneHandler&) = default;
	ZoneHandler& operator=(const ZoneHandler&) = default;
};

} // namespace aion::gameserver::world::zone::handler
