#include "aion/gameserver/dao/HouseBidsDAO.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view LOAD_QUERY = "SELECT b.*, IF(b.player_id = p.id, 1, 0) playerExists FROM `house_bids` b LEFT JOIN `players` p ON p.id = b.player_id ORDER BY `bid`, `bid_time`";
constexpr std::string_view INSERT_QUERY = "INSERT INTO `house_bids` (`player_id`, `house_id`, `bid`, `bid_time`) VALUES (?, ?, ?, ?)";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `house_bids` WHERE `house_id` = ?";
constexpr std::string_view DELETE_SINGLE_BID_QUERY = "DELETE FROM `house_bids` WHERE `player_id` = ? AND `house_id` = ? AND `bid` = ?";
constexpr std::string_view DISABLE_QUERY = "UPDATE `house_bids` SET `player_id` = 0 WHERE `player_id` = ?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.HouseBidsDAO");

std::unordered_set<int32_t> HouseBidsDAO::loadBids(const std::unordered_map<int32_t, runtime::Ptr<model::house::HouseBids>>& bidsById) {
	AION_UNPORTED();
}

bool HouseBidsDAO::addBid(model::house::HouseBids::Bid& bid) {
	AION_UNPORTED();
}

bool HouseBidsDAO::deleteOrDisableBids(int32_t playerObjectId, const std::vector<runtime::Ptr<model::house::HouseBids::Bid>>& bidsToDelete) {
	AION_UNPORTED();
}

bool HouseBidsDAO::deleteHouseBids(int32_t houseId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
