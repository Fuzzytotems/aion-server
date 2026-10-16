#include "aion/gameserver/services/trade/PricesService.h"

#include <cstdint>
#include <limits>
#include <string>

#include "aion/gameserver/configs/main/PricesConfig.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/siege/Influence.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/JavaMath.h"

namespace aion::gameserver::services::trade {

namespace {

/** Java: (long) a - NaN 0, saturating */
int64_t javaLongCast(double a) noexcept {
	if (a != a)
		return 0;
	if (a >= 9223372036854775808.0)
		return std::numeric_limits<int64_t>::max();
	if (a <= -9223372036854775808.0)
		return std::numeric_limits<int64_t>::min();
	return static_cast<int64_t>(a);
}

/** Java long multiplication (wraps on overflow) */
int64_t mulLong(int64_t a, int64_t b) noexcept {
	return static_cast<int64_t>(static_cast<uint64_t>(a) * static_cast<uint64_t>(b));
}

} // namespace

int32_t PricesService::getGlobalPrices(model::Race playerRace) {
	int32_t defaultPrices = configs::main::PricesConfig::DEFAULT_PRICES.load();
	float influenceValue = getPriceInfluenceRate(playerRace);
	if (influenceValue == 0.5f) {
		return defaultPrices;
	} else if (influenceValue > 0.5f) {
		float diff = influenceValue - 0.5f;
		return utils::JavaMath::round(defaultPrices - ((diff / 2) * 100));
	} else {
		float diff = 0.5f - influenceValue;
		return utils::JavaMath::round(defaultPrices + ((diff / 2) * 100));
	}
}

int32_t PricesService::getGlobalPricesModifier() {
	return configs::main::PricesConfig::DEFAULT_MODIFIER.load();
}

int32_t PricesService::getTaxes(model::Race playerRace) {
	int32_t defaultTax = configs::main::PricesConfig::DEFAULT_TAXES.load();
	float influenceValue = getPriceInfluenceRate(playerRace);
	if (influenceValue >= 0.5f) {
		return defaultTax;
	}
	float diff = 0.5f - influenceValue;
	return utils::JavaMath::round(defaultTax + ((diff / 4) * 100));
}

float PricesService::getPriceInfluenceRate(model::Race playerRace) {
	switch (playerRace) {
		case model::Race::ASMODIANS:
			return model::siege::Influence::getInstance().getAsmodianInfluenceRate();
		case model::Race::ELYOS:
			return model::siege::Influence::getInstance().getElyosInfluenceRate();
		default:
			break;
	}
	throw runtime::IllegalArgumentException(std::string(xml::enumName(playerRace)) + " is no valid player race.");
}

int32_t PricesService::getVendorBuyModifier() {
	return configs::main::PricesConfig::VENDOR_BUY_MODIFIER.load();
}

int32_t PricesService::getVendorSellModifier() {
	return configs::main::PricesConfig::VENDOR_SELL_MODIFIER.load();
}

int64_t PricesService::getPriceForService(int64_t basePrice, model::Race playerRace) {
	// Tricky. Requires multiplication by Prices, Modifier, Taxes
	// In order, and round down each time to match client calculation.
	// Java: (long) ((long) ((long) (basePrice * prices / 100D) * modifier / 100D) * taxes / 100D) - each product is a long product (wrapping)
	int64_t price = javaLongCast(static_cast<double>(mulLong(basePrice, getGlobalPrices(playerRace))) / 100.0);
	price = javaLongCast(static_cast<double>(mulLong(price, getGlobalPricesModifier())) / 100.0);
	return javaLongCast(static_cast<double>(mulLong(price, getTaxes(playerRace))) / 100.0);
}

int64_t PricesService::getBuyPrice(int64_t requiredKinah, model::Race playerRace) {
	// Requires double precision for 2mil+ kinah items
	int64_t price = javaLongCast(static_cast<double>(mulLong(requiredKinah, getVendorBuyModifier())) / 100.0);
	price = javaLongCast(static_cast<double>(mulLong(price, getGlobalPrices(playerRace))) / 100.0);
	price = javaLongCast(static_cast<double>(mulLong(price, getGlobalPricesModifier())) / 100.0);
	return javaLongCast(static_cast<double>(mulLong(price, getTaxes(playerRace))) / 100.0);
}

int64_t PricesService::getSellReward(int64_t kinahValue, int32_t sellModifier) {
	return javaLongCast(static_cast<double>(mulLong(kinahValue, sellModifier)) / 100.0);
}

} // namespace aion::gameserver::services::trade
