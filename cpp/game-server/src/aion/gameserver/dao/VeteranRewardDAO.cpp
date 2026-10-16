#include "aion/gameserver/dao/VeteranRewardDAO.h"

#include <string>
#include <string_view>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::SQLException;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.VeteranRewardDAO");

namespace {
constexpr std::string_view SELECT_QUERY = "SELECT `received_months` FROM `player_veteran_rewards` WHERE `player_id`=?";
constexpr std::string_view UPDATE_QUERY = "REPLACE INTO `player_veteran_rewards` (`player_id`, `received_months`) VALUES (?,?)";
} // namespace

int32_t VeteranRewardDAO::loadReceivedMonths(model::gameobjects::player::Player& player) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_QUERY);
		stmt->setInt(1, player.getObjectId());
		auto rset = stmt->executeQuery();
		if (rset->next())
			return rset->getInt("received_months");
		return 0;
	} catch (const SQLException& e) {
		log.error("Error loading received veteran reward months for player " + player.toString(), e);
		return -1;
	}
}

bool VeteranRewardDAO::storeReceivedMonths(model::gameobjects::player::Player& player, int32_t months) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(UPDATE_QUERY);
		stmt->setInt(1, player.getObjectId());
		stmt->setInt(2, months);
		stmt->execute();
		return true;
	} catch (const std::exception& e) {
		log.error("Error saving received veteran reward months (" + std::to_string(months) + ") for player " + player.toString(), e);
		return false;
	}
}

} // namespace aion::gameserver::dao
