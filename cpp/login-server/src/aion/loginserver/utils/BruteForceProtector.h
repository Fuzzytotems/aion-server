#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace aion::loginserver::utils {

/**
 * Counts failed logins per IP address (see CM_LOGIN).
 * <p>
 * Thread safe: the map is guarded by a leaf mutex (Java: an unsynchronized HashMap used by all client packet threads).
 * <p>
 * Java: com.aionemu.loginserver.utils.BruteForceProtector
 *
 * @author Mr. Poke
 */
class BruteForceProtector {
public:
	static BruteForceProtector& getInstance();

	/**
	 * Records a failed login. The first failure (or the first one after Config::WRONG_LOGIN_BAN_TIME minutes since the first failure of a series)
	 * starts a new series with count 1; each further failure increases the count until it reached Config::LOGIN_TRY_BEFORE_BAN.
	 *
	 * @return true if the failure exceeded the allowed number of tries (the series is removed then), so the IP should be banned
	 */
	bool addFailedConnect(std::string_view ip);

private:
	struct FailedLoginInfo {
		int32_t count;
		int64_t time;
	};

	BruteForceProtector() = default;

	std::mutex mutex;
	std::unordered_map<std::string, FailedLoginInfo> failedConnections;
};

} // namespace aion::loginserver::utils
