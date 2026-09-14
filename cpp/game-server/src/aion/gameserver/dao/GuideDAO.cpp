#include "aion/gameserver/dao/GuideDAO.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/guide/Guide.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view DELETE_QUERY = "DELETE FROM `guides` WHERE `guide_id`=?";
constexpr std::string_view SELECT_QUERY = "SELECT * FROM `guides` WHERE `player_id`=?";
constexpr std::string_view SELECT_GUIDE_QUERY = "SELECT * FROM `guides` WHERE `guide_id`=? AND `player_id`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.GuideDAO");

bool GuideDAO::deleteGuide(int32_t guide_id) {
	AION_UNPORTED();
}

std::vector<model::guide::Guide> GuideDAO::loadGuides(int32_t playerId) {
	AION_UNPORTED();
}

std::optional<model::guide::Guide> GuideDAO::loadGuide(int32_t player_id, int32_t guide_id) {
	AION_UNPORTED();
}

void GuideDAO::saveGuide(int32_t guide_id, model::gameobjects::player::Player& player, std::string_view title) {
	AION_UNPORTED();
}

std::vector<int32_t> GuideDAO::getUsedIDs() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
