#include "aion/gameserver/handlers/ai/instance/rentusBase/CaptainXastaAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/manager/EmoteManager.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::rentusBase {

AION_AI(CaptainXastaAI, "captain_xasta");

// Java CaptainXastaAI.java:41-44
bool CaptainXastaAI::canThink() {
	return canThink_.get();
}

// Java CaptainXastaAI.java:46-57
void CaptainXastaAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (isHome.compareAndSet(true, false)) {
		if (getNpcId() == 217309) {
			PacketSendUtility::broadcastMessage(getOwner(), 1500388);
			startPhaseTask(*this);
		} else {
			startPhase2Task();
		}
	}
}

// Java CaptainXastaAI.java:59-63
void CaptainXastaAI::cancelPhaseTask() {
	runtime::FutureRef task = phaseTask.get();
	if (task != nullptr && !task->isDone()) {
		task->cancel(true);
	}
}

// Java CaptainXastaAI.java:65-83 (the anonymous Runnable: a lambda pinned to the AI; ai is this AI)
void CaptainXastaAI::startPhaseTask(gameserver::ai::NpcAI& ai) {
	phaseTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		if (isDead()) {
			cancelPhaseTask();
		} else {
			canThink_ = false;
			EmoteManager::emoteStopAttacking(getOwner());
			getSpawnTemplate()->setWalkerId("B186C8F43FF13FDD50FA9483B7D8C2BEABAE7F5C");
			WalkManager::startWalking(*this);
			startRun(getOwner());
			spawnHelpers(*this);
			startSanctuaryEvent();
		}
	}, 28000, 28000));
}

// Java CaptainXastaAI.java:85-92
void CaptainXastaAI::handleMoveArrived() {
	AggressiveNpcAI::handleMoveArrived();
	if (getSpawnTemplate()->getWalkerId()) {
		getSpawnTemplate()->setWalkerId(std::nullopt);
		WalkManager::stopWalking(*this);
	}
}

// Java CaptainXastaAI.java:94-107 (the anonymous Runnable: a lambda pinned to the AI)
void CaptainXastaAI::startPhase2Task() {
	phaseTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		if (isDead()) {
			cancelPhaseTask();
		} else {
			SkillEngine::getInstance().getSkill(getOwner(), 19729, 60, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
			PacketSendUtility::broadcastMessage(getOwner(), 1500392);
		}
	}, 30000, 30000));
}

// Java CaptainXastaAI.java:109-136 (the anonymous Runnable: a lambda pinned to the AI)
void CaptainXastaAI::startSanctuaryEvent() {
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead()) {
			canThink_ = true;
			runtime::Ptr<Creature> creature = getAggroList().getTarget(AggroTarget::MOST_HATED);
			if (creature == nullptr) {
				setStateIfNot(AIState::FIGHT);
				getMoveController()->abortMove();
				onGeneralEvent(AIEventType::ATTACK_FINISH);
				onGeneralEvent(AIEventType::BACK_HOME);
			} else {
				getMoveController()->abortMove();
				getOwner().setTarget(creature);
				getOwner().getGameStats()->renewLastAttackTime();
				getOwner().getGameStats()->renewLastAttackedTime();
				getOwner().getGameStats()->renewLastSkillTime();
				setStateIfNot(AIState::FIGHT);
				handleMoveValidate();
				SkillEngine::getInstance().getSkill(getOwner(), 19657, 60, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
			}
		}

	}, 23000);
}

// Java CaptainXastaAI.java:138-141
void CaptainXastaAI::startRun(Npc& npc) {
	npc.setState(CreatureState::ACTIVE, true);
	PacketSendUtility::broadcastPacket(npc, SM_EMOTION(npc, EmotionType::CHANGE_SPEED, 0, npc.getObjectId()));
}

// Java CaptainXastaAI.java:143-163 (the anonymous Runnable: a lambda pinned to the AI)
void CaptainXastaAI::spawnHelpers(gameserver::ai::NpcAI& ai) {
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead()) {
			PacketSendUtility::broadcastMessage(getOwner(), 1500389);
			SkillEngine::getInstance().getSkill(getOwner(), 19968, 60, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
			runtime::Ptr<Npc> npc1 = runtime::cast<Npc>(spawn(282604, 263.0f, 537.0f, 203.0f, int8_t{0}));
			runtime::Ptr<Npc> npc2 = runtime::cast<Npc>(spawn(282604, 186.0f, 555.0f, 203.0f, int8_t{0}));
			npc1->getSpawn()->setWalkerId("30028000014");
			WalkManager::startWalking(dynamic_cast<gameserver::ai::NpcAI&>(npc1->getAi())); // Java: (NpcAI) npc1.getAi()
			npc2->getSpawn()->setWalkerId("30028000015");
			WalkManager::startWalking(dynamic_cast<gameserver::ai::NpcAI&>(npc2->getAi())); // Java: (NpcAI) npc2.getAi()
			startRun(*npc1);
			startRun(*npc2);
		}
	}, 3000);
}

// Java CaptainXastaAI.java:165-170
void CaptainXastaAI::deleteHelpers() {
	runtime::Ptr<WorldMapInstance> instance = getPosition()->getWorldMapInstance();
	if (instance != nullptr) {
		deleteNpcs(instance->getNpcs({282604}));
	}
}

// Java CaptainXastaAI.java:172-178
void CaptainXastaAI::deleteNpcs(const std::vector<runtime::Ptr<Npc>>& npcs) {
	for (const runtime::Ptr<Npc>& npc : npcs) {
		if (npc != nullptr) {
			npc->getController().delete_();
		}
	}
}

// Java CaptainXastaAI.java:180-211 (the tasks keep Ariana and the instance alive, as Java's captures do)
void CaptainXastaAI::handleDied() {
	cancelPhaseTask();
	if (getNpcId() == 217309) {
		PacketSendUtility::broadcastMessage(getOwner(), 1500390);
		spawn(217310, 238.160f, 598.624f, 178.480f, int8_t{0});
		deleteHelpers();
		AIActions::deleteOwner(*this);
	} else {
		PacketSendUtility::broadcastMessage(getOwner(), 1500391);
		const runtime::Ptr<WorldMapInstance> found = getPosition()->getWorldMapInstance();
		if (found != nullptr) {
			const runtime::Ref<WorldMapInstance> instance(*found);
			runtime::Ptr<Npc> arianaFound = instance->getNpc(799668);
			if (arianaFound != nullptr) {
				const runtime::Ref<Npc> ariana(*arianaFound);
				ariana->getEffectController()->removeEffect(19921);
				ThreadPoolManager::getInstance().schedule({this}, [ariana] {
					ariana->getSpawn()->setWalkerId("30028000016");
					WalkManager::startWalking(dynamic_cast<gameserver::ai::NpcAI&>(ariana->getAi())); // Java: (NpcAI) ariana.getAi()
				}, 1000);
				PacketSendUtility::broadcastMessage(ariana, 1500415, 4000);
				PacketSendUtility::broadcastMessage(ariana, 1500416, 13000);
				ThreadPoolManager::getInstance().schedule({this}, [this, ariana, instance] {
					SkillEngine::getInstance().getSkill(*ariana, 19358, 60, ariana)->useNoAnimationSkill();
					instance->setDoorState(145, true);
					deleteNpcs(instance->getNpcs({701156}));
					ThreadPoolManager::getInstance().schedule({this}, [ariana] { ariana->getController().deleteIfAliveOrCancelRespawn(); }, 13000);
				}, 13000);
			}
		}
	}
	AggressiveNpcAI::handleDied();
}

// Java CaptainXastaAI.java:213-217
void CaptainXastaAI::handleDespawned() {
	cancelPhaseTask();
	AggressiveNpcAI::handleDespawned();
}

// Java CaptainXastaAI.java:219-226
void CaptainXastaAI::handleBackHome() {
	canThink_ = true;
	cancelPhaseTask();
	deleteHelpers();
	isHome.set(true);
	AggressiveNpcAI::handleBackHome();
}

} // namespace aion::gameserver::handlers::ai::instance::rentusBase
