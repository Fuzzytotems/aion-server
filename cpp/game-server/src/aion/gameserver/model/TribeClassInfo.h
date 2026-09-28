#pragma once

#include <string_view>

#include "aion/gameserver/model/TribeClass.h"

namespace aion::gameserver::model {

/**
 * Companion of the generated enum TribeClass (docs/design/static-data.md §2.5): Java's constructor-computed field and methods as free functions
 * found by ADL (`isGuard(tribe)` for Java `tribe.isGuard()`).
 */

/**
 * Java: TribeClass.isGuard(), computed by the constructor as name().toUpperCase().contains("GUARD"). Every constant name is upper-case ASCII, so
 * the name is searched directly (on each call instead of a precomputed table: a constexpr table over 724 names would exceed MSVC's constexpr
 * step limit, and the search is a few bytes).
 */
constexpr bool isGuard(TribeClass tribe) noexcept {
	return xml::enumName(tribe).find("GUARD") != std::string_view::npos;
}

/** Java: TribeClass.isPC() */
constexpr bool isPC(TribeClass tribe) noexcept {
	return tribe == TribeClass::PC || tribe == TribeClass::PC_DARK;
}

} // namespace aion::gameserver::model
