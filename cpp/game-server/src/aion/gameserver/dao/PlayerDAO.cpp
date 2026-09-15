#include "aion/gameserver/dao/PlayerDAO.h"

#include <string>
#include <string_view>

#include "aion/commons/database/DB.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/dao/detail/DaoSupport.h"
#include "aion/gameserver/dao/detail/JavaHash.h"
#include "aion/gameserver/dao/detail/UsedIds.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.h"
#include "aion/gameserver/model/Gender.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/team/legion/LegionRank.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::DB;
using commons::database::PreparedStatement;
using commons::database::ResultSet;
using commons::database::SQLException;
using model::gameobjects::player::PlayerCommonData;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerDAO");

PlayerDAO::PlayerAndLegionInfo::PlayerAndLegionInfo(int32_t playerId, std::string_view name, int32_t legionId,
	std::optional<model::team::legion::LegionRank> legionRank)
	: playerId_(playerId), name_(name), legionId_(legionId), legionRank_(legionRank) {
}

bool PlayerDAO::PlayerAndLegionInfo::equals(const PlayerAndLegionInfo& obj) const {
	return playerId_ == obj.playerId_ && name_ == obj.name_ && legionId_ == obj.legionId_ && legionRank_ == obj.legionRank_;
}

int32_t PlayerDAO::PlayerAndLegionInfo::hashCode() const {
	// Java record hashCode: 31 * h + hash(component); String.hashCode over UTF-16 code units; the enum's identity hash (differs between JVM
	// runs) is replaced by its ordinal, a null rank hashes to 0 (Objects.hashCode)
	int32_t h = playerId_;
	h = detail::combineHash(h, detail::javaStringHashCode(name_));
	h = detail::combineHash(h, legionId_);
	h = detail::combineHash(h, legionRank_ ? static_cast<int32_t>(*legionRank_) : 0);
	return h;
}

bool PlayerDAO::isNameUsed(std::string_view name) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement("SELECT count(id) as cnt FROM players WHERE ? = players.name");
		stmt->setString(1, name);
		auto rs = stmt->executeQuery();
		rs->next();
		return rs->getInt("cnt") > 0;
	} catch (const SQLException& e) {
		log.error("Can't check if name " + std::string(name) + " is used, returning positive result", e);
		return true;
	}
}

void PlayerDAO::storePlayer(model::gameobjects::player::Player& player) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(
			"UPDATE players SET name=?, exp=?, recoverexp=?, x=?, y=?, z=?, heading=?, world_id=?, gender=?, race=?, player_class=?, quest_expands=?, npc_expands=?, item_expands=?, wh_npc_expands=?, wh_bonus_expands=?, note=?, title_id=?, bonus_title_id=?, dp=?, soul_sickness=?, mailbox_letters=?, reposte_energy=?, mentor_flag_time=?, world_owner=? WHERE id=?");
		runtime::Ptr<PlayerCommonData> pcd = player.getCommonData();
		stmt->setString(1, pcd->getName());
		stmt->setLong(2, pcd->getExp());
		stmt->setLong(3, pcd->getExpRecoverable());
		stmt->setFloat(4, player.getX());
		stmt->setFloat(5, player.getY());
		stmt->setFloat(6, player.getZ());
		stmt->setInt(7, player.getHeading());
		stmt->setInt(8, player.getWorldId());
		stmt->setString(9, detail::enumName(pcd->getGender()));
		stmt->setString(10, detail::enumName(pcd->getRace()));
		stmt->setString(11, detail::enumName(pcd->getPlayerClass()));
		stmt->setInt(12, pcd->getQuestExpands());
		stmt->setInt(13, pcd->getNpcExpands());
		stmt->setInt(14, pcd->getItemExpands());
		stmt->setInt(15, pcd->getWhNpcExpands());
		stmt->setInt(16, pcd->getWhBonusExpands());
		stmt->setString(17, pcd->getNote());
		stmt->setInt(18, pcd->getTitleId());
		stmt->setInt(19, pcd->getBonusTitleId());
		stmt->setInt(20, pcd->getDp());
		stmt->setInt(21, pcd->getDeathCount());
		runtime::Ptr<model::gameobjects::player::Mailbox> mailBox = player.getMailbox();
		int32_t mails = mailBox ? mailBox->size() : pcd->getMailboxLetters();
		stmt->setInt(22, mails);
		stmt->setLong(23, pcd->getCurrentReposeEnergy());
		stmt->setInt(24, pcd->getMentorFlagTime());
		stmt->setInt(25, pcd->getWorldOwnerId());
		stmt->setInt(26, player.getObjectId());
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Error saving " + player.toString(), e);
	}
}

bool PlayerDAO::saveNewPlayer(model::gameobjects::player::Player& player, int32_t accountId, std::string_view accountName) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(
			"INSERT INTO players(id, `name`, account_id, account_name, x, y, z, heading, world_id, gender, race, player_class , quest_expands, npc_expands, item_expands, wh_npc_expands, wh_bonus_expands, online) "
			"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0)");
		stmt->setInt(1, player.getObjectId());
		stmt->setString(2, player.getName());
		stmt->setInt(3, accountId);
		stmt->setString(4, accountName);
		stmt->setFloat(5, player.getCommonData()->getX());
		stmt->setFloat(6, player.getCommonData()->getY());
		stmt->setFloat(7, player.getCommonData()->getZ());
		stmt->setInt(8, player.getCommonData()->getHeading());
		stmt->setInt(9, player.getCommonData()->getMapId());
		stmt->setString(10, detail::enumName(player.getGender()));
		stmt->setString(11, detail::enumName(player.getRace()));
		stmt->setString(12, detail::enumName(player.getPlayerClass()));
		stmt->setInt(13, player.getQuestExpands());
		stmt->setInt(14, player.getNpcExpands());
		stmt->setInt(15, player.getItemExpands());
		stmt->setInt(16, player.getWhNpcExpands());
		stmt->setInt(17, player.getWhBonusExpands());
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Error saving new " + player.toString(), e);
		return false;
	}
	return true;
}

runtime::Ref<model::gameobjects::player::PlayerCommonData> PlayerDAO::loadPlayerCommonDataByName(std::string_view name) {
	int32_t playerObjId = 0;

	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement("SELECT id FROM players WHERE name = ?");
		stmt->setString(1, name);
		auto rset = stmt->executeQuery();
		if (rset->next())
			playerObjId = rset->getInt("id");
	} catch (const std::exception& e) {
		log.error("Could not restore playerId data for player name: " + std::string(name) + " from DB: " + e.what(), e);
	}

	if (playerObjId == 0) {
		return nullptr;
	}
	return loadPlayerCommonData(playerObjId);
}

runtime::Ref<model::gameobjects::player::PlayerCommonData> PlayerDAO::loadPlayerCommonData(int32_t playerObjId) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement("SELECT * FROM players WHERE id = ?");
		stmt->setInt(1, playerObjId);
		auto resultSet = stmt->executeQuery();
		if (resultSet->next()) {
			runtime::Ref<PlayerCommonData> cd = PlayerCommonData::create(playerObjId);
			cd->setName(resultSet->getString("name"));
			cd->setPlayerClass(detail::enumValueOf<model::PlayerClass>(resultSet->getString("player_class"), "com.aionemu.gameserver.model.PlayerClass"));
			cd->setExp(resultSet->getLong("exp")); // set class before exp for daeva determination
			cd->setRecoverableExp(resultSet->getLong("recoverexp"));
			cd->setRace(detail::enumValueOf<model::Race>(resultSet->getString("race"), "com.aionemu.gameserver.model.Race"));
			cd->setGender(detail::enumValueOf<model::Gender>(resultSet->getString("gender"), "com.aionemu.gameserver.model.Gender"));
			cd->setLastOnline(resultSet->getTimestamp("last_online"));
			cd->setNote(resultSet->getString("note"));
			cd->setQuestExpands(resultSet->getInt("quest_expands"));
			cd->setNpcExpands(resultSet->getInt("npc_expands"));
			cd->setItemExpands(resultSet->getInt("item_expands"));
			cd->setTitleId(resultSet->getInt("title_id"));
			cd->setBonusTitleId(resultSet->getInt("bonus_title_id"));
			cd->setWhNpcExpands(resultSet->getInt("wh_npc_expands"));
			cd->setWhBonusExpands(resultSet->getInt("wh_bonus_expands"));
			cd->setOnline(resultSet->getBoolean("online"));
			cd->setMailboxLetters(resultSet->getInt("mailbox_letters"));
			cd->setDp(resultSet->getInt("dp"));
			cd->setDeathCount(resultSet->getInt("soul_sickness"));
			cd->setCurrentReposeEnergy(resultSet->getLong("reposte_energy"));
			cd->setX(resultSet->getFloat("x"));
			cd->setY(resultSet->getFloat("y"));
			cd->setZ(resultSet->getFloat("z"));
			cd->setHeading(resultSet->getByte("heading"));
			cd->setMapId(resultSet->getInt("world_id"));
			cd->setWorldOwnerId(resultSet->getInt("world_owner"));
			cd->setMentorFlagTime(resultSet->getInt("mentor_flag_time"));
			cd->setLastTransferTime(resultSet->getLong("last_transfer_time"));
			return cd;
		}
	} catch (const std::exception& e) {
		log.error("Could not load PlayerCommonData data for player: " + std::to_string(playerObjId), e);
	}
	return nullptr;
}

void PlayerDAO::deletePlayer(int32_t playerId) {
	std::unique_ptr<PreparedStatement> statement = DB::prepareStatement("DELETE FROM players WHERE id = ?");
	if (!statement) // Java: statement.setInt on the null that DB.prepareStatement returned
		throw runtime::NullPointerException("Cannot invoke \"java.sql.PreparedStatement.setInt(int, int)\" because \"statement\" is null");
	try {
		statement->setInt(1, playerId);
	} catch (const SQLException& e) {
		log.error("Some crap, can't set int parameter to PreparedStatement", e);
	}
	DB::executeUpdateAndClose(statement);
}

std::vector<int32_t> PlayerDAO::getPlayerOidsOnAccount(int32_t accountId) {
	std::vector<int32_t> result;
	bool success = DB::select(
		"SELECT id FROM players WHERE account_id = ?", [&](PreparedStatement& preparedStatement) { preparedStatement.setInt(1, accountId); },
		[&](ResultSet& resultSet) {
			while (resultSet.next()) {
				result.push_back(resultSet.getInt("id"));
			}
		});

	// Java returns null, which both callers (AccountService.loadAccount, PlayerTransferService) iterate: NullPointerException
	if (!success)
		throw runtime::NullPointerException("PlayerDAO.getPlayerOidsOnAccount returned null for account " + std::to_string(accountId));
	return result;
}

std::vector<int32_t> PlayerDAO::getPlayerOidsOnAccount(int32_t accountId, int64_t exp) {
	std::vector<int32_t> result;
	bool success = DB::select(
		"SELECT id FROM players WHERE account_id = ? AND exp <= ?",
		[&](PreparedStatement& preparedStatement) {
			preparedStatement.setInt(1, accountId);
			preparedStatement.setLong(2, exp);
		},
		[&](ResultSet& resultSet) {
			while (resultSet.next()) {
				result.push_back(resultSet.getInt("id"));
			}
		});

	// Java returns null (no caller uses this overload)
	if (!success)
		throw runtime::NullPointerException("PlayerDAO.getPlayerOidsOnAccount returned null for account " + std::to_string(accountId));
	return result;
}

void PlayerDAO::setCreationDeletionTime(model::account::PlayerAccountData& acData) {
	DB::select(
		"SELECT creation_date, deletion_date FROM players WHERE id = ?",
		[&](PreparedStatement& stmt) { stmt.setInt(1, acData.getPlayerCommonData()->getPlayerObjId()); },
		[&](ResultSet& rset) {
			rset.next();

			acData.setDeletionDate(rset.getTimestamp("deletion_date"));
			acData.setCreationDate(rset.getTimestamp("creation_date"));
		});
}

void PlayerDAO::updateDeletionTime(int32_t objectId, std::optional<commons::database::Timestamp> deletionDate) {
	DB::insertUpdate("UPDATE players set deletion_date = ? where id = ?", [&](PreparedStatement& preparedStatement) {
		preparedStatement.setTimestamp(1, deletionDate);
		preparedStatement.setInt(2, objectId);
		preparedStatement.execute();
	});
}

void PlayerDAO::storeCreationTime(int32_t objectId, std::optional<commons::database::Timestamp> creationDate) {
	DB::insertUpdate("UPDATE players set creation_date = ? where id = ?", [&](PreparedStatement& preparedStatement) {
		preparedStatement.setTimestamp(1, creationDate);
		preparedStatement.setInt(2, objectId);
		preparedStatement.execute();
	});
}

void PlayerDAO::storeLastOnlineTime(int32_t objectId, std::optional<commons::database::Timestamp> lastOnline) {
	DB::insertUpdate("UPDATE players set last_online = ? where id = ?", [&](PreparedStatement& preparedStatement) {
		preparedStatement.setTimestamp(1, lastOnline);
		preparedStatement.setInt(2, objectId);
		preparedStatement.execute();
	});
}

std::vector<int32_t> PlayerDAO::getUsedIDs() {
	return detail::getUsedIDs(log, "SELECT id FROM players", "id", "Can't get list of IDs from players table");
}

bool PlayerDAO::isOnline(int32_t playerId) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement("SELECT online FROM players WHERE id=?");
		stmt->setInt(1, playerId);
		auto rs = stmt->executeQuery();
		if (rs->next())
			return rs->getBoolean("online");
	} catch (const SQLException& e) {
		log.error("Can't get online state of player " + std::to_string(playerId), e);
	}
	return false;
}

void PlayerDAO::onlinePlayer(model::gameobjects::player::Player& player, bool online) {
	DB::insertUpdate("UPDATE players SET online=? WHERE id=?", [&](PreparedStatement& stmt) {
		stmt.setBoolean(1, online);
		stmt.setInt(2, player.getObjectId());
		stmt.execute();
	});
}

void PlayerDAO::setAllPlayersOffline() {
	DB::insertUpdate("UPDATE players SET online=?", [](PreparedStatement& stmt) {
		stmt.setBoolean(1, false);
		stmt.execute();
	});
}

std::optional<std::string> PlayerDAO::getPlayerNameByObjId(int32_t playerObjId) {
	std::optional<std::string> result;
	DB::select(
		"SELECT name FROM players WHERE id = ?", [&](PreparedStatement& arg0) { arg0.setInt(1, playerObjId); },
		[&](ResultSet& arg0) {
			if (arg0.next())
				result = arg0.getObject<std::string>("name");
		});
	return result;
}

int32_t PlayerDAO::getPlayerIdByName(std::string_view playerName) {
	int32_t result = 0;
	DB::select(
		"SELECT id FROM players WHERE name = ?", [&](PreparedStatement& arg0) { arg0.setString(1, playerName); },
		[&](ResultSet& arg0) {
			if (arg0.next())
				result = arg0.getInt("id");
		});
	return result;
}

int32_t PlayerDAO::getAccountIdByName(std::string_view name) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement("SELECT `account_id` FROM `players` WHERE `name` = ?");
		stmt->setString(1, name);
		auto rs = stmt->executeQuery();
		if (rs->next())
			return rs->getInt("account_id");
	} catch (const std::exception& e) {
		log.error("", e);
	}
	return 0;
}

int32_t PlayerDAO::getAccountId(int32_t playerId) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement("SELECT `account_id` FROM `players` WHERE `id` = ?");
		stmt->setInt(1, playerId);
		auto rs = stmt->executeQuery();
		if (rs->next())
			return rs->getInt("account_id");
	} catch (const std::exception& e) {
		log.error("", e);
	}
	return 0;
}

void PlayerDAO::storePlayerName(model::gameobjects::player::PlayerCommonData& recipientCommonData) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement("UPDATE players SET name=? WHERE id=?");
		stmt->setString(1, recipientCommonData.getName());
		stmt->setInt(2, recipientCommonData.getPlayerObjId());
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Error saving playerName: " + std::to_string(recipientCommonData.getPlayerObjId()) + " " + recipientCommonData.getName(), e);
	}
}

int32_t PlayerDAO::getCharacterCountOnAccount(int32_t accountId) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(
			"SELECT COUNT(*) AS cnt FROM `players` WHERE `account_id` = ? AND (players.deletion_date IS NULL || players.deletion_date > CURRENT_TIMESTAMP)");
		stmt->setInt(1, accountId);
		auto rs = stmt->executeQuery();
		rs->next();
		return rs->getInt("cnt");
	} catch (const std::exception&) {
		return 0;
	}
}

int32_t PlayerDAO::getCharacterCountForRace(model::Race race) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement("SELECT COUNT(DISTINCT(`account_id`)) AS `count` FROM `players` WHERE `race` = ? AND `exp` >= ?");
		stmt->setString(1, detail::enumName(race));
		stmt->setLong(2, dataholders::DataManager::PLAYER_EXPERIENCE_TABLE->getStartExpForLevel(configs::main::GSConfig::RATIO_MIN_REQUIRED_LEVEL.load()));
		auto rs = stmt->executeQuery();
		rs->next();
		return rs->getInt("count");
	} catch (const std::exception&) {
		return 0;
	}
}

std::vector<PlayerDAO::PlayerAndLegionInfo> PlayerDAO::getPlayersOnInactiveAccounts(int64_t maxExp, int32_t daysOfAccountInactivity) {
	std::vector<PlayerAndLegionInfo> players;
	try {
		auto con = DatabaseFactory::getConnection();
		// Java text block (its common indentation removed, one trailing line break)
		auto stmt = con->prepareStatement("SELECT p.id, p.name, m.legion_id, m.rank\n"
										  "FROM players p\n"
										  "LEFT JOIN legion_members m ON p.id = m.player_id\n"
										  "WHERE p.exp <= ? AND p.account_id IN (SELECT account_id FROM players GROUP BY account_id HAVING MAX(last_online) < NOW() - INTERVAL ? DAY)\n");
		stmt->setLong(1, maxExp);
		stmt->setInt(2, daysOfAccountInactivity);
		auto rs = stmt->executeQuery();
		while (rs->next()) {
			std::optional<std::string> legionRank = rs->getObject<std::string>("rank");
			players.emplace_back(rs->getInt("id"), rs->getString("name"), rs->getInt("legion_id"),
				!legionRank ? std::nullopt
							: std::optional(detail::enumValueOf<model::team::legion::LegionRank>(*legionRank, "com.aionemu.gameserver.model.team.legion.LegionRank")));
		}
	} catch (const SQLException& e) {
		log.error("Couldn't get inactive players", e);
	}
	return players;
}

void PlayerDAO::setPlayerLastTransferTime(int32_t playerId, int64_t time) {
	DB::insertUpdate("UPDATE players SET last_transfer_time=? WHERE id=?", [&](PreparedStatement& stmt) {
		stmt.setLong(1, time);
		stmt.setInt(2, playerId);
		stmt.execute();
	});
}

int32_t PlayerDAO::getOldCharacterLevel(int32_t playerObjectId) {
	int32_t oldLevel = 0;
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement("SELECT old_level FROM players WHERE id=?");
		stmt->setInt(1, playerObjectId);
		auto rs = stmt->executeQuery();
		if (rs->next())
			oldLevel = rs->getInt("old_level");
	} catch (const std::exception& e) {
		log.error("Error reading old_level for player: " + std::to_string(playerObjectId), e);
	}
	return oldLevel;
}

void PlayerDAO::storeOldCharacterLevel(int32_t playerObjectId, int32_t level) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement("UPDATE players SET old_level=? WHERE id=?");
		stmt->setInt(1, level);
		stmt->setInt(2, playerObjectId);
		stmt->execute();
	} catch (const std::exception& e) {
		log.error("Error storing old_level: " + std::to_string(level) + " for player: " + std::to_string(playerObjectId), e);
	}
}

} // namespace aion::gameserver::dao
