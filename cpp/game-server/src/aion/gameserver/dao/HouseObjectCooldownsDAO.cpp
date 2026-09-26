#include "aion/gameserver/dao/HouseObjectCooldownsDAO.h"

#include <string_view>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/gameobjects/player/Cooldowns.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::SQLException;

namespace {

constexpr std::string_view INSERT_QUERY = "INSERT INTO `house_object_cooldowns` (`player_id`, `object_id`, `reuse_time`) VALUES (?,?,?)";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `house_object_cooldowns` WHERE `player_id`=?";
constexpr std::string_view SELECT_QUERY = "SELECT `object_id`, `reuse_time` FROM `house_object_cooldowns` WHERE `player_id`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.HouseObjectCooldownsDAO");

void HouseObjectCooldownsDAO::loadHouseObjectCooldowns(model::gameobjects::player::Player& player) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_QUERY);
		stmt->setInt(1, player.getObjectId());
		auto rset = stmt->executeQuery();
		while (rset->next()) {
			int64_t reuseTime = rset->getLong("reuse_time");
			if (reuseTime > commons::utils::currentTimeMillis())
				player.getHouseObjectCooldowns()->put(rset->getInt("object_id"), reuseTime);
		}
	} catch (const SQLException& e) {
		log.error("LoadHouseObjectCooldowns", e);
	}
}

void HouseObjectCooldownsDAO::storeHouseObjectCooldowns(model::gameobjects::player::Player& player) {
	deleteHouseObjectCoolDowns(player);
	for (const auto& entry : player.getHouseObjectCooldowns()->snapshot()) {
		int32_t templateId = entry.getKey();
		int64_t reuseTime = entry.getValue();
		if (reuseTime < commons::utils::currentTimeMillis())
			continue;
		try {
			auto con = DatabaseFactory::getConnection();
			auto stmt = con->prepareStatement(INSERT_QUERY);
			stmt->setInt(1, player.getObjectId());
			stmt->setInt(2, templateId);
			stmt->setLong(3, reuseTime);
			stmt->execute();
		} catch (const SQLException& e) {
			log.error("storeHouseObjectCoolDowns", e);
		}
	}
}

void HouseObjectCooldownsDAO::deleteHouseObjectCoolDowns(model::gameobjects::player::Player& player) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(DELETE_QUERY);
		stmt->setInt(1, player.getObjectId());
		stmt->execute();
	} catch (const SQLException& e) {
		log.error("deleteHouseObjectCoolDowns", e);
	}
}

} // namespace aion::gameserver::dao
