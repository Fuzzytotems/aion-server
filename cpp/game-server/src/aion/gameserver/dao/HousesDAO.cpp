#include "aion/gameserver/dao/HousesDAO.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dao/detail/UsedIds.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/templates/housing/Building.h"
#include "aion/gameserver/model/templates/housing/BuildingType.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/model/templates/housing/HousingLand.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::SQLException;
using model::gameobjects::Persistable;
using model::house::House;
using model::templates::housing::Building;
using model::templates::housing::HouseAddress;

namespace {

constexpr std::string_view SELECT_HOUSES_QUERY = "SELECT * FROM houses WHERE address <> 2001 AND address <> 3001";
constexpr std::string_view SELECT_STUDIOS_QUERY = "SELECT * FROM houses WHERE address = 2001 OR address = 3001";
constexpr std::string_view ADD_HOUSE_QUERY = "INSERT INTO houses (id, address, building_id, player_id, acquire_time, settings, next_pay, sign_notice)  VALUES (?,?,?,?,?,?,?,?)";
constexpr std::string_view UPDATE_HOUSE_QUERY = "UPDATE houses SET building_id=?, player_id=?, acquire_time=?, settings=?, next_pay=?, sign_notice=? WHERE id=?";
constexpr std::string_view DELETE_HOUSE_QUERY = "DELETE FROM houses WHERE player_id=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.HousesDAO");

std::vector<int32_t> HousesDAO::getUsedIDs() {
	return detail::getUsedIDs(log, "SELECT DISTINCT id FROM houses", "id", "Can't get list of IDs from houses table");
}

void HousesDAO::storeHouse(model::house::House& house) {
	if (house.getPersistentState() == Persistable::PersistentState::NEW)
		insertNewHouse(house);
	else if (house.getPersistentState() == Persistable::PersistentState::UPDATE_REQUIRED)
		updateHouse(house);
}

void HousesDAO::insertNewHouse(model::house::House& house) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(ADD_HOUSE_QUERY);
		stmt->setInt(1, house.getObjectId());
		stmt->setInt(2, house.getAddress()->getId());
		stmt->setInt(3, house.getBuilding()->getId());
		stmt->setInt(4, house.getOwnerId());
		stmt->setTimestamp(5, house.getAcquiredTime());
		stmt->setInt(6, house.getPermissionsForDB());
		stmt->setTimestamp(7, house.getNextPay());
		stmt->setString(8, house.getSignNotice());
		stmt->execute();
		house.setPersistentState(Persistable::PersistentState::UPDATED);
	} catch (const std::exception& e) {
		log.error("Could not insert house " + std::to_string(house.getObjectId()), e);
	}
}

void HousesDAO::updateHouse(model::house::House& house) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(UPDATE_HOUSE_QUERY);
		stmt->setInt(1, house.getBuilding()->getId());
		stmt->setInt(2, house.getOwnerId());
		stmt->setTimestamp(3, house.getAcquiredTime());
		stmt->setInt(4, house.getPermissionsForDB());
		stmt->setTimestamp(5, house.getNextPay());
		stmt->setString(6, house.getSignNotice());
		stmt->setInt(7, house.getObjectId());
		stmt->execute();
		house.setPersistentState(Persistable::PersistentState::UPDATED);
	} catch (const std::exception& e) {
		log.error("Could not store house " + std::to_string(house.getObjectId()), e);
	}
}

std::unordered_map<int32_t, runtime::Ref<model::house::House>> HousesDAO::loadHouses(
	const std::vector<const model::templates::housing::HousingLand*>& lands, bool studios) {
	std::unordered_map<int32_t, runtime::Ref<House>> houses;
	std::unordered_map<int32_t, const HouseAddress*> addressesById;
	std::unordered_map<int32_t, const std::optional<std::vector<Building>>*> buildingsForAddress;
	for (const model::templates::housing::HousingLand* land : lands) {
		if (!land->getAddresses()) // Java: NullPointerException iterating a land without addresses
			throw runtime::NullPointerException("Cannot invoke \"java.util.List.iterator()\" because the land's addresses are null");
		for (const HouseAddress& address : *land->getAddresses()) {
			addressesById.insert_or_assign(address.getId(), &address);
			buildingsForAddress.insert_or_assign(address.getId(), &land->getBuildings());
		}
	}

	std::unordered_map<int32_t, int32_t> addressHouseIds;
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(studios ? SELECT_STUDIOS_QUERY : SELECT_HOUSES_QUERY);
		auto rset = stmt->executeQuery();
		while (rset->next()) {
			int32_t houseId = rset->getInt("id");
			int32_t buildingId = rset->getInt("building_id");
			auto addressIt = addressesById.find(rset->getInt("address"));
			if (addressIt == addressesById.end()) // Java: address.getId() on the null of addressesById.get
				throw runtime::NullPointerException("Cannot invoke \"HouseAddress.getId()\" because \"address\" is null");
			const HouseAddress* address = addressIt->second;
			const Building* building = nullptr;
			const std::optional<std::vector<Building>>* buildings = buildingsForAddress.at(address->getId());
			if (!*buildings)
				throw runtime::NullPointerException("Cannot invoke \"java.util.List.iterator()\" because the land's buildings are null");
			for (const Building& b : **buildings) {
				if (b.getId() == buildingId) {
					building = &b;
					break;
				}
			}
			runtime::Ref<House> house;
			if (!building) {
				log.warn("Missing building type for address " + std::to_string(address->getId()));
				continue;
			} else if (addressHouseIds.contains(address->getId())) {
				log.warn("Duplicate house address " + std::to_string(address->getId()) + "!");
				continue;
			} else {
				house = model::gameobjects::VisibleObject::create<House>(houseId, building, address, 0);
				if (building->getType() == model::templates::housing::BuildingType::PERSONAL_FIELD)
					addressHouseIds.insert_or_assign(address->getId(), houseId);
			}
			house->setOwnerId(rset->getInt("player_id"));
			house->setAcquiredTime(rset->getTimestamp("acquire_time"));
			house->setPermissionsFromDB(rset->getInt("settings"));
			house->setNextPay(rset->getTimestamp("next_pay"));
			house->setSignNotice(rset->getString("sign_notice"));
			house->setPersistentState(Persistable::PersistentState::UPDATED);
			int32_t id = studios ? house->getOwnerId() : address->getId();
			houses.insert_or_assign(id, std::move(house));
		}
	} catch (const std::exception& e) {
		log.error("Could not load houses", e);
	}
	return houses;
}

void HousesDAO::deleteHouse(int32_t playerId) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(DELETE_HOUSE_QUERY);
		stmt->setInt(1, playerId);
		stmt->execute();
	} catch (const SQLException& e) {
		log.error("Delete House failed", e);
	}
}

} // namespace aion::gameserver::dao
