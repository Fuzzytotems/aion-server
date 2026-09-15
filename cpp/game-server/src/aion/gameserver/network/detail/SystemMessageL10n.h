#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/utils/stats/AbyssRankEnum.h"

namespace aion::gameserver::network::detail {

/**
 * C++ only (P4-15, read by the P4-06 SM_SYSTEM_MESSAGE body): the enum constructor data and the ChatUtil helper that SM_SYSTEM_MESSAGE and its
 * hand-written factories need, standing in for their companions (ChatType, Race: P4-05; AbyssRankEnum: P5-01) and utils/ChatUtil (P4-05).
 * Pure data and code copied from the Java sources. TODO(P4-05, P5-01): use the companions and ChatUtil::l10n once they are merged.
 */

/** Java: ChatType.getId() (ChatType.java constructor arguments in ordinal order) */
constexpr int8_t chatTypeIdOf(model::ChatType chatType) noexcept {
	static constexpr std::array<int8_t, 29> IDS{
		0, 1, 3, 4, 5, 6, 7, 8, 9, 10, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 27, 31, 32, 33, 34, 35, 36};
	return IDS[static_cast<size_t>(chatType)];
}

/**
 * Java: ChatUtil.l10n(l10nId) - "$" followed by the id * 2 + 1 as two UTF-16 chars (low and high word); null for 0 (C++: "", which writeS
 * writes with the same bytes). Lone surrogates in the two chars become U+FFFD in the UTF-8 string (commons StringUtils::toUtf8).
 */
inline std::string l10n(int32_t l10nId) {
	if (l10nId == 0)
		return std::string(); // Java: null (written as a null string)
	const uint32_t id = static_cast<uint32_t>(l10nId) << 1 | 1u; // client wants the rightmost bit = 1, followed by the id (effectively = l10nId * 2 + 1)
	const std::u16string idAsFourBytesString{static_cast<char16_t>(id & 0xFFFF), static_cast<char16_t>((id >> 16) & 0xFFFF)};
	return "$" + commons::utils::StringUtils::toUtf8(idAsFourBytesString);
}

/** Java: Race.getL10nId() (Race.java: ELYOS 900240, ASMODIANS 900241, 0 for the races without an l10n id) */
constexpr int32_t raceL10nIdOf(model::Race race) noexcept {
	return race == model::Race::ELYOS ? 900240 : race == model::Race::ASMODIANS ? 900241 : 0;
}

/** Java: AbyssRankEnum.getRankL10n(race) without the l10n call: (race == ELYOS ? 901215 : 901233) + ordinal() */
constexpr int32_t rankL10nIdOf(model::Race race, utils::stats::AbyssRankEnum rank) noexcept {
	const int32_t rank9L10nId = race == model::Race::ELYOS ? 901215 : 901233;
	return rank9L10nId + static_cast<int32_t>(rank);
}

/** Java (SM_SYSTEM_MESSAGE): "%SubZone:" + position.getMapId() + " " + position.getX() + " " + position.getY() + " " + position.getZ() */
inline std::string subZoneOf(int32_t mapId, float x, float y, float z) {
	return "%SubZone:" + std::to_string(mapId) + " " + geoEngine::math::JavaFloat::toString(x) + " " + geoEngine::math::JavaFloat::toString(y) + " " +
		geoEngine::math::JavaFloat::toString(z);
}

} // namespace aion::gameserver::network::detail
