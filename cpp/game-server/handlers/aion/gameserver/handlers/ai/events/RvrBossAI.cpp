#include "aion/gameserver/handlers/ai/events/RvrBossAI.h"

#include <chrono>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/gameobjects/LetterType.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/services/mail/SystemMailService.h"
#include "aion/gameserver/utils/time/ServerTime.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::events {

AION_AI(RvrBossAI, "rvr_boss");

// Java RvrBossAI.java:28-36
void RvrBossAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	// add player to event list for additional reward
	if (runtime::as<Player>(creature) != nullptr && getPosition()->getMapId() == 600010000) {
		services::SiegeService::getInstance().checkRvrEventPlayer(runtime::as<Player>(creature));
	}
	// TODO Spawn defensive guards (only for bosses in silentera Canyon)
}

// Java RvrBossAI.java:38-44
void RvrBossAI::handleDied() {
	AggressiveNpcAI::handleDied();
	despawnEnemyBoss();
	scheduleRespawn();
	sendParticipationRewards();
}

// Java RvrBossAI.java:47-52 (despawn enemy boss: only for silentera)
void RvrBossAI::despawnEnemyBoss() {
	if (getPosition()->getMapId() == 600010000) {
		runtime::Ptr<WorldMapInstance> instance = getPosition()->getWorldMapInstance();
		deleteNpcs(instance->getNpcs({getNpcId() == 220948 ? 220949 : 220948}));
	}
}

// Java RvrBossAI.java:55-66 (schedule respawn of both silentera bosses: bosses in other maps must not respawn)
void RvrBossAI::scheduleRespawn() {
	if (getPosition()->getMapId() == 600010000) {
		ThreadPoolManager::getInstance().schedule({this}, [this] {
			spawn(220948, 658.7087f, 795.21857f, 293.14087f, int8_t{7});
			spawn(220949, 657.95105f, 737.5624f, 293.19818f, int8_t{0});
		}, 43200000);
	}
}

// Java RvrBossAI.java:68-73
void RvrBossAI::deleteNpcs(const std::vector<runtime::Ptr<Npc>>& npcs) {
	for (const runtime::Ptr<Npc>& npc : npcs) {
		if (npc != nullptr)
			npc->getController().delete_();
	}
}

// Java RvrBossAI.java:75-85
void RvrBossAI::sendParticipationRewards() {
	if (getPosition()->getMapId() == 600010000) {
		const std::chrono::local_time<std::chrono::milliseconds> local = utils::time::ServerTime::now().get_local_time();
		const int64_t hour = std::chrono::hh_mm_ss(local - std::chrono::floor<std::chrono::days>(local)).hours().count(); // Java: getHour()
		if (hour >= 19 && hour <= 23) {
			for (const runtime::Ref<Player>& rewardedPlayer : *services::SiegeService::getInstance().getRvrEventPlayers()) {
				services::mail::SystemMailService::sendMail("EventService", rewardedPlayer->getName(), "EventReward", "Medal", 186000147, 1, 0,
					model::gameobjects::LetterType::NORMAL);
			}
		}
		services::SiegeService::getInstance().clearRvrEventPlayers();
	}
}

// Java RvrBossAI.java:87-90
void RvrBossAI::handleDespawned() {
	AggressiveNpcAI::handleDespawned();
}

} // namespace aion::gameserver::handlers::ai::events
