#include "aion/gameserver/taskmanager/tasks/MovementNotifyTask.h"

#include <exception>
#include <limits>
#include <string_view>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/knownlist/KnownObject.h"

namespace aion::gameserver::taskmanager::tasks {

namespace {

/** Java: the logger of AILogger */
const auto aiLog = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.ai.AILogger");

/** Java AILogger.moveinfo(owner, message) (ai/AILogger, P5-05, has no C++ header yet; its logger name is the AILogger class) */
void aiLoggerMoveinfo(model::gameobjects::Creature& owner, std::string_view message) {
	if (configs::main::AIConfig::MOVE_DEBUG.load() && owner.getAi().isLogging()) {
		aiLog.info("[AI] " + std::to_string(owner.getObjectId()) + " - " + std::string(message));
	}
}

} // namespace

MovementNotifyTask& MovementNotifyTask::getInstance() {
	// Java: SingletonHolder; a never-released Ref (hub-headers.md §11.1)
	static const runtime::Ref<MovementNotifyTask>& instance = *new runtime::Ref<MovementNotifyTask>(runtime::makeRef<MovementNotifyTask>());
	return *instance;
}

MovementNotifyTask::MovementNotifyTask() : AbstractFIFOPeriodicTaskManager(500, "MovementNotifyTask") {
}

MovementNotifyTask::~MovementNotifyTask() = default;

void MovementNotifyTask::callTask(model::gameobjects::Creature& creature) {
	if (creature.isDead())
		return;

	// In Reshanta:
	// max_move_broadcast_count is 200 and
	// min_move_broadcast_range is 75, as in client WorldId.xml
	int32_t limit = creature.getWorldId() == 400010000 ? 200 : std::numeric_limits<int32_t>::max();
	int32_t notified = 0;
	for (runtime::Ptr<world::knownlist::KnownObject> o : creature.getKnownList().stream()) {
		auto npc = runtime::as<model::gameobjects::Npc>(o->get());
		if (!npc)
			continue;
		if (notified++ >= limit)
			break;
		notifyCreatureMoved(*npc, creature);
	}
}

void MovementNotifyTask::notifyCreatureMoved(model::gameobjects::Npc& npc, model::gameobjects::Creature& creature) {
	try {
		if (npc.getAi().getState() == ai::AIState::DIED || npc.isDead()) {
			if (npc.getAi().isLogging()) {
				aiLoggerMoveinfo(npc, "WARN: NPC died but still in knownlist");
			}
			return;
		}
		npc.getAi().onCreatureEvent(ai::event::AIEventType::CREATURE_MOVED, creature);
	} catch (const std::exception& ex) {
		log.error("Could not notify {} about movement of {}", npc.toString(), creature.toString(), ex);
	}
}

std::string MovementNotifyTask::getCalledMethodName() {
	return "notifyOnMove()";
}

} // namespace aion::gameserver::taskmanager::tasks
