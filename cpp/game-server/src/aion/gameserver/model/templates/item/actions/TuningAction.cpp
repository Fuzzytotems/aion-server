#include "aion/gameserver/model/templates/item/actions/TuningAction.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::templates::item::actions {

bool TuningAction::canAct(gameobjects::player::Player& /*player*/, runtime::Ptr<gameobjects::Item> /*parentItem*/,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	AION_UNPORTED();
}

void TuningAction::act(gameobjects::player::Player& /*player*/, runtime::Ptr<gameobjects::Item> /*parentItem*/,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	AION_UNPORTED();
}

int32_t TuningAction::getRandomStatBonusIdFor(gameobjects::Item& /*item*/) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::templates::item::actions
