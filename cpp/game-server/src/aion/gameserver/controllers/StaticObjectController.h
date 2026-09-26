#pragma once

#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::controllers {

/**
 * The controller part of StaticObject and StaticDoor (late-bound by setOwner). Java has no body; getOwner() is narrowed to `StaticObject&`
 * (hub-headers.md §8.2).
 *
 * @author ATracer
 */
class StaticObjectController : public VisibleObjectController {
public:
	/** Java: implicit default constructor */
	StaticObjectController();
	~StaticObjectController() override;

	/** Narrowing accessor (Java: VisibleObjectController<StaticObject>.getOwner(), hub-headers.md §8.2). */
	model::gameobjects::StaticObject& getOwner() const;
};

} // namespace aion::gameserver::controllers
