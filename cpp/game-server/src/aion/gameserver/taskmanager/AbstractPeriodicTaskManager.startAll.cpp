// Java: AbstractPeriodicTaskManager.startAll (upstream 8912ec714). C++: its own translation unit, so that only the game server, which calls
// it, links the task managers of the other chunks (the libraries of the subclasses depend on AbstractPeriodicTaskManager.cpp, not the reverse).
#include "aion/gameserver/taskmanager/AbstractPeriodicTaskManager.h"

#include "aion/gameserver/taskmanager/tasks/ExpireTimerTask.h"
#include "aion/gameserver/taskmanager/tasks/MoveTaskManager.h"
#include "aion/gameserver/taskmanager/tasks/MovementNotifyTask.h"
#include "aion/gameserver/taskmanager/tasks/PlayerMoveTaskManager.h"
#include "aion/gameserver/taskmanager/tasks/TeamMoveUpdater.h"
#include "aion/gameserver/taskmanager/tasks/TeamStatUpdater.h"
#include "aion/gameserver/taskmanager/tasks/TemporaryTradeTimeTask.h"
#include "aion/gameserver/world/zone/ZoneUpdateService.h"

namespace aion::gameserver::taskmanager {

void AbstractPeriodicTaskManager::startAll() {
	tasks::ExpireTimerTask::getInstance();
	tasks::TemporaryTradeTimeTask::getInstance();
	tasks::TeamStatUpdater::getInstance();
	tasks::TeamMoveUpdater::getInstance();
	tasks::MovementNotifyTask::getInstance();
	tasks::MoveTaskManager::getInstance();
	tasks::PlayerMoveTaskManager::getInstance();
	world::zone::ZoneUpdateService::getInstance();
}

} // namespace aion::gameserver::taskmanager
