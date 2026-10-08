#include "aion/gameserver/services/DatabaseCleaningService.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <optional>
#include <unordered_set>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/DatabaseMetaData.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/commons/utils/concurrent/ThreadName.h"
#include "aion/gameserver/configs/main/CleaningConfig.h"
#include "aion/gameserver/dao/LegionDAO.h"
#include "aion/gameserver/dao/LegionMemberDAO.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.bind.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.h"
#include "aion/gameserver/dataholders/detail/JaxbDeserialize.h"
#include "aion/gameserver/model/team/legion/LegionHistoryAction.h"
#include "aion/gameserver/model/team/legion/LegionHistoryEntry.h"
#include "aion/gameserver/model/team/legion/LegionRank.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/services/player/PlayerService.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.DatabaseCleaningService");

namespace {

/** Java String.join(delimiter, elements) */
std::string join(const std::vector<std::string>& elements, std::string_view delimiter) {
	std::string joined;
	for (size_t i = 0; i < elements.size(); i++) {
		if (i > 0)
			joined += delimiter;
		joined += elements[i];
	}
	return joined;
}

} // namespace

// Java DatabaseCleaningService.java:30-49. Java's main thread is the thread with id 1; C++ names its main thread "main" (main.cpp:638), the
// only thread of that name
void DatabaseCleaningService::deletePlayersOnInactiveAccounts() {
	if (commons::utils::concurrent::getCurrentThreadName() != "main")
		throw runtime::IllegalStateException("DatabaseCleaningService can only be run from the main thread on server startup");

	if (configs::main::CleaningConfig::MIN_ACCOUNT_INACTIVITY_DAYS.load() <= 30)
		throw runtime::IllegalArgumentException(
			"The configured days for database cleaning is too low. For safety reasons the service will only execute with periods over 30 days");

	std::vector<dao::PlayerDAO::PlayerAndLegionInfo> players = dao::PlayerDAO::getPlayersOnInactiveAccounts(
		toMaxExp(configs::main::CleaningConfig::MAX_DELETABLE_CHAR_LEVEL.load()), configs::main::CleaningConfig::MIN_ACCOUNT_INACTIVITY_DAYS.load());
	if (players.empty()) {
		log.info("Found no inactive accounts with characters level <={} to delete", configs::main::CleaningConfig::MAX_DELETABLE_CHAR_LEVEL.load());
		return;
	}
	deletePlayers(players);
	std::vector<dao::PlayerDAO::PlayerAndLegionInfo> remainingLegionMembers = deleteEmptyLegions(players);
	maintainBrigadeGenerals(remainingLegionMembers);
	addLegionHistoryLeaveEntry(remainingLegionMembers);
	if (players.size() >= 500)
		optimizeDatabaseTables(withForeignKeyTables({"players", "inventory"}));
}

// Java DatabaseCleaningService.java:51-55: the experience table alone, before the static data loads (JAXBUtil.deserialize; C++ binds without
// the xsd, as every JAXB root outside static data, JaxbDeserialize.h)
int64_t DatabaseCleaningService::toMaxExp(int32_t charLevel) {
	std::unique_ptr<dataholders::PlayerExperienceTable> pxt = dataholders::detail::deserializeFile<dataholders::PlayerExperienceTable>(
		"./data/static_data/player_experience_table.xml", "com.aionemu.gameserver.dataholders.PlayerExperienceTable");
	return pxt->getStartExpForLevel(charLevel + 1) - 1;
}

// Java DatabaseCleaningService.java:57-66
void DatabaseCleaningService::deletePlayers(const std::vector<dao::PlayerDAO::PlayerAndLegionInfo>& players) {
	int64_t startMillis = commons::utils::currentTimeMillis();
	log.info("Deleting {} characters level <={} from inactive accounts...", players.size(), configs::main::CleaningConfig::MAX_DELETABLE_CHAR_LEVEL.load());
	for (size_t i = 0; i < players.size(); i++) {
		if (i % 20 == 0)
			std::printf("Progress: %4.1f%%\r", static_cast<double>(static_cast<float>(i) * 100.0f / static_cast<float>(players.size())));
		player::PlayerService::deletePlayerFromDB(players[i].playerId(), false);
	}
	log.info("Deleted characters and related data from database in {} seconds", (commons::utils::currentTimeMillis() - startMillis) / 1000);
}

// Java DatabaseCleaningService.java:68-84
std::vector<dao::PlayerDAO::PlayerAndLegionInfo> DatabaseCleaningService::deleteEmptyLegions(const std::vector<dao::PlayerDAO::PlayerAndLegionInfo>& players) {
	std::vector<dao::PlayerDAO::PlayerAndLegionInfo> remainingLegionMembers;
	std::unordered_set<int32_t> deleted;
	for (const dao::PlayerDAO::PlayerAndLegionInfo& player : players) {
		if (player.legionId() == 0 || deleted.contains(player.legionId()))
			continue;
		if (dao::LegionMemberDAO::loadLegionMembers(player.legionId()).empty()) {
			LegionService::deleteLegionFromDB(player.legionId());
			deleted.insert(player.legionId());
		} else {
			remainingLegionMembers.push_back(player);
		}
	}
	if (!deleted.empty())
		log.info("Deleted {} empty legions", deleted.size());
	return remainingLegionMembers;
}

// Java DatabaseCleaningService.java:86-102. PlayerDAO.getPlayerNameByObjId answers null for a missing row: the log line says "null" as Java's
// does, the history entry gets the empty name (Java: a null name column); the row exists, since setRank just updated it
void DatabaseCleaningService::maintainBrigadeGenerals(const std::vector<dao::PlayerDAO::PlayerAndLegionInfo>& deletedLegionMembers) {
	for (const dao::PlayerDAO::PlayerAndLegionInfo& deletedLegionMember : deletedLegionMembers) {
		if (deletedLegionMember.legionRank() != model::team::legion::LegionRank::BRIGADE_GENERAL)
			continue;
		std::vector<int32_t> legionMembers = dao::LegionMemberDAO::loadLegionMembers(deletedLegionMember.legionId());
		if (legionMembers.empty() || std::ranges::find(legionMembers, deletedLegionMember.playerId()) != legionMembers.end())
			continue;
		int32_t newBrigadeGeneralId = legionMembers.size() == 1 ? legionMembers.front() : 0;
		if (newBrigadeGeneralId != 0 && dao::LegionMemberDAO::setRank(newBrigadeGeneralId, model::team::legion::LegionRank::BRIGADE_GENERAL)) {
			std::optional<std::string> newBrigadeGeneralName = dao::PlayerDAO::getPlayerNameByObjId(newBrigadeGeneralId);
			log.info("Transferred brigade general of legion {} from deleted player {} to the only remaining member {}", deletedLegionMember.legionId(),
				deletedLegionMember.name(), newBrigadeGeneralName.value_or("null"));
			dao::LegionDAO::insertHistory(deletedLegionMember.legionId(), model::team::legion::LegionHistoryAction::APPOINTED,
				newBrigadeGeneralName.value_or(""), "");
		} else {
			log.warn("Legion {} has no brigade general anymore", deletedLegionMember.legionId());
		}
	}
}

// Java DatabaseCleaningService.java:104-107
void DatabaseCleaningService::addLegionHistoryLeaveEntry(const std::vector<dao::PlayerDAO::PlayerAndLegionInfo>& players) {
	for (const dao::PlayerDAO::PlayerAndLegionInfo& player : players)
		dao::LegionDAO::insertHistory(player.legionId(), model::team::legion::LegionHistoryAction::KICK, player.name(), "");
}

// Java DatabaseCleaningService.java:109-120
void DatabaseCleaningService::optimizeDatabaseTables(const std::vector<std::string>& tables) {
	int64_t startMillis = commons::utils::currentTimeMillis();
	log.info("Optimizing {} database tables: {}", tables.size(), join(tables, ", "));
	try {
		commons::database::PooledConnection con = commons::database::DatabaseFactory::getConnection();
		std::unique_ptr<commons::database::PreparedStatement> stmt = con->prepareStatement("OPTIMIZE TABLE " + join(tables, ","));
		stmt->execute();
		log.info("Optimized database tables in {} seconds", (commons::utils::currentTimeMillis() - startMillis) / 1000);
	} catch (const std::exception& e) {
		log.error("Optimize table failed", e);
	}
}

// Java DatabaseCleaningService.java:122-139: baseTables plus all tables which have a foreign key referencing the primary key of one of the given
// baseTables (a LinkedHashSet: the first occurrence keeps its place)
std::vector<std::string> DatabaseCleaningService::withForeignKeyTables(std::initializer_list<std::string_view> baseTables) {
	std::vector<std::string> tables;
	auto add = [&tables](std::string table) {
		if (std::ranges::find(tables, table) == tables.end())
			tables.push_back(std::move(table));
	};
	for (std::string_view table : baseTables) {
		add(std::string(table));
		try {
			commons::database::PooledConnection con = commons::database::DatabaseFactory::getConnection();
			std::unique_ptr<commons::database::ResultSet> importedKeys = con->getMetaData().getExportedKeys(con->getCatalog(), std::nullopt, table);
			while (importedKeys->next())
				add(importedKeys->getString("FKTABLE_NAME"));
		} catch (const std::exception& e) {
			log.error("Failed to collect tables on players table", e);
		}
	}
	return tables;
}

} // namespace aion::gameserver::services
