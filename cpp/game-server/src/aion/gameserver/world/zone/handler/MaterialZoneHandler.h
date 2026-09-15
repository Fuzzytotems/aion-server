#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/geoEngine/scene/fwd.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/templates/materials/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"
#include "aion/gameserver/world/zone/handler/ZoneHandler.h"

namespace aion::gameserver::world::zone::handler {

/**
 * Zone handler of a geo material mesh: applies the material skills to creatures inside the mesh (ZoneCollisionMaterialActor).
 * <p>
 * C++: RefCounted (fieldmap K4) and the first ZoneHandler implementor of its hierarchy, so it forwards retain()/release() (hub-headers.md §9.2).
 * Created with create() by ZoneService.createMaterialZoneTemplate.
 *
 * @author Rolandas
 */
class MaterialZoneHandler : public runtime::RefCounted, public ZoneHandler {
	AION_MAKE_REF_FRIEND
private:
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<controllers::observer::AbstractMaterialSkillActor>> observed{
		AION_LOCK_CLASS(MaterialZoneHandler::observed#stripe)};
	const runtime::Ref<geoEngine::scene::Spatial> geometry;
	const model::templates::materials::MaterialTemplate* template_;
	const model::Race ownerRace;

protected:
	MaterialZoneHandler(geoEngine::scene::Spatial& geometry, const model::templates::materials::MaterialTemplate* template_);
	~MaterialZoneHandler() override;

public:
	/** Java: new MaterialZoneHandler(geometry, template) */
	static runtime::Ref<MaterialZoneHandler> create(geoEngine::scene::Spatial& geometry, const model::templates::materials::MaterialTemplate* template_);

	void onEnterZone(model::gameobjects::Creature& creature, ZoneInstance& zone) override;

	void onLeaveZone(model::gameobjects::Creature& creature, ZoneInstance& zone) override;

	/** C++ only: ZoneHandler held by Ref retains this object (hub-headers.md §9.2). */
	void retain() const noexcept override { runtime::RefCounted::retain(); }
	/** C++ only: ZoneHandler held by Ref releases this object (hub-headers.md §9.2). */
	void release() const noexcept override { runtime::RefCounted::release(); }
};

} // namespace aion::gameserver::world::zone::handler
