#pragma once

#include "aion/gameserver/model/templates/item/actions/MegaphoneAction.xml.h"

#include <any>
#include <initializer_list>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::templates::item::actions {

/**
 * Java com.aionemu.gameserver.model.templates.item.actions.MegaphoneAction.
 * <p>
 * C++ notes: `params.begin()[0]` of act is the message as `std::string` (MegaphoneAction.java:38; CM_MEGAPHONE.java:54, :59 pass it to
 * canAct and act).
 *
 * @author Rolandas, ginho1
 */
class MegaphoneAction : public ::aion::gameserver::model::templates::item::actions::AbstractItemAction {
#include "aion/gameserver/model/templates/item/actions/MegaphoneAction.xml.inc"
public:
	bool canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> item, runtime::Ptr<gameobjects::Item> targetItem,
		std::initializer_list<std::any> params = {}) const override;

	void act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> item, runtime::Ptr<gameobjects::Item> targetItem,
		std::initializer_list<std::any> params = {}) const override;
};

} // namespace aion::gameserver::model::templates::item::actions
