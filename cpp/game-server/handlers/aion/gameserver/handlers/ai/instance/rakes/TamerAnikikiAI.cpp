#include "aion/gameserver/handlers/ai/instance/rakes/TamerAnikikiAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/walker/RouteStep.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::instance::rakes {

AION_AI(TamerAnikikiAI, "tamer_anikiki");

// Java TamerAnikikiAI.java:34-37
bool TamerAnikikiAI::canThink() {
	return false;
}

// Java TamerAnikikiAI.java:39-57
void TamerAnikikiAI::handleCreatureMoved(Creature& creature) {
	GeneralNpcAI::handleCreatureMoved(creature);
	runtime::Ptr<Player> player = runtime::as<Player>(creature);
	if (getNpcId() == 219040 && isInRange(creature, 10) && player != nullptr) {
		if (isStartedWalkEvent.compareAndSet(false, true)) {
			getSpawnTemplate()->setWalkerId("3004600001");
			WalkManager::startWalking(*this);
			getOwner().setState(CreatureState::ACTIVE, true);
			PacketSendUtility::broadcastPacket(getOwner(), SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getObjectId()));
			// Key Box
			spawn(700553, 611, 481, 936, int8_t{90});
			spawn(700553, 657, 482, 936, int8_t{60});
			spawn(700553, 626, 540, 936, int8_t{1});
			spawn(700553, 645, 534, 936, int8_t{75});
			PacketSendUtility::sendPacket(*player, SM_QUEST_ACTION(0, 180));
			PacketSendUtility::broadcastToMap(getOwner(), 1400262);
		}
	}
}

// Java TamerAnikikiAI.java:59-76
void TamerAnikikiAI::handleMoveArrived() {
	if (getNpcId() == 219040) {
		int32_t point = getOwner().getMoveController()->getCurrentStep()->getStepIndex();
		if (point == 12) {
			getSpawnTemplate()->setWalkerId(std::nullopt);
			WalkManager::stopWalking(*this);
			AIActions::deleteOwner(*this);
			return;
		} else if (point == 8) {
			GeneralNpcAI::handleMoveArrived();
			getOwner().setState(CreatureState::WALK_MODE, true);
			PacketSendUtility::broadcastPacket(getOwner(), SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getObjectId()));
			return;
		}
	}
	GeneralNpcAI::handleMoveArrived();
}

// Java TamerAnikikiAI.java:78-91 (the anonymous Runnable: a task pinning this)
void TamerAnikikiAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	if (getNpcId() != 219040) {
		ThreadPoolManager::getInstance().schedule({this}, [this] {
			SkillEngine::getInstance().getSkill(getOwner(), 18189, 20, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
		}, 5000);
	}
}

// Java TamerAnikikiAI.java:93-99
float TamerAnikikiAI::modifyDamage(Creature& /*attacker*/, float damage, runtime::Ptr<Effect> /*effect*/) {
	if (getNpcId() == 219037)
		return damage;
	else
		return 1;
}

} // namespace aion::gameserver::handlers::ai::instance::rakes
