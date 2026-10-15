#include "aion/gameserver/handlers/ai/instance/unstableSplinterpath/UnstableKaluvaAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/manager/EmoteManager.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath {

AION_AI(UnstableKaluvaAI, "unstablekaluva");

// Java UnstableKaluvaAI.java:33-42
void UnstableKaluvaAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (commons::utils::Rnd::chance() < 3) {
		if (!isInMove.get()) {
			isInMove = true;
			moveToSpawner(randomEgg());
		}
	}
}

// Java UnstableKaluvaAI.java:44-56
void UnstableKaluvaAI::moveToSpawner(int32_t egg) {
	runtime::Ptr<Npc> spawner = getPosition()->getWorldMapInstance()->getNpc(egg);
	if (spawner != nullptr) {
		SkillEngine::getInstance().getSkill(getOwner(), 19152, 55, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
		canThink_ = false;
		EmoteManager::emoteStopAttacking(getOwner());
		setStateIfNot(AIState::FOLLOWING);
		getOwner().setState(CreatureState::ACTIVE, true);
		PacketSendUtility::broadcastPacket(getOwner(), SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getObjectId()));
		getOwner().setTarget(spawner);
		getMoveController()->moveToTargetObject();
	}
}

// Java UnstableKaluvaAI.java:58-82
void UnstableKaluvaAI::handleMoveArrived() {
	if (!canThink_.get()) {
		if (runtime::Ptr<Npc> spawner = runtime::as<Npc>(getOwner().getTarget())) {
			spawner->getEffectController()->removeEffect(19222);
			SkillEngine::getInstance().getSkill(getOwner(), 19223, 55, spawner)->useNoAnimationSkill();
			getEffectController()->removeEffect(19152);
		}

		ThreadPoolManager::getInstance().schedule({this}, [this] {
			canThink_ = true;
			runtime::Ptr<Creature> creature = getAggroList().getTarget(AggroTarget::MOST_HATED);
			if (creature != nullptr) {
				getOwner().setTarget(creature);
				getOwner().getGameStats()->renewLastAttackTime();
				getOwner().getGameStats()->renewLastAttackedTime();
				getOwner().getGameStats()->renewLastSkillTime();
			}
			setStateIfNot(AIState::FIGHT);
			think();
		}, 2000);
		isInMove = false;
	}
	AggressiveNpcAI::handleMoveArrived();
}

// Java UnstableKaluvaAI.java:84-88
void UnstableKaluvaAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	isInMove = false;
}

// Java UnstableKaluvaAI.java:90-93
int32_t UnstableKaluvaAI::randomEgg() {
	return commons::utils::Rnd::get({219971, 219952, 219970, 219969});
}

// Java UnstableKaluvaAI.java:95-98
bool UnstableKaluvaAI::canThink() {
	return canThink_.get();
}

// Java UnstableKaluvaAI.java:100-106
bool UnstableKaluvaAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::REWARD_AP:
			return false;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath
