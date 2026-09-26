#pragma once

#include "aion/gameserver/model/templates/item/actions/DyeAction.xml.h"

#include <any>
#include <initializer_list>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::templates::item::actions {

/**
 * Java com.aionemu.gameserver.model.templates.item.actions.DyeAction.
 * <p>
 * C++ notes: `params.begin()[0]` of canAct/act is the target house object as `runtime::Ref<gameobjects::HouseObject>`, null for Java null
 * (DyeAction.java:32, :52; CM_USE_ITEM.java:101, :119).
 *
 * @author IceReaper, Neon
 */
class DyeAction : public ::aion::gameserver::model::templates::item::actions::AbstractItemAction {
#include "aion/gameserver/model/templates/item/actions/DyeAction.xml.inc"
public:
	bool canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> targetItem,
		std::initializer_list<std::any> params = {}) const override;

	void act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> targetItem,
		std::initializer_list<std::any> params = {}) const override;
};

} // namespace aion::gameserver::model::templates::item::actions
