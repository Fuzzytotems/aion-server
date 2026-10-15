#include "aion/gameserver/services/WarehouseService.h"

#include <optional>
#include <string>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/WarehouseExpandData.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/gameobjects/player/RequestResponseHandler.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/templates/StorageExpansionTemplate.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_WAREHOUSE_INFO.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::services {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
// anonymous RequestResponseHandler at WarehouseService.java:54 (com.aionemu.gameserver.services.WarehouseService$1); local responseHandler; storage:
// stored in ResponseRequester

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.WarehouseService");

namespace {

using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::player::Player;
using network::aion::serverpackets::SM_QUESTION_WINDOW;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

/**
 * Java: the anonymous RequestResponseHandler<Npc> of expandWarehouse (WarehouseService.java:54-63, fieldmap key WarehouseService$1), stored in
 * the player's ResponseRequester until he answers STR_WAREHOUSE_EXPAND_WARNING (CubeExpandService's pattern)
 */
class WarehouseService_RequestResponseHandler final : public model::gameobjects::player::RequestResponseHandler {
	AION_MAKE_REF_FRIEND
public:
	const std::optional<int32_t> price; // captured local Integer price [captured variable: final nullable scalar]

	static runtime::Ref<WarehouseService_RequestResponseHandler> create(Npc& npc, std::optional<int32_t> priceValue) {
		return runtime::makeRef<WarehouseService_RequestResponseHandler>(npc, priceValue);
	}

	// Java WarehouseService.java:56-61
	void acceptRequest(runtime::Ptr<Creature> /*requester*/, Player& responder) override {
		// Java unboxes the Integer; expandWarehouse never creates the handler for a null price
		if (!price)
			throw runtime::NullPointerException("price");
		if (responder.getInventory().tryDecreaseKinah(*price))
			WarehouseService::expand(responder, true);
		else
			PacketSendUtility::sendPacket(responder, SM_SYSTEM_MESSAGE::STR_WAREHOUSE_EXPAND_NOT_ENOUGH_MONEY()); // warehouse and cube use the same msg..
	}

protected:
	WarehouseService_RequestResponseHandler(Npc& npc, std::optional<int32_t> priceValue)
		: RequestResponseHandler(runtime::Ptr<Creature>(npc)), price(priceValue) {}
	~WarehouseService_RequestResponseHandler() override = default;
};

} // namespace

// Java WarehouseService.java:34-70
void WarehouseService::expandWarehouse(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc) {
	const model::templates::StorageExpansionTemplate* expansionTemplate =
		dataholders::DataManager::WAREHOUSEEXPANDER_DATA->getWarehouseExpansionTemplate(npc.getNpcId());
	if (expansionTemplate == nullptr) {
		log.warn("Warehouse expansion template could not be found for " + npc.toString());
		return;
	}

	if (!canExpand(player))
		return;
	int32_t newNpcExpansions = player.getWhNpcExpands() + 1;
	int32_t minExpansionLevel = expansionTemplate->getMinExpansionLevel();
	if (newNpcExpansions < minExpansionLevel) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_EXTEND_CHAR_WAREHOUSE_CANT_EXTEND_DUE_TO_MINIMUM_EXTEND_LEVEL_BY_THIS_NPC(
												  npc.getObjectTemplate()->getL10n(), minExpansionLevel - 1));
		return;
	}
	std::optional<int32_t> price = expansionTemplate->getPrice(newNpcExpansions);
	if (!price || newNpcExpansions > expansionTemplate->getMaxExpansionLevel()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_EXTEND_CHAR_WAREHOUSE_CANT_EXTEND_MORE_DUE_TO_MAXIMUM_EXTEND_LEVEL_BY_THIS_NPC(
												  npc.getObjectTemplate()->getL10n(), expansionTemplate->getMaxExpansionLevel()));
		return;
	}
	runtime::Ref<WarehouseService_RequestResponseHandler> responseHandler = WarehouseService_RequestResponseHandler::create(npc, price);

	bool result = player.getResponseRequester().putRequest(SM_QUESTION_WINDOW::STR_WAREHOUSE_EXPAND_WARNING, responseHandler);
	if (result) {
		PacketSendUtility::sendPacket(player, SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_WAREHOUSE_EXPAND_WARNING, 0, 0, std::to_string(*price)));
	}
}

// Java WarehouseService.java:72-85
void WarehouseService::expand(model::gameobjects::player::Player& player, bool isNpcExpand) {
	if (!canExpand(player))
		return;
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_EXTEND_CHAR_WAREHOUSE_SIZE_EXTENDED(8)); // 8 Slots added
	model::gameobjects::player::PlayerCommonData& pcd = *player.getCommonData();
	if (isNpcExpand) {
		pcd.setWhNpcExpands(pcd.getWhNpcExpands() + 1);
	} else {
		pcd.setWhBonusExpands(pcd.getWhBonusExpands() + 1);
	}
	player.setWarehouseLimit();

	sendWarehouseInfo(player, false);
}

// Java WarehouseService.java:87-95
bool WarehouseService::canExpandByTicket(model::gameobjects::player::Player& player, int32_t ticketLevel) {
	if (!canExpand(player))
		return false;
	if (player.getWhBonusExpands() - getCompletedWhQuests(player) >= ticketLevel) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_EXTEND_CHAR_WAREHOUSE_CANT_EXTEND_MORE());
		return false;
	}
	return true;
}

// Java WarehouseService.java:97-106
bool WarehouseService::canExpand(model::gameobjects::player::Player& player) {
	int32_t newExpansions = player.getWarehouseExpansions() + 1;
	if (newExpansions < 0)
		return false;
	if (newExpansions > MAX_EXPAND) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_EXTEND_CHAR_WAREHOUSE_CANT_EXTEND_MORE());
		return false;
	}
	return true;
}

// Java WarehouseService.java:108-117
int32_t WarehouseService::getCompletedWhQuests(model::gameobjects::player::Player& player) {
	int32_t result = 0;
	model::gameobjects::player::QuestStateList& qs = *player.getQuestStateList();
	const int32_t questIds[] = {1987, 2985};
	for (int32_t q : questIds) {
		if (qs.getQuestState(q) != nullptr && qs.getQuestState(q)->getStatus() == questEngine::model::QuestStatus::COMPLETE)
			result++;
	}
	return result;
}

void WarehouseService::sendWarehouseInfo(model::gameobjects::player::Player& player, bool sendAccountWh) {
	using model::items::storage::StorageType;
	using network::aion::serverpackets::SM_WAREHOUSE_INFO;
	using utils::PacketSendUtility;
	const int32_t regularWarehouse = model::items::storage::getId(StorageType::REGULAR_WAREHOUSE);
	const int32_t accountWarehouse = model::items::storage::getId(StorageType::ACCOUNT_WAREHOUSE);
	std::vector<runtime::Ptr<model::gameobjects::Item>> items = player.getStorage(regularWarehouse)->getItems();

	int32_t whSize = player.getWarehouseExpansions();
	int32_t itemsSize = static_cast<int32_t>(items.size());

	// regular warehouse
	bool firstPacket = true;
	if (itemsSize != 0) {
		int32_t index = 0;

		while (index + 10 < itemsSize) {
			PacketSendUtility::sendPacket(player,
				SM_WAREHOUSE_INFO(std::vector(items.begin() + index, items.begin() + index + 10), regularWarehouse, whSize, firstPacket, player));
			index += 10;
			firstPacket = false;
		}
		PacketSendUtility::sendPacket(player,
			SM_WAREHOUSE_INFO(std::vector(items.begin() + index, items.end()), regularWarehouse, whSize, firstPacket, player));
	}

	// Java: new SM_WAREHOUSE_INFO(null, ...) is the empty part (SM_WAREHOUSE_INFO.h)
	PacketSendUtility::sendPacket(player, SM_WAREHOUSE_INFO({}, regularWarehouse, whSize, false, player));

	if (sendAccountWh) {
		// account warehouse
		PacketSendUtility::sendPacket(player,
			SM_WAREHOUSE_INFO(player.getStorage(accountWarehouse)->getItemsWithKinah(), accountWarehouse, 0, true, player));
	}

	PacketSendUtility::sendPacket(player, SM_WAREHOUSE_INFO({}, accountWarehouse, 0, false, player));
}

} // namespace aion::gameserver::services
