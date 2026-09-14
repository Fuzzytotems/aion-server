#include "aion/gameserver/dao/OldNamesDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.OldNamesDAO");

bool OldNamesDAO::isNameReserved(std::optional<std::string_view> oldName, std::string_view newName, int32_t nameReservationDurationDays) {
	AION_UNPORTED();
}

void OldNamesDAO::insertNames(int32_t playerId, std::string_view oldName, std::string_view newName) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
