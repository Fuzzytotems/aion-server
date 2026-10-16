#include "aion/gameserver/handlers/ai/portals/TAGPortalAI.h"

#include <optional>

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/autogroup/AutoGroupTypeInfo.h"
#include "aion/gameserver/model/autogroup/AutoGroup.h"
#include "aion/gameserver/network/aion/serverpackets/SM_AUTO_GROUP.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"

namespace aion::gameserver::handlers::ai::portals {

AION_AI(TAGPortalAI, "t_a_g_portal");

// Java TAGPortalAI.java:23-41
bool TAGPortalAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	if (questId != 0) {
		PortalDialogAI::onDialogSelect(player, dialogActionId, questId, extendedRewardIndex);
		return true;
	}
	int32_t worldId = 0;
	switch (dialogActionId) {
		case model::DialogAction::SETPRO1:
			worldId = 300430000;
			break;
		case model::DialogAction::SETPRO2:
			worldId = 300420000;
			break;
		case model::DialogAction::SETPRO3:
			worldId = 300570000;
			break;
		default:
			worldId = 0;
			break;
	}
	std::optional<model::autogroup::AutoGroupType> agt = model::autogroup::getAutoGroupByWorld(player.getLevel(), worldId);
	if (agt.has_value()) {
		PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_AUTO_GROUP(model::autogroup::getTemplate(*agt)->getMaskId()));
	}
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 0));
	return true;
}

} // namespace aion::gameserver::handlers::ai::portals
