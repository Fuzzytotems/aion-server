#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/dao/PlayerDAO.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * Offers the functionality to delete all data about inactive players
 * <p>
 * C++: a static-only class (hub-headers.md §11.1). PlayerDAO.h is included for the nested record PlayerDAO::PlayerAndLegionInfo
 * (hub-headers.md §9.3).
 *
 * @author nrg, Neon
 */
class DatabaseCleaningService {
private:
	DatabaseCleaningService() = delete;
public:
	static void deletePlayersOnInactiveAccounts();
private:
	static int64_t toMaxExp(int32_t charLevel);
	static void deletePlayers(const std::vector<dao::PlayerDAO::PlayerAndLegionInfo>& players);
	static std::vector<dao::PlayerDAO::PlayerAndLegionInfo> deleteEmptyLegions(const std::vector<dao::PlayerDAO::PlayerAndLegionInfo>& players);
	static void maintainBrigadeGenerals(const std::vector<dao::PlayerDAO::PlayerAndLegionInfo>& deletedLegionMembers);
	static void addLegionHistoryLeaveEntry(const std::vector<dao::PlayerDAO::PlayerAndLegionInfo>& players);
	static void optimizeDatabaseTables(const std::vector<std::string>& tables);
	static std::vector<std::string> withForeignKeyTables(std::initializer_list<std::string_view> baseTables = {});
};

} // namespace aion::gameserver::services
