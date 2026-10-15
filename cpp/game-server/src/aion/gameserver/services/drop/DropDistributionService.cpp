#include "aion/gameserver/services/drop/DropDistributionService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/model/actions/PlayerMode.h"
#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/model/gameobjects/DropNpc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GROUP_LOOT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/collections/HashSet.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/services/drop/DropRegistrationService.h"
#include "aion/gameserver/services/drop/DropService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::services::drop {

using model::drop::DropItem;
using model::gameobjects::DropNpc;
using model::gameobjects::player::Player;
using network::aion::serverpackets::SM_GROUP_LOOT;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.drop.DropDistributionService");

DropDistributionService::DropDistributionService() = default;

DropDistributionService::~DropDistributionService() = default;

DropDistributionService& DropDistributionService::getInstance() {
	static DropDistributionService instance; // Java SingletonHolder
	return instance;
}

void DropDistributionService::handleRollOrBid(runtime::Ptr<Player> player, int32_t mode, int32_t roll, int64_t bid, int32_t itemId, int32_t npcObjId,
	int32_t index) {
	if (!player)
		return;
	runtime::Ptr<DropNpc> dropNpc = DropRegistrationService::getInstance().getDropRegistrationMap().get(npcObjId);
	if (!dropNpc)
		return;
	runtime::Ptr<runtime::RcHashSet<runtime::Ref<DropItem>>> dropItems = DropRegistrationService::getInstance().getCurrentDropMap().get(npcObjId);
	if (!dropItems)
		return;
	runtime::Ref<DropItem> requestedItem = nullptr;
	SYNCHRONIZED(*dropItems) {
		for (const runtime::Ptr<DropItem>& dropItem : dropItems->snapshot())
			if (dropItem->getIndex() == dropNpc->getCurrentIndex()) {
				requestedItem = dropItem;
				break;
			}
	}
	if (!requestedItem)
		return;
	if (mode == 2)
		handleRoll(*player, roll, itemId, *requestedItem, *dropNpc);
	else if (mode == 3)
		handleBid(*player, bid, itemId, *requestedItem, *dropNpc);
	else
		log.warn("{} requested invalid distributionMode {} for dropItem[itemId={}, index={}, npcObjId={}]", player->toString(), mode, itemId, index,
			npcObjId);
}

void DropDistributionService::handleRoll(Player& player, int32_t roll, int32_t itemId, DropItem& requestedItem, DropNpc& dropNpc) {
	int32_t luck = 0;
	if (roll == 0) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_DICE_GIVEUP_ME());
	} else {
		luck = commons::utils::Rnd::get(1, dropNpc.getMaxRoll());
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_DICE_RESULT_ME(luck, dropNpc.getMaxRoll()));
	}
	for (const runtime::Ptr<Player>& member : dropNpc.getInRangePlayers()->snapshot()) {
		if (!member) {
			log.warn("member null Owner is in group? {} Owner is in Alliance? {}", player.isInGroup(), player.isInAlliance());
			continue;
		}
		PacketSendUtility::sendPacket(*member, SM_GROUP_LOOT(dropNpc.getLootingTeamId(), member->getObjectId(), itemId,
												   static_cast<int32_t>(requestedItem.getCount()), dropNpc.getObjectId(), dropNpc.getDistributionId(), luck,
												   requestedItem.getIndex()));
		if (!player.equals(*member) && member->isOnline()) {
			if (roll == 0) {
				PacketSendUtility::sendPacket(*member, SM_SYSTEM_MESSAGE::STR_MSG_DICE_GIVEUP_OTHER(player.getName()));
			} else {
				PacketSendUtility::sendPacket(*member, SM_SYSTEM_MESSAGE::STR_MSG_DICE_RESULT_OTHER(player.getName(), luck, dropNpc.getMaxRoll()));
			}
		}
	}
	distributeLoot(player, luck, itemId, requestedItem, dropNpc);
}

void DropDistributionService::handleBid(Player& player, int64_t bid, int32_t itemId, DropItem& requestedItem, DropNpc& dropNpc) {
	if ((bid > 0 && player.getInventory().getKinah() < bid) || bid < 0 || bid > 999999999)
		bid = 0;
	PacketSendUtility::sendPacket(player, bid > 0 ? SM_SYSTEM_MESSAGE::STR_MSG_PAY_RESULT_ME() : SM_SYSTEM_MESSAGE::STR_MSG_PAY_GIVEUP_ME());
	for (const runtime::Ptr<Player>& member : dropNpc.getInRangePlayers()->snapshot()) {
		PacketSendUtility::sendPacket(*member, SM_GROUP_LOOT(dropNpc.getLootingTeamId(), member->getObjectId(), itemId,
												   static_cast<int32_t>(requestedItem.getCount()), dropNpc.getObjectId(), dropNpc.getDistributionId(), bid,
												   requestedItem.getIndex()));
		if (!player.equals(*member) && member->isOnline()) {
			if (bid > 0) {
				PacketSendUtility::sendPacket(*member, SM_SYSTEM_MESSAGE::STR_MSG_PAY_RESULT_OTHER(player.getName()));
			} else {
				PacketSendUtility::sendPacket(*member, SM_SYSTEM_MESSAGE::STR_MSG_PAY_GIVEUP_OTHER(player.getName()));
			}
		}
	}
	distributeLoot(player, bid, itemId, requestedItem, dropNpc);
}

void DropDistributionService::distributeLoot(Player& player, int64_t luckyPlayer, int32_t itemId, DropItem& requestedItem, DropNpc& dropNpc) {
	player.unsetPlayerMode(model::actions::PlayerMode::IN_ROLL);
	// Removes player from ARRAY once they have rolled or bid
	if (dropNpc.containsPlayerStatus(player))
		dropNpc.delPlayerStatus(player);

	if (luckyPlayer > requestedItem.getHighestValue()) {
		requestedItem.setHighestValue(luckyPlayer);
		requestedItem.setWinningPlayer(runtime::Ptr<Player>(player));
	}

	if (!dropNpc.getPlayerStatus().isEmpty())
		return;

	for (const runtime::Ptr<Player>& member : dropNpc.getInRangePlayers()->snapshot()) {
		if (!member) {
			continue;
		}
		if (!requestedItem.getWinningPlayer()) {
			PacketSendUtility::sendPacket(*member, SM_SYSTEM_MESSAGE::STR_MSG_PAY_ALL_GIVEUP());
		}
		runtime::Ptr<Player> winningPlayer = requestedItem.getWinningPlayer();
		PacketSendUtility::sendPacket(*member,
			SM_GROUP_LOOT(dropNpc.getLootingTeamId(), winningPlayer ? winningPlayer->getObjectId() : 1, itemId, static_cast<int32_t>(requestedItem.getCount()),
				dropNpc.getObjectId(), dropNpc.getDistributionId(), static_cast<int64_t>(static_cast<int32_t>(0xFFFFFFFF)) /* Java: the int literal 0xFFFFFFFF (-1) widened to long */, requestedItem.getIndex()));
	}

	runtime::Ptr<model::team::common::legacy::LootGroupRules> lgr = dropNpc.getLootGroupRules();
	if (lgr)
		lgr->removeItemToBeDistributed(requestedItem);

	// Check if there is a Winning Player registered if not all members must have passed...
	if (!requestedItem.getWinningPlayer()) {
		requestedItem.isFreeForAll(true);
		if (lgr && !lgr->getItemsToBeDistributed().isEmpty())
			DropService::getInstance().canDistribute(player, *lgr->getItemsToBeDistributed().getFirst());
		return;
	}

	requestedItem.isDistributeItem(true);
	DropService::getInstance().requestDropItem(player, dropNpc.getObjectId(), dropNpc.getCurrentIndex());
	if (lgr && !lgr->getItemsToBeDistributed().isEmpty())
		DropService::getInstance().canDistribute(player, *lgr->getItemsToBeDistributed().getFirst());
}

} // namespace aion::gameserver::services::drop
