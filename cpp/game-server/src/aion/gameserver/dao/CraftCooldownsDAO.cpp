#include "aion/gameserver/dao/CraftCooldownsDAO.h"

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

constexpr std::string_view INSERT_QUERY = "INSERT INTO `craft_cooldowns` (`player_id`, `delay_id`, `reuse_time`) VALUES (?,?,?)";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `craft_cooldowns` WHERE `player_id`=?";
constexpr std::string_view SELECT_QUERY = "SELECT `delay_id`, `reuse_time` FROM `craft_cooldowns` WHERE `player_id`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.CraftCooldownsDAO");

void CraftCooldownsDAO::loadCraftCooldowns(model::gameobjects::player::Player& player) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_QUERY);
		stmt->setInt(1, player.getObjectId());
		auto rset = stmt->executeQuery();
		while (rset->next()) {
			int32_t delayId = rset->getInt("delay_id");
			int64_t reuseTime = rset->getLong("reuse_time");
			player.getCraftCooldowns()->put(delayId, reuseTime);
		}
	} catch (const SQLException& e) {
		log.error("Couldn't load craft cooldowns for " + player.toString(), e);
	}
}

void CraftCooldownsDAO::storeCraftCooldowns(model::gameobjects::player::Player& player) {
	deleteCraftCoolDowns(player);
	for (const auto& entry : player.getCraftCooldowns()->snapshot()) {
		int32_t delayId = entry.getKey();
		int64_t reuseTime = entry.getValue();
		if (reuseTime < commons::utils::currentTimeMillis())
			continue;
		try {
			auto con = DatabaseFactory::getConnection();
			auto stmt = con->prepareStatement(INSERT_QUERY);
			stmt->setInt(1, player.getObjectId());
			stmt->setInt(2, delayId);
			stmt->setLong(3, reuseTime);
			stmt->execute();
		} catch (const SQLException& e) {
			log.error("Couldn't store craft cooldowns for " + player.toString(), e);
		}
	}
}

void CraftCooldownsDAO::deleteCraftCoolDowns(model::gameobjects::player::Player& player) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(DELETE_QUERY);
		stmt->setInt(1, player.getObjectId());
		stmt->execute();
	} catch (const SQLException& e) {
		log.error("Couldn't delete craft cooldowns for " + player.toString(), e);
	}
}

} // namespace aion::gameserver::dao
