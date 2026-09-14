#include "aion/gameserver/skillengine/task/AbstractInteractionTask.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::skillengine::task {

AbstractInteractionTask::AbstractInteractionTask(gameserver::model::gameobjects::player::Player& requesterValue,
	runtime::Ptr<gameserver::model::gameobjects::VisibleObject> responderValue)
	: requester(requesterValue), responder(responderValue ? runtime::Ref<gameserver::model::gameobjects::VisibleObject>(responderValue)
														   : runtime::Ref<gameserver::model::gameobjects::VisibleObject>(requesterValue)) {
}

AbstractInteractionTask::~AbstractInteractionTask() = default;

// callbacks: com.aionemu.gameserver.skillengine.task.AbstractInteractionTask$1 (scheduleAtFixedRate, stored in task)
void AbstractInteractionTask::start() {
	AION_UNPORTED();
}

void AbstractInteractionTask::stop() {
	AION_UNPORTED();
}

void AbstractInteractionTask::abort() {
	AION_UNPORTED();
}

bool AbstractInteractionTask::isInProgress() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::task
