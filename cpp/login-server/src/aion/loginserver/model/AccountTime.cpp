#include "aion/loginserver/model/AccountTime.h"

#include <chrono>

#include "aion/commons/utils/TimeUtils.h"

namespace aion::loginserver::model {

Timestamp currentTimestamp() noexcept {
	return Timestamp(std::chrono::milliseconds(commons::utils::currentTimeMillis()));
}

AccountTime::AccountTime() : lastLoginTime(currentTimestamp()) {
}

} // namespace aion::loginserver::model
