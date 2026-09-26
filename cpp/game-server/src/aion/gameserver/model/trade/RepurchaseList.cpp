#include "aion/gameserver/model/trade/RepurchaseList.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/services/RepurchaseService.h"

namespace aion::gameserver::model::trade {

RepurchaseList::RepurchaseList(int32_t sellerObjIdValue) : sellerObjId(sellerObjIdValue) {
}

RepurchaseList::~RepurchaseList() = default;

runtime::Ref<RepurchaseList> RepurchaseList::create(int32_t sellerObjIdValue) {
	return runtime::makeRef<RepurchaseList>(sellerObjIdValue);
}

void RepurchaseList::addRepurchaseItem(gameobjects::player::Player& player, int32_t itemObjectId, int64_t /*count*/) {
	if (services::RepurchaseService::getInstance().canRepurchase(player, itemObjectId))
		repurchases.add(itemObjectId);
}

int32_t RepurchaseList::size() {
	return repurchases.size();
}

} // namespace aion::gameserver::model::trade
