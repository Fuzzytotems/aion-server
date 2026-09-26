#include "aion/gameserver/model/templates/item/actions/TamperingAction.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::templates::item::actions {

bool TamperingAction::canAct(gameobjects::player::Player& /*player*/, runtime::Ptr<gameobjects::Item> /*parentItem*/,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	AION_UNPORTED();
}

void TamperingAction::act(gameobjects::player::Player& /*player*/, runtime::Ptr<gameobjects::Item> /*parentItem*/,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::templates::item::actions
