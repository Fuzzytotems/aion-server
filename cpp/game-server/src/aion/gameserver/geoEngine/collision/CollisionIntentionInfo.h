#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>

#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/geoEngine/collision/CollisionIntention.h"

namespace aion::gameserver::geoEngine::collision {

/**
 * Companion of the generated enum CollisionIntention (docs/design/static-data.md §2.5): Java's constructor data and static methods as free
 * functions (ADL). The ids are Java bytes: PHYSICAL_SEE_THROUGH is (byte) 128 = -128, DEFAULT_COLLISIONS is 1 | 16 | -128 = -111 and ALL is -1,
 * because Java combines the sign-extended byte ids with int arithmetic before narrowing them in the constructor.
 *
 * @author Rolandas
 */

namespace detail {
inline constexpr std::array<int8_t, 12> COLLISION_INTENTION_IDS{{
	0,                                // NONE
	1 << 0,                           // PHYSICAL: physical collision
	1 << 1,                           // MATERIAL: mesh materials with skills
	1 << 2,                           // SKILL: skill obstacles
	1 << 3,                           // WALK: walk/nowalk obstacles
	1 << 4,                           // DOOR: doors which have a state opened/closed
	1 << 5,                           // EVENT: appear on event only
	1 << 6,                           // MOVEABLE: ships, shugo boxes
	static_cast<int8_t>(1 << 7),      // PHYSICAL_SEE_THROUGH: (byte) 128
	static_cast<int8_t>(1 | 16 | -128), // DEFAULT_COLLISIONS: PHYSICAL | DOOR | PHYSICAL_SEE_THROUGH
	static_cast<int8_t>(1 | 16),      // CANT_SEE_COLLISIONS: PHYSICAL | DOOR
	static_cast<int8_t>(1 | 2 | 4 | 8 | 16 | 32 | 64 | -128), // ALL: nodes allow to enumerate their child geometries
}};
static_assert(static_cast<size_t>(CollisionIntention::ALL) + 1 == COLLISION_INTENTION_IDS.size(), "one entry per CollisionIntention constant");
} // namespace detail

/** Java: CollisionIntention.getId() */
constexpr int8_t getId(CollisionIntention intention) noexcept {
	return detail::COLLISION_INTENTION_IDS[static_cast<size_t>(intention)];
}

/** Java: CollisionIntention.getFlagsFromValue(int): the constants whose (sign-extended) id bits are all set in value, without NONE and ALL. */
inline std::set<CollisionIntention> getFlagsFromValue(int32_t value) {
	std::set<CollisionIntention> result;
	for (size_t ordinal = 0; ordinal < detail::COLLISION_INTENTION_IDS.size(); ++ordinal) {
		auto intention = static_cast<CollisionIntention>(ordinal);
		int32_t id = getId(intention);
		if ((value & id) == id) {
			if (intention == CollisionIntention::NONE || intention == CollisionIntention::ALL)
				continue;
			result.insert(intention);
		}
	}
	return result;
}

/** Java: CollisionIntention.toString(int): the names of the matching constants (without NONE and ALL) joined by ", ". */
inline std::string collisionIntentionToString(int32_t value) {
	std::string str;
	for (size_t ordinal = 0; ordinal < detail::COLLISION_INTENTION_IDS.size(); ++ordinal) {
		auto intention = static_cast<CollisionIntention>(ordinal);
		if (intention == CollisionIntention::NONE || intention == CollisionIntention::ALL)
			continue;
		int32_t id = getId(intention);
		if ((value & id) == id) {
			str += xml::enumName(intention);
			str += ", ";
		}
	}
	if (!str.empty())
		str.resize(str.size() - 2);
	return str;
}

} // namespace aion::gameserver::geoEngine::collision
