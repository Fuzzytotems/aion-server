#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/model/animations/fwd.h"
#include "aion/gameserver/model/flyring/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::controllers {

/**
 * Adds a FlyRingObserver to every player who sees the fly ring and removes it again when the player no longer sees it.
 * <p>
 * The controller part of FlyRing (late-bound by setOwner). Binds VisibleObjectController's type variable to FlyRing: getOwner() returns
 * `FlyRing&` (hub-headers.md §8.2). `observed` is the fieldmap spelling of Java's package-private non-final ConcurrentHashMap (created in the
 * constructor, like Java's field initializer).
 *
 * @author xavier
 */
class FlyRingController : public VisibleObjectController {
public:
	runtime::Field<runtime::Ref<runtime::RcConcurrentHashMap<int32_t, runtime::Ref<observer::FlyRingObserver>>>> observed{};

	FlyRingController();
	~FlyRingController() override;

	/** Narrowing accessor (Java: VisibleObjectController<FlyRing>.getOwner(), hub-headers.md §8.2). */
	model::flyring::FlyRing& getOwner() const;

	void see(model::gameobjects::VisibleObject& object) override;

	void notSee(model::gameobjects::VisibleObject& object, model::animations::ObjectDeleteAnimation animation) override;
};

} // namespace aion::gameserver::controllers
