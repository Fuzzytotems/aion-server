#include "aion/gameserver/handlers/ai/instance/rentusBase/ImprisonedReianAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/manager/WalkManager.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/WalkerData.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/walker/RouteStep.h"
#include "aion/gameserver/model/templates/walker/WalkerTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::handlers::ai::instance::rentusBase {

AION_AI(ImprisonedReianAI, "imprisoned_reian");

// Java ImprisonedReianAI.java:38-46
void ImprisonedReianAI::handleSpawned() {
	walkerId = getSpawnTemplate()->getWalkerId().value_or(std::string());
	getSpawnTemplate()->setWalkerId(std::nullopt);
	if (!walkerId.get().empty()) {
		template_ = DataManager::WALKER_DATA->getWalkerTemplate(walkerId.get());
	}
	GeneralNpcAI::handleSpawned();
}

// Java ImprisonedReianAI.java:48-57 (Java's null template or step: NullPointerException)
void ImprisonedReianAI::handleMoveArrived() {
	const model::templates::walker::RouteStep* step = getOwner().getMoveController()->getCurrentStep();
	GeneralNpcAI::handleMoveArrived();
	const model::templates::walker::WalkerTemplate* walker = template_.get();
	if (walker == nullptr)
		throw runtime::NullPointerException("ImprisonedReianAI.template is null");
	if (step == nullptr)
		throw runtime::NullPointerException("NpcMoveController.getCurrentStep()");
	if (static_cast<int32_t>(walker->getRouteSteps().size()) - 4 == step->getStepIndex()) {
		getSpawnTemplate()->setWalkerId(std::nullopt);
		WalkManager::stopWalking(*this);
		AIActions::deleteOwner(*this);
	}
}

// Java ImprisonedReianAI.java:59-94
void ImprisonedReianAI::handleCreatureMoved(Creature& creature) {
	if (!walkerId.get().empty()) {
		if (runtime::Ptr<Player> player = runtime::as<Player>(runtime::Ptr<Creature>(creature))) {
			if (PositionUtil::getDistance(getOwner(), *player) <= 21) {
				if (isAsked.compareAndSet(false, true)) {
					switch (commons::utils::Rnd::get(1, 10)) {
						case 1:
							PacketSendUtility::broadcastMessage(getOwner(), 390563);
							break;
						case 2:
							PacketSendUtility::broadcastMessage(getOwner(), 390567);
							break;
						default:
							break;
					}
				}
			}
			if (PositionUtil::getDistance(getOwner(), *player) <= 6) {
				if (isSaved.compareAndSet(false, true)) {
					getSpawnTemplate()->setWalkerId(walkerId.get());
					WalkManager::startWalking(*this);
					getOwner().setState(CreatureState::ACTIVE, true);
					PacketSendUtility::broadcastPacket(getOwner(), SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getObjectId()));
					switch (commons::utils::Rnd::get(1, 10)) {
						case 1:
							PacketSendUtility::broadcastMessage(getOwner(), 342410);
							break;
						case 2:
							PacketSendUtility::broadcastMessage(getOwner(), 342411);
							break;
						default:
							break;
					}
				}
			}
		}
	}
}

} // namespace aion::gameserver::handlers::ai::instance::rentusBase
