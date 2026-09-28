#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "aion/gameserver/model/templates/housing/PartType.h"

namespace aion::gameserver::model::templates::housing {

/**
 * Companion of the generated enum PartType (docs/design/static-data.md §2.5, pattern StorageTypeInfo.h): Java's constructor data and methods as
 * free functions found by ADL (`getRooms(type)` for Java `type.getRooms()`). The static `PartType.getForLineNr(lineNr)` returns
 * `std::nullopt` where Java returns null.
 *
 * @author Rolandas
 */

namespace detail {
/** Java constructor arguments (packetLineStart, packetLineEnd) in ordinal order */
struct PartTypeData {
	int32_t lineNrStart;
	int32_t lineNrEnd;
};

inline constexpr std::array<PartTypeData, 9> PART_TYPE_DATA{{
	{1, 1},   // ROOF
	{2, 2},   // OUTWALL
	{3, 3},   // FRAME
	{4, 4},   // DOOR
	{5, 5},   // GARDEN
	{6, 6},   // FENCE (7 is unused)
	{8, 13},  // INWALL_ANY
	{14, 19}, // INFLOOR_ANY (20 - 26 is unknown, sometimes sent in CM_HOUSE_DECORATE (lineNo))
	{27, 27}, // ADDON (28 - 29 is unknown, sometimes sent in CM_HOUSE_DECORATE (lineNo))
}};
static_assert(static_cast<size_t>(PartType::ADDON) + 1 == PART_TYPE_DATA.size(), "one entry per PartType constant");
} // namespace detail

constexpr int32_t getRooms(PartType type) noexcept {
	const detail::PartTypeData& data = detail::PART_TYPE_DATA[static_cast<size_t>(type)];
	return data.lineNrEnd - data.lineNrStart + 1;
}

constexpr int32_t getStartLineNr(PartType type) noexcept {
	return detail::PART_TYPE_DATA[static_cast<size_t>(type)].lineNrStart;
}

constexpr int32_t getEndLineNr(PartType type) noexcept {
	return detail::PART_TYPE_DATA[static_cast<size_t>(type)].lineNrEnd;
}

/** Java static PartType.getForLineNr(lineNr): the first type whose line range contains lineNr, null (nullopt) if none */
constexpr std::optional<PartType> getForLineNr(int32_t lineNr) noexcept {
	for (size_t ordinal = 0; ordinal < detail::PART_TYPE_DATA.size(); ++ordinal) {
		const auto type = static_cast<PartType>(ordinal);
		if (getStartLineNr(type) <= lineNr && getEndLineNr(type) >= lineNr)
			return type;
	}
	return std::nullopt;
}

} // namespace aion::gameserver::model::templates::housing
