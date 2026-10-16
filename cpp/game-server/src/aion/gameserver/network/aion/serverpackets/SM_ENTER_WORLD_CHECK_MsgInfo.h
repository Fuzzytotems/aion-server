#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/serverpackets/SM_ENTER_WORLD_CHECK_Msg.h"

namespace aion::gameserver::network::aion::serverpackets {

/** Companion of the generated nested enum SM_ENTER_WORLD_CHECK.Msg (docs/design/static-data.md §2.5): Java's constructor data (ADL). */

/** Java: Msg.getId() - OK(0), CHAR_ALREADY_ONLINE(1), CONNECTION_ERROR(2), BOTH_FACTIONS(3), RESERVATION_TIME(4), TOO_MANY_CHARACTERS(5), REENTRY_TIME(6) */
constexpr int8_t getId(SM_ENTER_WORLD_CHECK_Msg msg) noexcept {
	return static_cast<int8_t>(msg);
}

} // namespace aion::gameserver::network::aion::serverpackets
