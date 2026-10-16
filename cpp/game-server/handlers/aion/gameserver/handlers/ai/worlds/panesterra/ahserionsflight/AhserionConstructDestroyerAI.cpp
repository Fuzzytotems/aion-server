#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/AhserionConstructDestroyerAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/templates/spawns/panesterra/AhserionsFlightSpawnTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/spawnengine/SpawnHandlerType.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

AION_AI(AhserionConstructDestroyerAI, "ahserion_construct_destroyer");

// Java AhserionConstructDestroyerAI.java:28-35
void AhserionConstructDestroyerAI::handleSpawned() {
	AhserionAggressiveNpcAI::handleSpawned();
	if (getSpawnTemplate()->getHandlerType() == spawnengine::SpawnHandlerType::ATTACKER) {
		getOwner().getController().addTask(model::TaskId::DESPAWN, ThreadPoolManager::getInstance().schedule({this}, [this] {
			getOwner().getController().deleteIfAliveOrCancelRespawn();
		}, 9, runtime::TimeUnit::MINUTES));
	}
}

// Java AhserionConstructDestroyerAI.java:37-45
void AhserionConstructDestroyerAI::handleCreatureAggro(Creature& creature) {
	AhserionAggressiveNpcAI::handleCreatureAggro(creature);
	if (isActivated.compareAndSet(false, true)) {
		runtime::Ptr<world::WorldPosition> p = getPosition();
		spawn(297191, p->getX() + 5, p->getY() - 5, p->getZ() + 0.5f, int8_t{0}); // Ahserion Troopers Assassin
		spawn(297191, p->getX() - 5, p->getY() + 5, p->getZ() + 0.5f, int8_t{0}); // Ahserion Troopers Assassin
	}
}

// Java AhserionConstructDestroyerAI.java:47-51 (no super call, as Java; a null template is Java's NullPointerException)
void AhserionConstructDestroyerAI::onEndUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate == nullptr)
		throw runtime::NullPointerException("AhserionConstructDestroyerAI: skill template");
	if (skillTemplate->getSkillId() == 17446 && skillLevel == 57)
		addHateToRndTarget();
}

// Java AhserionConstructDestroyerAI.java:53-58
void AhserionConstructDestroyerAI::despawnAssassins() {
	getKnownList().forEachNpc([](Npc& npc) {
		if (npc.getNpcId() == 297191)
			npc.getController().deleteIfAliveOrCancelRespawn();
	});
}

// Java AhserionConstructDestroyerAI.java:60-65
void AhserionConstructDestroyerAI::handleBackHome() {
	despawnAssassins();
	isActivated.set(false);
	AhserionAggressiveNpcAI::handleBackHome();
}

// Java AhserionConstructDestroyerAI.java:67-71
void AhserionConstructDestroyerAI::handleDied() {
	despawnAssassins();
	AhserionAggressiveNpcAI::handleDied();
}

// Java AhserionConstructDestroyerAI.java:73-77
void AhserionConstructDestroyerAI::handleDespawned() {
	despawnAssassins();
	AhserionAggressiveNpcAI::handleDespawned();
}

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
