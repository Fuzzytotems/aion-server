#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"
#include "aion/gameserver/world/zone/handler/ZoneHandler.h"

namespace aion::gameserver::world::zone::handler {

/**
 * The zone handler of zones without a registered handler (ZoneService falls back to it); does nothing.
 * <p>
 * Written in spine step S0b as the direct base of the hub QuestZoneHandler (docs/design/hub-headers.md §3.1). The first ZoneHandler
 * implementor with a runtime base: RefCounted, forwards retain()/release() (§9.2). The empty Java bodies are ported.
 *
 * @author MrPoke
 */
class GeneralZoneHandler : public runtime::RefCounted, public ZoneHandler {
	AION_MAKE_REF_FRIEND
protected:
	GeneralZoneHandler() = default;
	~GeneralZoneHandler() override;

public:
	/** Java: new GeneralZoneHandler() */
	static runtime::Ref<GeneralZoneHandler> create();

	void onEnterZone(model::gameobjects::Creature& player, ZoneInstance& zone) override {}

	void onLeaveZone(model::gameobjects::Creature& player, ZoneInstance& zone) override {}

	/** C++ only: ZoneHandler held by Ref retains this object (hub-headers.md §9.2). */
	void retain() const noexcept override { runtime::RefCounted::retain(); }
	/** C++ only: ZoneHandler held by Ref releases this object (hub-headers.md §9.2). */
	void release() const noexcept override { runtime::RefCounted::release(); }
};

} // namespace aion::gameserver::world::zone::handler
