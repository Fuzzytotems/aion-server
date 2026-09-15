#include "aion/gameserver/utils/TimeUtil.h"

#include "aion/commons/utils/TimeUtils.h"

namespace aion::gameserver::utils {

bool TimeUtil::isExpired(int64_t time) {
	return time < commons::utils::currentTimeMillis();
}

} // namespace aion::gameserver::utils
