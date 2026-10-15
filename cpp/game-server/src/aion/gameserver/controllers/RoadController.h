#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/model/animations/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/road/fwd.h"

namespace aion::gameserver::controllers {

/**
 * Adds a RoadObserver to every player who sees the road and removes it again when the player no longer sees it.
 * <p>
 * The controller part of Road (late-bound by setOwner). Binds VisibleObjectController's type variable to Road: getOwner() returns `Road&`
 * (hub-headers.md §8.2). `observed` is the fieldmap spelling of Java's package-private non-final ConcurrentHashMap (created in the constructor).
 *
 * @author SheppeR
 */
class RoadController : public VisibleObjectController {
public:
	runtime::Field<runtime::Ref<runtime::RcConcurrentHashMap<int32_t, runtime::Ref<observer::RoadObserver>>>> observed{};

	RoadController();
	~RoadController() override;

	/** Narrowing accessor (Java: VisibleObjectController<Road>.getOwner(), hub-headers.md §8.2). */
	model::road::Road& getOwner() const;

	void see(model::gameobjects::VisibleObject& object) override;

	void notSee(model::gameobjects::VisibleObject& object, model::animations::ObjectDeleteAnimation animation) override;
};

} // namespace aion::gameserver::controllers
