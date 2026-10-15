#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/alliance/fwd.h"
#include "aion/gameserver/model/vortex/fwd.h"
#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/vortex/DimensionalVortex.h"

namespace aion::gameserver::services::vortex {

/**
 * Java: services.vortex.Invasion (a DimensionalVortex<VortexLocation>): the invasion of a vortex - the invaders who passed the vortex and the
 * defenders inside its location each join an alliance of their side; its end kicks the invaders out and restores the peace spawns.
 * M5i I-10.
 *
 * @author Source
 */
class Invasion : public DimensionalVortex {
	AION_MAKE_REF_FRIEND
private:
	// Java: = new ConcurrentHashMap<>()
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::gameobjects::player::Player>> invaders{AION_LOCK_CLASS(Invasion::invaders#stripe)};
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::gameobjects::player::Player>> defenders{AION_LOCK_CLASS(Invasion::defenders#stripe)};
	runtime::Field<runtime::Ref<model::team::alliance::PlayerAlliance>> invAlliance{};
	runtime::Field<runtime::Ref<model::team::alliance::PlayerAlliance>> defAlliance{};

protected:
	explicit Invasion(model::vortex::VortexLocation& vortex);

public:
	static runtime::Ref<Invasion> create(model::vortex::VortexLocation& vortex);

	void startInvasion() override;

	void stopInvasion() override;

	void addPlayer(model::gameobjects::player::Player& player, bool isInvader) override;

	void kickPlayer(model::gameobjects::player::Player& player, bool isInvader) override;

	void updateDefenders(model::gameobjects::player::Player& defender) override;

	void updateInvaders(model::gameobjects::player::Player& invader) override;

private:
	void updateAlliance();

public:
	std::unordered_map<int32_t, runtime::Ptr<model::gameobjects::player::Player>> getInvaders() override;

	std::unordered_map<int32_t, runtime::Ptr<model::gameobjects::player::Player>> getDefenders() override;

	/** C++: the inner class's read of the outer field defAlliance (Invasion$1, Invasion.java:138) */
	runtime::Ptr<model::team::alliance::PlayerAlliance> defAllianceForHandler() const { return defAlliance.get(); }

protected:
	~Invasion() override;
};

} // namespace aion::gameserver::services::vortex
