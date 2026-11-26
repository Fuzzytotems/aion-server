#pragma once
// C++ only (upstream sync foundation, docs/design/upstream-sync-2026-11.md "Frozen enums"): the generated enum of Java ZoneType as of 51d7a19cb, kept by hand
// (xmlgen.toml [hand_written_enums]) until wave 4 ports the constants upstream 206f9b5db changed.

#include <array>
#include <cstdint>
#include <string_view>

#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"

namespace aion::gameserver::model::templates::zone {

/** Java enum com.aionemu.gameserver.model.templates.zone.ZoneType (not bound by JAXB): constants in ordinal order (EnumTraits: names). */
enum class ZoneType : uint8_t {
	FLY,
	NO_FLY,
	SIEGE,
	PVP,
};

} // namespace aion::gameserver::model::templates::zone

namespace aion::gameserver::xml {

template <>
struct EnumTraits<::aion::gameserver::model::templates::zone::ZoneType> {
	static constexpr std::string_view javaName = "ZoneType";
	static constexpr std::array<std::string_view, 4> names{
	  "FLY",
	  "NO_FLY",
	  "SIEGE",
	  "PVP",
	};
	static constexpr std::array<EnumEntry<::aion::gameserver::model::templates::zone::ZoneType>, 4> xmlSorted{{
	  {"FLY", ::aion::gameserver::model::templates::zone::ZoneType::FLY},
	  {"NO_FLY", ::aion::gameserver::model::templates::zone::ZoneType::NO_FLY},
	  {"PVP", ::aion::gameserver::model::templates::zone::ZoneType::PVP},
	  {"SIEGE", ::aion::gameserver::model::templates::zone::ZoneType::SIEGE},
	}};
};
static_assert(verifyEnumTraits<::aion::gameserver::model::templates::zone::ZoneType>());

} // namespace aion::gameserver::xml
