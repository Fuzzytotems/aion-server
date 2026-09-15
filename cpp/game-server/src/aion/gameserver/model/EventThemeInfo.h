#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/EventTheme.h"

namespace aion::gameserver::model {

/** Companion of the generated enum EventTheme (docs/design/static-data.md §2.5): Java's constructor data as constexpr free functions (ADL). */

namespace detail {
inline constexpr std::array<int32_t, 9> EVENT_THEME_IDS{{
	0,      // NONE
	1 << 0, // CHRISTMAS: 1
	1 << 1, // HALLOWEEN: 2
	1 << 2, // VALENTINE: 4
	1 << 3, // BRAXCAFE: 8
	1 << 4, // TEST_BASIC_1: 16 (16, 32, 64, 128 are test IDs on map 900020000 Test_Basic)
	1 << 5, // TEST_BASIC_2: 32
	1 << 6, // TEST_BASIC_3: 64
	1 << 7, // TEST_BASIC_4: 128
}};
static_assert(static_cast<size_t>(EventTheme::TEST_BASIC_4) + 1 == EVENT_THEME_IDS.size(), "one entry per EventTheme constant");
} // namespace detail

/** Java: EventTheme.getId() */
constexpr int32_t getId(EventTheme theme) noexcept {
	return detail::EVENT_THEME_IDS[static_cast<size_t>(theme)];
}

} // namespace aion::gameserver::model
