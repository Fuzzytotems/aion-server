#include "aion/gameserver/handlers/ai/instance/raksang/IllusionMasterSharikAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::raksang {

AION_AI(IllusionMasterSharikAI, "illusion_maseter_sharik");

// Java IllusionMasterSharikAI.java:38-47
void IllusionMasterSharikAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	runtime::Ptr<WorldPosition> p = getPosition();
	if (p->getX() == 738.065f && p->getY() == 311.606f) {
		position = 1;
	} else {
		position = 2;
	}
}

// Java IllusionMasterSharikAI.java:49-56
void IllusionMasterSharikAI::handleCreatureMoved(Creature& creature) {
	runtime::Ptr<Player> player = runtime::as<Player>(runtime::Ptr<Creature>(creature));
	if (player != nullptr && PositionUtil::isInRange(getOwner(), *player, 30)) {
		if (startedEvent.compareAndSet(false, true)) {
			PacketSendUtility::broadcastMessage(getOwner(), 1401112);
		}
	}
}

// Java IllusionMasterSharikAI.java:58-62
void IllusionMasterSharikAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java IllusionMasterSharikAI.java:64-73
void IllusionMasterSharikAI::handleHpPhase(int32_t phaseHpPercent) {
	PacketSendUtility::broadcastToMap(getOwner(), 1401136);
	if (position.get() == 1) {
		spawn(730446, 738.766f, 317.482f, 911.897f, int8_t{0}, 5);
	} else {
		spawn(730447, 735.909f, 265.696f, 911.897f, int8_t{0}, 278);
	}
	startPhaseTask();
}

// Java IllusionMasterSharikAI.java:75-111 (the anonymous Runnables: lambdas pinned to the AI)
void IllusionMasterSharikAI::startPhaseTask() {
	phaseTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		if (isDead()) {
			cancelPhaseTask();
		} else {
			PacketSendUtility::broadcastMessage(getOwner(), 1401114);
			SkillEngine::getInstance().getSkill(getOwner(), 19981, 46, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
			ThreadPoolManager::getInstance().schedule({this}, [this] {
				if (!isDead()) {
					SkillEngine::getInstance().getSkill(getOwner(), 19901, 44, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
					if (percent <= 50) {
						ThreadPoolManager::getInstance().schedule({this}, [this] {
							if (!isDead()) {
								SkillEngine::getInstance().getSkill(getOwner(), 19903, 44, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
							}
						}, 13000);
					}
				}
			}, 3000);
		}
	}, 3000, 40000));
}

// Java IllusionMasterSharikAI.java:113-117
void IllusionMasterSharikAI::cancelPhaseTask() {
	runtime::FutureRef task = phaseTask.get();
	if (task != nullptr && !task->isDone()) {
		task->cancel(true);
	}
}

// Java IllusionMasterSharikAI.java:119-131
void IllusionMasterSharikAI::handleBackHome() {
	despawnMirrors();
	cancelPhaseTask();
	AggressiveNpcAI::handleBackHome();
	PacketSendUtility::broadcastToMap(getOwner(), 1401137);
	if (position.get() == 1) {
		spawn(217425, 736.21704f, 270.8546f, 910.678f, int8_t{53});
	} else {
		spawn(217425, 738.065f, 311.606f, 910.678f, int8_t{53});
	}
	AIActions::deleteOwner(*this);
}

// Java IllusionMasterSharikAI.java:133-142
void IllusionMasterSharikAI::despawnMirrors() {
	runtime::Ptr<WorldPosition> p = getPosition();
	if (p != nullptr) {
		runtime::Ptr<WorldMapInstance> instance = p->getWorldMapInstance();
		if (instance != nullptr) {
			deleteNpcs(instance->getNpcs({730446}));
			deleteNpcs(instance->getNpcs({730447}));
		}
	}
}

// Java IllusionMasterSharikAI.java:144-150
void IllusionMasterSharikAI::deleteNpcs(const std::vector<runtime::Ptr<Npc>>& npcs) {
	for (const runtime::Ptr<Npc>& npc : npcs) {
		if (npc != nullptr) {
			npc->getController().delete_();
		}
	}
}

// Java IllusionMasterSharikAI.java:152-156
void IllusionMasterSharikAI::handleDespawned() {
	cancelPhaseTask();
	AggressiveNpcAI::handleDespawned();
}

// Java IllusionMasterSharikAI.java:158-165
void IllusionMasterSharikAI::handleDied() {
	despawnMirrors();
	cancelPhaseTask();
	getPosition()->getWorldMapInstance()->setDoorState(294, true);
	getPosition()->getWorldMapInstance()->setDoorState(295, true);
	AggressiveNpcAI::handleDied();
}

} // namespace aion::gameserver::handlers::ai::instance::raksang
