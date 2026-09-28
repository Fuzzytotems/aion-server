#include "aion/gameserver/dao/InventoryDAO.h"

#include <string>
#include <string_view>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/database/SqlTypes.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dao/detail/DaoSupport.h"
#include "aion/gameserver/dao/detail/UsedIds.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/ItemSlotInfo.h"
#include "aion/gameserver/model/items/ItemStone_ItemStoneType.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::SQLException;
using model::gameobjects::Item;
using model::gameobjects::Persistable;
using model::items::storage::StorageType;

namespace {

constexpr std::string_view SELECT_QUERY = "SELECT * FROM `inventory` WHERE `item_owner`=? AND `item_location`=?";
constexpr std::string_view SELECT_ALL_QUERY = "SELECT * FROM `inventory` WHERE `item_location`=?";
constexpr std::string_view SELECT_EQUIPPED_QUERY = "SELECT i.item_skin, i.slot, i.item_color, s.item_id godstone_item_id FROM inventory i LEFT JOIN item_stones s ON s.item_unique_id = i.item_unique_id AND s.slot = 0 AND s.category = ? WHERE i.item_owner = ? AND i.item_location = ? AND i.is_equipped = 1";
constexpr std::string_view INSERT_QUERY = "INSERT INTO `inventory` (`item_unique_id`, `item_id`, `item_count`, `item_color`, `color_expires`, `item_creator`, `expire_time`, `activation_count`, `item_owner`, `is_equipped`, is_soul_bound, `slot`, `item_location`, `enchant`, `enchant_bonus`, `item_skin`, `fusioned_item`, `optional_socket`, `optional_fusion_socket`, `charge`, `tune_count`, `rnd_bonus`, `fusion_rnd_bonus`, `tempering`, `pack_count`, `is_amplified`, `buff_skill`, `rnd_plume_bonus`) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)";
constexpr std::string_view UPDATE_QUERY = "UPDATE inventory SET item_count=?, item_color=?, color_expires=?, item_creator=?, expire_time=?, activation_count=?, item_owner=?, is_equipped=?, is_soul_bound=?, slot=?, item_location=?, enchant=?, enchant_bonus=?, item_skin=?, fusioned_item=?, optional_socket=?, optional_fusion_socket=?, charge=?, tune_count=?, rnd_bonus=?, fusion_rnd_bonus=?, tempering=?, pack_count=?, is_amplified=?, buff_skill=?, rnd_plume_bonus=? WHERE item_unique_id=?";
constexpr std::string_view DELETE_QUERY = "DELETE FROM inventory WHERE item_unique_id=?";
constexpr std::string_view DELETE_CLEAN_QUERY = "DELETE FROM inventory WHERE item_owner=? AND item_location != 2"; // exclude acc wh since item_owner (acc id) is no idfactory id
constexpr std::string_view SELECT_ACCOUNT_QUERY = "SELECT `account_id` FROM `players` WHERE `id`=?";
constexpr std::string_view SELECT_LEGION_QUERY = "SELECT `legion_id` FROM `legion_members` WHERE `player_id`=?";
constexpr std::string_view DELETE_ACCOUNT_WH = "DELETE FROM inventory WHERE item_owner=? AND item_location=2";

/** Java Integer unboxing of a null accountId/legionId in getItemOwnerId */
int32_t unbox(const std::optional<int32_t>& value) {
	if (!value)
		throw runtime::NullPointerException("Cannot invoke \"java.lang.Integer.intValue()\" because the owner id is null");
	return *value;
}

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.InventoryDAO");

void InventoryDAO::loadStorage(int32_t ownerId, model::items::storage::Storage& storage) {
	loadItems(ownerId, storage.getStorageType(), [&storage](Item& item) { storage.onLoadHandler(item); });
}

std::vector<runtime::Ref<model::gameobjects::Item>> InventoryDAO::loadItems(int32_t ownerId, model::items::storage::StorageType storageType) {
	std::vector<runtime::Ref<Item>> items;
	loadItems(ownerId, storageType, [&items](Item& item) { items.push_back(runtime::Ref<Item>(item)); });
	return items;
}

void InventoryDAO::loadItems(int32_t ownerId, model::items::storage::StorageType storageType,
	const std::function<void(model::gameobjects::Item&)>& itemConsumer) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_QUERY);
		stmt->setInt(1, ownerId);
		stmt->setInt(2, model::items::storage::getId(storageType));
		auto rs = stmt->executeQuery();
		while (rs->next()) {
			runtime::Ref<Item> item = constructItem(model::items::storage::getId(storageType), *rs);
			item->setPersistentState(Persistable::PersistentState::UPDATED);
			itemConsumer(*item);
		}
	} catch (const std::exception& e) {
		log.error("Could not load " + detail::enumName(storageType) + " items of owner " + std::to_string(ownerId), e);
	}
}

std::vector<runtime::Ref<model::gameobjects::Item>> InventoryDAO::loadBrokerItems() {
	std::vector<runtime::Ref<Item>> items;
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_ALL_QUERY);
		stmt->setInt(1, model::items::storage::getId(StorageType::BROKER));
		auto rs = stmt->executeQuery();
		while (rs->next()) {
			runtime::Ref<Item> item = constructItem(model::items::storage::getId(StorageType::BROKER), *rs);
			item->setPersistentState(Persistable::PersistentState::UPDATED);
			items.push_back(std::move(item));
		}
	} catch (const std::exception& e) {
		log.error("Could not load broker items", e);
	}
	return items;
}

std::vector<runtime::Ref<model::account::PlayerAccountData::VisibleItem>> InventoryDAO::loadVisibleEquipment(int32_t ownerId) {
	std::vector<runtime::Ref<model::account::PlayerAccountData::VisibleItem>> items;
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_EQUIPPED_QUERY);
		stmt->setInt(1, static_cast<int32_t>(model::items::ItemStone_ItemStoneType::GODSTONE));
		stmt->setInt(2, ownerId);
		stmt->setInt(3, model::items::storage::getId(StorageType::CUBE));
		auto rs = stmt->executeQuery();
		while (rs->next()) {
			int8_t slotType = model::items::getEquipmentSlotType(rs->getLong("slot"));
			if (slotType == 0)
				continue; // skip equipment like rings and secondary weapons, as they are not visible and AbstractPlayerInfoPacket supports only 16 items
			int32_t itemSkinId = rs->getInt("item_skin");
			int32_t godStoneItemId = rs->getInt("godstone_item_id");
			std::optional<int32_t> itemColor = rs->getObject<int32_t>("item_color");
			items.push_back(model::account::PlayerAccountData::VisibleItem::create(slotType, itemSkinId, godStoneItemId, itemColor));
		}
	} catch (const std::exception& e) {
		log.error("Could not load equipped items of owner " + std::to_string(ownerId), e);
	}
	return items;
}

runtime::Ref<model::gameobjects::Item> InventoryDAO::constructItem(int32_t storage, commons::database::ResultSet& rset) {
	int32_t itemUniqueId = rset.getInt("item_unique_id");
	int32_t itemId = rset.getInt("item_id");
	int64_t itemCount = rset.getLong("item_count");
	std::optional<int32_t> itemColor = rset.getObject<int32_t>("item_color"); // accepts null (which means not dyed)
	int32_t colorExpireTime = rset.getInt("color_expires");
	std::string itemCreator = rset.getString("item_creator");
	int32_t expireTime = rset.getInt("expire_time");
	int32_t activationCount = rset.getInt("activation_count");
	int32_t isEquiped = rset.getInt("is_equipped");
	int32_t isSoulBound = rset.getInt("is_soul_bound");
	int64_t slot = rset.getLong("slot");
	int32_t enchant = rset.getInt("enchant");
	int32_t enchantBonus = rset.getInt("enchant_bonus");
	int32_t itemSkin = rset.getInt("item_skin");
	int32_t fusionedItem = rset.getInt("fusioned_item");
	int32_t optionalSocket = rset.getInt("optional_socket");
	int32_t optionalFusionSocket = rset.getInt("optional_fusion_socket");
	int32_t charge = rset.getInt("charge");
	int32_t tuneCount = rset.getInt("tune_count");
	int32_t bonusStatsId = rset.getInt("rnd_bonus");
	int32_t fusionedItemBonusStatsId = rset.getInt("fusion_rnd_bonus");
	int32_t tempering = rset.getInt("tempering");
	int32_t packCount = rset.getInt("pack_count");
	int32_t isAmplified = rset.getInt("is_amplified");
	int32_t buffSkill = rset.getInt("buff_skill");
	int32_t rndPlumeBonusValue = rset.getInt("rnd_plume_bonus");
	return Item::create(itemUniqueId, itemId, itemCount, itemColor, colorExpireTime, itemCreator, expireTime, activationCount, isEquiped == 1,
		isSoulBound == 1, slot, storage, enchant, enchantBonus, itemSkin, fusionedItem, optionalSocket, optionalFusionSocket, charge, tuneCount,
		bonusStatsId, fusionedItemBonusStatsId, tempering, packCount, isAmplified == 1, buffSkill, rndPlumeBonusValue);
}

int32_t InventoryDAO::loadPlayerAccountId(int32_t playerId) {
	int32_t accountId = 0;
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_ACCOUNT_QUERY);
		stmt->setInt(1, playerId);
		auto rset = stmt->executeQuery();
		if (rset->next()) {
			accountId = rset->getInt("account_id");
		}
	} catch (const std::exception& e) {
		log.error("Could not restore accountId data for player: " + std::to_string(playerId) + " from DB: " + e.what(), e);
	}
	return accountId;
}

int32_t InventoryDAO::loadLegionId(int32_t playerId) {
	int32_t legionId = 0;
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_LEGION_QUERY);
		stmt->setInt(1, playerId);
		auto rset = stmt->executeQuery();
		if (rset->next()) {
			legionId = rset->getInt("legion_id");
		}
	} catch (const std::exception& e) {
		log.error("Failed to load legion id for player id: " + std::to_string(playerId), e);
	}
	return legionId;
}

bool InventoryDAO::store(model::gameobjects::Item& item, int32_t playerId) {
	return store(std::vector<runtime::Ptr<Item>>{runtime::Ptr<Item>(&item)}, playerId);
}

bool InventoryDAO::store(model::gameobjects::Item& item, std::optional<int32_t> playerId, std::optional<int32_t> accountId,
	std::optional<int32_t> legionId) {
	return store(std::vector<runtime::Ptr<Item>>{runtime::Ptr<Item>(&item)}, playerId, accountId, legionId);
}

bool InventoryDAO::store(model::gameobjects::player::Player& player) {
	int32_t playerId = player.getObjectId();
	std::optional<int32_t> accountId = player.getAccount() ? std::optional<int32_t>(player.getAccount()->getId()) : std::nullopt;
	std::optional<int32_t> legionId = player.getLegion() ? std::optional<int32_t>(player.getLegion()->getLegionId()) : std::nullopt;
	std::vector<runtime::Ptr<Item>> allPlayerItems = player.getDirtyItemsToUpdate();
	return store(allPlayerItems, playerId, accountId, legionId);
}

bool InventoryDAO::store(model::gameobjects::Item& item, model::gameobjects::player::Player& player) {
	int32_t playerId = player.getObjectId();
	int32_t accountId = player.getAccount()->getId();
	std::optional<int32_t> legionId = player.getLegion() ? std::optional<int32_t>(player.getLegion()->getLegionId()) : std::nullopt;
	return store(item, playerId, accountId, legionId);
}

bool InventoryDAO::store(const std::vector<runtime::Ptr<model::gameobjects::Item>>& items, int32_t playerId) {
	std::optional<int32_t> accountId;
	std::optional<int32_t> legionId;

	for (const runtime::Ptr<Item>& item : items) {
		if (!accountId && item->getItemLocation() == model::items::storage::getId(StorageType::ACCOUNT_WAREHOUSE)) {
			accountId = loadPlayerAccountId(playerId);
		}

		if (!legionId && item->getItemLocation() == model::items::storage::getId(StorageType::LEGION_WAREHOUSE)) {
			int32_t localLegionId = loadLegionId(playerId);
			if (localLegionId > 0)
				legionId = localLegionId;
		}
	}

	return store(items, playerId, accountId, legionId);
}

bool InventoryDAO::store(const std::vector<runtime::Ptr<model::gameobjects::Item>>& items, std::optional<int32_t> playerId,
	std::optional<int32_t> accountId, std::optional<int32_t> legionId) {
	std::vector<runtime::Ptr<Item>> itemsToUpdate;
	std::vector<runtime::Ptr<Item>> itemsToInsert;
	std::vector<runtime::Ptr<Item>> itemsToDelete;
	// Java: items.stream().filter(Persistable.CHANGED / NEW / DELETED)
	for (const runtime::Ptr<Item>& item : items) {
		if (item->getPersistentState() == Persistable::PersistentState::UPDATE_REQUIRED)
			itemsToUpdate.push_back(item);
	}
	for (const runtime::Ptr<Item>& item : items) {
		if (item->getPersistentState() == Persistable::PersistentState::NEW)
			itemsToInsert.push_back(item);
	}
	for (const runtime::Ptr<Item>& item : items) {
		if (item->getPersistentState() == Persistable::PersistentState::DELETED)
			itemsToDelete.push_back(item);
	}

	bool deleteResult = false;
	bool insertResult = false;
	bool updateResult = false;

	try {
		auto con = DatabaseFactory::getConnection();
		con->setAutoCommit(false);
		deleteResult = deleteItems(*con, itemsToDelete);
		insertResult = insertItems(*con, itemsToInsert, playerId, accountId, legionId);
		updateResult = updateItems(*con, itemsToUpdate, playerId, accountId, legionId);
	} catch (const SQLException& e) {
		log.error("Can't save inventory for player: " + (playerId ? std::to_string(*playerId) : std::string("null")), e);
	}

	for (const runtime::Ptr<Item>& item : items) {
		item->setPersistentState(Persistable::PersistentState::UPDATED);
	}

	if (deleteResult) {
		std::vector<int32_t> ids;
		ids.reserve(itemsToDelete.size());
		for (const runtime::Ptr<Item>& item : itemsToDelete)
			ids.push_back(item->getObjectId());
		utils::idfactory::IDFactory::getInstance().releaseObjectIds(ids, "Item");
	}

	return deleteResult && insertResult && updateResult;
}

int32_t InventoryDAO::getItemOwnerId(model::gameobjects::Item& item, std::optional<int32_t> playerId, std::optional<int32_t> accountId,
	std::optional<int32_t> legionId) {
	if (item.getItemLocation() == model::items::storage::getId(StorageType::ACCOUNT_WAREHOUSE)) {
		return unbox(accountId);
	}

	if (item.getItemLocation() == model::items::storage::getId(StorageType::LEGION_WAREHOUSE)) {
		return legionId ? *legionId : unbox(playerId);
	}

	return unbox(playerId);
}

bool InventoryDAO::insertItems(commons::database::Connection& con, const std::vector<runtime::Ptr<model::gameobjects::Item>>& items,
	std::optional<int32_t> playerId, std::optional<int32_t> accountId, std::optional<int32_t> legionId) {
	if (items.empty()) {
		return true;
	}

	try {
		auto stmt = con.prepareStatement(INSERT_QUERY);
		for (const runtime::Ptr<Item>& item : items) {
			stmt->setInt(1, item->getObjectId());
			stmt->setInt(2, item->getItemTemplate()->getTemplateId());
			stmt->setLong(3, item->getItemCount());
			stmt->setObject(4, item->getItemColor(), commons::database::Types::INTEGER); // supports inserting null value
			stmt->setInt(5, item->getColorExpireTime());
			stmt->setString(6, item->getItemCreator());
			stmt->setInt(7, item->getExpireTime());
			stmt->setInt(8, item->getActivationCount());
			stmt->setInt(9, getItemOwnerId(*item, playerId, accountId, legionId));
			stmt->setBoolean(10, item->isEquipped());
			stmt->setInt(11, item->isSoulBound() ? 1 : 0);
			stmt->setLong(12, item->getEquipmentSlot());
			stmt->setInt(13, item->getItemLocation());
			stmt->setInt(14, item->getEnchantLevel());
			stmt->setInt(15, item->getEnchantBonus());
			stmt->setInt(16, item->getItemSkinTemplate()->getTemplateId());
			stmt->setInt(17, item->getFusionedItemId());
			stmt->setInt(18, item->getOptionalSockets());
			stmt->setInt(19, item->getFusionedItemOptionalSockets());
			stmt->setInt(20, item->getChargePoints());
			stmt->setInt(21, item->getTuneCount());
			stmt->setInt(22, item->getBonusStatsId());
			stmt->setInt(23, item->getFusionedItemBonusStatsId());
			stmt->setInt(24, item->getTempering());
			stmt->setInt(25, item->getPackCount());
			stmt->setBoolean(26, item->isAmplified());
			stmt->setInt(27, item->getBuffSkill());
			stmt->setInt(28, item->getRndPlumeBonusValue());
			stmt->addBatch();
		}

		stmt->executeBatch();
		con.commit();
	} catch (const std::exception& e) {
		log.error("Failed to execute insert batch", e);
		return false;
	}
	return true;
}

bool InventoryDAO::updateItems(commons::database::Connection& con, const std::vector<runtime::Ptr<model::gameobjects::Item>>& items,
	std::optional<int32_t> playerId, std::optional<int32_t> accountId, std::optional<int32_t> legionId) {
	if (items.empty()) {
		return true;
	}

	try {
		auto stmt = con.prepareStatement(UPDATE_QUERY);
		for (const runtime::Ptr<Item>& item : items) {
			stmt->setLong(1, item->getItemCount());
			stmt->setObject(2, item->getItemColor(), commons::database::Types::INTEGER); // supports inserting null value
			stmt->setInt(3, item->getColorExpireTime());
			stmt->setString(4, item->getItemCreator());
			stmt->setInt(5, item->getExpireTime());
			stmt->setInt(6, item->getActivationCount());
			stmt->setInt(7, getItemOwnerId(*item, playerId, accountId, legionId));
			stmt->setBoolean(8, item->isEquipped());
			stmt->setInt(9, item->isSoulBound() ? 1 : 0);
			stmt->setLong(10, item->getEquipmentSlot());
			stmt->setInt(11, item->getItemLocation());
			stmt->setInt(12, item->getEnchantLevel());
			stmt->setInt(13, item->getEnchantBonus());
			stmt->setInt(14, item->getItemSkinTemplate()->getTemplateId());
			stmt->setInt(15, item->getFusionedItemId());
			stmt->setInt(16, item->getOptionalSockets());
			stmt->setInt(17, item->getFusionedItemOptionalSockets());
			stmt->setInt(18, item->getChargePoints());
			stmt->setInt(19, item->getTuneCount());
			stmt->setInt(20, item->getBonusStatsId());
			stmt->setInt(21, item->getFusionedItemBonusStatsId());
			stmt->setInt(22, item->getTempering());
			stmt->setInt(23, item->getPackCount());
			stmt->setBoolean(24, item->isAmplified());
			stmt->setInt(25, item->getBuffSkill());
			stmt->setInt(26, item->getRndPlumeBonusValue());
			stmt->setInt(27, item->getObjectId());
			stmt->addBatch();
		}

		stmt->executeBatch();
		con.commit();
	} catch (const std::exception& e) {
		log.error("Failed to execute update batch", e);
		return false;
	}
	return true;
}

bool InventoryDAO::deleteItems(commons::database::Connection& con, const std::vector<runtime::Ptr<model::gameobjects::Item>>& items) {
	if (items.empty()) {
		return true;
	}

	try {
		auto stmt = con.prepareStatement(DELETE_QUERY);
		for (const runtime::Ptr<Item>& item : items) {
			stmt->setInt(1, item->getObjectId());
			stmt->addBatch();
		}
		stmt->executeBatch();
		con.commit();
	} catch (const std::exception& e) {
		log.error("Failed to execute delete batch", e);
		return false;
	}
	return true;
}

bool InventoryDAO::deletePlayerOrLegionItems(int32_t playerOrLegionId) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(DELETE_CLEAN_QUERY);
		stmt->setInt(1, playerOrLegionId);
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Error deleting all player or legion items. playerOrLegionId: " + std::to_string(playerOrLegionId), e);
		return false;
	}
	return true;
}

void InventoryDAO::deleteAccountWH(int32_t accountId) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(DELETE_ACCOUNT_WH);
		stmt->setInt(1, accountId);
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Error deleting all items from account WH. AccountId: " + std::to_string(accountId), e);
	}
}

std::vector<int32_t> InventoryDAO::getUsedIDs() {
	return detail::getUsedIDs(log, "SELECT item_unique_id FROM inventory", "item_unique_id", "Can't get list of IDs from inventory table");
}

} // namespace aion::gameserver::dao
