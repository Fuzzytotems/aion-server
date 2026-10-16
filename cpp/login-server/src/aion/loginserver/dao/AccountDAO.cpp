#include "aion/loginserver/dao/AccountDAO.h"

#include <fmt/format.h>

#include "aion/commons/database/DB.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/loginserver/configs/Config.h"

namespace aion::loginserver::dao::AccountDAO {

using commons::database::DatabaseFactory;
using commons::database::DB;
using commons::database::PreparedStatement;
using commons::database::SQLException;
using commons::database::Statement;
using configs::Config;
using model::Account;
using model::AccountTime;

namespace {

const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.loginserver.dao.AccountDAO");

/** Java: MYSQL_TABLE_ACCOUNT_NAME (see the header comment on why it is evaluated per call) */
std::string_view accountNameColumn() {
	return Config::useExternalAuth() ? "ext_auth_name" : "name";
}

template <typename Param>
std::shared_ptr<Account> getAccount(std::string_view accountQuery, const Param& accountQueryParam) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto st = con->prepareStatement(accountQuery);
		st->setObject(1, accountQueryParam);
		auto rs = st->executeQuery();
		if (rs->next()) {
			auto account = std::make_shared<Account>();
			account->setId(rs->getInt("id"));
			account->setName(rs->getString(accountNameColumn()));
			account->setPasswordHash(rs->getString("password"));
			account->setCreationDate(rs->getTimestamp("creation_date"));
			account->setAccessLevel(rs->getByte("access_level"));
			account->setMembership(rs->getByte("membership"));
			account->setActivated(rs->getByte("activated"));
			account->setLastServer(rs->getByte("last_server"));
			account->setLastIp(rs->getObject<std::string>("last_ip"));
			account->setLastMac(rs->getString("last_mac"));
			account->setIpForce(rs->getObject<std::string>("ip_force"));
			account->setAllowedHddSerial(rs->getObject<std::string>("allowed_hdd_serial"));
			return account;
		}
	} catch (const SQLException& e) {
		log.error(fmt::format("Could not load account for: {}", accountQueryParam), e);
	}
	return nullptr;
}

} // namespace

std::shared_ptr<Account> getAccount(std::string_view name) {
	return getAccount(fmt::format("SELECT * FROM account_data WHERE `{}` = ?", accountNameColumn()), name);
}

std::shared_ptr<Account> getAccount(int32_t id) {
	return getAccount("SELECT * FROM account_data WHERE `id` = ?", id);
}

bool insertAccount(Account& account) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto st = con->prepareStatement(
			fmt::format("INSERT INTO account_data(`{}`, `password`, access_level, membership, activated, last_server, last_ip, last_mac, ip_force) "
									"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
				accountNameColumn()),
			Statement::RETURN_GENERATED_KEYS);
		st->setString(1, account.getName());
		st->setString(2, account.getPasswordHash());
		st->setByte(3, account.getAccessLevel());
		st->setByte(4, account.getMembership());
		st->setByte(5, account.getActivated());
		st->setByte(6, account.getLastServer());
		st->setString(7, account.getLastIp());
		st->setString(8, account.getLastMac());
		st->setString(9, account.getIpForce());
		if (st->executeUpdate() == 0)
			throw SQLException("");
		auto rs = st->getGeneratedKeys();
		if (!rs->next())
			throw SQLException("Could not get ID of created account");
		account.setId(rs->getInt(1));
		account.setAccountTime(AccountTime());
		account.setCreationDate(model::currentTimestamp());
		return true;
	} catch (const SQLException& e) {
		log.error("Could not insert account for: " + account.getName(), e);
	}
	return false;
}

bool updateAccount(const Account& account) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto st = con->prepareStatement(fmt::format("UPDATE account_data SET `{}` = ?, `password` = ?, access_level = ?, membership = ?, last_server = ?, "
																								"last_ip = ?, last_mac = ?, ip_force = ? WHERE `id` = ?",
			accountNameColumn()));
		st->setString(1, account.getName());
		st->setString(2, account.getPasswordHash());
		st->setByte(3, account.getAccessLevel());
		st->setByte(4, account.getMembership());
		st->setByte(5, account.getLastServer());
		st->setString(6, account.getLastIp());
		st->setString(7, account.getLastMac());
		st->setString(8, account.getIpForce());
		st->setInt(9, account.getId().value());
		return st->executeUpdate() > 0;
	} catch (const SQLException& e) {
		log.error("Could not update account for: " + account.getName(), e);
	}
	return false;
}

bool updateLastServer(int32_t accountId, int8_t lastServer) {
	return DB::insertUpdate("UPDATE account_data SET last_server = ? WHERE id = ?", [&](PreparedStatement& st) {
		st.setByte(1, lastServer);
		st.setInt(2, accountId);
		st.execute();
	});
}

bool updateLastIp(int32_t accountId, std::string_view ip) {
	return DB::insertUpdate("UPDATE account_data SET last_ip = ? WHERE id = ?", [&](PreparedStatement& st) {
		st.setString(1, ip);
		st.setInt(2, accountId);
		st.execute();
	});
}

std::string getLastIp(int32_t accountId) {
	std::string lastIp;
	try {
		auto con = DatabaseFactory::getConnection();
		auto st = con->prepareStatement("SELECT `last_ip` FROM `account_data` WHERE `id` = ?");
		st->setInt(1, accountId);
		auto rs = st->executeQuery();
		if (rs->next())
			lastIp = rs->getString("last_ip");
	} catch (const std::exception& e) {
		log.error("Can't select last IP of account ID: " + std::to_string(accountId), e);
	}
	return lastIp;
}

bool updateLastMac(int32_t accountId, std::string_view mac) {
	return DB::insertUpdate("UPDATE `account_data` SET `last_mac` = ? WHERE `id` = ?", [&](PreparedStatement& st) {
		st.setString(1, mac);
		st.setInt(2, accountId);
		st.execute();
	});
}

bool updateLastHDDSerial(int32_t accountId, std::string_view hddSerial) {
	return DB::insertUpdate("UPDATE `account_data` SET `last_hdd_serial` = ? WHERE `id` = ?", [&](PreparedStatement& st) {
		st.setString(1, hddSerial);
		st.setInt(2, accountId);
		st.execute();
	});
}

bool updateMembership(int32_t accountId) {
	return DB::insertUpdate("UPDATE account_data SET membership = old_membership, expire = NULL WHERE id = ? and expire < CURRENT_TIMESTAMP",
		[&](PreparedStatement& st) {
			st.setInt(1, accountId);
			st.execute();
		});
}

bool updateAllowedHDDSerial(int32_t accountId, std::string_view hddSerial) {
	return DB::insertUpdate("UPDATE `account_data` SET `allowed_hdd_serial` = ? WHERE `id` = ?", [&](PreparedStatement& st) {
		st.setString(1, hddSerial);
		st.setInt(2, accountId);
		st.execute();
	});
}

} // namespace aion::loginserver::dao::AccountDAO
