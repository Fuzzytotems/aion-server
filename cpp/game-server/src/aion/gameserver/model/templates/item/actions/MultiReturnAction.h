#pragma once

#include "aion/gameserver/model/templates/item/actions/MultiReturnAction.xml.h"

#include <any>
#include <initializer_list>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::templates::item::actions {

/**
 * Java com.aionemu.gameserver.model.templates.item.actions.MultiReturnAction.
 * <p>
 * C++ notes: `params.begin()[0]` of act is the `int32_t` return index (MultiReturnAction.java:38; CM_USE_ITEM.java:104, :120 pass it to
 * canAct and act).
 *
 * @author ginho1
 */
class MultiReturnAction : public ::aion::gameserver::model::templates::item::actions::AbstractItemAction {
#include "aion/gameserver/model/templates/item/actions/MultiReturnAction.xml.inc"
public:
	bool canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> item, runtime::Ptr<gameobjects::Item> targetItem,
		std::initializer_list<std::any> params = {}) const override;

	void act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> item, runtime::Ptr<gameobjects::Item> targetItem,
		std::initializer_list<std::any> params = {}) const override;
};

} // namespace aion::gameserver::model::templates::item::actions
