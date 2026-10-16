#include "aion/gameserver/model/team/alliance/events/ChangeAllianceLootRulesEvent.h"

#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_INFO.h"

namespace aion::gameserver::model::team::alliance::events {

ChangeAllianceLootRulesEvent::ChangeAllianceLootRulesEvent(PlayerAlliance& allianceValue, common::legacy::LootGroupRules& lootGroupRulesValue)
	: alliance(allianceValue), lootGroupRules(lootGroupRulesValue) {
}

void ChangeAllianceLootRulesEvent::handleEvent() {
	PlayerAlliance& allianceValue = *alliance;
	allianceValue.setLootGroupRules(lootGroupRules);
	network::aion::serverpackets::SM_ALLIANCE_INFO packet(allianceValue);
	allianceValue.sendPackets({packet});
}

} // namespace aion::gameserver::model::team::alliance::events
