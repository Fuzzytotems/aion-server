#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionMountableAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/TribeClass.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_POSITION.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::eternalBastion {

AION_AI(EternalBastionMountableAI, "eternal_bastion_mountable");

// Java EternalBastionMountableAI.java:29-32
void EternalBastionMountableAI::handleUseItemFinish(Player& player) {
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
}

// Java EternalBastionMountableAI.java:34-39
bool EternalBastionMountableAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	if (dialogActionId == model::DialogAction::SETPRO1)
		useNpc(player);
	return true;
}

// Java EternalBastionMountableAI.java:41-46
void EternalBastionMountableAI::useNpc(Player& player) {
	if ((getOwner().getTribe() == model::TribeClass::IDF5_TD_WEAPON_PC || getOwner().getTribe() == model::TribeClass::IDF5_TD_WEAPON_PC_DARK))
		tryMountNpc(player, 185000136, 21138 + model::getRaceId(player.getRace()));
	else
		tryMountNpc(player, 185000137, 21141);
}

// Java EternalBastionMountableAI.java:48-57
void EternalBastionMountableAI::tryMountNpc(Player& player, int32_t keyItemId, int32_t skillId) {
	if (!player.getInventory().decreaseByItemId(keyItemId, 1)) {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), model::id(model::DialogPage::NO_RIGHT)));
		return;
	}
	world::World::getInstance().updatePosition(player, getPosition()->getX(), getPosition()->getY(), getPosition()->getZ(),
		getPosition()->getHeading());
	PacketSendUtility::broadcastPacketAndReceive(player, network::aion::serverpackets::SM_POSITION(player));
	SkillEngine::getInstance().applyEffectDirectly(skillId, player, player);
	AIActions::deleteOwner(*this);
}

} // namespace aion::gameserver::handlers::ai::instance::eternalBastion
