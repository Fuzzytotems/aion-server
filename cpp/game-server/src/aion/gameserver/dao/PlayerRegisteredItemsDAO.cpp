#include "aion/gameserver/dao/PlayerRegisteredItemsDAO.h"

#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/database/SqlTypes.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/dao/detail/DaoSupport.h"
#include "aion/gameserver/dao/detail/UsedIds.h"
#include "aion/gameserver/model/gameobjects/HouseDecoration.h"
#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/house/HouseRegistry.h"
#include "aion/gameserver/model/templates/housing/HousePart.h"
#include "aion/gameserver/model/templates/housing/HouseType.h"
#include "aion/gameserver/model/templates/housing/PlaceArea.h"
#include "aion/gameserver/model/templates/housing/PlaceableHouseObject.h"
#include "aion/gameserver/services/item/HouseObjectFactory.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::SQLException;
namespace Types = commons::database::Types;
using model::gameobjects::HouseDecoration;
using model::gameobjects::HouseObject;
using model::gameobjects::Persistable;

namespace {

constexpr std::string_view CLEAN_PLAYER_QUERY = "DELETE FROM `player_registered_items` WHERE `player_id` = ?";
constexpr std::string_view SELECT_QUERY = "SELECT * FROM `player_registered_items` WHERE `player_id`=?";
constexpr std::string_view INSERT_QUERY = "INSERT INTO `player_registered_items` (`expire_time`,`color`,`color_expires`,`owner_use_count`,`visitor_use_count`,`x`,`y`,`z`,`h`,`area`,`room`,`player_id`,`item_unique_id`,`item_id`) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?)";
constexpr std::string_view UPDATE_QUERY = "UPDATE `player_registered_items` SET `expire_time`=?,`color`=?,`color_expires`=?,`owner_use_count`=?,`visitor_use_count`=?,`x`=?,`y`=?,`z`=?,`h`=?,`area`=?,`room`=? WHERE `player_id`=? AND `item_unique_id`=? AND `item_id`=?";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `player_registered_items` WHERE `item_unique_id` = ?";
constexpr std::string_view RESET_QUERY = "UPDATE `player_registered_items` SET x=0,y=0,z=0,h=0,area='NONE' WHERE `player_id`=? AND `area` != 'DECOR'";

template <class T>
std::vector<runtime::Ptr<T>> filterByState(const std::vector<runtime::Ptr<T>>& objects, Persistable::PersistentState state) {
	std::vector<runtime::Ptr<T>> filtered;
	for (const runtime::Ptr<T>& object : objects) {
		if (object && object->getPersistentState() == state)
			filtered.push_back(object);
	}
	return filtered;
}

template <class T>
std::vector<int32_t> objectIds(const std::vector<runtime::Ptr<T>>& objects) {
	std::vector<int32_t> ids;
	ids.reserve(objects.size());
	for (const runtime::Ptr<T>& object : objects)
		ids.push_back(object->getObjectId());
	return ids;
}

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerRegisteredItemsDAO");

std::vector<int32_t> PlayerRegisteredItemsDAO::getUsedIDs() {
	return detail::getUsedIDs(log, "SELECT item_unique_id FROM player_registered_items WHERE item_unique_id <> 0", "item_unique_id",
		"Can't get list of IDs from player_registered_items table");
}

void PlayerRegisteredItemsDAO::loadRegistry(model::house::HouseRegistry& registry) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_QUERY);
		stmt->setInt(1, registry.getOwner()->getOwnerId());
		auto rset = stmt->executeQuery();
		while (rset->next()) {
			std::string area = rset->getString("area");
			if (area == "DECOR") {
				registry.putDecor(*createDecoration(registry, *rset), false);
			} else {
				registry.putObject(*constructObject(registry, *rset), false);
			}
		}
		bool hasInvalidDecors = false;
		for (const runtime::Ptr<HouseDecoration>& decor : registry.getDecors()) {
			if (decor->getPersistentState() == Persistable::PersistentState::DELETED) {
				hasInvalidDecors = true;
				break;
			}
		}
		registry.setPersistentState(hasInvalidDecors ? Persistable::PersistentState::UPDATE_REQUIRED : Persistable::PersistentState::UPDATED);
	} catch (const std::exception& e) {
		log.error("Could not load house registry data for player " + std::to_string(registry.getOwner()->getOwnerId()), e);
	}
}

runtime::Ref<model::gameobjects::HouseObject> PlayerRegisteredItemsDAO::constructObject(model::house::HouseRegistry& registry,
	commons::database::ResultSet& rset) {
	int32_t itemUniqueId = rset.getInt("item_unique_id");
	runtime::Ptr<model::gameobjects::VisibleObject> visObj = world::World::getInstance().findVisibleObject(itemUniqueId);
	runtime::Ref<HouseObject> obj;
	if (visObj) {
		runtime::Ptr<HouseObject> houseObject = runtime::as<HouseObject>(visObj);
		if (houseObject)
			obj = houseObject;
		else {
			// Java: IllegalAccessException
			throw commons::utils::Exception("Someone stole my house object id : " + std::to_string(itemUniqueId));
		}
	} else {
		obj = registry.getObjectByObjId(itemUniqueId);
		if (!obj)
			obj = services::item::HouseObjectFactory::createNew(registry, itemUniqueId, rset.getInt("item_id"));
	}
	obj->setOwnerUsedCount(rset.getInt("owner_use_count"));
	obj->setVisitorUsedCount(rset.getInt("visitor_use_count"));
	obj->setX(rset.getFloat("x"));
	obj->setY(rset.getFloat("y"));
	obj->setZ(rset.getFloat("z"));
	obj->setHeading(static_cast<int8_t>(rset.getInt("h")));
	obj->setColor(rset.getObject<int32_t>("color"));
	obj->setColorExpireEnd(rset.getInt("color_expires"));
	if (obj->getObjectTemplate()->getUseDays() > 0)
		obj->setExpireTime(rset.getInt("expire_time"));
	obj->setPersistentState(Persistable::PersistentState::UPDATED);
	return obj;
}

runtime::Ref<model::gameobjects::HouseDecoration> PlayerRegisteredItemsDAO::createDecoration(model::house::HouseRegistry& registry,
	commons::database::ResultSet& rset) {
	int32_t itemUniqueId = rset.getInt("item_unique_id");
	int32_t itemId = rset.getInt("item_id");
	int8_t room = rset.getByte("room");
	runtime::Ref<HouseDecoration> decor = HouseDecoration::create(itemUniqueId, itemId, room);
	if (!decor->getTemplate()->isForBuilding(*registry.getOwner()->getBuilding()) ||
		(decor->getRoom() > 0 && registry.getOwner()->getHouseType() != model::templates::housing::HouseType::PALACE))
		decor->setPersistentState(Persistable::PersistentState::DELETED);
	else
		decor->setPersistentState(Persistable::PersistentState::UPDATED);
	return decor;
}

bool PlayerRegisteredItemsDAO::store(model::house::HouseRegistry& registry, int32_t playerId) {
	std::vector<runtime::Ptr<HouseObject>> objects = registry.getObjects();
	std::vector<runtime::Ptr<HouseDecoration>> decors = registry.getDecors();
	std::vector<runtime::Ptr<HouseObject>> objectsToAdd = filterByState(objects, Persistable::PersistentState::NEW);
	std::vector<runtime::Ptr<HouseObject>> objectsToUpdate = filterByState(objects, Persistable::PersistentState::UPDATE_REQUIRED);
	std::vector<runtime::Ptr<HouseObject>> objectsToDelete = filterByState(objects, Persistable::PersistentState::DELETED);
	std::vector<runtime::Ptr<HouseDecoration>> decorsToAdd = filterByState(decors, Persistable::PersistentState::NEW);
	std::vector<runtime::Ptr<HouseDecoration>> decorsToUpdate = filterByState(decors, Persistable::PersistentState::UPDATE_REQUIRED);
	std::vector<runtime::Ptr<HouseDecoration>> decorsToDelete = filterByState(decors, Persistable::PersistentState::DELETED);

	bool objectDeleteResult = false;
	bool decorsDeleteResult = false;
	try {
		auto con = DatabaseFactory::getConnection();
		con->setAutoCommit(false);
		objectDeleteResult = deleteObjects(*con, objectsToDelete);
		decorsDeleteResult = deleteDecors(*con, decorsToDelete);
		storeObjects(*con, objectsToUpdate, playerId, false);
		storeDecors(*con, decorsToUpdate, playerId, false);
		storeObjects(*con, objectsToAdd, playerId, true);
		storeDecors(*con, decorsToAdd, playerId, true);
		registry.setPersistentState(Persistable::PersistentState::UPDATED);
	} catch (const SQLException& e) {
		log.error("Can't save registered items for player: " + std::to_string(playerId), e);
	}

	for (const runtime::Ptr<HouseObject>& obj : objects) {
		if (obj->getPersistentState() == Persistable::PersistentState::DELETED)
			registry.discardObject(*obj, true);
		else
			obj->setPersistentState(Persistable::PersistentState::UPDATED);
	}
	for (const runtime::Ptr<HouseDecoration>& decor : decors) {
		if (decor->getPersistentState() == Persistable::PersistentState::DELETED)
			registry.discardDecor(*decor, true);
		else
			decor->setPersistentState(Persistable::PersistentState::UPDATED);
	}

	if (objectDeleteResult)
		utils::idfactory::IDFactory::getInstance().releaseObjectIds(objectIds(objectsToDelete), "HouseObject");
	if (decorsDeleteResult)
		utils::idfactory::IDFactory::getInstance().releaseObjectIds(objectIds(decorsToDelete), "HouseDecoration");
	return true;
}

bool PlayerRegisteredItemsDAO::storeObjects(commons::database::Connection& con, const std::vector<runtime::Ptr<model::gameobjects::HouseObject>>& objects,
	int32_t playerId, bool isNew) {
	if (objects.empty())
		return true;

	try {
		auto stmt = con.prepareStatement(isNew ? INSERT_QUERY : UPDATE_QUERY);
		for (const runtime::Ptr<HouseObject>& obj : objects) {
			stmt->setObject(1, obj->getExpireTime() > 0 ? std::optional<int32_t>(obj->getExpireTime()) : std::nullopt, Types::INTEGER);
			stmt->setObject(2, obj->getColor(), Types::INTEGER);
			stmt->setInt(3, obj->getColorExpireEnd());
			stmt->setInt(4, obj->getOwnerUsedCount());
			stmt->setInt(5, obj->getVisitorUsedCount());
			stmt->setFloat(6, obj->getX());
			stmt->setFloat(7, obj->getY());
			stmt->setFloat(8, obj->getZ());
			stmt->setInt(9, obj->getHeading());
			if (obj->getX() > 0 || obj->getY() > 0 || obj->getZ() > 0)
				stmt->setString(10, detail::enumName(obj->getPlaceArea()));
			else
				stmt->setString(10, "NONE");
			stmt->setByte(11, static_cast<int8_t>(0));
			stmt->setInt(12, playerId);
			stmt->setInt(13, obj->getObjectId());
			stmt->setInt(14, obj->getObjectTemplate()->getTemplateId());
			stmt->addBatch();
		}

		stmt->executeBatch();
		con.commit();
	} catch (const std::exception& e) {
		log.error("Failed to execute house object update batch", e);
		return false;
	}
	return true;
}

bool PlayerRegisteredItemsDAO::storeDecors(commons::database::Connection& con,
	const std::vector<runtime::Ptr<model::gameobjects::HouseDecoration>>& decors, int32_t playerId, bool isNew) {
	if (decors.empty())
		return true;

	try {
		auto stmt = con.prepareStatement(isNew ? INSERT_QUERY : UPDATE_QUERY);
		for (const runtime::Ptr<HouseDecoration>& decor : decors) {
			stmt->setNull(1, Types::INTEGER);
			stmt->setNull(2, Types::INTEGER);
			stmt->setInt(3, 0);
			stmt->setInt(4, 0);
			stmt->setInt(5, 0);
			stmt->setFloat(6, 0);
			stmt->setFloat(7, 0);
			stmt->setFloat(8, 0);
			stmt->setInt(9, 0);
			stmt->setString(10, "DECOR");
			stmt->setByte(11, decor->getRoom());
			stmt->setInt(12, playerId);
			stmt->setInt(13, decor->getObjectId());
			stmt->setInt(14, decor->getTemplateId());
			stmt->addBatch();
		}
		stmt->executeBatch();
		con.commit();
	} catch (const std::exception& e) {
		log.error("Failed to execute house decor update batch", e);
		return false;
	}
	return true;
}

bool PlayerRegisteredItemsDAO::deleteObjects(commons::database::Connection& con, const std::vector<runtime::Ptr<model::gameobjects::HouseObject>>& objects) {
	if (objects.empty())
		return true;

	try {
		auto stmt = con.prepareStatement(DELETE_QUERY);
		for (const runtime::Ptr<HouseObject>& obj : objects) {
			stmt->setInt(1, obj->getObjectId());
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

bool PlayerRegisteredItemsDAO::deleteDecors(commons::database::Connection& con,
	const std::vector<runtime::Ptr<model::gameobjects::HouseDecoration>>& decors) {
	if (decors.empty())
		return true;

	try {
		auto stmt = con.prepareStatement(DELETE_QUERY);
		for (const runtime::Ptr<HouseDecoration>& decor : decors) {
			stmt->setInt(1, decor->getObjectId());
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

bool PlayerRegisteredItemsDAO::deletePlayerItems(int32_t playerId) {
	log.info("Deleting player items");
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(CLEAN_PLAYER_QUERY);
		stmt->setInt(1, playerId);
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Error in deleting all player registered items. PlayerObjId: " + std::to_string(playerId), e);
		return false;
	}
	return true;
}

void PlayerRegisteredItemsDAO::resetRegistry(int32_t playerId) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(RESET_QUERY);
		stmt->setInt(1, playerId);
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Error in resetting player registered items. PlayerObjId: " + std::to_string(playerId), e);
	}
}

} // namespace aion::gameserver::dao
