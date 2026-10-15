#include "aion/gameserver/handlers/ai/instance/tallocsHollow/TallocsSummonAI.h"

#include "aion/gameserver/model/CreatureTypeInfo.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CUSTOM_SETTINGS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TRANSFORM_IN_SUMMON.h"

namespace aion::gameserver::handlers::ai::instance::tallocsHollow {

AION_AI(TallocsSummonAI, "tallocssummon");

// Java TallocsSummonAI.java:28-40
bool TallocsSummonAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	if (dialogActionId != model::DialogAction::MAKE_MERCENARY || getOwner().getCreator() != nullptr)
		return false;
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
	getOwner().setCreatorId(player.getObjectId());
	getOwner().setMasterName(player.getName());
	getOwner().setState(CreatureState::ACTIVE, true);
	PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_TRANSFORM_IN_SUMMON(player, getObjectId()));
	PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_CUSTOM_SETTINGS(getObjectId(), 0, model::getId(CreatureType::FRIEND), 0));
	PacketSendUtility::broadcastPacket(getOwner(), SM_EMOTION(getOwner(), EmotionType::CHANGE_SPEED, 0, getObjectId()));
	return true;
}

// Java TallocsSummonAI.java:42-45
void TallocsSummonAI::handleDialogStart(Player& player) {
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 10));
}

} // namespace aion::gameserver::handlers::ai::instance::tallocsHollow
