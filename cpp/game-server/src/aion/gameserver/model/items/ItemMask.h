#pragma once

#include <cstdint>

namespace aion::gameserver::model::items {

/**
 * Bits of the item template mask (ItemTemplate.getMask()).
 * <p>
 * C++: a static-only class; the literal shifts are constant expressions (hub-headers.md §11.1).
 *
 * added by Blackhive original credits to xTr 2.0.0.5 mod by Tomate
 */
class ItemMask {
public:
	static constexpr int32_t LIMIT_ONE = 1;
	static constexpr int32_t TRADEABLE = (1 << 1);
	static constexpr int32_t SELLABLE = (1 << 2);
	static constexpr int32_t STORABLE_IN_WH = (1 << 3);
	static constexpr int32_t STORABLE_IN_AWH = (1 << 4);
	static constexpr int32_t STORABLE_IN_LWH = (1 << 5);
	static constexpr int32_t BREAKABLE = (1 << 6);
	static constexpr int32_t SOUL_BOUND = (1 << 7);
	static constexpr int32_t REMOVE_LOGOUT = (1 << 8);
	static constexpr int32_t NO_ENCHANT = (1 << 9);
	static constexpr int32_t CAN_PROC_ENCHANT = (1 << 10);
	static constexpr int32_t CAN_COMPOSITE_WEAPON = (1 << 11);
	static constexpr int32_t REMODELABLE = (1 << 12);
	static constexpr int32_t CAN_SPLIT = (1 << 13);
	static constexpr int32_t DELETABLE = (1 << 14);
	static constexpr int32_t DYEABLE = (1 << 15);
	static constexpr int32_t CAN_AP_EXTRACT = (1 << 16); // not sure
	static constexpr int32_t CAN_POLISH = (1 << 17);     // not sure
	static constexpr int32_t LEGION_TRADEABLE = (1 << 18);

	ItemMask() = delete;
};

} // namespace aion::gameserver::model::items
