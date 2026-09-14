#include "aion/gameserver/dao/VeteranRewardDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view SELECT_QUERY = "SELECT `received_months` FROM `player_veteran_rewards` WHERE `player_id`=?";
constexpr std::string_view UPDATE_QUERY = "REPLACE INTO `player_veteran_rewards` (`player_id`, `received_months`) VALUES (?,?)";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.VeteranRewardDAO");

int32_t VeteranRewardDAO::loadReceivedMonths(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool VeteranRewardDAO::storeReceivedMonths(model::gameobjects::player::Player& player, int32_t months) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
