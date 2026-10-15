#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/vortex/fwd.h"
#include "aion/gameserver/services/vortex/fwd.h"

namespace aion::gameserver::services::vortex {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 * Java generic DimensionalVortex<VL>: erased to a non-template class, type variables spelled as their bounds (hub-headers.md §8.1).
 *
 * @author Source
 */
class DimensionalVortex : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<model::vortex::VortexLocation> vortexLocation;
	runtime::AtomicBoolean finished{AION_LOCK_CLASS(DimensionalVortex::finished)}; // Java: = new AtomicBoolean()
	runtime::Field<bool> started{};

protected:
	virtual void startInvasion() = 0;

	virtual void stopInvasion() = 0;

public:
	virtual void addPlayer(model::gameobjects::player::Player& player, bool isInvader) = 0;

	virtual void kickPlayer(model::gameobjects::player::Player& player, bool isInvader) = 0;

	virtual void updateDefenders(model::gameobjects::player::Player& defender) = 0;

	virtual void updateInvaders(model::gameobjects::player::Player& invader) = 0;

	virtual std::unordered_map<int32_t, runtime::Ptr<model::gameobjects::player::Player>> getDefenders() = 0;

	virtual std::unordered_map<int32_t, runtime::Ptr<model::gameobjects::player::Player>> getInvaders() = 0;

protected:
	explicit DimensionalVortex(model::vortex::VortexLocation& vortexLocation);

public:
	void start();

	void stop();

protected:
	void initRiftGenerator();

	void spawn(model::vortex::VortexStateType type);

	void despawn();

public:
	bool isFinished();

	runtime::Ptr<model::vortex::VortexLocation> getVortexLocation() const { return this->vortexLocation; }

	int32_t getVortexLocationId();

protected:
	~DimensionalVortex() override;
};

} // namespace aion::gameserver::services::vortex
