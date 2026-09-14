#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::controllers::movement {

/**
 * Hub header (docs/design/hub-headers.md). Java `CreatureMoveController<T extends VisibleObject>` is one non-template class (erasure rule
 * §8.1): the owner is `OwnerRef<VisibleObject>`, and subclasses that bind T (NpcMoveController: Npc, PlayableMoveController: Creature) cast
 * where Java relied on the type variable. A part of Creature (`PartSlot<CreatureMoveController>`), bound to its owner in the constructor.
 * The members `isInMove` and `isJumping` carry a trailing underscore because the class has methods of those names (CONVENTIONS keyword rule).
 *
 * @author ATracer
 */
class CreatureMoveController : public runtime::OwnedPart {
public:
	static constexpr float MOVE_CHECK_OFFSET = 0.1f;

protected:
	runtime::OwnerRef<model::gameobjects::VisibleObject> owner; // fieldmap: OwnerRef<T>, T erased to its bound (hub-headers.md §8.1)
	runtime::Field<int8_t> heading{};
	runtime::Field<int64_t> lastMoveUpdate;
	runtime::Field<bool> isInMove_{false};
	runtime::Field<runtime::Ref<runtime::Rc<runtime::AtomicBoolean>>> started;

public:
	runtime::Field<int8_t> movementMask{};

protected:
	runtime::Field<float> targetDestX{};
	runtime::Field<float> targetDestY{};
	runtime::Field<float> targetDestZ{};

private:
	runtime::Field<bool> isJumping_{false};

protected:
	/** Java: public constructor of the abstract class (subclasses bind T: NpcMoveController, PlayableMoveController) */
	explicit CreatureMoveController(model::gameobjects::VisibleObject& owner);

public:
	~CreatureMoveController() override;

	virtual void moveToDestination() {}

	virtual float getTargetX2() { return targetDestX.get(); }

	virtual float getTargetY2() { return targetDestY.get(); }

	virtual float getTargetZ2() { return targetDestZ.get(); }

	void setNewDirection(float x, float y, float z, int8_t heading);

protected:
	virtual void setNewDirection(float x, float y, float z);

public:
	virtual void startMovingToDestination() {}

	virtual void abortMove() {}

protected:
	void setAndSendStartMove(model::gameobjects::Creature& owner);

	void setAndSendStopMove(model::gameobjects::Creature& owner);

public:
	void updateLastMove();

	int64_t getLastMoveUpdate() const { return lastMoveUpdate.get(); }

	int8_t getMovementMask() const { return movementMask.get(); }

	bool isJumping() const { return isJumping_.get(); }

	void setIsJumping(bool value) { isJumping_.set(value); }

	bool isInMove() const { return isInMove_.get(); }

	void setInMove(bool value) { isInMove_.set(value); }
};

} // namespace aion::gameserver::controllers::movement
