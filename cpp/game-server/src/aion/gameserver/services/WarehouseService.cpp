#include "aion/gameserver/services/WarehouseService.h"

#include <vector>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
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

void WarehouseService::expandWarehouse(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

void WarehouseService::expand(model::gameobjects::player::Player& player, bool isNpcExpand) {
	AION_UNPORTED();
}

bool WarehouseService::canExpandByTicket(model::gameobjects::player::Player& player, int32_t ticketLevel) {
	AION_UNPORTED();
}

bool WarehouseService::canExpand(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

int32_t WarehouseService::getCompletedWhQuests(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
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
