#include "aion/gameserver/handlers/ai/events/RvrGuardAI.h"

#include <chrono>

#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/utils/time/ServerTime.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::events {

AION_AI(RvrGuardAI, "rvr_guard");

// Java RvrGuardAI.java:22-36
void RvrGuardAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	// add player to event list for additional reward
	if (runtime::as<Player>(creature) != nullptr && getPosition()->getMapId() == 600010000) {
		const std::chrono::local_time<std::chrono::milliseconds> local = utils::time::ServerTime::now().get_local_time();
		const int64_t hour = std::chrono::hh_mm_ss(local - std::chrono::floor<std::chrono::days>(local)).hours().count(); // Java: getHour()
		if (hour >= 19 && hour <= 23) {
			runtime::Ptr<Npc> bossAsmo = getPosition()->getWorldMapInstance()->getNpc(220948);
			runtime::Ptr<Npc> bossElyos = getPosition()->getWorldMapInstance()->getNpc(220949);
			if (bossAsmo != nullptr && bossElyos != nullptr) {
				services::SiegeService::getInstance().checkRvrEventPlayer(runtime::as<Player>(creature));
			}
		}
	}
}

} // namespace aion::gameserver::handlers::ai::events
