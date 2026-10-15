#include "aion/gameserver/configs/main/PricesConfig.h"

#include "aion/gameserver/configs/detail/Bind.h"

namespace aion::gameserver::configs::main {

void PricesConfig::bind(commons::configuration::ConfigurableProcessor& p) {
	AION_BIND(p, "gameserver.prices.default.prices", DEFAULT_PRICES, "100");
	AION_BIND(p, "gameserver.prices.default.modifier", DEFAULT_MODIFIER, "100");
	AION_BIND(p, "gameserver.prices.default.taxes", DEFAULT_TAXES, "100");
	AION_BIND(p, "gameserver.prices.vendor.buymod", VENDOR_BUY_MODIFIER, "100");
	AION_BIND(p, "gameserver.prices.vendor.sellmod", VENDOR_SELL_MODIFIER, "20");
}

} // namespace aion::gameserver::configs::main
