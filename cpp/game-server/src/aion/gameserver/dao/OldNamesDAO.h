#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "aion/gameserver/dao/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author synchro2
 */
class OldNamesDAO {
public:
	/** @param oldName null for a new character (bound as SQL NULL) */
	static bool isNameReserved(std::optional<std::string_view> oldName, std::string_view newName, int32_t nameReservationDurationDays);
	static void insertNames(int32_t playerId, std::string_view oldName, std::string_view newName);
};

} // namespace aion::gameserver::dao
