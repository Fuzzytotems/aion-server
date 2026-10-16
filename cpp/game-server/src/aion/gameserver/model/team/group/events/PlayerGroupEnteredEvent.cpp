#include "aion/gameserver/model/team/group/events/PlayerGroupEnteredEvent.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/common/legacy/GroupEvent.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/model/team/group/PlayerGroupService.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_RANK_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GROUP_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GROUP_MEMBER_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::team::group::events {

using common::legacy::GroupEvent;
using gameobjects::player::Player;
using network::aion::serverpackets::SM_ABYSS_RANK_UPDATE;
using network::aion::serverpackets::SM_GROUP_INFO;
using network::aion::serverpackets::SM_GROUP_MEMBER_INFO;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

PlayerGroupEnteredEvent::PlayerGroupEnteredEvent(PlayerGroup& groupValue, Player& playerValue) : PlayerEnteredEvent(groupValue, playerValue) {
}

void PlayerGroupEnteredEvent::handleEvent() {
	PlayerGroup& groupValue = *runtime::cast<PlayerGroup>(team);
	Player& playerValue = *player;
	PlayerGroupService::addPlayerToGroup(groupValue, playerValue);
	PacketSendUtility::sendPacket(playerValue, SM_GROUP_INFO(groupValue));
	PacketSendUtility::sendPacket(playerValue, SM_SYSTEM_MESSAGE::STR_PARTY_ENTERED_PARTY());
	PacketSendUtility::sendPacket(playerValue, SM_GROUP_MEMBER_INFO(groupValue, playerValue, GroupEvent::JOIN));
	groupValue.sendBrands(playerValue);
	groupValue.forEach([&groupValue, &playerValue](gameobjects::AionObject& object) {
		Player& member = *runtime::cast<Player>(object);
		if (!member.equals(playerValue)) {
			// TODO probably here JOIN event
			PacketSendUtility::sendPacket(member, SM_GROUP_MEMBER_INFO(groupValue, playerValue, GroupEvent::ENTER));
			PacketSendUtility::sendPacket(member, SM_SYSTEM_MESSAGE::STR_PARTY_HE_ENTERED_PARTY(playerValue.getName()));
			PacketSendUtility::sendPacket(playerValue, SM_GROUP_MEMBER_INFO(groupValue, member, GroupEvent::ENTER));
		}
	});
	PacketSendUtility::broadcastPacket(playerValue, SM_ABYSS_RANK_UPDATE(1, playerValue), true);
	PlayerEnteredEvent::handleEvent();
}

} // namespace aion::gameserver::model::team::group::events
