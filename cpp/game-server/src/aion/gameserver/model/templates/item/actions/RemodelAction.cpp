#include "aion/gameserver/model/templates/item/actions/RemodelAction.h"

namespace aion::gameserver::model::templates::item::actions {

bool RemodelAction::canAct(gameobjects::player::Player& /*player*/, runtime::Ptr<gameobjects::Item> /*parentItem*/,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	return false;
}

void RemodelAction::act(gameobjects::player::Player& /*player*/, runtime::Ptr<gameobjects::Item> /*parentItem*/,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
}

} // namespace aion::gameserver::model::templates::item::actions
