#include "aion/loginserver/utils/BruteForceProtector.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/loginserver/configs/Config.h"

namespace aion::loginserver::utils {

BruteForceProtector& BruteForceProtector::getInstance() {
	static auto* instance = new BruteForceProtector(); // leaked: used by packet threads until the process ends
	return *instance;
}

bool BruteForceProtector::addFailedConnect(std::string_view ip) {
	using configs::Config;
	const int64_t now = commons::utils::currentTimeMillis();
	const int64_t windowMillis = Config::WRONG_LOGIN_BAN_TIME * 60'000LL;
	std::lock_guard lock(mutex);
	auto failed = failedConnections.find(std::string(ip));
	if (failed == failedConnections.end() || now - failed->second.time > windowMillis) {
		if (failedConnections.size() > PURGE_THRESHOLD) // the map only grows: drop the entries that can no longer be banned
			std::erase_if(failedConnections, [&](const auto& entry) { return now - entry.second.time > windowMillis; });
		failedConnections.insert_or_assign(std::string(ip), FailedLoginInfo{1, now});
	} else {
		if (failed->second.count >= Config::LOGIN_TRY_BEFORE_BAN) {
			failedConnections.erase(failed);
			return true;
		} else
			failed->second.count++;
	}
	return false;
}

} // namespace aion::loginserver::utils
