#include "aion/gameserver/services/item/ItemActionService.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::item {

// anonymous ItemUseObserver at ItemActionService.java:26 (fieldmap key ItemActionService$1); local observer; storage: stored in ObserveController
// anonymous Runnable at ItemActionService.java:38 (fieldmap key ItemActionService$2); argument 1 of schedule(); storage: task
void ItemActionService::identifyItem(model::gameobjects::player::Player& player, model::gameobjects::Item& item) {
	AION_UNPORTED();
}

void ItemActionService::applyTuneResult(model::gameobjects::player::Player& player, model::gameobjects::Item& item) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::item
