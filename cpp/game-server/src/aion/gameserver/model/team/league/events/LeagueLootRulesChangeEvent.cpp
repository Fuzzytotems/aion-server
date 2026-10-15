#include "aion/gameserver/model/team/league/events/LeagueLootRulesChangeEvent.h"

#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"
#include "aion/gameserver/model/team/league/League.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ALLIANCE_INFO.h"

namespace aion::gameserver::model::team::league::events {

LeagueLootRulesChangeEvent::LeagueLootRulesChangeEvent(League& leagueValue, common::legacy::LootGroupRules& lootGroupRulesValue)
	: league(leagueValue), lootGroupRules(lootGroupRulesValue) {
}

void LeagueLootRulesChangeEvent::handleEvent() {
	league->setLootGroupRules(lootGroupRules);
	league->forEach([](gameobjects::AionObject& object) {
		alliance::PlayerAlliance& alliance = *runtime::cast<alliance::PlayerAlliance>(object);
		network::aion::serverpackets::SM_ALLIANCE_INFO packet(alliance);
		alliance.sendPackets({packet});
	});
}

} // namespace aion::gameserver::model::team::league::events
