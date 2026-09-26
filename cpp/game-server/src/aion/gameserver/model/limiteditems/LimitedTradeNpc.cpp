#include "aion/gameserver/model/limiteditems/LimitedTradeNpc.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/limiteditems/LimitedItem.h"

namespace aion::gameserver::model::limiteditems {

LimitedTradeNpc::LimitedTradeNpc() {
}

runtime::Ref<LimitedTradeNpc> LimitedTradeNpc::create() {
	return runtime::makeRef<LimitedTradeNpc>();
}

void LimitedTradeNpc::addLimitedItems(const std::vector<runtime::Ptr<LimitedItem>>& value) {
	std::vector<runtime::Ref<LimitedItem>> items;
	items.reserve(value.size());
	for (const runtime::Ptr<LimitedItem>& item : value)
		items.emplace_back(item);
	this->limitedItems.addAll(items);
}

LimitedTradeNpc::~LimitedTradeNpc() = default;

} // namespace aion::gameserver::model::limiteditems
