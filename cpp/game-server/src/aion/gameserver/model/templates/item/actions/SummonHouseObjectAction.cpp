#include "aion/gameserver/model/templates/item/actions/SummonHouseObjectAction.h"

namespace aion::gameserver::model::templates::item::actions {

// Java SummonHouseObjectAction.java:24-28
bool SummonHouseObjectAction::canAct(gameobjects::player::Player& /*player*/, runtime::Ptr<gameobjects::Item> /*parentItem*/,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	// TODO Auto-generated method stub
	return false;
}

// Java SummonHouseObjectAction.java:30-34
void SummonHouseObjectAction::act(gameobjects::player::Player& /*player*/, runtime::Ptr<gameobjects::Item> /*parentItem*/,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	// TODO Auto-generated method stub
}

} // namespace aion::gameserver::model::templates::item::actions
