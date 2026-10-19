#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/IDTiamat_2_KahrunTalkAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

AION_AI(IDTiamat_2_KahrunTalkAI, "IDTiamat_2_Kahrun_Talk");

// Java IDTiamat_2_KahrunTalkAI.java:33-37
void IDTiamat_2_KahrunTalkAI::handleDialogStart(Player& player) {
	if (getPosition()->getWorldMapInstance()->getNpcs({730625}).empty())
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
}

// Java IDTiamat_2_KahrunTalkAI.java:39-56
bool IDTiamat_2_KahrunTalkAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	if (dialogActionId == model::DialogAction::SETPRO1) {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
		if (isActivated.compareAndSet(false, true)) {
			getOwner().overrideNpcType(CreatureType::PEACE);
			PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1500604, 500);
			ThreadPoolManager::getInstance().schedule({this}, [this] {
				getMoveController()->moveToPoint(500.0f, 516.6f, 240.27f);
				setStateIfNot(gameserver::ai::AIState::WALKING);
				getOwner().setState(CreatureState::ACTIVE, true);
				PacketSendUtility::broadcastToMap(getOwner(), network::aion::serverpackets::SM_EMOTION(getOwner(), EmotionType::WALK));
				ThreadPoolManager::getInstance().schedule({this}, [this] { spawnOrb(); }, 1500);
			}, 2000);
		}
	}
	return true;
}

// Java IDTiamat_2_KahrunTalkAI.java:58-61
void IDTiamat_2_KahrunTalkAI::spawnOrb() {
	spawn(730625, 503.2197f, 516.6517f, 242.6040f, int8_t{0}, 4);
	AIActions::deleteOwner(*this);
}

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
