#include "aion/gameserver/dao/PlayerTitleListDAO.h"

#include <memory>
#include <string>
#include <string_view>

#include "aion/commons/database/DB.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/title/Title.h"
#include "aion/gameserver/model/gameobjects/player/title/TitleList.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::DB;
using commons::database::PreparedStatement;
using commons::database::ResultSet;

namespace {

constexpr std::string_view LOAD_QUERY = "SELECT `title_id`, `remaining` FROM `player_titles` WHERE `player_id`=?";
constexpr std::string_view INSERT_QUERY = "INSERT INTO `player_titles`(`player_id`,`title_id`, `remaining`) VALUES (?,?,?)";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `player_titles` WHERE `player_id`=? AND `title_id` =?;";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerTitleListDAO");

namespace {

/**
 * The Java body of loadTitleList. TitleList is a late-bound OwnedPart (Player::setTitleList takes a std::unique_ptr), so the result is a
 * std::unique_ptr: a Ref to an unbound part cannot be created (OwnedPart::retain retains the owner; header request dao-1).
 */
std::unique_ptr<model::gameobjects::player::title::TitleList> loadTitleListPart(int32_t playerId) {
	auto tl = std::make_unique<model::gameobjects::player::title::TitleList>();
	DB::select(
		LOAD_QUERY, [&](PreparedStatement& stmt) { stmt.setInt(1, playerId); },
		[&](ResultSet& rset) {
			while (rset.next()) {
				int32_t id = rset.getInt("title_id");
				int32_t remaining = rset.getInt("remaining");
				tl->addEntry(id, remaining);
			}
		});
	return tl;
}

} // namespace

std::unique_ptr<model::gameobjects::player::title::TitleList> PlayerTitleListDAO::loadTitleList(int32_t playerId) {
	return loadTitleListPart(playerId);
}

bool PlayerTitleListDAO::storeTitles(model::gameobjects::player::Player& player, model::gameobjects::player::title::Title& entry) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(INSERT_QUERY);
		stmt->setInt(1, player.getObjectId());
		stmt->setInt(2, entry.getId());
		stmt->setInt(3, entry.getExpireTime());
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Could not store emotionId for player " + std::to_string(player.getObjectId()) + " from DB: " + e.what(), e);
		return false;
	}
	return true;
}

bool PlayerTitleListDAO::removeTitle(int32_t playerId, int32_t titleId) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(DELETE_QUERY);
		stmt->setInt(1, playerId);
		stmt->setInt(2, titleId);
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Could not delete title for player " + std::to_string(playerId) + " from DB: " + e.what(), e);
		return false;
	}
	return true;
}

} // namespace aion::gameserver::dao
