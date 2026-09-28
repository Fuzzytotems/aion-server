#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "aion/gameserver/configs/ingameshop/InGameShopProperty.xml.h"

namespace aion::gameserver::configs::ingameshop {

/**
 * Java com.aionemu.gameserver.configs.ingameshop.InGameShopProperty.
 * <p>
 * C++: an absent category list is the empty bound vector (Java creates it lazily). load binds ./config/ingameshop/in_game_shop.xml like
 * JAXBUtil.deserialize.
 *
 * @author xTz
 */
class InGameShopProperty : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/configs/ingameshop/InGameShopProperty.xml.inc"
public:
	const std::vector<model::templates::ingameshop::IGCategory>& getCategories() const;

	int32_t size() const;

	void clear();

	/** @throws commons::utils::Exception if the file cannot be read or bound */
	static std::unique_ptr<InGameShopProperty> load();
};

} // namespace aion::gameserver::configs::ingameshop
