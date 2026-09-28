#pragma once

#include <cstdint>
#include <string_view>

/**
 * Access to the account_login_history table. Thread safe.
 * <p>
 * Java: com.aionemu.loginserver.dao.AccountsLogDAO
 *
 * @author ViAl
 */
namespace aion::loginserver::dao::AccountsLogDAO {

/**
 * Stores a successful game server login. Errors are logged ("Error while inserting account login log.").
 *
 * @param time login time in milliseconds since the epoch
 */
void addRecord(int32_t accountId, int8_t gameserverId, int64_t time, std::string_view ip, std::string_view mac, std::string_view hddSerial);

} // namespace aion::loginserver::dao::AccountsLogDAO
