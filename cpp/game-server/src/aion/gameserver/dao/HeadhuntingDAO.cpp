#include "aion/gameserver/dao/HeadhuntingDAO.h"

#include <string_view>

#include "aion/commons/database/DB.h"
#include "aion/gameserver/dao/detail/DaoSupport.h"
#include "aion/gameserver/model/event/Headhunter.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/services/PvpService.h"

namespace aion::gameserver::dao {

using commons::database::DB;
using commons::database::PreparedStatement;
using commons::database::ResultSet;
using model::gameobjects::Persistable;

namespace {

constexpr std::string_view SELECT_QUERY = "SELECT * FROM `headhunting`";
constexpr std::string_view UPDATE_QUERY = "REPLACE INTO `headhunting` (`hunter_id`, `accumulated_kills`, `last_update`) VALUES (?,?,?)";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `headhunting`";

} // namespace

std::map<int32_t, runtime::Ref<model::event::Headhunter>> HeadhuntingDAO::loadHeadhunters() {
	std::map<int32_t, runtime::Ref<model::event::Headhunter>> loadedHunters;
	DB::select(
		SELECT_QUERY, [](PreparedStatement&) {},
		[&](ResultSet& rset) {
			while (rset.next()) {
				int32_t playerId = rset.getInt("hunter_id");
				if (!loadedHunters.contains(playerId)) {
					int32_t accumulatedKills = rset.getInt("accumulated_kills");
					int64_t lastUpdate = detail::getTime(rset.getTimestamp("last_update"));
					loadedHunters.insert_or_assign(playerId,
						model::event::Headhunter::create(playerId, accumulatedKills, lastUpdate, Persistable::PersistentState::UPDATED));
				}
			}
		});
	return loadedHunters;
}

bool HeadhuntingDAO::clearTables() {
	return DB::insertUpdate(DELETE_QUERY, [](PreparedStatement& stmt) { stmt.execute(); });
}

void HeadhuntingDAO::storeHeadhunter(int32_t hunterId) {
	runtime::Ptr<model::event::Headhunter> hunter = services::PvpService::getInstance().getHeadhunter(hunterId);
	if (!hunter || hunter->getPersistentState() != Persistable::PersistentState::UPDATE_REQUIRED)
		return;
	bool success = DB::insertUpdate(UPDATE_QUERY, [&](PreparedStatement& stmt) {
		stmt.setInt(1, hunter->getHunterId());
		stmt.setInt(2, hunter->getKills());
		stmt.setTimestamp(3, detail::toTimestamp(hunter->getLastUpdate()));
		stmt.execute();
	});
	if (success)
		hunter->setPersistentState(Persistable::PersistentState::UPDATED);
}

} // namespace aion::gameserver::dao
