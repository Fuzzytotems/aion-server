#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/EmotionType.h"

namespace aion::gameserver::model {

/** Companion of the generated enum EmotionType (docs/design/static-data.md §2.5): Java's constructor data and methods as free functions (ADL). */

namespace detail {
/** Java constructor argument id in ordinal order (comments: the client's ACT_EVENT names) */
inline constexpr std::array<int32_t, 51> EMOTION_TYPE_IDS{{
	-1, // NONE: ACT_EVENT_NONE
	0,  // SELECT_TARGET: ACT_EVENT_MOVE
	1,  // JUMP: ACT_EVENT_JUMP
	2,  // SIT: ACT_EVENT_SIT_DOWN
	3,  // STAND: ACT_EVENT_STAND_UP
	4,  // CHAIR_SIT: ACT_EVENT_SIT_DOWN_ON_CHAIR
	5,  // CHAIR_UP: ACT_EVENT_STAND_UP_FROM_CHAIR
	6,  // START_FLYTELEPORT: ACT_EVENT_PATH_FLY_TAKE_OFF
	7,  // LAND_FLYTELEPORT: ACT_EVENT_PATH_FLY_LAND
	8,  // WINDSTREAM: ACT_EVENT_PATH_FLY_FOLLOW
	9,  // WINDSTREAM_END: ACT_EVENT_PATH_FLY_ESCAPE
	10, // WINDSTREAM_EXIT: ACT_EVENT_PATH_FLY_FALL
	11, // WINDSTREAM_START_BOOST: ACT_EVENT_PATH_FLY_ACC
	12, // WINDSTREAM_END_BOOST: ACT_EVENT_PATH_FLY_DCC
	13, // FLY: ACT_EVENT_TAKE_OFF
	14, // LAND: ACT_EVENT_LAND
	15, // RIDE: ACT_EVENT_MOUNT
	16, // RIDE_END: ACT_EVENT_DISMOUNT
	17, // ATTACK: ACT_EVENT_ATTACK
	18, // DIE: ACT_EVENT_DEATH
	19, // RESURRECT: ACT_EVENT_RESURRECT
	21, // EMOTE: ACT_EVENT_EMOTION
	22, // EMOTE_END: ACT_EVENT_EMOTION_END
	24, // ATTACKMODE_IN_MOVE: ACT_EVENT_GOTO_COMBAT_MODE
	25, // NEUTRALMODE_IN_MOVE: ACT_EVENT_GOTO_PEACE_MODE
	26, // WALK: ACT_EVENT_CHANGE_MOVE_TYPE_WALK
	27, // RUN: ACT_EVENT_CHANGE_MOVE_TYPE_RUN
	31, // OPEN_DOOR: ACT_EVENT_OPEN
	32, // CLOSE_DOOR: ACT_EVENT_CLOSE
	33, // OPEN_PRIVATESHOP: ACT_EVENT_OPEN_PERSONAL_SHOP
	34, // CLOSE_PRIVATESHOP: ACT_EVENT_CLOSE_PERSONAL_SHOP
	35, // CHANGE_SPEED: ACT_EVENT_CHANGE_SPEED
	36, // POWERSHARD_ON: ACT_EVENT_START_WEAPON_BOOST
	37, // POWERSHARD_OFF: ACT_EVENT_END_WEAPON_BOOST
	38, // ATTACKMODE_IN_STANDING: ACT_EVENT_DRAW_WEAPON
	39, // NEUTRALMODE_IN_STANDING: ACT_EVENT_PUTIN_WEAPON
	40, // START_LOOT: ACT_EVENT_LOOT_START
	41, // END_LOOT: ACT_EVENT_LOOT_END
	42, // START_QUESTLOOT: ACT_EVENT_INTERACT_START
	43, // END_QUESTLOOT: ACT_EVENT_INTERACT_END
	44, // TURN_RIGHT: ACT_EVENT_ROTATE_RIGHT
	45, // TURN_LEFT: ACT_EVENT_ROTATE_LEFT
	46, // START_GLIDE: ACT_EVENT_START_GLIDE
	47, // STOP_GLIDE: ACT_EVENT_END_GLIDE
	48, // STOP_FLY: ACT_EVENT_GOTO_NORMAL_GLIDE
	49, // SUMMON_STOP_JUMP: ACT_EVENT_JUMP_END
	50, // START_FEEDING: ACT_EVENT_START_FNCNPET_FEEDING
	51, // END_FEEDING: ACT_EVENT_END_FNCNPET_FEEDING
	52, // WINDSTREAM_STRAFE: ACT_EVENT_PATH_FLY_SHIFT
	53, // START_SPRINT: ACT_EVENT_RIDE_SPRINT
	54, // END_SPRINT: ACT_EVENT_RIDE_SPRINT_END
}};
static_assert(static_cast<size_t>(EmotionType::END_SPRINT) + 1 == EMOTION_TYPE_IDS.size(), "one entry per EmotionType constant");
} // namespace detail

/** Java: EmotionType.getTypeId() */
constexpr int32_t getTypeId(EmotionType type) noexcept {
	return detail::EMOTION_TYPE_IDS[static_cast<size_t>(type)];
}

/** Java: EmotionType.getEmotionTypeById(int) - the first constant with the id, NONE if there is none */
constexpr EmotionType getEmotionTypeById(int32_t id) noexcept {
	for (size_t i = 0; i < detail::EMOTION_TYPE_IDS.size(); ++i) {
		if (detail::EMOTION_TYPE_IDS[i] == id)
			return static_cast<EmotionType>(i);
	}
	return EmotionType::NONE;
}

} // namespace aion::gameserver::model
