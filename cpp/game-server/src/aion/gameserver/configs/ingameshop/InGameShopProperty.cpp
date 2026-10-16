#include "aion/gameserver/configs/ingameshop/InGameShopProperty.h"

#include "aion/gameserver/configs/ingameshop/InGameShopProperty.bind.h"
#include "aion/gameserver/dataholders/detail/JaxbDeserialize.h"

namespace aion::gameserver::configs::ingameshop {

const std::vector<model::templates::ingameshop::IGCategory>& InGameShopProperty::getCategories() const {
	return this->categories; // Java: creates the list if it is null (the C++ list always exists)
}

int32_t InGameShopProperty::size() const {
	return static_cast<int32_t>(getCategories().size());
}

void InGameShopProperty::clear() {
	categories.clear();
}

std::unique_ptr<InGameShopProperty> InGameShopProperty::load() {
	return dataholders::detail::deserializeFile<InGameShopProperty>("./config/ingameshop/in_game_shop.xml",
	                                                                "com.aionemu.gameserver.configs.ingameshop.InGameShopProperty");
}

} // namespace aion::gameserver::configs::ingameshop
