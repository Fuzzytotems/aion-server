#include "aion/gameserver/dao/HouseBidsDAO.h"

#include <string>
#include <string_view>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dao/detail/DaoSupport.h"
#include "aion/gameserver/model/house/HouseBids.h"
#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using model::house::HouseBids;

namespace {

constexpr std::string_view LOAD_QUERY = "SELECT b.*, IF(b.player_id = p.id, 1, 0) playerExists FROM `house_bids` b LEFT JOIN `players` p ON p.id = b.player_id ORDER BY `bid`, `bid_time`";
constexpr std::string_view INSERT_QUERY = "INSERT INTO `house_bids` (`player_id`, `house_id`, `bid`, `bid_time`) VALUES (?, ?, ?, ?)";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `house_bids` WHERE `house_id` = ?";
constexpr std::string_view DELETE_SINGLE_BID_QUERY = "DELETE FROM `house_bids` WHERE `player_id` = ? AND `house_id` = ? AND `bid` = ?";
constexpr std::string_view DISABLE_QUERY = "UPDATE `house_bids` SET `player_id` = 0 WHERE `player_id` = ?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.HouseBidsDAO");

std::unordered_set<int32_t> HouseBidsDAO::loadBids(runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::house::HouseBids>>& bidsById) {
	// Java puts a new HouseBids into the caller's map (HousingBidService.bids) for the first bid of each house (header request dao-4)
	std::unordered_set<int32_t> deletedPlayerIds;
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(LOAD_QUERY);
		auto rset = stmt->executeQuery();
		while (rset->next()) {
			int32_t playerId = rset->getInt("player_id");
			int32_t houseId = rset->getInt("house_id");
			int64_t bidOffer = rset->getLong("bid");
			std::optional<commons::database::Timestamp> time = rset->getTimestamp("bid_time");
			runtime::Ptr<HouseBids> houseBids = bidsById.get(houseId);
			if (!houseBids)
				bidsById.put(houseId, HouseBids::create(houseId, bidOffer, detail::getTime(time)));
			else {
				houseBids->bid(playerId, bidOffer, detail::getTime(time));
				if (playerId != 0 && !rset->getBoolean("playerExists"))
					deletedPlayerIds.insert(playerId);
			}
		}
	} catch (const std::exception& e) {
		log.error("Cannot read house bids", e);
	}
	return deletedPlayerIds;
}

bool HouseBidsDAO::addBid(model::house::HouseBids::Bid& bid) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(INSERT_QUERY);
		stmt->setInt(1, bid.getPlayerObjectId());
		stmt->setInt(2, bid.getHouseObjectId());
		stmt->setLong(3, bid.getKinah());
		stmt->setTimestamp(4, detail::toTimestamp(bid.getTime()));
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Cannot insert house bid", e);
		return false;
	}
	return true;
}

bool HouseBidsDAO::deleteOrDisableBids(int32_t playerObjectId, const std::vector<runtime::Ptr<model::house::HouseBids::Bid>>& bidsToDelete) {
	try {
		auto con = DatabaseFactory::getConnection();
		if (!bidsToDelete.empty()) {
			auto delStmt = con->prepareStatement(DELETE_SINGLE_BID_QUERY);
			for (const runtime::Ptr<HouseBids::Bid>& bid : bidsToDelete) {
				delStmt->setInt(1, bid->getPlayerObjectId());
				delStmt->setInt(2, bid->getHouseObjectId());
				delStmt->setLong(3, bid->getKinah());
				delStmt->execute();
			}
		}
		auto stmt = con->prepareStatement(DISABLE_QUERY);
		stmt->setInt(1, playerObjectId);
		stmt->executeUpdate();
	} catch (const std::exception& e) {
		log.error("Cannot delete or disable house bids for player " + std::to_string(playerObjectId), e);
		return false;
	}
	return true;
}

bool HouseBidsDAO::deleteHouseBids(int32_t houseId) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(DELETE_QUERY);
		stmt->setInt(1, houseId);
		stmt->execute();
		return true;
	} catch (const std::exception& e) {
		log.error("Cannot delete house bids", e);
		return false;
	}
}

} // namespace aion::gameserver::dao
