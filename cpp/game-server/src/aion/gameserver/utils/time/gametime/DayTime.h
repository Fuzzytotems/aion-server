#pragma once
// C++ only (upstream sync foundation, docs/deviations/P4-10.md): the generated enum of Java DayTime as of 51d7a19cb, kept by hand after
// upstream 374336b3b removed DayTime with the weather rework. GameTime and the material conditions still use it until wave 4 ports 374336b3b.

#include <array>
#include <cstdint>
#include <string_view>

#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"

namespace aion::gameserver::utils::time::gametime {

/** Java enum com.aionemu.gameserver.utils.time.gametime.DayTime (not bound by JAXB): constants in ordinal order (EnumTraits: names). */
enum class DayTime : uint8_t {
	MORNING,
	AFTERNOON,
	EVENING,
	NIGHT,
};

} // namespace aion::gameserver::utils::time::gametime

namespace aion::gameserver::xml {

template <>
struct EnumTraits<::aion::gameserver::utils::time::gametime::DayTime> {
	static constexpr std::string_view javaName = "DayTime";
	static constexpr std::array<std::string_view, 4> names{
	  "MORNING",
	  "AFTERNOON",
	  "EVENING",
	  "NIGHT",
	};
	static constexpr std::array<EnumEntry<::aion::gameserver::utils::time::gametime::DayTime>, 4> xmlSorted{{
	  {"AFTERNOON", ::aion::gameserver::utils::time::gametime::DayTime::AFTERNOON},
	  {"EVENING", ::aion::gameserver::utils::time::gametime::DayTime::EVENING},
	  {"MORNING", ::aion::gameserver::utils::time::gametime::DayTime::MORNING},
	  {"NIGHT", ::aion::gameserver::utils::time::gametime::DayTime::NIGHT},
	}};
};
static_assert(verifyEnumTraits<::aion::gameserver::utils::time::gametime::DayTime>());

} // namespace aion::gameserver::xml
