#pragma once

#include <cstdint>
#include <limits>

namespace aion::gameserver::model::templates::detail {

/** Java `(int) value` of a float: NaN becomes 0, out-of-range values saturate (a plain C++ cast is undefined there) */
constexpr int32_t floatToInt(float value) noexcept {
	if (value != value)
		return 0;
	if (value >= 2147483648.0f)
		return std::numeric_limits<int32_t>::max();
	if (value <= -2147483648.0f)
		return std::numeric_limits<int32_t>::min();
	return static_cast<int32_t>(value);
}

} // namespace aion::gameserver::model::templates::detail
