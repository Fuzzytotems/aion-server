#include "aion/gameserver/dao/PlayerRegisteredItemsDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view CLEAN_PLAYER_QUERY = "DELETE FROM `player_registered_items` WHERE `player_id` = ?";
constexpr std::string_view SELECT_QUERY = "SELECT * FROM `player_registered_items` WHERE `player_id`=?";
constexpr std::string_view INSERT_QUERY = "INSERT INTO `player_registered_items` (`expire_time`,`color`,`color_expires`,`owner_use_count`,`visitor_use_count`,`x`,`y`,`z`,`h`,`area`,`room`,`player_id`,`item_unique_id`,`item_id`) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?)";
constexpr std::string_view UPDATE_QUERY = "UPDATE `player_registered_items` SET `expire_time`=?,`color`=?,`color_expires`=?,`owner_use_count`=?,`visitor_use_count`=?,`x`=?,`y`=?,`z`=?,`h`=?,`area`=?,`room`=? WHERE `player_id`=? AND `item_unique_id`=? AND `item_id`=?";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `player_registered_items` WHERE `item_unique_id` = ?";
constexpr std::string_view RESET_QUERY = "UPDATE `player_registered_items` SET x=0,y=0,z=0,h=0,area='NONE' WHERE `player_id`=? AND `area` != 'DECOR'";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerRegisteredItemsDAO");

std::vector<int32_t> PlayerRegisteredItemsDAO::getUsedIDs() {
	AION_UNPORTED();
}

void PlayerRegisteredItemsDAO::loadRegistry(model::house::HouseRegistry& registry) {
	AION_UNPORTED();
}

runtime::Ref<model::gameobjects::HouseObject> PlayerRegisteredItemsDAO::constructObject(model::house::HouseRegistry& registry,
	commons::database::ResultSet& rset) {
	AION_UNPORTED();
}

runtime::Ref<model::gameobjects::HouseDecoration> PlayerRegisteredItemsDAO::createDecoration(model::house::HouseRegistry& registry,
	commons::database::ResultSet& rset) {
	AION_UNPORTED();
}

bool PlayerRegisteredItemsDAO::store(model::house::HouseRegistry& registry, int32_t playerId) {
	AION_UNPORTED();
}

bool PlayerRegisteredItemsDAO::storeObjects(commons::database::Connection& con,
	const std::vector<runtime::Ptr<model::gameobjects::HouseObject>>& objects, int32_t playerId, bool isNew) {
	AION_UNPORTED();
}

bool PlayerRegisteredItemsDAO::storeDecors(commons::database::Connection& con,
	const std::vector<runtime::Ptr<model::gameobjects::HouseDecoration>>& decors, int32_t playerId, bool isNew) {
	AION_UNPORTED();
}

bool PlayerRegisteredItemsDAO::deleteObjects(commons::database::Connection& con,
	const std::vector<runtime::Ptr<model::gameobjects::HouseObject>>& objects) {
	AION_UNPORTED();
}

bool PlayerRegisteredItemsDAO::deleteDecors(commons::database::Connection& con,
	const std::vector<runtime::Ptr<model::gameobjects::HouseDecoration>>& decors) {
	AION_UNPORTED();
}

bool PlayerRegisteredItemsDAO::deletePlayerItems(int32_t playerId) {
	AION_UNPORTED();
}

void PlayerRegisteredItemsDAO::resetRegistry(int32_t playerId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
