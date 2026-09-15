#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/world/WorldMapType.h"

namespace aion::gameserver::world {

/**
 * Companion of the generated enum WorldMapType (static-data.md §2.5): Java's constructor data (worldId, isPersonal) and static methods as free
 * functions (ADL: `getId(WorldMapType::RESHANTA)`). Names come from the generated EnumTraits.
 */

namespace detail {

struct WorldMapTypeData {
	int32_t worldId;
	bool personal;
};

/** WorldMapType constructor arguments in ordinal order (WorldMapType.java) */
inline constexpr std::array<WorldMapTypeData, 160> WORLD_MAP_TYPE_DATA{{
	{120010000, false}, {120020000, false}, {220010000, false}, {220020000, false}, {220030000, false}, {220040000, false}, {220050000, false},
	{110010000, false}, {110020000, false}, {210010000, false}, {210020000, false}, {210030000, false}, {210040000, false}, {210060000, false},
	{210050000, false}, {220070000, false}, {600010000, false}, {510010000, false}, {520010000, false}, {400010000, false}, {300010000, false},
	{300020000, false}, {300030000, false}, {300040000, false}, {300050000, false}, {300060000, false}, {300070000, false}, {300080000, false},
	{300090000, false}, {300100000, false}, {300110000, false}, {300120000, false}, {300130000, false}, {300140000, false}, {300150000, false},
	{300160000, false}, {300170000, false}, {300190000, false}, {300200000, false}, {300210000, false}, {300220000, false}, {300230000, false},
	{310010000, false}, {310020000, false}, {310030000, false}, {310040000, false}, {310050000, false}, {310060000, false}, {310070000, false},
	{310080000, false}, {320090000, false}, {310090000, false}, {310100000, false}, {310110000, false}, {310120000, false}, {320010000, false},
	{320020000, false}, {320030000, false}, {320040000, false}, {320050000, false}, {320060000, false}, {320070000, false}, {320080000, false},
	{320100000, false}, {320110000, false}, {320120000, false}, {320130000, false}, {320140000, false}, {110070000, false}, {120080000, false},
	{300250000, false}, {300300000, false}, {300320000, false}, {300350000, false}, {300360000, false}, {300420000, false}, {300430000, false},
	{320150000, false}, {900020000, false}, {900030000, false}, {900100000, false}, {900110000, false}, {900120000, false}, {900130000, false},
	{900140000, false}, {900150000, false}, {900170000, false}, {900180000, false}, {900190000, false}, {900200000, false}, {900220000, false},
	{300310000, false}, {300280000, false}, {300240000, false}, {300460000, false}, {300440000, false}, {300450000, false}, {300510000, false},
	{300520000, false}, {300550000, false}, {300560000, false}, {300570000, false}, {300600000, false}, {300700000, false}, {300480000, false},
	{300540000, false}, {300590000, false}, {300800000, false}, {301110000, false}, {301120000, false}, {301130000, false}, {301140000, false},
	{301160000, false}, {301200000, false}, {600080000, false}, {301210000, false}, {301220000, false}, {301230000, false}, {301240000, false},
	{301250000, false}, {301260000, false}, {301270000, false}, {301280000, false}, {301290000, false}, {301300000, false}, {301310000, false},
	{301320000, false}, {301330000, false}, {301340000, false}, {301360000, false}, {301370000, false}, {400020000, false}, {400030000, false},
	{400040000, false}, {400050000, false}, {400060000, false}, {600090000, false}, {600100000, false}, {700010000, false}, {710010000, false},
	{300290000, false}, {301400000, false}, {130090000, false}, {140010000, false}, {220100000, false}, {210070000, false}, {210080000, false},
	{210090000, false}, {220080000, false}, {220090000, false}, {300610000, false}, {300620000, false}, {300630000, false}, {301380000, false},
	{301390000, false}, {301500000, false}, {700020000, true},  {710020000, true},  {720010000, true},  {730010000, true},
}};
static_assert(static_cast<size_t>(WorldMapType::HOUSING_IDDF_PERSONAL) + 1 == WORLD_MAP_TYPE_DATA.size(), "one entry per WorldMapType constant");
static_assert(WORLD_MAP_TYPE_DATA[static_cast<size_t>(WorldMapType::RESHANTA)].worldId == 400010000);
static_assert(WORLD_MAP_TYPE_DATA[static_cast<size_t>(WorldMapType::TRANSIDIUM_ANNEX)].worldId == 400030000);
static_assert(WORLD_MAP_TYPE_DATA[static_cast<size_t>(WorldMapType::ORIEL)].worldId == 700010000);
static_assert(WORLD_MAP_TYPE_DATA[static_cast<size_t>(WorldMapType::PERNON)].worldId == 710010000);
static_assert(WORLD_MAP_TYPE_DATA[static_cast<size_t>(WorldMapType::STONESPEAR_REACH)].worldId == 301500000);

} // namespace detail

/** Java: WorldMapType.getId() */
constexpr int32_t getId(WorldMapType type) noexcept {
	return detail::WORLD_MAP_TYPE_DATA[static_cast<size_t>(type)].worldId;
}

/** Java: WorldMapType.isPersonal() */
constexpr bool isPersonal(WorldMapType type) noexcept {
	return detail::WORLD_MAP_TYPE_DATA[static_cast<size_t>(type)].personal;
}

/** Java: WorldMapType.getWorld(id): the first constant with this id, empty (Java null) if there is none */
constexpr std::optional<WorldMapType> getWorldMapType(int32_t id) noexcept {
	for (size_t ordinal = 0; ordinal < detail::WORLD_MAP_TYPE_DATA.size(); ++ordinal) {
		if (detail::WORLD_MAP_TYPE_DATA[ordinal].worldId == id)
			return static_cast<WorldMapType>(ordinal);
	}
	return std::nullopt;
}

/** Java: WorldMapType.of(worldName): lower-cased name with spaces replaced by '_' compared with the lower-cased constant names */
inline std::optional<WorldMapType> worldMapTypeOf(std::string_view worldName) {
	std::string wanted = commons::utils::StringUtils::toLowerCase(worldName);
	for (char& c : wanted) {
		if (c == ' ')
			c = '_';
	}
	const auto& names = xml::EnumTraits<WorldMapType>::names;
	for (size_t ordinal = 0; ordinal < names.size(); ++ordinal) {
		if (commons::utils::StringUtils::toLowerCase(names[ordinal]) == wanted)
			return static_cast<WorldMapType>(ordinal);
	}
	return std::nullopt;
}

/** Java: WorldMapType.getMapId(worldName): the id of worldMapTypeOf(worldName), 0 if there is none */
inline int32_t getWorldMapTypeMapId(std::string_view worldName) {
	std::optional<WorldMapType> type = worldMapTypeOf(worldName);
	return type ? getId(*type) : 0;
}

/** Java: WorldMapType.isPanesterraMap(id) */
constexpr bool isPanesterraMap(int32_t id) noexcept {
	std::optional<WorldMapType> type = getWorldMapType(id);
	if (!type)
		return false;
	switch (*type) {
		case WorldMapType::BELUS:
		case WorldMapType::TRANSIDIUM_ANNEX:
		case WorldMapType::ASPIDA:
		case WorldMapType::ATANATOS:
		case WorldMapType::DISILLON:
			return true;
		default:
			return false;
	}
}

} // namespace aion::gameserver::world
