#pragma once

#include <cstdint>
#include <functional>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/geometry/fwd.h"
#include "aion/gameserver/model/templates/zone/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"
#include "aion/gameserver/world/zone/handler/fwd.h"

namespace aion::gameserver::world::zone {

/**
 * A zone of a world map (one per map id and zone template, shared by the map regions it intersects).
 * <p>
 * Hub header (docs/design/hub-headers.md). RefCounted (fieldmap K4): created with `ZoneInstance::create(mapId, template)` (Java
 * `new ZoneInstance(...)`); subclasses (FlyZoneInstance, PvPZoneInstance, ...) override the synchronized onEnter/onLeave. `creatures` retains
 * the creatures inside the zone until onLeave (cycles_report.md).
 *
 * @author ATracer
 */
class ZoneInstance : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	/** C++: trailing underscore (keyword rule) */
	const runtime::Ref<model::templates::zone::ZoneInfo> template_;
	const int32_t mapId;

protected:
	runtime::HashMap<int32_t, runtime::Ref<model::gameobjects::Creature>> creatures{};
	runtime::Field<runtime::Ref<runtime::RcArrayList<runtime::Ref<handler::ZoneHandler>>>> handlers{};

	ZoneInstance(int32_t mapId, model::templates::zone::ZoneInfo& template_);
	~ZoneInstance() override;

public:
	/** Java: new ZoneInstance(mapId, template) */
	static runtime::Ref<ZoneInstance> create(int32_t mapId, model::templates::zone::ZoneInfo& template_);

	/** @return the template */
	runtime::Ptr<model::geometry::Area> getAreaTemplate();

	/** @return the template */
	const model::templates::zone::ZoneTemplate* getZoneTemplate();

	bool revalidate(model::gameobjects::Creature& creature);

	virtual bool onEnter(model::gameobjects::Creature& creature); // synchronized

	virtual bool onLeave(model::gameobjects::Creature& creature); // synchronized

	bool onDie(model::gameobjects::Creature& attacker, model::gameobjects::Creature& target);

	bool isInsideCreature(model::gameobjects::Creature& creature);

	bool isInsideCordinate(float x, float y, float z);

	void addHandler(handler::ZoneHandler& handler);

	bool canFly();

	bool canGlide();

	bool canPutKisk();

	bool canRecall();

	bool canReturnToBattle();

	bool canRide();

	bool canFlyRide();

	bool isPvpAllowed();

	bool isSameRaceDuelsAllowed();

	bool isOtherRaceDuelsAllowed();

	int32_t getTownId();

	void forEach(const std::function<void(model::gameobjects::Creature&)>& action);

	bool isDominionZone();
};

} // namespace aion::gameserver::world::zone
