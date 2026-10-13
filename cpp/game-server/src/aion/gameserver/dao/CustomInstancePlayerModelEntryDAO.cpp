#include "aion/gameserver/dao/CustomInstancePlayerModelEntryDAO.h"

#include <exception>
#include <string>

#include "aion/commons/database/Connection.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/custom/instance/neuralnetwork/PlayerModelEntry.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::SQLException;
using custom::instance::neuralnetwork::PlayerModelEntry;

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view SELECT_QUERY = "SELECT * FROM `custom_instance_records` WHERE ? = player_id";
constexpr std::string_view INSERT_QUERY = "INSERT INTO `custom_instance_records` ( `player_id`, `timestamp`, `skill_id`, `player_class_id`, `player_hp_percentage`,`player_mp_percentage`, `player_is_rooted`, `player_is_silenced`, `player_is_bound`, `player_is_stunned`, `player_is_aetherhold`,`player_buff_count`, `player_debuff_count`, `player_is_shielded`, `target_hp_percentage`, `target_mp_percentage`,`target_focuses_player`, `distance`, `target_is_rooted`, `target_is_silenced`, `target_is_bound`, `target_is_stunned`,`target_is_aetherhold`, `target_buff_count`, `target_debuff_count`, `target_is_shielded`) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

/** the NOT NULL column `timestamp` (aion_gs.sql:201) */
commons::database::Timestamp timestampOf(const std::optional<commons::database::Timestamp>& timestamp) {
	if (!timestamp)
		throw runtime::NullPointerException("custom_instance_records.timestamp is NULL");
	return *timestamp;
}

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.CustomInstancePlayerModelEntryDAO");

// Java CustomInstancePlayerModelEntryDAO.java:39-61
std::vector<runtime::Ref<custom::instance::neuralnetwork::PlayerModelEntry>> CustomInstancePlayerModelEntryDAO::loadPlayerModelEntries(
	int32_t playerId) {
	std::vector<runtime::Ref<PlayerModelEntry>> entries;
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_QUERY);
		stmt->setInt(1, playerId);
		auto rset = stmt->executeQuery();
		while (rset->next()) {
			runtime::Ref<PlayerModelEntry> pme = PlayerModelEntry::create(playerId, timestampOf(rset->getTimestamp("timestamp")), rset->getInt("skill_id"),
				rset->getInt("player_class_id"), rset->getFloat("player_hp_percentage"), rset->getFloat("player_mp_percentage"),
				rset->getBoolean("player_is_rooted"), rset->getBoolean("player_is_silenced"), rset->getBoolean("player_is_bound"),
				rset->getBoolean("player_is_stunned"), rset->getBoolean("player_is_aetherhold"), rset->getInt("player_buff_count"),
				rset->getInt("player_debuff_count"), rset->getBoolean("player_is_shielded"), rset->getFloat("target_hp_percentage"),
				rset->getFloat("target_mp_percentage"), rset->getBoolean("target_focuses_player"), rset->getFloat("distance"),
				rset->getBoolean("target_is_rooted"), rset->getBoolean("target_is_silenced"), rset->getBoolean("target_is_bound"),
				rset->getBoolean("target_is_stunned"), rset->getBoolean("target_is_aetherhold"), rset->getInt("target_buff_count"),
				rset->getInt("target_debuff_count"), rset->getBoolean("target_is_shielded"));
			entries.push_back(pme);
		}
	} catch (const SQLException& e) {
		log.error("[CUSTOM_INSTANCE] Error loading player model entries on player id " + std::to_string(playerId), e);
	}
	return entries;
}

// Java CustomInstancePlayerModelEntryDAO.java:63-100
void CustomInstancePlayerModelEntryDAO::insertNewRecords(
	const std::vector<runtime::Ptr<custom::instance::neuralnetwork::PlayerModelEntry>>& filteredEntries) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(INSERT_QUERY);
		con->setAutoCommit(false);
		for (const runtime::Ptr<PlayerModelEntry>& pme : filteredEntries) {
			stmt->setInt(1, pme->getPlayerID());
			stmt->setTimestamp(2, pme->getTimestamp());
			stmt->setInt(3, pme->getSkillID());
			stmt->setInt(4, pme->getPlayerClassID());
			stmt->setFloat(5, pme->getPlayerHPpercentage());
			stmt->setFloat(6, pme->getPlayerMPpercentage());
			stmt->setBoolean(7, pme->isPlayerRooted());
			stmt->setBoolean(8, pme->isPlayerSilenced());
			stmt->setBoolean(9, pme->isPlayerBound());
			stmt->setBoolean(10, pme->isPlayerStunned());
			stmt->setBoolean(11, pme->isPlayerAetherhold());
			stmt->setInt(12, pme->getPlayerBuffCount());
			stmt->setInt(13, pme->getPlayerDebuffCount());
			stmt->setBoolean(14, pme->isPlayerIsShielded());
			stmt->setFloat(15, pme->getTargetHPpercentage());
			stmt->setFloat(16, pme->getTargetMPpercentage());
			stmt->setBoolean(17, pme->isTargetFocusesPlayer());
			stmt->setFloat(18, pme->getDistance());
			stmt->setBoolean(19, pme->isTargetRooted());
			stmt->setBoolean(20, pme->isTargetSilenced());
			stmt->setBoolean(21, pme->isTargetBound());
			stmt->setBoolean(22, pme->isTargetStunned());
			stmt->setBoolean(23, pme->isTargetAetherhold());
			stmt->setInt(24, pme->getTargetBuffCount());
			stmt->setInt(25, pme->getTargetDebuffCount());
			stmt->setBoolean(26, pme->isTargetIsShielded());
			stmt->addBatch();

			pme->setPersistentState(model::gameobjects::Persistable::PersistentState::UPDATED);
		}
		stmt->executeBatch();
		con->commit();
	} catch (const std::exception& e) {
		log.error("Error occured while saving player model entries.", e);
	}
}

} // namespace aion::gameserver::dao
