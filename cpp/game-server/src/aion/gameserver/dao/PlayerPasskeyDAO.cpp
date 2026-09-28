#include "aion/gameserver/dao/PlayerPasskeyDAO.h"

#include <string>
#include <string_view>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::SQLException;

namespace {

constexpr std::string_view INSERT_QUERY = "INSERT INTO `player_passkey` (`account_id`, `passkey`) VALUES (?,?)";
constexpr std::string_view UPDATE_QUERY = "UPDATE `player_passkey` SET `passkey`=? WHERE `account_id`=? AND `passkey`=?";
constexpr std::string_view UPDATE_FORCE_QUERY = "UPDATE `player_passkey` SET `passkey`=? WHERE `account_id`=?";
constexpr std::string_view CHECK_QUERY = "SELECT COUNT(*) cnt FROM `player_passkey` WHERE `account_id`=? AND `passkey`=?";
constexpr std::string_view EXIST_CHECK_QUERY = "SELECT COUNT(*) cnt FROM `player_passkey` WHERE `account_id`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.PlayerPasskeyDAO");

void PlayerPasskeyDAO::insertPlayerPasskey(int32_t accountId, std::string_view passkey) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(INSERT_QUERY);
		stmt->setInt(1, accountId);
		stmt->setString(2, passkey);
		stmt->execute();
	} catch (const SQLException& e) {
		log.error("Error saving PlayerPasskey. accountId: " + std::to_string(accountId), e);
	}
}

bool PlayerPasskeyDAO::updatePlayerPasskey(int32_t accountId, std::string_view oldPasskey, std::string_view newPasskey) {
	bool result = false;
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(UPDATE_QUERY);
		stmt->setString(1, newPasskey);
		stmt->setInt(2, accountId);
		stmt->setString(3, oldPasskey);
		if (stmt->executeUpdate() > 0)
			result = true;
	} catch (const SQLException& e) {
		log.error("Error updating PlayerPasskey. accountId: " + std::to_string(accountId), e);
	}
	return result;
}

bool PlayerPasskeyDAO::updateForcePlayerPasskey(int32_t accountId, std::string_view newPasskey) {
	bool result = false;
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(UPDATE_FORCE_QUERY);
		stmt->setString(1, newPasskey);
		stmt->setInt(2, accountId);
		if (stmt->executeUpdate() > 0)
			result = true;
	} catch (const SQLException& e) {
		log.error("Error updaing PlayerPasskey. accountId: " + std::to_string(accountId), e);
	}
	return result;
}

bool PlayerPasskeyDAO::checkPlayerPasskey(int32_t accountId, std::string_view passkey) {
	bool passkeyChecked = false;
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(CHECK_QUERY);
		stmt->setInt(1, accountId);
		stmt->setString(2, passkey);
		auto rset = stmt->executeQuery();
		if (rset->next()) {
			if (rset->getInt("cnt") == 1)
				passkeyChecked = true;
		}
	} catch (const SQLException& e) {
		log.error("Error loading PlayerPasskey. accountId: " + std::to_string(accountId), e);
		return false;
	}
	return passkeyChecked;
}

bool PlayerPasskeyDAO::existCheckPlayerPasskey(int32_t accountId) {
	bool existPasskeyChecked = false;
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(EXIST_CHECK_QUERY);
		stmt->setInt(1, accountId);
		auto rset = stmt->executeQuery();
		if (rset->next()) {
			if (rset->getInt("cnt") == 1)
				existPasskeyChecked = true;
		}
	} catch (const SQLException& e) {
		log.error("Error loading PlayerPasskey. accountId: " + std::to_string(accountId), e);
		return false;
	}
	return existPasskeyChecked;
}

} // namespace aion::gameserver::dao
