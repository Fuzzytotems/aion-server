#include "aion/gameserver/model/team/group/events/ChangeGroupLootRulesEvent.h"

#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GROUP_INFO.h"

namespace aion::gameserver::model::team::group::events {

ChangeGroupLootRulesEvent::ChangeGroupLootRulesEvent(PlayerGroup& groupValue, common::legacy::LootGroupRules& lootGroupRulesValue)
	: group(groupValue), lootGroupRules(lootGroupRulesValue) {
}

void ChangeGroupLootRulesEvent::handleEvent() {
	group->setLootGroupRules(lootGroupRules);
	network::aion::serverpackets::SM_GROUP_INFO packet(*group);
	group->sendPackets({packet});
}

} // namespace aion::gameserver::model::team::group::events
