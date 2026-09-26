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
	// Java: Config.WRONG_LOGIN_BAN_TIME * 1000 * 60 is an int multiplication, which wraps
	const int32_t banTimeMillis = static_cast<int32_t>(static_cast<uint32_t>(Config::WRONG_LOGIN_BAN_TIME) * 1000u * 60u);
	const int64_t now = commons::utils::currentTimeMillis();
	std::lock_guard lock(mutex);
	auto failed = failedConnections.find(std::string(ip));
	if (failed == failedConnections.end() || now - failed->second.time > banTimeMillis) {
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
