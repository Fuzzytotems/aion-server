#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/skillengine/task/fwd.h"

namespace aion::gameserver::skillengine::task {

/**
 * A timed interaction of a player with an object (crafting, gathering).
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). Abstract RefCounted (fieldmap K4, `Player::interactionTask`); subclasses provide
 * create. The constructor only stores the members (responder defaults to the requester) and is ported. The interaction task (Java anonymous
 * Runnable, fieldmap `AbstractInteractionTask_Runnable`, stored in `task`) is a callback struct in the .cpp once start() is ported.
 *
 * @author ATracer
 */
class AbstractInteractionTask : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<runtime::FutureRef> task{};

protected:
	runtime::Field<int32_t> interval{2500};
	runtime::Field<int32_t> delay{1000};

	const runtime::Ref<gameserver::model::gameobjects::player::Player> requester;
	const runtime::Ref<gameserver::model::gameobjects::VisibleObject> responder;

	/**
	 * @param responder null: the requester (Java null check)
	 */
	AbstractInteractionTask(gameserver::model::gameobjects::player::Player& requester,
		runtime::Ptr<gameserver::model::gameobjects::VisibleObject> responder);
	~AbstractInteractionTask() override;

	/**
	 * Called on each interaction
	 */
	virtual bool onInteraction() = 0;

	/**
	 * Called when interaction is complete
	 */
	virtual void onInteractionFinish() = 0;

	/**
	 * Called before interaction is started
	 */
	virtual void onInteractionStart() = 0;

	/**
	 * Called when interaction is not complete and need to be aborted
	 */
	virtual void onInteractionAbort() = 0;

public:
	/**
	 * Interaction scheduling method
	 */
	void start();

	/**
	 * Stop current interaction
	 */
	void stop();

	/**
	 * Abort current interaction
	 */
	void abort();

	/**
	 * @return true or false
	 */
	bool isInProgress();

	void setInterval(int32_t value) { interval.set(value); }
};

} // namespace aion::gameserver::skillengine::task
