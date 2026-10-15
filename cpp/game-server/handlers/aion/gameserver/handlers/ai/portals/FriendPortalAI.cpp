#include "aion/gameserver/handlers/ai/portals/FriendPortalAI.h"

#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/gameobjects/SummonedHouseNpc.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::handlers::ai::portals {

AION_AI(FriendPortalAI, "friendportal");

// Java FriendPortalAI.java:23-37
void FriendPortalAI::handleDialogStart(Player& player) {
	const runtime::Ptr<SummonedHouseNpc> me = runtime::cast<SummonedHouseNpc>(getOwner()); // Java: (SummonedHouseNpc) getOwner()
	const runtime::Ptr<model::house::House> creator = runtime::cast<model::house::House>(me->getCreator());
	if (!creator)
		throw runtime::NullPointerException("the relationship crystal has no house"); // Java: me.getCreator().getOwnerId()
	const int32_t playerOwner = creator->getOwnerId();

	const runtime::Ptr<model::team::legion::Legion> legion = player.getLegion();
	const bool allowed = playerOwner == 0 || player.getObjectId() == playerOwner || static_cast<bool>(player.getFriendList().getFriend(playerOwner)) ||
	                     (legion && legion->isMember(playerOwner));

	if (allowed) {
		PacketSendUtility::sendPacket(player, SM_FRIEND_LIST()); // client needs friendlist info to list entries in housing friendlist
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getOwner().getObjectId(), model::id(DialogPage::HOUSING_FRIENDLIST)));
	} else {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_HOUSING_TELEPORT_CANT_USE());
	}
}

// Java FriendPortalAI.java:39-41
void FriendPortalAI::handleDialogFinish(Player& player) {
	static_cast<void>(player);
}

} // namespace aion::gameserver::handlers::ai::portals
