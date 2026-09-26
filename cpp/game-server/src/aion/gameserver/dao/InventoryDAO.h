#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/commons/database/Connection.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/items/storage/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author ATracer
 */
class InventoryDAO {
public:
	static void loadStorage(int32_t ownerId, model::items::storage::Storage& storage);
	static std::vector<runtime::Ref<model::gameobjects::Item>> loadItems(int32_t ownerId, model::items::storage::StorageType storageType);
private:
	static void loadItems(int32_t ownerId, model::items::storage::StorageType storageType,
		const std::function<void(model::gameobjects::Item&)>& itemConsumer);
public:
	static std::vector<runtime::Ref<model::gameobjects::Item>> loadBrokerItems();
	static std::vector<runtime::Ref<model::account::PlayerAccountData::VisibleItem>> loadVisibleEquipment(int32_t ownerId);
private:
	static runtime::Ref<model::gameobjects::Item> constructItem(int32_t storage, commons::database::ResultSet& rset);
	static int32_t loadPlayerAccountId(int32_t playerId);
public:
	static int32_t loadLegionId(int32_t playerId);
	static bool store(model::gameobjects::Item& item, int32_t playerId);
	static bool store(model::gameobjects::Item& item, std::optional<int32_t> playerId, std::optional<int32_t> accountId,
		std::optional<int32_t> legionId);
	static bool store(model::gameobjects::player::Player& player);
	static bool store(model::gameobjects::Item& item, model::gameobjects::player::Player& player);
	static bool store(const std::vector<runtime::Ptr<model::gameobjects::Item>>& items, int32_t playerId);
	static bool store(const std::vector<runtime::Ptr<model::gameobjects::Item>>& items, std::optional<int32_t> playerId,
		std::optional<int32_t> accountId, std::optional<int32_t> legionId);
private:
	static int32_t getItemOwnerId(model::gameobjects::Item& item, std::optional<int32_t> playerId, std::optional<int32_t> accountId,
		std::optional<int32_t> legionId);
	static bool insertItems(commons::database::Connection& con, const std::vector<runtime::Ptr<model::gameobjects::Item>>& items,
		std::optional<int32_t> playerId, std::optional<int32_t> accountId, std::optional<int32_t> legionId);
	static bool updateItems(commons::database::Connection& con, const std::vector<runtime::Ptr<model::gameobjects::Item>>& items,
		std::optional<int32_t> playerId, std::optional<int32_t> accountId, std::optional<int32_t> legionId);
	static bool deleteItems(commons::database::Connection& con, const std::vector<runtime::Ptr<model::gameobjects::Item>>& items);
public:
	/** Since inventory is not using FK - need to clean items */
	static bool deletePlayerOrLegionItems(int32_t playerOrLegionId);
	static void deleteAccountWH(int32_t accountId);
	static std::vector<int32_t> getUsedIDs();
};

} // namespace aion::gameserver::dao
