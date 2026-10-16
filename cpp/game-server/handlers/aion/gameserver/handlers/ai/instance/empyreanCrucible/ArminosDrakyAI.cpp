#include "aion/gameserver/handlers/ai/instance/empyreanCrucible/ArminosDrakyAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/walker/RouteStep.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::handlers::ai::instance::empyreanCrucible {

AION_AI(ArminosDrakyAI, "arminos_draky");

// Java ArminosDrakyAI.java:28-35
void ArminosDrakyAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	getSpawnTemplate()->setWalkerId(walkerId);
	WalkManager::startWalking(*this);
	getOwner().setState(CreatureState::ACTIVE, true);
	PacketSendUtility::broadcastPacket(getOwner(), network::aion::serverpackets::SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getObjectId()));
}

// Java ArminosDrakyAI.java:37-48
void ArminosDrakyAI::handleMoveArrived() {
	GeneralNpcAI::handleMoveArrived();
	const model::templates::walker::RouteStep* step = getOwner().getMoveController()->getCurrentStep();
	if (step == nullptr) // Java: getCurrentStep().isLastStep() of no step
		throw runtime::NullPointerException("NpcMoveController.getCurrentStep()");
	if (step->isLastStep()) { // circle twice
		if (!isStart) {
			getSpawnTemplate()->setWalkerId(std::nullopt);
			WalkManager::stopWalking(*this);
			AIActions::deleteOwner(*this);
		} else
			isStart = false;
	}
}

// Java ArminosDrakyAI.java:50-56
bool ArminosDrakyAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES:
		case AIQuestion::REWARD_AP:
			return true;
		default:
			return GeneralNpcAI::ask(question);
	}
}

// Java ArminosDrakyAI.java:58-61
bool ArminosDrakyAI::canThink() {
	return false;
}

} // namespace aion::gameserver::handlers::ai::instance::empyreanCrucible
