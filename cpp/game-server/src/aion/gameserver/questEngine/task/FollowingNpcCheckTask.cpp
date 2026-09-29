#include "aion/gameserver/questEngine/task/FollowingNpcCheckTask.h"

#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/task/checker/DestinationChecker.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::questEngine::task {

namespace {

using gameserver::ai::event::AIEventType;
using gameserver::model::TaskId;
using gameserver::model::gameobjects::Npc;
using gameserver::model::gameobjects::player::Player;
using utils::PositionUtil;

} // namespace

FollowingNpcCheckTask::FollowingNpcCheckTask(model::QuestEnv& envValue, checker::DestinationChecker& destinationCheckerValue)
	: env(envValue), destinationChecker(destinationCheckerValue) {
}

FollowingNpcCheckTask::~FollowingNpcCheckTask() = default;

runtime::Ref<FollowingNpcCheckTask> FollowingNpcCheckTask::create(model::QuestEnv& env, checker::DestinationChecker& destinationChecker) {
	return runtime::makeRef<FollowingNpcCheckTask>(env, destinationChecker);
}

void FollowingNpcCheckTask::run() {
	const runtime::Ptr<Player> player = env->getPlayer();
	runtime::Ptr<Npc> npc = runtime::cast<Npc>(destinationChecker->getFollower());
	// java-bug kept: no return after onFail. The run goes on: a failure that is also out of range fails twice (onNpcLostTarget twice), and a
	// failed escort whose npc stands at its destination also succeeds (onNpcLostTarget, then onNpcReachTarget). Only the next period is gone:
	// stopFollowing cancelled the task (docs/deviations/P5-06a.md, E-07).
	if (player->isDead() || npc->isDead()) {
		onFail(*env);
	}
	if (!PositionUtil::isInRange(*player, *npc, 50)) {
		onFail(*env);
	}

	if (destinationChecker->check()) {
		onSuccess(*env);
	}
}

// The parameter of onSuccess, onFail and stopFollowing is Java's `env`, which shadows the member of the same name (the run passes the member)

void FollowingNpcCheckTask::onSuccess(model::QuestEnv& questEnv) {
	stopFollowing(questEnv);
	QuestEngine::getInstance().onNpcReachTarget(questEnv);
}

void FollowingNpcCheckTask::onFail(model::QuestEnv& questEnv) {
	stopFollowing(questEnv);
	QuestEngine::getInstance().onNpcLostTarget(questEnv);
}

void FollowingNpcCheckTask::stopFollowing(model::QuestEnv& questEnv) {
	runtime::Ptr<Player> player = questEnv.getPlayer();
	runtime::Ptr<Npc> npc = runtime::cast<Npc>(destinationChecker->getFollower());
	player->getController().cancelTask(TaskId::QUEST_FOLLOW);
	npc->getAi().onCreatureEvent(AIEventType::STOP_FOLLOW_ME, *player);
	if (npc->getAi().getName() != "following")
		npc->getController().delete_();
}

} // namespace aion::gameserver::questEngine::task
