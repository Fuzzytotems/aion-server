#include "aion/gameserver/dao/AccountPassportsDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
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
	AION_UNPORTED();
}

void AccountPassportsDAO::storePassportList(int32_t accountId, const std::vector<runtime::Ptr<model::account::Passport>>& pList) {
	AION_UNPORTED();
}

void AccountPassportsDAO::storePassport(model::account::Account& account) {
	AION_UNPORTED();
}

void AccountPassportsDAO::addPassports(int32_t accountId, model::account::Passport& passport) {
	AION_UNPORTED();
}

void AccountPassportsDAO::updatePassport(int32_t accountId, model::account::Passport& passport) {
	AION_UNPORTED();
}

void AccountPassportsDAO::deletePassport(int32_t accountId, model::account::Passport& passport) {
	AION_UNPORTED();
}

void AccountPassportsDAO::insertStamps(int32_t accountId) {
	AION_UNPORTED();
}

void AccountPassportsDAO::updateStamps(model::account::Account& account) {
	AION_UNPORTED();
}

void AccountPassportsDAO::resetAllLastStamps() {
	AION_UNPORTED();
}

void AccountPassportsDAO::resetAllStamps() {
	AION_UNPORTED();
}

std::optional<commons::database::Timestamp> AccountPassportsDAO::normTs(std::optional<commons::database::Timestamp> ts) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
