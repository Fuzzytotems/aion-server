#include "aion/gameserver/dao/InventoryDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view SELECT_QUERY = "SELECT * FROM `inventory` WHERE `item_owner`=? AND `item_location`=?";
constexpr std::string_view SELECT_ALL_QUERY = "SELECT * FROM `inventory` WHERE `item_location`=?";
constexpr std::string_view SELECT_EQUIPPED_QUERY = "SELECT i.item_skin, i.slot, i.item_color, s.item_id godstone_item_id FROM inventory i LEFT JOIN item_stones s ON s.item_unique_id = i.item_unique_id AND s.slot = 0 AND s.category = ? WHERE i.item_owner = ? AND i.item_location = ? AND i.is_equipped = 1";
constexpr std::string_view INSERT_QUERY = "INSERT INTO `inventory` (`item_unique_id`, `item_id`, `item_count`, `item_color`, `color_expires`, `item_creator`, `expire_time`, `activation_count`, `item_owner`, `is_equipped`, is_soul_bound, `slot`, `item_location`, `enchant`, `enchant_bonus`, `item_skin`, `fusioned_item`, `optional_socket`, `optional_fusion_socket`, `charge`, `tune_count`, `rnd_bonus`, `fusion_rnd_bonus`, `tempering`, `pack_count`, `is_amplified`, `buff_skill`, `rnd_plume_bonus`) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)";
constexpr std::string_view UPDATE_QUERY = "UPDATE inventory SET item_count=?, item_color=?, color_expires=?, item_creator=?, expire_time=?, activation_count=?, item_owner=?, is_equipped=?, is_soul_bound=?, slot=?, item_location=?, enchant=?, enchant_bonus=?, item_skin=?, fusioned_item=?, optional_socket=?, optional_fusion_socket=?, charge=?, tune_count=?, rnd_bonus=?, fusion_rnd_bonus=?, tempering=?, pack_count=?, is_amplified=?, buff_skill=?, rnd_plume_bonus=? WHERE item_unique_id=?";
constexpr std::string_view DELETE_QUERY = "DELETE FROM inventory WHERE item_unique_id=?";
constexpr std::string_view DELETE_CLEAN_QUERY = "DELETE FROM inventory WHERE item_owner=? AND item_location != 2";
constexpr std::string_view SELECT_ACCOUNT_QUERY = "SELECT `account_id` FROM `players` WHERE `id`=?";
constexpr std::string_view SELECT_LEGION_QUERY = "SELECT `legion_id` FROM `legion_members` WHERE `player_id`=?";
constexpr std::string_view DELETE_ACCOUNT_WH = "DELETE FROM inventory WHERE item_owner=? AND item_location=2";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.InventoryDAO");

void InventoryDAO::loadStorage(int32_t ownerId, model::items::storage::Storage& storage) {
	AION_UNPORTED();
}

std::vector<runtime::Ref<model::gameobjects::Item>> InventoryDAO::loadItems(int32_t ownerId, model::items::storage::StorageType storageType) {
	AION_UNPORTED();
}

void InventoryDAO::loadItems(int32_t ownerId, model::items::storage::StorageType storageType,
	const std::function<void(model::gameobjects::Item&)>& itemConsumer) {
	AION_UNPORTED();
}

std::vector<runtime::Ref<model::gameobjects::Item>> InventoryDAO::loadBrokerItems() {
	AION_UNPORTED();
}

std::vector<runtime::Ref<model::account::PlayerAccountData::VisibleItem>> InventoryDAO::loadVisibleEquipment(int32_t ownerId) {
	AION_UNPORTED();
}

runtime::Ref<model::gameobjects::Item> InventoryDAO::constructItem(int32_t storage, commons::database::ResultSet& rset) {
	AION_UNPORTED();
}

int32_t InventoryDAO::loadPlayerAccountId(int32_t playerId) {
	AION_UNPORTED();
}

int32_t InventoryDAO::loadLegionId(int32_t playerId) {
	AION_UNPORTED();
}

bool InventoryDAO::store(model::gameobjects::Item& item, int32_t playerId) {
	AION_UNPORTED();
}

bool InventoryDAO::store(model::gameobjects::Item& item, std::optional<int32_t> playerId, std::optional<int32_t> accountId,
	std::optional<int32_t> legionId) {
	AION_UNPORTED();
}

bool InventoryDAO::store(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool InventoryDAO::store(model::gameobjects::Item& item, model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool InventoryDAO::store(const std::vector<runtime::Ptr<model::gameobjects::Item>>& items, int32_t playerId) {
	AION_UNPORTED();
}

bool InventoryDAO::store(const std::vector<runtime::Ptr<model::gameobjects::Item>>& items, std::optional<int32_t> playerId,
	std::optional<int32_t> accountId, std::optional<int32_t> legionId) {
	AION_UNPORTED();
}

int32_t InventoryDAO::getItemOwnerId(model::gameobjects::Item& item, std::optional<int32_t> playerId, std::optional<int32_t> accountId,
	std::optional<int32_t> legionId) {
	AION_UNPORTED();
}

bool InventoryDAO::insertItems(commons::database::Connection& con, const std::vector<runtime::Ptr<model::gameobjects::Item>>& items,
	std::optional<int32_t> playerId, std::optional<int32_t> accountId, std::optional<int32_t> legionId) {
	AION_UNPORTED();
}

bool InventoryDAO::updateItems(commons::database::Connection& con, const std::vector<runtime::Ptr<model::gameobjects::Item>>& items,
	std::optional<int32_t> playerId, std::optional<int32_t> accountId, std::optional<int32_t> legionId) {
	AION_UNPORTED();
}

bool InventoryDAO::deleteItems(commons::database::Connection& con, const std::vector<runtime::Ptr<model::gameobjects::Item>>& items) {
	AION_UNPORTED();
}

bool InventoryDAO::deletePlayerOrLegionItems(int32_t playerOrLegionId) {
	AION_UNPORTED();
}

void InventoryDAO::deleteAccountWH(int32_t accountId) {
	AION_UNPORTED();
}

std::vector<int32_t> InventoryDAO::getUsedIDs() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
