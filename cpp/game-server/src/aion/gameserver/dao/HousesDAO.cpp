#include "aion/gameserver/dao/HousesDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view SELECT_HOUSES_QUERY = "SELECT * FROM houses WHERE address <> 2001 AND address <> 3001";
constexpr std::string_view SELECT_STUDIOS_QUERY = "SELECT * FROM houses WHERE address = 2001 OR address = 3001";
constexpr std::string_view ADD_HOUSE_QUERY = "INSERT INTO houses (id, address, building_id, player_id, acquire_time, settings, next_pay, sign_notice)  VALUES (?,?,?,?,?,?,?,?)";
constexpr std::string_view UPDATE_HOUSE_QUERY = "UPDATE houses SET building_id=?, player_id=?, acquire_time=?, settings=?, next_pay=?, sign_notice=? WHERE id=?";
constexpr std::string_view DELETE_HOUSE_QUERY = "DELETE FROM houses WHERE player_id=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.HousesDAO");

std::vector<int32_t> HousesDAO::getUsedIDs() {
	AION_UNPORTED();
}

void HousesDAO::storeHouse(model::house::House& house) {
	AION_UNPORTED();
}

void HousesDAO::insertNewHouse(model::house::House& house) {
	AION_UNPORTED();
}

void HousesDAO::updateHouse(model::house::House& house) {
	AION_UNPORTED();
}

std::unordered_map<int32_t, runtime::Ref<model::house::House>> HousesDAO::loadHouses(
	const std::vector<const model::templates::housing::HousingLand*>& lands, bool studios) {
	AION_UNPORTED();
}

void HousesDAO::deleteHouse(int32_t playerId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
