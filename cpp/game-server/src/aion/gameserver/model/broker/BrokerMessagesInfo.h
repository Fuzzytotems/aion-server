#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "aion/gameserver/model/broker/BrokerMessages.h"

namespace aion::gameserver::model::broker {

/**
 * Companion of the generated enum BrokerMessages (docs/design/static-data.md §2.5): Java's constructor data as a free function found by ADL
 * (`getId(message)` for Java `message.getId()`). Pure data.
 *
 * @author kosyachok
 */

namespace detail {
inline constexpr std::array<int32_t, 3> BROKER_MESSAGE_IDS{{
	2, // CANT_REGISTER_ITEM
	3, // NO_SPACE_AVAIABLE
	5, // NO_ENOUGHT_KINAH
}};
static_assert(static_cast<size_t>(BrokerMessages::NO_ENOUGHT_KINAH) + 1 == BROKER_MESSAGE_IDS.size(), "one entry per BrokerMessages constant");
} // namespace detail

constexpr int32_t getId(BrokerMessages message) noexcept {
	return detail::BROKER_MESSAGE_IDS[static_cast<size_t>(message)];
}

} // namespace aion::gameserver::model::broker
