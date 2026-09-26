#pragma once

#include "aion/gameserver/model/templates/item/actions/SummonHouseObjectAction.xml.h"

#include <any>
#include <initializer_list>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::templates::item::actions {

/** Java com.aionemu.gameserver.model.templates.item.actions.SummonHouseObjectAction. @author Rolandas */
class SummonHouseObjectAction : public ::aion::gameserver::model::templates::item::actions::AbstractItemAction {
#include "aion/gameserver/model/templates/item/actions/SummonHouseObjectAction.xml.inc"
public:
	bool canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> targetItem,
		std::initializer_list<std::any> params = {}) const override;

	void act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> targetItem,
		std::initializer_list<std::any> params = {}) const override;
};

} // namespace aion::gameserver::model::templates::item::actions
