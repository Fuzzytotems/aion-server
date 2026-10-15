#include "aion/gameserver/handlers/ai/instance/abyssal_splinter/KaluvaAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/manager/EmoteManager.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::abyssal_splinter {

AION_AI(KaluvaAI, "kaluva");

// Java KaluvaAI.java:34-44
void KaluvaAI::handleIndividualSpawnedSummons(const model::templates::ai::Percentage& percent) {
	runtime::Ptr<VisibleObject> kaluvasSpawner = spawn();
	canThink_ = false;
	EmoteManager::emoteStopAttacking(getOwner());
	setStateIfNot(AIState::FOLLOWING);
	getOwner().setState(CreatureState::ACTIVE, true);
	PacketSendUtility::broadcastPacket(getOwner(), SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getObjectId()));
	getOwner().setTarget(kaluvasSpawner);
	getMoveController()->moveToTargetObject();
}

// Java KaluvaAI.java:46-68
void KaluvaAI::handleMoveArrived() {
	if (!canThink_.get()) {
		runtime::Ptr<Npc> egg = getPosition()->getWorldMapInstance()->getNpc(281902);
		if (egg != nullptr) {
			SkillEngine::getInstance().getSkill(getOwner(), 19223, 55, egg)->useNoAnimationSkill();
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
	}
	SummonerAI::handleMoveArrived();
}

// Java KaluvaAI.java:70-77
runtime::Ptr<VisibleObject> KaluvaAI::spawn() {
	switch (commons::utils::Rnd::get(1, 4)) {
		case 1:
			return SummonerAI::spawn(281902, 663.322021f, 556.731995f, 424.295013f, int8_t{64});
		case 2:
			return SummonerAI::spawn(281902, 644.0224f, 523.9641f, 423.09103f, int8_t{32});
		case 3:
			return SummonerAI::spawn(281902, 611.008f, 539.73395f, 423.25034f, int8_t{119});
		default:
			return SummonerAI::spawn(281902, 628.4426f, 585.4443f, 424.31854f, int8_t{93});
	}
}

// Java KaluvaAI.java:79-82
bool KaluvaAI::canThink() {
	return canThink_.get();
}

// Java KaluvaAI.java:84-90
bool KaluvaAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::REWARD_AP:
			return false;
		default:
			return SummonerAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::abyssal_splinter
