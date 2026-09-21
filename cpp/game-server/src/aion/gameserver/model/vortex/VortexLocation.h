#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/vortex/fwd.h"
#include "aion/gameserver/model/vortex/fwd.h"
#include "aion/gameserver/services/vortex/fwd.h"
#include "aion/gameserver/world/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"
#include "aion/gameserver/world/zone/handler/ZoneHandler.h"

namespace aion::gameserver::model::vortex {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author Source
 */
class VortexLocation : public runtime::RefCounted, public world::zone::handler::ZoneHandler {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<bool> isActive_{};
	runtime::Field<runtime::Ref<services::vortex::DimensionalVortex>> activeVortex{};
	runtime::Field<runtime::Ref<controllers::RVController>> vortexController{};
	const templates::vortex::VortexTemplate* template_;
	runtime::ArrayList<runtime::Ref<world::zone::InvasionZoneInstance>> zones{AION_LOCK_CLASS(VortexLocation::zones)}; // Java: = new ArrayList<>()
	runtime::HashMap<int32_t, runtime::Ref<gameobjects::player::Player>> players{AION_LOCK_CLASS(VortexLocation::players)}; // Java: = new HashMap<>()
	runtime::HashMap<int32_t, runtime::Ref<gameobjects::Kisk>> kisks{AION_LOCK_CLASS(VortexLocation::kisks)}; // Java: = new HashMap<>()
	runtime::ArrayList<runtime::Ref<gameobjects::VisibleObject>> spawned{AION_LOCK_CLASS(VortexLocation::spawned)}; // Java: = new ArrayList<>()

protected:
	explicit VortexLocation(const templates::vortex::VortexTemplate* template_);

public:
	static runtime::Ref<VortexLocation> create(const templates::vortex::VortexTemplate* value);

	bool isActive() const { return this->isActive_.get(); }

	void setActiveVortex(runtime::Ptr<services::vortex::DimensionalVortex> vortex);

	runtime::Ptr<services::vortex::DimensionalVortex> getActiveVortex() const { return this->activeVortex.get(); }

	void setVortexController(runtime::Ptr<controllers::RVController> controller);

	runtime::Ptr<controllers::RVController> getVortexController() const { return this->vortexController.get(); }

	const templates::vortex::VortexTemplate* getTemplate() const { return this->template_; }

	runtime::Ptr<world::WorldPosition> getHomePoint();

	runtime::Ptr<world::WorldPosition> getResurrectionPoint();

	runtime::Ptr<world::WorldPosition> getStartPoint();

	int32_t getId();

	Race getDefendersRace();

	Race getInvadersRace();

	/**
	 * C++ only: Java writes `creature.getRace().equals(getInvadersRace())`, which is false (and never throws) when the template has no
	 * offence_race (VortexLocation.java:140, :164). getInvadersRace() throws for a missing attribute, so every comparison goes through this.
	 */
	bool isInvadersRace(Race race);

	int32_t getHomeWorldId();

	int32_t getInvasionWorldId();

	runtime::ArrayList<runtime::Ref<gameobjects::VisibleObject>>& getSpawned() { return this->spawned; }

	runtime::HashMap<int32_t, runtime::Ref<gameobjects::player::Player>>& getPlayers() { return this->players; }

	runtime::HashMap<int32_t, runtime::Ref<gameobjects::Kisk>>& getInvadersKisks() { return this->kisks; }

	bool isInvaderInside(int32_t objId);

	bool isInsideActiveVotrex(gameobjects::player::Player& player);

	void addZone(world::zone::InvasionZoneInstance& zone);

	bool isInsideLocation(gameobjects::Creature& creature);

	runtime::ArrayList<runtime::Ref<world::zone::InvasionZoneInstance>>& getZones() { return this->zones; }

	void onEnterZone(gameobjects::Creature& creature, world::zone::ZoneInstance& zone) override;

	void onLeaveZone(gameobjects::Creature& creature, world::zone::ZoneInstance& zone) override;

	/** C++ only: ZoneHandler retain the object itself. */
	void retain() const noexcept override { runtime::RefCounted::retain(); }

	void release() const noexcept override { runtime::RefCounted::release(); }

protected:
	~VortexLocation() override;
};

} // namespace aion::gameserver::model::vortex
