#pragma once

#include <cstdint>

#include "aion/gameserver/utils/fwd.h"

namespace aion::gameserver::utils {

/**
 * C++: a static-only class (fieldmap K5).
 *
 * @author ATracer
 */
class TimeUtil {
public:
	TimeUtil() = delete;

	/** Check whether supplied time in ms is expired */
	static bool isExpired(int64_t time);
};

} // namespace aion::gameserver::utils
