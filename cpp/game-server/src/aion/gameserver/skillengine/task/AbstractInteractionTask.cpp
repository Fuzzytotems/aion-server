#include "aion/gameserver/skillengine/task/AbstractInteractionTask.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/services/RecallService.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::skillengine::task {

AbstractInteractionTask::AbstractInteractionTask(gameserver::model::gameobjects::player::Player& requesterValue,
	runtime::Ptr<gameserver::model::gameobjects::VisibleObject> responderValue)
	: requester(requesterValue), responder(responderValue ? runtime::Ref<gameserver::model::gameobjects::VisibleObject>(responderValue)
														   : runtime::Ref<gameserver::model::gameobjects::VisibleObject>(requesterValue)) {
}

AbstractInteractionTask::~AbstractInteractionTask() = default;

// callbacks: com.aionemu.gameserver.skillengine.task.AbstractInteractionTask$1 (scheduleAtFixedRate, stored in task). The Java anonymous
// Runnable captures the task; here the scheduled body holds a Ref to it, which is the same retention (cycles.toml: stop / abort cancel the
// periodic task and release it). The task is pinned to the requester so the leak census and the zombie breaker can attribute it to its owner
// (runtime-architecture.md §5.4); the pin adds no retention Java does not have.
void AbstractInteractionTask::start() {
	runtime::Ptr<AbstractInteractionTask> oldTask = requester->getInteractionTask();
	if (oldTask)
		oldTask->abort();
	requester->setInteractionTask(*this);
	services::RecallService::getInstance().cancel(*requester, services::RecallService::CancelReason::CANCELLED);
	onInteractionStart();

	runtime::Ref<AbstractInteractionTask> self(*this);
	task.set(utils::ThreadPoolManager::getInstance().scheduleAtFixedRate({&*requester}, [self] { self->runInteraction(); }, delay.get(),
		interval.get()));
}

void AbstractInteractionTask::runInteraction() {
	// A throwing body does not end the periodic task, here or in Java: ThreadPoolManager.scheduleAtFixedRate wraps the Runnable in
	// RunnableWrapper(catchAndLogThrowables = true) (ThreadPoolManager.java:60-62), so the exception is logged inside the body and the task is
	// re-armed. Future does the same (logExceptions, Future.h:89), so `task` never outlives a run without a stop() that can cut it.
	bool stopTask = !requester->isOnline() || onInteraction();
	if (stopTask)
		stop();
}

void AbstractInteractionTask::stop() {
	if (requester->getInteractionTask().rawPointer() == this)
		requester->setInteractionTask(nullptr);
	onInteractionFinish();

	runtime::FutureRef current = task.get();
	// Java clears the field only inside this branch, and so does this port: a Future that is already cancelled has released its captures
	// (Future::cancel -> releaseCaptures), so the handle that stays behind retains nothing of this task.
	if (current && !current->isCancelled()) {
		current->cancel(false);
		task.set(nullptr);
	}
}

void AbstractInteractionTask::abort() {
	onInteractionAbort();
	stop();
}

bool AbstractInteractionTask::isInProgress() {
	runtime::FutureRef current = task.get();
	return current && !current->isCancelled();
}

} // namespace aion::gameserver::skillengine::task
