#include "aion/gameserver/handlers/ai/instance/rentusBase/ReianBomberAI.h"

#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/walker/RouteStep.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::rentusBase {

AION_AI(ReianBomberAI, "reian_bomber");

// Java ReianBomberAI.java:34-41
void ReianBomberAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	getSpawnTemplate()->setWalkerId("30028000024");
	WalkManager::startWalking(*this);
	getOwner().setState(CreatureState::ACTIVE, true);
	PacketSendUtility::broadcastPacket(getOwner(), SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getObjectId()));
}

// Java ReianBomberAI.java:43-54
void ReianBomberAI::handleMoveArrived() {
	const model::templates::walker::RouteStep* step = getOwner().getMoveController()->getCurrentStep();
	if (step == nullptr) // Java: getCurrentStep().getStepIndex() of no step
		throw runtime::NullPointerException("NpcMoveController.getCurrentStep()");
	int32_t point = step->getStepIndex();
	GeneralNpcAI::handleMoveArrived();
	if (hasArrivedBoss.get()) {
		startHelpEvent();
	} else if (point == 7 && hasArrivedBoss.compareAndSet(false, true)) {
		getSpawnTemplate()->setWalkerId(std::nullopt);
		WalkManager::stopWalking(*this);
		startHelpEvent();
	}
}

// Java ReianBomberAI.java:56-89 (the anonymous Runnable: a lambda pinned to the AI)
void ReianBomberAI::startHelpEvent() {
	getMoveController()->abortMove();
	setStateIfNot(AIState::IDLE);
	setSubStateIfNot(AISubState::NONE);
	SkillEngine::getInstance().getSkill(getOwner(), 19374, 60, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead()) {
			setSubStateIfNot(AISubState::WALK_RANDOM);
			setStateIfNot(AIState::WALKING);
			switch (position.get()) {
				case 1:
					help(359.763f, 585.597f, 145.525f);
					getMoveController()->moveToPoint(346.47787f, 604.0337f, 145.8766f);
					position = position + 1;
					break;
				case 2:
					help(346.086f, 597.062f, 146.119f);
					getMoveController()->moveToPoint(370.93597f, 607.6427f, 145.41916f);
					position = position + 1;
					break;
				case 3:
					help(362.143f, 604.723f, 146.125f);
					getMoveController()->moveToPoint(361.7722f, 584.4937f, 145.63573f);
					position = 1;
					break;
				default:
					break;
			}
		}
	}, 8000);

}

// Java ReianBomberAI.java:91-95
void ReianBomberAI::deleteNpc(runtime::Ptr<Npc> npc) {
	if (npc != nullptr) {
		npc->getController().delete_();
	}
}

// Java ReianBomberAI.java:97-115
void ReianBomberAI::help(float x, float y, float z) {
	runtime::Ptr<WorldMapInstance> instance = getPosition()->getWorldMapInstance();
	if (instance != nullptr) {
		for (const runtime::Ptr<Npc>& npc : instance->getNpcs({282530})) {
			runtime::Ptr<WorldPosition> p = npc->getPosition();
			if (p->getX() == x && p->getY() == y) {
				deleteNpc(npc);
			}
		}
		for (const runtime::Ptr<Npc>& npc : instance->getNpcs({282387})) {
			runtime::Ptr<WorldPosition> p = npc->getPosition();
			if (p->getX() == x && p->getY() == y) {
				return;
			}
		}
		runtime::Ptr<Npc> npc = runtime::cast<Npc>(spawn(282387, x, y, z, int8_t{0}));
		SkillEngine::getInstance().getSkill(*npc, 19731, 1, npc)->useNoAnimationSkill();
	}
}

} // namespace aion::gameserver::handlers::ai::instance::rentusBase
