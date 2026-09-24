#include "aion/gameserver/model/templates/item/actions/EnchantItemAction.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::templates::item::actions {

bool EnchantItemAction::canAct(gameobjects::player::Player& /*player*/, runtime::Ptr<gameobjects::Item> /*parentItem*/,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	AION_UNPORTED();
}

void EnchantItemAction::act(gameobjects::player::Player& /*player*/, runtime::Ptr<gameobjects::Item> /*parentItem*/,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	AION_UNPORTED();
}

void EnchantItemAction::act(gameobjects::player::Player& /*player*/, gameobjects::Item& /*parentItem*/, gameobjects::Item& /*targetItem*/,
	runtime::Ptr<gameobjects::Item> /*supplementItem*/, int32_t /*targetWeapon*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::templates::item::actions
