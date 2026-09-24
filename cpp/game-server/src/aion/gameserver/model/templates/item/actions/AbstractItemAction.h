#pragma once

#include "aion/gameserver/model/templates/item/actions/AbstractItemAction.xml.h"

#include <any>
#include <initializer_list>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::templates::item::actions {

/**
 * Java com.aionemu.gameserver.model.templates.item.actions.AbstractItemAction.
 * <p>
 * C++ notes (header request m5b3-h01, m5b3-plan.md D6): Java's two abstract methods are pure virtual here, and every one of the 32 action
 * classes ItemActions binds (ItemActions.java:15-31) declares both overrides, so the binders still instantiate concrete classes. Both are
 * `const` because action templates are referenced as `const X*` (hub-headers.md §9.1). `parentItem` and `targetItem` are nullable (§5.1):
 * CM_APPEARANCE.java:112-113 passes null for both to canAct and for the parent to act, CM_MEGAPHONE.java:54 and the handler
 * IceChunkAI.java:33 pass null for the target, and five bodies compare the parent item with null (AnimationAddAction.java:38,
 * EmotionLearnAction.java:40, EnchantItemAction.java:48, RideAction.java:48, TitleAddAction.java:29). `Object... params` is
 * `std::initializer_list<std::any>` with `= {}` (§7.4; overrides repeat the default); the four receivers that read it expect
 * `params.begin()[0]` to hold: DyeAction a `runtime::Ref<gameobjects::HouseObject>` (null for Java null, CM_USE_ITEM.java:101),
 * MultiReturnAction the `int32_t` return index (:104), InstanceTimeClear the `int32_t` sync id (:107), MegaphoneAction the message as
 * `std::string` (CM_MEGAPHONE.java:54).
 *
 * @author ATracer
 */
class AbstractItemAction : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/item/actions/AbstractItemAction.xml.inc"
public:
	/**
	 * Check if an item can be used.
	 *
	 * @param params Optional parameters (implementation specific), same as in the following act(Player, Item, Item, Object...) call.
	 * @return True if act() can be called
	 */
	virtual bool canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> targetItem,
		std::initializer_list<std::any> params = {}) const = 0;

	/**
	 * @param params Optional parameters (implementation specific), same as in previous canAct(Player, Item, Item, Object...) call.
	 */
	virtual void act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> targetItem,
		std::initializer_list<std::any> params = {}) const = 0;
};

} // namespace aion::gameserver::model::templates::item::actions
