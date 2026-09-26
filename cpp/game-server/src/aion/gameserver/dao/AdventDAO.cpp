#include "aion/gameserver/dao/AdventDAO.h"

#include <optional>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/database/SQLException.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::SQLException;

bool AdventDAO::canReceiveReward(model::gameobjects::player::Player& player, commons::database::Date date) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement("SELECT `last_day_received` FROM `advent` WHERE ? = account_id");
		stmt->setInt(1, player.getAccount()->getId());
		auto rs = stmt->executeQuery();
		if (!rs->next())
			return true;
		std::optional<commons::database::Date> lastDayReceived = rs->getDate("last_day_received");
		if (!lastDayReceived) // NOT NULL column; Java: toLocalDate() on null
			throw runtime::NullPointerException("Cannot invoke \"java.sql.Date.toLocalDate()\" because the date is null");
		return *lastDayReceived < date;
	} catch (const SQLException&) {
		return false;
	}
}

bool AdventDAO::storeLastReceivedDay(model::gameobjects::player::Player& player, commons::database::Date date) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement("REPLACE INTO `advent` VALUES (?, ?)");
		stmt->setInt(1, player.getAccount()->getId());
		stmt->setDate(2, date);
		stmt->execute();
		return true;
	} catch (const SQLException&) {
		return false;
	}
}

} // namespace aion::gameserver::dao
