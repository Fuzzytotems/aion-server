#pragma once

#include "aion/gameserver/model/templates/item/actions/ChargeAction.xml.h"

#include <any>
#include <initializer_list>
#include <vector>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::templates::item::actions {

/** Java com.aionemu.gameserver.model.templates.item.actions.ChargeAction. @author ATracer */
class ChargeAction : public ::aion::gameserver::model::templates::item::actions::AbstractItemAction {
#include "aion/gameserver/model/templates/item/actions/ChargeAction.xml.inc"
public:
	bool canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> targetItem,
		std::initializer_list<std::any> params = {}) const override;

	void act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> targetItem,
		std::initializer_list<std::any> params = {}) const override;

private:
	/** Java getConditioningItems: the items to condition (just targetItem if one was selected), sending the "not chargeable" message if there are none */
	std::vector<runtime::Ptr<gameobjects::Item>> getConditioningItems(gameobjects::player::Player& player, gameobjects::Item& parentItem, runtime::Ptr<gameobjects::Item> targetItem) const;
	void finishUse(gameobjects::player::Player& player, gameobjects::Item& parentItem, runtime::Ptr<gameobjects::Item> targetItem) const;
};

} // namespace aion::gameserver::model::templates::item::actions
