#include "aion/gameserver/dao/GuideDAO.h"

#include <string>
#include <string_view>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dao/detail/UsedIds.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/guide/Guide.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;

namespace {

constexpr std::string_view DELETE_QUERY = "DELETE FROM `guides` WHERE `guide_id`=?";
constexpr std::string_view SELECT_QUERY = "SELECT * FROM `guides` WHERE `player_id`=?";
constexpr std::string_view SELECT_GUIDE_QUERY = "SELECT * FROM `guides` WHERE `guide_id`=? AND `player_id`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.GuideDAO");

bool GuideDAO::deleteGuide(int32_t guide_id) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(DELETE_QUERY);
		stmt->setInt(1, guide_id);
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Error delete guide_id: " + std::to_string(guide_id), e);
		return false;
	}
	return true;
}

std::vector<model::guide::Guide> GuideDAO::loadGuides(int32_t playerId) {
	std::vector<model::guide::Guide> guides;
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_QUERY);
		stmt->setInt(1, playerId);
		auto rset = stmt->executeQuery();
		while (rset->next()) {
			int32_t guide_id = rset->getInt("guide_id");
			int32_t player_id = rset->getInt("player_id");
			std::string title = rset->getString("title");
			guides.emplace_back(guide_id, player_id, title);
		}
	} catch (const std::exception& e) {
		log.error("Could not restore Guide data for player: " + std::to_string(playerId) + " from DB: " + e.what(), e);
	}
	return guides;
}

std::optional<model::guide::Guide> GuideDAO::loadGuide(int32_t player_id, int32_t guide_id) {
	std::optional<model::guide::Guide> guide;
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_GUIDE_QUERY);
		stmt->setInt(1, guide_id);
		stmt->setInt(2, player_id);
		auto rset = stmt->executeQuery();
		while (rset->next()) {
			std::string title = rset->getString("title");
			guide.emplace(guide_id, player_id, title);
		}
	} catch (const std::exception& e) {
		log.error("Could not restore Survey data for player: " + std::to_string(player_id) + " from DB: " + e.what(), e);
	}
	return guide;
}

void GuideDAO::saveGuide(int32_t guide_id, model::gameobjects::player::Player& player, std::string_view title) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement("INSERT INTO guides(guide_id, title, player_id)VALUES (?, ?, ?)");
		stmt->setInt(1, guide_id);
		stmt->setString(2, title);
		stmt->setInt(3, player.getObjectId());
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Error saving playerName: " + player.toString(), e);
	}
}

std::vector<int32_t> GuideDAO::getUsedIDs() {
	return detail::getUsedIDs(log, "SELECT guide_id FROM guides", "guide_id", "Can't get list of IDs from guides table");
}

} // namespace aion::gameserver::dao
