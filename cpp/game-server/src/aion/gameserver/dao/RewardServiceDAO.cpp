#include "aion/gameserver/dao/RewardServiceDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view UPDATE_QUERY = "UPDATE `player_web_rewards` SET `received`=? WHERE `entry_id`=?";
constexpr std::string_view SELECT_QUERY = "SELECT entry_id, item_id, item_count FROM `player_web_rewards` WHERE `player_id`=? AND `received` IS NULL";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.RewardServiceDAO");

std::vector<runtime::Ref<model::templates::rewards::RewardEntryItem>> RewardServiceDAO::loadUnreceived(int32_t playerId) {
	AION_UNPORTED();
}

void RewardServiceDAO::storeReceived(const std::vector<int32_t>& ids, int64_t timeReceived) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
