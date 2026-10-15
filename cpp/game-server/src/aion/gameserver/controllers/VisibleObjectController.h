#pragma once

#include "aion/gameserver/runtime/fields/Final.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/animations/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::controllers {

/**
 * This class is for controlling VisibleObjects [players, npc's etc]. Its controlling movement, visibility etc.
 * <p>
 * Hub header (docs/design/hub-headers.md). Java `VisibleObjectController<T extends VisibleObject>` is one non-template class (erasure rule
 * §8.1): `T` is spelled `VisibleObject`, and each binding subclass narrows getOwner() (`NpcController::getOwner()` returns `Npc&`, §8.2).
 * A late-bound part (runtime-architecture.md §2.3 pattern 3): the owner's constructor passes the controller to `VisibleObject`, and the
 * owner's postConstruct() calls setOwner(*this), which binds the part to its owner (OwnedPart::bindOwner) before publication.
 *
 * @author -Nemesiss-
 */
class VisibleObjectController : public runtime::OwnedPart {
private:
	/** Object that is controlled by this controller. */
	runtime::Final<model::gameobjects::VisibleObject*> owner{};

protected:
	/** Java: the implicit constructor of the abstract class (the owner is bound later by setOwner). */
	VisibleObjectController() noexcept;

public:
	~VisibleObjectController() override;

	/** Set owner (controller object). Binds the part to its owner (hub-headers.md §10.3); called once, before the owner is published. */
	void setOwner(model::gameobjects::VisibleObject& owner);

	/** Get owner (controller object). Java final; subclasses narrow it non-virtually. */
	model::gameobjects::VisibleObject& getOwner() const { return *owner.get(); }

	/** Called when controlled object is seeing other VisibleObject. */
	virtual void see(model::gameobjects::VisibleObject& object) {}

	/** Called when controlled object no longer sees some other VisibleObject. */
	virtual void notSee(model::gameobjects::VisibleObject& object, model::animations::ObjectDeleteAnimation animation) {}

	/** Called when controlled object no longer knows some other VisibleObject. */
	virtual void notKnow(model::gameobjects::VisibleObject& object) {}

	/**
	 * Despawns the object (if spawned) and deletes it from the world
	 *
	 * @see World#removeObject(VisibleObject)
	 */
	bool delete_();

	/**
	 * Despawns the object (if spawned) and deletes it from the world. If allowed, a respawn task will be scheduled on successful deletion.
	 *
	 * @see World#removeObject(VisibleObject)
	 */
	void deleteAndScheduleRespawn();

	/**
	 * Despawns the object and deletes it from the world if alive. Otherwise, cancels its respawn task if present.
	 *
	 * @see #delete()
	 */
	void deleteIfAliveOrCancelRespawn();

	/**
	 * Both parameters are nullable: VisibleObject.setTarget passes the new target for both (it assigns the field before the call), and that is
	 * null when the target is cleared (`setTarget(null)`).
	 */
	virtual void onTargetChanged(runtime::Ptr<model::gameobjects::VisibleObject> oldTarget,
		runtime::Ptr<model::gameobjects::VisibleObject> newTarget) {}

	/** Called before object is placed into world */
	virtual void onBeforeSpawn();

	/** Called after object was placed into world */
	virtual void onAfterSpawn() {}

	/** Called before object despawns */
	virtual void onDespawn();

	/**
	 * Called before object gets removed from the world
	 * <p>
	 * Java: empty. C++: calls `model::gameobjects::player::LogoutBreakers::onDelete(getOwner())` (the delete breakers, cycles.toml; noexcept, the
	 * steps log a failing step); every override reaches it through the base call.
	 */
	virtual void onDelete();
};

} // namespace aion::gameserver::controllers
