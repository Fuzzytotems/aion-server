#include "aion/gameserver/dao/PlayerBindPointDAO.h"

#include <string>
#include <string_view>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/player/BindPointPosition.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using model::gameobjects::Persistable;
using model::gameobjects::player::BindPointPosition;

namespace {

constexpr std::string_view INSERT_QUERY = "REPLACE INTO `player_bind_point` (`player_id`, `map_id`, `x`, `y`, `z`, `heading`) VALUES (?,?,?,?,?,?)";
constexpr std::string_view SELECT_QUERY = "SELECT `map_id`, `x`, `y`, `z`, `heading` FROM `player_bind_point` WHERE `player_id`=?";
constexpr std::string_view UPDATE_QUERY = "UPDATE player_bind_point set `map_id`=?, `x`=?, `y`=? , `z`=?, `heading`=? WHERE `player_id`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerBindPointDAO");

void PlayerBindPointDAO::loadBindPoint(model::gameobjects::player::Player& player) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_QUERY);
		stmt->setInt(1, player.getObjectId());
		auto rset = stmt->executeQuery();
		if (rset->next()) {
			int32_t mapId = rset->getInt("map_id");
			float x = rset->getFloat("x");
			float y = rset->getFloat("y");
			float z = rset->getFloat("z");
			int8_t heading = rset->getByte("heading");
			runtime::Ref<BindPointPosition> bind = BindPointPosition::create(mapId, x, y, z, heading);
			bind->setPersistentState(Persistable::PersistentState::UPDATED);
			player.setBindPoint(bind);
		}
	} catch (const std::exception& e) {
		log.error("Could not restore BindPointPosition data for playerObjId: " + std::to_string(player.getObjectId()) + " from DB: " + e.what(), e);
	}
}

bool PlayerBindPointDAO::insertBindPoint(model::gameobjects::player::Player& player) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(INSERT_QUERY);
		runtime::Ptr<BindPointPosition> bpp = player.getBindPoint();
		stmt->setInt(1, player.getObjectId());
		stmt->setInt(2, bpp->getMapId());
		stmt->setFloat(3, bpp->getX());
		stmt->setFloat(4, bpp->getY());
		stmt->setFloat(5, bpp->getZ());
		stmt->setByte(6, bpp->getHeading());
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Could not store BindPointPosition data for player " + std::to_string(player.getObjectId()) + " from DB: " + e.what(), e);
		return false;
	}
	return true;
}

bool PlayerBindPointDAO::updateBindPoint(model::gameobjects::player::Player& player) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(UPDATE_QUERY);
		runtime::Ptr<BindPointPosition> bpp = player.getBindPoint();
		stmt->setInt(1, bpp->getMapId());
		stmt->setFloat(2, bpp->getX());
		stmt->setFloat(3, bpp->getY());
		stmt->setFloat(4, bpp->getZ());
		stmt->setByte(5, bpp->getHeading());
		// Java binds the player id with setFloat, kept: a float holds every integer only up to 2^24, so the update misses the row of a player
		// whose object id above 2^24 has no exact float value (the id allocator hands out ids up to 2^27)
		stmt->setFloat(6, static_cast<float>(player.getObjectId()));
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Could not update BindPointPosition data for player " + std::to_string(player.getObjectId()) + " from DB: " + e.what(), e);
		return false;
	}
	return true;
}

bool PlayerBindPointDAO::store(model::gameobjects::player::Player& player) {
	bool insert = false;
	runtime::Ptr<BindPointPosition> bind = player.getBindPoint();
	switch (bind->getPersistentState()) {
		case Persistable::PersistentState::NEW:
			insert = insertBindPoint(player);
			break;
		case Persistable::PersistentState::UPDATE_REQUIRED:
			insert = updateBindPoint(player);
			break;
		default:
			break;
	}
	bind->setPersistentState(Persistable::PersistentState::UPDATED);
	return insert;
}

} // namespace aion::gameserver::dao
