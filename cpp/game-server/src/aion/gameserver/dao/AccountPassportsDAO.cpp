#include "aion/gameserver/dao/AccountPassportsDAO.h"

#include <chrono>
#include <string_view>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/Passport.h"
#include "aion/gameserver/model/account/PassportsList.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::SQLException;
using model::account::Passport;
using model::gameobjects::Persistable;

namespace {

constexpr std::string_view SELECT_QUERY = "SELECT `passport_id`, `rewarded`, `arrive_date` FROM `account_passports` WHERE `account_id`=?";
constexpr std::string_view UPDATE_QUERY = "UPDATE `account_passports` SET `rewarded`=? WHERE `account_id`=? AND `passport_id`=? AND `arrive_date`=?";
constexpr std::string_view RESET_LAST_STAMPS_QUERY = "UPDATE `account_stamps` SET `last_stamp`=NULL";
constexpr std::string_view RESET_STAMPS_QUERY = "UPDATE `account_stamps` SET `stamps`=0";
constexpr std::string_view INSERT_QUERY = "INSERT INTO `account_passports` (`account_id`, `passport_id`, `rewarded`, `arrive_date`) VALUES (?,?,?,?) ON DUPLICATE KEY UPDATE `rewarded` = GREATEST(`rewarded`, VALUES(`rewarded`))";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `account_passports` WHERE account_id = ? AND passport_id = ? and arrive_date = ?";
constexpr std::string_view INSERT_STAMPS_QUERY = "INSERT INTO `account_stamps` (`account_id`, `stamps`, `last_stamp`) VALUES (?,?,?)";
constexpr std::string_view UPDATE_STAMPS_QUERY = "UPDATE `account_stamps` SET `stamps`= ?, `last_stamp`  = ? WHERE `account_id` = ?";
constexpr std::string_view SELECT_STAMPS_QUERY = "SELECT `stamps`, `last_stamp` FROM `account_stamps` WHERE `account_id`=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.AccountPassportsDAO");

void AccountPassportsDAO::loadPassport(model::account::Account& account) {
	runtime::Ref<model::account::PassportsList> passportList = model::account::PassportsList::create();
	try {
		auto con = DatabaseFactory::getConnection();
		{
			auto stmt = con->prepareStatement(SELECT_QUERY);
			stmt->setInt(1, account.getId());
			auto rset = stmt->executeQuery();
			while (rset->next()) {
				int32_t passport_id = rset->getInt("passport_id");
				bool rewarded = rset->getInt("rewarded") != 0;
				std::optional<commons::database::Timestamp> arriveDate = normTs(rset->getTimestamp("arrive_date"));
				runtime::Ref<Passport> pp = Passport::create(passport_id, rewarded, arriveDate);
				pp->setPersistentState(Persistable::PersistentState::UPDATED);
				passportList->addPassport(*pp);
			}
			account.setPassportsList(passportList);
		}
		{
			auto stmt = con->prepareStatement(SELECT_STAMPS_QUERY);
			stmt->setInt(1, account.getId());
			auto rset = stmt->executeQuery();
			int32_t stamps = 0;
			std::optional<commons::database::Timestamp> lastStamp;
			if (rset->next()) {
				stamps = rset->getInt("stamps");
				lastStamp = rset->getTimestamp("last_stamp");
			} else {
				insertStamps(account.getId());
			}
			account.setPassportStamps(stamps);
			account.setLastStamp(lastStamp);
		}
	} catch (const std::exception& e) {
		log.error("Could not restore completed passport data for account: {} from DB.", account.getId(), e);
	}
}

void AccountPassportsDAO::storePassportList(int32_t accountId, const std::vector<runtime::Ptr<model::account::Passport>>& pList) {
	for (const runtime::Ptr<Passport>& passport : pList) {
		switch (passport->getPersistentState()) {
			case Persistable::PersistentState::NEW:
				addPassports(accountId, *passport);
				break;
			case Persistable::PersistentState::UPDATE_REQUIRED:
				updatePassport(accountId, *passport);
				break;
			case Persistable::PersistentState::DELETED:
				deletePassport(accountId, *passport);
				break;
			default:
				break;
		}
		passport->setPersistentState(Persistable::PersistentState::UPDATED);
	}
}

void AccountPassportsDAO::storePassport(model::account::Account& account) {
	storePassportList(account.getId(), account.getPassportsList()->getAllPassports().snapshot());
	updateStamps(account);
}

void AccountPassportsDAO::addPassports(int32_t accountId, model::account::Passport& passport) {
	try {
		auto conn = DatabaseFactory::getConnection();
		auto ps = conn->prepareStatement(INSERT_QUERY);
		ps->setInt(1, accountId);
		ps->setInt(2, passport.getId());
		ps->setInt(3, passport.isRewarded() ? 1 : 0);
		ps->setTimestamp(4, passport.getArriveDate());
		ps->executeUpdate();
	} catch (const SQLException& e) {
		log.error("Error while adding passports for account {}.", accountId, e);
	}
}

void AccountPassportsDAO::updatePassport(int32_t accountId, model::account::Passport& passport) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto ps = con->prepareStatement(UPDATE_QUERY);
		ps->setInt(1, passport.isRewarded() ? 1 : 0);
		ps->setInt(2, accountId);
		ps->setInt(3, passport.getId());
		ps->setTimestamp(4, passport.getArriveDate());
		ps->executeUpdate();
	} catch (const SQLException& e) {
		log.error("Failed to update existing passports for account {}.", accountId, e);
	}
}

void AccountPassportsDAO::deletePassport(int32_t accountId, model::account::Passport& passport) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto ps = con->prepareStatement(DELETE_QUERY);
		ps->setInt(1, accountId);
		ps->setInt(2, passport.getId());
		ps->setTimestamp(3, passport.getArriveDate());
		ps->executeUpdate();
	} catch (const SQLException& e) {
		log.error("Failed to delete passports for account {}.", accountId, e);
	}
}

void AccountPassportsDAO::insertStamps(int32_t accountId) {
	try {
		auto conn = DatabaseFactory::getConnection();
		auto ps = conn->prepareStatement(INSERT_STAMPS_QUERY);
		ps->setInt(1, accountId);
		ps->setInt(2, 0);
		ps->setTimestamp(3, std::optional<commons::database::Timestamp>());
		ps->executeUpdate();
	} catch (const SQLException& e) {
		log.error("Error while adding stamps for account {}.", accountId, e);
	}
}

void AccountPassportsDAO::updateStamps(model::account::Account& account) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto ps = con->prepareStatement(UPDATE_STAMPS_QUERY);
		ps->setInt(1, account.getPassportStamps());
		ps->setTimestamp(2, account.getLastStamp());
		ps->setInt(3, account.getId());
		ps->executeUpdate();
	} catch (const SQLException& e) {
		log.error("Failed to update existing passports for account {}.", account.getId(), e);
	}
}

void AccountPassportsDAO::resetAllLastStamps() {
	try {
		auto con = DatabaseFactory::getConnection();
		auto ps = con->prepareStatement(RESET_LAST_STAMPS_QUERY);
		ps->executeUpdate();
	} catch (const SQLException& e) {
		log.error("Failed to reset all last stamps.", e);
	}
}

void AccountPassportsDAO::resetAllStamps() {
	try {
		auto con = DatabaseFactory::getConnection();
		auto ps = con->prepareStatement(RESET_STAMPS_QUERY);
		ps->executeUpdate();
	} catch (const SQLException& e) {
		log.error("Failed to reset all stamps.", e);
	}
}

std::optional<commons::database::Timestamp> AccountPassportsDAO::normTs(std::optional<commons::database::Timestamp> ts) {
	// Java: Timestamp.from(ts.toInstant().truncatedTo(ChronoUnit.SECONDS)) (Instant.truncatedTo floors)
	if (!ts)
		return std::nullopt;
	return std::chrono::floor<std::chrono::seconds>(*ts);
}

} // namespace aion::gameserver::dao
