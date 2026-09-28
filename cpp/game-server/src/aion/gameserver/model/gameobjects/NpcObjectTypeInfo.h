#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/gameobjects/NpcObjectType.h"

namespace aion::gameserver::model::gameobjects {

/**
 * Companion of the generated enum NpcObjectType (docs/design/static-data.md §2.5, pattern StorageTypeInfo.h): Java's constructor data and methods as
 * free functions found by ADL (`getId(value)` for Java `value.getId()`).
 *
 * @author ATracer
 */

namespace detail {
/** Java constructor argument `id` in ordinal order */
inline constexpr std::array<int32_t, 9> NPCOBJECTTYPE_IDS{
	1, // NORMAL
	2, // SUMMON
	16, // HOMING
	32, // TRAP
	64, // SKILLAREA
	128, // TOTEM
	256, // GROUPGATE
	1024, // SERVANT
	2048, // PET
};
static_assert(static_cast<size_t>(NpcObjectType::PET) + 1 == NPCOBJECTTYPE_IDS.size(), "one entry per NpcObjectType constant");
} // namespace detail

constexpr int32_t getId(NpcObjectType value) noexcept {
	return detail::NPCOBJECTTYPE_IDS[static_cast<size_t>(value)];
}

} // namespace aion::gameserver::model::gameobjects
