#include "aion/gameserver/model/team/league/events/LeagueKinahDistributionEvent.h"

#include <vector>

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/league/League.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::team::league::events {

using gameobjects::player::Player;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

LeagueKinahDistributionEvent::LeagueKinahDistributionEvent(Player& player, int64_t amountValue) : amount(amountValue), eventPlayer(player) {
}

void LeagueKinahDistributionEvent::handleEvent() {
	Player& player = *eventPlayer;
	if (player.getInventory().getKinah() < amount) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_NOT_ENOUGH_MONEY());
		return;
	}

	runtime::Ptr<League> league = player.getPlayerAlliance()->getLeague();
	std::vector<runtime::Ptr<Player>> onlineMembers = league->getOnlineMembers();
	const auto onlineCount = static_cast<int32_t>(onlineMembers.size());
	if (onlineCount > 1 && amount >= onlineCount) {
		int64_t rewardPerPlayer = amount / onlineCount;
		if (player.getInventory().tryDecreaseKinah(amount)) {
			for (const runtime::Ptr<Player>& member : onlineMembers) {
				member->getInventory().increaseKinah(rewardPerPlayer);
				if (member->equals(player)) {
					PacketSendUtility::sendPacket(*member, SM_SYSTEM_MESSAGE::STR_MSG_SPLIT_ME_TO_B(amount, onlineCount, rewardPerPlayer));
				} else {
					PacketSendUtility::sendPacket(*member, SM_SYSTEM_MESSAGE::STR_MSG_SPLIT_B_TO_ME(player.getName(), amount, onlineCount, rewardPerPlayer));
				}
			}
		}
	}
}

} // namespace aion::gameserver::model::team::league::events
