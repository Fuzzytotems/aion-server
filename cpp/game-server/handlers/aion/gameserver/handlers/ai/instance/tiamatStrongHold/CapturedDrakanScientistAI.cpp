#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/CapturedDrakanScientistAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/controllers/observer/DeathObserver.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

AION_AI(CapturedDrakanScientistAI, "captured_drakan_scientist");

// Java CapturedDrakanScientistAI.java:38-47
void CapturedDrakanScientistAI::handleCreatureSee(Creature& creature) {
	if (runtime::as<Player>(creature) != nullptr && isActivated.compareAndSet(false, true)) { // Java: creature instanceof Player
		getKnownList().forEachNpc([this](Npc& n) {
			if (n.getNpcId() == 219390 && PositionUtil::isInRange(getOwner(), n, 25)) {
				n.getObserveController()->attach(
					*DeathObserver::create(runtime::PinnedCallback<void(Creature&)>({this}, [this](Creature&) { handleObservedNpcDied(); })));
			}
		});
	}
}

// Java CapturedDrakanScientistAI.java:49-52
void CapturedDrakanScientistAI::handleObservedNpcDied() {
	if (deadGuardingEyes.incrementAndGet() >= 2)
		ThreadPoolManager::getInstance().schedule(this, &CapturedDrakanScientistAI::startWalk,
			commons::utils::Rnd::get(10, 20) * 100); // NPCs will start walking after some delay
}

// Java CapturedDrakanScientistAI.java:54-60
void CapturedDrakanScientistAI::startWalk() {
	setStateIfNot(AIState::WALKING);
	getOwner().setState(CreatureState::ACTIVE, true);
	getMoveController()->moveToPoint(838, 1317, 396);
	PacketSendUtility::broadcastPacket(getOwner(), SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getOwner().getObjectId()));
	ThreadPoolManager::getInstance().schedule(this, &CapturedDrakanScientistAI::handleNpcEscaping, 9000);
}

// Java CapturedDrakanScientistAI.java:62-65
void CapturedDrakanScientistAI::handleNpcEscaping() {
	handleQuestUpdate();
	AIActions::deleteOwner(*this);
}

// Java CapturedDrakanScientistAI.java:67-70
void CapturedDrakanScientistAI::handleQuestUpdate() {
	for (const runtime::Ptr<Player>& player : getPosition()->getWorldMapInstance()->getPlayersInside())
		updateQuestEntryIfPossible(*player);
}

// Java CapturedDrakanScientistAI.java:72-84
void CapturedDrakanScientistAI::updateQuestEntryIfPossible(Player& player) {
	int32_t quest = player.getRace() == Race::ELYOS ? 30708 : 30758;
	const runtime::Ptr<QuestState> qs = player.getQuestStateList()->getQuestState(quest);
	if (qs != nullptr) {
		SYNCHRONIZED(questLock) {
			if (qs->getQuestVarById(0) != 5) {
				qs->setQuestVar(qs->getQuestVarById(0) + 1);
				PacketSendUtility::sendPacket(player, SM_QUEST_ACTION(SM_QUEST_ACTION::ActionType::UPDATE, *qs));
			}
		}
	}
}

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
