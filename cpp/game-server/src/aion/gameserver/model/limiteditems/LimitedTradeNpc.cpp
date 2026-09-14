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
	AION_UNPORTED();
}

LimitedTradeNpc::~LimitedTradeNpc() = default;

} // namespace aion::gameserver::model::limiteditems
