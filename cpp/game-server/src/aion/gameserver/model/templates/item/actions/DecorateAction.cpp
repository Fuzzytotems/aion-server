#include "aion/gameserver/model/templates/item/actions/DecorateAction.h"

namespace aion::gameserver::model::templates::item::actions {

// Java DecorateAction.java:17-21 (an auto-generated stub in Java)
bool DecorateAction::canAct(gameobjects::player::Player& /*player*/, runtime::Ptr<gameobjects::Item> /*parentItem*/, runtime::Ptr<gameobjects::Item> /*targetItem*/,
	std::initializer_list<std::any> /*params*/) const {
	return false;
}

// Java DecorateAction.java:23-26
void DecorateAction::act(gameobjects::player::Player& /*player*/, runtime::Ptr<gameobjects::Item> /*parentItem*/, runtime::Ptr<gameobjects::Item> /*targetItem*/,
	std::initializer_list<std::any> /*params*/) const {
}

// Java DecorateAction.java:28-32
int32_t DecorateAction::getTemplateId() const {
	if (!partId) // Addons missing in client
		return 0;
	return *partId;
}

} // namespace aion::gameserver::model::templates::item::actions
