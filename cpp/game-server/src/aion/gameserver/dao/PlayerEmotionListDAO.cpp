#include "aion/gameserver/dao/PlayerEmotionListDAO.h"

#include <memory>
#include <string>
#include <string_view>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/emotion/Emotion.h"
#include "aion/gameserver/model/gameobjects/player/emotion/EmotionList.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;

namespace {

constexpr std::string_view INSERT_QUERY = "INSERT INTO `player_emotions` (`player_id`, `emotion`, `remaining`) VALUES (?,?,?)";
constexpr std::string_view SELECT_QUERY = "SELECT `emotion`, `remaining` FROM `player_emotions` WHERE `player_id`=?";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `player_emotions` WHERE `player_id`=? AND `emotion`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerEmotionListDAO");

void PlayerEmotionListDAO::loadEmotions(model::gameobjects::player::Player& player) {
	auto emotions = std::make_unique<model::gameobjects::player::emotion::EmotionList>(player);
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_QUERY);
		stmt->setInt(1, player.getObjectId());
		auto rset = stmt->executeQuery();
		while (rset->next()) {
			int32_t emotionId = rset->getInt("emotion");
			int32_t remaining = rset->getInt("remaining");
			emotions->add(emotionId, remaining, false);
		}
	} catch (const std::exception& e) {
		log.error("Could not restore emotionId for playerObjId: " + std::to_string(player.getObjectId()) + " from DB: " + e.what(), e);
	}
	player.setEmotions(std::move(emotions));
}

void PlayerEmotionListDAO::insertEmotion(model::gameobjects::player::Player& player, model::gameobjects::player::emotion::Emotion& emotion) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(INSERT_QUERY);
		stmt->setInt(1, player.getObjectId());
		stmt->setInt(2, emotion.getId());
		stmt->setInt(3, emotion.getExpireTime());
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Could not store emotionId for player " + std::to_string(player.getObjectId()) + " from DB: " + e.what(), e);
	}
}

void PlayerEmotionListDAO::deleteEmotion(int32_t playerId, int32_t emotionId) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(DELETE_QUERY);
		stmt->setInt(1, playerId);
		stmt->setInt(2, emotionId);
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Could not delete title for player " + std::to_string(playerId) + " from DB: " + e.what(), e);
	}
}

} // namespace aion::gameserver::dao
