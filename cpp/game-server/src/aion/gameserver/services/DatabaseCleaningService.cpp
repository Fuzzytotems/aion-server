#include "aion/gameserver/services/DatabaseCleaningService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.DatabaseCleaningService");

void DatabaseCleaningService::deletePlayersOnInactiveAccounts() {
	AION_UNPORTED();
}

int64_t DatabaseCleaningService::toMaxExp(int32_t charLevel) {
	AION_UNPORTED();
}

void DatabaseCleaningService::deletePlayers(const std::vector<dao::PlayerDAO::PlayerAndLegionInfo>& players) {
	AION_UNPORTED();
}

std::vector<dao::PlayerDAO::PlayerAndLegionInfo> DatabaseCleaningService::deleteEmptyLegions(const std::vector<dao::PlayerDAO::PlayerAndLegionInfo>& players) {
	AION_UNPORTED();
}

void DatabaseCleaningService::maintainBrigadeGenerals(const std::vector<dao::PlayerDAO::PlayerAndLegionInfo>& deletedLegionMembers) {
	AION_UNPORTED();
}

void DatabaseCleaningService::addLegionHistoryLeaveEntry(const std::vector<dao::PlayerDAO::PlayerAndLegionInfo>& players) {
	AION_UNPORTED();
}

void DatabaseCleaningService::optimizeDatabaseTables(const std::vector<std::string>& tables) {
	AION_UNPORTED();
}

std::vector<std::string> DatabaseCleaningService::withForeignKeyTables(std::initializer_list<std::string_view> baseTables) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
