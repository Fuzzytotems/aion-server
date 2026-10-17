#include "aion/gameserver/handlers/ai/instance/rakes/GoldenEyeMantutuAI.h"

#include <any>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/manager/EmoteManager.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::rakes {

AION_AI(GoldenEyeMantutuAI, "golden_eye_mantutu");

// Java GoldenEyeMantutuAI.java:38-41
bool GoldenEyeMantutuAI::canThink() {
	return canThink_.get();
}

// Java GoldenEyeMantutuAI.java:43-56. args[0]: the device as a Ref<Npc>, an empty std::any for Java's null (FeedingMantutuAI); Java's
// `args != null` is always true for a call with an argument.
void GoldenEyeMantutuAI::handleCustomEvent(int32_t eventId, std::span<const std::any> args) {
	if (eventId == 1) {
		canThink_ = false;
		getMoveController()->abortMove();
		EmoteManager::emoteStopAttacking(getOwner());
		runtime::Ptr<Npc> npc = args[0].has_value() ? runtime::Ptr<Npc>(std::any_cast<const runtime::Ref<Npc>&>(args[0])) : nullptr;
		getOwner().setTarget(npc);
		setStateIfNot(AIState::FOLLOWING);
		getMoveController()->moveToTargetObject();
		getOwner().setState(CreatureState::ACTIVE, true);
		PacketSendUtility::broadcastPacket(getOwner(), SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getObjectId()));
	}
}

// Java GoldenEyeMantutuAI.java:58-70
void GoldenEyeMantutuAI::handleMoveArrived() {
	AggressiveNpcAI::handleMoveArrived();
	if (!canThink_.get()) {
		runtime::Ptr<VisibleObject> target = getTarget();
		getMoveController()->abortMove();
		if (target != nullptr && target->isSpawned()) {
			if (runtime::Ptr<Npc> npc = runtime::as<Npc>(target)) { // Java: instanceof Npc npc
				if (npc->getNpcId() == 281128 || npc->getNpcId() == 281129) {
					startFeedTime(*npc);
				}
			}
		}
	}
}

// Java GoldenEyeMantutuAI.java:72-101 (the lambda pins this and the device)
void GoldenEyeMantutuAI::startFeedTime(Npc& npc) {
	ThreadPoolManager::getInstance().schedule({this, &npc}, [this, &npc] {
		if (!isDead()) { // Java: && npc != null (never null here)
			switch (npc.getNpcId()) {
				case 281128: // Feed Supply Device
					getEffectController()->removeEffect(20489);
					spawn(701386, 716.508f, 508.571f, 939.607f, int8_t{119});
					break;
				case 281129: // Water Supply Device
					getEffectController()->removeEffect(20490);
					spawn(701387, 716.389f, 494.207f, 939.607f, int8_t{119});
					break;
				default:
					break;
			}
			npc.getController().delete_();
			canThink_ = true;
			runtime::Ptr<Creature> creature = getAggroList().getTarget(AggroTarget::MOST_HATED);
			if (creature == nullptr) {
				setStateIfNot(AIState::FIGHT);
				think();
			} else {
				getOwner().setTarget(creature);
				getOwner().getGameStats()->renewLastAttackTime();
				getOwner().getGameStats()->renewLastAttackedTime();
				getOwner().getGameStats()->renewLastSkillTime();
				setStateIfNot(AIState::FIGHT);
				handleMoveValidate();
			}
		}
	}, 6000);
}

// Java GoldenEyeMantutuAI.java:103-109
bool GoldenEyeMantutuAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES:
			return true;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

// Java GoldenEyeMantutuAI.java:111-117
void GoldenEyeMantutuAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (isHome.compareAndSet(true, false)) {
		doSchedule();
	}
}

// Java GoldenEyeMantutuAI.java:119-123
void GoldenEyeMantutuAI::handleDespawned() {
	cancelHungerTask();
	AggressiveNpcAI::handleDespawned();
}

// Java GoldenEyeMantutuAI.java:125-133
void GoldenEyeMantutuAI::handleDied() {
	cancelHungerTask();
	runtime::Ptr<Npc> npc = getPosition()->getWorldMapInstance()->getNpc(219037);
	if (npc != nullptr && !npc->isDead()) {
		npc->getEffectController()->removeEffect(18189);
	}
	AggressiveNpcAI::handleDied();
}

// Java GoldenEyeMantutuAI.java:135-143
void GoldenEyeMantutuAI::handleBackHome() {
	cancelHungerTask();
	getEffectController()->removeEffect(20489);
	getEffectController()->removeEffect(20490);
	canThink_ = true;
	isHome.set(true);
	AggressiveNpcAI::handleBackHome();
}

// Java GoldenEyeMantutuAI.java:145-150
void GoldenEyeMantutuAI::doSchedule() {
	hungerTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] {
		int32_t skill = commons::utils::Rnd::nextBoolean() ? 20489 : 20490; // Hunger / Thirst
		SkillEngine::getInstance().getSkill(getOwner(), skill, 20, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
	}, 10000, 30000));
}

// Java GoldenEyeMantutuAI.java:152-156
void GoldenEyeMantutuAI::cancelHungerTask() {
	runtime::FutureRef task = hungerTask.get();
	if (task != nullptr && !task->isDone()) {
		task->cancel(true);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::rakes
