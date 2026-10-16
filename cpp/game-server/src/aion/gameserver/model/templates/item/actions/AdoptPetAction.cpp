#include "aion/gameserver/model/templates/item/actions/AdoptPetAction.h"

namespace aion::gameserver::model::templates::item::actions {

// Java AdoptPetAction.java:23-26 (the pet egg is adopted through CM_PET's dialog, not by using the item)
bool AdoptPetAction::canAct(gameobjects::player::Player& /*player*/, runtime::Ptr<gameobjects::Item> /*parentItem*/, runtime::Ptr<gameobjects::Item> /*targetItem*/,
	std::initializer_list<std::any> /*params*/) const {
	return false;
}

// Java AdoptPetAction.java:28-30
void AdoptPetAction::act(gameobjects::player::Player& /*player*/, runtime::Ptr<gameobjects::Item> /*parentItem*/, runtime::Ptr<gameobjects::Item> /*targetItem*/,
	std::initializer_list<std::any> /*params*/) const {
}

} // namespace aion::gameserver::model::templates::item::actions
