#include "aion/gameserver/dao/MotionDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view INSERT_QUERY = "INSERT INTO `player_motions` (`player_id`, `motion_id`, `active`,  `time`) VALUES (?,?,?,?)";
constexpr std::string_view SELECT_QUERY = "SELECT `motion_id`, `active`, `time` FROM `player_motions` WHERE `player_id`=?";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `player_motions` WHERE `player_id`=? AND `motion_id`=?";
constexpr std::string_view UPDATE_QUERY = "UPDATE `player_motions` SET `active`=? WHERE `player_id`=? AND `motion_id`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerEmotionListDAO");

void MotionDAO::loadMotionList(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool MotionDAO::storeMotion(int32_t objectId, model::gameobjects::player::motion::Motion& motion) {
	AION_UNPORTED();
}

bool MotionDAO::deleteMotion(int32_t objectId, int32_t motionId) {
	AION_UNPORTED();
}

bool MotionDAO::updateMotion(int32_t objectId, model::gameobjects::player::motion::Motion& motion) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
