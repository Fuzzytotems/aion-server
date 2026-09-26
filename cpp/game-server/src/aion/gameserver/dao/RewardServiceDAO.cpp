#include "aion/gameserver/dao/RewardServiceDAO.h"

#include <chrono>
#include <string>
#include <string_view>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/templates/rewards/RewardEntryItem.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.RewardServiceDAO");

namespace {
constexpr std::string_view UPDATE_QUERY = "UPDATE `player_web_rewards` SET `received`=? WHERE `entry_id`=?";
constexpr std::string_view SELECT_QUERY =
	"SELECT entry_id, item_id, item_count FROM `player_web_rewards` WHERE `player_id`=? AND `received` IS NULL";
} // namespace

std::vector<runtime::Ref<model::templates::rewards::RewardEntryItem>> RewardServiceDAO::loadUnreceived(int32_t playerId) {
	std::vector<runtime::Ref<model::templates::rewards::RewardEntryItem>> list;
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_QUERY);
		stmt->setInt(1, playerId);
		auto rset = stmt->executeQuery();
		while (rset->next()) {
			int32_t entryId = rset->getInt("entry_id");
			int32_t itemId = rset->getInt("item_id");
			int64_t count = rset->getLong("item_count");
			list.push_back(model::templates::rewards::RewardEntryItem::create(entryId, itemId, count));
		}
	} catch (const std::exception& e) {
		log.error("Couldn't load unreceived web rewards for player " + std::to_string(playerId), e);
	}
	return list;
}

void RewardServiceDAO::storeReceived(const std::vector<int32_t>& ids, int64_t timeReceived) {
	const commons::database::Timestamp time{std::chrono::milliseconds(timeReceived)};
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(UPDATE_QUERY);
		con->setAutoCommit(false);
		for (int32_t entryId : ids) {
			stmt->setTimestamp(1, time);
			stmt->setInt(2, entryId);
			stmt->addBatch();
		}
		stmt->executeBatch();
		con->commit();
	} catch (const std::exception& e) {
		// Java: Arrays.toString(ids.toArray())
		std::string idList = "[";
		for (size_t i = 0; i < ids.size(); i++) {
			if (i > 0)
				idList += ", ";
			idList += std::to_string(ids[i]);
		}
		idList += "]";
		log.error("Error saving received web rewards, player could potentially receive rewards multiple times! Check entry_id's: " + idList, e);
	}
}

} // namespace aion::gameserver::dao
