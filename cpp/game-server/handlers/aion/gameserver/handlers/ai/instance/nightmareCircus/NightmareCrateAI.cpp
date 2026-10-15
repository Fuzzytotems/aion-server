#include "aion/gameserver/handlers/ai/instance/nightmareCircus/NightmareCrateAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/configs/main/GroupConfig.h"
#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/drop/DropRegistrationService.h"
#include "aion/gameserver/services/drop/DropService.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

namespace aion::gameserver::handlers::ai::instance::nightmareCircus {

AION_AI(NightmareCrateAI, "nightmare_crate");

// Java NightmareCrateAI.java:31-51
void NightmareCrateAI::handleUseItemFinish(Player& player) {
	int32_t npcId = getOwner().getNpcId();
	if ((npcId == 831745)
		&& (player.getInventory().getItemCountByItemId(185000187) > 0 || player.getInventory().getItemCountByItemId(185000184) > 0)) {
		if (player.getInventory().decreaseByItemId(185000187, 1)) {
			analyzeOpening(player);
		} else if (player.getInventory().decreaseByItemId(185000184, 1)) {
			analyzeOpening(player);
		}
	} else if (npcId == 831575
		&& (player.getInventory().getItemCountByItemId(185000187) > 2 || player.getInventory().getItemCountByItemId(185000184) > 2)) {
		if (player.getInventory().decreaseByItemId(185000187, 3)) {
			analyzeOpening(player);
		} else if (player.getInventory().decreaseByItemId(185000184, 3)) {
			analyzeOpening(player);
		}
	} else {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_IDEVENT01_TREASUREBOX());
	}
}

// Java NightmareCrateAI.java:53-79 (Java: new HashSet<>() of players, whose hashCode is the object id)
void NightmareCrateAI::analyzeOpening(Player& player) {
	if (getOwner().isInState(CreatureState::DEAD)) {
		utils::audit::AuditLogger::log(player, "attempted multiple chest looting!");
		return;
	}

	dataholders::detail::JavaHashMapOrder<int32_t, runtime::Ptr<Player>> players;
	const float range = static_cast<float>(configs::main::GroupConfig::GROUP_MAX_DISTANCE.load());
	if (player.isInGroup()) {
		for (const runtime::Ptr<Player>& member : player.getPlayerGroup()->getOnlineMembers()) {
			if (PositionUtil::isInRange(*member, getOwner(), range)) {
				players.put(member->getObjectId(), member, member->getObjectId());
			}
		}
	} else if (player.isInAlliance()) {
		for (const runtime::Ptr<Player>& member : player.getPlayerAlliance()->getOnlineMembers()) {
			if (PositionUtil::isInRange(*member, getOwner(), range)) {
				players.put(member->getObjectId(), member, member->getObjectId());
			}
		}
	} else {
		players.put(player.getObjectId(), runtime::Ptr<Player>(player), player.getObjectId());
	}
	std::vector<runtime::Ptr<Player>> registered = players.values();
	services::drop::DropRegistrationService::getInstance().registerDrop(getOwner(), player, getHighestLevel(registered), registered);
	AIActions::die(*this, player);
	services::drop::DropService::getInstance().requestDropList(runtime::Ptr<Player>(player), getObjectId());
	ActionItemNpcAI::handleUseItemFinish(player);
}

// Java NightmareCrateAI.java:81-83 (Java: max().getAsInt() throws NoSuchElementException on no player)
int32_t NightmareCrateAI::getHighestLevel(const std::vector<runtime::Ptr<Player>>& players) {
	if (players.empty())
		throw runtime::NoSuchElementException("No value present");
	int32_t highest = players.front()->getLevel();
	for (const runtime::Ptr<Player>& p : players)
		highest = std::max<int32_t>(highest, p->getLevel());
	return highest;
}

} // namespace aion::gameserver::handlers::ai::instance::nightmareCircus
