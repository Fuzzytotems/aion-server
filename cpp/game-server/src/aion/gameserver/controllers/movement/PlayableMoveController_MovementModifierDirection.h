#pragma once
// C++ only (upstream sync foundation, docs/design/upstream-sync-2026-11.md "Frozen enums"): the generated enum of Java PlayableMoveController.MovementModifierDirection as of 51d7a19cb, kept by hand until
// wave 3 ports it (upstream e7a16c553 moved it to the top-level MovementModifierDirection).

#include <array>
#include <cstdint>
#include <string_view>

#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"

namespace aion::gameserver::controllers::movement {

/** Java enum com.aionemu.gameserver.controllers.movement.PlayableMoveController.MovementModifierDirection (not bound by JAXB): constants in ordinal order (EnumTraits: names). */
enum class PlayableMoveController_MovementModifierDirection : uint8_t {
	NONE,
	FORWARD,
	SIDEWAYS,
	BACKWARD,
};

} // namespace aion::gameserver::controllers::movement

namespace aion::gameserver::xml {

template <>
struct EnumTraits<::aion::gameserver::controllers::movement::PlayableMoveController_MovementModifierDirection> {
	static constexpr std::string_view javaName = "MovementModifierDirection";
	static constexpr std::array<std::string_view, 4> names{
	  "NONE",
	  "FORWARD",
	  "SIDEWAYS",
	  "BACKWARD",
	};
	static constexpr std::array<EnumEntry<::aion::gameserver::controllers::movement::PlayableMoveController_MovementModifierDirection>, 4> xmlSorted{{
	  {"BACKWARD", ::aion::gameserver::controllers::movement::PlayableMoveController_MovementModifierDirection::BACKWARD},
	  {"FORWARD", ::aion::gameserver::controllers::movement::PlayableMoveController_MovementModifierDirection::FORWARD},
	  {"NONE", ::aion::gameserver::controllers::movement::PlayableMoveController_MovementModifierDirection::NONE},
	  {"SIDEWAYS", ::aion::gameserver::controllers::movement::PlayableMoveController_MovementModifierDirection::SIDEWAYS},
	}};
};
static_assert(verifyEnumTraits<::aion::gameserver::controllers::movement::PlayableMoveController_MovementModifierDirection>());

} // namespace aion::gameserver::xml
