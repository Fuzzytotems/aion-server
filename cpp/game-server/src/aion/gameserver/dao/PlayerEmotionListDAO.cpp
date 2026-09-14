#include "aion/gameserver/dao/PlayerEmotionListDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view INSERT_QUERY = "INSERT INTO `player_emotions` (`player_id`, `emotion`, `remaining`) VALUES (?,?,?)";
constexpr std::string_view SELECT_QUERY = "SELECT `emotion`, `remaining` FROM `player_emotions` WHERE `player_id`=?";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `player_emotions` WHERE `player_id`=? AND `emotion`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerEmotionListDAO");

void PlayerEmotionListDAO::loadEmotions(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerEmotionListDAO::insertEmotion(model::gameobjects::player::Player& player, model::gameobjects::player::emotion::Emotion& emotion) {
	AION_UNPORTED();
}

void PlayerEmotionListDAO::deleteEmotion(int32_t playerId, int32_t emotionId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
