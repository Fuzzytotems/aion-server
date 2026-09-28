#include "aion/gameserver/model/templates/item/actions/MegaphoneAction.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::templates::item::actions {

bool MegaphoneAction::canAct(gameobjects::player::Player& /*player*/, runtime::Ptr<gameobjects::Item> /*item*/,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	AION_UNPORTED();
}

void MegaphoneAction::act(gameobjects::player::Player& /*player*/, runtime::Ptr<gameobjects::Item> /*item*/,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::templates::item::actions
