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
	// java-race: check-then-act on Player.interactionTask (AbstractInteractionTask.java:61-64), as in Java. Two concurrent starts for one
	// player read the same old task, both abort it, and the loser's setInteractionTask is overwritten by the winner's; the loser's own periodic
	// task keeps running until one of its runs stops it. See docs/deviations/P5-02.md.
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
	// java-race: check-then-act on Player.interactionTask (AbstractInteractionTask.java:84-85), as in Java. A start() that replaced the field
	// between this read and its write makes this task skip the clearing, so the field keeps naming the task that replaced it. This write is the
	// java-hook cycles.toml names for Player.interactionTask, and in C++ it is the only cut of the Player -> interactionTask -> requester cycle.
	// What the code says about that, and no more (docs/deviations/P5-02.md): every write that stores a task is a start()'s, which schedules the
	// periodic run right after it, and every run begins with `!requester->isOnline()` - leaveWorld nulls the client connection
	// (PlayerLeaveWorldService.java:65, .cpp:87) before it aborts the interaction task (:125-126) - so whichever task the field names after a
	// logout stops itself on its next run, and that stop() then does find itself in the field and clears it. No interleaving of the two guards
	// that leaves a task nothing can stop has been constructed. A throwing onInteractionStart() does leave one (start() writes the field before
	// it and schedules after it), as it does in Java: test InteractionTaskTest.AThrowingOnInteractionStartLeavesTheTaskInTheField.
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
