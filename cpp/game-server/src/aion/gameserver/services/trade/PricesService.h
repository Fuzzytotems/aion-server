#pragma once

#include <cstdint>

#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/services/trade/fwd.h"

namespace aion::gameserver::services::trade {

/**
 * Used to get effective prices for the player.<br/>
 * Packets: SM_PRICES, SM_TRADELIST, SM_SELL_ITEM<br/>
 * Services: Teleporter and similar fees
 *
 * @author Sarynth, wakizashi
 */
class PricesService {
public:
	/** Used in SM_PRICES */
	static int32_t getGlobalPrices(model::Race playerRace);
	/** Used in SM_PRICES */
	static int32_t getGlobalPricesModifier();
	/** Used in SM_PRICES */
	static int32_t getTaxes(model::Race playerRace);
private:
	static float getPriceInfluenceRate(model::Race playerRace);
public:
	/** Used in SM_TRADELIST. */
	static int32_t getVendorBuyModifier();
	/** Used in SM_SELL_ITEM */
	static int32_t getVendorSellModifier();
	static int64_t getPriceForService(int64_t basePrice, model::Race playerRace);
	static int64_t getBuyPrice(int64_t requiredKinah, model::Race playerRace);
	static int64_t getSellReward(int64_t kinahValue, int32_t sellModifier);
};

} // namespace aion::gameserver::services::trade
