#include "aion/gameserver/dao/ItemStoneListDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view INSERT_QUERY = "INSERT INTO `item_stones` (`item_unique_id`, `item_id`, `slot`, `category`, `polishNumber`, `polishCharge`, `proc_count`) VALUES (?,?,?,?,?,?,?)";
constexpr std::string_view UPDATE_QUERY = "UPDATE `item_stones` SET `item_id`=?, `slot`=?, `polishNumber`=?, `polishCharge`=?, `proc_count`=? where `item_unique_id`=? AND `category`=?";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `item_stones` WHERE `item_unique_id`=? AND slot=? AND category=?";
constexpr std::string_view SELECT_QUERY = "SELECT `item_id`, `slot`, `category`, `polishNumber`, `polishCharge`, `proc_count` FROM `item_stones` WHERE `item_unique_id`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.ItemStoneListDAO");

void ItemStoneListDAO::load(const std::vector<runtime::Ptr<model::gameobjects::Item>>& items) {
	AION_UNPORTED();
}

void ItemStoneListDAO::save(const std::vector<runtime::Ptr<model::gameobjects::Item>>& items) {
	AION_UNPORTED();
}

void ItemStoneListDAO::storeManaStones(const std::unordered_set<runtime::Ptr<model::items::ManaStone>>& manaStones) {
	AION_UNPORTED();
}

void ItemStoneListDAO::storeGodStones(model::items::GodStone& godStones) {
	AION_UNPORTED();
}

void ItemStoneListDAO::storeFusionStone(const std::unordered_set<runtime::Ptr<model::items::ManaStone>>& manaStones) {
	AION_UNPORTED();
}

void ItemStoneListDAO::storeIdianStones(model::items::IdianStone& idianStone) {
	AION_UNPORTED();
}

void ItemStoneListDAO::store(const std::unordered_set<runtime::Ptr<model::items::ItemStone>>& stones, model::items::ItemStone_ItemStoneType ist) {
	AION_UNPORTED();
}

void ItemStoneListDAO::addItemStones(commons::database::Connection& con, const std::vector<runtime::Ptr<model::items::ItemStone>>& itemStones,
	model::items::ItemStone_ItemStoneType ist) {
	AION_UNPORTED();
}

void ItemStoneListDAO::updateItemStones(commons::database::Connection& con, const std::vector<runtime::Ptr<model::items::ItemStone>>& itemStones,
	model::items::ItemStone_ItemStoneType ist) {
	AION_UNPORTED();
}

void ItemStoneListDAO::deleteItemStones(commons::database::Connection& con, const std::vector<runtime::Ptr<model::items::ItemStone>>& itemStones,
	model::items::ItemStone_ItemStoneType ist) {
	AION_UNPORTED();
}

void ItemStoneListDAO::deleteItemStone(commons::database::Connection& con, int32_t uid, int32_t slot, int32_t category) {
	AION_UNPORTED();
}

void ItemStoneListDAO::save(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
