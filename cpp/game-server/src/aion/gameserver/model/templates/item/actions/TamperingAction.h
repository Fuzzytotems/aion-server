#pragma once

#include "aion/gameserver/model/templates/item/actions/TamperingAction.xml.h"

#include <any>
#include <initializer_list>
#include <cstdint>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::templates::item::actions {

/** Java com.aionemu.gameserver.model.templates.item.actions.TamperingAction. @author Rolandas */
class TamperingAction : public ::aion::gameserver::model::templates::item::actions::AbstractItemAction {
#include "aion/gameserver/model/templates/item/actions/TamperingAction.xml.inc"
public:
	bool canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> targetItem,
		std::initializer_list<std::any> params = {}) const override;

	void act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> targetItem,
		std::initializer_list<std::any> params = {}) const override;

	/** Java public static setTemperingLevel (also called by //equip) */
	static void setTemperingLevel(gameobjects::Item& item, gameobjects::player::Player& player, int32_t temperingLevel);
};

} // namespace aion::gameserver::model::templates::item::actions
