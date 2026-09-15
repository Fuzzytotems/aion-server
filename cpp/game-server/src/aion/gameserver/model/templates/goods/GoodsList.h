#pragma once

#include <vector>

#include "aion/gameserver/model/limiteditems/fwd.h"
#include "aion/gameserver/model/templates/goods/GoodsList.xml.h"

#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::templates::goods {

/** Java com.aionemu.gameserver.model.templates.goods.GoodsList. @author ATracer */
class GoodsList : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/goods/GoodsList.xml.inc"
public:
	/** @return new LimitedItem objects for the items with both a sell and a buy limit (Java builds a new list per call) */
	std::vector<runtime::Ref<limiteditems::LimitedItem>> getLimitedItems() const;
};

} // namespace aion::gameserver::model::templates::goods
