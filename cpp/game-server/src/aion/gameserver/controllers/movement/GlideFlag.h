#pragma once

#include <cstdint>

namespace aion::gameserver::controllers::movement {

/**
 * Glide flags of CM_MOVE (upwinds and geysers).
 * <p>
 * Static-only constants class (fieldmap K5).
 *
 * @author Neon
 */
class GlideFlag final {
public:
	GlideFlag() = delete;

	static constexpr int8_t NONE = static_cast<int8_t>(0x00);
	static constexpr int8_t WEAK_UPWIND = static_cast<int8_t>(0x10);
	static constexpr int8_t MEDIUM_UPWIND = static_cast<int8_t>(0x20);
	static constexpr int8_t STRONG_UPWIND = WEAK_UPWIND + MEDIUM_UPWIND; // 0x30
	static constexpr int8_t GEYSER = static_cast<int8_t>(0x80);
};

} // namespace aion::gameserver::controllers::movement
