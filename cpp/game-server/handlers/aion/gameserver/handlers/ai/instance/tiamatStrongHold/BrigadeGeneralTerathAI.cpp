#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/BrigadeGeneralTerathAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/manager/EmoteManager.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

AION_AI(BrigadeGeneralTerathAI, "brigadegeneralterath");

// Java BrigadeGeneralTerathAI.java:44-61
void BrigadeGeneralTerathAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (isHome.compareAndSet(true, false)) {
		if (aethericField.get() == nullptr) {
			aethericField = runtime::Ref<Npc>(runtime::cast<Npc>(spawn(730692, 1030.08f, 1030.08f, 1030.08f, int8_t{0})));
			getPosition()->getWorldMapInstance()->setDoorState(706, false);
		}
		if (!isGravityEvent.get()) {
			startSkillTask();
		}
	}
	hpPhases->tryEnterNextPhase(*this);
	if (!isFinalBuff.get() && getOwner().getLifeStats()->getHpPercentage() <= 25) {
		isFinalBuff = true;
		AIActions::useSkill(*this, 20942);
	}
}

// Java BrigadeGeneralTerathAI.java:63-68
void BrigadeGeneralTerathAI::startSkillTask() {
	skillTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		if (!isDead())
			gravityDistortionEvent();
	}, 5000, 30000));
}

// Java BrigadeGeneralTerathAI.java:70-74
void BrigadeGeneralTerathAI::cancelskillTask() {
	runtime::FutureRef task = skillTask.get();
	if (task != nullptr && !task->isCancelled()) {
		task->cancel(true);
	}
}

// Java BrigadeGeneralTerathAI.java:76-82
void BrigadeGeneralTerathAI::gravityDistortionEvent() {
	SkillEngine::getInstance().getSkill(getOwner(), 20739, 55, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
	spawn(283096, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0}); // 4.0
	spawn(283097, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0}); // 4.0
	spawn(283098, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0}); // 4.0
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		SkillEngine::getInstance().getSkill(getOwner(), 20741, 55, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
	}, 5000);
}

// Java BrigadeGeneralTerathAI.java:84-130
void BrigadeGeneralTerathAI::handleHpPhase(int32_t /*phaseHpPercent*/) {
	if (isGravityEvent.get())
		return;
	canThink_ = false;
	isGravityEvent = true;
	cancelskillTask();
	spawn(283558, 1056.8f, 297.6f, 409.9f, int8_t{0}); // TODO find Right ID 4.0
	spawn(283558, 1002.07f, 297.4f, 409.85f, int8_t{0}); // TODO find Right ID 4.0
	SkillEngine::getInstance().getSkill(getOwner(), 20737, 55, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		EmoteManager::emoteStopAttacking(getOwner());
		setStateIfNot(AIState::WALKING);
		getOwner().getMoveController()->moveToPoint(getOwner().getSpawn()->getX(), getOwner().getSpawn()->getY(), getOwner().getSpawn()->getZ());
		WalkManager::startWalking(*this);
		getOwner().setState(CreatureState::ACTIVE, true);
		PacketSendUtility::broadcastPacket(getOwner(), SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getOwner().getObjectId()));
	}, 4000);
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		spawn(283109, 1029.9f, 297.26f, 409.08f, int8_t{0}); // 4.0
		spawn(283110, 1029.93f, 297.31f, 409.08f, int8_t{0}); // 4.0
	}, 10000);
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (isDead())
			return;
		despawn();
		getEffectController()->removeEffect(20737);
		canThink_ = true;
		isGravityEvent = false;
		startSkillTask();
		runtime::Ptr<Creature> creature = getAggroList().getTarget(AggroTarget::MOST_HATED);
		if (creature == nullptr) {
			setStateIfNot(AIState::FIGHT);
			think();
		} else {
			getMoveController()->abortMove();
			getOwner().setTarget(creature);
			getOwner().getGameStats()->renewLastAttackTime();
			getOwner().getGameStats()->renewLastAttackedTime();
			getOwner().getGameStats()->renewLastSkillTime();
			setStateIfNot(AIState::WALKING);
			getOwner().setState(CreatureState::ACTIVE, true);
			getOwner().getMoveController()->moveToTargetObject();
			PacketSendUtility::broadcastPacket(getOwner(), SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getOwner().getObjectId()));
		}
	}, 30000);
}

// Java BrigadeGeneralTerathAI.java:132-138
void BrigadeGeneralTerathAI::deleteNpcs(const std::vector<runtime::Ptr<Npc>>& npcs) {
	for (const runtime::Ptr<Npc>& npc : npcs) {
		if (npc != nullptr) {
			npc->getController().delete_();
		}
	}
}

// Java BrigadeGeneralTerathAI.java:140-147 (no field when he never fought: Java's NullPointerException)
void BrigadeGeneralTerathAI::handleDied() {
	AggressiveNpcAI::handleDied();
	cancelskillTask();
	aethericField.get()->getController().delete_();
	getPosition()->getWorldMapInstance()->setDoorState(706, true);
	despawn();
}

// Java BrigadeGeneralTerathAI.java:149-154
void BrigadeGeneralTerathAI::despawn() {
	runtime::Ptr<WorldMapInstance> instance = getPosition()->getWorldMapInstance();
	deleteNpcs(instance->getNpcs({283558})); // TODO find Right ID 4.0
	deleteNpcs(instance->getNpcs({283109})); // 4.0
	deleteNpcs(instance->getNpcs({283110})); // 4.0
}

// Java BrigadeGeneralTerathAI.java:156-168 (Java bug J-I6-4 ported: the deleted field stays referenced)
void BrigadeGeneralTerathAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	hpPhases->reset();
	isFinalBuff = false;
	cancelskillTask();
	isGravityEvent = false;
	canThink_ = true;
	isHome.set(true);
	aethericField.get()->getController().delete_();
	despawn();
	getPosition()->getWorldMapInstance()->setDoorState(706, true);
}

// Java BrigadeGeneralTerathAI.java:170-174
void BrigadeGeneralTerathAI::handleDespawned() {
	AggressiveNpcAI::handleDespawned();
	cancelskillTask();
}

// Java BrigadeGeneralTerathAI.java:176-179
bool BrigadeGeneralTerathAI::canThink() {
	return canThink_.get();
}

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
