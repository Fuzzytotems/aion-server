#include "aion/gameserver/services/trade/PricesService.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::trade {

int32_t PricesService::getGlobalPrices(model::Race playerRace) {
	AION_UNPORTED();
}

int32_t PricesService::getGlobalPricesModifier() {
	AION_UNPORTED();
}

int32_t PricesService::getTaxes(model::Race playerRace) {
	AION_UNPORTED();
}

float PricesService::getPriceInfluenceRate(model::Race playerRace) {
	AION_UNPORTED();
}

int32_t PricesService::getVendorBuyModifier() {
	AION_UNPORTED();
}

int32_t PricesService::getVendorSellModifier() {
	AION_UNPORTED();
}

int64_t PricesService::getPriceForService(int64_t basePrice, model::Race playerRace) {
	AION_UNPORTED();
}

int64_t PricesService::getBuyPrice(int64_t requiredKinah, model::Race playerRace) {
	AION_UNPORTED();
}

int64_t PricesService::getSellReward(int64_t kinahValue, int32_t sellModifier) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::trade
