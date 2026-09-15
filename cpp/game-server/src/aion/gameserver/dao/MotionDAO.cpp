#include "aion/gameserver/dao/MotionDAO.h"

#include <memory>
#include <string>
#include <string_view>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/motion/Motion.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using model::gameobjects::player::motion::Motion;

namespace {

constexpr std::string_view INSERT_QUERY = "INSERT INTO `player_motions` (`player_id`, `motion_id`, `active`,  `time`) VALUES (?,?,?,?)";
constexpr std::string_view SELECT_QUERY = "SELECT `motion_id`, `active`, `time` FROM `player_motions` WHERE `player_id`=?";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `player_motions` WHERE `player_id`=? AND `motion_id`=?";
constexpr std::string_view UPDATE_QUERY = "UPDATE `player_motions` SET `active`=? WHERE `player_id`=? AND `motion_id`=?";

} // namespace

// Java: LoggerFactory.getLogger(PlayerEmotionListDAO.class)
static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerEmotionListDAO");

void MotionDAO::loadMotionList(model::gameobjects::player::Player& player) {
	auto motions = std::make_unique<model::gameobjects::player::motion::MotionList>(player);
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_QUERY);
		stmt->setInt(1, player.getObjectId());
		auto rset = stmt->executeQuery();
		while (rset->next()) {
			int32_t motionId = rset->getInt("motion_id");
			int32_t time = rset->getInt("time");
			bool isActive = rset->getBoolean("active");
			motions->add(*Motion::create(motionId, time, isActive), false);
		}
	} catch (const std::exception& e) {
		log.error("Could not restore motions for playerObjId: " + std::to_string(player.getObjectId()) + " from DB: " + e.what(), e);
	}
	player.setMotions(std::move(motions));
}

bool MotionDAO::storeMotion(int32_t objectId, model::gameobjects::player::motion::Motion& motion) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(INSERT_QUERY);
		stmt->setInt(1, objectId);
		stmt->setInt(2, motion.getId());
		stmt->setBoolean(3, motion.isActive());
		stmt->setInt(4, motion.getExpireTime());
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Could not store motion for player " + std::to_string(objectId) + " from DB: " + e.what(), e);
		return false;
	}
	return true;
}

bool MotionDAO::deleteMotion(int32_t objectId, int32_t motionId) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(DELETE_QUERY);
		stmt->setInt(1, objectId);
		stmt->setInt(2, motionId);
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Could not delete motion for player " + std::to_string(objectId) + " from DB: " + e.what(), e);
		return false;
	}
	return true;
}

bool MotionDAO::updateMotion(int32_t objectId, model::gameobjects::player::motion::Motion& motion) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(UPDATE_QUERY);
		stmt->setBoolean(1, motion.isActive());
		stmt->setInt(2, objectId);
		stmt->setInt(3, motion.getId());
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Could not store motion for player " + std::to_string(objectId) + " from DB: " + e.what(), e);
		return false;
	}
	return true;
}

} // namespace aion::gameserver::dao
