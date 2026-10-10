#include "aion/gameserver/handlers/ai/ChestAI.h"

#include <algorithm>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/configs/main/DropConfig.h"
#include "aion/gameserver/configs/main/GroupConfig.h"
#include "aion/gameserver/dataholders/ChestData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/templates/chest/ChestTemplate.h"
#include "aion/gameserver/model/templates/chest/KeyItem.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/drop/DropRegistrationService.h"
#include "aion/gameserver/services/drop/DropService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

namespace aion::gameserver::handlers::ai {

AION_AI(ChestAI, "chest");

// Java: LoggerFactory.getLogger(ChestAI.class)
const commons::logging::Logger ChestAI::log = commons::logging::LoggerFactory::getLogger("ai.ChestAI");

// Java ChestAI.java:39-48
void ChestAI::handleDialogStart(Player& player) {
	chestTemplate.set(DataManager::CHEST_DATA->getChestTemplate(getNpcId()));

	if (chestTemplate.get() == nullptr) {
		log.warn("Missing chest template or incorrect AI for npc " + std::to_string(getNpcId()));
		return;
	}
	ActionItemNpcAI::handleDialogStart(player);
}

// Java ChestAI.java:50-77
void ChestAI::handleUseItemFinish(Player& player) {
	if (tryOpening(player)) {
		if (getOwner().isInState(CreatureState::DEAD)) {
			utils::audit::AuditLogger::log(player, "attempted multiple chest looting!");
			return;
		}

		// Java: new HashSet<>() of players, whose hashCode is the object id
		dataholders::detail::JavaHashMapOrder<int32_t, runtime::Ptr<Player>> playerSet;
		runtime::Ptr<model::team::TemporaryPlayerTeam> playerTeam = player.getCurrentTeam();
		if (playerTeam != nullptr) {
			int32_t range = configs::main::DropConfig::DISABLE_RANGE_CHECK_MAPS.get()->contains(getPosition()->getMapId())
				? 9999
				: configs::main::GroupConfig::GROUP_MAX_DISTANCE.load();
			for (const runtime::Ptr<Player>& member : playerTeam->getOnlineMembers()) {
				if (PositionUtil::isInRange(*member, getOwner(), static_cast<float>(range)))
					playerSet.put(member->getObjectId(), member, member->getObjectId());
			}
		}
		std::vector<runtime::Ptr<Player>> players = playerSet.values();
		if (players.empty()) // no team or nobody was in range
			players.push_back(runtime::Ptr<Player>(player));
		services::drop::DropRegistrationService::getInstance().registerDrop(getOwner(), player, getHighestLevel(players), players);
		AIActions::die(*this, player);
		services::drop::DropService::getInstance().requestDropList(runtime::Ptr<Player>(player), getObjectId());
		ActionItemNpcAI::handleUseItemFinish(player);
	} else {
		PacketSendUtility::sendMonologue(player, 1111301); // I'll need a key to open this.
	}
}

// Java ChestAI.java:79-104
bool ChestAI::tryOpening(Player& player) {
	const ChestTemplate* chest = chestTemplate.get();
	if (chest == nullptr) // Java: chestTemplate.getKeyItems() on a null template (a use finished without a dialog start)
		throw runtime::NullPointerException("ChestAI.chestTemplate is null");
	const std::vector<KeyItem>& keyItems = chest->getKeyItems();
	for (const KeyItem& keyItem : keyItems) { // check if enough key items are available
		if (keyItem.getItemIds().has_value()
			&& std::ranges::any_of(*keyItem.getItemIds(), [](int32_t itemId) { return itemId == 0; })) // chest can be opened w/o keys
			return true;
		if (!keyItem.getItemIds().has_value()) // Java: the for-each over a null list
			throw runtime::NullPointerException("KeyItem.itemIds is null");
		int64_t availableKeys = 0;
		for (int32_t keyItemId : *keyItem.getItemIds()) {
			availableKeys += player.getInventory().getItemCountByItemId(keyItemId);
		}
		if (availableKeys < keyItem.getCount())
			return false;
	}
	for (const KeyItem& keyItem : keyItems) { // remove key items
		int64_t keyCountToDecrease = keyItem.getCount();
		for (int32_t keyItemId : *keyItem.getItemIds()) { // non-null: checked by the loop above
			int64_t availableKeys = player.getInventory().getItemCountByItemId(keyItemId);
			if (availableKeys >= keyCountToDecrease) {
				player.getInventory().decreaseByItemId(keyItemId, keyCountToDecrease);
				keyCountToDecrease = 0;
			} else {
				keyCountToDecrease -= availableKeys;
				player.getInventory().decreaseByItemId(keyItemId, availableKeys);
			}
		}
	}
	return true;
}

// Java ChestAI.java:106-108
int32_t ChestAI::getHighestLevel(const std::vector<runtime::Ptr<Player>>& players) {
	int32_t highest = players.at(0)->getLevel(); // Java: max().getAsInt() of a non-empty stream
	for (const runtime::Ptr<Player>& player : players)
		highest = std::max<int32_t>(highest, player->getLevel());
	return highest;
}

} // namespace aion::gameserver::handlers::ai
