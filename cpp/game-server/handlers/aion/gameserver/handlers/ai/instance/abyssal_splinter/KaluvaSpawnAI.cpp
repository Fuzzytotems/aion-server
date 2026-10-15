#include "aion/gameserver/handlers/ai/instance/abyssal_splinter/KaluvaSpawnAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::abyssal_splinter {

AION_AI(KaluvaSpawnAI, "kaluvaspawn");

// Java KaluvaSpawnAI.java:23-29
void KaluvaSpawnAI::handleDied() {
	NpcAI::handleDied();
	runtime::FutureRef current = task.get();
	if (current != nullptr && !current->isDone())
		current->cancel(true);
	checkKaluva();
}

// Java KaluvaSpawnAI.java:31-35
void KaluvaSpawnAI::handleSpawned() {
	NpcAI::handleSpawned();
	scheduleHatch();
}

// Java KaluvaSpawnAI.java:37-43
void KaluvaSpawnAI::checkKaluva() {
	runtime::Ptr<Npc> kaluva = getPosition()->getWorldMapInstance()->getNpc(216950);
	if (kaluva != nullptr && !kaluva->isDead()) {
		kaluva->getEffectController()->removeEffect(19152);
	}
	AIActions::deleteOwner(*this);
}

// Java KaluvaSpawnAI.java:45-52
void KaluvaSpawnAI::scheduleHatch() {
	task.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead()) {
			hatchAdds();
			checkKaluva();
		}
	}, 22000)); // schedule hatch when debuff ends(20s)
}

// Java KaluvaSpawnAI.java:54-76
void KaluvaSpawnAI::hatchAdds() { // 4 different spawn-formations; See Powerwiki for more information
	runtime::Ptr<WorldPosition> p = getPosition();
	switch (commons::utils::Rnd::get(1, 4)) {
		case 1:
			spawn(281911, p->getX(), p->getY(), p->getZ(), p->getHeading());
			spawn(281911, p->getX(), p->getY(), p->getZ(), p->getHeading());
			break;
		case 2:
			for (int32_t i = 0; i < 12; i++) {
				spawn(281912, p->getX(), p->getY(), p->getZ(), p->getHeading());
			}
			break;
		case 3:
			spawn(282057, p->getX(), p->getY(), p->getZ(), p->getHeading());
			break;
		case 4:
			spawn(281911, p->getX(), p->getY(), p->getZ(), p->getHeading());
			spawn(281912, p->getX(), p->getY(), p->getZ(), p->getHeading());
			spawn(281912, p->getX(), p->getY(), p->getZ(), p->getHeading());
			spawn(281912, p->getX(), p->getY(), p->getZ(), p->getHeading());
			break;
		default:
			break;
	}
}

// Java KaluvaSpawnAI.java:78-84
bool KaluvaSpawnAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::REWARD_AP:
			return false;
		default:
			return NpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::abyssal_splinter
