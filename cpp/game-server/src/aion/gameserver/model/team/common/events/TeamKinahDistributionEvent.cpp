#include "aion/gameserver/model/team/common/events/TeamKinahDistributionEvent.h"

#include <vector>

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::team::common::events {

using gameobjects::player::Player;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

TeamKinahDistributionEvent::TeamKinahDistributionEvent(TemporaryPlayerTeam& teamValue, Player& distributor, int64_t amountValue)
	: AbstractTeamPlayerEvent(teamValue, runtime::Ptr<Player>(distributor)), amount(amountValue) {
}

bool TeamKinahDistributionEvent::checkCondition() {
	return team->hasMember(eventPlayer->getObjectId());
}

void TeamKinahDistributionEvent::handleEvent() {
	if (eventPlayer->getInventory().getKinah() < amount) {
		PacketSendUtility::sendPacket(*eventPlayer, SM_SYSTEM_MESSAGE::STR_NOT_ENOUGH_MONEY());
		return;
	}

	std::vector<runtime::Ptr<Player>> onlineMembers = team->getOnlineMembers();
	const auto onlineCount = static_cast<int32_t>(onlineMembers.size());
	if (onlineCount > 1 && amount >= onlineCount) {
		int64_t rewardPerPlayer = amount / onlineCount;
		if (eventPlayer->getInventory().tryDecreaseKinah(amount)) {
			for (const runtime::Ptr<Player>& member : onlineMembers) {
				member->getInventory().increaseKinah(rewardPerPlayer);
				if (member->equals(*eventPlayer)) {
					PacketSendUtility::sendPacket(*member, SM_SYSTEM_MESSAGE::STR_MSG_SPLIT_ME_TO_B(amount, onlineCount, rewardPerPlayer));
				} else {
					PacketSendUtility::sendPacket(*member,
						SM_SYSTEM_MESSAGE::STR_MSG_SPLIT_B_TO_ME(eventPlayer->getName(), amount, onlineCount, rewardPerPlayer));
				}
			}
		}
	}
}

} // namespace aion::gameserver::model::team::common::events
