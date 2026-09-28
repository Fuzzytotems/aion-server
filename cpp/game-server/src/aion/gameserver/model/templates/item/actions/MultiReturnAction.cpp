#include "aion/gameserver/model/templates/item/actions/MultiReturnAction.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::templates::item::actions {

bool MultiReturnAction::canAct(gameobjects::player::Player& /*player*/, runtime::Ptr<gameobjects::Item> /*item*/,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	AION_UNPORTED();
}

void MultiReturnAction::act(gameobjects::player::Player& /*player*/, runtime::Ptr<gameobjects::Item> /*item*/,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::templates::item::actions
