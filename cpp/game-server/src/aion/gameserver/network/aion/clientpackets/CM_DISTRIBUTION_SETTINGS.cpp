#include "aion/gameserver/network/aion/clientpackets/CM_DISTRIBUTION_SETTINGS.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceService.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/model/team/group/PlayerGroupService.h"
#include "aion/gameserver/model/team/league/League.h"
#include "aion/gameserver/model/team/league/LeagueService.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_DISTRIBUTION_SETTINGS::CM_DISTRIBUTION_SETTINGS(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

using model::gameobjects::player::Player;
using model::team::common::legacy::LootGroupRules;
using model::team::common::legacy::LootRuleType;

// Java CM_DISTRIBUTION_SETTINGS.java:41-60
void CM_DISTRIBUTION_SETTINGS::readImpl() {
	isLeague = readD();
	lootRule = readD();
	switch (lootRule) {
		case 0:
			lootRules = LootRuleType::FREEFORALL;
			break;
		case 1:
			lootRules = LootRuleType::ROUNDROBIN;
			break;
		case 2:
			lootRules = LootRuleType::LEADER;
			break;
		default:
			lootRules = LootRuleType::FREEFORALL;
			break;
	}
	misc = readD();
	commonItemAbove = readD();
	superiorItemAbove = readD();
	heroicItemAbove = readD();
	fabledItemAbove = readD();
	ethernalItemAbove = readD();
	mythicItemAbove = readD();
	unk = readD();
}

// Java CM_DISTRIBUTION_SETTINGS.java:63-78
void CM_DISTRIBUTION_SETTINGS::runImpl() {
	const runtime::Ptr<Player> leader = getConnection()->getActivePlayer();

	runtime::Ptr<model::team::group::PlayerGroup> group = leader->getPlayerGroup();
	if (group) {
		model::team::group::PlayerGroupService::changeGroupRules(*group, *LootGroupRules::create(lootRules, misc, commonItemAbove, superiorItemAbove,
																			  heroicItemAbove, fabledItemAbove, ethernalItemAbove, mythicItemAbove));
	}
	runtime::Ptr<model::team::alliance::PlayerAlliance> alliance = leader->getPlayerAlliance();
	if (alliance) {
		if (alliance->isInLeague())
			model::team::league::LeagueService::changeGroupRules(*alliance->getLeague(), *LootGroupRules::create(lootRules, misc, commonItemAbove,
																							   superiorItemAbove, heroicItemAbove, fabledItemAbove,
																							   ethernalItemAbove, mythicItemAbove));
		else
			model::team::alliance::PlayerAllianceService::changeGroupRules(*alliance, *LootGroupRules::create(lootRules, misc, commonItemAbove,
																						  superiorItemAbove, heroicItemAbove, fabledItemAbove,
																						  ethernalItemAbove, mythicItemAbove));
	}
}

AION_CLIENT_PACKET(CM_DISTRIBUTION_SETTINGS);

} // namespace aion::gameserver::network::aion::clientpackets
