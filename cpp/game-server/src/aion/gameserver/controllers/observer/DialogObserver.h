#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::controllers::observer {

/**
 * Closes a dialog when the player moves too far away from the object serving it.
 * <p>
 * Abstract RefCounted ActionObserver (fieldmap K4). Java's anonymous subclasses (AIActions) are callback structs deriving from this class
 * (hub-headers.md §7.3).
 *
 * @author nrg
 */
class DialogObserver : public ActionObserver {
	AION_MAKE_REF_FRIEND
protected:
	const runtime::Ref<model::gameobjects::player::Player> responder;
	const runtime::Ref<model::gameobjects::Creature> requester;

private:
	const int32_t maxDistance;

protected:
	DialogObserver(model::gameobjects::Creature& requester, model::gameobjects::player::Player& responder, int32_t maxDistance);
	~DialogObserver() override;

public:
	void moved() override;

	/**
	 * Is called when player is too far away from dialog serving object
	 */
	virtual void tooFar() = 0;
};

} // namespace aion::gameserver::controllers::observer
