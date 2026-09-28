#pragma once

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/spawnengine/fwd.h"

namespace aion::gameserver::spawnengine {

/**
 * The shift of a walker group member from the group's line (left/right and back/front).
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5): the base of ClusteredNpc. RefCounted (fieldmap K4), created with create.
 *
 * @author Rolandas
 */
class WalkerGroupShift : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<float> sagittalShift{}; // left and right (sides)
	runtime::Field<float> coronalShift{};  // or dorsoventral (back and front)

public:
	static constexpr float DISTANCE = 2; // 2 meters distance by default

protected:
	WalkerGroupShift(float leftRight, float backFront);
	~WalkerGroupShift() override;

public:
	/** Java: new WalkerGroupShift(leftRight, backFront) */
	static runtime::Ref<WalkerGroupShift> create(float leftRight, float backFront);

	/**
	 * left and right (sides)
	 */
	float getSagittalShift() const { return sagittalShift.get(); }

	/**
	 * dorsoventral (back and front)
	 */
	float getCoronalShift() const { return coronalShift.get(); }

	void set(WalkerGroupShift& shift);
};

} // namespace aion::gameserver::spawnengine
