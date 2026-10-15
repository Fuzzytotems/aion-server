#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/Logger.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::dao::detail {

/**
 * The shared body of the eight Java getUsedIDs() methods (PlayerDAO, InventoryDAO, PlayerRegisteredItemsDAO, LegionDAO, MailDAO, GuideDAO,
 * HousesDAO, PlayerPetsDAO), which differ only in the query, the column and the error message:
 * <pre>
 * PreparedStatement stmt = con.prepareStatement(query, ResultSet.TYPE_SCROLL_INSENSITIVE, ResultSet.CONCUR_READ_ONLY);
 * ResultSet rs = stmt.executeQuery();
 * rs.last(); int count = rs.getRow(); rs.beforeFirst();
 * int[] ids = new int[count];
 * for (int i = 0; rs.next(); i++) ids[i] = rs.getInt(column);
 * </pre>
 * On an SQLException Java logs the message and returns null, and its only caller (IDFactory.initializeUsedIds -> lockIds(int...)) fails
 * with a NullPointerException, which aborts the server start. C++: logs the same message and throws NullPointerException, because an empty
 * vector would let the server start without the used ids locked (duplicate object ids).
 */
inline std::vector<int32_t> getUsedIDs(const commons::logging::Logger& log, std::string_view query, std::string_view column,
	std::string_view errorMessage) {
	try {
		auto con = commons::database::DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(query, commons::database::ResultSet::TYPE_SCROLL_INSENSITIVE, commons::database::ResultSet::CONCUR_READ_ONLY);
		auto rs = stmt->executeQuery();
		rs->last();
		int32_t count = rs->getRow();
		rs->beforeFirst();
		std::vector<int32_t> ids(static_cast<size_t>(count));
		for (size_t i = 0; rs->next(); i++)
			ids[i] = rs->getInt(column);
		return ids;
	} catch (const commons::database::SQLException& e) {
		log.error(errorMessage, e);
		throw runtime::NullPointerException(std::string(errorMessage) + " (Java: getUsedIDs returned null)");
	}
}

} // namespace aion::gameserver::dao::detail
