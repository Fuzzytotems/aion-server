#pragma once

#include "aion/gameserver/network/sequrity/FloodManager_Result.h"

namespace aion::gameserver::network::sequrity {

/**
 * Companion of the generated enum FloodManager_Result (docs/design/static-data.md §2.5): Java's static method Result.max as a free function.
 */

/** Java: Result.max(r1, r2) - the result with the higher ordinal */
constexpr FloodManager_Result max(FloodManager_Result r1, FloodManager_Result r2) noexcept {
	if (static_cast<int>(r1) > static_cast<int>(r2))
		return r1;

	return r2;
}

} // namespace aion::gameserver::network::sequrity
