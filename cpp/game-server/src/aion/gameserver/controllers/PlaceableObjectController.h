#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::controllers {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 * Java generic PlaceableObjectController<T>: erased to a non-template class, type variables spelled as their bounds (hub-headers.md §8.1).
 *
 * @author Rolandas
 */
class PlaceableObjectController : public VisibleObjectController {
public:
	void onDespawn() override;

	void onDialogRequest(model::gameobjects::player::Player& player);

	void notKnow(model::gameobjects::VisibleObject& object) override;
};

} // namespace aion::gameserver::controllers
