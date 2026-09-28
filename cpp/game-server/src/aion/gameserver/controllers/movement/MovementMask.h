#pragma once

#include <cstdint>

namespace aion::gameserver::controllers::movement {

/**
 * Movement mask bits of SM_MOVE / CM_MOVE.
 * <p>
 * Static-only constants class (fieldmap K5). The Java byte casts are spelled as int8_t conversions of the same bit patterns.
 *
 * @author Mr. Poke, Neon
 */
class MovementMask final {
public:
	MovementMask() = delete;

	/**
	 * When stopping or instant action (this is zero, so a movement with any other flag has no immediate in it)
	 */
	static constexpr int8_t IMMEDIATE = static_cast<int8_t>(0x00);

	/**
	 * When gliding
	 */
	static constexpr int8_t GLIDE = static_cast<int8_t>(0x04); // 4

	/**
	 * When falling
	 */
	static constexpr int8_t FALL = static_cast<int8_t>(0x08); // 8

	/**
	 * When standing on moving objects like elevators
	 */
	static constexpr int8_t VEHICLE = static_cast<int8_t>(0x10); // 16

	/**
	 * Absolute destination coordinates (e.g. mouse related movement). Movements without this flag use relative directions.
	 */
	static constexpr int8_t ABSOLUTE = static_cast<int8_t>(0x20); // 32

	/**
	 * When changing direction or move state
	 */
	static constexpr int8_t MANUAL = static_cast<int8_t>(0x40); // 64

	/**
	 * When coords change, but not if heading updates
	 */
	static constexpr int8_t POSITION = static_cast<int8_t>(0x80); // 128

	static constexpr int8_t NPC_WALK_SLOW = static_cast<int8_t>(0xEA);
	static constexpr int8_t NPC_WALK_FAST = static_cast<int8_t>(0xE8);
	static constexpr int8_t NPC_RUN_SLOW = static_cast<int8_t>(0xE4);
	static constexpr int8_t NPC_RUN_FAST = static_cast<int8_t>(0xE2);
	static constexpr int8_t NPC_STARTMOVE = static_cast<int8_t>(0xE0);
};

} // namespace aion::gameserver::controllers::movement
