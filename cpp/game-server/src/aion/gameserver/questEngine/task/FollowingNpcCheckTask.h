#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/questEngine/model/fwd.h"
#include "aion/gameserver/questEngine/task/checker/fwd.h"
#include "aion/gameserver/questEngine/task/fwd.h"

namespace aion::gameserver::questEngine::task {

/**
 * The once-a-second check of a quest escort: it fails the escort (QuestEngine.onNpcLostTarget) when the player or the npc is dead or they are
 * 50 m or more apart, and ends it (QuestEngine.onNpcReachTarget) when the destination checker says the npc has arrived. Either way it stops:
 * the player's QUEST_FOLLOW task is cancelled, the npc gets STOP_FOLLOW_ME and, unless its AI is "following" (whose stop deletes it), is
 * deleted.
 * <p>
 * RefCounted (fieldmap K4, fieldmap.toml: QuestTasks schedules it at a fixed rate and the player keeps the Future as his QUEST_FOLLOW task; it
 * reads env and destinationChecker on the pool thread in every run, so it retains both, as Java's scheduled Runnable does). Java's
 * package-private `new FollowingNpcCheckTask(env, destinationChecker)` is `FollowingNpcCheckTask::create(env, destinationChecker)`.
 *
 * @author ATracer
 */
class FollowingNpcCheckTask : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<model::QuestEnv> env;
	const runtime::Ref<checker::DestinationChecker> destinationChecker;

protected:
	/**
	 * @param player
	 * @param npc
	 * @param destinationChecker
	 */
	FollowingNpcCheckTask(model::QuestEnv& env, checker::DestinationChecker& destinationChecker);
	~FollowingNpcCheckTask() override;

public:
	/** Java: new FollowingNpcCheckTask(env, destinationChecker) (package-private: QuestTasks creates it) */
	static runtime::Ref<FollowingNpcCheckTask> create(model::QuestEnv& env, checker::DestinationChecker& destinationChecker);

	void run(); // @Override of a Java library type (Runnable)

private:
	/**
	 * Following task succeeded, proceed with quest
	 */
	void onSuccess(model::QuestEnv& env);

protected:
	/**
	 * Following task failed, abort further progress
	 */
	virtual void onFail(model::QuestEnv& env);

private:
	void stopFollowing(model::QuestEnv& env);
};

} // namespace aion::gameserver::questEngine::task
