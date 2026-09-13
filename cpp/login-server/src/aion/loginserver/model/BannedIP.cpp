#include "aion/loginserver/model/BannedIP.h"

#include "aion/loginserver/model/AccountTime.h"
#include "aion/loginserver/model/detail/JavaStringHash.h"

namespace aion::loginserver::model {

bool BannedIP::isActive() const noexcept {
	return !timeEnd || *timeEnd > currentTimestamp();
}

int32_t BannedIP::hashCode() const {
	return detail::javaStringHashCode(mask);
}

} // namespace aion::loginserver::model
